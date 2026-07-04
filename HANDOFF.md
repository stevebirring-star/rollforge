# RollForge — Handoff / Continuation Guide

Operational guide for resuming work in a later session. For the full phase →
files/classes map see [`PLAN.md`](PLAN.md); for the manual test checklist see
[`TESTING.md`](TESTING.md). This file is the "how to pick up where we left off".

_Last updated: 2026-07-04 (Phase 4 complete — macro effects)._

---

## 1. Where we are

- **Phases 0–4 are complete, committed, pushed, and CI-green on both OSes.** HEAD =
  `5e393f7`. Phase 1 = 9 commits (`7ffbb6a`..`e16e87f`, + fixup `900be7f`);
  Phase 2 = 8 commits (`234584c`..`094adce`); Phase 3 = 8 commits (`3bfcd2d`..
  `fb01993`, + a Windows stack-overflow fix `f2e4ba8`); Phase 4 = 6 commits
  (`6071e3b`..`5e393f7`). Run `git log --oneline` for the list.
- **What Phase 1 delivers:** `SampleBuffer` (immutable, reference-counted) + a
  message-thread retirement pool (the final delete never runs on the audio
  thread); `Pad`/`Kit` model; a lock-free command FIFO + `DrumEngine` seam;
  `Voice` (per-voice resampling, one-shot AR envelope, equal-power pan, reverse);
  `VoicePool` (64 voices, steal-the-quietest + choke groups); `SampleLoader`
  (WAV/AIFF/FLAC/Ogg → native rate); `StarterKit` (16 synthesised drums, no
  binary assets); a `PadGrid` UI (click-to-audition + drag-drop file→pad); and
  keyboard + MIDI producers (each its own SPSC queue). Acceptance met: click a pad
  → instant sound; voice-steal + choke groups unit-tested.
- **What Phase 2 delivers:** `Step`/`Lane`/`Pattern` model + a lock-free
  `TripleBuffer` snapshot; a sample-accurate `Clock` (drift-free across buffer
  sizes; adversarially verified); a `Sequencer` that fires pads at exact sample
  offsets with per-step ratchets / probability / micro-shift + global swing; a
  `PatternBank` (A–H) with glitch-free bar-boundary switching; and the
  `TransportBar` + editable `SequencerGrid` UI with `UndoManager` (Cmd/Ctrl+Z).
  Acceptance met: timing exact across buffer sizes; A→B on the bar boundary.
- **What Phase 3 delivers (the differentiators):** a pure `RollCompiler` that turns
  a `RollRegion` (Speed/Volume/Pitch curves) into tempo-independent step-offset
  events; rolls carried in the `Pattern` snapshot and fired by the `Sequencer` at
  sample-accurate offsets (compiled on the message thread, scaled on the audio
  thread — no RT compilation); 10 named `RollPresets`; a deterministic per-style
  `FillEngine` (seeded, intensity 1–5, writes steps + a roll); a one-knob
  `Humaniser` (seeded forward-timing + velocity jitter, non-destructive); and the
  UI — a `FillBar` (FILL / Reroll / Humanise), a `RollBrushOverlay` (drag to paint
  an accelerating roll on a lane), and a roll-preset picker. Deferred (documented):
  per-lane triplet timing, backward micro-shift "rush", and alt-sample jitter.
- **What Phase 4 delivers:** a `MasterBus` on the audio output with four macro
  effects — `Punch` (transient emphasis + parallel saturation), `Drive` (tanh
  saturation + high-shelf), `Crush` (bit + sample-rate reduction), `Space` (plate
  reverb send) — each 0 = bypass, plus an always-on brickwall `MasterLimiter`; and
  a `MacroKnobs` UI (four rotary knobs). All effects are hand-rolled (no juce::dsp)
  so they are headless-tested (`FxSmokeTests`: bypass at 0, no NaN/clip at full).
  Deferred: per-pad SPACE sends (need a send level on `Pad`).
- **Verified:** 107 headless `juce::UnitTest` groups pass locally via
  `tests/headless-compile.sh`; full CI (Linux + Windows + ASan/UBSan) is green on
  every Phase-0..4 commit.
- **Still NOT verified (human checks — no audio machine was used):** actual
  *audible* output; the pad-grid + sequencer-grid UI (step editing, playhead,
  transport, swing, undo); the Phase-3 UI (paint a roll with the brush, FILL /
  Reroll, the Humanise knob); the Phase-4 macro knobs (PUNCH / SPACE / CRUSH /
  DRIVE) actually sounding good full-travel; drag-and-drop file→pad; and live MIDI
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

