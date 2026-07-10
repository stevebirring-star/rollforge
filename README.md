# RollForge

A standalone desktop **drum machine and groove sketchpad** for Linux and
Windows. RollForge is built around one idea: *the fastest, least technical way
to make hi-hat rolls, drum fills and finished-sounding beats.* Paint a roll,
click FILL, turn one knob — no manual required.

> **Status:** **v1 feature-complete** (Phase 7 — packaging & polish — done). A 16-pad
> sampler, a sample-accurate step sequencer, the roll/fill/humanise differentiators,
> master macro FX, a vendored-SQLite sample library with auto-kits, export to MIDI /
> WAV / per-pad stems (plus `.rollforge` project I/O), crash-recovery autosave, a
> settings view, a one-time welcome, and dual-OS packaging (Linux AppImage + tar.gz,
> Windows installer + portable zip); CI green on Linux + Windows. See [`PLAN.md`](PLAN.md)
> for the full phase roadmap and [`TESTING.md`](TESTING.md) for the manual checklist.

---

## Building

### Requirements

- CMake ≥ 3.22 and a C++20 compiler (GCC 11+, Clang 14+, or MSVC 2022).
- [Ninja](https://ninja-build.org/) (recommended generator).
- JUCE is fetched automatically by CMake (`FetchContent`, pinned to tag
  **8.0.8**) — no manual install.

### Linux

Install the JUCE GUI dependencies once. RollForge builds with `JUCE_WEB_BROWSER=0`
and `JUCE_USE_CURL=0`, so **webkit2gtk, gtk and libcurl are not required**:

```bash
sudo apt-get update
sudo apt-get install -y ninja-build \
  libasound2-dev libjack-jackd2-dev \
  libfreetype-dev libfontconfig1-dev \
  libx11-dev libxext-dev libxinerama-dev libxrandr-dev libxcursor-dev
```

Then build (out-of-source, Ninja):

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/RollForge_artefacts/Release/RollForge      # run the app
ctest --test-dir build --output-on-failure          # run the tests
```

> **Faster test-only builds:** `-DROLLFORGE_BUILD_APP=OFF` skips the GUI app
> target and builds just the unit tests:
> `cmake -B build -G Ninja -DROLLFORGE_BUILD_APP=OFF && cmake --build build && ctest --test-dir build`.
> The GUI dev packages listed above are **still required** — JUCE's `juceaide`
> build tool compiles `juce_gui_basics` at configure time regardless.

### Windows

With Visual Studio 2022 (MSVC) and Ninja on `PATH` (e.g. from a *Developer
Command Prompt*):

```bat
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

(You can also use `-G "Visual Studio 17 2022" -A x64` and build with
`--config Release`.)

---

## Audio backends

| OS      | Backends                                             |
|---------|------------------------------------------------------|
| Linux   | **ALSA** and **JACK** (both enabled).                |
| Windows | **WASAPI** (default) and **DirectSound** (fallback). |

**PipeWire** users: RollForge does not talk to PipeWire directly. Use
PipeWire's ALSA (`pipewire-alsa`) or JACK (`pipewire-jack` / `pw-jack`)
compatibility layers — both appear to RollForge as ordinary ALSA/JACK devices.

**ASIO** (Windows) is intentionally excluded for now due to Steinberg SDK
licensing; a clearly-marked stub is documented in
[`packaging/README.md`](packaging/README.md) for a future opt-in build.

---

## Downloads & packaging

Prebuilt packages are attached to each tagged **[GitHub Release](https://github.com/stevebirring-star/rollforge/releases)**:

| OS      | Installer                     | Portable                          |
|---------|-------------------------------|-----------------------------------|
| Linux   | `RollForge-<ver>-x86_64.AppImage` | `RollForge-<ver>-x86_64.tar.gz` (extract → `./AppRun`) |
| Windows | `RollForge-<ver>-setup.exe` (Inno Setup) | `RollForge-<ver>-windows-x64.zip` (self-contained `.exe`) |

The AppImage bundles the app's libraries (runs on a clean box, no dev packages
needed); the Windows `.exe` statically links the MSVC runtime, so the installer and
the portable zip need **no** Visual C++ redistributable.

Packages are built by [`.github/workflows/release.yml`](.github/workflows/release.yml):
pushing a `v*` tag builds the Release app on both OSes, packages it, and publishes a
GitHub Release; a manual **workflow_dispatch** run produces the same artifacts without
publishing (a packaging smoke test). To build them yourself after a Release build:

```bash
# Linux (needs curl + the app already built in build/)
VERSION=0.1.0 packaging/linux/build-appimage.sh          # -> dist/*.AppImage + *.tar.gz
```
```powershell
# Windows (needs Inno Setup 6 + the app already built in build\)
packaging\windows\build-packages.ps1 -Version 0.1.0      # -> dist\*-setup.exe + *.zip
```

---

## Known issues

None currently tracked. The two long-standing ones were fixed after v0.2.1:

- Exported stems now sum to the exported mix even when choke groups fire. A stem is the whole
  pattern played with one pad captured, so the closed hat still chokes the open hat — and voice
  stealing resolves identically to the mix. (Stems are pre-master, which is what makes summing
  meaningful: the master strip's limiter and compressor are non-linear.)
- Sample auto-categorisation matches filename tokens on word boundaries, so `Phat Kick` is a kick,
  not a closed hat. `KickDrum`, `kick01` and `808kick` all still read as kicks. A name no token
  matches falls through to the audio-feature classifier. You can still right-click a sample in the
  Library to re-tag it; the override is saved and survives a re-scan.

---

## Project layout

```
rollforge/
  CMakeLists.txt        # CMake + pinned JUCE FetchContent
  src/
    engine/             # real-time audio (voices, mixer, fx, clock) — NO GUI includes
    model/              # patterns, kits, steps, rolls — pure, unit-testable data
    library/            # sample scanner, SQLite index, categoriser
    ui/                 # all JUCE Components
    app/                # Main.cpp, application shell
  tests/                # unit tests (juce::UnitTest) — run in CI on both OSes
  assets/               # starter samples are SYNTHESISED in code (repo ships no binaries)
  packaging/            # AppImage / Inno Setup scripts, CI notes
  .github/workflows/    # GitHub Actions CI (Linux + Windows + ASan)
```

---

## License

RollForge is built against JUCE under its **GPLv3** option and is therefore
distributed under the **GNU General Public License v3.0** — see [`LICENSE`](LICENSE).

> **Note for closed-source distribution:** shipping RollForge under a
> proprietary/closed-source license requires a **commercial JUCE license** from
> [juce.com](https://juce.com). The GPLv3 build here is for open-source use.

RollForge itself vendors no third-party binaries. Bundled starter sounds are
generated procedurally in code at first run.
