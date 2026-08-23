#include "PluginEditor.h"

namespace SortSynth {

void SortSynthAudioProcessorEditor::setupKnob(juce::Slider& s, float min, float max, float step, float val,
                                               const juce::String& suffix) {
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setRange(min, max, step);
    s.setValue(val);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 46, 16);
    s.setTextValueSuffix(suffix);
}

void SortSynthAudioProcessorEditor::setupParamLabel(juce::Label& l, const juce::String& text) {
    l.setText(text, juce::dontSendNotification);
    l.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    l.setColour(juce::Label::textColourId, laf.textDim);
    l.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(l);
}

SortSynthAudioProcessorEditor::SortSynthAudioProcessorEditor(SortSynthAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&laf);
    setSize(900, 740);
    setResizable(true, true);
    setResizeLimits(640, 520, 1200, 950);

    // ── Title (click for About easter egg) ──
    titleLabel.setText("Sieve", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions().withHeight(28.0f).withStyle("Bold")));
    titleLabel.setColour(juce::Label::textColourId, laf.accent);
    titleLabel.setInterceptsMouseClicks(false, false);
    titleLabel.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText("Algorithmic Granular Synthesizer", juce::dontSendNotification);
    subtitleLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    subtitleLabel.setColour(juce::Label::textColourId, laf.textDim);
    addAndMakeVisible(subtitleLabel);

    // ── Section labels ──
    sortLabel.setText("SORT", juce::dontSendNotification);
    sortLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f).withStyle("Bold")));
    sortLabel.setColour(juce::Label::textColourId, laf.accent.withAlpha(0.5f));
    addAndMakeVisible(sortLabel);

    synthLabel.setText("SYNTH", juce::dontSendNotification);
    synthLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f).withStyle("Bold")));
    synthLabel.setColour(juce::Label::textColourId, laf.accent2.withAlpha(0.5f));
    addAndMakeVisible(synthLabel);

    // ── Algorithm ──
    algoCombo.addItemList({"Bubble","Insertion","Selection","Quick","Merge","Shell","Heap","Shaker","Bogo"}, 1);
    algoCombo.setSelectedId(1);
    addAndMakeVisible(algoCombo);
    algoAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "algorithm", algoCombo);

    // ── Slices ──
    juce::StringArray sv;
    for (int s : {4,8,16,32,64,128,256,512}) sv.add(juce::String(s));
    sliceCombo.addItemList(sv, 1);
    sliceCombo.setSelectedId(sv.indexOf("128")+1);
    addAndMakeVisible(sliceCombo);
    sliceAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "sliceCount", sliceCombo);

    // ── Speed / Duration knobs + labels ──
    setupParamLabel(speedLabel, "Speed");
    setupKnob(speedKnob, 1, 400, 1, 30, " ms");
    addAndMakeVisible(speedKnob);
    speedAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "sortSpeed", speedKnob);

    setupParamLabel(durLabel, "Duration");
    setupKnob(durationKnob, 0.05f, 5.0f, 0.01f, 1.0f, " x");
    addAndMakeVisible(durationKnob);
    durAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "grainDuration", durationKnob);

    // ── Buttons ──
    loadButton.setButtonText("Load");
    loadButton.onClick = [this] {
        auto* c = new juce::FileChooser("Load Audio...",
            juce::File::getSpecialLocation(juce::File::userMusicDirectory),
            "*.wav;*.aiff;*.flac;*.ogg;*.mp3");
        c->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this,c](const juce::FileChooser& fc) { auto r=fc.getResult(); if(r!=juce::File{})loadFile(r); delete c; });
    };
    addAndMakeVisible(loadButton);

    panicButton.setButtonText("PANIC");
    panicButton.onClick = [this] { processor.panic(); };
    panicButton.setColour(juce::TextButton::buttonColourId, laf.accent2);
    addAndMakeVisible(panicButton);

    perfToggle.setButtonText("Turbo");
    addAndMakeVisible(perfToggle);
    perfAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "performanceMode", perfToggle);

    // ── Synth knobs + labels ──
    setupParamLabel(attackLabel, "Attack");
    setupKnob(attackKnob, 1, 500, 1, 5, " ms");
    addAndMakeVisible(attackKnob);
    attackAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "attack", attackKnob);

    setupParamLabel(decayLabel, "Decay");
    setupKnob(decayKnob, 5, 1000, 1, 100, " ms");
    addAndMakeVisible(decayKnob);
    decayAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "decay", decayKnob);

    setupParamLabel(sustainLabel, "Sustain");
    setupKnob(sustainKnob, 0.0f, 1.0f, 0.01f, 0.7f, "");
    addAndMakeVisible(sustainKnob);
    sustainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "sustain", sustainKnob);

    setupParamLabel(releaseLabel, "Release");
    setupKnob(releaseKnob, 5, 2000, 1, 60, " ms");
    addAndMakeVisible(releaseKnob);
    releaseAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "release", releaseKnob);

    setupParamLabel(gainLabel, "Gain");
    setupKnob(gainKnob, 0.0f, 1.5f, 0.01f, 0.8f, "");
    addAndMakeVisible(gainKnob);
    gainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "gain", gainKnob);

    setupParamLabel(panLabel, "Pan");
    setupKnob(panKnob, -1.0f, 1.0f, 0.01f, 0.0f, "");
    addAndMakeVisible(panKnob);
    panAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "pan", panKnob);

    // ── Status ──
    statusLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
    statusLabel.setColour(juce::Label::textColourId, laf.textDim);
    statusLabel.setText("Drop audio or click Load", juce::dontSendNotification);
    addAndMakeVisible(statusLabel);

    addAndMakeVisible(visualizer);
    addAndMakeVisible(waveform);
    startTimerHz(25);
}

