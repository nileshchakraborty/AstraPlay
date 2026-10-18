#!/usr/bin/env bash
# Package a macOS .app bundle into a user-friendly .dmg installer disk image.
# Usage: ./scripts/package-macos-dmg.sh <app_bundle_path> <output_dmg_path> [sign_id]
set -euo pipefail

if [[ $# -lt 2 ]]; then
    echo "Usage: $0 <app_bundle_path> <output_dmg_path> [sign_id]"
    exit 1
fi

APP_BUNDLE="$1"
OUTPUT_DMG="$2"
SIGN_ID="${3:-"-"}"

if [[ ! -d "$APP_BUNDLE" ]]; then
    echo "Error: App bundle not found at '$APP_BUNDLE'" >&2
    exit 1
fi

OUTPUT_DIR="$(dirname "$OUTPUT_DMG")"
mkdir -p "$OUTPUT_DIR"
rm -f "$OUTPUT_DMG"

STAGE_DIR=$(mktemp -d "${TMPDIR:-/tmp}/astraplay-dmg-stage.XXXXXX")
cleanup() {
    rm -rf -- "$STAGE_DIR"
}
trap cleanup EXIT

echo "Staging DMG contents in $STAGE_DIR..."
cp -R "$APP_BUNDLE" "$STAGE_DIR/AstraPlay.app"
ln -s /Applications "$STAGE_DIR/Applications"

# 1. Add friendly README text for macOS users
cat <<'EOF' > "$STAGE_DIR/READ ME FIRST.txt"
================================================================================
                    AstraPlay for macOS - Installation Guide
================================================================================

Welcome to AstraPlay!

METHOD 1: Recommended 1-Line Install (Zero Gatekeeper Warnings)
---------------------------------------------------------------
Open Terminal and run:
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/nileshchakraborty/AstraPlay/master/scripts/install-macos.sh)"

This installs AstraPlay directly into /Applications, clears Gatekeeper quarantine,
applies local code signatures, and opens AstraPlay immediately.

METHOD 2: Standard Drag & Drop (Manual)
---------------------------------------
1. Drag the "AstraPlay" icon to the "Applications" alias in this window.
2. Open /Applications and double-click AstraPlay.
3. If macOS displays "Apple could not verify AstraPlay is free of malware":
   - Click "Done" on the alert modal.
   - Open System Settings -> Privacy & Security -> Security section.
   - Click "Open Anyway" (enter your Touch ID or password) and click "Open".
   - (Or run in Terminal: xattr -cr /Applications/AstraPlay.app)
   You only need to do this once; AstraPlay will launch normally thereafter.

================================================================================
EOF

echo "Creating DMG image with hdiutil..."
hdiutil create -srcfolder "$STAGE_DIR" -volname "AstraPlay" -ov "$OUTPUT_DMG"

if command -v codesign >/dev/null 2>&1; then
    echo "Signing DMG with '$SIGN_ID'..."
    codesign --force --sign "$SIGN_ID" "$OUTPUT_DMG" 2>/dev/null || true
fi

echo "DMG successfully created: $OUTPUT_DMG"
