# Privacy Policy for AstraPlay

**Effective Date:** October 18, 2026  
**Last Updated:** October 18, 2026

**AstraPlay** ("the Application") is an open-source, community-driven PlayStation Remote Play and Cloud Streaming client developed by Nilesh Chakraborty and the AstraPlay open-source community ("we", "us", or "our"). 

This Privacy Policy explains our practices regarding user privacy, data collection, device permissions, and regulatory compliance under the **Google Play Developer Policy**, **Apple App Store Review Guidelines**, the **General Data Protection Regulation (GDPR / UK-GDPR)**, the **California Consumer Privacy Act (CCPA / CPRA)**, and the **Children's Online Privacy Protection Act (COPPA)**.

---

## 1. Summary of Core Privacy Principles

- **Zero Remote Data Collection**: We do not operate telemetry, tracking, advertising, analytics, or surveillance servers. We do not collect, store, sell, or monetize your personal data.
- **Strictly Local Processing**: All authentication tokens, console registration keys, device IDs, and settings remain solely on your local device or communicate directly with official Sony PlayStation servers.
- **Open-Source Auditing**: AstraPlay is 100% open-source software licensed under the **GNU Affero General Public License v3 (AGPL-3.0)**. Its complete source code and network traffic patterns are publicly auditable at [github.com/nileshchakraborty/AstraPlay](https://github.com/nileshchakraborty/AstraPlay).

---

## 2. Google Play "Data Safety" & App Store Privacy Disclosure

To assist users and comply with Google Play Data Safety and Apple App Privacy declarations:

| Data Type | Collected by Developer? | Shared with Third Parties? | Handled Locally on Device? |
| :--- | :--- | :--- | :--- |
| **Personal Identifiers** (Name, Email, Phone, Address) | **No** | **No** | No |
| **Location Data** (Precise or Approximate GPS) | **No** | **No** | No |
| **Financial / Payment Information** | **No** | **No** | No |
| **Advertising / Tracking Identifiers** (AAID, IDFA) | **No** | **No** | No |
| **Analytics / Usage Tracking** | **No** | **No** | No |
| **Browsing History / Contacts / Photos** | **No** | **No** | No |
| **PSN Account ID & Session Tokens** | **No** | **No** (Sent only to Sony PSN) | **Yes** (Encrypted storage) |
| **Console Registration Keys & PINs** | **No** | **No** (Local LAN only) | **Yes** (Encrypted storage) |
| **Diagnostic Session Logs** | **No** | **No** | **Yes** (Local storage only) |

---

## 3. Information Handled Strictly on Your Device

AstraPlay processes certain operational data strictly within your local environment to enable Remote Play and Cloud Streaming:

1. **PlayStation Network (PSN) Account Credentials & Session Tokens**:
   - In order to discover consoles, register Remote Play, or authenticate Cloud Gaming sessions, the Application requests your PlayStation Network account identifier and session tokens (such as NPSSO).
   - **How it is used**: Authentication tokens are communicated **exclusively and directly** between your client device and Sony PlayStation official servers over encrypted Transport Layer Security (TLS/HTTPS).
   - **Local Storage Security**: Session tokens are stored using platform-native secure storage primitives (Android Keystore / EncryptedSharedPreferences, macOS Keychain, and iOS Keychain).
   - **No Intermediary Servers**: Neither AstraPlay developers nor any third parties have access to, receive, or intermediate your PSN credentials.

2. **Console Registration Keys & PINs**:
   - Pairing data, console nickname, local IP address, and 8-digit registration PINs are saved locally in your application database to facilitate automatic reconnection.

3. **Diagnostic Session Logs**:
   - Real-time streaming metrics (bitrate, frame drops, network jitter, audio buffer states) are processed in memory and written to local log files on your device.
   - Diagnostic logs are **never** uploaded automatically. Users retain complete control and must manually choose to export or share log files when troubleshooting issues on GitHub.

---

## 4. Device Permissions & Rationale (Android & Multi-Platform)

AstraPlay requests only the minimal system permissions required to deliver video and audio streaming and controller input:

- **`android.permission.INTERNET`**:
  - *Purpose*: To communicate with your PlayStation console over your local Wi-Fi/LAN and with Sony PSN servers for session negotiation and Cloud Streaming.
- **`android.permission.ACCESS_NETWORK_STATE`**:
  - *Purpose*: To monitor Wi-Fi, Ethernet, and cellular connection transitions to optimize video streaming bitrate and prevent packet loss during network handoffs.
- **`android.permission.VIBRATE`**:
  - *Purpose*: To deliver DualSense, DualShock, and Bluetooth gamepad haptic feedback and on-screen touch controller vibration.
- **`android.permission.HIGH_SAMPLING_RATE_SENSORS`**:
  - *Purpose*: To read high-frequency gyroscope and accelerometer data for low-latency motion control aiming.
- **`android.permission.RECORD_AUDIO` / Microphone Access** *(Optional / Runtime Prompt)*:
  - *Purpose*: To transmit your voice to in-game party chat when the in-game microphone toggle is actively enabled by the user. Voice audio is streamed directly to the console/session and is never recorded, saved, or uploaded to any third party.
- **Nearby Devices / Bluetooth (`BLUETOOTH_CONNECT` / `BLUETOOTH_SCAN`)** *(Android 12+)*:
  - *Purpose*: To connect and receive button/axis inputs from external wireless game controllers (DualSense, Xbox, DualShock 4).

---

## 5. Third-Party Services & Sony Interactive Entertainment Disclaimer

- **Sony PlayStation Network (PSN)**: When authenticating or streaming, AstraPlay connects directly to services operated by Sony Interactive Entertainment Inc. ("Sony"). Your interaction with Sony PSN servers is governed by Sony's terms of service and [PlayStation Privacy Policy](https://www.playstation.com/legal/privacy-policy/).
- **Third-Party SDKs**: AstraPlay contains **zero** third-party advertising SDKs, analytic SDKs, social media trackers, or third-party cloud data backends.

---

## 6. User Rights, Data Retention & Erasure

Because all configuration and authentication data is stored exclusively on your device:

- **Right to Access & Portability**: You have full access to your stored configurations, host entries, and logs directly within your application settings or device file system.
- **Right to Erasure (Data Deletion)**: You can completely delete all stored data, tokens, and settings at any time by:
  1. Opening **Settings** in AstraPlay and clicking **Log out** or **Delete Registered Console**.
  2. In your device settings (e.g., Android Settings → Apps → AstraPlay), tapping **Clear Data / Clear Storage**.
  3. Uninstalling the Application from your device.

---

## 7. Compliance with Regional Privacy Laws

### General Data Protection Regulation (GDPR / UK GDPR)
- We act neither as a data controller nor as a data processor of personal data on remote servers, as all personal data remains under your direct physical and technical custody on your local device.
- To the extent that local processing occurs, it is based on your explicit consent and contractual necessity to provide the streaming functionality requested.

### California Consumer Privacy Act (CCPA / CPRA)
- We do **not** sell, rent, disclose, or share personal information with third parties for commercial or behavioral advertising purposes. We have not sold or shared any consumer personal information in the preceding 12 months.

### Children’s Online Privacy Protection Act (COPPA)
- AstraPlay is not directed to children under the age of 13 (or under 16 in the European Economic Area). We do not knowingly collect or solicit personal information from children.

---

## 8. Changes to this Privacy Policy

We may update this Privacy Policy from time to time to reflect changes in legal requirements, operating system policies, or application features. All updates will be published with a revised "Last Updated" date at:
[https://nileshchakraborty.github.io/AstraPlay/privacy/](https://nileshchakraborty.github.io/AstraPlay/privacy/)

---

## 9. Contact & Inquiries

For any questions, compliance inquiries, or concerns regarding this Privacy Policy, please contact the maintainer:

- **Maintainer**: Nilesh Chakraborty
- **Project Repository**: [https://github.com/nileshchakraborty/AstraPlay](https://github.com/nileshchakraborty/AstraPlay)
- **Public Support & Issues**: [https://github.com/nileshchakraborty/AstraPlay/issues](https://github.com/nileshchakraborty/AstraPlay/issues)
- **Email Contact**: `nileshchakraborty@gmail.com`
