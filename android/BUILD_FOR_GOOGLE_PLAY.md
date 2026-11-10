# Building AstraPlay for Google Play Store

This guide explains how to build an Android App Bundle (AAB) for uploading to Google Play Console.

## Prerequisites

### 1. Create a Release Keystore (One-time setup)

If you don't already have a keystore, create one:

```bash
cd <repo>/android
keytool -genkeypair -v -keystore astraplay-release.keystore -alias astraplay -keyalg RSA -keysize 2048 -validity 10000
```

**IMPORTANT**: Save the passwords you create! You'll need them for every release.

### 2. Configure Signing (One-time setup)

Edit `android/local.properties` and add your keystore details:

```properties
astraplayKeystore=astraplay-release.keystore
astraplayKeystorePW=YOUR_KEYSTORE_PASSWORD
astraplayKeyAlias=astraplay
astraplayKeyPW=YOUR_KEY_PASSWORD
```

Replace `YOUR_KEYSTORE_PASSWORD` and `YOUR_KEY_PASSWORD` with your actual passwords.

## Building for Production

### Quick Command

To build a signed AAB for Google Play:

```powershell
cd <repo>\android
.\build.ps1 release -bundle
```

This will:
- Build a **release** version
- Create an **AAB** (Android App Bundle) instead of APK
- Sign it with your release keystore
- Build for **all ABIs** (arm64-v8a, armeabi-v7a, x86, x86_64)

### Output Location

The signed AAB will be located at:
```
android\app\build\outputs\bundle\release\app-release.aab
```

This file is ready to upload to Google Play Console!

### PSN Login Relay URL

The QR / Android TV login flow uses a token relay server (`psstream-server`).
Publishers can configure a default relay URL at build time, and end users/testers can also configure their own relay server or sign in with an NPSSO token directly inside the app at runtime.

#### Build-Time Configuration (Optional)
To bake a default relay host into the build, set the `AstraPlayRelayUrl` property via any of the following:

1. **`android/local.properties`** (git-ignored, recommended for local dev):
   ```properties
   AstraPlayRelayUrl=https://relay.example.com:25525
   ```
2. **Environment Variable**:
   ```bash
   export ASTRAPLAY_RELAY_URL="https://relay.example.com:25525"
   ```
3. **Gradle Command Line**:
   ```bash
   ./gradlew :app:assembleRelease -PAstraPlayRelayUrl=https://relay.example.com:25525
   ```
4. **Local build script**:
   ```bash
   ./build-local.sh --quick --relay-url http://10.0.2.2:25525
   ```

#### Runtime Configuration (In-App)
If no relay is configured at build time (or when testing self-hosted setups):
- **In-App Prompt**: Opening the login screen presents options to **"Configure Relay Server"** (enter your self-hosted URL on the spot) or **"Sign in with Browser / Code"** (follow 3 quick steps in your browser with direct links and automatic code extraction).
- **Settings**: Users can set or update the relay URL anytime under **Settings → Account → PSN Login Relay URL**.

See `../../psstream-server/README.md` for how to self-host the relay (default port 25525).

### Android 16 (API Level 36) & Compliance Requirements

- **Target SDK**: AstraPlay targets **API level 36** (`targetSdk 36`, `compileSdk 36`) complying with Google Play's latest requirements.
- **Package Name**: `io.github.nileshchakraborty.astraplay`
- **Data Safety**: All credentials (PSN Account ID, NPSSO tokens) are stored locally in the Android Keystore / encrypted SharedPreferences. Zero data is shared or transmitted to third parties. Refer to [`PRIVACY.md`](../PRIVACY.md) and [`docs/privacy.md`](../docs/privacy.md) for the exact Data Safety responses required in Google Play Console.

### R8 Optimization, Obfuscation & Resource Shrinking

AstraPlay utilizes R8 in Full Mode (`android.enableR8.fullMode=true`) paired with `shrinkResources true` and `minifyEnabled true` in production release builds to satisfy Google Play's performance and memory optimization benchmarks:

- **Aggressive Dead-Code Elimination & Class Inlining**: Legacy `-dontobfuscate` and blanket package keep rules have been removed, enabling R8 to inline single-caller methods, merge classes, prune dead code, and shrink member field names.
- **Resource Shrinking**: Unused drawables, layouts, and strings are stripped from final release bundles (`.aab` and `.apk`).
- **Surgical Keep Boundaries**:
  - `com.metallic.chiaki.lib.**` is kept to guarantee JNI boundary stability with `chiaki-jni.c`.
  - Room entities (`@Entity`), DAOs (`@Dao`), and database subclasses (`RoomDatabase`) are preserved.
  - Moshi JSON serialization classes (`@JsonClass`) and synthetic constructors are retained.
  - Native methods across all classes (`native <methods>;`) and `Parcelable` creators are protected.
