cmake_minimum_required(VERSION 3.24)
project(PonteMC2000 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PONTE_COMPANY_NAME "PonteDSP")
set(PONTE_DSP_MONOREPO_JUCE "${CMAKE_CURRENT_LIST_DIR}/../../external/JUCE")
if (EXISTS "${PONTE_DSP_MONOREPO_JUCE}/CMakeLists.txt")
    add_subdirectory("${PONTE_DSP_MONOREPO_JUCE}" "${CMAKE_CURRENT_BINARY_DIR}/JUCE")
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.1
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(PonteMC2000
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "PonteDSP zero-latency multiband compressor"
    COMPANY_NAME "${PONTE_COMPANY_NAME}"
    BUNDLE_ID "it.pontedsp.mbc4"
    PLUGIN_MANUFACTURER_CODE Pont
    PLUGIN_CODE Mbc4
    PRODUCT_NAME "Ponte MBC4"
    FORMATS VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE)

set(MC2000_DSP_SOURCES
    Source/DSP/CrossoverNetwork.cpp
    Source/DSP/MultiBandCompressor.cpp)

target_sources(PonteMC2000 PRIVATE
    ${MC2000_DSP_SOURCES}
    Source/Parameters.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/PonteLookAndFeel.cpp)

target_include_directories(PonteMC2000 PRIVATE
    Source
    Source/UI)

target_compile_definitions(PonteMC2000 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(PonteMC2000 PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

option(MC2000_BUILD_TESTS "Build Ponte MC2000 DSP tests" ON)
if (MC2000_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(MC2000Tests PRODUCT_NAME "Ponte MBC4 Tests")
    target_sources(MC2000Tests PRIVATE
        Tests/MC2000Tests.cpp
        ${MC2000_DSP_SOURCES})
    target_include_directories(MC2000Tests PRIVATE Source)
    target_link_libraries(MC2000Tests PRIVATE juce::juce_core)
    add_test(NAME MC2000Tests COMMAND MC2000Tests)

    juce_add_console_app(MC2000BlackBoxAnalysis PRODUCT_NAME "Ponte MBC4 Black Box Analysis")
    target_sources(MC2000BlackBoxAnalysis PRIVATE
        Research/BlackBoxAnalysis.cpp
        ${MC2000_DSP_SOURCES})
    target_include_directories(MC2000BlackBoxAnalysis PRIVATE Source)
    target_link_libraries(MC2000BlackBoxAnalysis PRIVATE
        juce::juce_audio_formats
        juce::juce_core
        juce::juce_recommended_config_flags)
endif()
