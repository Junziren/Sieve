#include "PluginEditor.h"
#include <BinaryData.h>
#include <cmath>
#include <cstring>

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

static bool isSupportedAudioFile(const juce::File& file)
{
    const auto extension = file.getFileExtension().toLowerCase();
    return extension == ".wav" || extension == ".aif" || extension == ".aiff"
        || extension == ".flac" || extension == ".ogg" || extension == ".mp3";
}

//==============================================================================
SortSynthAudioProcessorEditor::SortSynthAudioProcessorEditor(SortSynthAudioProcessor& p)
    : AudioProcessorEditor(&p)
    , processor(p)
{
    // Keep one stable desktop instrument size. The WebView backend is selected
    // per platform and all audio-facing state stays owned by the processor.
    setResizable(false, false);

    auto options = juce::WebBrowserComponent::Options{}
                       .withBackend(juce::WebBrowserComponent::Options::Backend::defaultBackend)
                       .withAppleWkWebViewOptions(
                           juce::WebBrowserComponent::Options::AppleWkWebView{}
                               .withDisabledAcceptsFirstMouse())
                       .withKeepPageLoadedWhenBrowserIsHidden()
                       .withNativeIntegrationEnabled()
                       .withNativeFunction("uiReady",
                                           [this](const auto&, auto complete)
                                           {
                                               uiReady = true;
                                               sendParameterState();
                                               sendSampleOverview();
                                               if (complete != nullptr)
                                                   complete(true);
                                           })
                       .withNativeFunction("setParameter",
                                           [this](const auto& args, auto complete)
                                           {
                                               handleSetParameter(args, std::move(complete));
                                           })
                       .withNativeFunction("loadFile",
                                           [this](const auto&, auto complete)
                                           {
                                               handleLoadFile(std::move(complete));
                                           })
                       .withNativeFunction("loadFileData",
                                           [this](const auto& args, auto complete)
                                           {
                                               handleLoadFileData(args, std::move(complete));
                                           })
                       .withResourceProvider(serveWebResource);

#if JUCE_WINDOWS
    auto userData = juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getChildFile("SieveWebView2");
    options = options
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options(
            juce::WebBrowserComponent::Options::WinWebView2{}
                .withUserDataFolder(userData)
                .withStatusBarDisabled()
                .withBuiltInErrorPageDisabled());
#endif

    // Keep the same document alive while a DAW hides and re-shows the editor.
    // A new editor instance may still be created by the host, but visibility
    // changes alone must not turn into an about:blank navigation.
    options = options.withKeepPageLoadedWhenBrowserIsHidden();

    webView = std::make_unique<juce::WebBrowserComponent>(std::move(options));
    addAndMakeVisible(*webView);
    webView->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());

    loadErrorLabel.setText(
        "Embedded browser unavailable\n\nSieve needs the platform WebView runtime\n(WebView2 on Windows, WebKit on Apple).",
        juce::dontSendNotification);
    loadErrorLabel.setJustificationType(juce::Justification::centred);
    loadErrorLabel.setColour(juce::Label::textColourId, juce::Colour(0xffb3b3bc));
    loadErrorLabel.setVisible(false);
    addChildComponent(loadErrorLabel);

    // Set the size after the WebView exists so the first resized() lays it out.
    setSize(1100, 660);
    startTimerHz(15);
}

SortSynthAudioProcessorEditor::~SortSynthAudioProcessorEditor()
{
    stopTimer();
    fileChooser.reset();
    webView.reset();
}

void SortSynthAudioProcessorEditor::timerCallback()
{
    if (!uiReady || webView == nullptr)
        return;

    SortSynthAudioProcessor::UiFrame frame;
    bool receivedFrame = false;
    while (processor.popUiFrame(frame))
        receivedFrame = true;

    if (receivedFrame) {
        if (frame.sampleGeneration != lastSampleGeneration)
            sendSampleOverview();
        sendUiFrame(frame);
    }

    // This is intentionally message-thread work. It keeps host automation and
    // restored APVTS state authoritative without asking the audio callback to
    // touch the WebView.
    sendParameterState();
}

