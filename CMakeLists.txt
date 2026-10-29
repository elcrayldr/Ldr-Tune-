cmake_minimum_required(VERSION 3.22)
project(CrayLdrRealtimeTune VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE se descarga automaticamente si no existe una copia local.
include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.9
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(juce)

juce_add_plugin(CrayLdrRealtimeTune
    COMPANY_NAME "CRAY LDR DSP"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Cray
    PLUGIN_CODE Cral
    FORMATS VST3
    PRODUCT_NAME "CRAY LDR REALTIME TUNE"
)

# Los .cpp y .h estan actualmente en la raiz del repositorio.
target_sources(CrayLdrRealtimeTune PRIVATE
    PluginProcessor.cpp
    PluginProcessor.h
    PluginEditor.cpp
    PluginEditor.h
    YinPitchDetector.h
    ScaleQuantizer.h
    WsolaPitchShifter.h
    CrayCustomLookAndFeel.h
)

target_include_directories(CrayLdrRealtimeTune PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_definitions(CrayLdrRealtimeTune PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(CrayLdrRealtimeTune
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

if (MSVC)
    target_compile_options(CrayLdrRealtimeTune PRIVATE /O2 /Oi /Ot /fp:fast /MP)
endif()

option(BUILD_DSP_TESTS "Compilar tests DSP" OFF)
if(BUILD_DSP_TESTS)
    add_executable(CrayLdrDspTests DspTests.cpp)
    target_include_directories(CrayLdrDspTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_features(CrayLdrDspTests PRIVATE cxx_std_20)
endif()
