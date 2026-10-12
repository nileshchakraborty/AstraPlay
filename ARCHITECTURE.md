# AstraPlay — Architecture Document

## 1. High-Level Overview

AstraPlay follows a **shared-core, platform-shell** architecture. All protocol logic lives in a shared C library. Platform layers are thin adapters that map the library's callbacks to platform-native audio, video, input, and UI systems.

```
┌────────────────────────────────────────────────────────────┐
│                     Platform Shells                        │
│  ┌──────────┐ ┌───────┐ ┌──────────┐ ┌─────────────────┐ │
│  │ Android  │ │  iOS  │ │ Qt (desk)│ │ Switch (future) │ │
│  │ (Kotlin/ │ │(Swift/│ │ (C++/QML)│ │  (Borealis/C++) │ │
│  │  Java)   │ │ObjC)  │ │          │ │                 │ │
│  └────┬─────┘ └───┬───┘ └────┬─────┘ └────────┬────────┘ │
│       │           │          │                 │           │
│       └───────────┴──────────┴─────────────────┘           │
│                        JNI / ObjC / C++ FFI                │
└────────────────────────────────────────────────────────────┘
                             │
          ┌──────────────────▼──────────────────┐
          │       lib/ — Shared C Library        │
          │  (chiaki core + AstraPlay extensions)    │
          └──────────────────────────────────────┘
                             │
         ┌───────────────────┼──────────────────┐
         ▼                   ▼                  ▼
   PlayStation         PSN Cloud          psstream-server
   console (LAN/WAN)   (Kamaji/Gaikai)    (Node.js relay)
```

---

## 2. Component Map

### 2.1 `lib/` — Shared C Library

The heart of AstraPlay. Must be the single source of truth for all protocol behavior.

| Module | File(s) | Responsibility |
|--------|---------|---------------|
| Session | `session.c`, `session.h` | PS4/PS5 Remote Play session lifecycle |
| Ctrl | `ctrl.c`, `ctrl.h` | Control channel (commands, keep-alive) |
| Takion | `takion.c` | A/V packet transport layer (custom UDP) |
| Stream Connection | `streamconnection.c` | Full duplex stream setup |
| Congestion Control | `congestioncontrol.c`, `congestioncontrol.h` | Real-time adaptive dynamic bitrate scaling & RTT monitoring |
| Audio | `audio.c`, `audiosender.c`, `audiorecv.c` | Opus encode/decode pipeline, configurable latency buffer sizes |
| Video | `videoreceiver.c`, `frameprocessor.c`, `ffmpegdecoder.c` | Video receive, FEC reassembly, decode |
| FEC | `fec.c` | Forward Error Correction (Jerasure/Reed-Solomon) |
| Controller | `controller.c` | DualSense / DS4 state encoding, battery level & charging state |
| Feedback | `feedback.c`, `feedbacksender.c` | Haptic and rumble feedback |
| Discovery | `discovery.c`, `discoveryservice.c` | LAN console discovery (UDP broadcast) |
| Registration | `regist.c` | Console PIN registration |
| Remote (RUDP) | `remote/` | Internet Play over RUDP & push notification WebSockets |
| Cloud Catalog | `cloudcatalog_*.c` | PSN game catalog fetch, cache, merge |
| Cloud Session | `cloudsession.c`, `cloudsession_kamaji.c`, `cloudsession_gaikai.c` | Internet Play session types |
| Cloud Ping | `cloudsession_ping.c` | Latency probing for server selection |
| Crypto | `rpcrypt.c`, `gkcrypt.c`, `ecdh.c` | Session encryption |
| HTTP | `http.c`, `curl_http.c` | libcurl-based HTTP client & secure WebSocket client |
| Log | `log.c`, `log.h` | Structured log with level filtering |
| iOS Bridge | `ios_bridge_helpers.c` | `CHIAKI_EXPORT` wrappers — **only access point for iOS into struct internals** |

> [!IMPORTANT]
> Never call `static inline` chiaki functions from iOS ObjC/Swift code. Struct layouts differ between CMake-compiled lib and Xcode-compiled ObjC. Use `_ex` wrappers in `ios_bridge_helpers.c`.

