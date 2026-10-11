# Pylux — Data Document

## 1. Overview

Pylux handles several distinct data domains: PSN authentication tokens, console metadata, cloud game catalog entries, session state, and streaming media. This document defines what data is stored, where, how long, and under what access model.

---

## 2. Data Categories

### 2.1 PSN Authentication Credentials

| Field | Type | Source | Storage |
|-------|------|--------|---------|
| NPSSO token | String (≤ 64 chars, alphanumeric) | PSN OAuth web flow | Platform keychain / secure storage |
| PSN Account ID | 64-bit unsigned integer | PSN API `/v1/users/me` | Persisted settings (encrypted on iOS, Android Keystore) |
| PSN Account name | String | PSN API | Persisted settings |
| OAuth access token | JWT | PSN OAuth | In-memory only; derived from NPSSO |
| OAuth refresh token | JWT | PSN OAuth | Secure storage (same as NPSSO) |

> [!CAUTION]
> NPSSO tokens are session cookies equivalent to a password. They must **never** be logged, written to plain files, or sent to any endpoint other than PSN.

### 2.2 Console (Host) Metadata

Stored per registered console in persistent settings:

| Field | Type | Notes |
|-------|------|-------|
| `host_id` | String (16 hex chars) | Unique console ID from discovery |
| `host_name` | String | User-visible name |
| `host_addr` | IPv4/IPv6 string | Last known address (LAN) |
| `registered_host_type` | Enum (PS4 / PS5) | Determines protocol variant |
| `registered_rpkey` | 16-byte blob | Remote Play registration key (encrypted) |
| `rp_regist_key` | 16-byte blob | Registration key for session auth |
| `morning` | 16-byte blob | Session auth material |
| `server_nickname` | String | Optional human label |

### 2.3 Cloud Game Catalog

Sourced from PSN cloud API; cached locally.

| Field | Type | Notes |
|-------|------|-------|
| `title_id` | String | PSN title ID (region-varying; use `cc_stable_key()` for matching) |
| `product_id` | String | PlayStation Store product identifier |
| `np_title_id` | String | NP title ID (for entitlement checks) |
| `name` | String | Localized display name |
| `image_url` | URL string | Cover art |
| `entitlement` | Bool | Whether user has access |
| `platform` | Enum (PS4/PS5) | Which console the title runs on |
| `launch_type` | Enum (Kamaji/Gaikai) | Cloud backend selector |
| `launchspec` | JSON blob | Opaque launch parameters for cloud session |

> [!IMPORTANT]
> **Do not** match titles by inferring prefix/regex patterns from `title_id` samples. IDs are region- and account-dependent. Always use `cc_stable_key()` from `lib/src/cloudcatalog_merge.c` for stable cross-region matching.

**Cache location:**
- Android: `Context.getCacheDir()/cloudcatalog/`
- iOS: `NSCachesDirectory/cloudcatalog/`
- Desktop: `QStandardPaths::CacheLocation + "/cloudcatalog/"`
- Cache TTL: configurable (default 24 h); invalidated on PSN error response

### 2.4 PSStream Relay Server State (In-Memory)

The `psstream-server` holds ephemeral login codes only in RAM (`Map<code, entry>`).

| Field | Type | TTL |
|-------|------|-----|
| `code` | 6-char uppercase string | 15 minutes |
| `npsso` | String or `null` | Deleted on first successful `get-tokens` |
| `createdAt` | Unix timestamp (ms) | — |

No database. No file persistence. Server restart clears all codes. Cleanup runs every 60 seconds.

### 2.5 Stream Session State (In-Memory Only)

| Data | Type | Scope |
|------|------|-------|
| Video frame buffer | Ring buffer of decoded YUV/NV12 frames | Per session |
| Audio PCM buffer | Ring buffer of decoded PCM chunks | Per session |
| Reorder queue | Priority queue of TAKION packets | Per session |
| Controller state | `chiaki_controller_state_t` (bitmask + axes) | Per session |
| Feedback / haptics | Ring buffer of haptic audio packets | Per session |
| Session keys | 16-byte AES key + IV | Per session; ephemeral |

All session state is destroyed when the session ends.

### 2.6 Application Settings

Stored via platform-native persistent storage:
- Android: `SharedPreferences` / Room DB (for host list)
- iOS: `UserDefaults` + Keychain
- Desktop: `QSettings` (platform-appropriate backend: Registry on Windows, plist on macOS, INI on Linux)

