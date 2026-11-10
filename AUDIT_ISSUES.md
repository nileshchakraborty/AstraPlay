# AstraPlay Issue Tickets & Audit Log

This audit documents pending items, enhancement opportunities, and technical tasks across the AstraPlay codebase to be filed as GitHub Issues once the `AstraPlay` repository is created and issues are enabled.

---

## Issue 1: [Android] Configure Production Google Play Signing Key & Keystore Secrets
- **Type**: Enhancement / DevSecOps
- **Priority**: High
- **Components**: `android/app/build.gradle`, `.github/workflows/release-on-merge.yml`
- **Description**:
  The Android build system is now configured to support automated release signing via GitHub Secrets:
  - `ANDROID_KEYSTORE_BASE64`
  - `ANDROID_KEYSTORE_PASSWORD`
  - `ANDROID_KEY_ALIAS`
  - `ANDROID_KEY_PASSWORD`
  When secrets are present, the workflow automatically generates both `AstraPlay-Android.apk` and `AstraPlay-Android.aab` (Play Store bundle).
- **Task**:
  1. Generate production keystore using `keytool` for `com.astraplay.stream`.
  2. Encode keystore to base64 and register repository secrets in the new `AstraPlay` repo.
  3. Verify release bundle is properly signed with the release certificate.

---

## Issue 2: [Windows] Authenticode Code Signing for `.msi` and `.exe` Installers
- **Type**: Enhancement / Security
- **Priority**: Medium
- **Components**: `scripts/astraplay.iss`, `scripts/astraplay.wxs`, `.github/workflows/release-on-merge.yml`
- **Description**:
  Windows SmartScreen may prompt untrusted publisher warnings for newly published binaries.
- **Task**:
  1. Integrate a Windows Authenticode code signing step (e.g., using `signtool.exe` or Azure Trusted Signing / SignPath in CI).
  2. Sign `astraplay.exe`, `astraplay-windows-installer.exe`, and `astraplay-windows-x64.msi`.

---

## Issue 3: [macOS] Apple Developer ID Signing & Notarization Integration
- **Type**: Enhancement / Security
- **Priority**: Medium
- **Components**: `scripts/build-macos.sh`, `.github/workflows/release-on-merge.yml`
- **Description**:
  macOS currently builds with ad-hoc signing (`--ad-hoc`) so users can open the application using Gatekeeper override.
- **Task**:
  1. When an Apple Developer Account is provisioned, configure Developer ID Application certificate and App Store Connect API keys in repository secrets (`APPLE_CERTIFICATE_BASE64`, `APPLE_CERTIFICATE_PASSWORD`, `NOTARY_API_KEY`).
  2. Update `scripts/build-macos.sh` to notarize `AstraPlay.dmg` and staple the notarization ticket.

---

## Issue 4: [Core / Network] Non-blocking Asynchronous DNS Resolution in Discovery (Resolved in v2.12.2)
- **Type**: Bug / Performance (Resolved)
- **Priority**: Low
- **Components**: `lib/src/discovery.c` (line 446), `lib/src/utils.h`
- **Description**:
  `getaddrinfo(host, NULL, &hints, &addrinfos)` previously blocked synchronously during host address resolution.
- **Resolution**:
  Resolved via `chiaki_resolve_host_async` with a configurable 3000ms deadline, preventing UI event loop and UDP discovery stalls.

---

## Issue 5: [Streaming Engine] Shared Timeout for Remote Play Holepunch Notifications (Resolved in v2.12.3)
- **Type**: Bug / Reliability (Resolved)
- **Priority**: Low
- **Components**: `lib/src/remote/holepunch.c` (lines 1075 & 1223)
- **Description**:
  Holepunching previously used separate timeouts for consecutive notification packets instead of a unified deadline timer.
- **Resolution**:
  Implemented shared monotonic deadline timers (`deadline_ms = chiaki_time_now_monotonic_ms() + timeout`) across notification rounds.

---

## Issue 6: [Flatpak] Publish and Register AstraPlay Flatpak Manifest on Flathub
- **Type**: Packaging / Distribution
- **Priority**: Medium
- **Components**: `gui/io.github.nileshchakraborty.AstraPlay.metainfo.xml`, `gui/io.github.nileshchakraborty.AstraPlay.desktop`, `scripts/flatpak/`
- **Description**:
  Use `io.github.nileshchakraborty.AstraPlay` consistently for the Flatpak app ID, AppStream component ID, and desktop file ID.
- **Task**:
  1. Complete local build and validation for the manifest.
  2. Submit the app to Flathub from a human-maintained packaging repository; Flathub requires disclosure of AI-assisted packaging work and prohibits AI agents from opening submission PRs.
  3. Configure the CI workflow to reference the Flathub repository after it exists.

---

