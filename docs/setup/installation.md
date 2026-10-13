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
        A Flathub listing is not available yet. On Steam Deck or Bazzite, the portable ZIP is the easiest current option: add its `launch.sh` as a non-Steam game for Gaming Mode.

    !!! Tip "Copying from and Pasting into Konsole Windows"

        You can copy from and paste into `konsole` windows with ++ctrl+shift+c++ (copy) and ++ctrl+shift+v++ (paste) instead of the normal ++ctrl+c++ (copy) and ++ctrl+v++ (paste) shortcuts.

    !!! note "Flatpak availability"
        AstraPlay's Flatpak build is currently verified in CI, but it is not published to Flathub yet. Install instructions will be added when the human-reviewed Flathub submission is published.

    === "AppImage"

        1. Download the [AppImage for your architecture](https://github.com/nileshchakraborty/AstraPlay/releases/latest){ target="_blank" rel="noopener" }

        2. Make it executable and run it:

            ```bash
            chmod +x AstraPlay-Linux-x86_64.AppImage
            ./AstraPlay-Linux-x86_64.AppImage
            ```

            !!! Tip "Steam Deck / Steam"
                Set `APPIMAGE_EXTRACT_AND_RUN=1` if Steam misbehaves with the AppImage.

    === "Portable zip"

        1. Download the [portable zip for your platform](https://github.com/nileshchakraborty/AstraPlay/releases/latest){ target="_blank" rel="noopener" }

        2. Extract the archive, then run `launch.sh` (sets up bundled libraries and the OpenSSL fallback automatically).

        3. To launch it in Gaming Mode, open Steam in Desktop Mode, choose **Games → Add a Non-Steam Game**, browse to the extracted `launch.sh`, and add it. Switch back to Gaming Mode and start AstraPlay from your library.
