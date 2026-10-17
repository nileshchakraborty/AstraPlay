# Contributing to AstraPlay

Thanks for your interest in contributing! AstraPlay is a free, community-built project and all help is welcome.

## Branching and release workflow

| Branch | Purpose |
|---|---|
| `dev` | Integration branch and target for all feature, fix, and documentation PRs. |
| `master` | Default, stable release branch. Only release PRs from `dev` target it. Merges trigger the release build and GitHub Release workflow. |
| Feature branches | Start from `dev` and open PRs back to `dev`. |

### How to contribute

1. **Fork** the repo on GitHub.
2. Create a feature or fix branch off `dev`.
3. Make your changes, commit, and push to your fork.
4. Open a **pull request** targeting `dev`.
5. A maintainer reviews and merges it into `dev`. This does not publish a release.
6. For larger changes, opening an issue first is appreciated so we can discuss the approach.
7. Keep PRs focused — one feature or fix per PR when possible.

### What happens after merge

Feature PRs merge into `dev` for integration. When the maintainer is ready to release, they open a separate **release PR from `dev` to `master`**. Merging that PR triggers `.github/workflows/release-on-merge.yml`, which builds platform binaries and publishes a GitHub Release. Store deployment uses the dedicated deploy workflows or the manually dispatched `release-all.yml` workflow; merging a feature PR into `dev` does not deploy to stores.

Do not push directly to `dev` or `master`.

## Local development & Testing

Each platform has its own build setup. See the relevant directories for details:

- **Desktop (Qt 6)** — root `CMakeLists.txt` (CMake + Qt 6)
- **Linux** — built in a container (see `.github/workflows/build-linux.yml`)
- **Windows** — built via MSYS2 / MSBuild (see `.github/workflows/build-windows.yml`)
- **macOS** — CMake + Qt 6 macOS bundle
- **Android** — `android/` (Gradle + CMake, requires Android SDK/NDK)
- **iOS** — `ios/` (Xcode project + CMake for native libs)
- **Flatpak** — manifest at `scripts/flatpak/io.github.nileshchakraborty.astraplay.yml`

### Running the Test Suite Locally

AstraPlay includes comprehensive unit, BDD, and updater test suites. Before submitting a PR, make sure all tests pass:

```bash
# Configure with tests enabled
cmake -B build -DCHIAKI_ENABLE_TESTS=ON
cmake --build build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)

# Run test suite
ctest --test-dir build --output-on-failure
```
All pull requests must maintain 100% passing test status.
