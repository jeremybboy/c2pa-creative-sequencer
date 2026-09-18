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

option(C2PASEQ_ENABLE_REAL_AUDIOWMARK_TEST
       "Enable opt-in tests that require scripts/setup_audiowmark.sh" OFF)
option(C2PASEQ_ENABLE_REAL_AUDFPRINT_TEST
       "Enable opt-in tests that require scripts/setup_audfprint.sh and ffmpeg" OFF)

set(C2PASEQ_WATERMARK_SOURCES
    src/fingerprint/AudfprintService.cpp
    src/fingerprint/AudfprintService.h
    src/fingerprint/FingerprintService.h
    src/watermark/AudioWMarkService.cpp
    src/watermark/AudioWMarkService.h
    src/watermark/SoftBindingOutbox.cpp
    src/watermark/SoftBindingOutbox.h
    src/watermark/SoftBindingPayload.cpp
    src/watermark/SoftBindingPayload.h
    src/watermark/WatermarkService.h)

juce_add_gui_app(C2PACreativeSequencer
    PRODUCT_NAME "C2PA Creative Sequencer"
    BUNDLE_ID "app.c2paseq.CreativeSequencer"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/app-icon-source.png"
    MICROPHONE_PERMISSION_ENABLED FALSE)

juce_generate_juce_header(C2PACreativeSequencer)

target_sources(C2PACreativeSequencer PRIVATE
    ${C2PASEQ_WATERMARK_SOURCES}
    src/app/AppInfo.h
    src/app/Application.cpp
    src/app/Application.h
    src/app/Commands.cpp
    src/app/Commands.h
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/engine/AudioEngine.cpp
    src/engine/AudioEngine.h
    src/engine/ClipOcclusion.cpp
    src/engine/ClipOcclusion.h
    src/engine/NativeAudioClipPolicy.cpp
    src/engine/NativeAudioClipPolicy.h
    src/engine/ProjectEngine.cpp
    src/engine/ProjectEngine.h
    src/engine/TracktionAdapter.cpp
    src/engine/TracktionAdapter.h
    src/export/ExportController.cpp
    src/export/ExportController.h
    src/export/ExportResult.h
    src/export/RenderService.cpp
    src/export/RenderService.h
    src/plugins/PluginDescriptor.cpp
    src/plugins/PluginDescriptor.h
    src/plugins/PluginHost.cpp
    src/plugins/PluginHost.h
    src/plugins/PluginScanner.cpp
    src/plugins/PluginScanner.h
    src/plugins/PluginWindow.cpp
    src/plugins/PluginWindow.h
    src/provenance/ProvenanceModel.cpp
    src/provenance/ProvenanceModel.h
    src/provenance/ProvenanceService.cpp
    src/provenance/ProvenanceService.h
    src/provenance/SigningProvider.cpp
    src/provenance/SigningProvider.h
    src/project/MediaLibrary.cpp
    src/project/MediaLibrary.h
    src/project/Project.cpp
    src/project/Project.h
    src/project/ProjectPaths.cpp
    src/project/ProjectPaths.h
    src/project/ProjectSerializer.cpp
    src/project/ProjectSerializer.h
    src/timeline/TimelineGeometry.cpp
    src/timeline/TimelineGeometry.h
    src/transport/TransportFormatting.cpp
    src/transport/TransportFormatting.h
    src/transport/ArrangementLoop.cpp
    src/transport/ArrangementLoop.h
    src/ui/ArrangementView.cpp
    src/ui/ArrangementView.h
    src/ui/ArrangementShortcuts.cpp
    src/ui/ArrangementShortcuts.h
    src/ui/PlacesBrowser.cpp
    src/ui/PlacesBrowser.h
    src/ui/PlacesStore.cpp
    src/ui/PlacesStore.h
    src/ui/TrackHeaderView.cpp
    src/ui/TrackHeaderView.h
    src/ui/WaveformView.cpp
    src/ui/WaveformView.h)

target_include_directories(C2PACreativeSequencer PRIVATE src)

target_compile_definitions(C2PACreativeSequencer PRIVATE
    JUCE_PLUGINHOST_AU=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    TRACKTION_ENABLE_ARA=0
    TRACKTION_ENABLE_CMAJOR=0
    TRACKTION_ENABLE_CONTROL_SURFACES=0
    TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=0)

