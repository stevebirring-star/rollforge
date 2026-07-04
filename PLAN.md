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

## Phase 1 — Pads + playback engine ⬜

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

---

## Phase 2 — Sequencer core ⬜

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

---

## Phase 3 — Roll Painter + Fill Engine + Humaniser ⬜ (differentiators — go deep)

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

---

## Phase 4 — Macro effects ⬜

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

---

## Phase 5 — Sample library + auto-kits ⬜

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

---

## Phase 6 — Export & interop ⬜

| File | Classes / responsibility |
|------|--------------------------|
| `engine/OfflineRenderer.{h,cpp}` | Sample-accurate faster-than-realtime render. |
| `model/MidiExporter.{h,cpp}` | Pattern → MIDI (rolls/ratchets flattened; GM drum map + remap table). |
| `model/WavExporter.{h,cpp}` | Full-mix WAV + per-pad stems (pre/post master FX option). |
| `model/ProjectIO.{h,cpp}` | `.rollforge` JSON save/load; relative sample paths; "collect samples" option. |
| `ui/ExportDialog.{h,cpp}`, `ui/DragOutGrip.{h,cpp}` | Export UI; drag-out via `performExternalDragDropOfFiles`. |
| `tests/` | `MidiExportTests`, `StemNullTests` (stems sum to mix), `ProjectRoundTripTests`. |

**Accept:** exported MIDI reproduces the groove in a DAW; stems null against the mix.

---

## Phase 7 — Packaging & polish ⬜

| File | Purpose |
|------|---------|
| `packaging/linux/` | AppImage via linuxdeploy (CI) + plain tar.gz; X11/XWayland test notes. |
| `packaging/windows/` | Inno Setup installer (CI) + portable zip. |
| `ui/FirstRun.{h,cpp}` | Starter kit + muted demo loop, pulsing Play, 3 coach marks (never a modal tutorial). |
| `ui/SettingsView.{h,cpp}` | Device, buffer size, theme scale 125/150%, sample folders. |
| `app/Autosave.{h,cpp}` | Recovery file every 60 s. |

**Accept:** clean install → sound in under 3 clicks on both OSes.

---

## Scope guard (NOT building in v1)

No plugin hosting · no melodic/pitched mode · no audio recording · no
time-stretching · no cloud/content store · no ML embeddings · no macOS target
(kept structurally possible) · no MIDI-controller mapping UI (incoming MIDI
notes trigger pads — that's it).
