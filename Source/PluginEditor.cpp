#include "PluginEditor.h"
#include <BinaryData.h>

namespace SortSynth {

//==============================================================================
// Serves the embedded web GUI (index.html / app.css / app.js) to the WebView.
static std::optional<juce::WebBrowserComponent::Resource> serveWebResource(const juce::String& url)
{
    auto path = (url == "/" || url.isEmpty())
        ? juce::String("index.html")
        : url.fromFirstOccurrenceOf("/", false, false).upToFirstOccurrenceOf("?", false, false);

    const char* data = nullptr;
    int size = 0;
    juce::String mime;

    if (path == "index.html" || path == "index_html") {
        data = BinaryData::index_html; size = BinaryData::index_htmlSize; mime = "text/html";
    } else if (path == "app.css" || path == "app_css") {
        data = BinaryData::app_css; size = BinaryData::app_cssSize; mime = "text/css";
    } else if (path == "app.js" || path == "app_js") {
        data = BinaryData::app_js; size = BinaryData::app_jsSize; mime = "application/javascript";
    } else {
        return std::nullopt;
    }

    std::vector<std::byte> bytes(static_cast<size_t>(size));
    std::memcpy(bytes.data(), data, static_cast<size_t>(size));
    return juce::WebBrowserComponent::Resource{ std::move(bytes), std::move(mime) };
}

//==============================================================================
SortSynthAudioProcessorEditor::SortSynthAudioProcessorEditor(SortSynthAudioProcessor& p)
    : AudioProcessorEditor(&p)
{
    // Visual preview shell: fixed size, DSP deliberately not wired yet.
    setResizable(false, false);
    setSize(1100, 660);

    auto userData = juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getChildFile("SieveWebView2");
    auto options = juce::WebBrowserComponent::Options{}
                       .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
                       .withWinWebView2Options(
                           juce::WebBrowserComponent::Options::WinWebView2{}
                               .withUserDataFolder(userData))
                       .withResourceProvider(serveWebResource);

    webView = std::make_unique<juce::WebBrowserComponent>(std::move(options));
    addAndMakeVisible(*webView);
    webView->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());

    loadErrorLabel.setText(
        "WebView2 unavailable\n\nSieve needs the WebView2 runtime and\nWebView2Loader.dll inside the plugin bundle.",
        juce::dontSendNotification);
    loadErrorLabel.setJustificationType(juce::Justification::centred);
    loadErrorLabel.setColour(juce::Label::textColourId, juce::Colour(0xffb3b3bc));
    loadErrorLabel.setVisible(false);
    addChildComponent(loadErrorLabel);
}

SortSynthAudioProcessorEditor::~SortSynthAudioProcessorEditor() = default;

void SortSynthAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff101014));
}

void SortSynthAudioProcessorEditor::resized()
{
    auto b = getLocalBounds();
    webView->setBounds(b);
    loadErrorLabel.setBounds(b);
}

} // namespace SortSynth
