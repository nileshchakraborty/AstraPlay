# Windows MSIX development package

`scripts/build-windows-msix.py` packages the same complete Qt runtime used by the Windows portable ZIP. The default `AstraPlay.Development` identity and `CN=AstraPlay Development` publisher are for build validation only. The on-demand `build-artifact-windows.yml` workflow creates **unsigned development artifacts**; do not publish or recommend them to end users.

On Windows with the Windows SDK installed:

```powershell
python scripts/build-windows-msix.py astraplay-windows-x64.zip 2.11.4.15 x64 astraplay-x64.msix
```

For a Microsoft Store submission, first reserve the app in Partner Center. Pass the exact package identity name and publisher subject shown there with `--package-name` and `--publisher`. The script maps release tag `v2.11.4.15` to MSIX version `2.11.4015.0`: the Store reserves the fourth field as `0`, while this mapping keeps release iterations ordered. Store distribution signs the accepted package. A GitHub-hosted MSIX requires a separately trusted signing certificate and must not reuse this unsigned artifact.

Before Store submission, test a signed development package on clean x64 and ARM64 Windows machines: install, launch, Remote Play, Cloud Play, PSN login, controller input, settings persistence, upgrade, and uninstall. Existing EXE/MSI installations are separate Windows packages; test migration of user data and disable the EXE/MSI self-updater for the MSIX build before publishing.
