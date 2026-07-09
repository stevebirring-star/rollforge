# RollForge — Handoff / Continuation Guide

Operational guide for resuming work in a later session. For the full phase →
files/classes map see [`PLAN.md`](PLAN.md); for the manual test checklist see
[`TESTING.md`](TESTING.md). This file is the "how to pick up where we left off".

_Last updated: 2026-07-09 (the Credibility / table-stakes tier is complete on `feature/make-a-beat`)._

---

## 0. Read this first (2026-07-09)

**Branch `feature/make-a-beat` is 23 commits ahead of origin and NOT pushed** — CI is still
billing-blocked, so verification has been local. `cmake --build build-local && ./build-local/
tests/RollForgeTests_artefacts/Release/RollForgeTests` → **196 test groups pass**.

Both post-v1 tiers are now done: the **moat** (generative rhythm) and **Credibility**
(table-stakes). Credibility #5–#10 landed this session: slice-loop-to-pads, browser
audition + send-to-pad + hot-swap, native drag-out to a DAW, per-pad tone + reverb send,
velocity layers + round-robin, and per-lane triplets.

**The GUI can now be driven and screenshotted from an agent session** when the laptop is
docked (two monitors on `:0`): windows open Normal rather than Iconic. Use `xwininfo`'s
"Absolute upper-left" for the client origin (`xdotool getwindowgeometry` reports the frame
and your clicks land wrong), `xdotool` to click/drag, and `ffmpeg -f x11grab` to capture
(there is no imagemagick on this box). Delete `~/.config/RollForge/recovery.rollforge`
before each run or the previous session is restored. Never `pkill -f 'RollForge…'` — the
pattern matches your own shell's argv and kills it; kill by PID.

That capability immediately found three bugs that had survived every headless test:

1. **Undo/redo never worked on Linux.** X11 hands JUCE the lowercase keysym for Ctrl+Z, and
   `keyPressed` compared against `'Z'`. Fixed (`9f1d6a3`).
2. **The master EQ and glue compressor were missing from every export**, contradicting the
   "guaranteed WYSIWYG export" the moat claims. The golden test only exercised one control;
   it now drives all eight. Stems also now render pre-master so they sum to the mix
   (`a973431`).
3. **Loading a sample onto a pad never reset its trim**, so dropping a kick on a slice pad
   played 12% of it (`004796a`).

Next up is the roadmap artifact's "Later / P2" tier (waveform-on-pads polish, browser
similarity sort, background scan) and the v2 big bet (beatbox / tap-to-pattern).

---

## 1. Where we are

- **Phases 0–7 are complete and committed — RollForge is v1 feature-complete.**
  Phases 0–6 are pushed and CI-green on both OSes; the Phase-7 commits
  (`e1d0264`..`404824f`) are committed and pushed this session for the same CI to
  verify. Phase 1 = 9 commits (`7ffbb6a`..`e16e87f`, + fixup `900be7f`); Phase 2 = 8
  commits (`234584c`..`094adce`); Phase 3 = 8 commits (`3bfcd2d`..`fb01993`, + a
  Windows stack-overflow fix `f2e4ba8`); Phase 4 = 6 commits (`6071e3b`..`5e393f7`);
  Phase 5 = 5 commits (`f2a0d7c`..`d18ec8b`); Phase 6 = 5 commits (`3c88551`..
  `5e69e57`); Phase 7 = 5 sub-phases + 1 review fixup (`e1d0264`..`404824f`). Run
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
- **What Phase 5 delivers:** a vendored-SQLite sample library — `FeatureExtractor`
  (RMS / ZCR / decay / onsets), `Categoriser` (filename tokens then feature rules
  → a drum category), `LibraryDb` (SQLite 3.53.3 amalgamation wrapper: upsert,
  category / favourites queries), `Scanner` (recursive folder scan → features →
  DB), and `KitBuilder` (NEW KIT: a seeded, coherent kit with per-pad locks), plus
  a `BrowserPanel` UI (scan / filter / NEW KIT, opened from a "Library" button).
  Deferred: background-threaded scan, audition-on-click, drag→pad.
- **What Phase 6 delivers:** export & interop — `model/ProjectIO` (`.rollforge`
  JSON save/load of the session), `model/MidiExporter` (Pattern → GM-drum MIDI;
  ratchets/rolls flattened), `engine/OfflineRenderer` (faster-than-realtime render
  reusing the Sequencer + DrumEngine + MasterBus, no device), `engine/WavExporter`
  (full-mix WAV + per-pad stems that null against the mix), and an `ExportPanel`
  UI (Export MIDI / WAV / stems, from an "Export" button; render on a fresh engine
  so live audio is untouched). Deferred: project save/load UI (needs per-pad path
  tracking) + drag-out (`performExternalDragDropOfFiles`).
- **What Phase 7 delivers (packaging & polish — the last phase):** crash-recovery
  `app/Autosave` (writes a recovery `.rollforge` via ProjectIO every ~60 s on the UI
  timer, restores it if present at launch, clears it on a clean quit); persisted
  `app/AppSettings` (UI scale + sample folders as JSON under app-data) + a
  `ui/SettingsView` (audio device/buffer selector + UI-scale chooser + add-sample-
  folder); a one-time `ui/FirstRun` welcome overlay gated by a pure, headless-tested
  `app/FirstRunState` marker file (deleted-safely on dismiss via SafePointer +
  callAsync); and dual-OS packaging — `packaging/linux` (`build-appimage.sh` → an
  AppImage via linuxdeploy + its appimage plugin, plus a portable tar.gz; a `.desktop`
  entry + SVG icon) and `packaging/windows` (`build-packages.ps1` → an Inno-Setup
  installer + a portable zip; the app statically links the MSVC runtime so neither
  needs a vcredist) — all driven by a new `.github/workflows/release.yml` (a `v*` tag
  publishes a GitHub Release; `workflow_dispatch` smoke-tests). Deferred: the FirstRun
  muted-demo-loop / pulsing-Play / coach-marks (a simpler welcome overlay shipped).
