# AstraPlay — Security Document

## 1. Threat Model

AstraPlay is a Remote Play client that authenticates to PlayStation Network and maintains long-lived streaming sessions. The primary threat surfaces are:

| Threat | Description | Severity |
|--------|-------------|----------|
| T1 | NPSSO token theft → full PSN account takeover | Critical |
| T2 | Man-in-the-middle on PSStream relay → token intercept | Critical |
| T3 | Malicious console spoofing during discovery | High |
| T4 | Session key exposure in logs or crash dumps | High |
| T5 | Replay attacks on TAKION session | Medium |
| T6 | Unencrypted LAN discovery spoofing | Medium |
| T7 | CI secret leakage via log artifacts | High |
| T8 | Supply-chain attack via third-party dependency | Medium |

---

## 2. Authentication Security

### 2.1 PSN OAuth / NPSSO

- NPSSO tokens are the functional equivalent of a session password for a Sony account.
- Tokens are obtained only via the official PSN OAuth flow (HTTPS to Sony endpoints).
- After retrieval, tokens are stored exclusively in the platform's secure credential store:
  - **iOS/macOS:** Keychain (`kSecAttrAccessibleAfterFirstUnlock`)
  - **Android:** Android Keystore / EncryptedSharedPreferences
  - **Desktop:** OS credential store (`QKeychain` or equivalent)
- Tokens are **never** written to plain log files, user-visible settings files, or crash reports.
- Token rotation: PSN issues short-lived access tokens derived from NPSSO. Refresh is handled transparently.

### 2.2 PSStream Relay Server

- Relay codes are 6-character uppercase codes with a 15-minute TTL.
- NPSSO is stored in-memory only, deleted immediately on first retrieval (one-time use).
- Production deployment **must** run behind HTTPS (TLS 1.2+).
- No NPSSO value is written to server logs.
- Rate limiting should be applied to `/create-code` and `/get-tokens` endpoints to prevent code enumeration.

**Recommended rate limits:**
```
POST /astraplay/create-code  → 5 req / min per IP
POST /astraplay/get-tokens   → 30 req / min per IP (polling)
POST /astraplay/validate-code → 10 req / min per IP
```

### 2.3 Console Registration

- Registration keys (`rpkey`, `morning`) are stored encrypted in platform settings.
- PIN-based registration uses a time-limited code displayed on the console screen; AstraPlay does not generate or store PINs.

---

## 3. Transport Security

### 3.1 PSN / Catalog HTTPS

All outbound HTTP calls to Sony APIs use:
- TLS 1.2 minimum (libcurl enforced)
- Certificate verification enabled — no `CURLOPT_SSL_VERIFYPEER=0`
- System CA bundle on all platforms; no custom or pinned CAs

### 3.2 Remote Play TAKION (UDP)

| Property | Implementation |
|----------|---------------|
| Encryption | AES-128-CTR per packet (`rpcrypt.c`) |
| Key derivation | ECDH (Curve25519) during session handshake (`ecdh.c`) |
| Anti-replay | Sequence numbers + reorder queue (`seqnum.h`, `reorderqueue.c`) |
| Integrity | HMAC-SHA256 per packet |

### 3.3 Internet Play (RUDP / Kamaji / Gaikai)

Sessions are brokered by Sony's servers. Transport encryption is established by the PSN infrastructure. AstraPlay implements the RUDP hole-punch but does not manage key exchange for cloud sessions independently.

---

## 4. Secrets & Credential Hygiene

| Rule | Enforcement |
|------|-------------|
| No secrets in source code | Code review + `.gitignore` for keystore/cert files |
| No secrets in logs | Logging code must redact token fields |
| CI secrets in GitHub Secrets only | Never in workflow YAML as plaintext |
| Keystore password entered interactively or injected as CI secret | `android/build.ps1` uses env vars |
| iOS signing identities locked in CI Keychain | `deploy-ios.yml` unlocks ephemeral Keychain |
| Steamworks SDK placed externally (not in repo) | See `gui/third_party/README_STEAMWORKS.md` |

> [!CAUTION]
> Violating these rules is a non-negotiable blocker for PR merge. Any commit that includes a raw credential, API key, or signing artifact will be rejected and the credential must be rotated immediately.

---

## 5. iOS ABI Safety

