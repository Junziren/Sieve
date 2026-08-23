#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include <vector>

namespace SortSynth {

class SortVisualizer : public juce::Component {
public:
    SortVisualizer();
    void paint(juce::Graphics& g) override;
    void updateState(const std::array<VoiceState, 4>& voices);

private:
    struct ColumnState {
        float targetHeight = 0.0f;
        float currentHeight = 0.0f;
        int   value = 0;
        bool  highlight = false;
        float highlightAlpha = 0.0f;
    };

    bool hasActiveVoices = false;
    int  numColumns = 0;
    std::vector<ColumnState> columns;

    inline static const juce::Colour voiceColors[4] = {
        juce::Colour(0xff00d4ff), // Cyan
        juce::Colour(0xffff6b6b), // Coral
        juce::Colour(0xff7cff6b), // Green
        juce::Colour(0xffd46bff)  // Purple
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SortVisualizer)
};

} // namespace SortSynth