void SortSynthAudioProcessorEditor::handleSetParameter(
    const juce::Array<juce::var>& args,
    juce::WebBrowserComponent::NativeFunctionCompletion complete)
{
    bool succeeded = false;

    if (args.size() >= 3 && args[0].isString()) {
        const auto parameterId = args[0].toString();
        const auto phase = args[2].toString();
        const bool validPhase = phase == "begin" || phase == "change" || phase == "end";

        if (validPhase && isAllowedParameter(parameterId)
            && (args[1].isDouble() || args[1].isInt() || args[1].isInt64())) {
            if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(
                    processor.apvts.getParameter(parameterId))) {
                const auto rawInput = static_cast<float>(static_cast<double>(args[1]));

                if (std::isfinite(rawInput)) {
                    const auto range = parameter->getNormalisableRange();
                    const auto rawValue = range.snapToLegalValue(
                        juce::jlimit(range.start, range.end, rawInput));

                    if (phase == "begin")
                        parameter->beginChangeGesture();

                    parameter->setValueNotifyingHost(range.convertTo0to1(rawValue));

                    if (phase == "end")
                        parameter->endChangeGesture();

                    succeeded = true;
                }
            }
        }
    }

    if (complete != nullptr)
        complete(succeeded);
}

void SortSynthAudioProcessorEditor::handleLoadFile(
    juce::WebBrowserComponent::NativeFunctionCompletion complete)
{
    if (fileChooser != nullptr) {
        if (complete != nullptr)
            complete(false);
        return;
    }

    fileChooser = std::make_unique<juce::FileChooser>(
        "Load audio sample", juce::File{}, "*.wav;*.aiff;*.aif;*.flac;*.ogg;*.mp3");
    auto safeThis = juce::Component::SafePointer<SortSynthAudioProcessorEditor>(this);

    fileChooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safeThis](const juce::FileChooser& chooser)
        {
            if (safeThis == nullptr)
                return;

            const auto file = chooser.getResult();
            safeThis->fileChooser.reset();
            if (!file.existsAsFile())
                return;
            safeThis->loadSampleFile(file);
        });

    if (complete != nullptr)
        complete(true);
}

void SortSynthAudioProcessorEditor::handleLoadFileData(
    const juce::Array<juce::var>& args,
    juce::WebBrowserComponent::NativeFunctionCompletion complete)
{
    bool succeeded = false;
    bool attemptedLoad = false;

    if (!sampleLoadInProgress && args.size() >= 2
        && args[0].isString() && args[1].isString()) {
        const auto fileName = juce::File(args[0].toString()).getFileName();
        const auto extension = juce::File(fileName).getFileExtension().toLowerCase();
        const auto encoded = args[1].toString();
        constexpr auto maxEncodedBytes = ((GrainEngine::MAX_FILE_BYTES + 2u) / 3u) * 4u + 4u;

        if (isSupportedAudioFile(juce::File(fileName))
            && static_cast<size_t>(encoded.getNumBytesAsUTF8()) <= maxEncodedBytes) {
            // FileReader produces standard RFC 4648 Base64. MemoryBlock's
            // fromBase64Encoding() expects JUCE's legacy dotted format.
            juce::MemoryOutputStream decoded;
            if (juce::Base64::convertFromBase64(decoded, encoded)
                && decoded.getDataSize() > 0
                && decoded.getDataSize() <= GrainEngine::MAX_FILE_BYTES) {
                const auto temporaryFile = juce::File::createTempFile(extension);
                if (temporaryFile.replaceWithData(decoded.getData(), decoded.getDataSize())) {
                    attemptedLoad = true;
                    const auto result = loadSampleFile(temporaryFile);
                    succeeded = result == LoadResult::OK;
                    temporaryFile.deleteFile();
                }
            }
        }
    }

    if (!succeeded && !attemptedLoad && webView != nullptr)
        webView->emitEventIfBrowserIsVisible(
            "sieveStatus", juce::var("Load failed: dropped file data is invalid"));

    if (complete != nullptr)
        complete(succeeded);
}

