cmake_minimum_required(VERSION 3.27)

project(C2PACreativeSequencer
    VERSION 0.1.0
    DESCRIPTION "A minimal arrangement-based audio sequencer with native C2PA provenance"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3" CACHE STRING "Minimum macOS version")

include(CTest)
include(cmake/Dependencies.cmake)

juce_add_gui_app(C2PACreativeSequencer
    PRODUCT_NAME "C2PA Creative Sequencer"
    BUNDLE_ID "app.c2paseq.CreativeSequencer"
    MICROPHONE_PERMISSION_ENABLED FALSE)

juce_generate_juce_header(C2PACreativeSequencer)

target_sources(C2PACreativeSequencer PRIVATE
    src/app/AppInfo.h
    src/app/Application.cpp
    src/app/Application.h
    src/app/Commands.cpp
    src/app/Commands.h
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/engine/AudioEngine.cpp
    src/engine/AudioEngine.h
    src/engine/TracktionAdapter.cpp
    src/engine/TracktionAdapter.h
    src/transport/TransportFormatting.cpp
    src/transport/TransportFormatting.h
    src/ui/ArrangementView.cpp
    src/ui/ArrangementView.h)

target_include_directories(C2PACreativeSequencer PRIVATE src)

target_compile_definitions(C2PACreativeSequencer PRIVATE
    JUCE_PLUGINHOST_AU=0
    JUCE_PLUGINHOST_VST3=0
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    TRACKTION_ENABLE_ARA=0
    TRACKTION_ENABLE_CMAJOR=0
    TRACKTION_ENABLE_CONTROL_SURFACES=0
    TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=0)

target_link_libraries(C2PACreativeSequencer PRIVATE
    tracktion::tracktion_engine
    juce::juce_audio_utils
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

set_target_properties(C2PACreativeSequencer PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES)

if(BUILD_TESTING)
    add_executable(C2PACreativeSequencerTests
        tests/app/ApplicationSkeletonTests.cpp
        src/app/AppInfo.h)
    target_include_directories(C2PACreativeSequencerTests PRIVATE src)
    target_compile_features(C2PACreativeSequencerTests PRIVATE cxx_std_20)
    add_test(NAME application_skeleton COMMAND C2PACreativeSequencerTests)

    add_executable(C2PATransportTests
        tests/engine/TransportFoundationTests.cpp
        src/transport/TransportFormatting.cpp
        src/transport/TransportFormatting.h)
    target_include_directories(C2PATransportTests PRIVATE src)
    target_compile_definitions(C2PATransportTests PRIVATE
        JUCE_PLUGINHOST_AU=0
        JUCE_PLUGINHOST_VST3=0
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        TRACKTION_ENABLE_ARA=0
        TRACKTION_ENABLE_CMAJOR=0
        TRACKTION_ENABLE_CONTROL_SURFACES=0
        TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=0)
    target_link_libraries(C2PATransportTests PRIVATE
        tracktion::tracktion_engine
        juce::juce_recommended_config_flags)
    add_test(NAME transport_foundation COMMAND C2PATransportTests)
endif()
