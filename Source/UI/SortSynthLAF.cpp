#include "SortSynthLAF.h"

namespace SortSynth {

SortSynthLAF::SortSynthLAF() {
    setColour(juce::ResizableWindow::backgroundColourId, bgDark);
    setColour(juce::Slider::thumbColourId, accent);
    setColour(juce::Slider::trackColourId, accent);
    setColour(juce::Slider::backgroundColourId, bgMid);
    setColour(juce::Slider::rotarySliderFillColourId, accent);
    setColour(juce::Slider::rotarySliderOutlineColourId, bgLight);
    setColour(juce::Slider::textBoxTextColourId, textMain);
    setColour(juce::Slider::textBoxBackgroundColourId, bgMid);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::backgroundColourId, bgLight);
    setColour(juce::ComboBox::outlineColourId, accent.withAlpha(0.3f));
    setColour(juce::ComboBox::arrowColourId, accent);
    setColour(juce::ComboBox::textColourId, textMain);
    setColour(juce::ComboBox::buttonColourId, accent);
    setColour(juce::PopupMenu::backgroundColourId, bgMid);
    setColour(juce::PopupMenu::textColourId, textMain);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha(0.2f));
    setColour(juce::PopupMenu::highlightedTextColourId, accent);
    setColour(juce::TextButton::buttonColourId, accent);
    setColour(juce::TextButton::buttonOnColourId, accent2);
    setColour(juce::TextButton::textColourOffId, bgDark);
    setColour(juce::TextButton::textColourOnId, bgDark);
    setColour(juce::ToggleButton::tickColourId, accent3);
    setColour(juce::ToggleButton::tickDisabledColourId, textDim);
    setColour(juce::Label::textColourId, textMain);
}

void SortSynthLAF::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                     float sliderPos, float startAngle, float endAngle,
                                     juce::Slider&) {
    auto bounds = juce::Rectangle<float>(static_cast<float>(x + 4), static_cast<float>(y + 4),
                                          static_cast<float>(w - 8), static_cast<float>(h - 8));
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.44f;
    auto centre = bounds.getCentre();
    auto toAngle = startAngle + sliderPos * (endAngle - startAngle);
    auto lw = radius * 0.12f;

    // Outer ring
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius, radius, 0, startAngle, endAngle, true);
    g.setColour(bgLight);
    g.strokePath(track, juce::PathStrokeType(lw));

    // Fill arc
    juce::Path fill;
    fill.addCentredArc(centre.x, centre.y, radius, radius, 0, startAngle, toAngle, true);
    juce::ColourGradient grad(accent, centre.x, centre.y - radius,
                              accent.withAlpha(0.3f), centre.x, centre.y + radius, false);
    g.setGradientFill(grad);
    g.strokePath(fill, juce::PathStrokeType(lw, juce::PathStrokeType::curved));

    // Pointer
    juce::Path ptr;
    auto ptrLen = radius * 0.55f;
    ptr.addLineSegment(juce::Line<float>::fromStartAndAngle(centre, ptrLen, toAngle), lw * 1.4f);
    g.setColour(accent.brighter(0.2f));
    g.strokePath(ptr, juce::PathStrokeType(lw * 1.4f, juce::PathStrokeType::curved));

    // Center dot
    g.setColour(accent);
    g.fillEllipse(centre.x - lw * 0.8f, centre.y - lw * 0.8f, lw * 1.6f, lw * 1.6f);
}

void SortSynthLAF::drawToggleButton(juce::Graphics& g, juce::ToggleButton& btn,
                                     bool hover, bool) {
    auto bounds = btn.getLocalBounds().toFloat();
    auto toggleArea = bounds.removeFromRight(22.0f).withSizeKeepingCentre(36.0f, 20.0f);

    // Track
    g.setColour(btn.getToggleState() ? accent3.withAlpha(0.25f) : bgLight);
    g.fillRoundedRectangle(toggleArea, 10.0f);

    // Thumb
    auto thumb = toggleArea.reduced(3.0f).withWidth(14.0f);
    if (!btn.getToggleState()) thumb.setX(toggleArea.getX() + 3.0f);
    else thumb.setX(toggleArea.getRight() - 17.0f);

    g.setColour(btn.getToggleState() ? accent3 : textDim);
    g.fillEllipse(thumb);
    if (hover) {
        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.fillEllipse(thumb.expanded(2.0f));
    }
}

void SortSynthLAF::drawButtonBackground(juce::Graphics& g, juce::Button& btn,
                                         const juce::Colour&, bool hover, bool down) {
    auto bounds = btn.getLocalBounds().toFloat().reduced(0.5f);
    auto base = btn.getToggleState() ? accent2 : accent;
    if (down) base = base.darker(0.35f);
    else if (hover) base = base.brighter(0.15f);

    g.setColour(base);
    g.fillRoundedRectangle(bounds, 6.0f);
    if (hover && !down) {
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.fillRoundedRectangle(bounds, 6.0f);
    }
}

void SortSynthLAF::drawComboBox(juce::Graphics& g, int w, int h, bool isDown,
                                 int, int, int, int, juce::ComboBox&) {
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h));
    g.setColour(isDown ? bgLight.brighter(0.15f) : bgLight);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(accent.withAlpha(isDown ? 0.5f : 0.25f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.5f);

    // Dropdown arrow
    auto arrowX = bounds.getRight() - 20.0f;
    juce::Path arrow;
    arrow.addTriangle(arrowX - 6.0f, bounds.getCentreY() - 3.0f,
                      arrowX + 6.0f, bounds.getCentreY() - 3.0f,
                      arrowX, bounds.getCentreY() + 4.0f);
    g.setColour(accent);
    g.fillPath(arrow);
}

void SortSynthLAF::positionComboBoxText(juce::ComboBox& box, juce::Label& label) {
    label.setBounds(8, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setColour(juce::Label::textColourId, textMain);
}

juce::Font SortSynthLAF::getComboBoxFont(juce::ComboBox&) {
    return juce::Font(juce::FontOptions().withHeight(13.0f));
}

juce::Font SortSynthLAF::getLabelFont(juce::Label&) {
    return juce::Font(juce::FontOptions().withHeight(11.0f));
}

} // namespace SortSynth


