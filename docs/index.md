# AstraPlay — PS4 & PS5 Remote Play for Everyone

**AstraPlay** is a free, open-source PS4 and PS5 Remote Play client for Android, Android TV, iOS, iPadOS, macOS, Windows, Linux, and Steam Deck. It focuses on easy app-store installs, Internet Play (stream games from the game catalog or your owned game library), automatic console discovery, and a touch-friendly mobile UI — all from one community-maintained codebase.

## Download

Desktop and mobile releases are published on the [releases page](https://github.com/nileshchakraborty/AstraPlay/releases/latest){ target="_blank" rel="noopener" } and documented at [AstraPlay](https://nileshchakraborty.github.io/AstraPlay){ target="_blank" rel="noopener" }:

=== "Android / Android TV"
    [:simple-googleplay: Get it on Google Play](https://github.com/nileshchakraborty/AstraPlay/releases/latest){ .md-button .md-button--primary target="_blank" rel="noopener" }

    Works on phones, tablets, and Android TV devices.

=== "iOS / iPadOS"
    [:simple-apple: Download on the App Store](https://github.com/nileshchakraborty/AstraPlay/releases/latest){ .md-button .md-button--primary target="_blank" rel="noopener" }

=== "macOS"
    [:simple-apple: Download on the Mac App Store](https://github.com/nileshchakraborty/AstraPlay/releases/latest){ .md-button .md-button--primary target="_blank" rel="noopener" }

=== "Windows"
    [:material-download: Download Installer](https://github.com/nileshchakraborty/AstraPlay/releases/latest){ .md-button .md-button--primary target="_blank" rel="noopener" }

    [:material-folder-zip-outline: Download Portable zip](https://github.com/nileshchakraborty/AstraPlay/releases/latest){ .md-button .md-button--primary target="_blank" rel="noopener" }

=== "Linux / Steam Deck"
    !!! tip "Steam Deck"
        A Flathub listing is not available yet. The [AppImage installer](setup/installation.md#linux-steam-deck) creates a desktop application entry; you can add it as a non-Steam game for Gaming Mode. The portable ZIP remains available.

    [:material-download: Download portable ZIP](https://github.com/nileshchakraborty/AstraPlay/releases/latest){ .md-button .md-button--primary target="_blank" rel="noopener" }

    [:material-download: Download AppImage](https://github.com/nileshchakraborty/AstraPlay/releases/latest){ .md-button .md-button--primary target="_blank" rel="noopener" }

    **Flatpak:** The Flathub listing is not published yet.

## Getting Started

Head to the [Setup section](setup/index.md) to get AstraPlay running on your device:

1. [Install](setup/installation.md) from your platform's app store or the releases page
2. [Configure](setup/configuration.md) and register your PlayStation console
3. [Set up a Remote Connection](setup/remoteconnection.md) to connect to your console outside your home network
4. [Configure your controller](setup/controlling.md)

## Key Features

- **Internet Play** — stream games from the game catalog or your owned game library
- **Automatic console discovery** — finds PS4/PS5 consoles on your local network automatically
- **Cross-platform** — Android, Android TV, iOS, macOS, Windows, Linux, Steam Deck
- **In-Stream Aspect Ratio Presets** — Fit (16:9), Fill / 16:10 (Steam Deck & ROG Ally), 20:9 (smartphones), Zoom, and Stretch
- **Contrast Adaptive Sharpening (CAS)** — integrated FidelityFX CAS and Lanczos shaders for high-fidelity upscaling
- **Dynamic Bitrate & Congestion Control** — real-time adaptive bitrate heuristics preventing dropped frames and audio stutter
- **Controller Battery Indicator HUD** — real-time battery status and charging state on the in-stream HUD
- **Low-Latency Audio Tuning** — configurable audio buffer sizing (Ultra-Low to High) and in-stream mic muting
- **Touch & Haptic Innovation** — virtual thumbstick tactile haptics, customizable drag-and-drop overlays, and DualSense touchpad emulation
- **App-store installs** — available on Google Play, App Store, and Mac App Store
- **Open source** — AGPL-3.0 licensed, community-maintained

## Contributing

Fork the repo, create a branch off `dev`, and open a pull request targeting `dev`. Release PRs go from `dev` to `master`; merging one triggers the release build. See [CONTRIBUTING.md](https://github.com/nileshchakraborty/AstraPlay/blob/master/CONTRIBUTING.md){ target="_blank" rel="noopener" } for the full workflow.

## Acknowledgements

AstraPlay is built on top of [Chiaki](https://git.sr.ht/~thestr4ng3r/chiaki){ target="_blank" rel="noopener" } and [chiaki-ng](https://github.com/streetpea/chiaki-ng){ target="_blank" rel="noopener" }. Special thanks to the Chiaki authors and the chiaki-ng maintainers for their foundational work. AstraPlay extends that work with a focus on app-store distribution, mobile support, Android TV, and Internet Play.

For questions or support, open a [GitHub Discussion](https://github.com/nileshchakraborty/AstraPlay/discussions){ target="_blank" rel="noopener" } or contact [contact@nileshchakraborty.com](mailto:contact@nileshchakraborty.com).