### 2.2 `gui/` — Qt Desktop Shell (macOS, Windows, Linux, Steam Deck)

| Sub-path | Responsibility |
|----------|---------------|
| `src/main.cpp` | Entry point, Qt::TabFocusAllControls, app init |
| `src/qmlbackend.cpp` | QML↔C++ bridge: session control, PSN auth, catalog |
| `src/qmlmainwindow.cpp` | Main window, fullscreen, HiDPI |
| `src/settings.cpp` | Persistent settings (QSettings) |
| `src/controllermanager.cpp` | SDL gamepad enumeration and events |
| `src/discoverymanager.cpp` | Wraps `lib/discoveryservice` |
| `src/cloudcatalogbackend.cpp` | Game catalog for QML |
| `src/cloudstreamingbackend.cpp` | Internet Play session for QML |
| `src/psntoken.cpp`, `psnaccountid.cpp` | PSN auth flows |
| `src/steamworks/` | Steamworks overlay (optional, `CHIAKI_ENABLE_STEAMWORKS`) |
| `src/qml/` | QML UI (settings, stream, launcher, dialogs) |
| `include/steamworks/` | `SteamworksWrapper` header |

### 2.3 `android/` — Android Shell

| Path | Responsibility |
|------|---------------|
| `app/src/main/java/…` | Kotlin/Java Activity, Fragment, ViewModels |
| `app/src/main/cpp/` | JNI glue to `lib/` |
| `app/src/main/res/` | Layouts, strings, drawables |
| Gradle + CMake | Build system (AAB, ABI splits: arm64-v8a, x86_64, armeabi-v7a, x86) |

### 2.4 `ios/` — iOS Shell

| Path | Responsibility |
|------|---------------|
| `ios/AstraPlay/` | Swift / ObjC source |
| `ios/AstraPlay.xcodeproj/` | Xcode project |
| `ios/CMakeLists.txt` | C lib build for iOS (arm64) |
| `ios/build.sh` | Build script (`dev`, `iterate`, `launch`, `logs`, `stop-logs`) |

> [!WARNING]
> Always allocate `ChiakiSession` via `chiaki_session_get_sizeof()`, **not** `sizeof(ChiakiSession)`. Xcode does not compile the full CMake config, so struct sizes differ.

### 2.5 `psstream-server/` — PSN Login Relay

A lightweight Node.js / Express server used as a one-time-code relay so mobile users can authenticate to PSN from a web browser without typing a URL.

```
Client App           psstream-server          Browser (user)
─────────────────────────────────────────────────────────────
POST /astraplay/create-code {code}
                     store code → npsso: null
                     ─────────────────────────►
                                              User enters code at /login
                                              PSN OAuth → npsso token
                     ◄─────────────────────────
                     store npsso for code
POST /astraplay/get-tokens {code}
                     return npsso, delete code
Token received ◄────
```

| File | Purpose |
|------|---------|
| `server.js` | Express app; `/astraplay/create-code`, `/astraplay/get-tokens`, `/astraplay/validate-code`, `/astraplay/submit-token` |
| `docker-compose.yml` | Container deployment |
| `public/` | Static web UI for browser-side login |
| Code TTL | 15 minutes; cleaned up every 60 s |

### 2.6 `psn-api/` — PSN Trophy / Account Utilities

Shell scripts and Python helpers for querying PSN APIs (trophies, account ID).

### 2.7 `scripts/` — Build Infrastructure

| Script | Purpose |
|--------|---------|
| `build-macos.sh` | macOS Qt build (arm64, universal) |
| `build-appimage.sh` | Linux AppImage |
| `build-ffmpeg.sh`, `build-libplacebo.sh` | Dependency builds |
| `flatpak/` | Flathub manifest |
| `holepunch/` | RUDP hole-punch helpers |

### 2.8 `switch/`, `steamdeck_native/`, `setsu/`, `shared/`

Additional platform targets and native hardware abstraction modules.