LoadResult SortSynthAudioProcessorEditor::loadSampleFile(const juce::File& file)
{
    if (sampleLoadInProgress)
        return LoadResult::FileNotFound;

    sampleLoadInProgress = true;
    if (webView != nullptr)
        webView->emitEventIfBrowserIsVisible("sieveStatus", juce::var("Loading sample..."));

    // Loading reallocates the sample and grain tables. Suspend the callback
    // while doing this so the audio thread cannot observe a half-loaded engine.
    processor.suspendProcessing(true);
    const auto result = processor.loadFile(file);
    processor.suspendProcessing(false);
    sampleLoadInProgress = false;

    sendSampleOverview();
    if (webView != nullptr)
        webView->emitEventIfBrowserIsVisible(
            "sieveStatus", juce::var(loadResultToString(result)));
    return result;
}

bool SortSynthAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& path : files) {
        const juce::File file(path);
        if (file.existsAsFile() && isSupportedAudioFile(file))
            return true;
    }
    return false;
}

void SortSynthAudioProcessorEditor::fileDragEnter(const juce::StringArray&, int, int)
{
    if (webView != nullptr)
        webView->emitEventIfBrowserIsVisible("sieveDropState", juce::var(true));
}

void SortSynthAudioProcessorEditor::fileDragExit(const juce::StringArray&)
{
    if (webView != nullptr)
        webView->emitEventIfBrowserIsVisible("sieveDropState", juce::var(false));
}

void SortSynthAudioProcessorEditor::filesDropped(
    const juce::StringArray& files, int, int)
{
    if (webView != nullptr)
        webView->emitEventIfBrowserIsVisible("sieveDropState", juce::var(false));

    if (sampleLoadInProgress)
        return;

    for (const auto& path : files) {
        const juce::File file(path);
        if (file.existsAsFile() && isSupportedAudioFile(file)) {
            loadSampleFile(file);
            break;
        }
    }
}

void SortSynthAudioProcessorEditor::sendParameterState()
{
    if (webView != nullptr)
        webView->emitEventIfBrowserIsVisible("sieveParameterState", makeParameterState());
}

void SortSynthAudioProcessorEditor::sendSampleOverview()
{
    if (webView == nullptr)
        return;

    std::array<float, SortSynthAudioProcessor::UI_WAVEFORM_POINTS> minValues{};
    std::array<float, SortSynthAudioProcessor::UI_WAVEFORM_POINTS> maxValues{};
    int pointCount = 0;
    uint64_t generation = 0;
    processor.copySampleOverview(minValues, maxValues, pointCount, generation);

    juce::Array<juce::var> points;
    for (int i = 0; i < pointCount; ++i) {
        juce::Array<juce::var> point;
        point.add(minValues[static_cast<size_t>(i)]);
        point.add(maxValues[static_cast<size_t>(i)]);
        points.add(std::move(point));
    }

    juce::DynamicObject::Ptr state(new juce::DynamicObject());
    state->setProperty("points", std::move(points));
    state->setProperty("slices", processor.apvts.getRawParameterValue("sliceCount")
                                      ->load(std::memory_order_relaxed));
    state->setProperty("generation", static_cast<juce::int64>(generation));
    webView->emitEventIfBrowserIsVisible("sieveSampleOverview", juce::var(state.get()));
    lastSampleGeneration = generation;
}

