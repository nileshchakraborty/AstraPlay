![AstraPlay Logo](astraplay-logo.png)

# AstraPlay

[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-blue)](https://github.com/nileshchakraborty/AstraPlay/blob/master/LICENSES/AGPL-3.0-only-OpenSSL.txt)
[![Platforms](https://img.shields.io/badge/platforms-Windows%20(x64%20%2B%20ARM64)%20%7C%20macOS%20(Apple%20Silicon%20%2B%20Intel)%20%7C%20Linux%20(x86__64%20%2B%20ARM64)%20%7C%20Android%20%7C%20iOS-brightgreen)](https://github.com/nileshchakraborty/AstraPlay/releases)
[![Build Status](https://github.com/nileshchakraborty/AstraPlay/actions/workflows/release-on-merge.yml/badge.svg)](https://github.com/nileshchakraborty/AstraPlay/actions/workflows/release-on-merge.yml)
[![Release](https://img.shields.io/github/v/release/nileshchakraborty/AstraPlay?color=purple)](https://github.com/nileshchakraborty/AstraPlay/releases/latest)

**AstraPlay is a high-performance, 100% free and open-source PlayStation (PS4 & PS5) Remote Play client for Windows, macOS, Linux, SteamOS, Bazzite, Android, and iOS.**

Engineered for ultra-low latency, crisp visuals, and cross-platform flexibility, AstraPlay delivers native hardware-accelerated video streaming, seamless local and Internet Play, gamepad navigation for handheld devices, and private self-hosted authentication options.

---

## Key Highlights

- **100% Free & Open-Source**: Zero paywalls, zero locked features, zero subscription gates, and zero donation nags. All features are fully unlocked for everyone.
- **True Multi-Platform**: Native binaries for Windows (x64 and ARM64), macOS (Apple Silicon and Intel), Linux (x86_64 and aarch64), Android (Phones, Tablets, and Android TV), and iOS/iPadOS.
- **Handheld-First Design**: Tailored for the **ROG Xbox Ally X**, **Steam Deck**, **Lenovo Legion Go**, and **MSI Claw** with gamepad UI navigation, customizable controller bindings, and Quick Access Menu (QAM) integrations.
- **Ultra-Low Latency & High Fidelity**: Hardware-accelerated decoding via **Vulkan**, **Metal**, **D3D11VA**, **VAAPI**, **VideoToolbox**, and **AMediaCodec** with full support for HDR, AV1, HEVC, and H.264 codecs.
- **Inbuilt QR Code Authentication**: Pair and sign in to PlayStation Network seamlessly using the built-in, offline QR code generator directly inside the Settings and Login dialogs.
- **Private Self-Hosted Token Relay**: Full support for self-hosting the authentication relay via `psstream-server` for zero third-party telemetry and complete network sovereignty.
- **Seamless Settings Migration**: Automatically discovers and inherits configurations from legacy configurations so previously registered consoles, keys, and bitrate settings carry over instantly.
- **Integrated In-App Updates**: Built-in background update checker (`UpdateManager`) in Settings with one-click download, automated package extraction, and stale file cleanup.

---

## Download

<p align="center">
  <a href="https://github.com/nileshchakraborty/AstraPlay/releases/latest"><img src="assets/google-play-badge.svg" height="48" alt="Get it on Google Play"></a>&nbsp;
  <a href="https://github.com/nileshchakraborty/AstraPlay/releases/latest/download/astraplay-windows-installer.exe"><img src="assets/windows-installer-badge.svg" height="48" alt="Download for Windows"></a>&nbsp;
  <a href="https://github.com/nileshchakraborty/AstraPlay/releases/latest"><img src="assets/app-store-badge.svg" height="48" alt="Download on the App Store"></a>
</p>
<p align="center">
  <a href="https://github.com/nileshchakraborty/AstraPlay/releases/latest"><img src="assets/mac-app-store-badge.svg" height="48" alt="Download on the Mac App Store"></a>&nbsp;
  <a href="https://github.com/nileshchakraborty/AstraPlay/releases/latest"><img src="assets/flathub-badge.svg" height="48" alt="Get it on Flathub"></a>&nbsp;
  <a href="https://github.com/nileshchakraborty/AstraPlay/releases/latest/download/AstraPlay-Linux-x86_64.AppImage"><img src="assets/linux-appimage-badge.svg" height="48" alt="Download AppImage"></a>
</p>

<p align="center">
  For full release notes, SHA-256 checksums, and all platform packages, visit the <a href="https://github.com/nileshchakraborty/AstraPlay/releases/latest"><b>Releases Page</b></a>.
</p>

---

## Installation Guide

AstraPlay supports flexible installation methods across every platform, matching standard application store channels and native package managers.

### ⚡ Quick 1-Line Automated Installers (Recommended)

Get running immediately with one command in Terminal or PowerShell. These automated installers verify checksums, configure system shortcuts, bypass quarantine/Gatekeeper friction, and enable in-app updates:

- **🍏 macOS (Apple Silicon & Intel — Zero Gatekeeper Warnings)**:
  ```bash
  /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/nileshchakraborty/AstraPlay/master/scripts/install-macos.sh)"
  ```
- **🐧 Linux, Steam Deck & Handhelds (Bazzite / SteamOS / Ubuntu / Fedora)**:
  ```bash
  curl -fsSL https://raw.githubusercontent.com/nileshchakraborty/AstraPlay/master/scripts/install-appimage.sh | bash
  ```
- **🪟 Windows (10 & 11 — PowerShell)**:
  ```powershell
  irm https://raw.githubusercontent.com/nileshchakraborty/AstraPlay/master/scripts/install-windows.ps1 | iex
  ```

---

## Detailed Platform Installation

### 🍏 macOS (Apple Silicon & Intel)

- **1-Line Terminal Installer (Recommended — Zero Gatekeeper Warnings)**:
  ```bash
  /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/nileshchakraborty/AstraPlay/master/scripts/install-macos.sh)"
  ```
  *Why this is recommended on macOS Sonoma & Sequoia*: Files downloaded through `curl` in Terminal are never quarantined by macOS Gatekeeper, avoiding *"Apple could not verify..."* errors completely. Installs to `/Applications`, applies local ad-hoc codesigning, and launches immediately.
- **In-App Direct Updates (v2.12.16+)**:
  AstraPlay includes a built-in background updater in **Settings → Check for Updates**. Updates download lightweight `.zip` application bundles, extract via `ditto` without disk image mount locks, automatically prompt for Touch ID elevation if `/Applications` permissions require it, and restart seamlessly with clean code signatures.
- **Direct Disk Image (`.dmg`)**:
  Download [`AstraPlay-macOS-arm64.dmg`](https://github.com/nileshchakraborty/AstraPlay/releases/latest) (Apple Silicon M1/M2/M3/M4+).
  - Open the DMG and drag `AstraPlay.app` into `Applications`.
  - If macOS displays *"Apple could not verify..."*, open **System Settings > Privacy & Security**, scroll down to **Security**, and click **Open Anyway**.
  - *Legacy Intel Mac Notice*: **v2.12.18** is the final release supporting Intel (`x86_64`) Macs. Intel users can download the final legacy binary from the [v2.12.18 Release](https://github.com/nileshchakraborty/AstraPlay/releases/tag/v2.12.18). All future releases exclusively target Apple Silicon.
- **Homebrew Cask (Recommended for macOS — Automated Gatekeeper Bypass)**:
  Install via our official Homebrew tap. Homebrew automatically removes the quarantine attribute during installation so AstraPlay runs out-of-the-box without Gatekeeper warnings:
  ```bash
  brew install --cask nileshchakraborty/astraplay/astraplay
  ```
  Or tap the repository first:
  ```bash
  brew tap nileshchakraborty/astraplay
  brew install --cask astraplay
  ```
- **Portable ZIP (In-App Updater Compatible)**:
  Download and extract `AstraPlay-macOS-arm64.zip` directly into `~/Applications`.

---

### 🐧 Linux, Steam Deck & Handhelds (Bazzite / ROG Xbox Ally X / Legion Go / Claw)

- **1-Command User Installer (Recommended for Handhelds & Desktop)**:
  Run our verified installer script in terminal / Konsole. It automatically detects your CPU architecture (`x86_64` or `aarch64`), verifies release SHA-256 checksums, installs desktop menu shortcuts with high-resolution icons, and enables clean in-app updates:
  ```bash
  curl -fsSL https://raw.githubusercontent.com/nileshchakraborty/AstraPlay/master/scripts/install-appimage.sh | bash
  ```
- **In-App Direct Updates**:
  AppImage installations update automatically inside the app via **Settings → Check for Updates** without requiring root privileges. Flatpak installations update via Discover or `flatpak update`.
- **Direct AppImage**:
  Download [`AstraPlay-Linux-x86_64.AppImage`](https://github.com/nileshchakraborty/AstraPlay/releases/latest/download/AstraPlay-Linux-x86_64.AppImage), mark it executable, and launch:
  ```bash
  chmod +x AstraPlay-Linux-x86_64.AppImage
  ./AstraPlay-Linux-x86_64.AppImage
  ```
- **Flathub / Flatpak**:
  Install via Flatpak from terminal or your desktop software store (Discover / GNOME Software):
  ```bash
  flatpak install flathub io.github.nileshchakraborty.AstraPlay
  flatpak run io.github.nileshchakraborty.AstraPlay
  ```
- **Portable ZIP with `launch.sh`**:
  Extract `astraplay-linux-x86_64.zip` and run `./launch.sh`. Bundled libraries and OpenSSL runtime environments are initialized automatically.
- **Steam Gaming Mode Setup (Steam Deck & Bazzite)**:
  1. Switch to **Desktop Mode**.
  2. Open **Steam** and click **Games → Add a Non-Steam Game to My Library...**
  3. Select **AstraPlay** (or browse to `~/.local/bin/astraplay` or your extracted `launch.sh`).
  4. Return to **Gaming Mode** — AstraPlay is now integrated into your Steam library with full gamepad controls and QAM support.

---

### 🪟 Windows (10 & 11)

- **1-Line PowerShell Installer (Recommended)**:
  ```powershell
  irm https://raw.githubusercontent.com/nileshchakraborty/AstraPlay/master/scripts/install-windows.ps1 | iex
  ```
- **In-App Direct Updates**:
  Installed setups update automatically via **Settings → Check for Updates** with native UAC elevation and Mark-of-the-Web unblocking.
- **Setup Installer (`.exe`)**:
  Download and run [`astraplay-windows-installer.exe`](https://github.com/nileshchakraborty/AstraPlay/releases/latest/download/astraplay-windows-installer.exe) (or `astraplay-windows-arm64-installer.exe` for Snapdragon / Windows on ARM devices). Follow the setup wizard to customize Start Menu and Desktop shortcuts.
- **Windows Package Manager (`winget`)**:
  ```powershell
  winget install AstraPlay
  ```
- **Portable ZIP**:
  Download and extract [`astraplay-windows-x64.zip`](https://github.com/nileshchakraborty/AstraPlay/releases/latest), then launch `AstraPlay.exe` directly without administrative privileges.

---

### 📱 Android & Android TV

- **Google Play Store**:
  Install directly from the [Google Play Store](https://github.com/nileshchakraborty/AstraPlay/releases/latest) for automatic background updates across phones, tablets, and Android TV / Google TV devices.
- **Direct APK Sideload**:
  Download the latest [`AstraPlay-release.apk`](https://github.com/nileshchakraborty/AstraPlay/releases/latest) from GitHub Releases and open it on your device to install.
- **Android TV / Fire TV Wireless Sideloading (ADB)**:
  Install to your television or streaming device over your local network:
  ```bash
  adb connect <android-tv-ip>:5555
  adb install -r AstraPlay-release.apk
  ```

---

### 🍎 iOS & iPadOS

- **Apple App Store / TestFlight**:
  Download via the [Apple App Store](https://github.com/nileshchakraborty/AstraPlay/releases/latest) or join the TestFlight community channel.
- **IPA Sideloading**:
  Download [`AstraPlay.ipa`](https://github.com/nileshchakraborty/AstraPlay/releases/latest) and install using your preferred sideloading manager:
  - **AltStore / SideStore**: Add the IPA directly through the on-device AltStore manager.
  - **Sideloadly**: Connect your iPhone/iPad to a PC or Mac and install with automatic signing.
  - **LiveContainer / TrollStore**: Install the IPA directly without weekly resign requirements (iOS 14.0–17.0).

---

## Downloads & Supported Packages

All release binaries, packages, and SHA-256 checksums are published on the [**GitHub Releases Page**](https://github.com/nileshchakraborty/AstraPlay/releases/latest).

| Operating System | Architecture | Available Packages | Description |
| :--- | :--- | :--- | :--- |
| **Windows** | **x86_64** (Desktop, ROG Xbox Ally X) | [`.exe Setup Installer`](https://github.com/nileshchakraborty/AstraPlay/releases/latest)<br>[`.msi WiX Installer`](https://github.com/nileshchakraborty/AstraPlay/releases/latest)<br>[`.zip Portable`](https://github.com/nileshchakraborty/AstraPlay/releases/latest)<br>[`Standalone .exe`](https://github.com/nileshchakraborty/AstraPlay/releases/latest) | Full Inno Setup installer (`astraplay-windows-installer.exe`) with Start Menu and desktop shortcut choices, WiX MSI package (`astraplay-windows-x64.msi`), portable runtime ZIP, or standalone executable. |
| **Windows** | **ARM64** (Snapdragon X Elite, WoA) | [`.exe Setup Installer (ARM64)`](https://github.com/nileshchakraborty/AstraPlay/releases/latest)<br>[`.zip Portable (ARM64)`](https://github.com/nileshchakraborty/AstraPlay/releases/latest)<br>[`Standalone .exe (ARM64)`](https://github.com/nileshchakraborty/AstraPlay/releases/latest) | Native Windows on ARM Inno Setup installer (`astraplay-windows-arm64-installer.exe`), portable ARM64 ZIP, or standalone ARM64 executable. |
| **macOS** | **Apple Silicon** (`arm64`: M1–M4) | [`.zip Installer (arm64)`](https://github.com/nileshchakraborty/AstraPlay/releases/latest)<br>[`.dmg Disk Image (arm64)`](https://github.com/nileshchakraborty/AstraPlay/releases/latest)<br>[`.zip App Bundle (arm64)`](https://github.com/nileshchakraborty/AstraPlay/releases/latest) | Native Apple Silicon installer archive with Touch ID authorization (`AstraPlay-macOS-arm64-installer.zip`), `.dmg` disk image, and portable app bundle. |
| **macOS** | **Intel** (`x86_64`) — *Final Release* | [`.zip Installer (x86_64)`](https://github.com/nileshchakraborty/AstraPlay/releases/tag/v2.12.18)<br>[`.dmg Disk Image (x86_64)`](https://github.com/nileshchakraborty/AstraPlay/releases/tag/v2.12.18)<br>[`.zip App Bundle (x86_64)`](https://github.com/nileshchakraborty/AstraPlay/releases/tag/v2.12.18) | Native Intel x86_64 installer archive with Touch ID authorization (`AstraPlay-macOS-x86_64-installer.zip`), `.dmg` disk image, and portable app bundle (final release v2.12.18). |
| **Linux & Handhelds** | **x86_64** (Steam Deck, ROG Ally, PC) | [`.AppImage (x86_64)`](https://github.com/nileshchakraborty/AstraPlay/releases/latest)<br>[`.zip Portable (x86_64)`](https://github.com/nileshchakraborty/AstraPlay/releases/latest) | Portable AppImage executable and Steam-ready ZIP archive with `launch.sh` for Steam Deck and Bazzite Gaming Mode. |
| **Linux & Handhelds** | **ARM64** (`aarch64`: Raspberry Pi 5, SBCs) | [`.AppImage (aarch64)`](https://github.com/nileshchakraborty/AstraPlay/releases/latest)<br>[`.zip Portable (aarch64)`](https://github.com/nileshchakraborty/AstraPlay/releases/latest) | Native ARM64 AppImage executable and portable archive. |
| **Android** | **Universal / ARM64 & x86** (Phones, Tablets, TV) | [`.apk Package`](https://github.com/nileshchakraborty/AstraPlay/releases/latest)<br>[`.aab Play Store Bundle`](https://github.com/nileshchakraborty/AstraPlay/releases/latest) | Sideloadable release APK and Google Play Store App Bundle (`io.github.nileshchakraborty.astraplay`), with native binaries for `arm64-v8a`, `armeabi-v7a`, `x86_64`, and `x86`. |
| **iOS / iPadOS** | **ARM64** (iPhone & iPad) | [`.ipa Package`](https://github.com/nileshchakraborty/AstraPlay/releases/latest) | Sideloadable unsigned IPA (AltStore, Sideloadly, LiveContainer, TrollStore) and TestFlight. |

The Flathub listing is not published yet. For Steam Deck or Bazzite, use the [AppImage installer script](scripts/install-appimage.sh) to add AstraPlay to the desktop application menu, then add it as a non-Steam game for Gaming Mode. The portable ZIP and its `launch.sh` are another option. See the [Linux installation guide](docs/setup/installation.md) for steps.

The Windows `.exe` setup asks whether to create Start Menu and desktop shortcuts (Start Menu is selected by default). The MSI creates a Start Menu shortcut by default; administrators can opt out or add a desktop shortcut with `msiexec /i astraplay-windows-x64.msi CREATE_START_MENU_SHORTCUT=0 CREATE_DESKTOP_SHORTCUT=1`. The standalone `.exe` and portable ZIP do not run an installer or create shortcuts. After installation, launch AstraPlay and use **Pin to taskbar** from its taskbar icon if desired; Windows requires that choice to be made by the user.

Windows MSIX packages are being prepared for Microsoft Store distribution; they are not yet a published install option. The on-demand development MSIX is unsigned and intended only for build validation. Store-distributed MSIX updates are managed by Windows; the existing GitHub EXE/MSI installers retain their in-app updater. See [Windows MSIX packaging](packaging/windows-msix/README.md) for the preparation and validation steps.

---

## Features & Capabilities

### Console Discovery & Remote Play
- **Local Network Broadcast**: Automatic discovery of PS4 and PS5 consoles on local networks via standard discovery protocols, with active multi-interface subnet broadcast routing on macOS, Linux, and Windows.
- **PIN Pairing**: Fast pairing using the official PlayStation Remote Play registration PIN.
- **Internet Remote Play**: Connect to your home consoles over WAN and mobile networks with port forwarding or UPnP.

### Video & Audio Performance
- **Codec Support**: AV1, HEVC (H.265), and H.264 streaming up to 1080p @ 60 FPS (and PS5 Pro dynamic resolutions).
- **Hardware Decoders**:
  - Windows: D3D11VA, Direct3D12, Vulkan
  - macOS & iOS: Apple VideoToolbox (Metal-backed)
  - Linux: VAAPI, VDPAU, Vulkan Video
  - Android: Android MediaCodec (AMediaCodec NDK)
- **Contrast Adaptive Sharpening (CAS) & Upscaling**: In-stream sharpness quick toggle on Desktop QML HUD supporting FidelityFX CAS and EWA Lanczos Sharp shaders.
- **Aspect Ratio Presets**: Flexible aspect ratio controls across Desktop, Android, and iOS supporting **Fit (16:9)**, **Fill / 16:10** (ideal for Steam Deck and handhelds), **20:9** (modern smartphones), **Zoom**, and **Stretch**.
- **Dynamic Bitrate & Congestion Control**: Adaptive bitrate scaling heuristics in `libchiaki` that react in real-time to network congestion, packet drops, and RTT spikes to eliminate stuttering.
- **Audio Engines & Latency Tuning**: Low-latency audio with SDL, Oboe (Android), CoreAudio (Apple), and PulseAudio / PipeWire (Linux). Configurable audio latency buffer size tuning (Ultra-Low, Low, Balanced, High) across mobile and desktop.
- **In-Stream Microphone Mute**: Quick toggle to mute or unmute microphone pass-through on the fly during active streaming sessions.

### Controller & Input Support
- **DualSense & DualShock 4**: Full support for haptic feedback, adaptive triggers, motion sensors (gyro/accelerometer), and touchpad navigation.
- **Controller Battery Indicator**: Real-time battery status HUD overlay displaying charge level and state (Charging, Low, Normal) on Desktop, Android, and iOS.
- **Xbox & Handheld Controllers**: Native mapping for Xbox wireless controllers, ROG Ally controls, Steam Deck controls, and third-party XInput / DirectInput controllers with deadzone calibration, stick sensitivity curves, and button remapping.
- **Tactile Touch Haptics**: Virtual thumbstick haptic feedback using device vibration motors on Android and iOS when crossing stick thresholds or clicking virtual L3/R3.
- **Customizable Virtual Touch Overlay**: Drag-and-drop repositioning with layout persistence, adjustable touch opacity and scale sliders, genre layout presets (Standard, Action/Shooter, RPG), and phone vibrator fallback for mobile grips.
- **Touchpad Emulation**: Dedicated DualSense touchpad gesture and tap emulation for mobile controller grips and touch overlays.
- **Android App Shortcuts**: Quick 1-tap launcher shortcuts on Android home screens directly into Remote Play and Cloud streaming sessions.

---

## Technical Documentation & Architecture

Comprehensive architectural and design documents are available in the repository root:

- [**PRD.md**](PRD.md) — Product Requirements Document, platform roadmap, and UX specifications
- [**ARCHITECTURE.md**](ARCHITECTURE.md) — Subsystem architecture, cross-platform bridging, and data flow
- [**DESIGN.md**](DESIGN.md) — Design systems, UI components, and handheld ergonomics
- [**DATA.md**](DATA.md) — Configuration persistence, token management, and data contracts
- [**SECURITY.md**](SECURITY.md) — Security guardrails, cryptographic keys, and credential boundaries
- [**CODE_STYLE.md**](CODE_STYLE.md) — Engineering standards for C++, Qt/QML, Kotlin, Swift, and CMake
- [**TESTING.md**](TESTING.md) — Testing methodologies, automation suites, and device verification
- [**AGENTS.md**](AGENTS.md) — Developer operating guide and CI/CD policies

---

## Building from Source

### Prerequisites
- **CMake** 3.25+
- **Ninja** build system
- **Python** 3.10+ (with `protobuf`)
- **Qt 6.6+** (`Core`, `Gui`, `Quick`, `Qml`, `Network`, `Svg`, `Widgets`)
- **FFmpeg 6.0+**
- **SDL 2.0+**

### Linux / macOS Desktop
```bash
git clone --recursive https://github.com/nileshchakraborty/AstraPlay.git
cd astraplay
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Windows (MSYS2 MinGW64 / ClangARM64)
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Android Build
```bash
cd android
./gradlew assembleRelease
# Or use the local automated build helper:
bash build-local.sh
```

### iOS / iPadOS Build
```bash
bash ios/build.sh
```

---

## Self-Hosted Token Relay (`psstream-server`)

If you prefer to authenticate PlayStation Network accounts entirely within your local network without external cloud services:

```bash
cd psstream-server
npm install
npm start
```

The server provides a local web UI on port `25525` to log in to PSN, capture authorization tokens, and relay them to AstraPlay securely.

---

## Responsible Use

AstraPlay is intended for use with games and hardware you legally own and are licensed to access. It does not circumvent digital rights management (DRM), distribute proprietary firmware, or facilitate piracy.

AstraPlay is an independent community project and is not affiliated with, endorsed by, or certified by Sony Interactive Entertainment Inc. PlayStation, PS4, PS5, DualSense, and DualShock are trademarks or registered trademarks of Sony Interactive Entertainment Inc.

---

## Credits & Acknowledgments

AstraPlay is built upon the foundational work of [Chiaki](https://git.sr.ht/~thestr4ng3r/chiaki) and [chiaki-ng](https://github.com/streetpea/chiaki-ng). Heartfelt thanks go to the original Chiaki authors, the chiaki-ng maintainers, and open-source contributors across the handheld gaming ecosystem.
