#!/usr/bin/env bash
# =============================================================================
# Download and install pre-built FFmpeg for Windows MSYS2 with retry resilience
# =============================================================================
set -euo pipefail

ARCH="${1:-x64}"
TARGET_DIR="${2:-/mingw64}"

if [ "$ARCH" = "arm64" ] || [ "$ARCH" = "winarm64" ]; then
  FFMPEG_URL="https://github.com/streetpea/FFmpeg-Builds/releases/download/latest/ffmpeg-n7.1-latest-winarm64-gpl-shared-7.1.zip"
  PKG_DIR="ffmpeg-n7.1-latest-winarm64-gpl-shared-7.1"
else
  FFMPEG_URL="https://github.com/streetpea/FFmpeg-Builds/releases/download/latest/ffmpeg-n7.1-latest-win64-gpl-shared-7.1.zip"
  PKG_DIR="ffmpeg-n7.1-latest-win64-gpl-shared-7.1"
fi

echo "Downloading pre-built FFmpeg ($ARCH) to $TARGET_DIR..."
DOWNLOADED=false
for attempt in 1 2 3 4 5; do
  echo "Attempt $attempt: downloading from $FFMPEG_URL..."
  if curl -fSL --retry 3 --retry-delay 3 --connect-timeout 15 --max-time 180 "$FFMPEG_URL" -o ffmpeg.zip && unzip -t ffmpeg.zip >/dev/null 2>&1; then
    echo "FFmpeg archive downloaded and integrity verified!"
    DOWNLOADED=true
    break
  fi
  echo "Attempt $attempt failed or archive was incomplete. Retrying in 5 seconds..."
  rm -f ffmpeg.zip
  sleep 5
done

if [ "$DOWNLOADED" != true ]; then
  echo "ERROR: Failed to download a valid FFmpeg archive after 5 attempts." >&2
  exit 1
fi

unzip -o ffmpeg.zip
mkdir -p "$TARGET_DIR/bin" "$TARGET_DIR/include" "$TARGET_DIR/lib"
cp -a "$PKG_DIR/bin/." "$TARGET_DIR/bin"
cp -a "$PKG_DIR/include/." "$TARGET_DIR/include"
cp -a "$PKG_DIR/lib/." "$TARGET_DIR/lib"
rm -rf "$PKG_DIR" ffmpeg.zip
echo "FFmpeg ($ARCH) successfully installed to $TARGET_DIR."
