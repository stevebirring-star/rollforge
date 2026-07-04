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
    ${CMAKE_SOURCE_DIR}/src/engine/SampleBuffer.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/SampleRetirementPool.cpp
)

target_include_directories(RollForgeTests PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_compile_definitions(RollForgeTests PRIVATE
    ${ROLLFORGE_JUCE_DEFINITIONS}
    JUCE_STANDALONE_APPLICATION=1
    JUCE_UNIT_TESTS=1
)

target_link_libraries(RollForgeTests PRIVATE
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

rollforge_apply_sanitizers(RollForgeTests)

add_test(NAME RollForgeTests COMMAND RollForgeTests)
