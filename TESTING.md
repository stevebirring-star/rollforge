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

## Phase 3 — Roll Painter + Fill Engine + Humaniser

**Automated (headless):** `tests/headless-compile.sh` -> 95 test groups, adding
RollCompiler (event spacing / acceleration / velocity + pitch ramps / determinism),
RollSequencing (a compiled roll fires through the Sequencer, sample-accurate),
RollPresets (10 shapes differ in character), FillEngine (same seed -> identical
fill; intensity/style vary), and Humaniser (robot is a no-op; jitter forward /
bounded / deterministic; hit count preserved). CI builds the full app on
Linux + Windows + ASan/UBSan.

**Manual (needs a machine with audio + a display):**
- ☐ FILL: pick a style + intensity, press FILL -> the grid fills with a fitting
  beat + fill; press it again / Reroll -> a different variation; Play sounds good.
- ☐ Humanise: raise the knob -> timing/velocity loosen (less machine-like); 0 =
  tight. It never changes which hits play, only their feel.
- ☐ Roll brush: toggle "Roll Brush" on, drag across a lane -> an accelerating roll
  block appears (drag up = denser) and plays; toggle off -> normal step editing.
- ☐ Roll preset picker: choose a preset (e.g. Machine Gun, Drill Slide) then paint
  -> that shape is used instead of the auto density curve.
- ☐ Clear Rolls removes all painted rolls; a FILL also resets them.
- ☐ Quit -> clean exit, no crash/hang (ASan-clean).

## Phase 4 — Macro effects

**Automated (headless):** `tests/headless-compile.sh` -> 107 test groups, adding
FxSmoke: the limiter caps loud + extreme input to its ceiling with no NaN/inf and
passes quiet signal ~unchanged; each macro (Drive / Crush / Punch / Space) is an
exact bypass at 0 and shapes the signal at full travel with no NaN/blow-up (Space
grows an audible reverb tail). CI builds the full app on Linux + Windows + ASan.

**Manual (needs a machine with audio + a display):**
- ☐ Four macro knobs (PUNCH / SPACE / CRUSH / DRIVE) show below the sequencer grid,
  all at 0 (clean) by default.
- ☐ DRIVE up -> grittier / louder saturation; CRUSH up -> lo-fi bitcrush + aliasing;
  PUNCH up -> snappier attacks; SPACE up -> reverb tail.
- ☐ All knobs at max -> still no digital clipping (the limiter holds the ceiling);
  no crackle/dropouts (CPU stays low).
- ☐ Knobs at 0 -> output is clean (bypass) and identical to Phase-3 behaviour.
- ☐ Quit -> clean exit, no crash/hang (ASan-clean).

## Phase 5 — Sample library + auto-kits

**Automated (headless):** `tests/headless-compile.sh` -> 115 test groups, adding
Categoriser (filename tokens ≥85% on named files + feature fallback), LibraryDb
(in-memory round-trip, upsert-by-path, category/favourites filters), Scanner
(writes temp WAVs -> scans -> DB populated), and KitBuilder (per-category selection,
deterministic per seed, locks respected). CI builds the full app (incl. the
vendored SQLite) on Linux + Windows + ASan.

**Manual (needs a machine with audio + a display):**
- ☐ Click "Library" -> the browser window opens.
- ☐ "Scan Folder..." -> pick a folder of samples -> the status shows a count and
  the list fills; the category filter narrows it (Kick / Snare / Hat / …).
- ☐ Categories look right for obviously-named files (a "Kick_*.wav" lands in Kick).
- ☐ "NEW KIT" -> the 16 pads load a coherent kit (kick/snare/hats/… by pad); the
  pad labels update; playing a pattern uses the new sounds. Repeat -> a different
  kit each time.
- ☐ The library persists across app restarts (DB under the user app-data dir).
- ☐ Quit -> clean exit, no crash/hang.

## Phase 6 — Export & interop

