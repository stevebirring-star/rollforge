# RollForge — Implementation Plan

**RollForge** is a standalone desktop drum machine and groove sketchpad for
Linux and Windows. Its identity: *the fastest, least technical way to make
hi-hat rolls, drum fills and finished-sounding beats.* A non-technical user
must get a good-sounding groove in under 60 seconds without reading anything.

This document maps every phase to concrete files/classes and is kept current
as the build progresses. Status legend: ✅ done · 🚧 in progress · ⬜ not started.

---

## Architecture at a glance

Four layers, strictly separated so the engine can later be wrapped as a VST3
without touching model or UI code:

| Layer      | Directory      | Rule                                                            |
|------------|----------------|-----------------------------------------------------------------|
| `model/`   | pure data      | No JUCE GUI. Deterministic, unit-tested pure functions.         |
| `engine/`  | real-time audio| No JUCE GUI. No locks/alloc/IO/logging on the audio thread.     |
| `library/` | sample index   | Background scanning, SQLite, categoriser. No GUI.               |
| `ui/`      | JUCE Components | All GUI lives here. Talks to engine via lock-free FIFOs/atomics.|
| `app/`     | shell          | `JUCEApplication`, main window, wiring.                         |

**Threading contract**
- Audio thread reads a **double-buffered pattern snapshot** (atomic pointer
  swap); it never mutates shared model state and never blocks.
- UI → engine commands go through a lock-free `juce::AbstractFifo` command queue.
- Engine → UI telemetry (playhead, meters) is published via `std::atomic`,
  read by a 60 Hz UI timer. The whole window is never repainted per frame.

---

## Phase 0 — Skeleton ✅

CMake + JUCE fetch, empty window, audio device init, sine blip, CI on both OSes.

| File | Purpose |
|------|---------|
| `CMakeLists.txt` | CMake ≥3.22, C++20, `FetchContent` JUCE **8.0.8** (pinned), `juce_add_gui_app`, ALSA+JACK / WASAPI+DirectSound flags, `WEB_BROWSER=0`/`USE_CURL=0`, ASan option, `ROLLFORGE_BUILD_APP` toggle. |
| `src/engine/AudioEngine.{h,cpp}` | Owns `AudioDeviceManager`, `AudioIODeviceCallback`, RT-safe one-shot sine blip via a single atomic flag. No GUI includes. |
| `src/ui/MainComponent.{h,cpp}` | Dark view: title, **Play Blip** button, **Audio Settings** (async `AudioDeviceSelectorComponent`), status line, space-bar → blip. |
| `src/app/Main.cpp` | `RollForgeApplication` + `MainWindow` (DocumentWindow). |
| `tests/CMakeLists.txt`, `tests/TestMain.cpp` | `juce::UnitTest` runner; Phase-0 smoke test; CTest registration; non-zero exit on failure. |
| `.github/workflows/ci.yml` | Build+test matrix (ubuntu-22.04, windows-latest) + Linux ASan/UBSan job. |
| `README.md`, `TESTING.md`, `LICENSE` | Docs, manual checklist, GPLv3 notice. |

**Accept:** builds clean on both OSes in CI ✅ (green on `7ddd435` — Linux +
Windows + ASan/UBSan); blip audible locally ⬜ (pending human check — headless
build machine).

---

## Phase 1 — Pads + playback engine ✅

16 pads (4×4), 64-voice polyphonic pool, per-pad params, sample loading, code-synthesised starter kit.

| File | Classes / responsibility |
|------|--------------------------|
| `model/Pad.h` | `Pad`: sample ref, volume, pan, pitch (±12 st), attack/release, choke group, reverse, up-to-4 sample alternates. |
| `model/Kit.h` | `Kit`: 16 `Pad`s + kit metadata. |
| `engine/Voice.{h,cpp}` | One playing voice: resampled sample playback, ADSR-ish env, pan, reverse. |
| `engine/VoicePool.{h,cpp}` | 64-voice pool, voice stealing (oldest/quietest), choke-group handling. |
| `engine/SampleBuffer.{h,cpp}` | Loaded, resampled audio (device-rate) + refcounting. |
| `engine/DrumEngine.{h,cpp}` | Trigger routing pad→voice; owns the pool + master mix. |
| `library/SampleLoader.{h,cpp}` | WAV/AIFF/FLAC/MP3/OGG via `juce::AudioFormatManager`; resample on load. |
| `library/StarterKit.{h,cpp}` | Synthesises 16 clean sounds in code at first run (no binary assets). |
| `ui/PadGrid.{h,cpp}`, `ui/PadComponent.{h,cpp}` | 4×4 pads, click-to-audition, drag-and-drop file → pad. |
| `tests/` | `VoicePoolTests`, `ChokeGroupTests`. |

