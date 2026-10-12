# AstraPlay Release Notes

## Version 2.12.0

### Overview
AstraPlay 2.12.0 is a major release introducing end-to-end rebranding from Pylux/Chiaki to AstraPlay, critical network and registration reliability improvements, audio and video enhancements, rich controller HUD integrations, and expanded installer options for Windows and handheld devices.

---

### Highlights & What's New

#### 🎮 Input & Controller Features
- **Controller Battery Indicator HUD**:
  - Live battery level indicator on the in-stream HUD across Desktop (QML), Android, and iOS.
  - Displays real-time battery status (`Charging`, `Low`, `Normal`) and percentage for connected controllers (DualSense, DualShock 4, Xbox Wireless, and HID gamepads).
- **Tactile Touch Haptics for Virtual Thumbsticks**:
  - Haptic feedback when crossing virtual thumbstick boundaries or activating virtual L3/R3 clicks on touch devices (Android & iOS).
- **Customizable Virtual Touch Controls**:
  - Drag-and-drop repositioning with layout persistence across app restarts.
  - Virtual controller opacity slider and scale factor adjustments.
  - Quick genre presets (`Standard`, `Action/Shooter`, `RPG`).
  - Phone vibration motor fallback for mobile grips without built-in rumble motors.
- **DualSense Touchpad Emulation**:
  - Full touchpad gesture and tap emulation for mobile controller grips and touch overlays.
- **Stick Deadzone & Sensitivity Tuning**:
  - Granular deadzone calibration, stick sensitivity curves, and custom button remapping across all platforms.

#### 📺 Video & Rendering Improvements
- **In-Stream Aspect Ratio Presets**:
  - Multi-ratio scaling options: **Fit (16:9)**, **Fill / 16:10** (ideal for Steam Deck and ROG Xbox Ally X), **20:9** (modern smartphones), **Zoom**, and **Stretch**.
  - Accessible via dedicated in-stream HUD controls across Android, iOS, and Desktop.
- **Contrast Adaptive Sharpening (CAS)**:
  - In-stream sharpness quick toggle on Desktop QML HUD supporting AMD FidelityFX CAS and EWA Lanczos Sharp shaders for pristine upscaling.
- **Dynamic Bitrate Scaling & Congestion Control**:
  - Real-time network heuristics in `libchiaki` measuring round-trip time (RTT), packet loss, and frame drop ratios.
  - Automatically modulates video bitrate during packet congestion to maintain fluid 60 FPS streaming without stuttering or audio dropouts.

#### 🔊 Audio Enhancements
- **Audio Latency Buffer Size Tuning**:
  - User-configurable audio buffer sizing (`Ultra-Low`, `Low`, `Balanced`, `High`) across Android, iOS, and Desktop to trade buffer margin for lowest possible input-to-sound latency.
- **In-Stream Microphone Mute Toggle**:
  - One-tap microphone pass-through mute/unmute directly from the in-stream HUD on mobile and desktop.

#### 🌐 Connectivity, Registration & Networking
- **Streamlined Manual Console Registration**:
  - Automatically pre-populates Account ID when user is already logged in to PSN, removing redundant NPSSO token prompts.
- **PlayStation Push Notification WebSocket Fix**:
  - Resolved SSL handshake verification failures on macOS by utilizing the native system root certificate store for curl WebSocket handshakes.
  - Added fallback verification handling for Sony push notification relay servers with non-standard intermediate CA certificates.
- **Android App Shortcuts**:
  - Long-press home screen app shortcuts for 1-tap launching directly into registered Remote Play consoles and Cloud streaming catalog.

#### 💻 Platform & Packaging Polish
- **macOS Native Branding**:
  - Renamed executable bundle output to `AstraPlay.app` and canonical configuration path to `astraplay.conf` with automatic migration from legacy `pylux.conf` and `Chiaki.conf`.
- **Windows Packaging Enhancements**:
  - Fixed Windows CI binary artifact preservation (`chiaki.exe` and `astraplay.exe`).
  - Complete WiX MSI installer (`astraplay-windows-x64.msi`) with configurable Start Menu and desktop shortcut parameters.
  - Development and Store-ready MSIX packaging options.

---

## System Requirements

### All Systems
- GPU or integrated graphics supporting Vulkan 1.2 or higher, DirectX 11/12, Metal, or OpenGL 4.1+.
- PlayStation 4 (System Software 7.00+) or PlayStation 5 (System Software 2.00+).
- Wired Ethernet connection recommended for the console; 5 GHz Wi-Fi or wired connection recommended for the client.

### Linux & Steam Deck
- PipeWire (recommended for low-latency audio and DualSense haptics) or PulseAudio.
- `libva` / VAAPI for hardware video decoding.
- Flathub release or portable AppImage.

### macOS
- macOS 12 Monterey or newer (Universal binary supporting Apple Silicon M1–M4 and Intel x86_64).
- Metal / Apple VideoToolbox hardware acceleration.

### Windows
- Windows 10 (1903+) or Windows 11 (x86_64 or ARM64).
- D3D11VA, D3D12, or Vulkan video decoders.

### Android
- Android 8.0 (Oreo) or higher.
- ARM64-v8a or x86_64 processor recommended.

### iOS / iPadOS
- iOS 15.0 or iPadOS 15.0 or newer.

---

## Where to Download

Release binaries, installers, and checksums are available on the [GitHub Releases Page](https://github.com/nileshchakraborty/AstraPlay/releases/latest).

- **Windows**: Inno Setup installer (`.exe`), WiX MSI (`.msi`), portable `.zip`, standalone `.exe`.
- **macOS**: DMG disk images and `.app.zip` bundles for Apple Silicon and Intel.
- **Linux / Steam Deck**: Flatpak on Flathub, portable `.AppImage`, and Steam-ready ZIP.
- **Android**: Google Play Store (`io.github.nileshchakraborty.astraplay`) and standalone release APK.
- **iOS / iPadOS**: App Store and IPA package.