- **Verified:** 130 headless `juce::UnitTest` groups pass locally via
  `tests/headless-compile.sh` (Phase 7 added Autosave + AppSettings + FirstRunState);
  full CI (Linux + Windows + ASan/UBSan) is green on every Phase-0..6 commit, and the
  Phase-7 commits (`e1d0264`..`404824f`) are pushed for the same CI to verify. An
  adversarial multi-agent review of the Phase-7 diff caught one blocker (the missing
  linuxdeploy appimage plugin, fixed in `404824f`); the C++/CMake/CI-YAML review
  dimensions came back clean.
- **Still NOT verified (human checks — no audio machine was used):** actual
  *audible* output; the pad-grid + sequencer-grid UI (step editing, playhead,
  transport, swing, undo); the Phase-3 UI (paint a roll with the brush, FILL /
  Reroll, the Humanise knob); the Phase-4 macro knobs (PUNCH / SPACE / CRUSH /
  DRIVE) actually sounding good full-travel; the Phase-5 library (scan a real
  folder → browse by category → NEW KIT loads a playable kit); the Phase-6 export
  (Export MIDI / WAV / stems open in a DAW correctly); drag-and-drop file→pad; live
  MIDI input; and the Phase-7 polish (install the AppImage / installer on a clean box
  → sound in ≤ 3 clicks, the first-run welcome overlay, settings persistence + UI
  scale, and autosave crash-recovery). The build machine is headless, so every
  audio/GUI path compiles, links, is RT-safe and unit-tested, but *hearing* it and
  *clicking* pads needs a human at a machine with audio (see TESTING.md).

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

## 4. Immediate next actions (v1 is feature-complete)

All seven phases are done (see §1). RollForge is **v1 feature-complete**: pads +
sequencer, the roll/fill/humanise differentiators, master macro FX, the SQLite
sample library + auto-kits, MIDI/WAV/stems export + `.rollforge` I/O, and Phase-7
packaging & polish (Autosave, Settings, a one-time welcome, and Linux + Windows
installers/portable builds). What remains is **not** new features — it is
verification and release mechanics:

1. **Cut a release.** Push a `v*` tag (via the VPS relay, §2) — `release.yml` builds
   the Release app on both OSes, produces the AppImage + tar.gz (Linux) and the Inno
   installer + portable zip (Windows), and publishes a GitHub Release. To dry-run the
   packaging without publishing, trigger `release.yml` via **workflow_dispatch** and
   inspect the uploaded artifacts.
2. **Human acceptance on real hardware** (the one thing no build machine has done —
   see the NOT-verified list in §1 and TESTING.md): install each package on a clean
   box and confirm sound in ≤ 3 clicks; click through pads / sequencer / roll brush /
   FILL / macro knobs / library + NEW KIT / export; verify the first-run overlay,
   settings persistence + UI scale, and autosave recovery; verify live MIDI input.
3. **Optional follow-ups (documented, not blockers):** the deferred items in §5 (a
   linked static engine lib, single-instance policy, MP3-decode flag, triplet timing,
   backward micro-shift, alt-sample jitter, per-pad SPACE sends, project save/load UI
   + drag-out, the FirstRun muted-demo-loop/coach-marks) and — if closed-source
   distribution is ever wanted — a commercial JUCE license (see README / LICENSE).

**Release gotchas:** `release.yml` is packaging-only (tags + manual dispatch); the
everyday build+test signal stays in `ci.yml`. The Windows exe statically links the
MSVC runtime (CMakeLists) so the zip/installer need no vcredist. The Linux AppImage
build needs BOTH `linuxdeploy` and its separate `linuxdeploy-plugin-appimage` (the
`--output appimage` backend) — `build-appimage.sh` downloads both and puts the tools
dir on PATH; missing that plugin was a real bug caught in review (fixed in `404824f`).

## 5. Open items / risks

- **CI proven on both OSes, through Phase 6; Phase 7 pushed for the same CI.**
  `.github/workflows/ci.yml` (ubuntu-22.04 + windows-latest + a Linux ASan/UBSan job)
  is green on every Phase-0..6 commit up to `5e69e57` — including the vendored SQLite
  amalgamation compiling under MSVC + ASan. Phase 7 adds two new CI surfaces to watch
  on the pushed run: the **static MSVC runtime** on the Windows app build
  (`MSVC_RUNTIME_LIBRARY` — a /MT vs /MD mismatch would surface as a link error) and
  the packaging-only **`release.yml`** (only runs on `v*` tags / `workflow_dispatch`,
  so exercise it deliberately — it is not part of the per-push signal). (Linux + ASan finish in a couple of
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
- `packaging/ci/` is a reserved (currently empty) dir; the real packaging lives in
  `packaging/linux` + `packaging/windows` + `.github/workflows/release.yml` (Phase 7).

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
