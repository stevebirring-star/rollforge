# RollForge unit tests.
#
# Phase 0 ships a smoke test that proves the test harness + CI wiring work.
# Real model tests (sequencer timing, roll compiler, fill generator,
# categoriser, choke logic, JSON round-trip) arrive from Phase 1 onward and
# link only pure model/engine code — never the GUI.

juce_add_console_app(RollForgeTests
    PRODUCT_NAME "RollForgeTests"
)

target_sources(RollForgeTests PRIVATE
    TestMain.cpp
    SampleBufferTests.cpp
    PadKitTests.cpp
    CommandQueueTests.cpp
    DrumEngineTests.cpp
    VoiceTests.cpp
    VoicePoolTests.cpp
    ChokeGroupTests.cpp
    SampleLoaderTests.cpp
    StarterKitTests.cpp
    KitInstallerTests.cpp
    LayerTests.cpp
    PadMappingTests.cpp
    PatternModelTests.cpp
    ClockTimingTests.cpp
    SequencerTests.cpp
    TripletTests.cpp
    PatternSwitchTests.cpp
    RollCompilerTests.cpp
    RollSequencingTests.cpp
    RollPresetsTests.cpp
    FillEngineTests.cpp
    VariatorTests.cpp
    FeelPresetsTests.cpp
    HumaniserTests.cpp
    FxSmokeTests.cpp
    OutputMeterTests.cpp
    CategoriserTests.cpp
    SlicerTests.cpp
    SimilarityTests.cpp
    ResampleTests.cpp
    SongTests.cpp
    TextTests.cpp
    SimilarSearchTests.cpp
    LibraryDbTests.cpp
    ScannerTests.cpp
    KitBuilderTests.cpp
    ProjectIOTests.cpp
    MidiExportTests.cpp
    OfflineRenderTests.cpp
    StemNullTests.cpp
    WysiwygExportTests.cpp
    AutosaveTests.cpp
    AppSettingsTests.cpp
    FirstRunStateTests.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/SampleBuffer.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/SampleRetirementPool.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/Clock.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/DrumEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/MasterBus.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/OfflineRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/WavExporter.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/fx/Compressor.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/fx/Crush.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/fx/Drive.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/fx/MasterEq.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/fx/MasterLimiter.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/fx/Punch.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/fx/Space.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/fx/ToneFilter.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/PadMapping.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/Sequencer.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/Voice.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/VoicePool.cpp
    ${CMAKE_SOURCE_DIR}/src/library/SampleLoader.cpp
    ${CMAKE_SOURCE_DIR}/src/library/StarterKit.cpp
    ${CMAKE_SOURCE_DIR}/src/library/KitInstaller.cpp
    ${CMAKE_SOURCE_DIR}/src/library/FeatureExtractor.cpp
    ${CMAKE_SOURCE_DIR}/src/library/Slicer.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/Resample.cpp
    ${CMAKE_SOURCE_DIR}/src/model/Song.cpp
    ${CMAKE_SOURCE_DIR}/src/library/Similarity.cpp
    ${CMAKE_SOURCE_DIR}/src/library/SimilarSearch.cpp
    ${CMAKE_SOURCE_DIR}/src/library/Categoriser.cpp
    ${CMAKE_SOURCE_DIR}/src/library/LibraryDb.cpp
    ${CMAKE_SOURCE_DIR}/src/library/Scanner.cpp
    ${CMAKE_SOURCE_DIR}/src/library/KitBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/library/sqlite/sqlite3.c
    ${CMAKE_SOURCE_DIR}/src/model/RollCompiler.cpp
    ${CMAKE_SOURCE_DIR}/src/model/RollPresets.cpp
    ${CMAKE_SOURCE_DIR}/src/model/FillEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/model/Variator.cpp
    ${CMAKE_SOURCE_DIR}/src/model/FeelPresets.cpp
    ${CMAKE_SOURCE_DIR}/src/model/Humaniser.cpp
    ${CMAKE_SOURCE_DIR}/src/model/ProjectIO.cpp
    ${CMAKE_SOURCE_DIR}/src/model/MidiExporter.cpp
    ${CMAKE_SOURCE_DIR}/src/app/Autosave.cpp
    ${CMAKE_SOURCE_DIR}/src/app/AppSettings.cpp
    ${CMAKE_SOURCE_DIR}/src/app/FirstRunState.cpp
)

target_include_directories(RollForgeTests PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Vendored SQLite (third-party C): pin its options + silence its warnings so the
# project's strict warning flags don't choke on it.
set_source_files_properties(${CMAKE_SOURCE_DIR}/src/library/sqlite/sqlite3.c PROPERTIES
    COMPILE_DEFINITIONS "SQLITE_OMIT_LOAD_EXTENSION=1;SQLITE_THREADSAFE=1"
    COMPILE_OPTIONS "-w")

target_compile_definitions(RollForgeTests PRIVATE
    ${ROLLFORGE_JUCE_DEFINITIONS}
    JUCE_STANDALONE_APPLICATION=1
    JUCE_UNIT_TESTS=1
)

target_link_libraries(RollForgeTests PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_core
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

rollforge_apply_sanitizers(RollForgeTests)

add_test(NAME RollForgeTests COMMAND RollForgeTests)
