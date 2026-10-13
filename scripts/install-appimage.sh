#!/usr/bin/env bash
# Install the current AstraPlay AppImage into the user's home directory.
# Usage: ./scripts/install-appimage.sh [--file PATH] [--extract-and-run]
# With no --file, download the latest GitHub release and verify SHA256SUMS.txt.
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: install-appimage.sh [--file PATH] [--extract-and-run]

Without --file, downloads the latest AstraPlay AppImage for this CPU and verifies
it against the release's SHA256SUMS.txt. --file installs an existing download.
--extract-and-run uses the AppImage FUSE fallback (slower; only if needed).
Run the script again to update the installed AppImage. No root access is needed.
EOF
}

source_file=""
extract_and_run=0
while (($#)); do
    case "$1" in
        --file)
            (($# >= 2)) || { usage >&2; exit 2; }
            source_file=$2
            shift 2
            ;;
        --extract-and-run)
            extract_and_run=1
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

if ((EUID == 0)); then
    printf 'Run this installer as your normal user, without sudo.\n' >&2
    exit 1
fi

if [[ -n "$source_file" && ! -f "$source_file" ]]; then
    printf 'AppImage not found: %s\n' "$source_file" >&2
    exit 1
fi

case "$(uname -m)" in
    x86_64) asset=AstraPlay-Linux-x86_64.AppImage ;;
    aarch64|arm64) asset=AstraPlay-Linux-aarch64.AppImage ;;
    *) printf 'Unsupported CPU architecture: %s\n' "$(uname -m)" >&2; exit 1 ;;
esac

install_dir=${ASTRAPLAY_INSTALL_DIR:-"$HOME/.local/opt/astraplay"}
bin_dir=${ASTRAPLAY_BIN_DIR:-"$HOME/.local/bin"}
data_home=${XDG_DATA_HOME:-"$HOME/.local/share"}
application_dir="$data_home/applications"
icon_dir="$data_home/icons/hicolor/512x512/apps"
installed_appimage="$install_dir/AstraPlay.AppImage"
launcher="$bin_dir/astraplay"
desktop_file="$application_dir/io.github.nileshchakraborty.AstraPlay.desktop"
icon_file="$icon_dir/io.github.nileshchakraborty.AstraPlay.png"

# Desktop Entry Exec fields use % as a field-code prefix and quote special chars.
desktop_quote() {
    local value=$1
    value=${value//%/%%}
    value=${value//\\/\\\\}
    value=${value//\"/\\\"}
    value=${value//\$/\\\$}
    value=${value//\`/\\\`}
    printf '"%s"' "$value"
}

temp_dir=$(mktemp -d "${TMPDIR:-/tmp}/astraplay-install.XXXXXX")
staged_appimage=""
cleanup() {
    [[ -z "$staged_appimage" ]] || rm -f -- "$staged_appimage"
    rm -f -- "$temp_dir/$asset" "$temp_dir/SHA256SUMS.txt" "$temp_dir/icon.png"
    rmdir -- "$temp_dir" 2>/dev/null || true
}
trap cleanup EXIT

if [[ -z "$source_file" ]]; then
    for command_name in curl sha256sum awk; do
        command -v "$command_name" >/dev/null || { printf '%s is required\n' "$command_name" >&2; exit 1; }
    done
    release_url=https://github.com/nileshchakraborty/AstraPlay/releases/latest/download
    printf 'Downloading %s...\n' "$asset"
    curl -fsSL --retry 3 -o "$temp_dir/$asset" "$release_url/$asset"
    curl -fsSL --retry 3 -o "$temp_dir/SHA256SUMS.txt" "$release_url/SHA256SUMS.txt"
    source_file="$temp_dir/$asset"
fi

checksum_file="$(dirname -- "$source_file")/SHA256SUMS.txt"
if [[ -f "$checksum_file" ]]; then
    command -v sha256sum >/dev/null || { printf 'sha256sum is required\n' >&2; exit 1; }
    expected=$(awk -v name="$(basename -- "$source_file")" '$2 == name { print $1; exit }' "$checksum_file")
    if [[ ! "$expected" =~ ^[a-fA-F0-9]{64}$ ]]; then
        printf 'No valid checksum for %s in %s\n' "$(basename -- "$source_file")" "$checksum_file" >&2
        exit 1
    fi
    printf '%s  %s\n' "$expected" "$source_file" | sha256sum --check --status || {
        printf 'SHA-256 verification failed; nothing was installed.\n' >&2
        exit 1
    }
    printf 'SHA-256 verified.\n'
else
    printf 'No SHA256SUMS.txt beside local file; installing without checksum verification.\n' >&2
fi

mkdir -p -- "$install_dir" "$bin_dir" "$application_dir" "$icon_dir"
staged_appimage=$(mktemp "$install_dir/.AstraPlay.AppImage.XXXXXX")
cp -- "$source_file" "$staged_appimage"
chmod 755 "$staged_appimage"
mv -f -- "$staged_appimage" "$installed_appimage"
staged_appimage=""

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repository_icon="$script_dir/../gui/res/logo_square_1024.png"
if [[ -f "$repository_icon" ]]; then
    cp -- "$repository_icon" "$icon_file"
elif command -v curl >/dev/null && curl -fsSL --retry 2 \
    -o "$temp_dir/icon.png" \
    https://raw.githubusercontent.com/nileshchakraborty/AstraPlay/master/gui/res/logo_square_1024.png; then
    cp -- "$temp_dir/icon.png" "$icon_file"
fi

printf '#!/usr/bin/env bash\nset -e\n' > "$launcher"
if ((extract_and_run)); then
    printf 'export APPIMAGE_EXTRACT_AND_RUN=1\n' >> "$launcher"
fi
printf 'exec %q "$@"\n' "$installed_appimage" >> "$launcher"
chmod 755 "$launcher"

{
    printf '[Desktop Entry]\n'
    printf 'Type=Application\nName=AstraPlay\n'
    printf 'Comment=PlayStation Remote Play client\n'
    printf 'Exec=%s\n' "$(desktop_quote "$launcher")"
    if [[ -f "$icon_file" ]]; then
        printf 'Icon=%s\n' "$icon_file"
    else
        printf 'Icon=applications-games\n'
    fi
    printf 'Terminal=false\nCategories=Game;\nStartupNotify=true\n'
} > "$desktop_file"

if command -v desktop-file-validate >/dev/null; then
    desktop-file-validate "$desktop_file"
fi
if command -v update-desktop-database >/dev/null; then
    update-desktop-database "$application_dir" >/dev/null 2>&1 || true
fi

printf '\nInstalled AstraPlay: %s\n' "$installed_appimage"
printf 'Launch from the application menu or run: %s\n' "$launcher"
printf 'For Steam Gaming Mode, add AstraPlay as a non-Steam game in Steam Desktop Mode.\n'
printf 'Steam shortcuts you already have are unchanged.\n'
