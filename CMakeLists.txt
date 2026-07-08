cmake_minimum_required(VERSION 3.22)

project(RollForge VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Global build settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to Release for single-config generators (Ninja, Makefiles).
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

option(ROLLFORGE_BUILD_APP   "Build the RollForge GUI application"           ON)
option(ROLLFORGE_BUILD_TESTS "Build RollForge unit tests"                    ON)
option(ROLLFORGE_ASAN        "Build with Address/UB sanitizers (GCC/Clang)"  OFF)

# -DROLLFORGE_BUILD_APP=OFF skips compiling/linking the GUI app target, which
# speeds up test-only builds (used by the ASan CI job below). NOTE: JUCE still
# builds its "juceaide" helper at configure time and juceaide itself compiles
# juce_gui_basics, so the Linux GUI dev packages (freetype, fontconfig, X11
# extensions) are required even for a tests-only build.

# ---------------------------------------------------------------------------
# Dependency: JUCE (pinned to a specific release tag for reproducibility)
# ---------------------------------------------------------------------------
include(FetchContent)

set(ROLLFORGE_JUCE_TAG "8.0.8" CACHE STRING "JUCE release tag to build against")

message(STATUS "RollForge: fetching JUCE ${ROLLFORGE_JUCE_TAG}")
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${ROLLFORGE_JUCE_TAG}
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# Sanitizer helper (opt-in, used by the ASan CI job)
# ---------------------------------------------------------------------------
function(rollforge_apply_sanitizers target)
    if(ROLLFORGE_ASAN AND (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang"))
        message(STATUS "RollForge: enabling ASan/UBSan for ${target}")
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -g)
        target_link_options(${target}    PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Compile definitions shared by every RollForge target.
#   * WEB_BROWSER/CURL off  -> no webkit2gtk / libcurl needed on Linux.
#   * ALSA + JACK on        -> PipeWire is reached via its ALSA/JACK bridges.
#   * WASAPI + DirectSound  -> Windows backends; ASIO stays off (SDK licensing).
# ---------------------------------------------------------------------------
set(ROLLFORGE_JUCE_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_ASIO=0                     # ASIO excluded on every platform (SDK licensing)
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    # Audio backends MUST be gated per platform. JUCE honours JUCE_JACK on
    # Windows too (audio_devices #includes <jack/jack.h>), which needs the JACK
    # SDK — absent on the Windows CI runner. Likewise ALSA is Linux-only.
    $<$<PLATFORM_ID:Linux>:JUCE_ALSA=1>
    $<$<PLATFORM_ID:Linux>:JUCE_JACK=1>
    $<$<PLATFORM_ID:Windows>:JUCE_WASAPI=1>
    $<$<PLATFORM_ID:Windows>:JUCE_DIRECTSOUND=1>
)

# ---------------------------------------------------------------------------
# Application target (Standalone only for now; engine/UI kept decoupled so a
# VST3/AU wrapper can be added later without touching engine or model code).
# ---------------------------------------------------------------------------
if(ROLLFORGE_BUILD_APP)
    juce_add_gui_app(RollForge
        PRODUCT_NAME "RollForge"
        COMPANY_NAME "StackBase"
        BUNDLE_ID    "com.stackbase.rollforge"
        VERSION      ${PROJECT_VERSION}
    )

    target_sources(RollForge PRIVATE
        src/app/Main.cpp
        src/app/Autosave.cpp
        src/app/AppSettings.cpp
        src/app/FirstRunState.cpp
        src/ui/MainComponent.cpp
        src/ui/FirstRun.cpp
        src/ui/PadComponent.cpp
        src/ui/PadGrid.cpp
        src/ui/TransportBar.cpp
        src/ui/StepComponent.cpp
        src/ui/SequencerGrid.cpp
        src/ui/LaneLockButton.cpp
        src/ui/AboutView.cpp
        src/ui/FillBar.cpp
        src/ui/RollBrushOverlay.cpp
        src/ui/MacroKnobs.cpp
        src/ui/BrowserPanel.cpp
        src/ui/ExportPanel.cpp
        src/ui/SettingsView.cpp
        src/engine/AudioEngine.cpp
        src/engine/Clock.cpp
        src/engine/DrumEngine.cpp
        src/engine/MasterBus.cpp
        src/engine/OfflineRenderer.cpp
        src/engine/WavExporter.cpp
        src/engine/fx/Crush.cpp
        src/engine/fx/Drive.cpp
        src/engine/fx/MasterLimiter.cpp
        src/engine/fx/Punch.cpp
        src/engine/fx/Space.cpp
        src/engine/PadMapping.cpp
        src/engine/SampleBuffer.cpp
        src/engine/SampleRetirementPool.cpp
        src/engine/Sequencer.cpp
        src/engine/Voice.cpp
        src/engine/VoicePool.cpp
        src/library/SampleLoader.cpp
        src/library/StarterKit.cpp
        src/library/KitInstaller.cpp
        src/library/FeatureExtractor.cpp
        src/library/Categoriser.cpp
        src/library/LibraryDb.cpp
        src/library/Scanner.cpp
        src/library/KitBuilder.cpp
        src/library/sqlite/sqlite3.c
        src/model/RollCompiler.cpp
        src/model/RollPresets.cpp
        src/model/FillEngine.cpp
        src/model/Variator.cpp
        src/model/Humaniser.cpp
        src/model/ProjectIO.cpp
        src/model/MidiExporter.cpp
    )

    target_include_directories(RollForge PRIVATE src)

    # Vendored SQLite amalgamation (third-party C): pin its options (no extension
    # loading; thread-safe) and silence its warnings so the project's strict
    # warning flags don't choke on it.
    set_source_files_properties(src/library/sqlite/sqlite3.c PROPERTIES
        COMPILE_DEFINITIONS "SQLITE_OMIT_LOAD_EXTENSION=1;SQLITE_THREADSAFE=1"
        COMPILE_OPTIONS "-w")

    target_compile_definitions(RollForge PRIVATE
        ${ROLLFORGE_JUCE_DEFINITIONS}
        JUCE_APPLICATION_NAME_STRING="RollForge"
        JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
    )

    # juce_audio_utils transitively pulls in audio_devices/formats/processors,
    # gui_extra/gui_basics/graphics, events, data_structures and core.
    target_link_libraries(RollForge PRIVATE
        juce::juce_audio_utils
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
    )

    rollforge_apply_sanitizers(RollForge)

    # Windows: statically link the MSVC runtime so the portable .zip and the Inno
    # installer are self-contained (no Visual C++ redistributable dependency). JUCE
    # module sources compile into this target, so they pick up the same runtime.
    # CMP0091 is NEW by default at cmake_minimum_required 3.22, making this work.
    if(MSVC)
        set_target_properties(RollForge PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(ROLLFORGE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
