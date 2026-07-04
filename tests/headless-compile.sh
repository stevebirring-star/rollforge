#!/usr/bin/env bash
#
# Headless, GUI-free compile + run of the RollForge unit tests.
#
# WHY: JUCE's normal CMake build runs `juceaide`, which compiles juce_gui_basics
# and therefore needs the Linux GUI dev libs (freetype / fontconfig / X11 exts) —
# see HANDOFF.md §2. On a box without those (or without sudo to apt-install them)
# you can still run the engine/model unit tests, which depend only on juce_core +
# juce_audio_basics. This script compiles those two modules plus the pure
# engine/model code and the tests directly, bypassing juceaide, so the headless
# tests can be run locally in seconds.
#
# This is a convenience for local iteration ONLY. CI (.github/workflows/ci.yml)
# builds the full app + tests via CMake on Linux + Windows and remains the
# authoritative cross-platform check.
#
# Usage:   tests/headless-compile.sh
# Env:     CXX (default g++), JUCE_SRC (default build/_deps/juce-src), OUT.
# Needs:   a C++20 compiler and JUCE already fetched (run `cmake -B build ...`
#          once, or point JUCE_SRC at an existing juce-src checkout).
#
# NOTE: keep ENGINE_SOURCES / TEST_SOURCES below in sync with tests/CMakeLists.txt
# as new headless engine/model tests land.

set -euo pipefail
cd "$(dirname "$0")/.."

JUCE_SRC="${JUCE_SRC:-build/_deps/juce-src}"
MODULES="$JUCE_SRC/modules"
if [[ ! -d "$MODULES" ]]; then
    echo "JUCE modules not found at '$MODULES'." >&2
    echo "Configure once (cmake -B build -G Ninja) to fetch JUCE, or set JUCE_SRC." >&2
    exit 1
fi

OUT="${OUT:-build/headless/RollForgeTests}"   # under build/ -> already gitignored
mkdir -p "$(dirname "$OUT")"

# Pure engine/model code under test (no JUCE GUI dependency). AudioEngine.cpp is
# intentionally excluded — it needs juce_audio_devices; DrumEngine is device-free.
ENGINE_SOURCES=(
    src/engine/SampleBuffer.cpp
    src/engine/SampleRetirementPool.cpp
    src/engine/Clock.cpp
    src/engine/DrumEngine.cpp
    src/engine/PadMapping.cpp
    src/engine/Sequencer.cpp
    src/engine/Voice.cpp
    src/engine/VoicePool.cpp
    src/library/SampleLoader.cpp
    src/library/StarterKit.cpp
    src/library/KitInstaller.cpp
    src/model/RollCompiler.cpp
    src/model/RollPresets.cpp
    src/model/FillEngine.cpp
    src/model/Humaniser.cpp
)

# Test translation units (TestMain.cpp provides main() + the category runner).
TEST_SOURCES=(
    tests/SampleBufferTests.cpp
    tests/PadKitTests.cpp
    tests/CommandQueueTests.cpp
    tests/DrumEngineTests.cpp
    tests/VoiceTests.cpp
    tests/VoicePoolTests.cpp
    tests/ChokeGroupTests.cpp
    tests/SampleLoaderTests.cpp
    tests/StarterKitTests.cpp
    tests/KitInstallerTests.cpp
    tests/PadMappingTests.cpp
    tests/PatternModelTests.cpp
    tests/ClockTimingTests.cpp
    tests/SequencerTests.cpp
    tests/PatternSwitchTests.cpp
    tests/RollCompilerTests.cpp
    tests/RollSequencingTests.cpp
    tests/RollPresetsTests.cpp
    tests/FillEngineTests.cpp
    tests/HumaniserTests.cpp
    tests/TestMain.cpp
)

CXX="${CXX:-g++}"

# Compile flags mirror the test target's config in CMakeLists.txt.
"$CXX" -std=c++20 -O0 -g \
    -I "$MODULES" -I src \
    -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 \
    -DJUCE_STANDALONE_APPLICATION=1 -DJUCE_UNIT_TESTS=1 \
    -DJUCE_WEB_BROWSER=0 -DJUCE_USE_CURL=0 -DJUCE_ASIO=0 \
    -DJUCE_STRICT_REFCOUNTEDPOINTER=1 \
    "$MODULES/juce_core/juce_core.cpp" \
    "$MODULES/juce_core/juce_core_CompilationTime.cpp" \
    "$MODULES/juce_audio_basics/juce_audio_basics.cpp" \
    "$MODULES/juce_audio_formats/juce_audio_formats.cpp" \
    "${ENGINE_SOURCES[@]}" "${TEST_SOURCES[@]}" \
    -o "$OUT" \
    -lpthread -ldl -lrt -lm

echo "built: $OUT"
exec "$OUT"
