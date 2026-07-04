# RollForge — Handoff / Continuation Guide

Operational guide for resuming work in a later session. For the full phase →
files/classes map see [`PLAN.md`](PLAN.md); for the manual test checklist see
[`TESTING.md`](TESTING.md). This file is the "how to pick up where we left off".

_Last updated: 2026-07-04 (Phase 0 CI green on both OSes)._

---

## 1. Where we are

- **Phase 0 (skeleton) is complete, committed, and pushed.** Four commits on
  `master`, HEAD = `7ddd435`:
  - `ac122ac` — repo skeleton, CMake + JUCE 8.0.8, audio engine blip, UI shell, CI, docs.
  - `d11f4d9` — run only the `rollforge` test category (fixes ASan/UBSan CI).
  - `8de94ea` — add this HANDOFF.md.
  - `7ddd435` — gate audio backends per platform (fixes Windows CI).
- **Verified locally:** configures + builds clean (GUI app + tests), Release
  tests pass, ASan/UBSan run is leak-clean. Passed a 4-lens adversarial review
  (findings verified against the pinned JUCE source; all confirmed ones fixed).
  Re-audited 2026-07-04 against pinned JUCE 8.0.8 — RT contract, engine/UI
  layering, and the async-settings-dialog lifetime all confirmed clean; no
  blockers or correctness bugs (two minor follow-ups landed in §5).
- **Verified in CI:** GitHub Actions is green on **both OSes** — Linux build,
  Windows build, and the Linux ASan/UBSan job all passed on `7ddd435` (full run
  SUCCESS in ~9m48s). The first run (on `8de94ea`) failed on Windows with a
  `jack/jack.h` error; `7ddd435` fixed it. See §5.
- **Still NOT verified:** the blip has not been *heard*. The build machine was
  headless, so the audio path builds/links and the callback is RT-safe, but
  audible output needs a human at a machine with audio (see TESTING.md).

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

**Recommended first commit** (from the 2026-07-04 readiness review — pulls the
riskiest RT decision forward, ahead of the original "start with `Pad.h`"):
`engine/SampleBuffer` as an immutable-once-loaded `juce::ReferenceCountedObject`
**plus its RT-safe reclamation scheme**, with a headless unit test and a written
ownership contract in the header. Sample lifetime is the single most load-bearing
decision in Phase 1 — every downstream file is shaped by it, and getting it wrong
is a use-after-free, not a compile error. It's fully headless-testable (no audio
device), so it fits CI.

**Ownership / reclamation contract.** Voices hold a strong
`ReferenceCountedObjectPtr` for the whole note (incref at note-on is RT-safe).
The hazard is the *free*: a voice dropping the last ref would run `delete` on the
audio thread (with `JUCE_STRICT_REFCOUNTEDPOINTER=1` that is the silent default).
Fix: a message-thread-owned `ReferenceCountedArray` is the sole retirement owner;
a 1–2 s timer frees a retired buffer only once its refcount hits 1 (no voice
references it). Pad edits / NEW KIT / device-SR rebuilds swap the pointer via the
command FIFO and move the old buffer to the retirement set; live voices keep
playing it until they end.

**Decisions to pin before coding (the plan leaves these open):**
1. **Buffer storage rate** — native-rate + resample per-voice (robust to a
   runtime device-SR change; the Voice already needs an interpolator for ±12 st)
   vs device-rate + resample-on-load (must rebuild *every* buffer when the device
   SR changes in the settings dialog). Recommend native-rate. Decide before the
   Voice read loop.
2. **Voice-steal order + tie-break** — a pure function of testable state: prefer
   released/choking → lowest envelope gain → oldest (a monotonic trigger counter,
   *not* wall-clock); tie-break on lowest voice index. "Quietest" = tracked
   envelope gain, never live audio RMS (keeps tests deterministic + headless).
3. **Choke semantics** — group 0 = no choke; choke fires at trigger, before
   allocation; same-group voices get a ~3–5 ms declick release (not a hard cut);
   a choked voice is *logically* dead immediately (out of age/steal bookkeeping)
   while it finishes the ramp. Does closed-hat choke both open and pedal hats?
4. **Same-pad retrigger** — polyphonic-per-pad (new voice, steal at 64) vs
   monophonic (a new hit steals its own previous voice). Drum machines default
   polyphonic except hats. Changes both steal and choke test expectations.
5. **Fixed MIDI-note → pad map + velocity → gain curve**, and the exact 16-key
   keyboard layout (e.g. `1234`/`QWER`/`ASDF`/`ZXCV` mirroring the 4×4 grid).
   "No mapping UI" still needs a chosen default.
6. **Envelope** — one-shot AR (play to end, ignore note-off; typical for drums)
   vs gated ADSR. Decide whether note-off does anything at all in Phase 1.
7. **Starter kit** — regenerate the 16 synthesised sounds each launch vs cache to
   disk (repo ships no binary assets either way). Any length/RAM cap on dropped
   files, or full in-RAM decode?