The iOS ↔ C library boundary is a security-relevant correctness concern:

- **Problem:** `ChiakiSession` struct layout depends on CMake build config. Xcode compiles ObjC/Swift independently, so `sizeof(ChiakiSession)` differs between sides. Writing to the wrong offset silently corrupts memory.
- **Mitigation:** 
  - All struct field access from iOS code goes through `CHIAKI_EXPORT` `_ex` wrappers in `lib/src/ios_bridge_helpers.c`.
  - `ChiakiSession` is heap-allocated via `chiaki_session_get_sizeof()` — never stack-allocated or `sizeof`'d in Xcode-compiled code.
  - `ios/build.sh check_forbidden_inline_setters` fails the build if violations are detected.

---

## 6. Third-Party Dependency Security

| Library | Risk | Mitigation |
|---------|------|-----------|
| libcurl | CVE surface for TLS, WebSocket | Pin to specific release; verify against upstream checksums in CI |
| OpenSSL / mbedTLS | Crypto library CVEs | Use system-provided where possible; update on upstream advisory |
| nanopb | Protobuf parsing | Nanopb is designed for embedded — minimal attack surface; keep updated |
| Jerasure | Memory safety in FEC | Fuzz-test FEC paths; keep updated |
| FFmpeg | Large CVE surface (video parsing) | Use system FFmpeg on Linux; build from source with minimal decoders elsewhere |
| SDL2 | Input device CVEs | System package on Linux; bundled on Windows/macOS |
| Qt 6 | WebEngine CVEs (Chromium-based) | Disable WebEngine on Linux (`CHIAKI_ENABLE_GUI_WEBENGINE=OFF`); keep Qt updated |

**Dependency update policy:** Review upstream security advisories monthly. Critical CVEs in streaming-path libraries (FFmpeg, OpenSSL, libcurl) must be patched within 7 days.

---

## 7. Platform-Specific Security

### 7.1 Android
- `minSdkVersion` ≥ 26 (Android 8.0) for `EncryptedSharedPreferences`
- Exported activities reviewed for intent injection
- ProGuard/R8 enabled in release builds to prevent reverse engineering of PSN logic
- Network security config: cleartext traffic disabled

### 7.2 iOS
- App Transport Security (ATS) enforced; no `NSAllowsArbitraryLoads`
- Keychain items use `kSecAttrAccessibleAfterFirstUnlock` (not accessible while device locked)
- Background modes limited to `audio` (for active stream keep-alive)
- Entitlements reviewed each App Store submission

### 7.3 macOS
- Hardened Runtime enabled (required for notarization)
- Sandbox disabled for Desktop build (required for LAN broadcast + controller input); App Store build uses sandboxed entitlements
- Notarization performed in CI via `altool` / `notarytool`

### 7.4 Desktop (Windows / Linux)
- No admin/root required for normal operation
- AppImage and installer do not request elevated privileges
- Flatpak runs in user-space sandbox with only required portals (network, input devices)

---

## 8. Build & CI Security

| Control | Detail |
|---------|--------|
| No direct or force-push to `dev` or `master` | Contribution policy; current GitHub rulesets do not enforce the PR route |
| Maintainer review before merge | Contribution policy; current GitHub rulesets do not require an approval |
| Signed commits encouraged | GPG signing for maintainers |
| Workflow permissions | `contents: read`, elevated only for deploy steps |
| Artifacts with signing keys | Ephemeral; not persisted in job artifacts |
| OIDC for cloud auth (future) | Replace long-lived service account keys |

---

## 9. Responsible Disclosure

Security issues should be reported via [GitHub Private Vulnerability Reporting](https://github.com/nileshchakraborty/AstraPlay/security/advisories/new).

- Do not open public GitHub issues for security vulnerabilities.
- Maintainers will acknowledge within 48 hours.
- Coordinated disclosure after patch release (typically ≤ 30 days).

---

## 10. Out-of-Scope Security Considerations

- AstraPlay does not implement DRM; all content protection is Sony's responsibility on the console side.
- AstraPlay does not circumvent copy protection. If Sony's protocol is reversed-engineered by the upstream chiaki project, AstraPlay inherits that work and the associated legal/ethical analysis is unchanged.
- Jailbreak / root device support is not a goal; behavior on such devices is untested and unsupported.