**Accept:** click pads → instant sound; unit tests for voice stealing + choke.
✅ Done (commits `7ffbb6a`..`e16e87f`): 16-pad synth StarterKit, 64-voice pool
(steal-the-quietest + choke groups), native-rate SampleLoader + per-voice
resampling, PadGrid UI with drag-drop, keyboard + MIDI input. 52 headless test
groups; CI green on both OSes. Samples stored at native rate (Voice resamples).

---

## Phase 2 — Sequencer core ✅

Up to 16 lanes × 64 steps, base 1/16 grid, per-lane triplet + length (polyrhythms), rich per-step data, transport.

| File | Classes / responsibility |
|------|--------------------------|
| `model/Step.h` | `Step`: on/off, velocity, micro-shift (±50%), ratchet count 1–8 + ramp, probability (100/75/50/25), sample-lock index. |
| `model/Lane.h` | `Lane`: `Step[]`, length, triplet toggle, target pad. |
| `model/Pattern.{h,cpp}` | `Pattern`: lanes + BPM + swing; pure `snapshot()` for the audio thread. |
| `model/PatternBank.h` | Slots A–H, chaining/queueing. |
| `engine/Clock.{h,cpp}` | Sample-accurate scheduler → exact sample offsets per block; drift-free at any buffer size. |
| `engine/Sequencer.{h,cpp}` | Reads snapshot, emits events to `DrumEngine`; swing/probability/ratchets; bar-boundary pattern switch. |
| `ui/SequencerGrid.{h,cpp}`, `ui/StepComponent.{h,cpp}` | Grid, vertical-drag velocity, playhead (60 Hz atomic read). |
| `ui/TransportBar.{h,cpp}` | Play/stop, BPM, tap tempo, swing, metronome, pattern slots. |
| `model/UndoableActions.h` | `juce::UndoManager`-backed edits. |
| `tests/` | `ClockTimingTests` (offsets exact @ 64/256/1024), `RatchetTests`, `SwingTests`, `PatternSwitchTests`. |

**Accept:** timing tests exact across buffer sizes; ratchets even; A→B on bar boundary, glitch-free.
✅ Done (commits `234584c`..`094adce`): sample-accurate Clock (adversarially
verified — identical across 64/256/1024 buffers), Sequencer with ratchets /
probability / micro-shift / swing, A–H PatternBank with bar-boundary switch, and
the TransportBar + editable SequencerGrid UI with UndoManager. Triplet timing +
backward micro-shift deferred (see HANDOFF §5). 76 headless test groups; CI green.

---

## Phase 3 — Roll Painter + Fill Engine + Humaniser ✅ (differentiators — go deep)

| File | Classes / responsibility |
|------|--------------------------|
| `model/RollRegion.{h,cpp}` | Non-destructive roll object: start/end step, density curve, 3 mini-curves (Speed/Volume/Pitch), snap set {1/8,1/16,1/16T,1/32,1/32T,1/64,1/128}. |
| `model/RollCompiler.{h,cpp}` | Pure `compileRoll(region, bpm) -> std::vector<Event>`. Heavily unit-tested. |
| `model/RollPresets.h` | 10 presets (Trap Triplet, Buildup, Stutter, Drill Slide, Machine Gun, Fade Roll, …). |
| `model/FillEngine.{h,cpp}` | Rule-based templates per style (Trap/Drill/House/DnB/Boom-Bap/Techno/Pop); seeded RNG; intensity 1–5; writes real steps/rolls. |
| `model/Humaniser.{h,cpp}` | One-knob Robot↔Human: seeded-per-loop micro-timing/velocity/alt-sample jitter, non-destructive at playback. |
| `ui/RollBrushOverlay.{h,cpp}` | Brush mode; click-drag → one editable roll block; vertical = end-density. |
| `ui/RollInlineEditor.{h,cpp}` | Inline (not dialog) Speed/Volume/Pitch curve editors. |
| `ui/FillControls.{h,cpp}` | FILL button, Reroll dice, intensity slider, hover-hold preview; right-click "Fill this bar". |
| `tests/` | `RollCompilerTests` (event counts, monotonic times, ramp values), `FillDeterminismTests` (same seed → same output). |

