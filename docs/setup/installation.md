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
        Flatpak is the recommended install for Steam Deck — it integrates with the Discover store and updates automatically.

    !!! Tip "Copying from and Pasting into Konsole Windows"

        You can copy from and paste into `konsole` windows with ++ctrl+shift+c++ (copy) and ++ctrl+shift+v++ (paste) instead of the normal ++ctrl+c++ (copy) and ++ctrl+v++ (paste) shortcuts.

    === "Flatpak (Recommended)"

        === "Using the Discover Store"

            1. Open the Discover store

                ![Open Discover](images/OpenDiscover.png)

            2. Search for `AstraPlay` in the search bar

            3. Click Install

        === "Using the `konsole`"

            1. Run the following command in the `konsole`

                ```
                flatpak install flathub io.github.nileshchakraborty.AstraPlay -y
                ```

        !!! Note "About the AstraPlay Flatpak"

            The AstraPlay flatpak is available on [Flathub](https://github.com/nileshchakraborty/AstraPlay/releases/latest){ target="_blank" rel="noopener" }.

            You can also build the flatpak yourself by following the instructions in [Building the Flatpak Yourself](../diy/buildit.md){ target="_blank" rel="noopener" }.

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

        2. Unzip and run `launch.sh` (sets up bundled libs and OpenSSL fallback automatically)
