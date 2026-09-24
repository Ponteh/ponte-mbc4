cmake_minimum_required(VERSION 3.24)
project(PonteMC2000 VERSION 0.2.3 LANGUAGES C CXX)

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

# JUCE 9.0.1's RC rule omits Info.txt from its dependencies. Without it,
# incremental version bumps can leave Windows FileVersion at the old value
# even when the VST3 factory/manifest already reports the new version.
if (WIN32)
    get_target_property(mc2000_generated_dir PonteMC2000 JUCE_GENERATED_SOURCES_DIRECTORY)
    get_target_property(mc2000_info_file PonteMC2000 JUCE_INFO_FILE)
    add_custom_command(
        OUTPUT "${mc2000_generated_dir}/PonteMC2000_resources.rc"
        APPEND DEPENDS "${mc2000_info_file}")
endif()

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
    juce::juce_dsp
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

    juce_add_console_app(MC2000UITests PRODUCT_NAME "Ponte MBC4 UI Tests")
    target_sources(MC2000UITests PRIVATE
        Tests/MC2000UITests.cpp
        Tests/RealtimeAudit.cpp
        ${MC2000_DSP_SOURCES}
        Source/Parameters.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp
        Source/UI/PonteLookAndFeel.cpp)
    target_include_directories(MC2000UITests PRIVATE Source Source/UI)
    target_compile_definitions(MC2000UITests PRIVATE
        JucePlugin_Name="Ponte MBC4" JucePlugin_VersionString="${PROJECT_VERSION}"
        PONTE_DISABLE_NETWORK_CHECK=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        MC2000_TECHNICAL_TESTS=1
        JUCE_REPORT_APP_USAGE=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(MC2000UITests PRIVATE
        juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp juce::juce_gui_extra)
    if (WIN32)
        target_link_libraries(MC2000UITests PRIVATE psapi)
    endif()
    add_test(NAME MC2000UITests COMMAND MC2000UITests)
    if (WIN32)
        add_test(NAME MC2000RealtimeMatrix COMMAND MC2000UITests --technical-matrix)
        set_tests_properties(MC2000RealtimeMatrix PROPERTIES TIMEOUT 300)
    endif()

    juce_add_console_app(MC2000UpdateProbe PRODUCT_NAME "Ponte Update Check Probe")
    target_sources(MC2000UpdateProbe PRIVATE Tests/UpdateCheckProbe.cpp)
    target_include_directories(MC2000UpdateProbe PRIVATE Source)
    target_compile_definitions(MC2000UpdateProbe PRIVATE JUCE_USE_CURL=0)
    target_link_libraries(MC2000UpdateProbe PRIVATE juce::juce_core)

    add_executable(MC2000NapValidation Research/NapValidation.cpp ${MC2000_DSP_SOURCES})
    target_include_directories(MC2000NapValidation PRIVATE Source)
    add_test(NAME MC2000NapValidation COMMAND MC2000NapValidation)

    add_executable(MC2000Performance Research/PerformanceBenchmark.cpp ${MC2000_DSP_SOURCES})
    target_include_directories(MC2000Performance PRIVATE Source)
    add_executable(MC2000OriginalPackRender Research/OriginalPackRender.cpp ${MC2000_DSP_SOURCES})
    target_include_directories(MC2000OriginalPackRender PRIVATE Source)
    set(MC2000_BASELINE_SOURCE_DIR "" CACHE PATH "Frozen baseline Source directory for performance/audio comparisons")
    if (MC2000_BASELINE_SOURCE_DIR)
        add_executable(MC2000PerformanceBaseline Research/PerformanceBenchmark.cpp
            "${MC2000_BASELINE_SOURCE_DIR}/DSP/MultiBandCompressor.cpp"
            "${MC2000_BASELINE_SOURCE_DIR}/DSP/CrossoverNetwork.cpp")
        target_include_directories(MC2000PerformanceBaseline PRIVATE "${MC2000_BASELINE_SOURCE_DIR}")
        add_executable(MC2000OriginalPackRenderBaseline Research/OriginalPackRender.cpp
            "${MC2000_BASELINE_SOURCE_DIR}/DSP/MultiBandCompressor.cpp"
            "${MC2000_BASELINE_SOURCE_DIR}/DSP/CrossoverNetwork.cpp")
        target_include_directories(MC2000OriginalPackRenderBaseline PRIVATE "${MC2000_BASELINE_SOURCE_DIR}")
    endif()

    juce_add_console_app(MC2000BlackBoxAnalysis PRODUCT_NAME "Ponte MBC4 Black Box Analysis")
    target_sources(MC2000BlackBoxAnalysis PRIVATE
        Research/BlackBoxAnalysis.cpp
        ${MC2000_DSP_SOURCES})
    target_include_directories(MC2000BlackBoxAnalysis PRIVATE Source)
    target_link_libraries(MC2000BlackBoxAnalysis PRIVATE
        juce::juce_audio_formats
        juce::juce_core
        juce::juce_recommended_config_flags)

    juce_add_console_app(MC2000ExtendedAnalysis PRODUCT_NAME "Ponte MBC4 Extended Test-Pack Analysis")
    target_sources(MC2000ExtendedAnalysis PRIVATE
        Research/ExtendedPackAnalysis.cpp)
    target_include_directories(MC2000ExtendedAnalysis PRIVATE Source)
    target_link_libraries(MC2000ExtendedAnalysis PRIVATE
        juce::juce_audio_formats
        juce::juce_core
        juce::juce_recommended_config_flags)

    juce_add_console_app(MC2000NextPackAnalysis PRODUCT_NAME "Ponte MBC4 Next Validation-Pack Analysis")
    target_sources(MC2000NextPackAnalysis PRIVATE
        Research/NextPackAnalysis.cpp)
    target_include_directories(MC2000NextPackAnalysis PRIVATE Source)
    target_link_libraries(MC2000NextPackAnalysis PRIVATE
        juce::juce_audio_formats
        juce::juce_core
        juce::juce_recommended_config_flags)
endif()
