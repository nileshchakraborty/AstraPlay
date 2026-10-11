# Pylux — Product Requirements Document (PRD)

## 1. Overview

**Pylux** is a free, open-source, community-maintained PS4 and PS5 Remote Play + Internet/Cloud Play client targeting Android, Android TV, iOS, iPadOS, macOS, Windows, Linux, and Steam Deck. It is forked from [chiaki-ng](https://github.com/streetpea/chiaki-ng) and extends that work with app-store distribution, mobile-first UI, and Internet Play (game catalog streaming).

**License:** AGPL-3.0  
**Current Version:** 2.11.4  
**Organization:** ForWard Technologies LLC

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
Builds on Pylux to add features, port to new platforms, or cherry-pick upstream fixes. Needs a clear C library boundary, documented build scripts, and safe merge patterns.

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
| F7 | Congestion control & adaptive quality | P1 |

### 5.2 Input & Haptics
| ID | Feature | Priority |
|----|---------|----------|
| F8 | DualSense / DualShock 4 controller pass-through | P0 |
| F9 | Gyroscope / IMU orientation forwarding | P1 |
| F10 | DualSense haptic audio (PS5 Remote Play rumble) | P1 |
| F11 | On-screen touch overlay controls (mobile) | P0 |
| F12 | SDL gamepad support (desktop) | P1 |
| F13 | libsetsu touchpad input from controller (Linux) | P2 |
| F14 | Steam Deck native gyro + haptics | P2 |

### 5.3 Account & Discovery
| ID | Feature | Priority |
|----|---------|----------|
| F15 | PSN OAuth login (NPSSO token acquisition) | P0 |
| F16 | Automatic console discovery (LAN broadcast) | P0 |
| F17 | Manual console registration | P1 |
| F18 | PSStream server relay for web-based PSN login (mobile-friendly) | P1 |
| F19 | Steamworks overlay for PSN login (desktop Steam) | P2 |

### 5.4 Platform Distribution
| ID | Feature | Priority |
|----|---------|----------|
| F20 | Google Play (Android + Android TV) — production + beta tracks | P0 |
| F21 | Apple App Store + TestFlight (iOS / iPadOS) | P0 |
| F22 | Mac App Store (macOS universal) | P0 |
| F23 | Flathub (Linux x86_64 + aarch64) | P0 |
| F24 | Windows installer + portable zip (x86_64) | P1 |
| F25 | Linux AppImage | P1 |

### 5.5 Desktop UI (Qt)
| ID | Feature | Priority |
|----|---------|----------|
| F26 | QML-based main window, settings, game launcher | P0 |
| F27 | Stream quality configuration (resolution, FPS, bitrate, codec) | P0 |
| F28 | Controller keyboard navigation (Qt::TabFocusAllControls) | P1 |
| F29 | Steam shortcut creation | P2 |
| F30 | Donation prompt (non-App-Store builds) | P3 |

---

## 6. User Stories

- **US1:** As a mobile user I can log into PSN by entering a short code shown in the app on any web browser, so I do not need to type a URL on my phone.
- **US2:** As a Steam Deck user I can launch Pylux from my Steam library and start streaming my PS5 without leaving Gaming Mode.
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
| Time-to-beta from PR merge | < 30 min (automated) |

---

## 9. Release Cadence

- **Beta** — triggered on merge to `release/beta`; all platforms auto-deploy
- **Production** — manual dispatch on `master`; Android promotes to production track, Apple platforms auto-submit for review

---

## 10. Stakeholders

| Role | Party |
|------|-------|
| Owner / Maintainer | ForWard Technologies LLC |
| Platform reviewers | Google Play, Apple App Store, Flathub |
| Community contributors | Open-source contributors via GitHub PRs |
| Upstream | chiaki-ng (streetpea), Chiaki (thestr4ng3r) |
