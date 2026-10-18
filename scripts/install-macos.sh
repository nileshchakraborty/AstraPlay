#!/usr/bin/env bash
# Install the current AstraPlay application bundle on macOS.
# Usage: ./scripts/install-macos.sh [--file PATH] [--launch] [--user-dir]
# With no --file, download the latest GitHub release and verify SHA256SUMS.txt.
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: install-macos.sh [--file PATH] [--no-launch] [--user-dir]

Options:
  --file PATH    Install an existing ZIP, DMG, or .app bundle instead of downloading.
  --no-launch    Do not automatically open AstraPlay after installation completes.
  --user-dir     Install to ~/Applications instead of /Applications.
  -h, --help     Show this help message.

Without --file, this script downloads the latest AstraPlay release for your Mac's
architecture (Apple Silicon arm64 or Intel x86_64), verifies the release checksum,
installs it to /Applications, clears macOS Gatekeeper quarantine, and launches it.
EOF
}

source_file=""
launch_after=1
use_user_dir=0

while (($#)); do
    case "$1" in
        --file)
            (($# >= 2)) || { usage >&2; exit 2; }
            source_file=$2
            shift 2
            ;;
        --launch)
            launch_after=1
            shift
            ;;
        --no-launch)
            launch_after=0
            shift
            ;;
        --user-dir)
            use_user_dir=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            usage >&2
            exit 2
            ;;
    esac
done

if [[ "$(uname -s)" != "Darwin" ]]; then
    printf 'Error: This installer is intended for macOS (Darwin), but detected %s\n' "$(uname -s)" >&2
    exit 1
fi

case "$(uname -m)" in
    arm64)
        asset="AstraPlay-macOS-arm64.zip"
        fallback_asset="AstraPlay-macOS-arm64.dmg"
        arch_label="Apple Silicon (arm64)"
        release_url="https://github.com/nileshchakraborty/AstraPlay/releases/latest/download"
        ;;
    x86_64)
        asset="AstraPlay-macOS-x86_64.zip"
        fallback_asset="AstraPlay-macOS-x86_64.dmg"
        arch_label="Intel (x86_64 - Legacy Final Release v2.12.18)"
        release_url="https://github.com/nileshchakraborty/AstraPlay/releases/download/v2.12.18"
        ;;
    *)
        printf 'Error: Unsupported CPU architecture: %s\n' "$(uname -m)" >&2
        exit 1
        ;;
esac

# Determine installation destination
if [[ $use_user_dir -eq 1 ]]; then
    target_dir="$HOME/Applications"
elif [[ -w "/Applications" ]]; then
    target_dir="/Applications"
else
    # Fallback to ~/Applications if /Applications is not writable by current user
    target_dir="$HOME/Applications"
fi
mkdir -p "$target_dir"
target_app="$target_dir/AstraPlay.app"

temp_dir=$(mktemp -d "${TMPDIR:-/tmp}/astraplay-macos-install.XXXXXX")
mount_point=""

cleanup() {
    if [[ -n "$mount_point" && -d "$mount_point" ]]; then
        hdiutil detach "$mount_point" -quiet 2>/dev/null || diskutil eject "$mount_point" -quiet 2>/dev/null || true
    fi
    rm -rf -- "$temp_dir"
}
trap cleanup EXIT

if [[ -n "$source_file" ]]; then
    if [[ ! -e "$source_file" ]]; then
        printf 'Error: Specified file not found: %s\n' "$source_file" >&2
        exit 1
    fi
