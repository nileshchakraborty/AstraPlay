# Pylux — Code Style Guide

## 1. Language-Specific Conventions

### 1.1 C (`lib/`)

Pylux inherits the chiaki/chiaki-ng C style. Follow these rules:

**Naming**
```c
// Types: snake_case, _t suffix
typedef struct chiaki_session ChiakiSession;
typedef struct cloud_catalog_entry_s CloudCatalogEntry;

// Functions: chiaki_ prefix + snake_case
chiaki_session_init(ChiakiSession *session, ...);
cc_stable_key(const char *title_id);  // module-prefixed helpers

// Constants / macros: SCREAMING_SNAKE_CASE
#define CHIAKI_SESSION_MAX_FEEDBACK_LEN 256
CHIAKI_EXPORT void chiaki_session_set_haptics_sink_ex(...);

// Local variables / parameters: snake_case
int packet_len = 0;
uint8_t *recv_buf = NULL;
```

**Struct layout**
```c
// Members in logical groups; no padding hacks
typedef struct chiaki_controller_state_s {
    uint32_t buttons;       // bitmask — see controller.h constants
    uint8_t  l2_state;
    uint8_t  r2_state;
    int16_t  left_x,  left_y;
    int16_t  right_x, right_y;
    // ...haptics, gyro...
} ChiakiControllerState;
```

**Error handling**
```c
// Return CHIAKI_ERR_* codes; never abort() in library code
ChiakiErrorCode err = chiaki_session_init(&session, &connect_info, log);
if (err != CHIAKI_ERR_SUCCESS) {
    CHIAKI_LOGE(log, "Session init failed: %s", chiaki_error_string(err));
    return err;
}
```

**Memory**
- No global mutable state in `lib/`. All state lives in caller-provided structs.
- Allocate `ChiakiSession` via `chiaki_session_get_sizeof()`, not `sizeof`.
- Always pair `chiaki_*_init()` with `chiaki_*_fini()`.
- Prefer stack allocation for small fixed-size buffers (< 4 KB).

**iOS bridge rule**
```c
// ✅ DO: Add CHIAKI_EXPORT wrappers in ios_bridge_helpers.c
CHIAKI_EXPORT void chiaki_session_set_haptics_sink_ex(
    ChiakiSession *session, ChiakiHapticsSink sink, void *user);

// ❌ DON'T: call static inline setters from ObjC/Swift
// chiaki_session_set_haptics_sink(session, sink);  // WRONG from Xcode code
```

**Comments**
```c
// Single-line: explain *why*, not *what*

/**
 * cc_stable_key - derive a region-invariant key from a PSN title ID.
 *
 * PSN title IDs include a region suffix that varies by account. This
 * function strips the tail so entries from different regions merge correctly.
 * DO NOT add ad-hoc string comparisons elsewhere; extend this function.
 */
const char *cc_stable_key(const char *title_id);
```

---

### 1.2 C++ (`gui/`)

Pylux's Qt desktop code is C++17.

**Naming**
```cpp
// Classes: PascalCase
class QmlBackend : public QObject { ... };
class SteamworksWrapper { ... };

// Methods: camelCase
void openPsnLoginInSteamOverlay();
bool isStreamActive() const;

// Member variables: m_ prefix
QString m_deviceId;
QmlMainWindow *m_mainWindow = nullptr;

// Constants: k prefix or ALL_CAPS macro
constexpr int kDefaultBitrate = 15000;

// File names: match class name, lowercase with underscores
// qmlbackend.cpp / qmlbackend.h
```

**Qt-specific**
```cpp
// Use Q_OBJECT and proper signal/slot declarations
class QmlBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool streamActive READ isStreamActive NOTIFY streamActiveChanged)
public:
    Q_INVOKABLE void startStream(const QString &host);
signals:
    void streamActiveChanged();
};

// Prefer connect() with new-style syntax
connect(m_session, &ChiakiSession::stateChanged,
        this, &QmlBackend::onSessionStateChanged);
```

**Upstream Qt files**
```
// ❌ DON'T: make sweeping edits to upstream-owned Qt files
// ✅ DO: add new files for Pylux features
//        e.g. gui/src/cloudcatalogbackend.cpp (not patching settingsdialog.cpp for catalog)
```

**Steamworks isolation**
```cpp
// All Steam code stays in gui/src/steamworks/ and gui/include/steamworks/
// Touch only the PSN login button logic in the rest of codebase
```

---

### 1.3 Kotlin / Java (`android/`)