target_link_libraries(C2PACreativeSequencer PRIVATE
    tracktion::tracktion_engine
    juce::juce_audio_utils
    juce::juce_cryptography
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

c2paseq_enable_c2pa(C2PACreativeSequencer)

set_target_properties(C2PACreativeSequencer PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES)

if(BUILD_TESTING)
    juce_add_plugin(C2PATestGainVST3
        COMPANY_NAME "C2PA Test"
        BUNDLE_ID "app.c2paseq.TestGain"
        PLUGIN_MANUFACTURER_CODE C2PA
        PLUGIN_CODE TGa1
        FORMATS VST3
        PRODUCT_NAME "C2PA Test Gain"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE)
    juce_generate_juce_header(C2PATestGainVST3)
    target_sources(C2PATestGainVST3 PRIVATE tests/plugins/TestGainPlugin.cpp)
    target_link_libraries(C2PATestGainVST3 PRIVATE
        juce::juce_audio_utils
        juce::juce_recommended_config_flags)

    add_executable(C2PACreativeSequencerTests
        tests/app/ApplicationSkeletonTests.cpp
        src/app/AppInfo.h)
    target_include_directories(C2PACreativeSequencerTests PRIVATE src)
    target_compile_features(C2PACreativeSequencerTests PRIVATE cxx_std_20)
    add_test(NAME application_skeleton COMMAND C2PACreativeSequencerTests)

    add_executable(C2PATransportTests
        tests/engine/TransportFoundationTests.cpp
        src/engine/NativeAudioClipPolicy.cpp
        src/engine/NativeAudioClipPolicy.h
        src/transport/TransportFormatting.cpp
        src/transport/TransportFormatting.h)
    target_include_directories(C2PATransportTests PRIVATE src)
    target_compile_definitions(C2PATransportTests PRIVATE
        JUCE_PLUGINHOST_AU=0
        JUCE_PLUGINHOST_VST3=0
        JUCE_USE_MP3AUDIOFORMAT=1
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

    add_executable(C2PAProjectModelTests
        tests/project/ProjectModelTests.cpp
        src/app/AppInfo.h
        src/project/MediaLibrary.cpp
        src/project/MediaLibrary.h
        src/project/Project.cpp
        src/project/Project.h
        src/project/ProjectPaths.cpp
        src/project/ProjectPaths.h
        src/project/ProjectSerializer.cpp
        src/project/ProjectSerializer.h
        src/provenance/ProvenanceModel.cpp
        src/provenance/ProvenanceModel.h)
    target_include_directories(C2PAProjectModelTests PRIVATE src)
    target_compile_features(C2PAProjectModelTests PRIVATE cxx_std_20)
    target_link_libraries(C2PAProjectModelTests PRIVATE
        juce::juce_cryptography
        juce::juce_recommended_config_flags)
    add_test(NAME project_model COMMAND C2PAProjectModelTests)

    add_executable(C2PATimelineTests
        tests/timeline/TimelineGeometryTests.cpp
        src/timeline/TimelineGeometry.cpp
        src/timeline/TimelineGeometry.h
        src/ui/PlacesStore.cpp
        src/ui/PlacesStore.h)
    target_include_directories(C2PATimelineTests PRIVATE src)
    target_compile_features(C2PATimelineTests PRIVATE cxx_std_20)
    target_link_libraries(C2PATimelineTests PRIVATE
        juce::juce_gui_basics
        juce::juce_recommended_config_flags)
    add_test(NAME timeline_geometry COMMAND C2PATimelineTests)

    add_executable(C2PAArrangementRulesTests
        tests/engine/ArrangementRulesTests.cpp
        src/engine/ClipOcclusion.cpp
        src/engine/ClipOcclusion.h
        src/project/Project.h
        src/transport/ArrangementLoop.cpp
        src/transport/ArrangementLoop.h
        src/ui/ArrangementShortcuts.cpp
        src/ui/ArrangementShortcuts.h)
    target_include_directories(C2PAArrangementRulesTests PRIVATE src)
    target_compile_features(C2PAArrangementRulesTests PRIVATE cxx_std_20)
    target_link_libraries(C2PAArrangementRulesTests PRIVATE
        juce::juce_gui_basics
        juce::juce_recommended_config_flags)
    add_test(NAME arrangement_rules COMMAND C2PAArrangementRulesTests)

    add_executable(C2PASoftBindingTests
        tests/watermark/SoftBindingTests.cpp
        src/watermark/SoftBindingOutbox.cpp
        src/watermark/SoftBindingOutbox.h
        src/watermark/SoftBindingPayload.cpp
        src/watermark/SoftBindingPayload.h)
    target_include_directories(C2PASoftBindingTests PRIVATE src)
    target_compile_features(C2PASoftBindingTests PRIVATE cxx_std_20)
    target_link_libraries(C2PASoftBindingTests PRIVATE
        juce::juce_cryptography
        juce::juce_recommended_config_flags)
    add_test(NAME soft_binding COMMAND C2PASoftBindingTests)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME soft_binding_resolver
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/softbinding-resolver/tests/test_resolver.py")
        add_test(NAME soft_binding_demo_derivative
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/watermark/test_demo_derivative.py")
        add_test(NAME audfprint_wrapper
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/fingerprint/test_audfprint_wrapper.py")
    endif()

    add_executable(C2PAArrangementEditingTests
        ${C2PASEQ_WATERMARK_SOURCES}
        tests/engine/ArrangementEditingTests.cpp
        src/app/AppInfo.h
        src/engine/AudioEngine.cpp
        src/engine/AudioEngine.h
        src/engine/ClipOcclusion.cpp
        src/engine/ClipOcclusion.h
        src/engine/NativeAudioClipPolicy.cpp
        src/engine/NativeAudioClipPolicy.h
        src/engine/ProjectEngine.cpp
        src/engine/ProjectEngine.h
        src/engine/TracktionAdapter.cpp
        src/engine/TracktionAdapter.h
        src/export/ExportController.cpp
        src/export/ExportController.h
        src/export/ExportResult.h
        src/export/RenderService.cpp
        src/export/RenderService.h
        src/plugins/PluginDescriptor.cpp
        src/plugins/PluginDescriptor.h
        src/plugins/PluginHost.cpp
        src/plugins/PluginHost.h
        src/plugins/PluginScanner.cpp
        src/plugins/PluginScanner.h
        src/plugins/PluginWindow.cpp
        src/plugins/PluginWindow.h
        src/provenance/ProvenanceModel.cpp
        src/provenance/ProvenanceModel.h
        src/provenance/ProvenanceService.cpp
        src/provenance/ProvenanceService.h
        src/provenance/SigningProvider.cpp
        src/provenance/SigningProvider.h
        src/project/MediaLibrary.cpp
        src/project/MediaLibrary.h
        src/project/Project.cpp
        src/project/Project.h
        src/project/ProjectPaths.cpp
        src/project/ProjectPaths.h
        src/project/ProjectSerializer.cpp
        src/project/ProjectSerializer.h
        src/transport/ArrangementLoop.cpp
        src/transport/ArrangementLoop.h
        src/transport/TransportFormatting.cpp
        src/transport/TransportFormatting.h)
    target_include_directories(C2PAArrangementEditingTests PRIVATE src)
    target_compile_definitions(C2PAArrangementEditingTests PRIVATE
        JUCE_PLUGINHOST_AU=0
        JUCE_PLUGINHOST_VST3=1
        JUCE_USE_MP3AUDIOFORMAT=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        TRACKTION_ENABLE_ARA=0
        TRACKTION_ENABLE_CMAJOR=0
        TRACKTION_ENABLE_CONTROL_SURFACES=0
        TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=0)
    target_link_libraries(C2PAArrangementEditingTests PRIVATE
        tracktion::tracktion_engine
        juce::juce_audio_utils
        juce::juce_cryptography
        juce::juce_gui_extra
        juce::juce_recommended_config_flags)
    c2paseq_enable_c2pa(C2PAArrangementEditingTests)
    add_test(NAME arrangement_editing COMMAND C2PAArrangementEditingTests)

    add_executable(C2PAProvenanceTests
        tests/provenance/ProvenanceServiceTests.cpp
        src/app/AppInfo.h
        src/provenance/ProvenanceModel.cpp
        src/provenance/ProvenanceModel.h
        src/provenance/ProvenanceService.cpp
        src/provenance/ProvenanceService.h
        src/provenance/SigningProvider.cpp
        src/provenance/SigningProvider.h
        src/watermark/SoftBindingPayload.cpp
        src/watermark/SoftBindingPayload.h)
    target_include_directories(C2PAProvenanceTests PRIVATE src)
    target_compile_features(C2PAProvenanceTests PRIVATE cxx_std_20)
    target_compile_definitions(C2PAProvenanceTests PRIVATE
        C2PA_KNOWN_SIGNED_WAV="${c2pa_cpp_source_SOURCE_DIR}/tests/fixtures/sample1_signed.wav"
        C2PA_TEST_CERTIFICATE_PEM="${c2pa_cpp_source_SOURCE_DIR}/tests/fixtures/es256_certs.pem"
        C2PA_TEST_PRIVATE_KEY_PEM="${c2pa_cpp_source_SOURCE_DIR}/tests/fixtures/es256_private.key")
    target_link_libraries(C2PAProvenanceTests PRIVATE
        juce::juce_audio_formats
        juce::juce_cryptography
        juce::juce_recommended_config_flags)
    c2paseq_enable_c2pa(C2PAProvenanceTests)
    add_test(NAME provenance_service COMMAND C2PAProvenanceTests)

    add_executable(C2PAExportPipelineTests
        ${C2PASEQ_WATERMARK_SOURCES}
        tests/export/ExportPipelineTests.cpp
        src/app/AppInfo.h
        src/engine/AudioEngine.cpp
        src/engine/AudioEngine.h
        src/engine/ClipOcclusion.cpp
        src/engine/ClipOcclusion.h
        src/engine/NativeAudioClipPolicy.cpp
        src/engine/NativeAudioClipPolicy.h
        src/engine/ProjectEngine.cpp
        src/engine/ProjectEngine.h
        src/engine/TracktionAdapter.cpp
        src/engine/TracktionAdapter.h
        src/export/ExportController.cpp
        src/export/ExportController.h
        src/export/ExportResult.h
        src/export/RenderService.cpp
        src/export/RenderService.h
        src/plugins/PluginDescriptor.cpp
        src/plugins/PluginDescriptor.h
        src/plugins/PluginHost.cpp
        src/plugins/PluginHost.h
        src/plugins/PluginScanner.cpp
        src/plugins/PluginScanner.h
        src/plugins/PluginWindow.cpp
        src/plugins/PluginWindow.h
        src/project/MediaLibrary.cpp
        src/project/MediaLibrary.h
        src/project/Project.cpp
        src/project/Project.h
        src/project/ProjectPaths.cpp
        src/project/ProjectPaths.h
        src/project/ProjectSerializer.cpp
        src/project/ProjectSerializer.h
        src/provenance/ProvenanceModel.cpp
        src/provenance/ProvenanceModel.h
        src/provenance/ProvenanceService.cpp
        src/provenance/ProvenanceService.h
        src/provenance/SigningProvider.cpp
        src/provenance/SigningProvider.h
        src/transport/ArrangementLoop.cpp
        src/transport/ArrangementLoop.h
        src/transport/TransportFormatting.cpp
        src/transport/TransportFormatting.h)
    target_include_directories(C2PAExportPipelineTests PRIVATE src)
    target_compile_definitions(C2PAExportPipelineTests PRIVATE
        JUCE_PLUGINHOST_AU=0
        JUCE_PLUGINHOST_VST3=1
        JUCE_USE_MP3AUDIOFORMAT=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        TRACKTION_ENABLE_ARA=0
        TRACKTION_ENABLE_CMAJOR=0
        TRACKTION_ENABLE_CONTROL_SURFACES=0
        TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=0
        C2PA_KNOWN_SIGNED_WAV="${c2pa_cpp_source_SOURCE_DIR}/tests/fixtures/sample1_signed.wav"
        C2PA_TEST_CERTIFICATE_PEM="${c2pa_cpp_source_SOURCE_DIR}/tests/fixtures/es256_certs.pem"
        C2PA_TEST_PRIVATE_KEY_PEM="${c2pa_cpp_source_SOURCE_DIR}/tests/fixtures/es256_private.key"
        C2PASEQ_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        JUCE_MODAL_LOOPS_PERMITTED=1
        C2PASEQ_TEST_VST3_BUNDLE="$<TARGET_BUNDLE_DIR:C2PATestGainVST3_VST3>")
    target_link_libraries(C2PAExportPipelineTests PRIVATE
        tracktion::tracktion_engine
        juce::juce_audio_utils
        juce::juce_cryptography
        juce::juce_gui_extra
        juce::juce_recommended_config_flags)
    add_dependencies(C2PAExportPipelineTests C2PATestGainVST3_VST3)
    c2paseq_enable_c2pa(C2PAExportPipelineTests)
    add_test(NAME export_pipeline COMMAND C2PAExportPipelineTests)
    if(C2PASEQ_ENABLE_REAL_AUDIOWMARK_TEST)
        add_test(NAME real_audiowmark_pipeline COMMAND C2PAExportPipelineTests)
        set_tests_properties(real_audiowmark_pipeline PROPERTIES
            ENVIRONMENT "C2PASEQ_RUN_REAL_AUDIOWMARK_TEST=1"
            LABELS "audiowmark;integration")
        add_test(NAME audiowmark_benchmark COMMAND C2PAExportPipelineTests)
        set_tests_properties(audiowmark_benchmark PROPERTIES
            ENVIRONMENT "C2PASEQ_RUN_AUDIOWMARK_BENCHMARK=1"
            LABELS "audiowmark;benchmark"
            TIMEOUT 600)
    endif()
    if(C2PASEQ_ENABLE_REAL_AUDFPRINT_TEST)
        add_test(NAME real_mp3_soft_binding_pipeline COMMAND C2PAExportPipelineTests)
        set_tests_properties(real_mp3_soft_binding_pipeline PROPERTIES
            ENVIRONMENT "C2PASEQ_RUN_REAL_AUDIOWMARK_TEST=1;C2PASEQ_RUN_REAL_AUDFPRINT_TEST=1"
            LABELS "audiowmark;audfprint;ffmpeg;integration"
            TIMEOUT 300)
    endif()

    add_executable(C2PAPluginHostingTests
        ${C2PASEQ_WATERMARK_SOURCES}
        tests/plugins/PluginHostingTests.cpp
        src/app/AppInfo.h
        src/engine/AudioEngine.cpp
        src/engine/AudioEngine.h
        src/engine/ClipOcclusion.cpp
        src/engine/ClipOcclusion.h
        src/engine/NativeAudioClipPolicy.cpp
        src/engine/NativeAudioClipPolicy.h
        src/engine/ProjectEngine.cpp
        src/engine/ProjectEngine.h
        src/engine/TracktionAdapter.cpp
        src/engine/TracktionAdapter.h
        src/export/ExportController.cpp
        src/export/ExportController.h
        src/export/ExportResult.h
        src/export/RenderService.cpp
        src/export/RenderService.h
        src/plugins/PluginDescriptor.cpp
        src/plugins/PluginDescriptor.h
        src/plugins/PluginHost.cpp
        src/plugins/PluginHost.h
        src/plugins/PluginScanner.cpp
        src/plugins/PluginScanner.h
        src/plugins/PluginWindow.cpp
        src/plugins/PluginWindow.h
        src/project/MediaLibrary.cpp
        src/project/MediaLibrary.h
        src/project/Project.cpp
        src/project/Project.h
        src/project/ProjectPaths.cpp
        src/project/ProjectPaths.h
        src/project/ProjectSerializer.cpp
        src/project/ProjectSerializer.h
        src/provenance/ProvenanceModel.cpp
        src/provenance/ProvenanceModel.h
        src/provenance/ProvenanceService.cpp
        src/provenance/ProvenanceService.h
        src/provenance/SigningProvider.cpp
        src/provenance/SigningProvider.h
        src/transport/ArrangementLoop.cpp
        src/transport/ArrangementLoop.h
        src/transport/TransportFormatting.cpp
        src/transport/TransportFormatting.h)
    target_include_directories(C2PAPluginHostingTests PRIVATE src)
    target_compile_definitions(C2PAPluginHostingTests PRIVATE
        JUCE_PLUGINHOST_AU=0
        JUCE_PLUGINHOST_VST3=1
        JUCE_USE_MP3AUDIOFORMAT=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        TRACKTION_ENABLE_ARA=0
        TRACKTION_ENABLE_CMAJOR=0
        TRACKTION_ENABLE_CONTROL_SURFACES=0
        TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=0
        C2PASEQ_TEST_VST3_BUNDLE="$<TARGET_BUNDLE_DIR:C2PATestGainVST3_VST3>")
    target_link_libraries(C2PAPluginHostingTests PRIVATE
        tracktion::tracktion_engine
        juce::juce_audio_utils
        juce::juce_cryptography
        juce::juce_gui_extra
        juce::juce_recommended_config_flags)
    add_dependencies(C2PAPluginHostingTests C2PATestGainVST3_VST3)
    c2paseq_enable_c2pa(C2PAPluginHostingTests)
    add_test(NAME plugin_hosting COMMAND C2PAPluginHostingTests)
endif()
