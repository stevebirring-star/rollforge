#!/usr/bin/env bash
#
# Build a self-contained Linux AppImage (+ a portable tar.gz) for RollForge.
#
# Runs in CI (see .github/workflows/release.yml) after the Release app is built,
# but also works locally once you have a built binary. Uses linuxdeploy to bundle
# the app's non-system shared libraries and generate the AppImage.
#
# Usage:   packaging/linux/build-appimage.sh
# Env:
#   APP_BIN   path to the built RollForge binary   (default: auto-discover under build/)
#   VERSION   version string embedded in filenames (default: 0.0.0)
#   OUTDIR    where the artifacts are written       (default: dist)
#   TOOLSDIR  where linuxdeploy is cached           (default: build/_tools)
#
# Needs: curl, a built RollForge binary, and (for the AppImage) the same runtime
# libs the app links. FUSE is NOT required — linuxdeploy runs via
# APPIMAGE_EXTRACT_AND_RUN so it works on headless CI runners.

set -euo pipefail
cd "$(dirname "$0")/../.."          # repo root
here="packaging/linux"

VERSION="${VERSION:-0.0.0}"
OUTDIR="${OUTDIR:-dist}"
TOOLSDIR="${TOOLSDIR:-build/_tools}"
ARCH="$(uname -m)"                  # x86_64 on CI

# --- locate the built binary -------------------------------------------------
APP_BIN="${APP_BIN:-}"
if [[ -z "$APP_BIN" ]]; then
    APP_BIN="$(find build -type f -name RollForge -perm -u+x 2>/dev/null | head -n1 || true)"
fi
if [[ -z "$APP_BIN" || ! -f "$APP_BIN" ]]; then
    echo "error: RollForge binary not found. Build the Release app first, or set APP_BIN." >&2
    exit 1
fi
echo "Using binary: $APP_BIN"

mkdir -p "$OUTDIR" "$TOOLSDIR"

# --- render the icon to PNG (linuxdeploy is happiest with a raster icon) ------
# Falls back to the source SVG if neither rsvg-convert nor ImageMagick is present.
ICON_SRC="$here/rollforge.svg"
ICON_ARG="$ICON_SRC"
ICON_PNG="$TOOLSDIR/rollforge.png"
if command -v rsvg-convert >/dev/null 2>&1; then
    rsvg-convert -w 256 -h 256 "$ICON_SRC" -o "$ICON_PNG" && ICON_ARG="$ICON_PNG"
elif command -v convert >/dev/null 2>&1; then
    convert -background none "$ICON_SRC" -resize 256x256 "$ICON_PNG" && ICON_ARG="$ICON_PNG"
else
    echo "note: no rsvg-convert/ImageMagick; using SVG icon directly."
fi

# --- fetch linuxdeploy (cached) ----------------------------------------------
LINUXDEPLOY="$TOOLSDIR/linuxdeploy-$ARCH.AppImage"
if [[ ! -x "$LINUXDEPLOY" ]]; then
    echo "Downloading linuxdeploy..."
    curl -fL --retry 3 -o "$LINUXDEPLOY" \
        "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-$ARCH.AppImage"
    chmod +x "$LINUXDEPLOY"
fi

# --- stage the AppDir + build the AppImage -----------------------------------
APPDIR="build/AppDir"
rm -rf "$APPDIR"
mkdir -p "$APPDIR"

export APPIMAGE_EXTRACT_AND_RUN=1   # no FUSE on CI runners
export VERSION                       # linuxdeploy embeds this in the AppImage name
export OUTPUT="$OUTDIR/RollForge-$VERSION-$ARCH.AppImage"

"$LINUXDEPLOY" \
    --appdir "$APPDIR" \
    --executable "$APP_BIN" \
    --desktop-file "$here/rollforge.desktop" \
    --icon-file "$ICON_ARG" \
    --output appimage

echo "AppImage: $OUTPUT"

# --- portable tar.gz (extract-and-run, no FUSE) ------------------------------
# The AppDir is fully populated (binary + bundled libs + AppRun launcher), so a
# tarball of it is a self-contained portable build: extract and run ./AppRun.
STAGE="RollForge-$VERSION-$ARCH"
rm -rf "build/$STAGE"
cp -r "$APPDIR" "build/$STAGE"
tar -czf "$OUTDIR/$STAGE.tar.gz" -C build "$STAGE"
echo "tar.gz:   $OUTDIR/$STAGE.tar.gz"

echo "Done. Artifacts in $OUTDIR/:"
ls -la "$OUTDIR"
