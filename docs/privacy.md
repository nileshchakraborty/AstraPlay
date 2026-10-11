# Privacy Policy

Last updated: October 11, 2026

**AstraPlay** is an open-source PlayStation Remote Play and Cloud Streaming client developed by the community. Your privacy is paramount.

---

## 1. Information We Do Not Collect

- **No Personal Data Collection**: AstraPlay does not collect, harvest, store on remote servers, or transmit your personal data, names, emails, physical locations, or contacts.
- **No Analytics / No Tracking**: AstraPlay contains no third-party tracking libraries, analytics trackers, or advertising SDKs.
- **No Monetization / Data Selling**: We do not sell, rent, or trade any user information.

---

## 2. Information Handled Locally on Your Device

All sensitive credentials and connection data remain strictly on your local device:

- **PlayStation Network (PSN) Account ID & Tokens**: Used solely to authenticate directly with Sony PlayStation servers to establish your Remote Play and Cloud sessions. Authentication tokens (such as NPSSO) are stored securely in your device's local encrypted storage (e.g. Android EncryptedSharedPreferences / Keychain) and are communicated exclusively to official PlayStation API endpoints over secure HTTPS.
- **Console Registration Data & PINs**: Stored locally on your device in local preferences / databases to facilitate reconnecting to your consoles.
- **Crash Logs & Diagnostics**: Session logs and debugging output are kept locally on your device. Logs are never uploaded automatically; you have complete discretion whether to export or share log files for troubleshooting.

---

## 3. Network Communication

When operating, AstraPlay communicates directly with:

1. **Your Local PlayStation Console**: Over your local network (LAN / Wi-Fi) using standard Remote Play UDP/TCP protocols.
2. **Sony PlayStation Network (PSN) Services**: Directly between your client and PlayStation authentication / cloud servers to negotiate streaming sessions.

None of this network traffic passes through any third-party intermediary servers operated by AstraPlay developers.

---

## 4. Open Source Transparency

AstraPlay is licensed under the **GNU Affero General Public License v3 (AGPL-3.0)**. The complete source code is publicly accessible and auditable:

- Source Code: [https://github.com/nileshchakraborty/AstraPlay](https://github.com/nileshchakraborty/AstraPlay)
- Issues & Support: [https://github.com/nileshchakraborty/AstraPlay/issues](https://github.com/nileshchakraborty/AstraPlay/issues)

---

## 5. Contact

If you have questions regarding this Privacy Policy, please open an issue or discussion on GitHub:  
[https://github.com/nileshchakraborty/AstraPlay](https://github.com/nileshchakraborty/AstraPlay)
