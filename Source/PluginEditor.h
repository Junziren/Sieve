#pragma once
#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>

namespace SortSynth {

class SortSynthAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::FileDragAndDropTarget,
                                      private juce::Timer {
public:
    explicit SortSynthAudioProcessorEditor(SortSynthAudioProcessor&);
    ~SortSynthAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void handleSetParameter(const juce::Array<juce::var>&,
                            juce::WebBrowserComponent::NativeFunctionCompletion);
    void handleLoadFile(juce::WebBrowserComponent::NativeFunctionCompletion);
    void handleLoadFileData(const juce::Array<juce::var>&,
                            juce::WebBrowserComponent::NativeFunctionCompletion);
    LoadResult loadSampleFile(const juce::File&);
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void fileDragEnter(const juce::StringArray&, int, int) override;
    void fileDragExit(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int, int) override;
    void sendParameterState();
    void sendSampleOverview();
    void sendUiFrame(const SortSynthAudioProcessor::UiFrame&);
    juce::var makeParameterState() const;
    static bool isAllowedParameter(const juce::String& parameterId);
    static juce::String loadResultToString(LoadResult);

    SortSynthAudioProcessor& processor;
    std::unique_ptr<juce::WebBrowserComponent> webView;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::Label loadErrorLabel;
    bool uiReady = false;
    bool sampleLoadInProgress = false;
    uint64_t lastSampleGeneration = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SortSynthAudioProcessorEditor)
};

} // namespace SortSynth
