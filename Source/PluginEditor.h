#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "UI/SortVisualizer.h"
#include "UI/WaveformDisplay.h"
#include "UI/SortSynthLAF.h"

namespace SortSynth {

class SortSynthAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       public juce::FileDragAndDropTarget,
                                       public juce::Timer {
public:
    explicit SortSynthAudioProcessorEditor(SortSynthAudioProcessor&);
    ~SortSynthAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    void timerCallback() override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    void loadFile(const juce::File& file);
    void setupKnob(juce::Slider& s, float min, float max, float step, float val, const juce::String& suffix);
    void setupParamLabel(juce::Label& l, const juce::String& text);
    void showAbout();

    SortSynthAudioProcessor& processor;
    SortSynthLAF laf;

    // Sorting controls
    juce::ComboBox algoCombo, sliceCombo;
    juce::Slider   speedKnob, durationKnob;
    juce::ToggleButton perfToggle;
    juce::TextButton  loadButton, panicButton;

    // Synth controls
    juce::Slider gainKnob, panKnob;
    juce::Slider attackKnob, decayKnob, sustainKnob, releaseKnob;

    // Labels
    juce::Label statusLabel, titleLabel, subtitleLabel;
    juce::Label sortLabel, synthLabel;
    juce::Label speedLabel, durLabel;
    juce::Label attackLabel, decayLabel, sustainLabel, releaseLabel, gainLabel, panLabel;

    WaveformDisplay waveform;
    SortVisualizer visualizer;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> algoAttach, sliceAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> speedAttach, durAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   perfAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttach, panAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttach, decayAttach, sustainAttach, releaseAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SortSynthAudioProcessorEditor)
};

} // namespace SortSynth