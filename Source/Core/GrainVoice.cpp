#include "GrainVoice.h"
#include <algorithm>

namespace SortSynth {

// 4-point Catmull-Rom: much stronger imaging suppression than linear
// interpolation on tonal material, for a few extra MACs per sample.
static inline float catmullRom(float p0, float p1, float p2, float p3, float t) {
    float t2 = t * t;
    float t3 = t2 * t;
    return 0.5f * (2.0f * p1 + (p2 - p0) * t
         + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
         + (3.0f * p1 - p0 - 3.0f * p2 + p3) * t3);
}

GrainVoice::GrainVoice() { recalcRates(); }

void GrainVoice::prepare(double sr) {
    sampleRate = sr;
    recalcRates();
    stealFadeLen = juce::jmax(8, static_cast<int>(0.002 * sr));
    reset();
}

void GrainVoice::recalcRates() {
    if (sampleRate <= 0.0) sampleRate = 44100.0;
    attackInc  = 1000.0f / (attackMs  * static_cast<float>(sampleRate));
    decayDec   = 1000.0f / (decayMs   * static_cast<float>(sampleRate));
    releaseDec = 1000.0f / (releaseMs * static_cast<float>(sampleRate));
}

void GrainVoice::setGrain(const Grain& g, const float* buf, int len,
                           float pr, float vel, float grainDurationFactor, int ownerNote_) {
    sourceData  = buf;
    sourceLength = len;
    ownerNote = ownerNote_;
    velocityScale = juce::jlimit(0.0f, 1.0f, vel);
    pitchRate.store(pr, std::memory_order_release);
    playbackPos  = (g.startMs / 1000.0) * sampleRate;

    // Calculate end position based on grain duration factor (0.1x to 4.0x)
    double grainLenMs = g.durationMs * grainDurationFactor;
    grainEndSample = static_cast<int>(playbackPos + (grainLenMs / 1000.0) * sampleRate);
    if (grainEndSample > sourceLength) grainEndSample = sourceLength;

    // Fade-out spans 35% of the grain (never longer than the user release),
    // stretched to at least 8 output samples so fast pitch rates stay smooth.
    double grainSamples  = static_cast<double>(grainEndSample) - playbackPos;
    double releaseSamples = (releaseMs / 1000.0) * sampleRate;
    fadeOutLen = juce::jmin(grainSamples * 0.35, releaseSamples);
    fadeOutLen = juce::jmin(juce::jmax(fadeOutLen, 8.0 * static_cast<double>(pr)), grainSamples);
    fadeOutLen = juce::jmax(1.0, fadeOutLen);
}

void GrainVoice::noteOn() {
    if (active.load(std::memory_order_acquire) && envLevel > 0.0001f) {
        // Blend down from the slot's actual output level (envelope x tail
        // window). Blending from the raw envelope makes a stolen tail voice
        // jump UP in amplitude - a periodic buzz at the sort tick rate.
        double remaining = static_cast<double>(grainEndSample) - playbackPos;
        double window = 1.0;
        if (fadeOutLen > 0.0 && remaining < fadeOutLen)
            window = juce::jlimit(0.0, 1.0, remaining / fadeOutLen);
        stealFadeLevel = envLevel * static_cast<float>(window);
        stealFadeSamples = stealFadeLen;
    } else {
        stealFadeLevel = 0.0f;
        stealFadeSamples = 0;
    }
    phase = EnvPhase::Attack;
    envLevel = 0.0f;
    active.store(true, std::memory_order_release);
}

void GrainVoice::noteOff() {
    if (phase != EnvPhase::Idle && phase != EnvPhase::Release)
        phase = EnvPhase::Release;
}

void GrainVoice::renderNextBlock(juce::AudioBuffer<float>& output, int start, int n) {
    if (!active.load(std::memory_order_acquire)) return;

    float pr  = pitchRate.load(std::memory_order_acquire);
    float mg  = masterGain.load(std::memory_order_acquire);
    float pv  = pan.load(std::memory_order_acquire);

    float panL = (pv <= 0.0f) ? 1.0f : (1.0f - pv);
    float panR = (pv >= 0.0f) ? 1.0f : (1.0f + pv);

    auto* L = output.getWritePointer(0, start);
    auto* R = output.getNumChannels() > 1 ? output.getWritePointer(1, start) : nullptr;
    if (!R) R = L;

    for (int i = 0; i < n; ++i) {
        // ADSR envelope
        switch (phase) {
        case EnvPhase::Attack:
            envLevel += attackInc;
            if (envLevel >= 1.0f) { envLevel = 1.0f; phase = EnvPhase::Decay; }
            break;
        case EnvPhase::Decay:
            envLevel -= decayDec;
            if (envLevel <= sustainLv) { envLevel = sustainLv; phase = EnvPhase::Sustain; }
            break;
        case EnvPhase::Sustain:
            break;
        case EnvPhase::Release:
            envLevel -= releaseDec;
            if (envLevel <= 0.0f) {
                envLevel = 0.0f; phase = EnvPhase::Idle;
                active.store(false, std::memory_order_release);
                return;
            }
            break;
        case EnvPhase::Idle:
            return;
        }

        float env = envLevel;
        if (stealFadeSamples > 0) {
            float t = static_cast<float>(stealFadeSamples) / static_cast<float>(stealFadeLen);
            env = juce::jmin(1.0f, env + stealFadeLevel * t);
            --stealFadeSamples;
        }

        if (env <= 0.0f) continue;

        double remaining = static_cast<double>(grainEndSample) - playbackPos;
        if (remaining <= 0.0) {
            phase = EnvPhase::Idle; envLevel = 0.0f;
            active.store(false, std::memory_order_release);
            return;
        }

        float windowGain = 1.0f;
        if (fadeOutLen > 0.0 && remaining < fadeOutLen)
            windowGain = static_cast<float>(remaining / fadeOutLen);

        // Sample with Catmull-Rom interpolation (linear near the buffer edges)
        if (sourceData && playbackPos >= 0.0 && playbackPos < static_cast<double>(sourceLength)) {
            int idx = static_cast<int>(playbackPos);
            float frac = static_cast<float>(playbackPos - idx);
            float s0 = sourceData[idx];
            float s1 = (idx + 1 < sourceLength) ? sourceData[idx + 1] : s0;
            float interp;
            if (idx >= 1 && idx + 2 < sourceLength) {
                interp = catmullRom(sourceData[idx - 1], s0, s1, sourceData[idx + 2], frac);
            } else {
                interp = s0 + (s1 - s0) * frac;
            }
            float samp = interp * env * windowGain * velocityScale * mg;
            L[i] += samp * panL;
            R[i] += samp * panR;
        }

        playbackPos += pr;

        // Grain finished (windowGain already brought amplitude to ~0 here)
        if (playbackPos >= static_cast<double>(grainEndSample) ||
            playbackPos >= static_cast<double>(sourceLength) ||
            playbackPos < 0.0) {
            phase = EnvPhase::Idle; envLevel = 0.0f;
            active.store(false, std::memory_order_release);
            return;
        }
    }
}

void GrainVoice::reset() {
    sourceData = nullptr; sourceLength = 0; grainEndSample = 0;
    fadeOutLen = 0.0; stealFadeLevel = 0.0f; stealFadeSamples = 0;
    velocityScale = 1.0f; ownerNote = -1;
    playbackPos = 0.0; phase = EnvPhase::Idle; envLevel = 0.0f;
    active.store(false, std::memory_order_release);
    pitchRate.store(1.0f, std::memory_order_release);
}

} // namespace SortSynth