**Accept:** paint a 2-beat accelerating hat roll in one gesture; 10 distinct usable trap fills in 10 clicks; deterministic compiler/fill tests.
✅ Done (commits `3bfcd2d`..`fb01993`, + Windows stack-overflow fix `f2e4ba8`):
pure `RollCompiler` (tempo-independent step offsets) + `RollRegion`; rolls carried
in the `Pattern` snapshot and fired sample-accurately by the `Sequencer` (compiled
on the message thread, no RT compilation); 10 `RollPresets`; deterministic
per-style `FillEngine` (seeded, intensity 1–5); one-knob `Humaniser` (forward
timing + velocity jitter); and the UI — `FillBar` (FILL/Reroll/Humanise) +
`RollBrushOverlay` (drag-paint) + roll-preset picker. `RollInlineEditor` (curve
editing) and alt-sample jitter deferred (see HANDOFF §5). 95 headless test groups;
CI green.

---

## Phase 4 — Macro effects ✅

Four master-bus macro knobs, per-pad SPACE sends, always-on transparent limiter. `juce::dsp`. Defaults 0 = bypass. No routing UI.

| File | Classes / responsibility |
|------|--------------------------|
| `engine/fx/Punch.{h,cpp}` | Transient emphasis + parallel compression blend. |
| `engine/fx/Space.{h,cpp}` | Pre-filtered short plate reverb (send bus). |
| `engine/fx/Crush.{h,cpp}` | Bitcrush/downsample + soft clip, dry/wet. |
| `engine/fx/Drive.{h,cpp}` | Saturation + gentle high-shelf comp. |
| `engine/fx/MasterLimiter.{h,cpp}` | Always-on transparent brickwall. |
| `engine/MasterBus.{h,cpp}` | Chains the above; per-pad send routing. |
| `ui/MacroKnobs.{h,cpp}` | Four big knobs (PUNCH/SPACE/CRUSH/DRIVE). |
| `tests/` | `FxSmokeTests` (no NaN/clip at extremes; limiter caps output). |

**Accept:** each knob sounds good full-travel; no clipping at max; CPU < 15% of one core @ 44.1k/256.
✅ Done (commits `6071e3b`..`5e393f7`, 6 commits): a `MasterBus` on the audio output
with `Punch` / `Drive` / `Crush` / `Space` macros (each 0 = bypass) + an always-on
brickwall `MasterLimiter`, and a `MacroKnobs` UI. All effects hand-rolled (no
juce::dsp) and headless-tested via `FxSmokeTests` (bypass at 0; no NaN/clip at full
travel). Per-pad SPACE sends deferred (need a send level on `Pad`). 107 headless
test groups; CI green.

---

## Phase 5 — Sample library + auto-kits ✅

| File | Classes / responsibility |
|------|--------------------------|
| `library/sqlite/sqlite3.{c,h}` | Vendored SQLite amalgamation (no external dep). |
| `library/LibraryDb.{h,cpp}` | SQLite schema + queries: path, duration, RMS, centroid, ZCR, onset count, category, confidence, favourite. |
| `library/Scanner.{h,cpp}` | Background recursive scan (`juce::ThreadPool`), feature extraction. |
| `library/FeatureExtractor.{h,cpp}` | RMS, spectral centroid, ZCR, onset count. |
| `library/Categoriser.{h,cpp}` | Filename tokens first → feature-rule fallback → {kick,snare,clap,hat-closed,hat-open,tom,perc,fx}. |
| `library/KitBuilder.{h,cpp}` | NEW KIT: coherent, level-matched random kit; per-pad locks. |
| `ui/BrowserPanel.{h,cpp}` | Category tabs, search, similarity sort, audition, drag→pad, favourites. |
| `tests/` | `CategoriserTests` (≥~85% on named files), `SimilaritySortTests`, `KitBuilderTests`. |

