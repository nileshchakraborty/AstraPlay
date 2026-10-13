# Installing AstraPlay

=== "Android / Android TV"

    Install directly from the Play Store:

    [Get it on Google Play](https://github.com/nileshchakraborty/AstraPlay/releases/latest){ target="_blank" rel="noopener" }

    Works on phones, tablets, and Android TV devices.

=== "iOS / iPadOS"

    Install from the App Store:

    [Download on the App Store](https://github.com/nileshchakraborty/AstraPlay/releases/latest){ target="_blank" rel="noopener" }

=== "macOS"

    Install from the Mac App Store:

    [Download on the Mac App Store](https://github.com/nileshchakraborty/AstraPlay/releases/latest){ target="_blank" rel="noopener" }

=== "Windows"

    Get the latest release from the [releases page](https://github.com/nileshchakraborty/AstraPlay/releases/latest){ target="_blank" rel="noopener" }:

    - **Installer** — recommended for most users
    - **Portable zip** — unzip and run, no install required

=== "Linux / Steam Deck"

    !!! tip "Steam Deck"
        A Flathub listing is not available yet. The AppImage installer below adds AstraPlay to the desktop application menu; the portable ZIP is another current option.

    !!! Tip "Copying from and Pasting into Konsole Windows"

        You can copy from and paste into `konsole` windows with ++ctrl+shift+c++ (copy) and ++ctrl+shift+v++ (paste) instead of the normal ++ctrl+c++ (copy) and ++ctrl+v++ (paste) shortcuts.

    !!! note "Flatpak availability"
        AstraPlay's Flatpak build is currently verified in CI, but it is not published to Flathub yet. Install instructions will be added when the human-reviewed Flathub submission is published.

    === "AppImage with user installer"

        Download the [installer script](https://github.com/nileshchakraborty/AstraPlay/blob/dev/scripts/install-appimage.sh){ target="_blank" rel="noopener" } and run it from Konsole:

        ```bash
        curl -fL -o install-appimage.sh https://raw.githubusercontent.com/nileshchakraborty/AstraPlay/dev/scripts/install-appimage.sh
        bash install-appimage.sh
        ```

        It downloads the latest AppImage for your CPU, verifies the release checksum, and creates an AstraPlay entry in the desktop application menu. It installs under your home directory, without `sudo`. Run it again to update.

        If you already downloaded the [AppImage](https://github.com/nileshchakraborty/AstraPlay/releases/latest){ target="_blank" rel="noopener" }, use `bash install-appimage.sh --file ~/Downloads/AstraPlay-Linux-x86_64.AppImage` instead. Download `SHA256SUMS.txt` beside it if you want the script to verify this local file. If FUSE is unavailable, add `--extract-and-run`; this is slower because the AppImage is extracted on every launch.

        To use the installed app in Steam Gaming Mode, add **AstraPlay** as a non-Steam game from Steam Desktop Mode. The installer leaves existing Steam shortcuts and controller layouts unchanged. On the ROG Xbox Ally X with Bazzite, also [map a rear paddle to the PS button](controlling.md#rog-xbox-ally-x-on-bazzite).

    === "Portable zip"

        1. Download the [portable zip for your platform](https://github.com/nileshchakraborty/AstraPlay/releases/latest){ target="_blank" rel="noopener" }

        2. Extract the archive, then run `launch.sh` (sets up bundled libraries and the OpenSSL fallback automatically). This is portable but does not create an application-menu entry or handle updates.

        3. To launch it in Gaming Mode, open Steam in Desktop Mode, choose **Games → Add a Non-Steam Game**, browse to the extracted `launch.sh`, and add it. Switch back to Gaming Mode and start AstraPlay from your library.
