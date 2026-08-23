juce_add_plugin(Sieve
    COMPANY_NAME      "UnpureBloom"
    PLUGIN_MANUFACTURER_CODE UPBL
    PLUGIN_CODE        Siev
    FORMATS            VST3
    PRODUCT_NAME       "Sieve"
    VERSION            "1.0.0"
    BUNDLE_ID          "com.unpurebloom.sieve"
    IS_SYNTH           TRUE
    NEEDS_MIDI_INPUT   TRUE
)

target_sources(Sieve PRIVATE
    PluginProcessor.h
    PluginProcessor.cpp
    PluginEditor.h
    PluginEditor.cpp
    Core/Common.h
    Core/GrainEngine.h
    Core/GrainEngine.cpp
    Core/GrainVoice.h
    Core/GrainVoice.cpp
    Core/SortingAlgorithms.h
    Core/SortingAlgorithms.cpp
    UI/SortVisualizer.h
    UI/SortVisualizer.cpp
    UI/SortSynthLAF.h
    UI/SortSynthLAF.cpp
    UI/WaveformDisplay.h
    UI/WaveformDisplay.cpp
)

target_compile_options(Sieve PRIVATE /utf-8)

target_compile_definitions(Sieve PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISABLE_JUCE_VERSION_PRINTING=1
)

target_include_directories(Sieve PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(Sieve
    PRIVATE
        juce::juce_audio_utils
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)