#include "WaveformDisplay.h"
#include <algorithm>

namespace SortSynth {

WaveformDisplay::WaveformDisplay() { setOpaque(false); }

void WaveformDisplay::setAudioData(const juce::AudioBuffer<float>& buffer, double /*sampleRate*/) {
    hasData = false;
    totalSamples = static_cast<double>(buffer.getNumSamples());
    if (totalSamples <= 0) return;
    buildThumbnail(buffer);
    hasData = true;
}

void WaveformDisplay::setPlaybackPosition(double posSamples) {
    playbackPosSamples = posSamples;
}

void WaveformDisplay::clear() {
    hasData = false;
    thumbnail.clear();
    playbackPosSamples = 0.0;
    totalSamples = 0.0;
    repaint();
}

void WaveformDisplay::buildThumbnail(const juce::AudioBuffer<float>& buffer) {
    const float* data = buffer.getReadPointer(0);
    int n = buffer.getNumSamples();

    // Downsample to ~width*2 points for good resolution
    int targetPoints = 800;  // fallback, resized() triggers repaint
    
    if (targetPoints > n) targetPoints = n;

    thumbnail.resize(static_cast<size_t>(targetPoints));
    int step = n / targetPoints;
    if (step < 1) step = 1;

    for (int i = 0; i < targetPoints; ++i) {
        int start = i * step;
        int end = juce::jmin(start + step, n);
        float maxVal = 0.0f;
        for (int j = start; j < end; ++j) {
            float absVal = std::abs(data[j]);
            if (absVal > maxVal) maxVal = absVal;
        }
        thumbnail[static_cast<size_t>(i)] = maxVal;
    }
}

void WaveformDisplay::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff0d0d1a));
    g.fillRoundedRectangle(bounds, 8.0f);

    if (!hasData || thumbnail.empty()) {
        g.setColour(juce::Colour(0xff404060));
        g.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
        g.drawText("Drop audio file or click Load", bounds, juce::Justification::centred);
        return;
    }

    float margin = 6.0f;
    auto area = bounds.reduced(margin);
    float midY = area.getCentreY();
    float halfH = area.getHeight() * 0.38f;
    float w = area.getWidth();

    // Draw waveform (symmetrical)
    auto waveColor = juce::Colour(0xff00d4ff).withAlpha(0.35f);
    g.setColour(waveColor);

    juce::Path upperPath, lowerPath;
    bool first = true;
    for (size_t i = 0; i < thumbnail.size(); ++i) {
        float x = area.getX() + (static_cast<float>(i) / (thumbnail.size() - 1)) * w;
        float amp = thumbnail[i] * halfH;
        if (first) {
            upperPath.startNewSubPath(x, midY - amp);
            lowerPath.startNewSubPath(x, midY + amp);
            first = false;
        } else {
            upperPath.lineTo(x, midY - amp);
            lowerPath.lineTo(x, midY + amp);
        }
    }
    g.strokePath(upperPath, juce::PathStrokeType(1.2f));
    g.strokePath(lowerPath, juce::PathStrokeType(1.2f));

    // Center line
    g.setColour(juce::Colour(0xff404060));
    g.drawHorizontalLine(midY, area.getX(), area.getRight());

    // Playback position line
    if (totalSamples > 0.0) {
        float posRatio = static_cast<float>(playbackPosSamples / totalSamples);
        float posX = area.getX() + posRatio * w;
        if (posX >= area.getX() && posX <= area.getRight()) {
            g.setColour(juce::Colour(0xffff6b6b).withAlpha(0.9f));
            g.drawVerticalLine(static_cast<int>(posX), area.getY(), area.getBottom());
            // Small triangle at top
            juce::Path tri;
            tri.addTriangle(posX, area.getY(), posX - 4.0f, area.getY() + 7.0f,
                           posX + 4.0f, area.getY() + 7.0f);
            g.fillPath(tri);
        }
    }
}

} // namespace SortSynth

