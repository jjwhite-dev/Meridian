cmake_minimum_required(VERSION 3.22)

project(MeridianEQ VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# DSP unit tests: plain C++, no JUCE needed
# ---------------------------------------------------------------------------
option(MERIDIAN_TESTS_ONLY "Build only the DSP unit tests (skips JUCE and the plugin)" OFF)

enable_testing()
add_executable(meridian_test_design tests/test_design.cpp)
target_include_directories(meridian_test_design PRIVATE Source)
add_executable(meridian_test_engine tests/test_engine.cpp Source/dsp/EqEngine.cpp)
target_include_directories(meridian_test_engine PRIVATE Source)
add_test(NAME filter_design COMMAND meridian_test_design)
add_test(NAME engine COMMAND meridian_test_engine)

if(MERIDIAN_TESTS_ONLY)
    return()
endif()

# ---------------------------------------------------------------------------
# JUCE: use a local checkout if one is given, otherwise download it
# ---------------------------------------------------------------------------
set(MERIDIAN_JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")
if(MERIDIAN_JUCE_DIR)
    add_subdirectory(${MERIDIAN_JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.10
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(MeridianEQ
    COMPANY_NAME "Meridian"
    BUNDLE_ID "com.jjwhite.meridianeq"
    PLUGIN_MANUFACTURER_CODE Mrdn
    PLUGIN_CODE MrEq
    FORMATS VST3 AU
    PRODUCT_NAME "Meridian EQ"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx EQ)

target_sources(MeridianEQ PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Params.cpp
    Source/dsp/EqEngine.cpp
    Source/ui/LookAndFeel.cpp
    Source/ui/Controls.cpp
    Source/ui/EqDisplay.cpp
    Source/ui/Panels.cpp)

target_include_directories(MeridianEQ PRIVATE Source)

target_compile_definitions(MeridianEQ PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MeridianEQ
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
