#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace SortSynth {

class SortSynthLAF : public juce::LookAndFeel_V3 {
public:
    SortSynthLAF();

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                          float sliderPos, float startAngle, float endAngle,
                          juce::Slider& slider) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&,
                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&,
                              const juce::Colour& bg, bool isHover, bool isDown) override;
    void drawComboBox(juce::Graphics&, int w, int h, bool isDown,
                      int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getLabelFont(juce::Label&) override;

    // Color palette
    juce::Colour bgDark    = juce::Colour(0xff0a0a16);
    juce::Colour bgMid     = juce::Colour(0xff14142a);
    juce::Colour bgLight   = juce::Colour(0xff1e1e40);
    juce::Colour accent    = juce::Colour(0xff00d4ff);
    juce::Colour accent2   = juce::Colour(0xffff6b6b);
    juce::Colour accent3   = juce::Colour(0xff7cff6b);
    juce::Colour textMain  = juce::Colour(0xffd0d0f0);
    juce::Colour textDim   = juce::Colour(0xff606090);
};

} // namespace SortSynth
