# RollForge — Handoff / Continuation Guide

Operational guide for resuming work in a later session. For the full phase →
files/classes map see [`PLAN.md`](PLAN.md); for the manual test checklist see
[`TESTING.md`](TESTING.md). This file is the "how to pick up where we left off".

_Last updated: 2026-07-10 (**v0.2.2 shipped** — the two bugs v0.2.1 left open are fixed, and the
packages on the download page are built from them)._

---

## 0. Read this first (2026-07-10)

**There is nothing left to BUILD, and the two known bugs are now FIXED** (see 0a). Every tier of the
roadmap artifact is shipped: the moat (generative rhythm), Credibility (table stakes), the whole
**P2 "Later"** tier, and the **v2 big bet** (Capture: tap-to-pattern + beatbox). Three P2 items
were deliberately never built and are claimed nowhere: perceptual filter sliders, auto silence-trim
on import, and keyword prompt-to-beat (dropped on its own critique -- "a preset-picker in NL
clothing").

**`master` is the only branch.** `feature/make-a-beat` was merged in (merge commit `258d76a`,
2026-07-10) after 55 commits and three releases unmerged, and `master` is green on all three CI jobs.
The four old branches were then deleted, local and remote:

| deleted branch | tip | still reachable as |
| --- | --- | --- |
| `feature/make-a-beat` | `3e9a533` | `master` itself |
| `feature/metering-visual-feedback` | `bcd0d9a` | 2nd parent of `cb70b73` |
| `feature/quick-wins-velocity-hatchoke` | `2c1f728` | on `master`'s first-parent line |
| `fix/export-tempo-and-length` | `5f068b4` | merged by `9849501` |

Nothing was lost -- every tip is an ancestor of `master`, so `git branch <name> <sha>` restores any of
them. `git branch -d` (never `-D`) was used, so git itself refused to delete anything unmerged.

The merge is a merge commit, not a fast-forward, for one reason worth knowing: `master` carried
`cb70b73` (the PR #2 merge of `feature/metering-visual-feedback`) which the branch did not. Both of
that commit's parents were already ancestors of the branch, and its diff against its own second parent
is empty, so it contributed no content -- but it did mean `master` was not an ancestor of the branch
and a fast-forward was impossible. The merged tree is byte-identical to the branch's.

**Watch out:** local `master` was stale at `9aeb5e8` while `origin/master` was at `cb70b73`, so
`git rev-list master..<branch>` reported a clean fast-forward that did not exist. Always diff against
`origin/master` after a `git fetch`.

**CI is green on all three jobs, Windows included.** The repo was made **public** on 2026-07-09 to
unblock GitHub Actions -- a failed payment / spending limit had been failing every job in 2-4
seconds. Public repos get free unlimited Actions. **307 test groups.** MSVC gotchas that have
actually bitten: `M_PI` is POSIX and absent from MSVC's `<cmath>` (use `juce::MathConstants`), and
`juce::String` decodes a narrow `char` literal as **Latin-1**, so any non-ASCII UI literal must go
through `utf8()` (`src/ui/Text.h`).

**v0.2.2 is built and hosted** at <https://getstackbase.com/rollforge> (page public, downloads
behind HTTP basic auth; the 0.2.0 and 0.2.1 packages are still on disk, unlinked). Built by
dispatch run `29086993209` from `7f2ead9`, sha256-verified both ends, packages uploaded before the
page. **Tagging is now safe, and it did not used to be.** The repo is public, so a GitHub Release
puts its assets at unauthenticated URLs and the download password protects nothing. `release.yml`
used to publish on *any* `v*` tag, so the rule was simply "never tag". Its publish job is now gated
on `github.ref_type == 'tag' && vars.ROLLFORGE_PUBLISH_RELEASE == 'true'`, and that variable is not
set -- so a tag packages and publishes nothing. **Only set that variable if the binaries are truly
meant to be public.** To cut packages without a tag, dispatch `release.yml`. See
[`web/README.md`](web/README.md). (The old `v0.1.0` Release's four assets were public for exactly
this reason and were **deleted 2026-07-10**; the tag and the Release page remain, with 0 assets.)

Note that Actions runs the workflow file **from the ref that triggered it**, so a tag placed on a
commit with the old, ungated `release.yml` would still publish. `v0.2.2` is tagged on a commit that
contains the gate.

## 0a. v0.2.1 -- what was fixed, and the two bugs it left behind (2026-07-10)

Prompted by a user report that "the clear button does not work: click Make a Beat, then Clear or
Clear Rolls". Both buttons were firing correctly; neither could show it. Chasing that found worse.

**Fixed (commits `d146252`, `3116a0d`):**

1. **No step in the sequencer could be toggled by mouse.** `RollBrushOverlay` overrides
   `Component::hitTest`, and JUCE honours `setInterceptsMouseClicks(false)` only *inside the
   default* `hitTest` (`juce_Component.cpp:1095`). The override returned `x >= labelWidth`
   unconditionally, so it swallowed every click over the step area even with the brush off.
   Since `11b3496`. **Any `hitTest` override must re-check its own enabled flag.** The lane
   padlocks kept working (they sit left of the step area), which is what disguised it.
2. **Every offline render came out dead straight.** Swing lives on a `Sequencer` atomic, *not* in
   the `Pattern` snapshot it reads, and `OfflineRenderer` built a fresh `Sequencer` and never
   called `setSwing`. Export WAV / Stems / drag-out / Resample all lost the groove. Regression
   test: "swing is baked into the rendered audio".
3. **An immediate `setPattern` ate a bar-queued switch** without bumping `switchCount`, so
   `pendingActive` never cleared -- editing a step while EVOLVE ran (which the Help tells you to
   do) permanently wedged EVOLVE, A-H switching and Song mode until you stopped the transport.
   `pushEditPattern()` now cancels the queued switch deliberately and says so.
4. **Generated rolls were never drawn**, so `Clear Rolls` deleted something invisible. New pure
   `rollExtent()` derives the overlay from `editPattern.rolls` (a roll stores only a target *pad*,
   so the lane is looked up; the span comes from the compiled hits -- no file-format change).
5. **Nothing cleared the beat.** Added **Clear Pattern** (undoable, greys out when empty). The
   song row's `Clear` is now **Clear Song** and greys out on an empty chain.
6. Roll brush painted on the wrong lane: `laneAt` used floor division while the grid tiles rows
   with `gridSpan`'s *rounded* edges -- the exact drift `GridGeometry.h` exists to prevent.
7. Clearing a slot used `blankPattern()`, silently resetting it to 120 BPM and dropping its
   triplet lanes. New pure `clearedPattern()` keeps lanes/tempo/swing and wipes only the notes.
8. Save / Open / Export discarded their `bool` result and failed **silently**. All report now.
9. Folder scans used the default `FollowSymlinks::yes`; a symlink cycle hung the watcher thread,
   which is joined on quit. Now `noCycles`.

**Both remaining bugs are now FIXED (commits `f73c025`, `d3f699f`):**

1. **Stems now sum to the mix when choke groups are active.** `WavExporter::stemPattern` used to
   strip every other pad's lanes, so the closed hat never fired in the open-hat stem and never
   choked it -- and `FillEngine` places open hats *specifically* to be choked. A stem is now the
   **whole pattern played with one pad captured**: `OfflineRenderer::Options::capturePad` ->
   `DrumEngine::setCapturePad` -> `VoicePool::renderAdditive` -> `Voice::renderAdditive`'s
   `writeOutput` gate. `stemPattern` is deleted.

   The subtlety worth keeping: a non-captured voice must still **advance**. `getLevel()` drives
   "steal the quietest", so a voice that stopped advancing would make a stem steal differently
   from the mix. It is advanced in *closed form* (envelope is a pure function of the frame index;
   the sample read and the tone filter feed only the discarded output, and `setTone()` resets the
   filter on every note) -- which is also why stem export costs the same as before rather than
   16x: measured 242 ms vs the old 244 ms for a dense 4-bar, 16-pad export.

   Stems null against the mix to **float precision** (maxDiff < 1e-7), not just the old 0.01
   gate, under choke + rolls + reverb sends + voice stealing all at once. Three new tests in
   `StemNullTests` cover it -- the last one through the real 24-bit WAV writer, because summing
   the files is what a producer actually does. Each carries a guard that fails if it stops
   exercising what it claims: that the choke really fires, that the pool really steals, and that
   the mix WAV does not clip. A **pre-master mix has no limiter** and peaks at 1.04 at unity, so
   without that last guard a file-level null test fails for a reason unrelated to stems.

2. **`Categoriser` matches tokens on word boundaries**, so "Phat Kick" is a Kick
   (`src/library/Categoriser.cpp`). `toWords()` splits at punctuation, at letter<->digit
   boundaries and at camelCase humps, and matches each token against **two** spellings (camel-split
   and glued) so both `OpenHat` and `openhat` still resolve. Order is still load-bearing: HatOpen
   must be tested before HatClosed, because the split form of "OpenHat" contains the word "hat".
   The >=85% claim was re-checked: the corpus was grown from 16 to 30 names (adding `KickDrum`,
   `kick01`, `808kick`, `Hi-Hat_Closed`, `Phatty_Kick`, `That_Snare`, ...) and now scores **100%**.
   An all-lowercase compound (`kickdrum.wav`) is the one thing boundary matching gives up; it
   returns Unknown and `categorise()` falls through to the feature rules, which is why callers
   must use `categorise()` and not `fromFilename()`.

### The other open item

**A producer friend is testing the Windows build and will report back.** That is the first human
audio/GUI acceptance pass on Windows. Two things are compiler-verified but never human-verified,
and both are new:

1. `AudioEngine` now opens **one input channel** for beatbox capture (WASAPI on Windows). The
   output-only fallback -- no microphone, or mic privacy blocked -- has never executed anywhere.
2. Beatbox capture was verified by piping synthesised audio through a PulseAudio null sink. A
   real mouth, a real microphone and room noise are a different test; the classifier is coarse
   by design (three classes, because a mouth makes three sounds).

### How to verify anything here

The laptop is a real X11 desktop, so the app can be **driven and screenshotted**: `xdotool` to
click, `ffmpeg -f x11grab` to capture. Build with `build-local`, never the stale `build/`:

```
cmake --build build-local -j8
./build-local/tests/RollForgeTests_artefacts/Release/RollForgeTests    # 307 test groups
```

Sanitizers, both of which have earned their keep:

```
cmake --build build-asan2 --target RollForgeTests && ./build-asan2/.../RollForgeTests
cmake -B build-tsan -DROLLFORGE_TSAN=ON && setarch $(uname -m) -R ./build-tsan/.../RollForgeTests
```

The `setarch -R` is **not optional** -- TSan's shadow mapping collides with modern kernel ASLR
and the binary aborts before a single test runs. TSan is what makes `InputRecorder`,
`MidiCaptureQueue` and `FolderWatcher` believable; ASan cannot see a data race.

Real-signal testing that paid off, and is repeatable:
- **Audio in**: `pactl load-module module-null-sink sink_name=rfbeatbox`, set it as the default
  source, `paplay -d rfbeatbox take.wav`. Restore the default source afterwards.
- **MIDI in**: the app subscribes to ALSA's `Midi Through` port, so `aplaymidi -p 14:0 taps.mid`
  plays notes straight into it.

### Bugs found by this work, worth remembering as a class

- `Project` grew to **473 KB** and blew the stack (caught by ASan; Windows' 1 MB thread stack
  would have followed). A `static_assert` now guards its size.
- The **CLIP lamp could never light**: the limiter hard-clamps at 0.98 and the meter was fed
  after it. It now watches the limiter's *input*.
- Settings' "Add folder" button wrote to `AppSettings::sampleFolders`, which **nothing ever
  read**. It showed a count that meant nothing and scanned nothing. Removed; orphaned folders
  are migrated into the real watch list once, on launch.
- `juce::String` decodes a `char` literal as **Latin-1**, so every em-dash in the UI rendered as
  mojibake. Use `utf8()` (`src/ui/Text.h`).
- `Slicer` snaps its first onset to sample 0 -- correct for tiling slices, wrong for anything
  that cares *when* a hit happened. Opt out with `snapFirstToZero = false`.
- A 30-agent audit of the in-app Help found **three claims that were simply false**. The Help now
  also generates the public user manual, so stale Help is stale documentation. See
  `web/gen_page.py`.

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
