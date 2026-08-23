#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Core/Common.h"
#include "Core/GrainEngine.h"
#include "Core/GrainVoice.h"
#include "Core/SortingAlgorithms.h"
#include "Utils/LockFreeQueue.h"
#include <atomic>
#include <array>

namespace SortSynth {

struct VoiceState {
    std::vector<SortStep> steps;
    std::vector<int>      currentValues;
    int   stepIndex = 0;
    int   totalSteps = 0;
    float pitchRate = 1.0f;
    float velocity = 1.0f;
    bool  active = false;     // voice is assigned to a MIDI note
    bool  paused = false;     // note released mid-sort → preserve state
    bool  completed = false;  // sort ran to end → next press reshuffles
    int   midiNote = -1;

    void reset() {
        steps.clear(); currentValues.clear(); stepIndex=0; totalSteps=0;
        active=false; paused=false; completed=false; midiNote=-1;
    }
    void softReset() {
        // Keep active & midiNote, just rebuild sort state
        steps.clear(); currentValues.clear(); stepIndex=0; totalSteps=0;
        paused=false; completed=false;
    }
};

struct VoiceMessage {
    enum Type { Trigger, Release, Panic };
    Type type; int voiceId; int midiNote; float velocity;
};

class SortSynthAudioProcessor : public juce::AudioProcessor {
public:
    static constexpr int MAX_VOICES = 4;
    static constexpr int GRAIN_POOL_SIZE = 32;

    SortSynthAudioProcessor();
    ~SortSynthAudioProcessor() override;
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Sieve"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    bool supportsMPE() const override { return false; }
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    GrainEngine grainEngine;
    const std::array<VoiceState, 4>& getVoiceStates() const { return voices; }
    const std::array<GrainVoice, GRAIN_POOL_SIZE>& getGrainVoices() const { return grainVoices; }
    LoadResult loadFile(const juce::File& file);
    void panic();
    void resetAllVoices();

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void handleMidi(const juce::MidiBuffer&);
    void processVoiceMessages();
    void rebuildStepsForVoice(int);
    void advanceVoice(int);
    int  findFreeVoice();
    int  findPausedOrCompletedVoice(int midiNote);
    int  stealVoice();

    std::array<VoiceState, MAX_VOICES> voices;
    std::array<GrainVoice, GRAIN_POOL_SIZE> grainVoices;
    int nextGrainVoice = 0;
    SPSCQueue<VoiceMessage, 64> voiceMessageQueue;

    uint64_t tickCounter = 0;
    int  samplesPerTick = 0;
    float sortSpeedMs = 30.0f;
    Algorithm currentAlgorithm = Algorithm::Bubble;
    int  currentSliceCount = 128;
    double currentSampleRate = 44100.0;
    float masterGain = 0.8f;
    float masterPan  = 0.0f;
    float grainDurationFactor = 1.0f;
    juce::dsp::Limiter<float> limiter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SortSynthAudioProcessor)
};

} // namespace SortSynth
