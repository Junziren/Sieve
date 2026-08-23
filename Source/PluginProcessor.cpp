#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <random>

namespace SortSynth {

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

void SortSynthAudioProcessor::prepareToPlay(double sampleRate, int /*maxBlock*/) {
    currentSampleRate = sampleRate;
    for (auto& v : grainVoices) v.prepare(sampleRate);
    for (auto& vs : voices) vs.reset();
    tickCounter = 0;
    limiterHoldL = 0.0f; limiterHoldR = 0.0f;
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

    // Soft limiter
    float ceilLin = std::pow(10.0f, limiterCeilDb / 20.0f);
    auto* L = buffer.getWritePointer(0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : L;
    for (int i = 0; i < totalSamples; ++i) {
        auto limit = [&](float& hold, float& sample) {
            float absSample = std::abs(sample);
            if (absSample > hold) hold = absSample;
            else hold *= 0.9999f;
            float gain = (hold > ceilLin) ? ceilLin / hold : 1.0f;
            sample *= gain;
        };
        limit(limiterHoldL, L[i]);
        limit(limiterHoldR, R[i]);
    }
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
                    // Sort completed → shuffle and restart
                    vs.softReset();
                    vs.midiNote = vm.midiNote;
                    vs.pitchRate = std::pow(2.0f, (vm.midiNote - 60.0f) / 12.0f);
                    vs.velocity = vm.velocity;
                    vs.active = true;
                    rebuildStepsForVoice(slot);
                } else {
                    // Paused mid-sort → resume
                    vs.paused = false;
                    vs.velocity = vm.velocity;
                    vs.pitchRate = std::pow(2.0f, (vm.midiNote - 60.0f) / 12.0f);
                }
            } else {
                // 2. No paused/completed match → find free or steal
                slot = findFreeVoice();
                if (slot < 0) slot = stealVoice();
                auto& vs = voices[slot];
                vs.reset();
                vs.active = true;
                vs.midiNote = vm.midiNote;
                vs.pitchRate = std::pow(2.0f, (vm.midiNote - 60.0f) / 12.0f);
                vs.velocity = vm.velocity;
                rebuildStepsForVoice(slot);
            }
        } else if (vm.type == VoiceMessage::Release) {
            for (int i = 0; i < MAX_VOICES; ++i) {
                if (voices[i].active && voices[i].midiNote == vm.midiNote && !voices[i].paused) {
                    voices[i].paused = true;
                    // Note-off all active grain voices (envelope → release)
                    for (auto& gv : grainVoices) {
                        if (gv.isActive()) gv.noteOff();
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
            auto& gv = grainVoices[nextGrainVoice];
            nextGrainVoice = (nextGrainVoice + 1) % static_cast<int>(grainVoices.size());
            const auto& grain = grains[static_cast<size_t>(grainIdx)];
            gv.setGrain(grain, audioData, totalSamples, vs.pitchRate, vs.velocity, grainDurationFactor);
            gv.noteOn();
        }

        ++vs.stepIndex;
    } else {
        // Sort complete → mark as completed & paused
        vs.completed = true;
        vs.paused = true;
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
    auto result = grainEngine.loadFile(file, currentSampleRate);
    if (result == LoadResult::OK) {
        grainEngine.sliceBuffer(currentSliceCount);
    }
    return result;
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
