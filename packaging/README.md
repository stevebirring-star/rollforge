# Packaging

This directory holds the release/packaging tooling for RollForge. Most of it is
scaffolded in **Phase 7 — Packaging & polish**; this README documents the plan
and the few decisions that already matter earlier.

## Layout (planned)

```
packaging/
  ci/         # notes/scripts referenced by .github/workflows/ci.yml
  linux/      # AppImage (linuxdeploy) build script + .desktop / icon (Phase 7)
  windows/    # Inno Setup script + portable-zip manifest (Phase 7)
```

## Targets

| OS      | Primary artifact           | Also                |
|---------|----------------------------|---------------------|
| Linux   | AppImage (via linuxdeploy) | plain `tar.gz`      |
| Windows | Inno Setup installer       | portable `zip`      |

CI builds and tests on `ubuntu-22.04` and `windows-latest`; see
[`../.github/workflows/ci.yml`](../.github/workflows/ci.yml).

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