- **Crash Stack Trace Retention**: `SourceFile` and `LineNumberTable` attributes are preserved, and ProGuard mapping files are automatically generated and uploaded to Google Play Console for de-obfuscation.

### Container & Emulation Resiliency (Waydroid, SteamOS, BlueStacks)

To ensure crash-proof operation on handheld devices running Waydroid or containerized Android environments:
- **Dynamic WebView Decoupling**: WebView is instantiated dynamically rather than statically inflated from XML. If a system lacks a functional Android System WebView provider (e.g., bare AOSP Waydroid images without GAPPS), the app displays a fallback layout with "Open in External Browser" and "Switch to Phone Login" rather than crashing with `InflateException`.
- **Software Layer Rendering**: When running inside an emulator or container, `LAYER_TYPE_SOFTWARE` is applied to avoid Chromium hardware acceleration crashes in LXC containers.
- **Non-blocking Socket Operations**: Local relay server initialization and LAN IP discovery run on `Dispatchers.IO` coroutines to prevent `NetworkOnMainThreadException` on modern Android versions and StrictMode.
- **API Guardrails**: `InputDevice.isVirtual` is guarded by `Build.VERSION_CODES.Q` (API 29+) with `Throwable` exception handling.

## Build Options

- `.\build.ps1 release -bundle` - Full production build with all ABIs (recommended for Google Play)
- `.\build.ps1 release -bundle -clean` - Clean build (removes all cached artifacts first)
- `.\build.ps1 debug -bundle` - Debug AAB (for testing Play Store features locally)

## For Local Development

For faster local development builds (APK only, single ABI):

```powershell
.\build.ps1 debug -quick
```

This builds only arm64-v8a and skips tests/linting.

## Uploading to Google Play

1. Build the release AAB: `.\build.ps1 release -bundle`
2. Go to [Google Play Console](https://play.google.com/console)
3. Select your app
4. Go to "Release" → "Production" (or "Internal testing" for testing)
5. Click "Create new release"
6. Upload `app-release.aab`
7. Fill in release notes and submit for review

## Incrementing Version

Before each release, update the version in:
- `CMakeLists.txt` (root) - Update `CHIAKI_VERSION_MAJOR/MINOR/PATCH`
- `android/app/build.gradle` - Increment `versionCode`

The version name is automatically read from CMakeLists.txt.

## GitHub Actions & Google Play Developer API (CI)

AstraPlay uses Google Play Developer API integration via Fastlane to automatically deploy signed Android packages to Google Play Console:

1. **Required GitHub Secrets**:
   - `ANDROID_KEYSTORE_BASE64`: Base64-encoded release keystore (`.jks`).
   - `ANDROID_KEYSTORE_PASSWORD`: Keystore password.
   - `ANDROID_KEY_ALIAS`: Signing key alias.
   - `ANDROID_KEY_PASSWORD`: Signing key password.
   - `GOOGLE_PLAY_SERVICE_ACCOUNT_JSON_BASE64`: Base64 (or plaintext) Google Cloud service account JSON key with Google Play Developer API publishing permissions.

2. **Artifact Generation & Deployment**:
   - **Signed APK (`AstraPlay-*.apk`)**: Built via `./gradlew assembleRelease`, signed with your release keystore, uploaded as workflow artifact `astraplay-android-apk`, and eligible for direct deployment / Internal App Sharing via the Play API.
   - **Signed AAB (`AstraPlay-*.aab`)**: Built via `./gradlew bundleRelease`, signed with your release keystore, uploaded as workflow artifact `astraplay-android-aab`, and published directly to your selected track (`production`, `beta`, `alpha`, or `internal`).

3. **Workflows**:
   - **`Deploy Android + Android TV (Google Play)`** (`.github/workflows/deploy-android.yml`): Manual dispatch or reusable workflow for targeted Android deployments. Supports selecting target (`both`, `aab`, `apk`) and track.
   - **`Release on Merge`** (`.github/workflows/release-on-merge.yml`): When merging to `master` or cutting a release, automatically signs and publishes to Google Play when API credentials are provided.

## Troubleshooting

### "Signing not enabled" message
- Check that `local.properties` has all four signing properties configured
- Verify the keystore file path is correct

### Build fails with signing error
- Verify passwords in `local.properties` are correct
- Ensure keystore file exists at the specified path

### AAB file not found
- Check `android\app\build\outputs\bundle\release\` directory
- Review build output in `build-output.txt` for errors

## Security Notes

- **NEVER** commit `local.properties` to git (it's already in .gitignore)
- **NEVER** commit your keystore file to git
- Keep backups of your keystore in a secure location
- If you lose your keystore, you cannot update your app on Google Play!
