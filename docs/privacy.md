# Privacy Policy for AstraPlay

**Effective Date:** October 18, 2026  
**Last Updated:** November 9, 2026

**AstraPlay** ("the Application") is a free, open-source PlayStation Remote Play and Cloud Streaming client developed by Nilesh Chakraborty and the AstraPlay open-source community ("we", "us", or "our"). 

This Privacy Policy explains our commitment to user privacy, device permissions, and regulatory compliance under the **Google Play Developer Policy**, **Apple App Store Review Guidelines**, the **General Data Protection Regulation (GDPR / UK-GDPR)**, the **California Consumer Privacy Act (CCPA / CPRA)**, and the **Children's Online Privacy Protection Act (COPPA)**.

---

## 1. Zero Data Collection Guarantee ("FYI: We Do Not Collect User Data")

> [!IMPORTANT]
> **We do not collect user data. Period.**
> AstraPlay operates entirely on a zero-data-collection architecture:
> - **No Telemetry & No Analytics**: We do not include any tracking software, telemetry libraries, analytics SDKs, advertising SDKs, or background crash-reporting servers.
> - **No User Accounts on Our Servers**: We do not maintain user databases, accounts, or cloud profiles. We operate no surveillance or data-harvesting infrastructure.
> - **Direct Peer-to-Peer & PSN Only**: When you stream, all audio, video, and controller data flows directly between your client device and your PlayStation console on your local network (or direct remote connection). When you log in, authentication occurs directly and exclusively with official PlayStation servers over encrypted HTTPS (TLS).
> - **100% Auditable Open-Source Code**: Every line of code, network socket call, and build script is publicly inspectable and auditable on GitHub at [github.com/nileshchakraborty/AstraPlay](https://github.com/nileshchakraborty/AstraPlay).

---

## 2. Google Play "Data Safety" & Apple App Store Privacy Disclosures

To assist users and comply with Google Play Data Safety declarations and Apple App Store Privacy Nutrition Labels:

| Data Category | Collected by AstraPlay? | Transmitted to Developer / 3rd Party? | Stored Locally on Device? | Purpose |
| :--- | :--- | :--- | :--- | :--- |
| **Personal Identifiers** (Name, Email, Phone, Address) | **No** | **No** | No | Not accessed or collected |
| **Location Data** (GPS or Network-based) | **No** | **No** | No | Not accessed or collected |
| **Financial / Payment Information** | **No** | **No** | No | Not accessed or collected |
| **Advertising / Tracking Identifiers** (AAID, IDFA) | **No** | **No** | No | Not accessed or collected |
| **Analytics / Diagnostic Usage Telemetry** | **No** | **No** | No | Not accessed or collected |
| **Contacts / Browsing History / Photos** | **No** | **No** | No | Not accessed or collected |
| **PSN Account ID & Session Tokens** | **No** | **No** (Direct to PSN only) | **Yes** (Secure Storage / Keychain) | Authenticating your Remote Play session |
| **Console Registration Keys & PINs** | **No** | **No** (Local LAN only) | **Yes** (Encrypted on device) | Connecting to your PlayStation |
| **Diagnostic Session Logs** | **No** | **No** (Manual export only) | **Yes** (Local text files on device) | Debugging connection issues (user controlled) |

---

## 3. Platform Permissions: Why They Are Required & How They Are Used

AstraPlay requests only the minimal system permissions required to enable local network discovery, low-latency audio/video streaming, and game controller input. Below is the complete rationale for every permission requested across all platforms:

### 3.1 Android Permissions (`AndroidManifest.xml`)

| Permission | Protection Level | Why It Is Required | How It Is Used | User Data Collected |
| :--- | :--- | :--- | :--- | :--- |
| **`android.permission.INTERNET`** | Normal | To stream audio/video and send controller inputs between your device and your PlayStation console, and communicate with PSN for authentication. | Opens TCP/UDP sockets for real-time Remote Play streaming and direct HTTPS requests to official PlayStation servers. | **Zero** |
| **`android.permission.ACCESS_NETWORK_STATE`** | Normal | To detect network connectivity changes (e.g., switching between Wi-Fi and mobile data). | Dynamically adapts streaming bitrate and buffer sizes to prevent frame stutter or packet loss during network handoffs. | **Zero** |
| **`android.permission.ACCESS_WIFI_STATE`** | Normal | To determine your device's local Wi-Fi IP address. | Generates and displays the exact local IP address on the Embedded Relay Server screen and in the pairing QR code (`http://<local-ip>:25525`) so you can pair from other devices on the same Wi-Fi network. | **Zero** (IP is never sent to any remote server) |
| **`android.permission.CHANGE_WIFI_MULTICAST_STATE`** | Normal | To receive UDP multicast/broadcast packets on your home Wi-Fi. | PlayStation consoles broadcast their presence and wake-up status using UDP packets. Without this permission, Android drops multicast packets to save battery, preventing automatic console discovery and console wake-up. | **Zero** |
| **`android.permission.VIBRATE`** | Normal | To deliver controller haptic feedback. | Converts in-game rumble and vibration events sent from your console (and on-screen touch controller feedback) into physical device vibrations. | **Zero** |
| **`android.permission.HIGH_SAMPLING_RATE_SENSORS`** | Normal | To access gyroscope and accelerometer data with low latency. | Reads motion sensors at high sampling rates to emulate DualSense / DualShock 6-axis motion aiming in supported PlayStation games. | **Zero** |

### 3.2 macOS Entitlements (`gui/entitlements.xml` & App Sandbox)

| Entitlement | Why It Is Required | How It Is Used | User Data Collected |
| :--- | :--- | :--- | :--- |
| **`com.apple.security.network.client`** | Outgoing network connections under macOS App Sandbox. | Connects to your PlayStation console on your local network or via remote IP, and accesses PlayStation login endpoints. | **Zero** |
| **`com.apple.security.network.server`** | Incoming network socket binding under macOS App Sandbox. | Powers the local Embedded Relay Server (`http://127.0.0.1:25525`) to receive OAuth login callback redirects from your default web browser, and binds incoming UDP streaming ports. | **Zero** |
| **`com.apple.security.device.usb`** | Raw USB HID controller communication under Hardened Runtime. | Enables the SDL library to access wired DualSense and DualShock 4 controllers via raw HID (`hidapi`) to support advanced haptics, adaptive triggers, and lightbar controls. | **Zero** |
| **`com.apple.security.device.bluetooth`** | Raw Bluetooth HID controller communication under Hardened Runtime. | Enables wireless DualSense and DualShock 4 controller rumble and haptics over Bluetooth. | **Zero** |
| **`com.apple.security.device.audio-input`** | Microphone access for in-game voice chat. | When you explicitly enable the in-stream microphone toggle, your voice is captured, encoded into Opus audio frames, and transmitted directly to your console's party chat. Voice data is never recorded, stored, or sent elsewhere. | **Zero** |
| **`com.apple.security.files.user-selected.read-write`** | Explicit file access via native open/save dialogs. | Allows importing or exporting settings configurations or saving diagnostic log files when you manually choose to do so in the file picker. | **Zero** |

### 3.3 iOS & iPadOS Permissions (`Info.plist`)

| Key | Usage Description Shown to User | Why It Is Required & How It Is Used | User Data Collected |
| :--- | :--- | :--- | :--- |
| **`NSLocalNetworkUsageDescription`** | *"AstraPlay needs local network access to discover and stream from PlayStation consoles on your home network."* | Required by iOS 14+ for local socket discovery. AstraPlay sends UDP broadcast packets on your local subnet to find your PS4/PS5 consoles automatically. | **Zero** |
| **`NSMicrophoneUsageDescription`** | *"For voice chat and microphone support, AstraPlay requires access to your microphone."* | Captures voice input only when the user explicitly un-mutes the in-stream microphone for PlayStation party chat. | **Zero** |
| **`NSBonjourServices` (`_psstream._tcp`)** | Declares local network mDNS discovery service. | System-level declaration enabling Bonjour discovery of consoles on the local network. | **Zero** |

### 3.4 Windows (`AppxManifest.xml`) & Linux

- **Windows (`runFullTrust`)**: Required for the desktop MSIX package to access raw gamepad HID devices (DualSense haptics / XInput / DirectInput), utilize low-latency hardware video decoders (D3D11VA, Vulkan), and bind UDP streaming sockets.
- **Linux (`udev` / `hidraw`)**: Requires udev rules (`71-chiaki4deck.rules`) solely to allow non-root users direct access to DualSense motion sensors and touchpads via `/dev/hidraw*`.

---

## 4. How Information Is Handled Strictly on Your Device

AstraPlay handles three specific types of data, all of which remain under your complete control:

1. **PSN Account Credentials & Session Tokens**:
   - In order to discover registered consoles or authenticate Cloud Gaming sessions, the Application requests your PlayStation Network account identifier and session tokens (such as NPSSO).
   - **How it is used**: Tokens are sent **exclusively and directly** to official PlayStation servers over encrypted HTTPS (TLS).
   - **Local Storage Security**: Session tokens are stored using platform-native encrypted storage primitives (Android Keystore / EncryptedSharedPreferences, macOS Keychain, and iOS Keychain).
   - **No Intermediary**: AstraPlay developers operate no servers that receive, see, or store your tokens.

2. **Console Registration Keys & PINs**:
   - Pairing data, console nickname, local IP address, and registration keys are saved locally on your device to facilitate automatic reconnection.

3. **Diagnostic Session Logs**:
   - Streaming metrics (bitrate, frame drops, network jitter, audio buffer states) are processed in memory and written to local log files (`astraplay_session_*.log`) on your device.
   - Diagnostic logs are **never** uploaded automatically. You retain full control and must manually choose to share a log file when asking for troubleshooting help.

---

## 5. Third-Party Services Disclaimer

- **Sony Interactive Entertainment**: AstraPlay communicates directly with services operated by Sony Interactive Entertainment Inc. ("Sony"). Your interaction with Sony servers is governed by Sony's terms and the [PlayStation Privacy Policy](https://www.playstation.com/legal/privacy-policy/).
- **Zero Third-Party Trackers**: AstraPlay contains **zero** third-party advertising SDKs, analytic SDKs, social media trackers, or third-party cloud data backends.

---

## 6. User Rights & Data Deletion

Because all configuration and authentication data is stored exclusively on your own device:

- **Complete Data Control**: You can delete all tokens, pairing keys, and saved settings at any time by:
  1. In AstraPlay, going to **Settings → Manage Consoles** and deleting your registered consoles, or clicking **Clear Token**.
  2. In your operating system settings (e.g., Android Settings → Apps → AstraPlay), tapping **Clear Data / Clear Storage**.
  3. Uninstalling AstraPlay from your device.

---

## 7. Contact & Inquiries

For questions or inquiries regarding our privacy practices:

- **Maintainer**: Nilesh Chakraborty
- **Project Repository**: [https://github.com/nileshchakraborty/AstraPlay](https://github.com/nileshchakraborty/AstraPlay)
- **Issues & Discussions**: [https://github.com/nileshchakraborty/AstraPlay/discussions](https://github.com/nileshchakraborty/AstraPlay/discussions)
- **Email**: `nileshchakraborty@gmail.com`