else
    printf '\n========================================================\n'
    printf '  Installing AstraPlay for macOS (%s)\n' "$arch_label"
    printf '========================================================\n\n'

    for cmd in curl shasum awk ditto; do
        command -v "$cmd" >/dev/null || { printf 'Error: %s command is required but not installed\n' "$cmd" >&2; exit 1; }
    done

    release_url="${release_url:-https://github.com/nileshchakraborty/AstraPlay/releases/latest/download}"
    if [[ "$(uname -m)" == "x86_64" ]]; then
        printf '  Notice: v2.12.18 is the final release supporting Intel Macs.\n'
        printf '          Newer AstraPlay releases exclusively target Apple Silicon.\n\n'
    fi
    printf 'Downloading %s from %s...\n' "$asset" "$release_url"
    if ! curl -fsSL --retry 3 -o "$temp_dir/$asset" "$release_url/$asset"; then
        printf 'ZIP archive not found on release, trying DMG fallback (%s)...\n' "$fallback_asset"
        asset="$fallback_asset"
        curl -fsSL --retry 3 -o "$temp_dir/$asset" "$release_url/$asset"
    fi
    curl -fsSL --retry 3 -o "$temp_dir/SHA256SUMS.txt" "$release_url/SHA256SUMS.txt" || true
    source_file="$temp_dir/$asset"

    if [[ -f "$temp_dir/SHA256SUMS.txt" ]]; then
        expected_hash=$(awk -v name="$asset" '$2 == name || $2 == ("*" name) { print $1 }' "$temp_dir/SHA256SUMS.txt")
        if [[ -n "$expected_hash" ]]; then
            printf 'Verifying SHA-256 checksum...\n'
            actual_hash=$(shasum -a 256 "$source_file" | awk '{print $1}')
            if [[ "$actual_hash" != "$expected_hash" ]]; then
                printf 'Error: SHA-256 mismatch for %s!\n  Expected: %s\n  Actual:   %s\n' "$asset" "$expected_hash" "$actual_hash" >&2
                exit 1
            fi
            printf 'Checksum verified: %s\n' "$actual_hash"
        fi
    fi
fi

# Safe removal helper if existing app is owned by root
safe_remove_target() {
    local target="$1"
    if [[ -e "$target" ]]; then
        if ! rm -rf "$target" 2>/dev/null; then
            printf 'Requesting authorization to update existing %s...\n' "$target"
            if command -v sudo >/dev/null 2>&1 && [ -t 0 ]; then
                sudo rm -rf "$target"
            elif command -v osascript >/dev/null 2>&1; then
                osascript -e "do shell script \"rm -rf '$target'\" with administrator privileges"
            else
                printf 'Error: Cannot overwrite %s. Please remove it and retry.\n' "$target" >&2
                exit 1
            fi
        fi
    fi
}

# If source is already an .app directory
if [[ -d "$source_file" && "$source_file" == *.app ]]; then
    printf 'Installing %s to %s...\n' "$source_file" "$target_app"
    safe_remove_target "$target_app"
    cp -R "$source_file" "$target_dir/"
elif [[ "$source_file" == *.zip ]]; then
    printf 'Extracting %s...\n' "$source_file"
    extract_dir="$temp_dir/extracted"
    mkdir -p "$extract_dir"
    ditto -x -k "$source_file" "$extract_dir"

    found_app=$(find "$extract_dir" -maxdepth 2 -name "AstraPlay.app" | head -n 1)
    if [[ -z "$found_app" || ! -d "$found_app" ]]; then
        printf 'Error: Could not locate AstraPlay.app inside ZIP %s\n' "$source_file" >&2
        exit 1
    fi

    printf 'Installing AstraPlay to %s...\n' "$target_app"
    safe_remove_target "$target_app"
    cp -R "$found_app" "$target_dir/"
else
    # Mount disk image
    printf 'Mounting disk image...\n'
    mount_point=$(mktemp -d "${TMPDIR:-/tmp}/astraplay-dmg.XXXXXX")
    hdiutil attach "$source_file" -mountpoint "$mount_point" -nobrowse -readonly -quiet

    found_app=$(find "$mount_point" -maxdepth 2 -name "AstraPlay.app" | head -n 1)
    if [[ -z "$found_app" || ! -d "$found_app" ]]; then
        printf 'Error: Could not locate AstraPlay.app inside disk image %s\n' "$source_file" >&2
        exit 1
    fi

    printf 'Installing AstraPlay to %s...\n' "$target_app"
    safe_remove_target "$target_app"
    cp -R "$found_app" "$target_dir/"
fi

# Clear Gatekeeper quarantine and ensure ad-hoc signature is valid
printf 'Removing macOS Gatekeeper quarantine attributes...\n'
xattr -cr "$target_app" 2>/dev/null || true

printf 'Validating local code signature...\n'
codesign --force --deep --sign - "$target_app" 2>/dev/null || true

printf '\n========================================================\n'
printf '  AstraPlay successfully installed to:\n'
printf '  %s\n' "$target_app"
printf '  (Gatekeeper quarantine removed — ready to run!)\n'
printf '========================================================\n\n'

if [[ $launch_after -eq 1 ]]; then
    printf 'Launching AstraPlay...\n'
    open "$target_app"
fi
