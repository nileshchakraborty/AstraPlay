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

## Issue 4: [Core / Network] Non-blocking Asynchronous DNS Resolution in Discovery
- **Type**: Bug / Performance
- **Priority**: Low
- **Components**: `lib/src/discovery.c` (line 446)
- **Description**:
  `getaddrinfo(host, NULL, &hints, &addrinfos)` currently blocks synchronously during host address resolution in `lib/src/discovery.c`.
- **Task**:
  Refactor hostname resolution to be non-blocking or dispatch to a background worker thread to ensure the UI event loop and UDP discovery loop never stall during network lookup.

---

## Issue 5: [Streaming Engine] Shared Timeout for Remote Play Holepunch Notifications
- **Type**: Bug / Reliability
- **Priority**: Low
- **Components**: `lib/src/remote/holepunch.c` (lines 1075 & 1223)
- **Description**:
  Holepunching currently uses separate timeouts for consecutive notification packets instead of a unified deadline timer.
- **Task**:
  Implement a shared monotonic deadline for holepunch notification rounds to prevent indefinite or prolonged delays when one endpoint is unresponsive.

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

## Issue 7: [CLI] Add Explicit Target and Verbose Flags to CLI Arguments
- **Type**: Enhancement
- **Priority**: Low
- **Components**: `gui/src/main.cpp` (lines 411, 499)
- **Description**:
  The command-line launcher currently lacks explicit options to select console target generation directly (`--target=ps4|ps5`) and verbose diagnostic logging (`-v / --verbose`).
- **Task**:
  Add CLI parser options in `gui/src/main.cpp` for `--target` and `--verbose`.

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
