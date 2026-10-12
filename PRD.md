# AstraPlay — Product Requirements Document (PRD)

## 1. Overview

**AstraPlay** is a free, open-source, community-maintained PS4 and PS5 Remote Play + Internet/Cloud Play client targeting Android, Android TV, iOS, iPadOS, macOS, Windows, Linux, and Steam Deck. It is forked from [chiaki-ng](https://github.com/streetpea/chiaki-ng) and extends that work with app-store distribution, mobile-first UI, and Internet Play (game catalog streaming).

**License:** AGPL-3.0  
**Current Version:** 2.12.0  
**Organization:** nileshchakraborty

---

## 2. Goals

| # | Goal |
|---|------|
| G1 | Allow any user to stream their PS4/PS5 games to any supported device with minimal setup |
| G2 | Support both **Remote Play** (local LAN + WAN) and **Internet Play** (PSN cloud game catalog) |
| G3 | Distribute through official app stores (Google Play, App Store, Mac App Store, Flathub) |
| G4 | Provide a touch-friendly mobile UI for phones/tablets alongside a controller-optimized desktop UI |
| G5 | Maintain full upstream (chiaki-ng) cherry-pick compatibility |

---

## 3. Non-Goals

- Does not circumvent copy protection or facilitate piracy
- Does not manage PlayStation accounts or replace official PSN services
- Does not provide multiplayer/party features beyond what the PS console supports
- Does not stream PC games

---

## 4. User Personas

### P1 — Console Gamer on Mobile
Plays PS5 at home but wants to continue in bed on their phone or on the go. Needs simple onboarding, good touch controls, and low-latency streaming over Wi-Fi.

### P2 — Android TV / Big-Screen User
Uses a Fire TV Stick or NVIDIA Shield to stream from their console to the living-room TV. Needs controller support and leanback-friendly UI.

### P3 — Steam Deck / Desktop Linux Player
Wants to play their PlayStation catalog from a portable Linux device. Needs Flathub distribution, good controller mapping, and Steam integration.

### P4 — Internet Play User
Does not own a PS console or wants to play from a remote network. Subscribes to PlayStation Plus and uses the cloud catalog to stream titles directly.

### P5 — Developer / Contributor
Builds on AstraPlay to add features, port to new platforms, or cherry-pick upstream fixes. Needs a clear C library boundary, documented build scripts, and safe merge patterns.

---

## 5. Features

### 5.1 Core Streaming
| ID | Feature | Priority |
|----|---------|----------|
| F1 | Low-latency PS4/PS5 Remote Play over LAN | P0 |
| F2 | Remote Play over Internet (RUDP / hole-punch) | P0 |
| F3 | Internet/Cloud Play — PSN game catalog streaming (Kamaji / Gaikai) | P0 |
| F4 | H.264 / H.265 / AV1 hardware-accelerated video decoding (FFmpeg) | P0 |
| F5 | Opus audio decode/encode | P0 |
| F6 | Forward Error Correction (FEC) for packet loss recovery | P1 |
| F7 | Congestion control & adaptive dynamic bitrate scaling | P1 |
| F7a | Audio latency buffer size tuning (Ultra-Low to High) | P1 |
| F7b | In-stream microphone pass-through mute/unmute | P1 |

### 5.2 Input & Haptics
| ID | Feature | Priority |
|----|---------|----------|
| F8 | DualSense / DualShock 4 controller pass-through | P0 |
| F9 | Gyroscope / IMU orientation forwarding | P1 |
| F10 | DualSense haptic audio (PS5 Remote Play rumble) | P1 |
| F11 | On-screen touch overlay controls with drag-and-drop customization | P0 |
| F11a | Virtual thumbstick tactile touch haptic feedback | P1 |
| F11b | DualSense touchpad touch/gesture emulation for mobile grips | P1 |
| F12 | SDL gamepad support (desktop) with deadzone & sensitivity calibration | P1 |
| F13 | libsetsu touchpad input from controller (Linux) | P2 |
| F14 | Steam Deck native gyro + haptics | P2 |
| F14a | Controller battery indicator HUD across all platforms | P1 |

### 5.3 Account & Discovery
| ID | Feature | Priority |
|----|---------|----------|
| F15 | PSN OAuth login (NPSSO token acquisition) | P0 |
| F16 | Automatic console discovery (LAN broadcast) | P0 |
| F17 | Manual console registration with auto-filled Account ID | P1 |
| F18 | PSStream server relay for web-based PSN login (mobile-friendly) | P1 |
| F19 | Steamworks overlay for PSN login (desktop Steam) | P2 |
| F19a | Android App Shortcuts for 1-tap console & catalog launch | P2 |

### 5.4 Platform Distribution
| ID | Feature | Priority |
|----|---------|----------|
| F20 | Google Play (Android + Android TV) — production + beta tracks | P0 |
| F21 | Apple App Store + TestFlight (iOS / iPadOS) | P0 |
| F22 | Mac App Store & DMG/Zip (macOS universal) | P0 |
| F23 | Flathub (Linux x86_64 + aarch64) | P0 |
| F24 | Windows Inno Setup installer, MSI, MSIX, and portable zip (x86_64 & ARM64) | P1 |
| F25 | Linux AppImage | P1 |

### 5.5 Desktop UI (Qt)
| ID | Feature | Priority |
|----|---------|----------|
| F26 | QML-based main window, settings, game launcher | P0 |
| F27 | Stream quality configuration (resolution, FPS, bitrate, codec) | P0 |
| F27a | In-stream aspect ratio presets (Fit, Fill / 16:10, 20:9, Zoom, Stretch) | P1 |
| F27b | In-stream Contrast Adaptive Sharpening (CAS / Lanczos) toggle | P1 |
| F27c | In-stream Performance Stats & Battery Indicator HUD overlay | P1 |
| F28 | Controller keyboard navigation (Qt::TabFocusAllControls) | P1 |
| F29 | Steam shortcut creation | P2 |

---

## 6. User Stories

- **US1:** As a mobile user I can log into PSN by entering a short code shown in the app on any web browser, so I do not need to type a URL on my phone.
- **US2:** As a Steam Deck user I can launch AstraPlay from my Steam library and start streaming my PS5 without leaving Gaming Mode.
- **US3:** As an Internet Play subscriber I can browse and launch PSN catalog games without owning a PS console.
- **US4:** As a developer I can cherry-pick a fix from chiaki-ng without merge conflicts because platform-specific code is isolated.

---

## 7. Out-of-Scope (v2.x)

- Screen recording or broadcasting of streamed content
- Cloud saves or trophy management
- Voice chat
- In-app purchases beyond iOS IAP groundwork (`CHIAKI_IS_MAC_APPSTORE`)

---

## 8. Success Metrics

| Metric | Target |
|--------|--------|
| Stream latency (LAN) | < 30 ms |
| App crash-free rate | > 99% |
| App Store rating | ≥ 4.0 |
| Google Play rating | ≥ 4.0 |
| CI build success rate | > 95% per platform |
| Time from release PR merge to GitHub Release | < 30 min (target) |

---

## 9. Release Cadence

- **Integration** — feature, fix, and documentation PRs merge into `dev`; this does not publish a release
- **Release** — a release PR from `dev` to `master` triggers the platform build and GitHub Release workflow when merged
- **Stores** — deployment uses the dedicated deploy workflows or manually dispatched `release-all.yml`

---

## 10. Stakeholders

| Role | Party |
|------|-------|
| Owner / Maintainer | nileshchakraborty |
| Platform reviewers | Google Play, Apple App Store, Flathub |
| Community contributors | Open-source contributors via GitHub PRs |
| Upstream | chiaki-ng (streetpea), Chiaki (thestr4ng3r) |
