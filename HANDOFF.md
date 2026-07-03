# RollForge — Handoff / Continuation Guide

Operational guide for resuming work in a later session. For the full phase →
files/classes map see [`PLAN.md`](PLAN.md); for the manual test checklist see
[`TESTING.md`](TESTING.md). This file is the "how to pick up where we left off".

_Last updated: 2026-07-03 (end of Phase 0)._

---

## 1. Where we are

- **Phase 0 (skeleton) is complete and committed.** Two commits on `master`:
  - `ac122ac` — repo skeleton, CMake + JUCE 8.0.8, audio engine blip, UI shell, CI, docs.
  - `d11f4d9` — run only the `rollforge` test category (fixes ASan/UBSan CI).
- **Verified locally:** configures + builds clean (GUI app + tests), Release
  tests pass, ASan/UBSan run is leak-clean. Passed a 4-lens adversarial review
  (findings verified against the pinned JUCE source; all confirmed ones fixed).
- **NOT yet verified:** GitHub Actions CI has not run on either OS (see §5),
  and the blip has not been *heard* (headless machine — audio path builds/links
  and the callback is RT-safe, but audible output needs a human at the machine).

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

## 4. Immediate next actions (Phase 1 — Pads + playback engine)

Goal: 16 pads (4×4), 64-voice polyphonic pool, per-pad params, sample loading,
a code-synthesised starter kit. Acceptance: click a pad → instant sound; unit
tests for voice stealing + choke groups.

Suggested build order (each a commit; keep the model pure/unit-tested):

1. **`model/Pad.h` + `model/Kit.h`** — pure data: sample ref, volume, pan, pitch
   (±12 st), attack/release, choke group, reverse, up-to-4 sample alternates.
   No JUCE GUI.
2. **`engine/SampleBuffer`** — owns loaded, device-rate-resampled audio +
   refcount. **`library/SampleLoader`** — `AudioFormatManager` for
   WAV/AIFF/FLAC/MP3/OGG; resample on load. (Add `juce_audio_formats` is already
   pulled via `juce_audio_utils`.)
3. **`engine/Voice` + `engine/VoicePool`** — 64 voices, resampled playback,
   ADSR-ish env, pan, reverse; **voice stealing** (oldest/quietest) and
   **choke groups** (open/closed hat). Unit-test both hard (deterministic).
4. **`engine/DrumEngine`** — trigger routing pad→voice; owns the pool; replaces
   the Phase-0 blip in `AudioEngine`'s callback (keep the RT-safety rules:
   command FIFO from UI, no alloc/lock on the audio thread).
5. **`library/StarterKit`** — synthesise 16 clean sounds in code at first run
   (kick = pitched sine + click, snare = noise+tone, hats = filtered noise
   bursts of varying decay, …). Repo ships NO binary assets.
6. **`ui/PadGrid` + `ui/PadComponent`** — 4×4 pads, click-to-audition,
   drag-and-drop audio file → pad. Dark theme, ≥28px hit targets.
7. **Wire keyboard:** keys `1`–`8` (and beyond) trigger pads; keep space for
   play/stop later. Accept incoming MIDI notes → pad triggers (no mapping UI).
8. **Tests:** `VoicePoolTests` (stealing), `ChokeGroupTests` — all under the
   `rollforge` category (the runner only executes that category).

Introduce `juce::UndoManager` from Phase 2 onward (per spec), not Phase 1.

## 5. Open items / risks

- **CI unproven until pushed.** The workflow (`.github/workflows/ci.yml`) builds
  on `ubuntu-22.04` + `windows-latest` + a Linux ASan job. The Linux + ASan
  commands were run locally and pass; the **Windows leg has never executed** (no
  Windows machine). Watch the first Actions run — likely Windows-specific
  wrinkles: MSVC+Ninja via `ilammy/msvc-dev-cmd`, `choco install ninja`, and
  JUCE fetch time. If the JUCE clone is slow, add an `actions/cache` step for
  `build/_deps`.
- **Blip audibility** is a human check (see TESTING.md manual steps).
- `packaging/ci/` is an empty dir (real packaging = Phase 7).

## 6. Key decisions & rationale

- **Standalone-only, engine/UI decoupled** — `src/engine` and `src/model` must
  never include a JUCE GUI module, so a VST3 wrapper can reuse them later.
- **JSON for projects/kits/patterns; SQLite (vendored amalgamation) for the
  sample library index** (SQLite arrives in Phase 5).
- **RT-safety is non-negotiable:** UI↔engine via lock-free FIFO + atomics;
  pattern data read via double-buffered snapshot swap; 60 Hz UI timer reads
  atomics (never full-window repaints per frame).
- **Test runner runs only the `rollforge` category** — never JUCE's internal
  suite (faster; avoids unrelated UBSan noise). All RollForge tests must
  register under that category.
- **GPLv3 build** (JUCE GPL option). A JUCE commercial license is required
  before any closed-source distribution — see `README.md` / `packaging/README.md`.