SortSynthAudioProcessorEditor::~SortSynthAudioProcessorEditor() { setLookAndFeel(nullptr); }

void SortSynthAudioProcessorEditor::paint(juce::Graphics& g) { g.fillAll(laf.bgDark); }

void SortSynthAudioProcessorEditor::resized() {
    auto a = getLocalBounds().reduced(14);

    // Header
    auto hdr = a.removeFromTop(48);
    titleLabel   .setBounds(hdr.removeFromLeft(200).withTrimmedBottom(18));
    subtitleLabel.setBounds(hdr.withY(hdr.getY()+28).withHeight(16));

    a.removeFromTop(6);

    // ── SORT row ──
    auto sortArea = a.removeFromTop(86);
    sortLabel.setBounds(sortArea.getX(), sortArea.getY()-2, 50, 12);

    auto sortLabels = sortArea.removeFromTop(12);
    auto sortRow = sortArea;
    int kw = sortRow.getWidth() / 10;

    algoCombo .setBounds(sortRow.removeFromLeft(kw*2).reduced(3).withHeight(28).withY(sortRow.getY()+4));
    sliceCombo.setBounds(sortRow.removeFromLeft(kw).reduced(3).withHeight(28).withY(sortRow.getY()+4));

    auto speedArea = sortRow.removeFromLeft(kw + kw/2);
    speedLabel.setBounds(speedArea.withHeight(12).withY(sortLabels.getY()));
    speedKnob .setBounds(speedArea.reduced(3).withY(speedArea.getY()+14));

    auto durArea = sortRow.removeFromLeft(kw + kw/2);
    durLabel.setBounds(durArea.withHeight(12).withY(sortLabels.getY()));
    durationKnob.setBounds(durArea.reduced(3).withY(durArea.getY()+14));

    auto btns = sortRow.removeFromLeft(kw*2).reduced(3).withY(sortRow.getY()+4);
    loadButton.setBounds(btns.removeFromTop(28));
    btns.removeFromTop(3);
    auto b2 = btns;
    panicButton.setBounds(b2.removeFromLeft(b2.getWidth()/2 - 2).withHeight(22));
    perfToggle .setBounds(b2.withHeight(22));

    a.removeFromTop(4);

    // ── SYNTH row ──
    auto synthArea = a.removeFromTop(86);
    synthLabel.setBounds(synthArea.getX(), synthArea.getY()-2, 50, 12);

    auto synthLabels = synthArea.removeFromTop(12);
    auto synthRow = synthArea;
    int skw = synthRow.getWidth() / 6;

    auto atkCol = synthRow.removeFromLeft(skw);
    attackLabel.setBounds(atkCol.withHeight(12).withY(synthLabels.getY()));
    attackKnob .setBounds(atkCol.reduced(3).withY(atkCol.getY()+14));

    auto dcyCol = synthRow.removeFromLeft(skw);
    decayLabel.setBounds(dcyCol.withHeight(12).withY(synthLabels.getY()));
    decayKnob .setBounds(dcyCol.reduced(3).withY(dcyCol.getY()+14));

    auto susCol = synthRow.removeFromLeft(skw);
    sustainLabel.setBounds(susCol.withHeight(12).withY(synthLabels.getY()));
    sustainKnob.setBounds(susCol.reduced(3).withY(susCol.getY()+14));

    auto relCol = synthRow.removeFromLeft(skw);
    releaseLabel.setBounds(relCol.withHeight(12).withY(synthLabels.getY()));
    releaseKnob.setBounds(relCol.reduced(3).withY(relCol.getY()+14));

    auto gainCol = synthRow.removeFromLeft(skw);
    gainLabel.setBounds(gainCol.withHeight(12).withY(synthLabels.getY()));
    gainKnob .setBounds(gainCol.reduced(3).withY(gainCol.getY()+14));

    auto panCol = synthRow;
    panLabel.setBounds(panCol.withHeight(12).withY(synthLabels.getY()));
    panKnob  .setBounds(panCol.reduced(3).withY(panCol.getY()+14));

    // ── Waveform ──
    a.removeFromTop(6);
    waveform.setBounds(a.removeFromTop(56).withTrimmedLeft(2).withTrimmedRight(2));

    // ── Sort visualizer ──
    a.removeFromTop(6);
    visualizer.setBounds(a.removeFromBottom(a.getHeight() - 22));

    // ── Status bar ──
    a.removeFromTop(2);
    statusLabel.setBounds(a.removeFromBottom(16));
}