**RT-safety guardrails carried from Phase 0:**
- **Two producers → two FIFOs.** `juce::AbstractFifo` is single-producer /
  single-consumer. Keyboard/UI commands arrive on the message thread; incoming
  MIDI arrives on JUCE's own MIDI-input thread — a *second* producer. Design one
  SPSC FIFO **per producer thread** from the first engine commit; the callback
  drains both at block start. Do not bolt MIDI onto the UI FIFO later.
- **Command payloads are trivially-copyable POD** (enum tag + pad / velocity /
  offset / raw `SampleBuffer*`). No strings, no smart pointers, no heap in the
  ring. `static_assert(std::is_trivially_copyable_v<Command>)`.
- **Preallocate everything up front** — 64 voices + per-voice resampler state +
  both FIFOs in the constructor / `prepare()`. `trigger()` / `process()` allocate
  nothing.
- **Keep `DrumEngine` free of any `AudioDeviceManager` dependency.** `AudioEngine`
  owns a `DrumEngine` member and forwards `prepare(sr, block)` from
  `audioDeviceAboutToStart` + `process(buffer, n)` from the callback (replacing
  the blip). This is what lets `VoicePoolTests` / `ChokeGroupTests` run headless.
- When you add engine/model files, **extract a STATIC engine lib** that links
  only audio modules — mechanically enforces "no JUCE-GUI includes under
  `src/engine`" (today it's convention-only) and lets the tests link the engine.

**Revised build order** (each a commit; keep the model pure + unit-tested):

1. `engine/SampleBuffer` + refcount + message-thread retirement/sweep + headless
   test [the recommended first commit above].
2. `model/Pad.h` + `model/Kit.h` — trivial POD (sample ref, volume, pan, pitch
   ±12 st, attack/release, choke group, reverse, ≤4 alternates). Grow fields as
   later steps need them. No JUCE GUI.
3. Command FIFO(s) + `engine/DrumEngine` seam replacing the blip in `AudioEngine`,
   driven by a trivial synth buffer, with a headless "push trigger → assert
   non-silent output" test.
4. `engine/Voice` — single-voice interpolated playback, AR envelope, pitch, pan,
   reverse; unit-tested.
5. `engine/VoicePool` — 64 voices, voice stealing + choke groups →
   `VoicePoolTests`, `ChokeGroupTests` (all headless, under the `rollforge`
   category — the runner only executes that category).
6. `library/SampleLoader` — `AudioFormatManager` (WAV/AIFF/FLAC/MP3/OGG) on a
   background thread; installs via the retirement set + command FIFO. (Link
   `juce_audio_formats` — already transitively present via `juce_audio_utils`.)
7. `library/StarterKit` — synthesise 16 clean sounds on the message thread at
   startup (kick = pitched sine + click, snare = noise+tone, hats = filtered
   noise bursts of varying decay, …). Repo ships NO binary assets.
8. `ui/PadGrid` + `ui/PadComponent` — 4×4 pads, click-to-audition, background
   drag-and-drop audio file → pad. Dark theme, ≥28 px hit targets.
9. Keyboard producer (message-thread FIFO) + MIDI producer (its **own** SPSC
   FIFO). Keys trigger pads (keep space for play/stop later); incoming MIDI notes
   → pad triggers (no mapping UI).

Introduce `juce::UndoManager` from Phase 2 onward (per spec), not Phase 1.

## 5. Open items / risks

- **CI proven on both OSes.** `.github/workflows/ci.yml` (ubuntu-22.04 +
  windows-latest + a Linux ASan/UBSan job) is green on `7ddd435` — full run
  SUCCESS in ~9m48s. History: the first run (on `8de94ea`) failed on Windows with
  a `jack/jack.h` error; `7ddd435` fixed it by gating audio backends per platform.
  Optional future optimisation: an `actions/cache` step for `build/_deps` if the
  JUCE clone time grows.
- **Only remaining open verification: blip audibility** — a human check on a
  machine with audio (see TESTING.md manual steps). Everything else in Phase 0 is
  verified (local build + CI + adversarial re-audit).
- **Engine has no build boundary or test coverage yet.** `AudioEngine.cpp` is
  compiled straight into the GUI-app target, so the "no JUCE-GUI includes under
  `src/engine`" rule is convention-only — an accidental GUI include would still
  link, and the engine is never built in isolation. When Phase 1 adds
  `DrumEngine`/`VoicePool`, extract a STATIC/INTERFACE engine lib that links only
  audio modules (enforces the rule + lets the headless voice/choke tests link the
  engine). _(Re-audit 2026-07-04, minor.)_
- **Single-instance behaviour undecided.** `Main.cpp` allows multiple instances
  while each opens the default output device; a 2nd instance can silently fail to
  open it (exclusive-mode WASAPI / some ALSA configs). Make a deliberate call
  before shipping. _(Re-audit 2026-07-04, nit.)_
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
