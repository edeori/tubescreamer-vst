cmake_minimum_required(VERSION 3.22)
project(MothBite VERSION 0.1.0 LANGUAGES C CXX)

include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.6 GIT_SHALLOW TRUE)
FetchContent_Declare(chowdsp_wdf GIT_REPOSITORY https://github.com/Chowdhury-DSP/chowdsp_wdf.git GIT_TAG 43bcd295e8403de913f9966f4fd7ff9d17c9f1a5)
FetchContent_MakeAvailable(JUCE chowdsp_wdf)

option(MOTHBITE_LAYOUT_TUNER "Enable the debug-only JUCE layout tuner" OFF)

if (WIN32)
    set(MOTHBITE_COPY_PLUGIN_AFTER_BUILD FALSE)
else()
    set(MOTHBITE_COPY_PLUGIN_AFTER_BUILD TRUE)
endif()

juce_add_plugin(MothBite
    COMPANY_NAME "Moth Production"
    BUNDLE_ID "com.mothproduction.mothbite"
    PLUGIN_MANUFACTURER_CODE Moth
    PLUGIN_CODE Mbit
    FORMATS VST3 AU Standalone
    COPY_PLUGIN_AFTER_BUILD ${MOTHBITE_COPY_PLUGIN_AFTER_BUILD}
    PRODUCT_NAME "MothBite"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE)

# JUCE's default is build/MothBite_artefacts/<Config>/<Format>/... - flatten that one level so
# builds land in build/<Config>/<Format>/... instead. build.sh then copies these into the final
# flattened build/{VST3,AU,Standalone}/ locations.
set_target_properties(MothBite PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>")

set(MOTHBITE_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DSP/ClipperStage.cpp
    Source/DSP/ToneStage.cpp)

juce_add_binary_data(MothBiteAssets
    SOURCES
        Source/Assets/pedal.png
        Source/Assets/footswitch.png
        Source/Assets/moth_mark.png
        Source/Assets/knob_modern.svg
        Source/Assets/knob_modern_pointer.svg
        Source/Assets/oxanium_bold.ttf
        Source/Assets/barlow_condensed_medium.ttf)

target_sources(MothBite PRIVATE ${MOTHBITE_SOURCES})
target_include_directories(MothBite PRIVATE Source)
target_compile_features(MothBite PRIVATE cxx_std_17)
target_compile_definitions(MothBite PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

if (MOTHBITE_LAYOUT_TUNER)
    target_compile_definitions(MothBite PRIVATE JUCE_LAYOUT_TUNER=1)
endif()

target_link_libraries(MothBite PRIVATE MothBiteAssets juce::juce_audio_utils juce::juce_dsp chowdsp::chowdsp_wdf
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

juce_add_console_app(ClipperStageTests PRODUCT_NAME "ClipperStage Tests")
target_sources(ClipperStageTests PRIVATE Tests/ClipperStageTests.cpp Source/DSP/ClipperStage.cpp)
target_include_directories(ClipperStageTests PRIVATE Source)
target_compile_features(ClipperStageTests PRIVATE cxx_std_17)
target_compile_definitions(ClipperStageTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(ClipperStageTests PRIVATE juce::juce_audio_basics juce::juce_dsp chowdsp::chowdsp_wdf
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(ToneStageTests PRODUCT_NAME "ToneStage Tests")
target_sources(ToneStageTests PRIVATE Tests/ToneStageTests.cpp Source/DSP/ToneStage.cpp)
target_include_directories(ToneStageTests PRIVATE Source)
target_compile_features(ToneStageTests PRIVATE cxx_std_17)
target_compile_definitions(ToneStageTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(ToneStageTests PRIVATE juce::juce_audio_basics juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

enable_testing()
add_test(NAME clipper-stage-dsp COMMAND ClipperStageTests)
add_test(NAME tone-stage-dsp COMMAND ToneStageTests)

# --- scratch measurement tool: measures how fast AdaptiveGate closes after a note-off, ahead of
# MothBite's clipper, sweeping hold/release multipliers for a tighter rhythm-playing response.
# Remove this block plus Tools/MeasureGateClose.cpp once the tuning session is done. ---
set(ADAPTIVEGATE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../AdaptiveGate-VST/Source")
if (EXISTS "${ADAPTIVEGATE_SOURCE_DIR}/DSP/AdaptiveGateEngine.h")
    juce_add_console_app(GateCloseMeasure PRODUCT_NAME "GateCloseMeasure")
    target_sources(GateCloseMeasure PRIVATE
        Tools/MeasureGateClose.cpp
        Source/DSP/ClipperStage.cpp
        Source/DSP/ToneStage.cpp
        "${ADAPTIVEGATE_SOURCE_DIR}/DSP/AdaptiveGateEngine.cpp"
        "${ADAPTIVEGATE_SOURCE_DIR}/DSP/EnvelopeFollower.cpp"
        "${ADAPTIVEGATE_SOURCE_DIR}/DSP/FilterBank.cpp"
        "${ADAPTIVEGATE_SOURCE_DIR}/DSP/GainEnvelope.cpp"
        "${ADAPTIVEGATE_SOURCE_DIR}/DSP/GateDecision.cpp"
        "${ADAPTIVEGATE_SOURCE_DIR}/DSP/NoiseFloorTracker.cpp"
        "${ADAPTIVEGATE_SOURCE_DIR}/DSP/TransientDetector.cpp"
        "${ADAPTIVEGATE_SOURCE_DIR}/Presets/FrequencyProfile.cpp"
        "${ADAPTIVEGATE_SOURCE_DIR}/Presets/Coherence.cpp")
    target_include_directories(GateCloseMeasure PRIVATE Source "${ADAPTIVEGATE_SOURCE_DIR}")
    target_compile_features(GateCloseMeasure PRIVATE cxx_std_17)
    target_compile_definitions(GateCloseMeasure PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(GateCloseMeasure PRIVATE juce::juce_audio_formats juce::juce_dsp chowdsp::chowdsp_wdf
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
