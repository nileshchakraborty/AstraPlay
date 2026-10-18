#!/usr/bin/env python3
import os
import sys

def generate_release_notes(notes_path: str, tag: str, ver: str, changes: str = ""):
    lines = [
        f"## AstraPlay {tag}",
        "",
        "Automated multi-device build release from `master` branch.",
        "",
        "### Platform Binaries & Packages",
        "",
        "#### Windows (x86_64 — Desktop & ROG Xbox Ally X)",
        "- **`astraplay-windows-x64-msi.zip`** — Windows 64-bit MSI installer archive with bundled runtime files",
        "- **`astraplay-windows-installer.exe`** — Complete Inno Setup installer package",
        "- **`astraplay-windows-x64.exe`** — Standalone 64-bit executable (in-app updater compatible)",
        "- **`astraplay.exe`** — Standalone executable alias",
        "- **`astraplay-windows-x64.zip`** — Portable zip archive with bundled Qt6 runtime libraries",
        "",
        "#### Windows (ARM64 — Snapdragon X Elite, Surface Pro & WoA)",
        "- **`astraplay-windows-arm64-installer.exe`** — Native Windows ARM64 Inno Setup installer",
        "- **`astraplay-windows-arm64.exe`** — Native ARM64 standalone executable",
        "- **`astraplay-windows-arm64.zip`** — Native ARM64 portable zip archive with bundled libraries",
        "",
        "#### macOS (Apple Silicon — M1 / M2 / M3 / M4)",
        "- **`AstraPlay-macOS-arm64-installer.zip`** — **Recommended Native Installer Archive**: Includes 1-click Touch ID / Admin elevation installer (`Install AstraPlay.app` and `Install AstraPlay.command`), app bundle, and `/Applications` alias",
        "- **`AstraPlay-macOS-arm64.dmg`** / **`AstraPlay-macOS.dmg`** — Disk image installer with integrated Touch ID elevation helper",
        "- **`AstraPlay-macOS-arm64.zip`** / **`AstraPlay-macOS.zip`** — Portable application bundle (in-app updater compatible)",
        "",
        "#### macOS (Intel — x86_64)",
        "- **`AstraPlay-macOS-x86_64-installer.zip`** — **Recommended Native Installer Archive**: Includes 1-click Touch ID / Admin elevation installer (`Install AstraPlay.app` and `Install AstraPlay.command`)",
        "- **`AstraPlay-macOS-x86_64.dmg`** — Native Intel Mac disk image installer with integrated Touch ID elevation helper",
        "- **`AstraPlay-macOS-x86_64.zip`** — Native Intel Mac application bundle (in-app updater compatible)",
        "",
        "> [!TIP]",
        "> **macOS Security & Elevation Note (Community Open-Source Build)**:",
        "> - **Native 1-Click Installer (Recommended)**: Download `AstraPlay-macOS-arm64-installer.zip`, unzip, and double-click `Install AstraPlay.app` (or `Install AstraPlay.command`). Authorize when prompted via Touch ID or Mac password — AstraPlay installs to `/Applications` with zero Gatekeeper friction.",
        "> - **Fastest Terminal 1-Liner**: `curl -fsSL https://raw.githubusercontent.com/nileshchakraborty/AstraPlay/master/scripts/install-macos.sh | bash`",
        "> - **Manual Drag & Drop (System Settings Bypass)**: Drag AstraPlay to Applications. If prompted with *\"Apple could not verify...\"*, open **System Settings > Privacy & Security**, scroll down to **Security**, and click **Open Anyway**.",
        "",
        "#### Linux (x86_64 — SteamOS, Bazzite, ROG Xbox Ally X Linux & Desktop)",
        "- **`AstraPlay-Linux-x86_64.AppImage`** — Standalone AppImage executable",
        "- **`AstraPlay-Linux-x86_64.zip`** — Steam-compatible portable zip with `launch.sh` and bundled libs",
        "",
        "#### Linux (ARM64 / aarch64 — Raspberry Pi 5, ARM SBCs & Handhelds)",
        "- **`AstraPlay-Linux-aarch64.AppImage`** — Native ARM64 AppImage executable",
        "- **`AstraPlay-Linux-aarch64.zip`** — Native ARM64 portable zip with bundled libs",
        "",
        "#### Android (Phones, Tablets & Handhelds)",
        "- **`AstraPlay-Android.apk`** — Sideloadable debug / release APK package",
        "- **`AstraPlay-Android.aab`** — Google Play Store App Bundle",
        "",
        "#### iOS",
        "- **`AstraPlay-iOS.ipa`** — Sideloadable unsigned IPA (install via AltStore, Sideloadly, or TrollStore)",
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
        "### Recent Changes",
        changes if changes else "No additional commits.",
    ])

    with open(notes_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Usage: generate_release_notes.py <output_path> <tag> <version> [changes]")
        sys.exit(1)
    notes_file = sys.argv[1]
    tag_name = sys.argv[2]
    version = sys.argv[3]
    commits = sys.argv[4] if len(sys.argv) > 4 else ""
    generate_release_notes(notes_file, tag_name, version, commits)
