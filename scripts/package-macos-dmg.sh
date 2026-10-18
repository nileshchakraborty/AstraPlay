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

# 1. Create Terminal installer command with elevation request
cat <<'EOF' > "$STAGE_DIR/Install AstraPlay.command"
#!/usr/bin/env bash
# AstraPlay macOS Installer
# Requests administrator elevation via standard macOS system authorization,
# installs AstraPlay into /Applications, and configures permissions.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
APP_SRC="$SCRIPT_DIR/AstraPlay.app"
DEST_APP="/Applications/AstraPlay.app"

echo "=============================================================="
echo "                   AstraPlay macOS Installer"
echo "=============================================================="
echo ""

if [[ ! -d "$APP_SRC" ]]; then
    APP_SRC=$(find "$SCRIPT_DIR" -maxdepth 2 -name "AstraPlay.app" | head -n 1)
    if [[ -z "$APP_SRC" || ! -d "$APP_SRC" ]]; then
        echo "Error: Could not locate AstraPlay.app alongside this installer."
        echo "Checked directory: $SCRIPT_DIR"
        read -p "Press Enter to exit..."
        exit 1
    fi
fi

echo "Source: $APP_SRC"
echo "Target: $DEST_APP"
echo ""

if [[ $EUID -ne 0 ]]; then
    echo "Requesting administrator authorization to install into /Applications..."
    echo "(Please authenticate with Touch ID or your macOS user password when prompted)"
    echo ""
    
    ESCAPED_SRC=$(printf '%s' "$APP_SRC" | sed "s/'/'\\\\''/g")
    ESCAPED_DEST=$(printf '%s' "$DEST_APP" | sed "s/'/'\\\\''/g")
    CMD="rm -rf '$ESCAPED_DEST' && cp -R '$ESCAPED_SRC' '$ESCAPED_DEST' && xattr -cr '$ESCAPED_DEST' && codesign --force --deep --sign - '$ESCAPED_DEST' 2>/dev/null || true"
    
    if osascript -e "do shell script \"$CMD\" with administrator privileges"; then
        echo ""
        echo "=============================================================="
        echo "  AstraPlay was successfully installed to /Applications!"
        echo "=============================================================="
        echo ""
        echo "Opening AstraPlay..."
        open "$DEST_APP"
        sleep 2
        exit 0
    else
        echo ""
        echo "Administrator authorization was cancelled or unavailable."
        echo "Falling back to user Applications directory ($HOME/Applications)..."
        mkdir -p "$HOME/Applications"
        USER_DEST="$HOME/Applications/AstraPlay.app"
        rm -rf "$USER_DEST"
        cp -R "$APP_SRC" "$USER_DEST"
        xattr -cr "$USER_DEST" 2>/dev/null || true
        codesign --force --deep --sign - "$USER_DEST" 2>/dev/null || true
        echo "Installed to $USER_DEST!"
        open "$USER_DEST"
        sleep 2
        exit 0
    fi
else
    rm -rf "$DEST_APP"
    cp -R "$APP_SRC" "$DEST_APP"
    xattr -cr "$DEST_APP" 2>/dev/null || true
    codesign --force --deep --sign - "$DEST_APP" 2>/dev/null || true
    echo "Installed to $DEST_APP!"
    open "$DEST_APP"
fi
EOF
chmod +x "$STAGE_DIR/Install AstraPlay.command"
cp "$STAGE_DIR/Install AstraPlay.command" "$STAGE_DIR/Fix-Gatekeeper-and-Install.command"

# 2. Create native macOS AppleScript GUI installer app with Touch ID / Password prompt
if command -v osacompile >/dev/null 2>&1; then
    APPLESCRIPT_SRC="$STAGE_DIR/installer.applescript"
    cat <<'EOF' > "$APPLESCRIPT_SRC"
tell application "System Events" to set frontmost of process 1 to true
set sourceDir to POSIX path of ((path to me as text) & "::")
set appSource to sourceDir & "AstraPlay.app"
set targetApp to "/Applications/AstraPlay.app"

set userChoice to button returned of (display dialog "AstraPlay macOS Installer" & return & return & "This will install AstraPlay into your Applications folder, configure permissions, and remove macOS Gatekeeper quarantine." buttons {"Cancel", "Install"} default button "Install" with icon note)

if userChoice is "Install" then
    try
        set shellCmd to "rm -rf " & quoted form of targetApp & " && cp -R " & quoted form of appSource & " " & quoted form of targetApp & " && xattr -cr " & quoted form of targetApp & " && codesign --force --deep --sign - " & quoted form of targetApp & " 2>/dev/null || true"
        do shell script shellCmd with administrator privileges
        
        set launchChoice to button returned of (display dialog "AstraPlay has been successfully installed to /Applications!" & return & return & "Would you like to open AstraPlay now?" buttons {"Done", "Launch"} default button "Launch" with icon note)
        if launchChoice is "Launch" then
            do shell script "open " & quoted form of targetApp
        end if
    on error errMsg number errNum
        if errNum is not -128 then
            display alert "Installation: " & errMsg
        end if
    end try
end if
EOF
    echo "Compiling native GUI installer application in DMG (Install AstraPlay.app)..."
    osacompile -o "$STAGE_DIR/Install AstraPlay.app" "$APPLESCRIPT_SRC" 2>/dev/null || true
    rm -f "$APPLESCRIPT_SRC"
fi

# 3. Add friendly README text for macOS users
cat <<'EOF' > "$STAGE_DIR/READ ME FIRST.txt"
================================================================================
                    AstraPlay for macOS - Installation Guide
================================================================================

Welcome to AstraPlay!

To install AstraPlay with standard macOS security authorization:

METHOD 1: Native 1-Click GUI Installer (Recommended)
----------------------------------------------------
Double-click "Install AstraPlay.app" in this disk image.
Click "Install", then authenticate with Touch ID or your Mac password.
AstraPlay will be installed to /Applications and launched automatically.

METHOD 2: Terminal Script Installer
-----------------------------------
Double-click "Install AstraPlay.command" in this disk image.
Follow the on-screen instructions to authorize and install.

METHOD 3: Drag & Drop (Manual)
------------------------------
1. Drag the "AstraPlay" icon to the "Applications" alias.
2. If macOS blocks opening with "Apple could not verify...", open:
   System Settings -> Privacy & Security -> Security section -> "Open Anyway".
   Or run in Terminal: xattr -cr /Applications/AstraPlay.app

================================================================================
EOF

echo "Creating DMG image with hdiutil..."
hdiutil create -srcfolder "$STAGE_DIR" -volname "AstraPlay" -ov "$OUTPUT_DMG"

if command -v codesign >/dev/null 2>&1; then
    echo "Signing DMG with '$SIGN_ID'..."
    codesign --force --sign "$SIGN_ID" "$OUTPUT_DMG" 2>/dev/null || true
fi

echo "DMG successfully created: $OUTPUT_DMG"