**Automated (headless):** `tests/headless-compile.sh` -> 125 test groups, adding
ProjectIO (`.rollforge` JSON round-trip; malformed input fails gracefully),
MidiExport (GM notes; ratchets flatten; a saved `.mid` reads back), OfflineRender
(a kit+pattern renders non-silent audio; empty -> silence), and StemNull (per-pad
stems sum to the full mix with master FX off; a WAV per active pad is written).

**Manual (needs a machine with audio + a display):**
- ☐ Click "Export" -> the dialog shows Export MIDI / WAV (mix) / Stems.
- ☐ Export MIDI -> the `.mid` opens in a DAW and plays the pattern's groove on a
  GM drum map.
- ☐ Export WAV (mix) -> the file plays back the pattern (with the current macro-FX
  applied) and matches what you hear.
- ☐ Export Stems -> a folder of `pad_NN.wav` files; summing them equals the mix
  (master FX off).
- ☐ Exporting does not interrupt live playback (renders on a separate engine).
- ☐ Quit -> clean exit, no crash/hang.

## Phase 7 — Packaging & polish

**Automated (headless):** `tests/headless-compile.sh` -> 130 test groups, adding
Autosave (recovery file save / detect / load / clear round-trip; a missing or empty
file is not a recovery), AppSettings (UI scale + sample-folders JSON + file
round-trip; defaults for a missing file), and FirstRunState (the welcome is offered
until the marker is written, then never again; marking is idempotent; clearing
re-arms it). CI builds the full app on Linux + Windows + ASan.

**Packaging (CI — `.github/workflows/release.yml`):** the packaging is produced by a
separate workflow, not the per-push CI. Trigger it with **workflow_dispatch** (a
smoke build that uploads artifacts but publishes nothing) or by pushing a **`v*`
tag** (which also cuts a GitHub Release).
- ☐ `release.yml` Linux job builds the Release app on `ubuntu-22.04`, and
  `build-appimage.sh` produces `RollForge-<ver>-x86_64.AppImage` + a
  `RollForge-<ver>-x86_64.tar.gz` (both uploaded as the `rollforge-linux` artifact).
- ☐ `release.yml` Windows job builds on `windows-latest`, and `build-packages.ps1`
  produces `RollForge-<ver>-setup.exe` (Inno Setup) + `RollForge-<ver>-windows-x64.zip`
  (portable), uploaded as `rollforge-windows`.
- ☐ On a `v*` tag, the `release` job gathers both platforms' artifacts into one
  published GitHub Release with generated notes.

**Manual (needs a machine with audio + a display — the acceptance test):**
- ☐ **Linux AppImage:** `chmod +x RollForge-*.AppImage && ./RollForge-*.AppImage`
  on a clean box (no dev libs installed) -> the app launches and pads make sound in
  ≤ 3 clicks. The portable tar.gz extracts and runs via `./AppRun`.
- ☐ **Windows installer:** run `RollForge-*-setup.exe` -> installs to Program Files,
  Start-menu (and optional desktop) shortcut works, app launches and makes sound; the
  portable `.zip` runs `RollForge.exe` directly with **no** Visual C++ redistributable
  installed (static MSVC runtime).
- ☐ **First run:** on the very first launch the welcome overlay dims the app and
  shows tips; "Let's go" dismisses it and it never reappears (a `welcome.done` marker
  is written under the user app-data dir).
- ☐ **Settings:** open Settings -> change the audio device / buffer size (selector),
  the UI scale (100/125/150 % applies immediately and after a restart), and add a
  sample folder (persists to `settings.json`).
- ☐ **Autosave recovery:** with unsaved work, kill the app (don't quit cleanly) ->
  relaunch -> the previous pattern + macro-FX are restored from the recovery file. A
  clean quit clears the recovery file so the next launch starts fresh.
- ☐ Quit -> clean exit, no crash/hang (ASan-clean).