**Accept:** scan 5k files < 60s; categoriser ≥ ~85% on obviously named files; NEW KIT always playable.
✅ Done (commits `f2a0d7c`..`d18ec8b`, 5 commits): `FeatureExtractor` + `Categoriser`
(filename tokens → feature rules, ≥85% on named files), vendored SQLite 3.53.3 +
`LibraryDb`, `Scanner` (folder → features → DB), `KitBuilder` (NEW KIT, seeded,
per-pad locks), and a `BrowserPanel` UI (scan / filter / NEW KIT). Deferred:
background-threaded scan, audition-on-click, drag→pad, similarity sort. 115 headless
test groups; CI green (incl. SQLite under MSVC + ASan).

---

## Phase 6 — Export & interop ✅

| File | Classes / responsibility |
|------|--------------------------|
| `engine/OfflineRenderer.{h,cpp}` | Sample-accurate faster-than-realtime render. |
| `model/MidiExporter.{h,cpp}` | Pattern → MIDI (rolls/ratchets flattened; GM drum map + remap table). |
| `model/WavExporter.{h,cpp}` | Full-mix WAV + per-pad stems (pre/post master FX option). |
| `model/ProjectIO.{h,cpp}` | `.rollforge` JSON save/load; relative sample paths; "collect samples" option. |
| `ui/ExportDialog.{h,cpp}`, `ui/DragOutGrip.{h,cpp}` | Export UI; drag-out via `performExternalDragDropOfFiles`. |
| `tests/` | `MidiExportTests`, `StemNullTests` (stems sum to mix), `ProjectRoundTripTests`. |

**Accept:** exported MIDI reproduces the groove in a DAW; stems null against the mix.
✅ Done (commits `3c88551`..`5e69e57`, 5 commits): `ProjectIO` (`.rollforge` JSON
round-trip), `MidiExporter` (Pattern → GM-drum MIDI), `OfflineRenderer` (faster-
than-RT, reuses Sequencer + DrumEngine + MasterBus), `WavExporter` (mix + per-pad
stems that null against the mix), and an `ExportPanel` UI. Deferred: project
save/load UI (needs per-pad path tracking) + drag-out. 125 headless test groups;
CI green.

---

## Phase 7 — Packaging & polish ✅ (the last phase)

| File | Purpose |
|------|---------|
| `app/Autosave.{h,cpp}` | Recovery `.rollforge` written every ~60 s; restored if present at launch. |
| `app/AppSettings.{h,cpp}` | Persisted prefs (UI scale + sample folders) as JSON under app-data. |
| `app/FirstRunState.{h,cpp}` | The one-time-welcome gate: a marker file in app-data (pure, headless-tested). |
| `ui/SettingsView.{h,cpp}` | Audio device + buffer size (device selector), UI scale 100/125/150%, sample folders. |
| `ui/FirstRun.{h,cpp}` | One-time welcome overlay (dim + tips + dismiss); never a modal tutorial. |
| `packaging/linux/` | AppImage via linuxdeploy (+ its appimage plugin) + portable tar.gz; `.desktop` + SVG icon. |
| `packaging/windows/` | Inno Setup installer + portable zip; static MSVC runtime → no vcredist. |
| `.github/workflows/release.yml` | Packaging workflow: `v*` tag cuts a GitHub Release; `workflow_dispatch` smoke-tests. |

