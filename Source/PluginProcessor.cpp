#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <random>

namespace SortSynth {

// Transposition is clamped to ±24 semitones (0.25x–4x): beyond that,
// linear-interpolation resampling aliases audibly on tonal material.
static float noteToRate(int midiNote) {
    float semis = juce::jlimit(-24.0f, 24.0f, static_cast<float>(midiNote) - 60.0f);
    return std::pow(2.0f, semis / 12.0f);
}

SortSynthAudioProcessor::SortSynthAudioProcessor()
    : juce::AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{}

SortSynthAudioProcessor::~SortSynthAudioProcessor() = default;

bool SortSynthAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainInputChannels() != 0) return false;
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

juce::AudioProcessorValueTreeState::ParameterLayout SortSynthAudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterInt>("sliceCount", "Slice Count", 4, 512, 128));
    juce::StringArray algos("bubble","insertion","selection","quick","merge","shell","heap","shaker","bogo");
    layout.add(std::make_unique<juce::AudioParameterChoice>("algorithm", "Algorithm", algos, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>("sortSpeed", "Sort Speed",
        juce::NormalisableRange<float>(1.0f, 400.0f, 1.0f, 0.4f), 30.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("grainDuration", "Grain Duration",
        juce::NormalisableRange<float>(0.05f, 5.0f, 0.01f, 0.45f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("gain", "Gain",
        juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f, 0.5f), 0.8f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("pan", "Pan",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("attack", "Attack",
        juce::NormalisableRange<float>(1.0f, 500.0f, 1.0f, 0.4f), 5.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("decay", "Decay",
        juce::NormalisableRange<float>(5.0f, 1000.0f, 1.0f, 0.4f), 100.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("sustain", "Sustain",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.7f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("release", "Release",
        juce::NormalisableRange<float>(5.0f, 2000.0f, 1.0f, 0.35f), 80.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>("performanceMode", "Performance Mode", false));
    return layout;
}

void SortSynthAudioProcessor::prepareToPlay(double sampleRate, int maxBlock) {
    currentSampleRate = sampleRate;
    for (auto& v : grainVoices) v.prepare(sampleRate);
    for (auto& vs : voices) vs.reset();
    tickCounter = 0;
    uiFrameAccumulator = 0.0;
    uiFrameSequence = 0;
    uiFrameQueue.reset();

    juce::dsp::ProcessSpec spec{
        sampleRate,
        static_cast<juce::uint32>(juce::jmax(1, maxBlock)),
        static_cast<juce::uint32>(juce::jmax(1u, static_cast<unsigned>(getTotalNumOutputChannels()))) };
    limiter.reset();
    limiter.prepare(spec);
    limiter.setThreshold(-1.0f);
    limiter.setRelease(100.0f);
}

void SortSynthAudioProcessor::releaseResources() {
    for (auto& v : grainVoices) v.reset();
}

void SortSynthAudioProcessor::panic() {
    for (auto& vs : voices) vs.reset();
    for (auto& gv : grainVoices) gv.forceOff();
    voiceMessageQueue.reset();
}

void SortSynthAudioProcessor::resetAllVoices() {
    for (auto& vs : voices) vs.reset();
}

void SortSynthAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    if (isSuspended())
        return;

    int   algoIdx = static_cast<int>(apvts.getRawParameterValue("algorithm")->load(std::memory_order_relaxed));
    float newSpeed = apvts.getRawParameterValue("sortSpeed")->load(std::memory_order_relaxed);
    int   newSlices = static_cast<int>(apvts.getRawParameterValue("sliceCount")->load(std::memory_order_relaxed));
    masterGain  = apvts.getRawParameterValue("gain")->load(std::memory_order_relaxed);
    masterPan   = apvts.getRawParameterValue("pan")->load(std::memory_order_relaxed);
    grainDurationFactor = apvts.getRawParameterValue("grainDuration")->load(std::memory_order_relaxed);

    bool paramsChanged = false;
    Algorithm newAlgo = static_cast<Algorithm>(algoIdx);
    if (newAlgo != currentAlgorithm || newSpeed != sortSpeedMs || newSlices != currentSliceCount) {
        currentAlgorithm = newAlgo;
        sortSpeedMs = newSpeed;
        if (newSlices != currentSliceCount) {
            currentSliceCount = newSlices;
            if (grainEngine.isLoaded()) grainEngine.sliceBuffer(currentSliceCount);
        }
        paramsChanged = true;
    }

    float atk = apvts.getRawParameterValue("attack")->load(std::memory_order_relaxed);
    float dcy = apvts.getRawParameterValue("decay")->load(std::memory_order_relaxed);
    float sus = apvts.getRawParameterValue("sustain")->load(std::memory_order_relaxed);
    float rel = apvts.getRawParameterValue("release")->load(std::memory_order_relaxed);
    for (auto& gv : grainVoices) {
        gv.setGain(masterGain);
        gv.setPan(masterPan);
        gv.setEnvelope(atk, dcy, sus, rel);
    }

    handleMidi(midi);
    processVoiceMessages();

    if (paramsChanged) {
        for (int i = 0; i < MAX_VOICES; ++i)
            if (voices[i].active && !voices[i].paused)
                rebuildStepsForVoice(i);
    }

    if (sortSpeedMs <= 1.0f) sortSpeedMs = 1.0f;
    samplesPerTick = static_cast<int>((sortSpeedMs / 1000.0) * currentSampleRate);
    if (samplesPerTick < 1) samplesPerTick = 1;

    bool turbo = apvts.getRawParameterValue("performanceMode")->load(std::memory_order_relaxed) > 0.5f;
    int totalSamples = buffer.getNumSamples();
    int offset = 0;

    while (offset < totalSamples) {
        int remaining = totalSamples - offset;
        int tickSize = samplesPerTick;
        if (tickSize > remaining) tickSize = remaining;

        for (auto& gv : grainVoices) gv.renderNextBlock(buffer, offset, tickSize);

        int ticksToRun = turbo ? 999999 : 1;
        for (int t = 0; t < ticksToRun; ++t) {
            bool anyProgress = false;
            for (int i = 0; i < MAX_VOICES; ++i) {
                if (voices[i].active && !voices[i].paused) {
                    int beforeIdx = voices[i].stepIndex;
                    advanceVoice(i);
                    if (voices[i].stepIndex != beforeIdx) anyProgress = true;
                }
            }
            if (!anyProgress) break;
        }

        offset += tickSize;
        tickCounter++;
    }

    juce::dsp::AudioBlock<float> block(buffer);
    limiter.process(juce::dsp::ProcessContextReplacing<float>(block));

    publishUiFrame(buffer.getNumSamples());
}

void SortSynthAudioProcessor::handleMidi(const juce::MidiBuffer& midi) {
    for (auto meta : midi) {
        auto msg = meta.getMessage();
        if (msg.isNoteOn()) {
            VoiceMessage vm{ VoiceMessage::Trigger, -1, msg.getNoteNumber(), msg.getFloatVelocity() };
            voiceMessageQueue.push(vm);
        } else if (msg.isNoteOff()) {
            VoiceMessage vm{ VoiceMessage::Release, -1, msg.getNoteNumber(), 0.0f };
            voiceMessageQueue.push(vm);
        }
    }
}

void SortSynthAudioProcessor::processVoiceMessages() {
    VoiceMessage vm;
    while (voiceMessageQueue.pop(vm)) {
        if (vm.type == VoiceMessage::Panic) { panic(); continue; }
        if (!grainEngine.isLoaded()) continue;

        if (vm.type == VoiceMessage::Trigger) {
            // 1. Try to find a paused/completed voice with the same MIDI note
            int slot = findPausedOrCompletedVoice(vm.midiNote);

            if (slot >= 0) {
                auto& vs = voices[slot];
                if (vs.completed) {
                    // Sort completed → build a fresh random order and restart
                    vs.softReset();
                    vs.midiNote = vm.midiNote;
                    vs.pitchRate = noteToRate(vm.midiNote);
                    vs.velocity = vm.velocity;
                    vs.active = true;
                    rebuildStepsForVoice(slot);
                } else {
                    // Paused mid-sort → resume
                    vs.paused = false;
                    vs.velocity = vm.velocity;
                    vs.pitchRate = noteToRate(vm.midiNote);
                }
            } else {
                // 2. No paused/completed match → find free or steal
                slot = findFreeVoice();
                if (slot < 0) slot = stealVoice();
                auto& vs = voices[slot];
                vs.reset();
                vs.active = true;
                vs.midiNote = vm.midiNote;
                vs.pitchRate = noteToRate(vm.midiNote);
                vs.velocity = vm.velocity;
                rebuildStepsForVoice(slot);
            }
        } else if (vm.type == VoiceMessage::Release) {
            for (int i = 0; i < MAX_VOICES; ++i) {
                if (voices[i].active && voices[i].midiNote == vm.midiNote && !voices[i].paused) {
                voices[i].paused = true;
                // Note-off only the grains belonging to this note
                for (auto& gv : grainVoices) {
                    if (gv.isActive() && gv.getOwnerNote() == vm.midiNote) gv.noteOff();
                }
                }
            }
        }
    }
}

void SortSynthAudioProcessor::rebuildStepsForVoice(int voiceId) {
    auto& vs = voices[voiceId];
    const auto& grains = grainEngine.getGrains();
    int n = static_cast<int>(grains.size());
    if (n == 0) return;

    vs.currentValues.resize(n);
    for (int i = 0; i < n; ++i) vs.currentValues[i] = i;

    std::random_device rd;
    std::mt19937 rng(rd());
    std::shuffle(vs.currentValues.begin(), vs.currentValues.end(), rng);

    auto workCopy = vs.currentValues;
    vs.steps = SortingAlgorithms::generateSteps(currentAlgorithm, workCopy);
    vs.totalSteps = static_cast<int>(vs.steps.size());
    vs.stepIndex = 0;
}

void SortSynthAudioProcessor::advanceVoice(int voiceId) {
    auto& vs = voices[voiceId];
    if (!vs.active || vs.paused || vs.steps.empty()) return;

    if (vs.stepIndex < static_cast<int>(vs.steps.size())) {
        const auto& step = vs.steps[static_cast<size_t>(vs.stepIndex)];
        const auto& grains = grainEngine.getGrains();
        const auto& audioBuf = grainEngine.getAudioBuffer();
        const float* audioData = audioBuf.getReadPointer(0);
        int totalSamples = audioBuf.getNumSamples();

        if (step.type == SortStep::Swap && step.indexA >= 0 && step.indexB >= 0
            && step.indexA < static_cast<int>(vs.currentValues.size())
            && step.indexB < static_cast<int>(vs.currentValues.size())) {
            std::swap(vs.currentValues[step.indexA], vs.currentValues[step.indexB]);
        } else if (step.type == SortStep::Overwrite && step.indexA >= 0
            && step.indexA < static_cast<int>(vs.currentValues.size())) {
            vs.currentValues[step.indexA] = step.indexB;
        }

        int grainIdx = step.indexA;
        if (grainIdx >= 0 && grainIdx < static_cast<int>(vs.currentValues.size()))
            grainIdx = vs.currentValues[static_cast<size_t>(grainIdx)];

        if (grainIdx >= 0 && grainIdx < static_cast<int>(grains.size())) {
            const auto totalDurationMs = grainEngine.getTotalDurationMs();
            vs.playbackPosition = totalDurationMs > 0.0
                ? juce::jlimit(0.0f, 1.0f,
                              static_cast<float>(grains[static_cast<size_t>(grainIdx)].startMs
                                                  / totalDurationMs))
                : 0.0f;

            // Reuse only fully-idle slots. Any steal - even deep in a grain's
            // tail fade - jumps the waveform to new content at the old level;
            // at the sort tick rate those jumps repeat periodically and are
            // heard as a high-frequency buzz. Under saturation this step's
            // grain is skipped instead.
            int poolSize = static_cast<int>(grainVoices.size());
            int slot = -1;
            for (int i = 0; i < poolSize; ++i) {
                int candidate = (nextGrainVoice + i) % poolSize;
                if (!grainVoices[candidate].isActive()) { slot = candidate; break; }
            }
            nextGrainVoice = (nextGrainVoice + 1) % poolSize;

            if (slot >= 0) {
                const auto& grain = grains[static_cast<size_t>(grainIdx)];
                auto& gv = grainVoices[slot];
                gv.setGrain(grain, audioData, totalSamples, vs.pitchRate, vs.velocity,
                            grainDurationFactor, vs.midiNote);
                gv.noteOn();
            }
        }

        ++vs.stepIndex;
    } else {
        // Sort complete → mark as completed & paused
        vs.completed = true;
        vs.paused = true;
        vs.playbackPosition = 1.0f;
    }
}

int SortSynthAudioProcessor::findFreeVoice() {
    for (int i = 0; i < MAX_VOICES; ++i) if (!voices[i].active) return i;
    return -1;
}

int SortSynthAudioProcessor::findPausedOrCompletedVoice(int midiNote) {
    for (int i = 0; i < MAX_VOICES; ++i)
        if (voices[i].active && voices[i].midiNote == midiNote && (voices[i].paused || voices[i].completed))
            return i;
    return -1;
}

int SortSynthAudioProcessor::stealVoice() {
    // Prefer paused/completed voices to steal
    for (int i = 0; i < MAX_VOICES; ++i) if (voices[i].paused || voices[i].completed) return i;
    return 0;  // steal voice 0 as last resort
}

LoadResult SortSynthAudioProcessor::loadFile(const juce::File& file) {
    for (auto& gv : grainVoices) gv.reset();
    for (auto& vs : voices) vs.reset();
    waveformPointCount = 0;
    waveformMin.fill(0.0f);
    waveformMax.fill(0.0f);
    ++waveformGeneration;

    auto result = grainEngine.loadFile(file, currentSampleRate);
    if (result == LoadResult::OK) {
        grainEngine.sliceBuffer(currentSliceCount);
        updateWaveformOverview();
    }
    return result;
}

void SortSynthAudioProcessor::updateWaveformOverview() {
    const auto& audioBuffer = grainEngine.getAudioBuffer();
    const int totalSamples = audioBuffer.getNumSamples();

    if (!grainEngine.isLoaded() || totalSamples <= 0) {
        waveformPointCount = 0;
        return;
    }

    const auto* samples = audioBuffer.getReadPointer(0);
    waveformPointCount = juce::jmin(UI_WAVEFORM_POINTS, totalSamples);

    for (int point = 0; point < waveformPointCount; ++point) {
        const int start = (point * totalSamples) / waveformPointCount;
        const int end = juce::jmax(start + 1, ((point + 1) * totalSamples) / waveformPointCount);
        float minValue = 1.0f;
        float maxValue = -1.0f;

        for (int sample = start; sample < juce::jmin(end, totalSamples); ++sample) {
            minValue = juce::jmin(minValue, samples[sample]);
            maxValue = juce::jmax(maxValue, samples[sample]);
        }

        waveformMin[static_cast<size_t>(point)] = minValue;
        waveformMax[static_cast<size_t>(point)] = maxValue;
    }
}

void SortSynthAudioProcessor::copySampleOverview(
    std::array<float, UI_WAVEFORM_POINTS>& minValues,
    std::array<float, UI_WAVEFORM_POINTS>& maxValues,
    int& pointCount,
    uint64_t& generation) const {
    minValues = waveformMin;
    maxValues = waveformMax;
    pointCount = waveformPointCount;
    generation = waveformGeneration;
}

void SortSynthAudioProcessor::publishUiFrame(int blockSamples) {
    uiFrameAccumulator += static_cast<double>(juce::jmax(0, blockSamples))
                          / juce::jmax(1.0, currentSampleRate);
    if (uiFrameAccumulator < (1.0 / 15.0))
        return;

    uiFrameAccumulator -= (1.0 / 15.0);

    UiFrame frame;
    frame.sliceCount = currentSliceCount;
    frame.sampleLoaded = grainEngine.isLoaded();
    frame.waveformPointCount = waveformPointCount;
    frame.sampleGeneration = waveformGeneration;
    frame.sequence = ++uiFrameSequence;
    frame.waveformMin = waveformMin;
    frame.waveformMax = waveformMax;

    for (int voiceIndex = 0; voiceIndex < MAX_VOICES; ++voiceIndex) {
        const auto& source = voices[voiceIndex];
        auto& target = frame.voices[static_cast<size_t>(voiceIndex)];
        target.valueCount = juce::jmin(UI_MAX_SLICES,
                                       static_cast<int>(source.currentValues.size()));
        target.stepIndex = source.stepIndex;
        target.totalSteps = source.totalSteps;
        target.midiNote = source.midiNote;
        target.playbackPosition = source.playbackPosition;
        target.active = source.active;
        target.paused = source.paused;
        target.completed = source.completed;

        if (target.valueCount > 0)
            std::copy_n(source.currentValues.begin(), target.valueCount,
                        target.currentValues.begin());
    }

    uiFrameQueue.push(frame);
}

void SortSynthAudioProcessor::getStateInformation(juce::MemoryBlock& data) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, data);
}

void SortSynthAudioProcessor::setStateInformation(const void* data, int size) {
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* SortSynthAudioProcessor::createEditor() {
    return new SortSynthAudioProcessorEditor(*this);
}

} // namespace SortSynth

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new SortSynth::SortSynthAudioProcessor();
}
