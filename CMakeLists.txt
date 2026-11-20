cmake_minimum_required(VERSION 3.24)

if(APPLE AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS universal architectures")
endif()
if(APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
endif()

project(HarmonizerV VERSION 2.5.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HARMONIZERV_BUILD_TESTS "Build Catch2 DSP unit tests" ON)
option(HARMONIZERV_COPY_PLUGIN "Copy built VST3/AU to system plugin folder after build" OFF)
option(HARMONIZERV_ENABLE_ASIO "Enable Steinberg ASIO support on Windows Standalone" ON)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.17-security
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(
    signalsmith_stretch
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
    GIT_TAG        88061fd4eda56a2353c5d65c74aa3f4ccf76f9da
)

FetchContent_MakeAvailable(JUCE signalsmith_stretch)

add_library(HarmonizerV_DSP STATIC
    Source/Dsp/DspConstants.h
    Source/Dsp/DelayLine.h
    Source/Dsp/PitchTracker.h
    Source/Dsp/PitchTracker.cpp
    Source/Dsp/ScaleHarmonizer.h
    Source/Dsp/ScaleHarmonizer.cpp
    Source/Dsp/PitchMarkDetector.h
    Source/Dsp/PitchMarkDetector.cpp
    Source/Dsp/TdPsolaVoice.h
    Source/Dsp/TdPsolaVoice.cpp
    Source/Dsp/SpectralVoice.h
    Source/Dsp/SpectralVoice.cpp
    Source/Dsp/VoiceChannelStrip.h
    Source/Dsp/VoiceChannelStrip.cpp
    Source/Dsp/HarmonyBusFx.h
    Source/Dsp/HarmonyBusFx.cpp
    Source/Dsp/MidiHarmonyController.h
    Source/Dsp/MidiHarmonyController.cpp
    Source/Dsp/MidiControlMap.h
    Source/Dsp/VoicingPresets.h
    Source/Dsp/SnapshotBank.h
    Source/Dsp/HarmonizerEngine.h
    Source/Dsp/HarmonizerEngine.cpp
)

target_include_directories(HarmonizerV_DSP PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_link_libraries(HarmonizerV_DSP PUBLIC
    signalsmith-stretch
)

target_compile_features(HarmonizerV_DSP PUBLIC cxx_std_20)

if(MSVC)
    target_compile_definitions(HarmonizerV_DSP PUBLIC NOMINMAX _USE_MATH_DEFINES)
    target_compile_options(HarmonizerV_DSP PRIVATE /W4 /utf-8)
else()
    target_compile_options(HarmonizerV_DSP PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(APPLE)
    set(HARMONIZERV_FORMATS VST3 AU Standalone)
else()
    set(HARMONIZERV_FORMATS VST3 Standalone)
endif()

juce_add_plugin(HarmonizerV
    COMPANY_NAME "HarmonizerV Audio"
    BUNDLE_ID "com.harmonizerv.HarmonizerV"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${HARMONIZERV_COPY_PLUGIN}
    PLUGIN_MANUFACTURER_CODE HrmV
    PLUGIN_CODE Hrm1
    FORMATS ${HARMONIZERV_FORMATS}
    PRODUCT_NAME "HarmonizerV"
    VST3_CATEGORIES "Fx" "Pitch Shift"
    AU_MAIN_TYPE kAudioUnitType_Effect
)

target_sources(HarmonizerV PRIVATE
    Source/Plugin/ParameterIds.h
    Source/Plugin/SnapshotManager.h
    Source/Plugin/SnapshotManager.cpp
    Source/Plugin/LiveMidiController.h
    Source/Plugin/LiveMidiController.cpp
    Source/Plugin/PresetManager.h
    Source/Plugin/PresetManager.cpp
    Source/Plugin/PluginProcessor.h
    Source/Plugin/PluginProcessor.cpp
    Source/Plugin/PluginEditor.h
    Source/Plugin/PluginEditor.cpp
    Source/Ui/Theme.h
    Source/Ui/ModernLookAndFeel.h
    Source/Ui/ModernLookAndFeel.cpp
    Source/Ui/HeaderBar.h
    Source/Ui/HeaderBar.cpp
    Source/Ui/HarmonyFeelBar.h
    Source/Ui/HarmonyFeelBar.cpp
    Source/Ui/HarmonyStage.h
    Source/Ui/HarmonyStage.cpp
    Source/Ui/VoiceInspector.h
    Source/Ui/VoiceInspector.cpp
)

target_include_directories(HarmonizerV PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_compile_definitions(HarmonizerV PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

if(WIN32 AND HARMONIZERV_ENABLE_ASIO)
    target_compile_definitions(HarmonizerV PUBLIC JUCE_ASIO=1)
endif()

if(MSVC)
    target_compile_options(HarmonizerV PRIVATE /utf-8)
endif()

target_link_libraries(HarmonizerV
    PRIVATE
        HarmonizerV_DSP
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(HARMONIZERV_BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.7.1
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    enable_testing()
    add_subdirectory(Tests)
endif()