Follow [Android Kotlin style guide](https://developer.android.com/kotlin/style-guide) with these additions:

```kotlin
// Coroutines: use structured concurrency; no GlobalScope
viewModelScope.launch {
    val catalog = repository.fetchCatalog()
    _uiState.value = UiState.Success(catalog)
}

// Prefer StateFlow over LiveData for new code
private val _uiState = MutableStateFlow<StreamUiState>(StreamUiState.Idle)
val uiState: StateFlow<StreamUiState> = _uiState.asStateFlow()

// JNI: document memory ownership clearly
external fun chiakiSessionInit(config: ChiakiConnectConfig): Long  // returns handle
```

---

### 1.4 Swift / ObjC (`ios/`)

Follow [Swift API Design Guidelines](https://swift.org/documentation/api-design-guidelines/).

```swift
// Use value types (struct/enum) for data models
struct PyluxHost {
    let hostId: String
    let address: String
    var displayName: String
}

// Async/await for async operations
func fetchCatalog() async throws -> [CloudGame] { ... }

// Never call static inline chiaki setters — use bridge helpers only
// ✅ chiaki_session_set_haptics_sink_ex(session, sink, ctx)
// ❌ chiaki_session_set_haptics_sink(session, sink)  // wrong from Swift/ObjC
```

---

### 1.5 QML (`gui/src/qml/`)

```qml
// Component names: PascalCase files, camelCase properties
// StreamView.qml

import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: streamView
    
    // Explicit type annotations on properties
    property int targetFps: 60
    property string codecName: "H265"
    
    // Use Connections for cross-component signals
    Connections {
        target: Chiaki
        function onStreamDisconnected(reason) { ... }
    }
    
    // seedFocus() in dialogs — required for Qt keyboard nav
    function seedFocus() {
        closeButton.forceActiveFocus()
    }
}
```

---

### 1.6 Node.js (`psstream-server/`)

```javascript
// CommonJS modules (existing codebase)
const express = require('express');

// Named handler functions (not anonymous lambdas) for clarity
function handleCreateCode(req, res) { ... }
function handleGetTokens(req, res) { ... }

// No npsso values in logs
console.log(`[psstream] Retrieved NPSSO token for code: ${code}`);
// ✅ log code, not the token itself

// Validate all inputs; never trust req.body blindly
const code = (req.body && req.body.code
    ? String(req.body.code).trim().toUpperCase() : '');
if (!code) return res.status(400).json({ result: 'error', error: 'Missing code' });
```

---

## 2. General Rules (All Languages)

### 2.1 Comments & Documentation
- Write comments that explain *why* — the code explains *what*.
- Every public C function in `lib/include/chiaki/` must have a doc comment.
- Every new exported `CHIAKI_EXPORT` wrapper in `ios_bridge_helpers.c` must state why a wrapper is needed instead of the inline version.
- Do not leave `TODO` comments without a tracking issue reference: `// TODO(#123): ...`

### 2.2 Error Handling
- C library: return `ChiakiErrorCode`; never silently ignore errors on the streaming path.
- C++/Qt: use `qWarning()` / `qCritical()` appropriately; propagate errors to QML via `Q_PROPERTY`/`signal`.
- Android/iOS: surface errors to the UI — do not swallow exceptions in catch blocks.
- psstream-server: always return structured JSON `{ result: "error", error: "..." }` with appropriate HTTP status.

### 2.3 Logging
- Use the chiaki log system (`CHIAKI_LOGE`, `CHIAKI_LOGW`, `CHIAKI_LOGD`, `CHIAKI_LOGV`) in `lib/`.
- Never log: NPSSO tokens, OAuth tokens, registration keys, session crypto keys.
- Log: session IDs (truncated), error codes, timing info, console addresses (for debugging).

### 2.4 Platform Isolation
```
lib/          C only — no platform SDK includes
gui/          C++ / Qt only — links lib
android/app/  Kotlin/Java — JNI to lib
ios/Pylux/    Swift/ObjC — via bridge helpers
```
- No `#ifdef ANDROID` / `#ifdef __APPLE__` in `lib/` source (only allowed in `lib/include/chiaki/` platform detection headers).
- Platform-specific behavior → add a `CHIAKI_EXPORT` function in `lib/` and call it from the platform shell.

### 2.5 Upstream-Safe Changes
- **Prefer new files** over editing upstream-owned Qt files for new features.
- When editing upstream files is unavoidable, mark changes clearly:
  ```cpp
  // [PYLUX BEGIN] — Internet Play button
  connectInternetButton->setVisible(true);
  // [PYLUX END]
  ```
- Keep Pylux additions syntactically isolated so a `git diff upstream/main` shows a clean set.

### 2.6 Line Length & Formatting
- C/C++: 100 characters max (clang-format via `.clang-format` if present)
- Kotlin: 120 characters (ktlint)
- Swift: 120 characters (SwiftLint)
- QML: 120 characters
- JS: 100 characters (ESLint)

### 2.7 Commit Messages
```
<type>(<scope>): <short description>

[optional body]
[optional footer: Fixes #issue]
```
Types: `feat`, `fix`, `refactor`, `docs`, `test`, `chore`, `ci`
Scopes: `lib`, `gui`, `android`, `ios`, `macos`, `psstream`, `build`

Examples:
```
feat(lib): add cc_stable_key() for region-invariant title ID matching
fix(ios): use chiaki_session_get_sizeof() instead of sizeof in session alloc
chore(ci): update iOS deploy workflow to inject CHIAKI_VERSION from CMake
```

---

## 3. Forbidden Patterns

| Pattern | Reason | Alternative |
|---------|--------|-------------|
| `static inline` chiaki setters from ObjC/Swift | Wrong struct offset → silent corruption | `CHIAKI_EXPORT _ex` wrappers in `ios_bridge_helpers.c` |
| `sizeof(ChiakiSession)` in Xcode code | Struct layout differs without CMake config | `chiaki_session_get_sizeof()` |
| Ad-hoc title ID regex in platform code | Region-dependent → corrupt matching | `cc_stable_key()` in `lib/src/cloudcatalog_merge.c` |
| `forceActiveFocus()` competing with `seedFocus()` | Breaks Qt keyboard/controller navigation | Use `seedFocus()` in dialogs |
| Logging NPSSO / session keys | Security — token leakage in log files | Log code/ID only, never the secret |
| Secrets in source files | Security | CI secrets / `.env` (gitignored) |
| `rm -rf` without explicit path list | Data loss risk | Targeted removal of specific files |
| Global mutable state in `lib/` | Thread-safety; platform isolation | Caller-provided structs |
| `CURLOPT_SSL_VERIFYPEER = 0` | MitM vulnerability | Always verify TLS certs |
