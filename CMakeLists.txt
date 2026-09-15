cmake_minimum_required(VERSION 3.25)

file(READ "${CMAKE_SOURCE_DIR}/VERSION" PUZZLEQ_VERSION)
string(STRIP "${PUZZLEQ_VERSION}" PUZZLEQ_VERSION)

project(PuzzlEQ VERSION ${PUZZLEQ_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
include(FetchContent)

if(EXISTS "${CMAKE_SOURCE_DIR}/modules/JUCE/CMakeLists.txt")
    add_subdirectory(modules/JUCE EXCLUDE_FROM_ALL)
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.10
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(PUZZLEQ_HAS_CLAP OFF)
if(EXISTS "${CMAKE_SOURCE_DIR}/modules/clap-juce-extensions/CMakeLists.txt")
    add_subdirectory(modules/clap-juce-extensions EXCLUDE_FROM_ALL)
    set(PUZZLEQ_HAS_CLAP ON)
endif()

# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------
set(PUZZLEQ_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND PUZZLEQ_FORMATS AU)
endif()

juce_add_plugin(PuzzlEQ
    COMPANY_NAME "Puzzl"
    COMPANY_WEBSITE "https://puzzl.dev"
    BUNDLE_ID com.puzzl.puzzleq
    PLUGIN_MANUFACTURER_CODE Pzzl
    PLUGIN_CODE PzEQ
    FORMATS ${PUZZLEQ_FORMATS}
    PRODUCT_NAME "PuzzlEQ"
    VERSION ${PUZZLEQ_VERSION}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES EQ
    AU_MAIN_TYPE kAudioUnitType_Effect
    MICROPHONE_PERMISSION_ENABLED FALSE)

if(PUZZLEQ_HAS_CLAP)
    clap_juce_extensions_plugin(
        TARGET PuzzlEQ
        CLAP_ID "com.puzzl.puzzleq"
        CLAP_FEATURES audio-effect equalizer)
endif()

juce_generate_juce_header(PuzzlEQ)

set(PUZZLEQ_SOURCES
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/state/ParameterLayout.cpp
    src/state/Presets.cpp
    src/dsp/FilterDesign.cpp
    src/dsp/Fft.cpp
    src/dsp/SpectrumAnalyzer.cpp
    src/dsp/DynamicEngine.cpp
    src/dsp/LinearPhaseEq.cpp
    src/dsp/SpectralDynamics.cpp
    src/dsp/EqMatch.cpp
    src/dsp/EqEngine.cpp
    src/ui/PuzzlLookAndFeel.cpp
    src/ui/EqDisplay.cpp
    src/ui/BandInspector.cpp
    src/ui/BottomBar.cpp)

target_sources(PuzzlEQ PRIVATE ${PUZZLEQ_SOURCES})

target_include_directories(PuzzlEQ PRIVATE src)

target_compile_definitions(PuzzlEQ
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=1
        JUCE_REPORT_APP_USAGE=0
        JUCE_MODAL_LOOPS_PERMITTED=0
        PUZZLEQ_VERSION="${PUZZLEQ_VERSION}")

target_link_libraries(PuzzlEQ
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

if(MSVC)
    target_compile_options(PuzzlEQ PRIVATE /W4 /permissive-)
else()
    target_compile_options(PuzzlEQ PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# Tests (JUCE-free DSP + a processor smoke test)
# ---------------------------------------------------------------------------
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(Catch2)

add_executable(PuzzlEQTests
    tests/FilterDesignTests.cpp
    tests/EngineTests.cpp
    src/dsp/FilterDesign.cpp
    src/dsp/Fft.cpp
    src/dsp/DynamicEngine.cpp
    src/dsp/EqMatch.cpp
    src/dsp/LinearPhaseEq.cpp)

target_include_directories(PuzzlEQTests PRIVATE src)
target_link_libraries(PuzzlEQTests PRIVATE Catch2::Catch2WithMain)
target_compile_features(PuzzlEQTests PRIVATE cxx_std_20)

enable_testing()
add_test(NAME PuzzlEQTests COMMAND PuzzlEQTests)
