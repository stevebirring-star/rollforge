# RollForge — Manual Test Checklist

Automated tests live in `tests/` and run in CI on Linux + Windows (plus an
ASan/UBSan job). This file tracks **manual** acceptance checks per phase.

Legend: ☐ untested · ☑ passing.

---

## Phase 0 — Skeleton

**Build (Linux)**
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

**Automated**
- ☑ `ctest --test-dir build --output-on-failure` → smoke test passes _(verified locally, Release)_.
- ☑ ASan/UBSan run green + leak-clean on Linux _(verified locally: `-DROLLFORGE_ASAN=ON -DROLLFORGE_BUILD_APP=OFF`)_.
- ☑ CI green on `ubuntu-22.04` and `windows-latest` (+ Linux ASan/UBSan) _(verified on `7ddd435`, GitHub Actions run SUCCESS in ~9m48s)_.

> The test runner executes only the `rollforge` category, not JUCE's internal
> unit-test suite (faster, and avoids unrelated UBSan noise from JUCE's tests).

**Manual**
- ☐ App launches; a dark 720×420 window titled *RollForge* appears.
- ☐ Status line shows the opened audio device (name / sample rate / buffer).
- ☐ Click **Play Blip** → a short ~880 Hz sine blip is audible.
- ☐ Press the **space bar** (window focused) → same blip.
- ☐ Click **Audio Settings** → device selector opens; changing the output
  device updates the status line and the blip still plays.
- ☐ Close the settings dialog (X or Esc) → app still responsive; reopening works.
- ☐ Resize the window → layout reflows, no clipping; window respects min size.
- ☐ Quit via the window close button → clean exit, no crash/hang.
- ☐ Launch with **no audio device** available → app still opens, status line
  says no device, no crash.

**Known limitations (Phase 0)**
- No pads/sequencer yet — the blip is the only sound.
- The blip is a fixed one-shot; velocity/pitch controls arrive in Phase 1.

---

## Phase 1 — Pads + playback engine

**Automated (headless):** `tests/headless-compile.sh` → 52 test groups pass
(SampleBuffer + retirement, Pad/Kit, CommandQueue, DrumEngine, Voice, VoicePool,
choke, SampleLoader, StarterKit, KitInstaller, PadMapping). CI builds the full
app and runs these on Linux + Windows + ASan/UBSan.

**Manual (needs a machine with audio + a display):**
- ☐ App launches; a 4×4 pad grid appears, pads labelled (kick, snare, hats, …).
- ☐ Click a pad → its sound plays immediately; the pad flashes.
- ☐ Keys `1234`/`qwer`/`asdf`/`zxcv` trigger the matching pads (grid-mirrored).
- ☐ Rapid repeated hits stay polyphonic (no premature cut-offs) up to ~64 voices.
- ☐ Trigger the closed hat while the open hat is ringing → the open hat is choked.
- ☐ Drag a WAV/AIFF/FLAC/OGG file onto a pad → it loads (label updates) and that
  pad plays the file; other pads unchanged; no glitch if a voice was mid-play.
- ☐ Connect a MIDI keyboard → notes 36–51 trigger pads 0–15 with velocity.
- ☐ Audio Settings → change device / sample rate → pads still play at correct
  pitch (samples are native-rate + resampled per-voice).
- ☐ Quit → clean exit, no crash/hang (ASan-clean; no leaked sample buffers).

## Phase 2 — Sequencer core

**Automated (headless):** `tests/headless-compile.sh` -> 76 test groups
(PatternModel + TripleBuffer, ClockTiming, Sequencer, PatternSwitch). CI builds
the full app and runs these on Linux + Windows + ASan/UBSan. The Clock timing was
additionally verified by a 3-lens adversarial pass.

**Manual (needs a machine with audio + a display):**
- ☐ The window shows a transport bar + an 8-lane × 16-step grid + the pad grid.
- ☐ Click steps to toggle them; vertical-drag a step to set velocity (top = loud).
- ☐ Press Play → the pattern loops; a playhead column sweeps left to right.
- ☐ Change BPM (slider) or Tap Tempo → the loop speed follows.
- ☐ Raise Swing → off-beat steps shuffle later; 0 = straight.
- ☐ Ratchets / probability / micro-shift sound right — a ratcheted step machine-
  guns; low probability drops hits; forward micro-shift nudges a step late.
- ☐ Cmd/Ctrl+Z undoes a step edit; Shift+Z (or Ctrl+Y) redoes; the grid updates.
- ☐ Stop → playback halts (voices ring out); Play restarts from the top.
- ☐ Quit → clean exit, no crash/hang (ASan-clean).

## Phase 3 — Roll Painter + Fill Engine
_(to be filled)_

## Phase 4 — Macro effects
_(to be filled)_

## Phase 5 — Sample library + auto-kits
_(to be filled)_

## Phase 6 — Export & interop
_(to be filled)_

## Phase 7 — Packaging & polish
_(to be filled)_
