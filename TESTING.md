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
- ☐ `ctest --test-dir build --output-on-failure` → smoke test passes.
- ☐ CI green on `ubuntu-22.04` and `windows-latest`.
- ☐ ASan/UBSan job green on Linux.

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
_(to be filled when the phase lands)_

## Phase 2 — Sequencer core
_(to be filled)_

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
