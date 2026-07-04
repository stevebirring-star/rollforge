# RollForge — Handoff / Continuation Guide

Operational guide for resuming work in a later session. For the full phase →
files/classes map see [`PLAN.md`](PLAN.md); for the manual test checklist see
[`TESTING.md`](TESTING.md). This file is the "how to pick up where we left off".

_Last updated: 2026-07-04 (Phase 1 complete — pads + playback engine)._

---

## 1. Where we are

- **Phase 0 (skeleton) and Phase 1 (pads + playback engine) are complete,
  committed, pushed, and CI-green on both OSes.** HEAD = `e16e87f`. Phase 1 is 9
  commits (`7ffbb6a`..`e16e87f`) plus a compile fixup (`900be7f`); run
  `git log --oneline` for the list.
- **What Phase 1 delivers:** `SampleBuffer` (immutable, reference-counted) + a
  message-thread retirement pool (the final delete never runs on the audio
  thread); `Pad`/`Kit` model; a lock-free command FIFO + `DrumEngine` seam;
  `Voice` (per-voice resampling, one-shot AR envelope, equal-power pan, reverse);
  `VoicePool` (64 voices, steal-the-quietest + choke groups); `SampleLoader`
  (WAV/AIFF/FLAC/Ogg → native rate); `StarterKit` (16 synthesised drums, no
  binary assets); a `PadGrid` UI (click-to-audition + drag-drop file→pad); and
  keyboard + MIDI producers (each its own SPSC queue). Acceptance met: click a pad
  → instant sound; voice-steal + choke groups unit-tested.
- **Verified:** 52 headless `juce::UnitTest` groups pass locally via
  `tests/headless-compile.sh`; full CI (Linux + Windows + ASan/UBSan) is green on
  every Phase-1 commit.
- **Still NOT verified (human checks — no audio machine was used):** actual
  *audible* output, the pad-grid UI interaction, drag-and-drop, and live MIDI
  input. The build machine is headless, so every audio/GUI path compiles, links,
  is RT-safe and unit-tested, but *hearing* it and *clicking* pads needs a human
  at a machine with audio (see TESTING.md).

## 2. Machine / environment gotchas (READ FIRST when resuming)

This is `steveslaptop` (Ubuntu 24.04, gcc 13.3). Key facts:

- **cmake + ninja are NOT system packages** — they live in `~/.local/bin`
  (installed via `pip install --user`). They persist across sessions. Ensure
  `export PATH="$HOME/.local/bin:$PATH"`.
- **`gh` CLI** is also in `~/.local/bin` (v2.96.0, userland).
- **JUCE needs Linux GUI dev libs even to build the *tests*.** JUCE's CMake
  builds a helper tool (`juceaide`) at configure time, and juceaide itself
  compiles `juce_gui_basics` — so freetype/fontconfig/X11-extension dev headers
  are required regardless of `ROLLFORGE_BUILD_APP`. Install them once (needs
  sudo password):
  ```
  sudo apt-get update && sudo apt-get install -y ninja-build \
    libasound2-dev libjack-jackd2-dev libfreetype-dev libfontconfig1-dev \
    libx11-dev libxext-dev libxinerama-dev libxrandr-dev libxcursor-dev
  ```
  > During Phase 0 (no sudo available) these were staged into a **userland
  > prefix** under the session scratchpad + an `xenv.sh` that pointed
  > `PKG_CONFIG_SYSROOT_DIR`/include/lib paths at it. That scratchpad is
  > **session-ephemeral** — do NOT rely on it next session. Prefer the apt
  > install above for a durable setup.
- **JUCE is pinned to tag `8.0.8`** (`ROLLFORGE_JUCE_TAG` in `CMakeLists.txt`).
  Latest available at pin time was 8.0.9. FetchContent re-clones into
  `build/_deps/juce-src`; reuse it across build dirs with
  `-DFETCHCONTENT_SOURCE_DIR_JUCE=<path>/build/_deps/juce-src` to skip re-download.
