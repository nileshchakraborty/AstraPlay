# Installing AstraPlay

AstraPlay provides native application packages, automated installers, and store distribution options for all major operating systems and handheld gaming devices.

---

=== "Windows (10 & 11)"

    AstraPlay supports all modern 64-bit Windows installations on both **x86_64** (Intel/AMD) and **ARM64** (Qualcomm Snapdragon X Elite / Windows on ARM).

    ### Option 1: One-Line PowerShell Installer — Recommended

    Install, configure shortcuts, and launch AstraPlay in one automated PowerShell command:

    ```powershell
    irm https://raw.githubusercontent.com/nileshchakraborty/AstraPlay/master/scripts/install-windows.ps1 | iex
    ```

    ### Option 2: Setup Installer (`.exe`)

    The Inno Setup installer configures shortcuts, file associations, and bundled runtime dependencies:

    1. Download [`astraplay-windows-installer.exe`](https://github.com/nileshchakraborty/AstraPlay/releases/latest/download/astraplay-windows-installer.exe) (or `astraplay-windows-arm64-installer.exe` for ARM64).
    2. Run the executable and follow the wizard instructions.
    3. Select whether to create Start Menu and Desktop shortcuts (Start Menu is enabled by default).
    4. Launch **AstraPlay** from your Start Menu.

    ### Option 3: Windows Package Manager (`winget`)

    Install or update AstraPlay from PowerShell or Windows Terminal:

    ```powershell
    winget install AstraPlay
    ```

    To upgrade AstraPlay in the future:
    ```powershell
    winget upgrade AstraPlay
    ```

    ### Option 4: Enterprise MSI Package

    For IT administrators or automated enterprise deployments:

    ```powershell
    msiexec /i astraplay-windows-x64.msi /quiet
    ```

    Options:
    - `CREATE_START_MENU_SHORTCUT=0` to skip Start Menu entry
    - `CREATE_DESKTOP_SHORTCUT=1` to add desktop icon

    ### Option 5: Portable ZIP

    Zero-installation runtime for USB drives or standard user accounts:

    1. Download [`astraplay-windows-x64.zip`](https://github.com/nileshchakraborty/AstraPlay/releases/latest).
    2. Extract the archive into any folder.
    3. Launch `AstraPlay.exe`.

=== "Linux & Handhelds (Steam Deck / Bazzite / ROG Ally)"

    AstraPlay is optimized for handheld gaming consoles like the **Steam Deck**, **ROG Xbox Ally X**, **Lenovo Legion Go**, and **MSI Claw**, as well as desktop Linux distributions (Ubuntu, Fedora, Arch, Debian, openSUSE).

    ### Option 1: One-Line User Installer — Recommended for Handhelds

    Run this automated command in terminal / Konsole (no `sudo` required):

    ```bash
    curl -fsSL https://raw.githubusercontent.com/nileshchakraborty/AstraPlay/master/scripts/install-appimage.sh | bash
    ```

    **What this does automatically:**
    - Detects your CPU architecture (`x86_64` or `aarch64`)
    - Downloads the latest verified release from GitHub
    - Verifies release checksums against `SHA256SUMS.txt`
    - Installs the binary into `~/.local/opt/astraplay/AstraPlay.AppImage`
    - Creates a terminal launcher at `~/.local/bin/astraplay`
    - Registers a desktop menu shortcut with high-resolution 512x512 icons
    - Supports clean in-place updates by re-running the command anytime

    ### Option 2: Flathub / Flatpak

    Install via Flatpak from terminal or your software manager (KDE Discover / GNOME Software):

    ```bash
    flatpak install flathub io.github.nileshchakraborty.AstraPlay
    flatpak run io.github.nileshchakraborty.AstraPlay
    ```

    ### Option 3: Direct AppImage

    1. Download [`AstraPlay-Linux-x86_64.AppImage`](https://github.com/nileshchakraborty/AstraPlay/releases/latest/download/AstraPlay-Linux-x86_64.AppImage).
    2. Make it executable:
       ```bash
       chmod +x AstraPlay-Linux-x86_64.AppImage
       ```
    3. Run directly:
       ```bash
       ./AstraPlay-Linux-x86_64.AppImage
       ```

    ### Option 4: Portable ZIP with `launch.sh`

    1. Download [`astraplay-linux-x86_64.zip`](https://github.com/nileshchakraborty/AstraPlay/releases/latest).
    2. Extract the archive and execute `./launch.sh`.
    3. Bundled runtime libraries and OpenSSL certificates are configured automatically.

    ### Adding to Steam Gaming Mode (Steam Deck & Bazzite)

    To play seamlessly with full gamepad navigation and Gamescope overlay:

    1. Open **Steam** in **Desktop Mode**.
    2. Click **Games → Add a Non-Steam Game to My Library...**
    3. Check **AstraPlay** in the list (or browse to `~/.local/bin/astraplay` or the extracted `launch.sh`).
    4. Click **Add Selected Programs**.
    5. Return to **Gaming Mode**. AstraPlay will be in your **Non-Steam** library ready to stream.

=== "macOS (Apple Silicon & Intel)"

    AstraPlay runs natively on macOS Sonoma and Sequoia with Metal hardware acceleration, low-latency CoreAudio, and DualSense controller support.

    ### Option 1: One-Line Terminal Installer — Recommended (Zero Gatekeeper Warnings)

    The fastest and most reliable way to install AstraPlay on modern macOS systems:

    ```bash
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/nileshchakraborty/AstraPlay/master/scripts/install-macos.sh)"
    ```

    **Why this is recommended on macOS Sonoma & Sequoia:**
    - **Bypasses Gatekeeper without warnings**: Downloads performed via `curl` inside Terminal never inherit Apple's `com.apple.quarantine` extended attribute, completely avoiding the *"Apple could not verify..."* error.
    - **Automatic CPU Detection**: Detects Apple Silicon (M1–M4) or Intel and downloads the matching build.
    - **Checksum Verification**: Verifies integrity against `SHA256SUMS.txt`.
    - **Instant Clean Launch**: Installs to `/Applications/AstraPlay.app`, validates ad-hoc signatures, and opens AstraPlay immediately.

    ### Option 2: Native 1-Click Installer ZIP

    1. Download [`AstraPlay-macOS-arm64-installer.zip`](https://github.com/nileshchakraborty/AstraPlay/releases/latest) (Apple Silicon M1–M4) or [`AstraPlay-macOS-x86_64-installer.zip`](https://github.com/nileshchakraborty/AstraPlay/releases/latest) (Intel).
    2. Double-click to extract the ZIP archive.
    3. Double-click **`Install AstraPlay.app`** (or `Install AstraPlay.command`).
    4. Click **Install** and authenticate when prompted via **Touch ID** or your Mac password.
    5. AstraPlay is automatically installed into `/Applications/AstraPlay.app` with proper code signatures.

    ### Option 3: Drag-and-Drop Disk Image (`.dmg`) & Gatekeeper Handling

    1. Download [`AstraPlay-macOS-arm64.dmg`](https://github.com/nileshchakraborty/AstraPlay/releases/latest) (Apple Silicon M1–M4) or [`AstraPlay-macOS-x86_64.dmg`](https://github.com/nileshchakraborty/AstraPlay/releases/latest) (Intel).
    2. Open the downloaded `.dmg` disk image.
    3. Double-click `Install AstraPlay.app` inside the disk image, or drag **AstraPlay** into `/Applications`.
    4. **If macOS Sequoia or Sonoma displays** *"Apple could not verify AstraPlay is free of malware"*:
        - Click **Done** on the alert modal.
        - Open **System Settings → Privacy & Security**.
        - Scroll down to the **Security** section.
        - You will see: *"AstraPlay was blocked to protect your Mac."*
        - Click **Open Anyway**, enter your Mac password or Touch ID, and click **Open**.
        - *(Or run `xattr -cr /Applications/AstraPlay.app` in Terminal).*
        - You only need to do this once; AstraPlay will launch normally thereafter.

    ### Option 4: Homebrew Cask

    Install via Homebrew terminal:

    ```bash
    brew install --cask astraplay
    ```

    To upgrade via Homebrew:
    ```bash
    brew upgrade --cask astraplay
    ```

    ### Option 5: Portable App Bundle ZIP

    Download and extract `AstraPlay-macOS-arm64.zip` directly into `~/Applications`.

=== "Android & Android TV"

    AstraPlay supports modern Android phones, foldable devices, tablets, and Android TV / Google TV streaming boxes.

    ### Option 1: Google Play Store — Recommended

    Install directly from Google Play:

    - [:simple-googleplay: Get it on Google Play](https://github.com/nileshchakraborty/AstraPlay/releases/latest){ target="_blank" rel="noopener" }

    Automatic updates, cloud saves, and TV leanback launcher integrations are enabled out of the box.

    ### Option 2: Sideload Release APK

    1. Download [`AstraPlay-release.apk`](https://github.com/nileshchakraborty/AstraPlay/releases/latest) on your Android device.
    2. Tap the downloaded APK in your notification shade or file manager.
    3. Allow "Install unknown apps" if prompted by your browser or file manager.
    4. Tap **Install** to complete setup.

    ### Option 3: Android TV & Fire TV Sideloading via ADB

    If installing to a Smart TV or streaming device (e.g. Chromecast with Google TV, NVIDIA Shield, Fire TV Cube):

    ```bash
    # Enable ADB debugging in Developer Options on your TV
    adb connect <tv-ip-address>:5555
    adb install -r AstraPlay-release.apk
    ```

=== "iOS & iPadOS"

    AstraPlay delivers full-screen remote streaming on iPhone and iPad with native touch overlays, DualSense adaptive triggers, and Apple M-series hardware decoding.

    ### Option 1: Apple App Store / TestFlight — Recommended

    Install directly via the App Store:

    - [:simple-apple: Download on the App Store](https://github.com/nileshchakraborty/AstraPlay/releases/latest){ target="_blank" rel="noopener" }

    ### Option 2: Sideloading Unsigned IPA (`.ipa`)

    Download [`AstraPlay.ipa`](https://github.com/nileshchakraborty/AstraPlay/releases/latest) and install using your preferred method:

    - **AltStore / SideStore**: Add the IPA file directly in your on-device AltStore app.
    - **Sideloadly**: Connect your iPhone or iPad to your PC or Mac via USB and drag the `.ipa` into Sideloadly to install.
    - **LiveContainer / TrollStore**: Install the IPA directly without weekly resigning limits (supported on iOS 14.0–17.0).

