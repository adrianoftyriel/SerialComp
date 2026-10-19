cmake_minimum_required(VERSION 3.22)
project(SerialComp VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.12
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(SerialComp
    VERSION                     "${PROJECT_VERSION}"
    COMPANY_NAME                "James Wallace"
    BUNDLE_ID                   "com.svt.serialcomp"
    PLUGIN_MANUFACTURER_CODE    "SVTA"
    PLUGIN_CODE                 "SRLC"
    FORMATS                     VST3 AU Standalone
    PRODUCT_NAME                "SerialComp"
    PLUGIN_IS_SYNTH             FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             "Fx|Dynamics"
    AU_MAIN_TYPE                "kAudioUnitType_Effect"
)

juce_generate_juce_header(SerialComp)

target_sources(SerialComp
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/CompressorStage.cpp
        Source/StageComponent.cpp
        Source/CustomLookAndFeel.cpp
)

target_compile_definitions(SerialComp
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(SerialComp
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
