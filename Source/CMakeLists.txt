set(SIEVE_PLUGIN_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND SIEVE_PLUGIN_FORMATS AU)
    if(CMAKE_GENERATOR STREQUAL "Xcode")
        list(APPEND SIEVE_PLUGIN_FORMATS AUv3)
    endif()
endif()

juce_add_plugin(Sieve
    COMPANY_NAME      "UnpureBloom"
    PLUGIN_MANUFACTURER_CODE UPBL
    PLUGIN_CODE        Siev
    FORMATS            ${SIEVE_PLUGIN_FORMATS}
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

if(MSVC)
    target_compile_options(Sieve PRIVATE /utf-8)
endif()

juce_add_binary_data(SieveWebUI SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../Resources/web/index.html
    ${CMAKE_CURRENT_SOURCE_DIR}/../Resources/web/app.css
    ${CMAKE_CURRENT_SOURCE_DIR}/../Resources/web/app.js
)

target_compile_definitions(Sieve PRIVATE
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISABLE_JUCE_VERSION_PRINTING=1
)

if(WIN32)
    target_compile_definitions(Sieve PRIVATE JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1)
endif()

target_include_directories(Sieve PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(Sieve
    PRIVATE
        juce::juce_audio_utils
        juce::juce_gui_extra
        juce::juce_dsp
        SieveWebUI
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(WIN32)
    target_link_libraries(Sieve PRIVATE juce::juce_webview2)
endif()
