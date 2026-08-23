#include "SortVisualizer.h"
// VoiceState included via SortVisualizer.h

namespace SortSynth {

SortVisualizer::SortVisualizer() {
    setOpaque(false);
}

void SortVisualizer::updateState(const std::array<VoiceState, 4>& voices) {
    hasActiveVoices = false;
    numColumns = 0;

    for (const auto& vs : voices) {
        if (vs.active && !vs.currentValues.empty()) {
            hasActiveVoices = true;
            numColumns = juce::jmax(numColumns, static_cast<int>(vs.currentValues.size()));
        }
    }

    if (!hasActiveVoices) { columns.clear(); return; }

    int totalCols = numColumns * 4;
    if (static_cast<int>(columns.size()) != totalCols) {
        columns.resize(static_cast<size_t>(totalCols));
        for (auto& c : columns) { c.currentHeight = 0.0f; c.targetHeight = 0.0f; c.highlightAlpha = 0.0f; }
    }

    for (int v = 0; v < 4; ++v) {
        const auto& vs = voices[static_cast<size_t>(v)];
        if (!vs.active || vs.currentValues.empty()) {
            for (int c = 0; c < numColumns; ++c)
                columns[static_cast<size_t>(c * 4 + v)].targetHeight = 0.0f;
            continue;
        }

        int maxVal = static_cast<int>(vs.currentValues.size()) - 1;
        for (int c = 0; c < static_cast<int>(vs.currentValues.size()); ++c) {
            auto& col = columns[static_cast<size_t>(c * 4 + v)];
            col.value = vs.currentValues[static_cast<size_t>(c)];
            col.targetHeight = (maxVal > 0) ? static_cast<float>(col.value) / maxVal : 0.0f;

            // Highlight recently swapped indices
            if (vs.stepIndex > 0 && vs.stepIndex <= static_cast<int>(vs.steps.size())) {
                const auto& step = vs.steps[static_cast<size_t>(vs.stepIndex - 1)];
                if (step.type == SortStep::Swap || step.type == SortStep::Overwrite) {
                    if (c == step.indexA || c == step.indexB) {
                        col.highlight = true;
                        col.highlightAlpha = 1.0f;
                    }
                }
            }
        }

        for (int c = static_cast<int>(vs.currentValues.size()); c < numColumns; ++c)
            columns[static_cast<size_t>(c * 4 + v)].targetHeight = 0.0f;
    }

    // Animate
    for (auto& col : columns) {
        col.currentHeight += (col.targetHeight - col.currentHeight) * 0.12f;
        if (col.highlight) col.highlightAlpha *= 0.82f;
        if (col.highlightAlpha < 0.01f) col.highlight = false;
    }
}

void SortVisualizer::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff0d0d1a));
    g.fillRoundedRectangle(bounds, 8.0f);

    if (!hasActiveVoices || columns.empty()) {
        g.setColour(juce::Colour(0xff404060));
        g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));
        g.drawText("MIDI key to sort...", bounds, juce::Justification::centred);
        return;
    }

    float m = 8.0f, iw = bounds.getWidth() - m * 2, ih = bounds.getHeight() - m * 2;
    float bw = iw / columns.size();
    float rowH = ih / 4.0f;
    float baseY = bounds.getY() + m + ih;

    // Row separators
    g.setColour(juce::Colour(0xff15152a));
    for (int r = 1; r < 4; ++r)
        g.drawHorizontalLine(bounds.getY() + m + rowH * r, bounds.getX() + m, bounds.getRight() - m);

    for (size_t i = 0; i < columns.size(); ++i) {
        const auto& col = columns[i];
        int vid = static_cast<int>(i % 4);
        float x = bounds.getX() + m + i * bw;
        float maxH = rowH - 4.0f;
        float h = col.currentHeight * maxH;
        float y = baseY - vid * rowH - h;

        // Glow on highlight
        if (col.highlight && col.highlightAlpha > 0.01f) {
            g.setColour(juce::Colours::white.withAlpha(col.highlightAlpha * 0.5f));
            g.fillRoundedRectangle(x - 1, y - 2, bw + 2, h + 4, 2.0f);
        }

        // Bar
        float bright = 0.5f + col.currentHeight * 0.5f;
        float sat = 0.5f + col.currentHeight * 0.5f;
        auto barColor = voiceColors[static_cast<size_t>(vid)]
            .withMultipliedBrightness(bright)
            .withMultipliedSaturation(sat);
        g.setColour(barColor);
        g.fillRoundedRectangle(x + 1, y, juce::jmax(1.0f, bw - 2), juce::jmax(1.0f, h), 2.0f);

        // Top gleam
        if (h > 3) {
            g.setColour(juce::Colours::white.withAlpha(0.08f));
            g.fillRoundedRectangle(x + 2, y, juce::jmax(1.0f, bw - 4), 2.0f, 1.0f);
        }
    }

    // Voice labels
    static const char* vlabels[] = {"V1", "V2", "V3", "V4"};
    for (int v = 0; v < 4; ++v) {
        g.setColour(voiceColors[static_cast<size_t>(v)].withAlpha(0.35f));
        g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
        g.drawText(vlabels[v],
            juce::Rectangle<float>(bounds.getX() + m + 2, bounds.getY() + m + rowH * v + 4, 24, 12),
            juce::Justification::left);
    }
}

} // namespace SortSynth