## 4. Immediate next actions (Phase 5 — Sample library + auto-kits)

Phase 4 is complete (see §1). Next is Phase 5: a vendored-SQLite sample library
that scans folders in the background, extracts audio features, auto-categorises
sounds, and builds coherent random kits ("NEW KIT"). Full file/class map: PLAN.md
Phase 5. Acceptance: scan 5k files < 60s; categoriser ~85% on obviously-named
files; NEW KIT always playable.

**Recommended first commit:** `library/FeatureExtractor` + `library/Categoriser` —
the pure analysis core (RMS / spectral centroid / ZCR / onset count -> a category
{kick,snare,clap,hat-closed,hat-open,tom,perc,fx}, filename tokens first then a
feature-rule fallback). Fully headless-testable (`CategoriserTests` on named
files); the DB, scanner, and browser wrap it.

**Suggested build order (each a commit):**
1. `library/FeatureExtractor` (RMS, centroid, ZCR, onsets) + `library/Categoriser`
   (filename tokens -> feature-rule fallback). `CategoriserTests`.
2. `library/sqlite/sqlite3.{c,h}` (vendored amalgamation) + `library/LibraryDb`
   (schema + queries). Round-trip test.
3. `library/Scanner` (background recursive scan via `juce::ThreadPool` -> features
   -> DB). Test a small folder scans + populates.
4. `library/KitBuilder` (NEW KIT: coherent, level-matched random kit; per-pad
   locks; seeded RNG). `KitBuilderTests` (always playable).
5. `ui/BrowserPanel` (category tabs, search, similarity sort, audition, drag->pad,
   favourites) + `SimilaritySortTests`.

**Carry forward:** the analysis / DB / kit-builder code is non-GUI, so keep it
headless-testable (add `sqlite3.c` + the library `.cpp` to `headless-compile.sh`;
SQLite is C, the amalgamation compiles as-is). The scanner runs OFF the audio
thread (a `juce::ThreadPool`); only the `BrowserPanel` UI needs CI. Seed
`KitBuilder`'s RNG so `KitBuilderTests` can pin it.

**Decisions to pin for Phase 5:** where the DB file lives (user app-data dir);
categoriser token list + feature thresholds; similarity metric (feature-vector
distance); NEW KIT coherence rules. Default to the simplest testable choice.

## 5. Open items / risks

- **CI proven on both OSes, through all of Phase 4.** `.github/workflows/ci.yml`
  (ubuntu-22.04 + windows-latest + a Linux ASan/UBSan job) is green on every
  Phase-0..4 commit up to HEAD `5e393f7`. (Linux + ASan finish in a couple of
  minutes; the Windows job — a cold MSVC + JUCE build — often takes 10–15 min but
  has never failed on a green Linux commit.) History worth knowing: the first-ever
  run failed on Windows (`jack/jack.h`), fixed by per-platform backend gating; 8/9
  first failed to compile the app (a JUCE override name — `isInterestedInFileDrag`,
  not `...AndDrop`), fixed by `900be7f`; and a Phase-3 commit SEGFAULTed the Windows
  test binary — a stack overflow from the enlarged `Pattern` (compiled rolls) held
  by-value in a stack-allocated `Sequencer` (~350 KB > Windows' 1 MB stack), fixed
  by heap-allocating the pattern state (`f2e4ba8`; Linux's 8 MB stack + ASan had
  hidden it — so **large engine state belongs on the heap**). Optional future
  optimisation: an `actions/cache` step for `build/_deps` if the JUCE clone grows.
- **Deferred (documented in code; not blockers):** per-lane **triplet** timing —
  the `Clock` emits a straight 1/16 grid, so triplet lanes need lane timing
  decoupled from that grid (a finer clock or per-lane sub-clocking); **backward
  micro-shift / swing "rush"** — a step's events are generated at its grid
  boundary, so a backward shift needs a one-step look-ahead (forward micro-shift,
  swing, and the Humaniser's forward jitter all work and are tested); and
  **alt-sample jitter** in the Humaniser — needs the Voice/DrumEngine to support
  per-hit alternate-sample selection.
- **Open human verifications (no audio machine used yet):** audible output; the
  pad-grid + sequencer-grid UI (audition, drag-drop, step editing, transport,
  playhead, swing, undo); the Phase-3 UI (paint a roll with the brush + preset
  picker, FILL / Reroll, the Humanise knob); and live MIDI input (notes 36–51 →
  pads). All compile/link/unit-test clean; they need a human at a machine with
  audio (see TESTING.md).
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