bool SortSynthAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files) {
    for (auto& f : files) { auto e=juce::File(f).getFileExtension().toLowerCase();
        if (e==".wav"||e==".aiff"||e==".flac"||e==".ogg"||e==".mp3") return true; }
    return false;
}
void SortSynthAudioProcessorEditor::filesDropped(const juce::StringArray& f, int, int) {
    if(!f.isEmpty())loadFile(juce::File(f[0]));
}

void SortSynthAudioProcessorEditor::loadFile(const juce::File& file) {
    auto r = processor.loadFile(file);
    juce::String m; juce::Colour c = laf.textDim;
    switch(r){
        case LoadResult::OK:
            m = "Loaded: " + file.getFileName();
            c = laf.accent3;
            waveform.setAudioData(processor.grainEngine.getAudioBuffer(),
                                  processor.grainEngine.getSampleRate());
            break;
        case LoadResult::FileNotFound: m = "Error: Not found"; break;
        case LoadResult::UnsupportedFormat: m = "Error: Bad format"; break;
        case LoadResult::TooLarge: m = "Error: >100MB"; break;
        default: m = "Error: Unknown"; break;
    }
    statusLabel.setText(m, juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, c);
}

void SortSynthAudioProcessorEditor::timerCallback() {
    if (processor.grainEngine.isLoaded()) {
        int activeVoices = 0;
        juce::String progress;
        for (auto& v : processor.getVoiceStates()) {
            if (v.active) {
                activeVoices++;
                if (v.totalSteps > 0) {
                    int pct = static_cast<int>(100.0f * v.stepIndex / v.totalSteps);
                    if (progress.isNotEmpty()) progress += " | ";
                    progress += "V" + juce::String(activeVoices) + " " + juce::String(pct) + "%";
                }
            }
        }
        juce::String info = juce::String(activeVoices) + "/4 voices";
        if (progress.isNotEmpty()) info += "  ::  " + progress;
        info += "  ::  " + juce::String(processor.grainEngine.getGrains().size()) + " grains";
        statusLabel.setText(info, juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, laf.textDim);

        double maxPos = 0.0;
        for (const auto& gv : processor.getGrainVoices()) {
            if (gv.isActive()) {
                double pos = gv.getPlaybackPos();
                if (pos > maxPos) maxPos = pos;
            }
        }
        waveform.setPlaybackPosition(maxPos);
        waveform.repaint();
    }
    visualizer.updateState(processor.getVoiceStates());
    visualizer.repaint();
}


// ── About dialog (easter egg: click the "Sieve" title) ──
struct AboutContent : public juce::Component {
    juce::Colour bg, accent, dim, main;
    AboutContent(const juce::Colour& b, const juce::Colour& a, const juce::Colour& d, const juce::Colour& m)
        : bg(b), accent(a), dim(d), main(m) { setSize(340, 240); }

    void paint(juce::Graphics& g) override {
        g.fillAll(bg);
        g.setColour(accent);
        g.setFont(juce::Font(juce::FontOptions().withHeight(22.0f).withStyle("Bold")));
        g.drawText("Sieve", 0, 8, 340, 30, juce::Justification::centred);

        g.setColour(dim);
        g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
        g.drawText("Algorithmic Granular Synthesizer", 0, 34, 340, 14, juce::Justification::centred);

        g.setColour(accent.withAlpha(0.2f));
        g.drawHorizontalLine(60, 40.0f, 300.0f);

        g.setColour(main);
        g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
        int y = 74;
        g.drawText("Built with JUCE", 0, y, 340, 16, juce::Justification::centred); y += 16;
        g.drawText("GPLv3 License", 0, y, 340, 16, juce::Justification::centred); y += 18;
        g.drawText("Developer: UnpureBloom", 0, y, 340, 16, juce::Justification::centred); y += 22;

        g.setColour(accent);
        g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f).withStyle("Bold")));
        g.drawText("bilibili: @UnpureBloom", 0, y, 340, 16, juce::Justification::centred); y += 24;

        g.setColour(dim);
        g.setFont(juce::Font(juce::FontOptions().withHeight(8.0f)));
        g.drawText("Click to open  |  Click outside to close", 0, y, 340, 14, juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent&) override {
        juce::URL("https://space.bilibili.com/227573145").launchInDefaultBrowser();
    }
};

void SortSynthAudioProcessorEditor::mouseDown(const juce::MouseEvent& e) {
    if (titleLabel.getBounds().contains(e.getPosition()))
        showAbout();
}

void SortSynthAudioProcessorEditor::showAbout() {
    auto* content = new AboutContent(laf.bgDark, laf.accent, laf.textDim, laf.textMain);
    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = "About Sieve";
    opts.dialogBackgroundColour = laf.bgDark;
    opts.content.setOwned(content);
    opts.componentToCentreAround = this;
    opts.launchAsync();
}
} // namespace SortSynth