- **No GitHub auth on this laptop.** Every push has gone via the VPS relay:
  `git bundle create <b> master` → `scp <b> webvps:~/` → on `webvps:~/rollforge-tmp`
  `git fetch <b> master && git merge --ff-only FETCH_HEAD && git push origin master`.
  (Force-push is blocked by the safety guard — land a fixup commit instead of an
  amend if a pushed commit turns out broken.)

## 3. Build / test / sanitize

```bash
export PATH="$HOME/.local/bin:$PATH"
cd ~/rollforge

# Release build + run + test
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/RollForge_artefacts/Release/RollForge
ctest --test-dir build --output-on-failure

# Faster test-only build (still needs the GUI dev libs — juceaide)
cmake -B build -G Ninja -DROLLFORGE_BUILD_APP=OFF && cmake --build build && ctest --test-dir build

# ASan/UBSan (tests only) — mirrors the CI asan job
cmake -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DROLLFORGE_ASAN=ON -DROLLFORGE_BUILD_APP=OFF
cmake --build build-asan --target RollForgeTests
ctest --test-dir build-asan --output-on-failure
```

**No GUI dev libs / no sudo?** The CMake path above needs the Linux GUI dev
packages because `juceaide` compiles `juce_gui_basics` (see §2). When you can't
install them, run the headless engine/model tests directly — this compiles only
`juce_core` + `juce_audio_basics` + `juce_audio_formats` + the pure code under
test, bypassing juceaide:

```bash
tests/headless-compile.sh      # builds build/headless/RollForgeTests and runs it
```

Keep its `ENGINE_SOURCES`/`TEST_SOURCES` lists in sync with `tests/CMakeLists.txt`
as new headless tests land. Device-coupled code (`AudioEngine`, using
`juce_audio_devices`) and all `ui/` code are NOT in the headless build — only CI
compiles those. CI remains the authoritative full build on both OSes.

## 4. Immediate next actions (Phase 2 — Sequencer core)

Goal: up to 16 lanes × 64 steps, base 1/16 grid, per-lane triplet + length
(polyrhythms), rich per-step data (velocity, micro-shift, ratchets, probability,
sample-lock), and a sample-accurate transport. Acceptance: timing tests exact
across buffer sizes; ratchets even; pattern A→B on the bar boundary, glitch-free.
Full file/class map: PLAN.md Phase 2. `juce::UndoManager` arrives here (per spec).

