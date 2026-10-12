# Windows MSIX development package

`scripts/build-windows-msix.py` packages the same complete Qt runtime used by the Windows portable ZIP. The default `AstraPlay.Development` identity and `CN=AstraPlay Development` publisher are for build validation only. The on-demand `build-artifact-windows.yml` workflow creates **unsigned development artifacts**; do not publish or recommend them to end users.

On Windows with the Windows SDK installed:

```powershell
python scripts/build-windows-msix.py astraplay-windows-x64.zip 2.11.4.15 x64 astraplay-x64.msix
```

For a Microsoft Store submission, first reserve the app in Partner Center. Pass the exact package identity name and publisher subject shown there with `--package-name` and `--publisher`. The script maps release tag `v2.11.4.15` to MSIX version `2.11.4015.0`: the Store reserves the fourth field as `0`, while this mapping keeps release iterations ordered. Store distribution signs the accepted package. A GitHub-hosted MSIX requires a separately trusted signing certificate and must not reuse this unsigned artifact.

After a release containing the MSIX updater guard is published from `master`, dispatch **Prepare Windows Store MSIX** on `master` with its release tag and the exact Partner Center identity values. The workflow downloads the published x64 and ARM64 portable ZIPs, verifies both against `SHA256SUMS.txt`, checks that the release source has the MSIX updater guard, and produces two unsigned Store submission packages as a short-lived private workflow artifact. Review the artifact's package identity, version, and architecture before uploading each package to Partner Center. The workflow does not submit to the Store or attach an unsigned MSIX to a public GitHub release.

The package identity must remain the same for every Store update. Always use a *newer* release tag so the mapped MSIX version increases. The Store listing, certification, and distribution require a Partner Center account and are separate from merging a PR into `dev` or publishing a GitHub release.

Packaged AstraPlay installations use Windows App Installer or Microsoft Store updates. Their in-app GitHub EXE/MSI updater is disabled at runtime; unpackaged EXE/MSI installations keep their existing update path.

Before Store submission, test a signed development package on clean x64 and ARM64 Windows machines: install, launch, Remote Play, Cloud Play, PSN login, controller input, settings persistence, upgrade, and uninstall. Confirm Start Menu integration and user-managed taskbar pinning. Existing EXE/MSI installations are separate Windows packages; verify settings and save-data migration from each installer before publishing. Store acceptance and real-device behavior have not been verified by CI packaging alone.
