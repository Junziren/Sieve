#include "GrainEngine.h"

namespace SortSynth {

GrainEngine::GrainEngine() {
    formatManager.registerBasicFormats();
}

LoadResult GrainEngine::loadFile(const juce::File& file, double hostSampleRate) {
    if (!file.existsAsFile())
        return LoadResult::FileNotFound;
    
    if (file.getSize() == 0)
        return LoadResult::EmptyFile;
    
    if (static_cast<size_t>(file.getSize()) > MAX_FILE_BYTES)
        return LoadResult::TooLarge;
    
    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager.createReaderFor(file));
    
    if (!reader)
        return LoadResult::UnsupportedFormat;
    
    sampleRate = reader->sampleRate;
    
    // Convert to mono if needed
    if (reader->numChannels > 1) {
        juce::AudioBuffer<float> stereoBuffer(
            static_cast<int>(reader->numChannels),
            static_cast<int>(reader->lengthInSamples));
        reader->read(&stereoBuffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);
        
        audioBuffer.setSize(1, static_cast<int>(reader->lengthInSamples));
        audioBuffer.clear();
        auto* dst = audioBuffer.getWritePointer(0);
        for (int ch = 0; ch < static_cast<int>(reader->numChannels); ++ch) {
            const auto* src = stereoBuffer.getReadPointer(ch);
            for (int i = 0; i < static_cast<int>(reader->lengthInSamples); ++i)
                dst[i] += src[i];
        }
        float scale = 1.0f / reader->numChannels;
        for (int i = 0; i < static_cast<int>(reader->lengthInSamples); ++i)
            dst[i] *= scale;
    } else {
        audioBuffer.setSize(1, static_cast<int>(reader->lengthInSamples));
        reader->read(&audioBuffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);
    }
    
    totalDurationMs = (audioBuffer.getNumSamples() / sampleRate) * 1000.0;
    loaded = true;
    return LoadResult::OK;
}

void GrainEngine::sliceBuffer(int numGrains) {
    jassert(loaded);
    jassert(numGrains > 0);
    
    grains.clear();
    grains.reserve(static_cast<size_t>(numGrains));
    
    int totalSamples = audioBuffer.getNumSamples();
    float grainLenSamples = static_cast<float>(totalSamples) / numGrains;
    float grainLenMs = static_cast<float>(totalDurationMs / numGrains);
    
    for (int i = 0; i < numGrains; ++i) {
        Grain g;
        g.id = i;
        g.originalIndex = i;
        g.startMs = i * grainLenMs;
        g.durationMs = grainLenMs;
        g.color = indexToColour(i, numGrains);
        grains.push_back(g);
    }
}

juce::Colour GrainEngine::indexToColour(int idx, int total) {
    float hue = (static_cast<float>(idx) / total) * 0.85f; // 0 to ~300 deg
    return juce::Colour::fromHSV(hue, 0.7f, 0.8f, 1.0f);
}

} // namespace SortSynth