void SortSynthAudioProcessorEditor::sendUiFrame(
    const SortSynthAudioProcessor::UiFrame& frame)
{
    if (webView == nullptr)
        return;

    juce::Array<juce::var> voiceStates;
    int playbackVoice = -1;

    for (int i = 0; i < SortSynthAudioProcessor::MAX_VOICES; ++i) {
        const auto& source = frame.voices[static_cast<size_t>(i)];
        auto voice = juce::DynamicObject::Ptr(new juce::DynamicObject());
        juce::Array<juce::var> values;

        for (int valueIndex = 0; valueIndex < source.valueCount; ++valueIndex)
            values.add(source.currentValues[static_cast<size_t>(valueIndex)]);

        voice->setProperty("currentValues", std::move(values));
        voice->setProperty("sliceCount", frame.sliceCount);
        voice->setProperty("stepIndex", source.stepIndex);
        voice->setProperty("totalSteps", source.totalSteps);
        voice->setProperty("progress", source.totalSteps > 0
                                          ? static_cast<double>(source.stepIndex)
                                                / static_cast<double>(source.totalSteps)
                                          : 0.0);
        voice->setProperty("midiNote", source.midiNote);
        voice->setProperty("active", source.active);
        voice->setProperty("paused", source.paused);
        voice->setProperty("completed", source.completed);
        voice->setProperty("playbackPosition", source.playbackPosition);
        voiceStates.add(juce::var(voice.get()));

        if (playbackVoice < 0 && source.active && !source.paused)
            playbackVoice = i;
    }

    auto state = juce::DynamicObject::Ptr(new juce::DynamicObject());
    state->setProperty("sequence", static_cast<juce::int64>(frame.sequence));
    state->setProperty("sampleLoaded", frame.sampleLoaded);
    state->setProperty("sliceCount", frame.sliceCount);
    state->setProperty("voices", std::move(voiceStates));

    if (playbackVoice >= 0)
        state->setProperty("playbackPosition",
                           frame.voices[static_cast<size_t>(playbackVoice)].playbackPosition);
    else
        state->setProperty("playbackPosition", juce::var());

    webView->emitEventIfBrowserIsVisible("sieveUiFrame", juce::var(state.get()));
}

juce::var SortSynthAudioProcessorEditor::makeParameterState() const
{
    auto state = juce::DynamicObject::Ptr(new juce::DynamicObject());
    const auto read = [this](const char* id) {
        return processor.apvts.getRawParameterValue(id)->load(std::memory_order_relaxed);
    };

    state->setProperty("algorithm", static_cast<int>(read("algorithm")));
    state->setProperty("slices", static_cast<int>(read("sliceCount")));
    state->setProperty("speed", read("sortSpeed"));
    state->setProperty("duration", read("grainDuration"));
    state->setProperty("gain", read("gain"));
    state->setProperty("pan", read("pan"));
    state->setProperty("attack", read("attack"));
    state->setProperty("decay", read("decay"));
    state->setProperty("sustain", read("sustain"));
    state->setProperty("release", read("release"));
    state->setProperty("turbo", read("performanceMode") > 0.5f);
    return juce::var(state.get());
}

bool SortSynthAudioProcessorEditor::isAllowedParameter(const juce::String& parameterId)
{
    static const juce::StringArray allowed {
        "algorithm", "sliceCount", "sortSpeed", "grainDuration", "gain", "pan",
        "attack", "decay", "sustain", "release", "performanceMode"
    };
    return allowed.contains(parameterId);
}

juce::String SortSynthAudioProcessorEditor::loadResultToString(LoadResult result)
{
    switch (result) {
        case LoadResult::OK:                 return "Sample loaded";
        case LoadResult::FileNotFound:       return "Load failed: file not found";
        case LoadResult::UnsupportedFormat:  return "Load failed: unsupported format";
        case LoadResult::TooLarge:           return "Load failed: file is larger than 100 MB";
        case LoadResult::SampleRateMismatch: return "Load failed: sample-rate mismatch";
        case LoadResult::EmptyFile:          return "Load failed: empty file";
    }
    return "Load failed";
}

void SortSynthAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff101014));
}

void SortSynthAudioProcessorEditor::resized()
{
    auto b = getLocalBounds();
    if (webView != nullptr)
        webView->setBounds(b);
    loadErrorLabel.setBounds(b);
}

} // namespace SortSynth
