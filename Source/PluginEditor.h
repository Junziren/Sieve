#pragma once
#include "PluginProcessor.h"

namespace SortSynth {

class SortSynthAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    explicit SortSynthAudioProcessorEditor(SortSynthAudioProcessor&);
    ~SortSynthAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    std::unique_ptr<juce::WebBrowserComponent> webView;
    juce::Label loadErrorLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SortSynthAudioProcessorEditor)
};

} // namespace SortSynth
