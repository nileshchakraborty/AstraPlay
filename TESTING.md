# AstraPlay — Testing Document

## 1. Testing Philosophy

- **Verify on device, not just at compile time.** A build that compiles is not a verified change. Run on the real platform and read its logs.
- **`lib/` is the highest-value test target.** Every protocol fix must have a unit test in `test/` that exercises the code path.
- **Platform integration tests** catch issues that unit tests can't: ABI boundary bugs, audio/video sync, controller latency.
- **CI is the gatekeeper.** All tests must pass on CI before merge to `release/beta`.

---

## 2. Test Layers

```
                 ┌─────────────────────┐
                 │   Manual E2E Tests  │  ← Stream to real PS4/PS5 console
                 ├─────────────────────┤
                 │  Platform UI Tests  │  ← Espresso, XCTest UI, Qt test
                 ├─────────────────────┤
                 │ Integration Tests   │  ← lib ↔ platform boundary
                 ├─────────────────────┤
                 │   Unit Tests (C)    │  ← test/ (munit framework)
                 └─────────────────────┘
```

---

## 3. Unit Tests (`test/`)

The `test/` directory contains C unit tests using the [munit](https://nemequ.github.io/munit/) micro-unit-testing framework.

### 3.1 Running tests

```bash
# Build with tests enabled (default)
cmake -B build -DCHIAKI_ENABLE_TESTS=ON
cmake --build build -t chiaki-unit-tests

# Run
./build/test/chiaki-unit-tests

# Run with verbose output
./build/test/chiaki-unit-tests --log-visible debug
```

### 3.2 Existing test modules

| File | Tests |
|------|-------|
| `test/main.c` | Test runner entry point |
| `test/bitstream.c` | Bitstream read/write |
| `test/cloudcatalog_merge.c` | Title ID normalization, catalog dedup, `cc_stable_key()` |
| `test/cloudsession_kamaji.c` | Kamaji session parsing |
| `test/ctrl_lifecycle.c` | Control channel lifecycle state machine |
| `test/fec.c` + `fec_test_cases.inl` | Forward error correction encode/decode, loss patterns |
| `test/gkcrypt.c` | Session crypto (GkCrypt) |
| `test/http.c` | HTTP parser |
| `test/keystate.c` | Controller key state transitions |
| `test/pschord.c` | PlayStation chord / button combo detection |
| `test/regist.c` | Console registration protocol |
| `test/reorderqueue.c` | TAKION packet reorder queue |
| `test/rpcrypt.c` | RP session encryption round-trip |
| `test/seqnum.c` | Sequence number arithmetic |
| `test/takion.c` | TAKION packet parsing |
| `test/takion_av_packet_parse_real_video.inl` | Real-capture AV packet parse fixtures |

### 3.3 Writing new unit tests

```c
// test/cloudcatalog_merge.c — example pattern
#include "munit/munit.h"
#include <chiaki/cloudcatalog.h>

static MunitResult test_cc_stable_key_strips_region(
    const MunitParameter params[], void *user_data)
{
    // Arrange
    const char *id_us = "CUSA12345_00";
    const char *id_eu = "CUSA12345_01";

    // Act
    const char *key_us = cc_stable_key(id_us);
    const char *key_eu = cc_stable_key(id_eu);

    // Assert — both map to same stable key
    munit_assert_string_equal(key_us, key_eu);

    return MUNIT_OK;
}

static MunitTest catalog_tests[] = {
    { "/cc_stable_key/strips_region", test_cc_stable_key_strips_region,
      NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};
```

**Rules:**
- Test naming: `test_<module>_<behavior>` (snake_case)
- Arrange / Act / Assert structure
- Use `munit_assert_*` macros; never `assert()`
- Include realistic fixture data (e.g., real-capture `.inl` files)
- Test both success and failure paths

---

## 4. Build Verification

Before merging any change, confirm the build succeeds on the affected platform(s):

### 4.1 macOS / Desktop
```bash
./scripts/build-macos.sh arm64 --iterate --skip-deps --ad-hoc \
    --no-credentials-file --skip-notary-keychain --no-steamworks
# Then run:
open build-output/AstraPlay.app
# Check logs:
./scripts/build-macos.sh logs
```

### 4.2 iOS
```bash
./ios/build.sh iterate          # incremental C lib + Swift build
./ios/build.sh launch           # install + launch on device
./ios/build.sh logs             # tail Wi-Fi device log
./ios/build.sh stop-logs        # stop log capture

# Full rebuild when lib/ changes:
ASTRAPLAY_FULL_BUILD=1 ./ios/build.sh dev
```

### 4.3 Android
```bash
# macOS/Linux
./android/build-local.sh

# Windows
pwsh android/build.ps1
```

---

## 5. Platform Integration Tests

### 5.1 Android (Espresso)

Test location: `android/app/src/androidTest/`

Key test cases:
- `MainActivityTest` — app launches, home screen renders
- `DiscoveryTest` — console discovery list populates (mocked)
- `StreamSessionTest` — session init / teardown lifecycle (mocked lib)
- `PSNLoginRelayTest` — code display, polling flow (mocked server)

Run:
```bash
./gradlew connectedAndroidTest
```

### 5.2 iOS (XCTest)

Test location: `ios/AstraPlayTests/` (unit) and `ios/AstraPlayUITests/` (UI)

Key test cases:
- `SessionBridgeTests` — verifies all `_ex` wrapper calls compile and link
- `CatalogParserTests` — cloud catalog JSON parse
- `LoginRelayTests` — PSStream code display and token poll

Run from Xcode: `Product → Test` (⌘U)

### 5.3 Desktop Qt

```bash
cmake --build build -t chiaki-qt-test  # if Qt test target exists
```

Manual verification checklist:
- [ ] Controller keyboard navigation works (Tab, arrow keys, Enter)
- [ ] `seedFocus()` called correctly in all dialogs
- [ ] No competing `forceActiveFocus()` calls
- [ ] Settings persist across restarts
- [ ] Steamworks build compiles when `-DCHIAKI_ENABLE_STEAMWORKS=ON` and SDK present
- [ ] Non-Steamworks build compiles when `OFF` and SDK absent

---

## 6. PSStream Server Tests

Test location: `psstream-server/` (add `test/` subdirectory)

Recommended framework: Jest + supertest

```javascript
// psstream-server/test/relay.test.js
const request = require('supertest');
const app = require('../server');

describe('POST /astraplay/create-code', () => {
    it('returns success with valid code', async () => {
        const res = await request(app)
            .post('/astraplay/create-code')
            .send({ code: 'ABC123' });
        expect(res.status).toBe(200);
        expect(res.body.result).toBe('success');
        expect(res.body.code).toBe('ABC123');
    });

    it('returns 400 for missing code', async () => {
        const res = await request(app).post('/astraplay/create-code').send({});
        expect(res.status).toBe(400);
    });
});

describe('POST /astraplay/get-tokens', () => {
    it('returns 404 when token not yet ready', async () => {
        await request(app).post('/astraplay/create-code').send({ code: 'XYZ999' });
        const res = await request(app)
            .post('/astraplay/get-tokens')
            .send({ code: 'XYZ999' });
        expect(res.status).toBe(404);
        expect(res.body.error).toMatch(/not ready/i);
    });

    it('returns token and deletes code on success', async () => { ... });
    it('returns 404 for unknown/expired code', async () => { ... });
});
```

Run: `npm test` (add `jest` to devDependencies)

---

## 7. FEC / Streaming Protocol Tests

The FEC module uses pre-computed test case inlines for regression coverage:

```c
// test/fec_test_cases.inl — add new cases for discovered loss patterns
// Format: { .k = 4, .m = 2, .lost = {1, 3}, .expected = <recovered_data> }
```

When a streaming bug involving packet loss or reordering is found:
1. Capture a packet log from the broken session
2. Extract the raw packet sequence
3. Add it as a fixture in the relevant `_real_video.inl` or `fec_test_cases.inl`
4. Write a test that reproduces the failure before fixing

---

## 8. CI Test Gates

All PRs targeting `release/beta` must pass:

| Gate | Platform | Tool |
|------|----------|------|
| C unit tests | Linux (CI container) | munit (`chiaki-unit-tests`) |
| Android build | Ubuntu (GitHub Actions) | Gradle |
| iOS build | macOS (GitHub Actions) | `ios/build.sh dev` |
| macOS build | macOS (GitHub Actions) | `scripts/build-macos.sh` |
| Windows build | Windows (GitHub Actions / AppVeyor) | MSYS2 + CMake |
| Linux AppImage | Ubuntu (GitHub Actions) | Docker container |
| Flatpak build | Ubuntu (GitHub Actions) | `flatpak-builder` |

**No platform gate may be skipped.** If a platform is broken by a change, either fix it in the same PR or revert.

---

## 9. Manual Verification Checklist

Run before tagging a production release (merge to `master`):

### Remote Play
- [ ] PS5 discovered automatically on LAN
- [ ] Remote Play session starts within 5 s of console wake
- [ ] 1080p @ 60 FPS streams without visible tearing
- [ ] DualSense haptics work (PS5 Remote Play — haptic audio path)
- [ ] Controller gyro/IMU works where supported
- [ ] Disconnect/reconnect is graceful

### Internet Play
- [ ] Cloud game catalog loads and displays cover art
- [ ] Internet Play session starts for PS Plus catalog title
- [ ] Controller input is responsive (< 100 ms perceived)

### PSN Login
- [ ] PSStream code flow completes on Android
- [ ] PSStream code flow completes on iOS
- [ ] Steam overlay login works when `CHIAKI_ENABLE_STEAMWORKS=ON` (desktop)
- [ ] Sign-out clears token and returns to login screen

### UI / Navigation
- [ ] Full settings dialog navigable by controller (Steam Deck / desktop)
- [ ] iOS Create/PS button system gesture conflict does not trigger (`StreamInput.attachController`)
- [ ] Touch overlay visible and functional on Android phone
- [ ] Stream overlay exit button accessible

---

## 10. Logging for Test Diagnostics

When a test fails or a stream is broken, collect logs from the correct location:

| Platform | Log location | Command |
|----------|-------------|---------|
| macOS | `tmp/astraplay-macos.log` | `./scripts/build-macos.sh logs` |
| iOS | `ios/logs/astraplay.log` | `./ios/build.sh logs` |
| Android | Logcat | `adb logcat -s Chiaki AstraPlay` |
| Desktop (Linux/Win) | stdout / file | `CHIAKI_LOG_LEVEL=verbose ./astraplay` |

**Log diff technique:** Capture logs from a working platform and the broken platform; diff them to find the first divergence. Compare suspect files against `upstream/main` if cross-platform behavior differs.
