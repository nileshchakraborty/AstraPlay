![AstraPlay Logo](pylux-logo.png)

# AstraPlay

[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-blue)](https://github.com/nileshchakraborty/Pylux/blob/master/LICENSES/AGPL-3.0-only-OpenSSL.txt)
[![Platforms](https://img.shields.io/badge/platforms-Windows%20(x64%20%2B%20ARM64)%20%7C%20macOS%20(Apple%20Silicon%20%2B%20Intel)%20%7C%20Linux%20(x86__64%20%2B%20ARM64)%20%7C%20Android%20%7C%20iOS-brightgreen)](https://github.com/nileshchakraborty/Pylux/releases)
[![Build Status](https://github.com/nileshchakraborty/Pylux/actions/workflows/release-on-merge.yml/badge.svg)](https://github.com/nileshchakraborty/Pylux/actions/workflows/release-on-merge.yml)

**AstraPlay is an open-source, multi-device PS4 and PS5 Remote Play client for Windows, macOS, Linux, SteamOS, Bazzite, Android, and iOS.** It features seamless console discovery, Internet Play, automatic background update checking, and high-performance hardware-accelerated AV1 / HEVC / H.264 video decoding.

---

## Downloads & Supported Platforms

All release packages and binary checksums are published on the [**GitHub Releases Page**](https://github.com/nileshchakraborty/Pylux/releases/latest).

| Operating System | Architectures | Download Packages | Description |
| :--- | :--- | :--- | :--- |
| **Windows** | **x86_64** (Desktop, ROG Xbox Ally X)<br>**ARM64** (Snapdragon X Elite, WoA) | [`.msi Installer (x64)`](https://github.com/nileshchakraborty/Pylux/releases/latest)<br>[`.exe Setup Installer`](https://github.com/nileshchakraborty/Pylux/releases/latest)<br>[`.zip Portable`](https://github.com/nileshchakraborty/Pylux/releases/latest)<br>[`astraplay.exe Standalone`](https://github.com/nileshchakraborty/Pylux/releases/latest) | Full Inno Setup installer, WiX `.msi` package, portable ZIP with bundled Qt6 & FFmpeg DLLs, or standalone executable for the in-app updater. |
| **macOS** | **Apple Silicon** (`arm64`: M1/M2/M3/M4)<br>**Intel** (`x86_64`) | [`.dmg Disk Image`](https://github.com/nileshchakraborty/Pylux/releases/latest)<br>[`.zip App Bundle`](https://github.com/nileshchakraborty/Pylux/releases/latest) | Native drag-and-drop `.dmg` installers and portable `AstraPlay.app` archives for both Apple Silicon and Intel Macs. |
| **Linux / Handhelds** | **x86_64** (Desktop, SteamOS, Bazzite)<br>**ARM64** (`aarch64`: Raspberry Pi 5, ARM SBCs) | [`.AppImage Executable`](https://github.com/nileshchakraborty/Pylux/releases/latest)<br>[`.zip Portable Package`](https://github.com/nileshchakraborty/Pylux/releases/latest) | Standalone portable AppImage executables and Steam-ready ZIP packages with `launch.sh`. |
| **Android** | **ARM64 / Universal** (Phones, Tablets, Android TV) | [`.apk Package`](https://github.com/nileshchakraborty/Pylux/releases/latest)<br>[`.aab Play Store Bundle`](https://github.com/nileshchakraborty/Pylux/releases/latest) | Sideloadable APK and Google Play Store App Bundle (`com.astraplay.stream`). |
| **iOS / iPadOS** | **ARM64** (iPhone & iPad) | [`.ipa Package`](https://github.com/nileshchakraborty/Pylux/releases/latest) | Sideloadable IPA (install via AltStore, Sideloadly, or TrollStore) or TestFlight. |

---

## Features

- **Multi-Device Support**: Native packages for Windows (x64 & ARM64), macOS (Apple Silicon & Intel), Linux (x86_64 & aarch64), Android, and iOS.
- **Handheld Optimized**: Native controller support, gamepad navigation, and Quick Access Menu integrations for the ROG Xbox Ally X, Steam Deck, Legion Go, and Claw.
- **In-App Auto-Updater**: Built-in background update checker (`UpdateManager`) that fetches latest releases, downloads packages, purges stale `.bak` files, and applies updates seamlessly.
- **Seamless Settings Migration**: Automatically migrates previous `pylux.conf` and `Chiaki.conf` configurations so existing paired consoles and preferences carry over without reconfiguration.
- **Console Discovery**: Automatic broadcast discovery of PS4 and PS5 consoles on local networks with PIN pairing.
- **Internet Play**: Connect and stream from registered consoles over WAN and mobile networks.
- **Modern Hardware Decoding**: Accelerated decoding with Vulkan, Metal, D3D11VA, VAAPI, VideoToolbox, and AMediaCodec.
- **Touch & Gamepad Controls**: On-screen overlay controls for touch devices alongside full DualSense, DualShock 4, and Xbox controller support with haptics.

---

## Project Specifications & Architecture

Comprehensive technical documentation for AstraPlay is available in the repository root:

- [**PRD.md**](PRD.md) — Product Requirements Document and roadmap
- [**DESIGN.md**](DESIGN.md) — UI/UX guidelines, design systems, and handheld input paradigms
- [**ARCHITECTURE.md**](ARCHITECTURE.md) — System architecture, cross-platform subsystems, and build pipeline
- [**DATA.md**](DATA.md) — Data storage, configuration models, and state persistence
- [**SECURITY.md**](SECURITY.md) — Security policies, cryptographic keys, credentials, and network privacy
- [**CODE_STYLE.md**](CODE_STYLE.md) — Code style guides for C++, Qt, QML, Python, and shell scripts
- [**TESTING.md**](TESTING.md) — Testing strategy, unit tests, integration tests, and hardware validation
- [**AGENTS.md**](AGENTS.md) — Autonomous development agents, operating instructions, and CI/CD policies

---

## Building from Source

### Prerequisites
- CMake 3.25+
- Ninja build system
- Python 3.10+ (with `protobuf`)
- Qt 6.6+ (`Core`, `Gui`, `Quick`, `Qml`, `Network`, `Svg`, `Widgets`)
- FFmpeg 6.0+ (or bundled static libraries)
- SDL 2.0+

### Linux / macOS
```bash
git clone --recursive https://github.com/nileshchakraborty/Pylux.git
cd Pylux
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Windows (MSYS2 MinGW64 or ClangARM64)
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

---

## ROG Xbox Ally X Deployment Helper

To push and test the latest build on an ROG Xbox Ally X Windows host over SSH:

```bash
bash scripts/push-to-allyx.sh
```

---

## Responsible Use

AstraPlay is intended for use with games and hardware you own or are legally licensed to access with a valid account. It does not circumvent copy protection, distribute proprietary firmware, or facilitate piracy.

This project is an independent community project and is not endorsed or certified by Sony Interactive Entertainment Inc. PlayStation, PS4, PS5, DualSense, and DualShock are registered trademarks of Sony Interactive Entertainment Inc.

---

## Credits

AstraPlay is built on top of [Chiaki](https://git.sr.ht/~thestr4ng3r/chiaki) and [chiaki-ng](https://github.com/streetpea/chiaki-ng). Special thanks to the Chiaki and chiaki-ng maintainers and community contributors for their foundational Remote Play engineering.
