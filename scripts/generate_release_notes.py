#!/usr/bin/env python3
import os
import sys

def generate_release_notes(notes_path: str, tag: str, ver: str, changes: str = ""):
    lines = [
        f"## Pylux {tag}",
        "",
        "Automated multi-device build release from `master` branch.",
        "",
        "### Platform Binaries & Packages",
        "",
        "#### Windows (x86_64 — Desktop & ROG Xbox Ally X)",
        "- **`pylux-windows-x64.msi`** — Windows 64-bit Installer package with automated `*.bak` file purging",
        "- **`pylux-windows-installer.exe`** — Complete Inno Setup installer package",
        "- **`pylux-windows-x64.exe`** — Standalone 64-bit executable (in-app updater compatible)",
        "- **`pylux.exe`** — Standalone executable alias",
        "- **`pylux-windows-x64.zip`** — Portable zip archive with bundled Qt6 runtime libraries",
        "",
        "#### Windows (ARM64 — Snapdragon X Elite, Surface Pro & WoA)",
        "- **`pylux-windows-arm64.msi`** — Native Windows ARM64 Installer package",
        "- **`pylux-windows-arm64-installer.exe`** — Native Windows ARM64 Inno Setup installer",
        "- **`pylux-windows-arm64.exe`** — Native ARM64 standalone executable",
        "- **`pylux-windows-arm64.zip`** — Native ARM64 portable zip archive with bundled libraries",
        "",
        "#### macOS (Apple Silicon — M1 / M2 / M3 / M4)",
        "- **`Pylux-macOS-arm64.dmg`** / **`Pylux-macOS.dmg`** — Drag-and-drop disk image installer",
        "- **`Pylux-macOS-arm64.zip`** / **`Pylux-macOS.zip`** — Portable `Pylux.app` application bundle",
        "",
        "#### macOS (Intel — x86_64)",
        "- **`Pylux-macOS-x86_64.dmg`** — Native Intel Mac disk image installer",
        "- **`Pylux-macOS-x86_64.zip`** — Native Intel Mac `Pylux.app` application bundle",
        "",
        "#### Linux (x86_64 — SteamOS, Bazzite, ROG Xbox Ally X Linux & Desktop)",
        "- **`Pylux-Linux-x86_64.AppImage`** — Standalone AppImage executable",
        "- **`Pylux-Linux-x86_64.zip`** — Steam-compatible portable zip with `launch.sh` and bundled libs",
        "",
        "#### Linux (ARM64 / aarch64 — Raspberry Pi 5, ARM SBCs & Handhelds)",
        "- **`Pylux-Linux-aarch64.AppImage`** — Native ARM64 AppImage executable",
        "- **`Pylux-Linux-aarch64.zip`** — Native ARM64 portable zip with bundled libs",
        "",
        "#### Android (Phones, Tablets & Handhelds)",
        "- **`Pylux-Android.apk`** — Sideloadable debug APK package",
        "",
        "#### iOS",
        "- **`Pylux-iOS.ipa`** — Sideloadable unsigned IPA (install via AltStore, Sideloadly, or TrollStore)",
        "",
        "### SHA-256 Checksums",
        "```text",
    ]

    checksums_file = "dist/SHA256SUMS.txt"
    if os.path.exists(checksums_file):
        with open(checksums_file, "r", encoding="utf-8") as f:
            lines.append(f.read().strip())
    else:
        lines.append("(No checksums file found)")

    lines.extend([
        "```",
        "",
        "### Recent Commits",
        "```text",
        changes.strip() if changes else "(No commit history available)",
        "```",
        "",
    ])

    with open(notes_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print(f"Generated release notes at {notes_path}")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: generate_release_notes.py <output_path> <tag> [version] [changes]")
        sys.exit(1)
    out_path = sys.argv[1]
    tag_val = sys.argv[2]
    ver_val = sys.argv[3] if len(sys.argv) > 3 else tag_val.lstrip("v")
    changes_val = sys.argv[4] if len(sys.argv) > 4 else ""
    generate_release_notes(out_path, tag_val, ver_val, changes_val)
