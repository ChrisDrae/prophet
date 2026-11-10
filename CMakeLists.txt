# =================================================================================================
#  Seer — top-level build script
#
#  Quick start (Windows, from this folder):
#      cmake --preset vs2026            # configure: generate build/vs2026/Seer.slnx
#      cmake --build --preset release   # compile the Standalone app, the VST3 and the tests
#      ctest --preset release           # run the unit tests
# =================================================================================================
cmake_minimum_required(VERSION 3.25)

project(Seer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(SEER_BUILD_TESTS "Build the Catch2 unit tests" ON)
option(SEER_BUILD_TOOLS "Build the SeerRender dev tool" ON)

# -------------------------------------------------------------------------------------------------
#  JUCE: prefer the checkout next to this repo (../JUCE); otherwise download the pinned version.
# -------------------------------------------------------------------------------------------------
set(SEER_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE" CACHE PATH "Path to a local JUCE checkout")

if(EXISTS "${SEER_JUCE_DIR}/CMakeLists.txt")
    message(STATUS "Seer: using local JUCE at ${SEER_JUCE_DIR}")
    add_subdirectory("${SEER_JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    message(STATUS "Seer: local JUCE not found, fetching JUCE 8.0.12 from GitHub")
    include(FetchContent)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.12
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(juce)
endif()

# -------------------------------------------------------------------------------------------------
#  SeerEngine: oscillators, filters, envelopes, voices, as an INTERFACE library.
#  The plugin and the unit tests both compile these sources, so the tested engine is the shipped one.
# -------------------------------------------------------------------------------------------------
add_library(SeerEngine INTERFACE)

target_sources(SeerEngine INTERFACE
    source/engine/SynthEngine.cpp
    source/engine/Voice.cpp
    source/engine/effects/Effects.cpp
    source/engine/effects/Reverb.cpp
)

target_include_directories(SeerEngine INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/source")

target_link_libraries(SeerEngine INTERFACE
    juce::juce_audio_basics)

target_compile_definitions(SeerEngine INTERFACE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

# -------------------------------------------------------------------------------------------------
#  The plugin: one shared-code target (Seer) plus a thin wrapper per format (Seer_Standalone, ...).
# -------------------------------------------------------------------------------------------------
set(SEER_FORMATS Standalone VST3)
if(APPLE)
    list(APPEND SEER_FORMATS AU)
endif()

juce_add_plugin(Seer
    COMPANY_NAME "ChrisDrae"
    PRODUCT_NAME "Seer"
    VERSION "${PROJECT_VERSION}"
    PLUGIN_MANUFACTURER_CODE Cdra
    PLUGIN_CODE Seer
    FORMATS ${SEER_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE)

add_library(SeerPluginSources INTERFACE)
target_sources(SeerPluginSources INTERFACE
    source/Parameters.cpp
    source/PluginEditor.cpp
    source/PluginProcessor.cpp
)

target_compile_definitions(Seer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Seer
    PRIVATE
        SeerPluginSources
        SeerEngine
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# -------------------------------------------------------------------------------------------------
#  Unit tests (Catch2 v3). They link SeerEngine, so they run without a host or a window.
#  Reuses Pocket Looper's downloaded Catch2 when it's there, to skip the download.
# -------------------------------------------------------------------------------------------------
if(SEER_BUILD_TESTS)
    include(FetchContent)
    set(_seer_local_catch2 "${CMAKE_CURRENT_SOURCE_DIR}/../plugin/build/vs2026/_deps/catch2-src")
    if(EXISTS "${_seer_local_catch2}/CMakeLists.txt" AND NOT DEFINED FETCHCONTENT_SOURCE_DIR_CATCH2)
        set(FETCHCONTENT_SOURCE_DIR_CATCH2 "${_seer_local_catch2}")
    endif()
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.16.0
        GIT_SHALLOW ON
        SYSTEM)
    FetchContent_MakeAvailable(Catch2)

    enable_testing()

    add_executable(SeerTests
        tests/EngineTests.cpp
        tests/EnvelopeTests.cpp
        tests/FilterTests.cpp
        tests/OscillatorTests.cpp
    )

    target_link_libraries(SeerTests PRIVATE
        SeerEngine
        Catch2::Catch2WithMain
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    include(Catch)
    catch_discover_tests(SeerTests)
endif()

# -------------------------------------------------------------------------------------------------
#  Render tool: plays test phrases through the engine into WAV files, and prints their levels.
#      build/vs2026/SeerRender_artefacts/Release/SeerRender.exe build/renders
# -------------------------------------------------------------------------------------------------
if(SEER_BUILD_TOOLS)
    juce_add_console_app(SeerRender PRODUCT_NAME "SeerRender")
    target_sources(SeerRender PRIVATE tests/SeerRender.cpp)
    target_compile_definitions(SeerRender PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(SeerRender PRIVATE
        SeerEngine
        juce::juce_audio_formats
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif()
