# Packaging

This directory holds the release/packaging tooling for RollForge, built in
**Phase 7 — Packaging & polish**.

## Layout

```
packaging/
  ci/         # (reserved) notes/scripts referenced by the workflows
  linux/      # build-appimage.sh + rollforge.desktop + rollforge.svg
  windows/    # build-packages.ps1 + rollforge.iss
```

## Targets

| OS      | Primary artifact           | Also                          |
|---------|----------------------------|-------------------------------|
| Linux   | AppImage (via linuxdeploy) | portable `tar.gz`             |
| Windows | Inno Setup installer       | portable `zip` (self-contained) |

## How packages are built

Packaging is a **separate** workflow from the per-push CI:

- **[`../.github/workflows/ci.yml`](../.github/workflows/ci.yml)** — builds + tests on
  `ubuntu-22.04` and `windows-latest` (+ a Linux ASan job) on every push. Authoritative.
- **[`../.github/workflows/release.yml`](../.github/workflows/release.yml)** — builds the
  Release app on both OSes and packages it. A **`v*` tag** publishes a GitHub Release
  with all four artifacts; a **workflow_dispatch** run produces them without publishing
  (a smoke test).

The two build scripts also run locally after a Release build:

```bash
VERSION=0.1.0 packaging/linux/build-appimage.sh          # Linux: AppImage + tar.gz
```
```powershell
packaging\windows\build-packages.ps1 -Version 0.1.0      # Windows: setup.exe + zip
```

`build-appimage.sh` downloads `linuxdeploy` **and** its separate
`linuxdeploy-plugin-appimage` output plugin, renders the SVG icon to PNG, bundles the
app's non-system libraries, and runs FUSE-free (`APPIMAGE_EXTRACT_AND_RUN=1`) so it
works on headless CI. `build-packages.ps1` builds the portable zip and drives Inno
Setup's `ISCC`; the app statically links the MSVC runtime (see `../CMakeLists.txt`), so
neither Windows artifact needs a Visual C++ redistributable.

## ASIO stub (Windows)

RollForge ships with **ASIO disabled** (`JUCE_ASIO=0` in
[`../CMakeLists.txt`](../CMakeLists.txt)). Windows audio uses **WASAPI**
(default) and **DirectSound** (fallback), which cover the vast majority of
users without any proprietary SDK.

ASIO is intentionally excluded because it requires Steinberg's ASIO SDK, whose
license does not permit redistribution of the headers. To build an ASIO-enabled
variant yourself for private use:

1. Download the ASIO SDK from Steinberg and accept its license.
2. Point JUCE at the SDK headers (e.g. `juce_add_gui_app(... )` with the ASIO
   SDK include path) and set `JUCE_ASIO=1`.
3. Do **not** redistribute the resulting binary unless your ASIO SDK license
   permits it.

This is left as a clearly-marked, opt-in path rather than a default build.

## PipeWire (Linux)

No PipeWire-native code is used. PipeWire is reached through its ALSA
(`pipewire-alsa`) or JACK (`pipewire-jack` / `pw-jack`) compatibility layers,
which appear to RollForge as ordinary ALSA/JACK devices.
