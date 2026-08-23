#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace SortSynth {

class WaveformDisplay : public juce::Component {
public:
    WaveformDisplay();
    void setAudioData(const juce::AudioBuffer<float>& buffer, double sampleRate);
    void setPlaybackPosition(double posSamples);
    void clear();

    void paint(juce::Graphics& g) override;

private:
    std::vector<float> thumbnail;    // downsampled waveform points
    double totalSamples = 0.0;
    double playbackPosSamples = 0.0;
    bool hasData = false;

    void buildThumbnail(const juce::AudioBuffer<float>& buffer);
};

} // namespace SortSynth