**Recommended first commit:** the **model + the double-buffered snapshot** —
`model/Step`, `model/Lane`, `model/Pattern` (lanes + BPM + swing) with a pure
`snapshot()` the audio thread reads via an **atomic pointer swap** (never
mutating shared state, never blocking). This is Phase 2's load-bearing threading
decision (mirrors Phase 1's SampleBuffer-first choice): the Clock/Sequencer are
shaped by how they read pattern data RT-safely. Fully headless-testable.

**Suggested build order (each a commit; keep the model pure + unit-tested):**
1. `model/Step` + `model/Lane` + `model/Pattern` + double-buffered `snapshot()`.
2. `engine/Clock` — sample-accurate scheduler → exact per-block sample offsets;
   drift-free at 64/256/1024 buffers. `ClockTimingTests`.
3. `engine/Sequencer` — reads the snapshot, emits pad triggers to `DrumEngine` at
   the right sample offsets within a block; swing. Wire a transport (play/stop,
   BPM) into `AudioEngine`. Headless test that a one-step pattern fires once/bar.
4. Per-step richness in the Sequencer: ratchets (1–8 + ramp), probability
   (100/75/50/25, seeded), micro-shift (±50%). `RatchetTests`, `SwingTests`.
5. `model/PatternBank` (slots A–H) + bar-boundary pattern switch. `PatternSwitchTests`.
6. `ui/TransportBar` — play/stop, BPM, tap tempo, swing, metronome, pattern slots.
7. `ui/SequencerGrid` + `ui/StepComponent` — the grid, vertical-drag velocity,
   60 Hz playhead read from an atomic (never full-window repaints per frame).
8. `model/UndoableActions` — `juce::UndoManager`-backed edits.

**Threading contract (carry forward):** the audio thread reads a double-buffered
pattern snapshot (atomic pointer swap) — it never mutates shared model state and
never blocks. UI→engine still goes through the lock-free command queue; engine→UI
telemetry (playhead, meters) via `std::atomic`, read by a 60 Hz UI timer. The
Sequencer emits into the SAME `DrumEngine` trigger path built in Phase 1 (feed it
via the command queue, or a direct in-audio-thread call from `process()` — decide
in commit 3).

**Decisions to pin for Phase 2:** exact swing model (delay of the off-beat 8ths);
ratchet ramp shape; probability RNG seeding (per-loop, deterministic for tests);
pattern-switch quantisation (bar only, or configurable); whether lanes are
fixed-16 or dynamic. Default to the simplest testable choice and note it.

## 5. Open items / risks

- **CI proven on both OSes, through all of Phase 1.** `.github/workflows/ci.yml`
  (ubuntu-22.04 + windows-latest + a Linux ASan/UBSan job) is green on every
  Phase-1 commit up to HEAD `e16e87f`. History worth knowing: the first-ever run
  failed on Windows (`jack/jack.h`), fixed by per-platform backend gating; and 8/9
  first failed to compile the app (a JUCE override name — `isInterestedInFileDrag`,
  not `...AndDrop`), fixed by `900be7f`. Optional future optimisation: an
  `actions/cache` step for `build/_deps` if the JUCE clone time grows.
- **Open human verifications (no audio machine used yet):** audible output; the
  pad-grid UI (click-to-audition, flash, drag-drop file→pad); and live MIDI input
  (notes 36–51 → pads). All compile/link/unit-test clean; they need a human at a
  machine with audio (see TESTING.md).
- **Engine still has no build boundary.** `AudioEngine.cpp` (and all engine/model
  code) is compiled straight into the app / test targets, so the "no JUCE-GUI
  includes under `src/engine`/`src/model`" rule is convention-only. The headless
  test path (`headless-compile.sh`) compiles the pure engine/model/library code
  without GUI, which catches accidental GUI includes in practice, but there is no
  linked STATIC engine lib. Extract one before/at the VST3 wrapper stage to make
  the rule mechanical. _(Follow-up, minor.)_
- **Single-instance behaviour undecided.** `Main.cpp` allows multiple instances
  while each opens the default output device; a 2nd instance can silently fail to
  open it (exclusive-mode WASAPI / some ALSA configs). Make a deliberate call
  before shipping. _(nit.)_
- **MP3 decoding is present but off** (`JUCE_USE_MP3AUDIOFORMAT=0`) — enable the
  flag if MP3 import is wanted (patents expired; matches the ASIO-off caution).
- `packaging/ci/` is an empty dir (real packaging = Phase 7).

## 6. Key decisions & rationale

- **Standalone-only, engine/UI decoupled** — `src/engine` and `src/model` must
  never include a JUCE GUI module, so a VST3 wrapper can reuse them later.
- **JSON for projects/kits/patterns; SQLite (vendored amalgamation) for the
  sample library index** (SQLite arrives in Phase 5).
- **RT-safety is non-negotiable:** UI↔engine via lock-free FIFO + atomics;
  pattern data read via double-buffered snapshot swap; 60 Hz UI timer reads
  atomics (never full-window repaints per frame).
- **Samples stored at native rate; the Voice resamples per-voice** — robust to a
  runtime device-sample-rate change without rebuilding any buffer.
- **Test runner runs only the `rollforge` category** — never JUCE's internal
  suite (faster; avoids unrelated UBSan noise). All RollForge tests must
  register under that category.
- **GPLv3 build** (JUCE GPL option). A JUCE commercial license is required
  before any closed-source distribution — see `README.md` / `packaging/README.md`.
