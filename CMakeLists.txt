cmake_minimum_required(VERSION 3.22)
project(Pulsar VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows : on embarque le runtime C++ dans le plugin, pour qu'il marche sur n'importe quel PC
# sans installer les "Visual C++ Redistributable".
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

# JUCE : téléchargé automatiquement, sauf si on indique un dossier local avec -DPULSAR_JUCE_PATH=...
set(PULSAR_JUCE_PATH "" CACHE PATH "Dossier JUCE local (vide = téléchargement automatique)")

if(PULSAR_JUCE_PATH)
    add_subdirectory("${PULSAR_JUCE_PATH}" juce EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
endif()

juce_add_plugin(Pulsar
    COMPANY_NAME "Trou Noir Audio"
    BUNDLE_ID "audio.trounoir.pulsar"
    PLUGIN_MANUFACTURER_CODE Tnra
    PLUGIN_CODE Pls1
    FORMATS VST3
    PRODUCT_NAME "Pulsar"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Modulation
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Pulsar PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(Pulsar PRIVATE Source)

target_compile_definitions(Pulsar PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(MSVC)
    # Les fichiers source sont en UTF-8 (textes accentués de l'interface).
    target_compile_options(Pulsar PUBLIC /utf-8)
endif()

target_link_libraries(Pulsar
    PRIVATE
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# Outils de développement (facultatifs) : banc de test du moteur et capture de l'interface.
option(PULSAR_BUILD_TOOLS "Compiler les outils de test" OFF)

if(PULSAR_BUILD_TOOLS)
    add_executable(PulsarEngineTest tests/engine_test.cpp)
    target_include_directories(PulsarEngineTest PRIVATE Source)

    add_executable(PulsarSnapshot tests/snapshot.cpp)
    target_include_directories(PulsarSnapshot PRIVATE Source $<TARGET_PROPERTY:Pulsar,INCLUDE_DIRECTORIES>)
    target_compile_definitions(PulsarSnapshot PRIVATE $<TARGET_PROPERTY:Pulsar,COMPILE_DEFINITIONS>)
    target_link_libraries(PulsarSnapshot PRIVATE Pulsar)
endif()