---

## 3. Data Flow — Remote Play Session

```
User Input (controller/touch)
        │
        ▼
Controller State → lib: chiaki_controller_state_t
        │
        ▼
Feedback Sender → TAKION (UDP) → PS Console
                                        │
                              Video / Audio stream
                                        │
                                        ▼
             TAKION receive → FEC reassembly → Frame Processor
                                        │
                    ┌───────────────────┤
                    ▼                   ▼
              Audio Receiver      Video Receiver
                    │                   │
              Opus Decoder         FFmpeg Decoder
                    │                   │
             Audio Output          Platform Render
           (platform sink)        (OpenGL/Metal/Vulkan)
```

---

## 4. Build System

| Platform | Build System | Key CMake Options |
|----------|-------------|------------------|
| Desktop (all) | CMake + Ninja | `CHIAKI_ENABLE_GUI=ON`, `CHIAKI_ENABLE_FFMPEG_DECODER=ON` |
| Android | Gradle + CMake | `CHIAKI_ENABLE_ANDROID=ON` |
| iOS | Xcode + CMake | `CHIAKI_ENABLE_IOS=ON` |
| Linux Flatpak | Flatpak builder | See `scripts/flatpak/` manifest |
| Windows | MSYS2 + CMake | `CHIAKI_ENABLE_STEAMWORKS=ON` (optional) |

### Key CMake Feature Flags

| Flag | Default | Purpose |
|------|---------|---------|
| `CHIAKI_ENABLE_RUDP` | AUTO | Remote Play over Internet |
| `CHIAKI_ENABLE_STEAMWORKS` | OFF | Steamworks overlay |
| `CHIAKI_ENABLE_GUI_WEBENGINE` | ON (non-Linux) | Qt WebEngine for PSN OAuth |
| `CHIAKI_IS_MAC_APPSTORE` | OFF | Enable Mac App Store review controller |
| `CHIAKI_LIB_ENABLE_MBEDTLS` | OFF | mbedTLS instead of OpenSSL |
| `CHIAKI_ENABLE_STEAMDECK_NATIVE` | ON | Native Steam Deck gyro/haptics |

---

## 5. Third-Party Dependencies

| Library | Purpose | Source |
|---------|---------|--------|
| FFmpeg | Video decode (H.264/H.265/AV1) | System or built from `scripts/build-ffmpeg.sh` |
| Opus | Audio codec | System |
| libcurl | HTTP (catalog, PSN auth) | System or bundled (WebSocket support required) |
| OpenSSL / mbedTLS | Crypto (session) | System or CMake external project |
| SDL2 | Gamepad (desktop) | System |
| nanopb | Protobuf (cloud sessions) | `third-party/nanopb/` |
| Jerasure | Reed-Solomon FEC | `third-party/` |
| libplacebo | Video rendering | Built from `scripts/build-libplacebo.sh` |
| Qt 6 | Desktop UI | System |

---

## 6. CI/CD Architecture

```
Feature / fix / docs branch
      │
      └── PR to dev ──► Integration and applicable CI checks
                            │
                            └── Release PR (dev → master)
                                      │
                                      └── Merge to master
                                               │
                                               └── release-on-merge.yml
                                                    └── Platform binaries + GitHub Release

Store deployments use separate manually dispatched workflows.
```

**Key workflow files:** `.github/workflows/`

---

## 7. Version Management

Single source of truth in `CMakeLists.txt`:
```cmake
set(CHIAKI_VERSION_MAJOR 2)
set(CHIAKI_VERSION_MINOR 11)
set(CHIAKI_VERSION_PATCH 4)
```

CI injects these into iOS `MARKETING_VERSION`, Android `versionName`, and macOS bundle.

---

## 8. Upstream Merge Strategy

1. Add remote: `git remote add upstream https://github.com/streetpea/chiaki-ng`
2. Cherry-pick individual commits that touch `lib/` or protocol logic
3. Skip/adapt commits that touch Qt GUI files already diverged in AstraPlay
4. New AstraPlay features → new files → no upstream conflicts
