#pragma once
#include "Core/Common.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <atomic>

namespace SortSynth {

class GrainVoice {
public:
    GrainVoice();
    void prepare(double sr);
    void setGrain(const Grain& g, const float* bufferData, int bufferLen,
                  float pitchRate, float velocity, float grainDurationFactor);
    void setGain(float g)           { masterGain.store(g, std::memory_order_release); }
    void setPan(float p)            { pan.store(p, std::memory_order_release); }
    void setEnvelope(float atk, float dcy, float sus, float rel) {
        attackMs = atk; decayMs = dcy; sustainLv = sus; releaseMs = rel;
        recalcRates();
    }
    void noteOn();
    void noteOff();
    void forceOff()                 { phase = EnvPhase::Idle; envLevel = 0.0f;
                                      active.store(false, std::memory_order_release); }
    bool isActive() const           { return active.load(std::memory_order_acquire); }
    double getPlaybackPos() const   { return playbackPos; }
    int    getSourceLength() const  { return sourceLength; }
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples);
    void reset();

private:
    const float* sourceData = nullptr;
    int   sourceLength = 0;
    int   grainEndSample = 0;    // the sample index where this grain ends
    double playbackPos = 0.0;
    double sampleRate = 44100.0;

    std::atomic<float> pitchRate{1.0f};
    std::atomic<float> masterGain{1.0f};
    std::atomic<float> pan{0.0f};
    std::atomic<bool>  active{false};

    enum class EnvPhase { Idle, Attack, Decay, Sustain, Release };
    EnvPhase phase = EnvPhase::Idle;
    float envLevel = 0.0f;

    float attackMs  = 2.0f;
    float decayMs   = 80.0f;
    float sustainLv = 0.7f;
    float releaseMs = 60.0f;

    float attackInc  = 0.0f;
    float decayDec   = 0.0f;
    float releaseDec = 0.0f;

    void recalcRates();
};

} // namespace SortSynth
