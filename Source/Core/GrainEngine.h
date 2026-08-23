#pragma once
#include "Core/Common.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace SortSynth {

enum class LoadResult { OK, FileNotFound, UnsupportedFormat, TooLarge, SampleRateMismatch, EmptyFile };

class GrainEngine {
public:
    GrainEngine();
    
    LoadResult loadFile(const juce::File& file, double hostSampleRate);
    void sliceBuffer(int numGrains);
    
    const std::vector<Grain>& getGrains() const { return grains; }
    const juce::AudioBuffer<float>& getAudioBuffer() const { return audioBuffer; }
    double getSampleRate() const { return sampleRate; }
    double getTotalDurationMs() const { return totalDurationMs; }
    bool isLoaded() const { return loaded; }
    
    static constexpr size_t MAX_FILE_BYTES = 100 * 1024 * 1024;  // 100MB

private:
    juce::AudioFormatManager formatManager;
    juce::AudioBuffer<float> audioBuffer;
    std::vector<Grain> grains;
    double sampleRate = 44100.0;
    double totalDurationMs = 0.0;
    bool loaded = false;
    
    juce::Colour indexToColour(int idx, int total);
};

} // namespace SortSynth