| Setting | Type | Default |
|---------|------|---------|
| Video resolution | Enum (720p/1080p/4K) | 1080p |
| Video FPS | Enum (30/60) | 60 |
| Video codec | Enum (H264/H265/AV1) | H265 |
| Video bitrate | Integer (kbps) | 15000 |
| Audio buffer size | Integer (ms) | 100 |
| Rumble enabled | Bool | true |
| Mic enabled | Bool | false |
| Hardware decode | Bool | true |
| Steam Deck native mode | Bool | auto |

---

## 3. ID Normalization

PSN IDs are inherently region-sensitive. The only safe matching algorithm is:

```c
// lib/src/cloudcatalog_merge.c
const char *cc_stable_key(const char *title_id);
// Splits on '-'/'_', drops the region-varying tail.
// Used by: normalize_apollo_game(), normalize_title()
```

**Rule:** Extend `cc_stable_key()` / `normalize_*` helpers in `lib/src/cloudcatalog_merge.c`. Never add ad-hoc string comparisons in platform code or UI layers.

---

## 4. Data Retention & Deletion

| Data | Retention | Deletion mechanism |
|------|-----------|-------------------|
| NPSSO / OAuth tokens | Until user logs out | User-initiated "Sign Out"; also cleared on auth error 401 |
| Host registration keys | Until user removes console | "Remove console" action |
| Cloud catalog cache | 24 h or TTL expiry | App clears cache on refresh; OS clears on low storage |
| PSStream relay codes | 15 min | Server auto-expiry; one-time use after token retrieved |
| Session buffers | Duration of stream session | Freed on session end |
| Log files | 7 rolling days | `logs` rotation in `tmp/pylux-macos.log`, `ios/logs/pylux.log` |

---

## 5. Data Transmission

| Data | Destination | Protocol | Encrypted |
|------|-------------|----------|-----------|
| NPSSO / PSN auth | PSN servers (Sony) | HTTPS | TLS 1.2+ |
| Cloud catalog fetch | PSN API | HTTPS | TLS 1.2+ |
| PSStream relay code | psstream-server | HTTPS (prod) | TLS 1.2+ |
| Remote Play A/V | PS console | TAKION (UDP) | AES-128 (rpcrypt) |
| Internet Play A/V | PSN cloud (Kamaji/Gaikai) | TAKION/RUDP (UDP) | AES-128 |
| Controller input | PS console | UDP (within TAKION) | AES-128 |
| Discovery probes | LAN broadcast | UDP | None (local network) |

> [!NOTE]
> The PSStream relay server receives a one-time code and NPSSO token during login. In production it **must** run behind HTTPS. The NPSSO is deleted from server memory immediately after the client retrieves it (one-time use).

---

## 6. Privacy

- **No telemetry or analytics** — Pylux does not send usage data to any Pylux/ForWard Technologies endpoint.
- **No user tracking** — No device IDs, no fingerprinting.
- **PSN data** — All PSN API calls go directly to Sony. Pylux does not proxy or log PSN API responses except in debug log files on the local device.
- **Log files** — May contain IP addresses, console IDs, and timing data. Users should not share logs publicly without redacting sensitive fields.

---

## 7. Data Schema — cloudcatalog (Protobuf via nanopb)

Cloud session launch parameters are serialized using Protocol Buffers (nanopb). Proto definitions live in `lib/` (generated from `.proto` files compiled via `scripts/fetch-protoc.sh`).

Key messages (illustrative):
```proto
message LaunchSpec {
  string title_id = 1;
  string product_id = 2;
  LaunchType launch_type = 3;  // KAMAJI or GAIKAI
  string region = 4;
  bytes opaque_params = 5;
}
```

---

## 8. Secrets & Credentials Management

| Secret | Location | Access |
|--------|----------|--------|
| PSN NPSSO | Device Keychain / Android Keystore | Runtime only |
| Steam App ID | `gui/src/steamworks/steamworks_api.cpp` (build-time constant) | Public (Steam) |
| iOS signing cert | CI environment secrets (Keychain locked) | CI only |
| Android keystore | CI environment secrets | CI only |
| macOS notarization key | CI environment secrets | CI only |

> [!CAUTION]
> Never commit tokens, keystore files, or notarization credentials to the repository. Use CI secret storage (GitHub Secrets / Keychain) exclusively.
