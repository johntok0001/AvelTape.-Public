cmake_minimum_required(VERSION 3.22)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")
endif()

project(AvelTape VERSION 1.1.0 LANGUAGES C CXX)

# Windows: link the C++ runtime statically so no VC++ redistributable is needed
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use a local JUCE checkout with -DJUCE_DIR=/path/to/JUCE, otherwise it is downloaded.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

set(AVEL_FORMATS VST3)
if(APPLE)
    list(APPEND AVEL_FORMATS AU)
endif()

juce_add_plugin(AvelTape
    COMPANY_NAME "Avel"
    BUNDLE_ID "com.avel.tape"
    PLUGIN_MANUFACTURER_CODE Avel
    PLUGIN_CODE AvTp
    FORMATS ${AVEL_FORMATS}
    PRODUCT_NAME "Avel Tape"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Delay
    AU_MAIN_TYPE kAudioUnitType_Effect)

juce_add_binary_data(AvelTapeFonts SOURCES
    Resources/Fonts/Inter-Regular.otf
    Resources/Fonts/Inter-Medium.otf
    Resources/Fonts/Inter-SemiBold.otf
    Resources/Fonts/Montserrat-Medium.otf
    Resources/Fonts/Montserrat-SemiBold.otf)

set(AVEL_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

set(AVEL_DEFINES
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    AVEL_VERSION="${PROJECT_VERSION}")

if(UNIX AND NOT APPLE)
    # Linux is only used for testing; these X11 extras are optional at runtime.
    list(APPEND AVEL_DEFINES JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0)
endif()

set(AVEL_MODULES
    juce::juce_audio_processors
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_gui_basics)

target_sources(AvelTape PRIVATE ${AVEL_SOURCES})
target_include_directories(AvelTape PRIVATE Source)
target_compile_definitions(AvelTape PUBLIC ${AVEL_DEFINES})
target_link_libraries(AvelTape
    PRIVATE ${AVEL_MODULES} AvelTapeFonts
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Offline test harness (not part of the release build)
option(AVEL_BUILD_TESTS "Build the offline test harness" OFF)
if(AVEL_BUILD_TESTS)
    juce_add_console_app(AvelTapeTest PRODUCT_NAME "AvelTapeTest")
    target_sources(AvelTapeTest PRIVATE Tests/AvelTapeTest.cpp ${AVEL_SOURCES})
    target_include_directories(AvelTapeTest PRIVATE Source)
    target_compile_definitions(AvelTapeTest PRIVATE ${AVEL_DEFINES} JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1
        JucePlugin_Name="Avel Tape" JucePlugin_VersionString="${PROJECT_VERSION}")
    target_link_libraries(AvelTapeTest PRIVATE ${AVEL_MODULES} AvelTapeFonts juce::juce_recommended_config_flags)

    juce_add_console_app(HostTest PRODUCT_NAME "HostTest")
    target_sources(HostTest PRIVATE Tests/HostTest.cpp)
    target_compile_definitions(HostTest PRIVATE ${AVEL_DEFINES} JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(HostTest PRIVATE juce::juce_audio_processors juce::juce_recommended_config_flags)
endif()