**Accept:** clean install → sound in under 3 clicks on both OSes.
✅ Done (commits `e1d0264`..`404824f`, 5 sub-phases + 1 review fixup): crash-recovery
`Autosave`, persisted `AppSettings` + a `SettingsView` (device / buffer / UI scale /
sample folders), a one-time `FirstRun` welcome overlay gated by a pure headless-tested
`FirstRunState` marker, and dual-OS packaging — a Linux AppImage + tar.gz
(`build-appimage.sh` via linuxdeploy) and a Windows Inno-Setup installer + portable zip
(`build-packages.ps1`), with the app statically linking the MSVC runtime so the Windows
artifacts need no redistributable. Packaging is driven by a new `release.yml`
(tags → a published GitHub Release; manual dispatch → an unpublished smoke build). An
adversarial multi-agent review of the changes caught one blocker (the missing
linuxdeploy appimage-output plugin), fixed in `404824f`. Deferred (documented): the
FirstRun *muted demo loop / pulsing Play / coach marks* (shipped a simpler welcome
overlay instead) and any macOS packaging (out of v1 scope). 130 headless test groups;
CI green. **RollForge is feature-complete for v1.**

---

## Post-v1 — the moat, Credibility, P2, and the big bet ✅ (2026-07-08 / 07-09)

Everything on the "Roadmap to beat Atlas" artifact is shipped except three items that were
deliberately never built (perceptual filter sliders, auto silence-trim on import, keyword
prompt-to-beat). Branch `feature/make-a-beat`, 47 commits ahead of `master`, CI green on Linux +
Windows + ASan, clean under TSan. **299 headless test groups.**

| File | Role |
|---|---|
| `model/Capture.{h,cpp}` | One quantiser for both capture sources. Rounds a hit onto **its own lane's** grid (12 steps for a triplet lane, 16 for a straight one). Rounding past the last step **wraps to 0** — nobody taps a downbeat late. |
| `library/BeatboxDetector.{h,cpp}` | Onsets → kick / snare / hat. Three classes, because a mouth makes three sounds. Reuses `Slicer` + `Categoriser`. |
| `engine/InputRecorder.{h,cpp}` | RT-safe audio-thread → message-thread handoff (`armed`/`writing` release-acquire handshake). Never allocates on the audio thread. |
| `engine/MidiCaptureQueue.{h,cpp}` | Multi-producer (one thread per MIDI device) → message thread. Bounded, drop-on-full; drops come back **with** the notes, because reading them separately loses exactly the notes dropped between the two calls. |
| `model/Song.{h,cpp}` | The arrangement chain. One question, once per bar: "which slot at bar B?" |
| `library/Similarity.{h,cpp}` | Z-scored feature space + PCA, sign-pinned so the map never renders mirrored. |
| `library/SimilarSearch.{h,cpp}` | Category gates, features order. A kick's neighbours are always kicks. |
| `library/FolderWatcher.{h,cpp}` | Auto-ingest. **The worker thread never touches the database**; it queues rows, the message thread writes them. |
| `library/SampleAnalyser.{h,cpp}` | Split out of `Scanner`: the half that has nothing to do with a DB. |
| `engine/Resample.{h,cpp}` | Folds a bounce's decay tail back over the loop start, where the next pass would have put it. |
| `ui/CoachMarks.{h,cpp}` | The guided tour. The hole is a **real** hole: `hitTest` is false inside it, so the highlighted control is live. |
| `ui/Text.h` | `utf8()`. `juce::String` decodes a `char` literal as **Latin-1**; every em-dash in the UI was mojibake without this. |
| `ui/SimilarityMap`, `ui/PatternSlots`, `ui/SongBar` | Constellation browser, A–H strip, arrangement chips. |
| `web/gen_page.py` | Generates <https://getstackbase.com/rollforge>. Its **user manual is generated from `AboutView.cpp`**, so page and app cannot drift. |

---

## Scope guard (what we are NOT building)

No plugin hosting · no melodic/pitched mode · no time-stretching · no cloud/content store ·
no ML embeddings · no macOS target (kept structurally possible) · no MIDI-controller mapping UI
(incoming MIDI notes trigger pads and are captured — that's it).

**"No audio recording" was a v1 guard and it is now deliberately broken.** The roadmap flagged
this as a scope change, not an oversight: Capture's MIC mode records the input for up to 30
seconds so it can find the hits in it. `AudioEngine` therefore opens **one input channel**, with
a fallback to output-only when there is no microphone. Nothing else records, and nothing is
written to disk without the user asking.
