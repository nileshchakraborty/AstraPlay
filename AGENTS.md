# AstraPlay — Agents Guide

This file is the **AI agent operating manual** for the AstraPlay codebase. Read it completely before making any changes. It extends `CLAUDE.md` with expanded context for autonomous agents.

---

## 1. What AstraPlay Is

AstraPlay is a free, open-source PS4 / PS5 Remote Play and Internet/Cloud Play client for Android, iOS, macOS, Windows, Linux, and Steam Deck. It is a fork of [chiaki-ng](https://github.com/streetpea/chiaki-ng), extended with:

- App-store distribution (Google Play, App Store, Mac App Store, Flathub)
- Internet/Cloud Play (PSN game catalog streaming via Kamaji and Gaikai)
- Mobile-first touch UI
- PSStream server relay for mobile PSN login
- Steamworks overlay integration (optional, desktop)

**Codebase layout (always check these first):**

```
lib/            Shared C library — protocol, streaming, catalog, crypto
gui/            Qt desktop shell (macOS, Windows, Linux, Steam Deck)
android/        Kotlin/Java Android shell + JNI
ios/            Swift/ObjC iOS shell + CMake for C lib
psstream-server/ Node.js PSN login relay server
psn-api/        PSN API utilities (shell/Python)
scripts/        Build scripts (self-documenting — read headers)
test/           C unit tests (munit framework)
switch/         Nintendo Switch port (Borealis)
third-party/    nanopb, Jerasure, etc.
```

---

## 2. Core Rules (Non-Negotiable)

### 2.1 Architecture Rule
**Put all protocol/streaming/catalog/feature logic in `lib/` (C).** Platform shells only adapt I/O (video render, audio output, controller input, UI). If a fix needs to be repeated per platform, it belongs in `lib/`.

### 2.2 iOS ABI Rule
**Never call `static inline` chiaki functions from ObjC/Swift code.** `ChiakiSession`'s layout depends on the CMake config; Xcode compiles ObjC/Swift without it, causing writes to wrong offsets that silently no-op or corrupt memory.

- ✅ **DO:** Use `CHIAKI_EXPORT` `_ex` wrappers in `lib/src/ios_bridge_helpers.c`
- ✅ **DO:** Allocate `ChiakiSession` via `chiaki_session_get_sizeof()`
- ❌ **DON'T:** Call `chiaki_session_set_*` inline setters from `ios/AstraPlay/**`
- `ios/build.sh` runs `check_forbidden_inline_setters` — it will fail the build on violation

### 2.3 ID Normalization Rule
**Never infer prefix/regex patterns from PSN title ID samples.** IDs are region- and account-dependent; a pattern that fits one library corrupts matching for others.

- ✅ **DO:** Use `cc_stable_key()` in `lib/src/cloudcatalog_merge.c` (and `normalize_apollo_game` / `normalize_title` there) for stable cross-region key derivation
- ✅ **DO:** Extend those helpers; never add ad-hoc string checks elsewhere

### 2.4 Upstream Merge Safety Rule
**Prefer new files over editing upstream-owned Qt files.** This keeps chiaki-ng cherry-picks clean. When editing upstream files is unavoidable, mark changes with `// [ASTRAPLAY BEGIN]` / `// [ASTRAPLAY END]`.

### 2.5 Credential Rule
**Never store private keys, API tokens, or credentials in source code.** All secrets live in CI secret storage (GitHub Secrets), `.env` files (gitignored), or platform keychains.

### 2.6 Deletion Safety Rule
**Never run broad/recursive deletions** (`rm -rf *`) without providing a list of affected files and getting explicit confirmation. Prefer surgical deletes.

---

## 3. Build Commands (Read Script Headers First)

Scripts are self-documenting. Always `head -30 <script>` before running.

### macOS / Desktop Qt
```bash
./scripts/build-macos.sh arm64 --iterate --skip-deps --ad-hoc \
    --no-credentials-file --skip-notary-keychain --no-steamworks
# Open the app:
open build-output/AstraPlay.app
# View logs:
./scripts/build-macos.sh logs   # → tmp/astraplay-macos.log
```

### iOS
```bash
./ios/build.sh dev          # full build (use when lib/ changes)
./ios/build.sh iterate      # incremental (UI-only changes)
./ios/build.sh launch       # install + launch on connected device
./ios/build.sh logs         # stream Wi-Fi device log → ios/logs/astraplay.log
./ios/build.sh stop-logs    # stop log capture (don't TERM the process)

# Force full lib rebuild:
ASTRAPLAY_FULL_BUILD=1 ./ios/build.sh dev
```

### Android
```bash
./android/build-local.sh     # macOS / Linux
pwsh android/build.ps1       # Windows
```

### C Unit Tests
```bash
cmake -B build -DCHIAKI_ENABLE_TESTS=ON
cmake --build build -t chiaki-unit-tests
./build/test/chiaki-unit-tests
```

---

## 4. Verification Protocol

**Compiling is not verification.** After every change:

1. Build for the affected platform using the build script above.
2. Run on the real device / platform.
3. Collect and read the platform logs.
4. For cross-platform discrepancies: capture logs from a working and broken platform, diff them, and diff suspect files against `upstream/main`.

---

## 5. Workflow & Branching

| Branch | Purpose |
|--------|---------|
| `dev` | Integration branch; target for all feature, fix, and documentation PRs |
| `master` | Stable release branch; only release PRs from `dev` target it |
| Feature branches | Branch from `dev`; open PRs back to `dev` |

**Never push directly to `dev` or `master`.** Prepare a release by opening a PR from `dev` to `master`. Merging that release PR triggers the `release-on-merge.yml` build and GitHub Release workflow.

---

## 6. Key Non-Obvious System Facts

### PS5 Remote Play Rumble
PS5 Remote Play delivers rumble **only** as DualSense haptic audio:
- Client must set `enable_dualsense=true`
- Client must register a haptics sink (`chiaki_session_set_haptics_sink_ex` on iOS)
- Cloud/Internet Play uses classic rumble events — cloud working ≠ Remote Play working

### Qt Keyboard / Controller Navigation
- Relies on `Qt::TabFocusAllControls` set in `gui/src/main.cpp`
- Dialogs override `seedFocus()` to seed the first focused widget
- **Never** add competing `forceActiveFocus()` calls — they break controller navigation

### iOS Create/PS Button Claim
iOS binds Create/PS controller buttons to system gestures by default.
`StreamInput.attachController` disables system gesture preference via:
```swift
preferredSystemGestureState = .disabled
```
Don't change or remove this.

### Version Source of Truth
```cmake
# CMakeLists.txt
set(CHIAKI_VERSION_MAJOR 2)
set(CHIAKI_VERSION_MINOR 11)
set(CHIAKI_VERSION_PATCH 4)
```
CI injects these into iOS `MARKETING_VERSION`, Android `versionName`, macOS bundle. Do not edit version in platform-specific files directly.

---

## 7. Common Tasks — Agent Playbook

### Task: Add a new C library feature

1. Add header to `lib/include/chiaki/<feature>.h`
2. Implement in `lib/src/<feature>.c`
3. Add CMake target to `lib/CMakeLists.txt`
4. Write unit test in `test/<feature>.c` using munit
5. If iOS needs it: add `CHIAKI_EXPORT` wrapper in `lib/src/ios_bridge_helpers.c`
6. Wire up in the platform shell (Android JNI / iOS ObjC / Qt C++)
7. Run unit tests: `./build/test/chiaki-unit-tests`
8. Build and verify on the affected platform

### Task: Fix a streaming protocol bug

1. Read the logs from the broken platform
2. Identify the failing module in `lib/src/`
3. Check `upstream/main` for fixes already landed
4. Write a regression test case (add fixture data to `.inl` file if needed)
5. Fix in `lib/src/` — not in the platform shell
6. Run all unit tests to confirm no regression
7. Verify on device

### Task: Add a new cloud catalog field

1. Update the proto definition (if applicable) under `lib/`
2. Extend the merge/normalization logic in `lib/src/cloudcatalog_merge.c` using `cc_stable_key()`
3. Update `lib/src/cloudcatalog_util.c` / `cloudcatalog_cache.c` as needed
4. Update platform backends: `gui/src/cloudcatalogbackend.cpp`, Android model, iOS model
5. Write/update unit tests in `test/cloudcatalog_merge.c`

### Task: Add a PSStream server endpoint

1. Add handler function in `psstream-server/server.js` (named function, not anonymous lambda)
2. Register route for both `/psstream/*` and `/astraplay/*` paths for compatibility
3. Validate all `req.body` fields; return structured JSON errors
4. Never log NPSSO tokens
5. Add Jest test for the new endpoint
6. Update `psstream-server/README.md`

### Task: Cherry-pick from chiaki-ng upstream

```bash
git fetch upstream
git log upstream/main --oneline -20   # identify candidate commits
git cherry-pick <commit-sha>
# If it touches Qt files AstraPlay has diverged: cherry-pick --no-commit, then manually integrate
# Build and verify
```

### Task: Update a dependency (FFmpeg, libcurl, etc.)

1. Update version in relevant build script (`scripts/build-ffmpeg.sh`, etc.)
2. Verify checksum (SHA-256) of new release
3. Build with the new version: `./scripts/build-ffmpeg.sh && ./scripts/build-macos.sh arm64 --iterate`
4. Run unit tests
5. Verify streaming works end-to-end on macOS and at least one mobile platform

---

## 8. Files Agents Should NOT Touch Without Explicit Instruction

| File / Path | Reason |
|-------------|--------|
| `LICENSES/` | Legal — never modify |
| `COPYING` (AGPL-3.0) | Legal — never modify |
| `ios/AstraPlay.xcodeproj/project.pbxproj` | Xcode project file — use `ios/build.sh` or the xcode-project-setup skill for SDK additions |
| `android/app/google-services.json` | Platform config — managed by CI |
| `.github/workflows/*.yml` | CI workflows — changes require maintainer review |
| `third-party/nanopb/` | Submodule — update via `git submodule update`, not manual edits |

---

## 9. Debugging Checklist

When a feature works on one platform but not another:

- [ ] Is the behavior in `lib/` or in the platform shell?
- [ ] Diff the `lib/` call path for both platforms — are both calling the same function?
- [ ] Is iOS using an inline setter instead of a `_ex` wrapper?
- [ ] For catalog issues: is `cc_stable_key()` being used, or an ad-hoc string compare?
- [ ] For PS5 haptics: is `enable_dualsense=true` and a haptics sink registered?
- [ ] For Qt controller nav: any `forceActiveFocus()` competing with `seedFocus()`?
- [ ] Did the lib change require `ASTRAPLAY_FULL_BUILD=1` on iOS?
- [ ] Are logs being read from the correct path (not stdout of a different process)?

---

## 10. Related Documentation

| Document | Purpose |
|----------|---------|
| [`CLAUDE.md`](CLAUDE.md) | Compact agent guide (read first, then this file) |
| [`PRD.md`](PRD.md) | Product requirements, features, personas |
| [`DESIGN.md`](DESIGN.md) | UI/UX design, user flows, visual language |
| [`ARCHITECTURE.md`](ARCHITECTURE.md) | Component map, data flow, build system |
| [`DATA.md`](DATA.md) | Data categories, retention, transmission, privacy |
| [`SECURITY.md`](SECURITY.md) | Threat model, auth security, transport, CI security |
| [`CODE_STYLE.md`](CODE_STYLE.md) | Language-specific style rules, forbidden patterns |
| [`TESTING.md`](TESTING.md) | Unit tests, CI gates, manual verification checklists |
| [`CONTRIBUTING.md`](CONTRIBUTING.md) | PR workflow, branching, CI deployment table |
| [`STEAMWORKS_INTEGRATION.md`](STEAMWORKS_INTEGRATION.md) | Steamworks overlay setup |
