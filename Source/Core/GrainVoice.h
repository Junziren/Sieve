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
                  float pitchRate, float velocity, float grainDurationFactor, int ownerNote_);
    void setGain(float g)           { masterGain.store(g, std::memory_order_release); }
    void setPan(float p)            { pan.store(p, std::memory_order_release); }
    void setEnvelope(float atk, float dcy, float sus, float rel) {
        attackMs = atk; decayMs = dcy; sustainLv = sus; releaseMs = rel;
        recalcRates();
    }
    void noteOn();
    void noteOff();
    void forceOff()                 { phase = EnvPhase::Idle; envLevel = 0.0f;
                                      stealFadeSamples = 0;
                                      active.store(false, std::memory_order_release); }
    bool isActive() const           { return active.load(std::memory_order_acquire); }
    bool isTailFading() const       { return active.load(std::memory_order_acquire)
                                          && fadeOutLen > 0.0
                                          && static_cast<double>(grainEndSample) - playbackPos < fadeOutLen * 0.5; }
    double getPlaybackPos() const   { return playbackPos; }
    int    getSourceLength() const  { return sourceLength; }
    int    getOwnerNote() const     { return ownerNote; }
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples);
    void reset();

private:
    const float* sourceData = nullptr;
    int   sourceLength = 0;
    int   grainEndSample = 0;    // the sample index where this grain ends
    double playbackPos = 0.0;
    double sampleRate = 44100.0;

    // Click prevention: every grain fades to zero before grainEndSample, and a
    // stolen pool slot blends down from the previous grain's envelope level
    // instead of jumping to silence.
    double fadeOutLen = 0.0;     // in source samples
    float  stealFadeLevel = 0.0f;
    int    stealFadeSamples = 0;
    int    stealFadeLen = 88;    // ~2ms @ 44.1kHz, refreshed in prepare()

    float  velocityScale = 1.0f;
    int    ownerNote = -1;

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