## Issue 7: [CLI] Add Explicit Target and Verbose Flags to CLI Arguments (Resolved in v2.12.26)
- **Type**: Enhancement (Resolved)
- **Priority**: Low
- **Components**: `gui/src/main.cpp` (lines 214-248, 377-385, 462-469)
- **Description**:
  The command-line launcher lacked explicit options to select console target generation directly (`--target=ps4|ps5`) and verbose diagnostic logging (`-v / --verbose`).
- **Resolution**:
  Added CLI parser options in `gui/src/main.cpp` for `--target` and `-v / --verbose` with input validation.

---

## Issue 8: [Discovery] macOS Subnet Broadcast Routing Failure (Resolved in v2.12.2)
- **Type**: Bug / Network (Resolved)
- **Priority**: Critical
- **Components**: `lib/src/utils.h`
- **Description**:
  Broadcast socket calls on macOS returned `errno 65 (No route to host)` when sending to `INADDR_BROADCAST (255.255.255.255)`.
- **Resolution**:
  Enabled interface traversal via `getifaddrs` on macOS (`defined(__APPLE__)`) to broadcast directly to each active interface's `ifa_broadaddr`.

---

## Issue 9: [Relay] Dedicated Port 25525 for Embedded and Standalone PSN Relay (Resolved in v2.12.3)
- **Type**: Bug / Port Collision (Resolved)
- **Priority**: Medium
- **Components**: `gui/include/embeddedrelayserver.h`, `gui/src/embeddedrelayserver.cpp`, `gui/src/qmlbackend.cpp`, `psstream-server/`
- **Description**:
  Port 8080 caused binding conflicts with local development web servers and developer environments.
- **Resolution**:
  Migrated embedded server and standalone `psstream-server` defaults to dedicated port `25525`.

---

## Issue 10: [Security / Transport] Enforce TLS and CA Verification on Push Notifications (Resolved in v2.12.26)
- **Type**: Bug / Security (Resolved)
- **Priority**: Critical
- **Components**: `lib/src/remote/holepunch.c`, `switch/src/discoverymanager.cpp`
- **Description**:
  Push notification WebSocket connections previously disabled verification (`CURLOPT_SSL_VERIFYPEER, 0L; CURLOPT_SSL_VERIFYHOST, 0L`), violating `SECURITY.md` §3.1.
- **Resolution**:
  Restored strict TLS certificate and hostname verification (`CURLOPT_SSL_VERIFYPEER, 1L; CURLOPT_SSL_VERIFYHOST, 2L`) using system trust stores.

---

## Issue 11: [Security / Updater] Cryptographic Digest Verification in Auto-Updater (Resolved in v2.12.26)
- **Type**: Bug / Security (Resolved)
- **Priority**: High
- **Components**: `gui/include/updatemanager.h`, `gui/src/updatemanager.cpp`, `gui/test/updatemanager_test.cpp`
- **Description**:
  Downloaded release binaries were verified only by file size and magic headers, without cryptographic checksum validation against release digests.
- **Resolution**:
  Implemented SHA-256 checksum verification against release `SHA256SUMS.txt` before permitting payload execution or elevated replacement across Windows, macOS, and Linux.

---

## Issue 12: [Security / Storage] Hardware-Bound AES-256 Credential Vault for Desktop (Resolved in v2.12.26)
- **Type**: Enhancement / Security (Resolved)
- **Priority**: High
- **Components**: `gui/include/credentialvault.h`, `gui/src/credentialvault.cpp`, `gui/test/credentialvault_test.cpp`, `gui/src/settings.cpp`, `gui/src/host.cpp`
- **Description**:
  Desktop platforms stored PSN NPSSO tokens, OAuth access/refresh tokens, and console registration keys in plaintext `QSettings` storage.
- **Resolution**:
  Implemented `CredentialVault` providing AES-256-CBC encryption derived from machine UUID and current user identity via PBKDF2 with automatic legacy migration and plaintext purging.

---

## Issue 13: [Security / Relay] Rate Limiting, TCP Stream Framing, and Memory Bounding (Resolved in v2.12.26)
- **Type**: Enhancement / Security (Resolved)
- **Priority**: Medium
- **Components**: `gui/include/embeddedrelayserver.h`, `gui/src/embeddedrelayserver.cpp`, `gui/test/embeddedrelayserver_bdd.cpp`, `psstream-server/server.js`, `psstream-server/test/relay.feature.test.js`
- **Description**:
  Desktop embedded relay dropped fragmented HTTP POST payloads, lacked active code capacity bounds, and omitted per-IP rate limiting specified in `SECURITY.md`.
- **Resolution**:
  Added `Content-Length` stream reassembly, a 500-code active capacity limit (HTTP 429), and per-IP rate limiting middleware across both embedded and standalone relay servers.
