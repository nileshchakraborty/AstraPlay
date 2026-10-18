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

# Add one-click Gatekeeper installer helper
cat <<'EOF' > "$STAGE_DIR/Fix-Gatekeeper-and-Install.command"
#!/usr/bin/env bash
# AstraPlay macOS Helper
# Copies AstraPlay.app to /Applications and clears macOS Gatekeeper quarantine.

set -e
echo "=========================================================="
echo "         AstraPlay macOS Installer & Gatekeeper Fix"
echo "=========================================================="
echo ""

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
APP_SRC="$SCRIPT_DIR/AstraPlay.app"
DEST_DIR="/Applications"
APP_DEST="$DEST_DIR/AstraPlay.app"

if [ ! -d "$APP_SRC" ]; then
    echo "Error: Cannot find AstraPlay.app next to this script."
    echo "Path checked: $APP_SRC"
    read -p "Press Enter to exit..."
    exit 1
fi

echo "Installing AstraPlay to $DEST_DIR..."
if [ -d "$APP_DEST" ]; then
    echo "Updating existing installation in $APP_DEST..."
    rm -rf "$APP_DEST"
fi

cp -R "$APP_SRC" "$DEST_DIR/"

echo "Removing macOS Gatekeeper quarantine attributes..."
xattr -cr "$APP_DEST" 2>/dev/null || true

echo "Applying local ad-hoc code signature..."
codesign --force --deep --sign - "$APP_DEST" 2>/dev/null || true

echo ""
echo "=========================================================="
echo "  Installation Successful! AstraPlay is ready to use."
echo "=========================================================="
echo ""
echo "Launching AstraPlay..."
open "$APP_DEST"
sleep 2
EOF
chmod +x "$STAGE_DIR/Fix-Gatekeeper-and-Install.command"

# Add friendly README text for macOS users
cat <<'EOF' > "$STAGE_DIR/READ ME FIRST.txt"
================================================================================
                    AstraPlay for macOS - Installation Guide
================================================================================

AstraPlay is an open-source community application. If macOS Gatekeeper displays:
  "Apple could not verify 'AstraPlay-macOS-arm64.dmg' is free of malware"
  or "Apple cannot check it for malicious software"

Choose any of the simple options below:

METHOD 1: Automated 1-Click Helper (Easiest)
--------------------------------------------
Double-click "Fix-Gatekeeper-and-Install.command" in this disk image.
It will install AstraPlay directly into /Applications, remove the Gatekeeper
quarantine attribute, and open the app automatically.

METHOD 2: Standard Drag & Drop + System Settings (GUI)
------------------------------------------------------
1. Drag the "AstraPlay" icon into the "Applications" folder.
2. Open /Applications and double-click AstraPlay.
3. When the alert ("Apple could not verify...") appears, click "Done".
4. Open System Settings -> Privacy & Security.
5. Scroll down to the "Security" section.
6. You will see: "AstraPlay was blocked to protect your Mac."
7. Click "Open Anyway", enter your password/Touch ID, and click "Open".
   (You only need to do this once; AstraPlay will open normally thereafter).

METHOD 3: Terminal Command
--------------------------
If you already dragged AstraPlay to /Applications:
   xattr -cr /Applications/AstraPlay.app

================================================================================
EOF

echo "Creating DMG image with hdiutil..."
hdiutil create -srcfolder "$STAGE_DIR" -volname "AstraPlay" -ov "$OUTPUT_DMG"

if command -v codesign >/dev/null 2>&1; then
    echo "Signing DMG with '$SIGN_ID'..."
    codesign --force --sign "$SIGN_ID" "$OUTPUT_DMG" 2>/dev/null || true
fi

echo "DMG successfully created: $OUTPUT_DMG"
