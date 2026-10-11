# AGPL Compliance - Source Code Distribution

## Overview

astraplay is licensed under the GNU Affero General Public License v3.0 (AGPL-3.0). This license requires that when distributing binary versions, you must provide access to the corresponding source code.

## How It Works

### 1. Source Code on GitHub Releases

Every GitHub release automatically attaches full source archives (`Source code (zip)` /
`Source code (tar.gz)`) for its tag, and the repository itself is public. No extra
upload step or secret is needed — publishing a release satisfies source distribution.

### 2. License Files in Binary Distribution

Build scripts use the helper function `add_agpl_compliance()` from `scripts/add-agpl-compliance.sh` to add:
- **`usr/share/licenses/astraplay/COPYING`**: Full AGPL-3.0 license text
- **`usr/share/licenses/astraplay/SOURCE_CODE.txt`**: Notice with link to download source code

Files are placed in `usr/share/licenses/astraplay/` to follow Linux standards while being less prominent to end users.

### 3. Reusable Helper Script

The template `legal/agpl-source-notice.txt` contains the notice text with placeholders that are replaced at build time:
- `__BUILD_DATE__` → Build timestamp
- `__GIT_COMMIT__` → Git commit hash
- `__GIT_BRANCH__` → Git branch name
- `__ARCHITECTURE__` → System architecture

## Setup for GitHub Releases

No secrets or extra setup are required: source archives are attached automatically
when a release is published from a tag. Keep release tags available as long as
you distribute the corresponding binaries.

## Using in Build Scripts

To add AGPL compliance to any build script:

```bash
# Source the helper script
source scripts/add-agpl-compliance.sh

# Call the function with your distribution directory
add_agpl_compliance "/path/to/distribution/root"
```

**Example** (as used in `build-appimage.sh`):
```bash
cd ..
source scripts/add-agpl-compliance.sh
add_agpl_compliance "appimage/${PORTABLE_DIR}"
cd appimage
```

This will automatically:
- Create `usr/share/licenses/astraplay/` directory
- Copy `COPYING` license file
- Generate `SOURCE_CODE.txt` with build information

## The Source Code Link

`SOURCE_CODE.txt` points at the stable releases page, so it never needs updating:

```
    https://github.com/nileshchakraborty/AstraPlay/releases/latest
```

Every release there carries its matching source archive. If you distribute a custom
build outside GitHub releases, host its source yourself and update the URL in
`legal/agpl-source-notice.txt` accordingly.

## AGPL Compliance Requirements

✅ **What we do:**
- Include full license text (COPYING)
- Provide written offer to source code (SOURCE_CODE.txt)
- Make source available via public download link
- Include all source needed to build the software

✅ **AGPL Section 6(d) compliance:**
> "Convey the object code by offering access from a designated place...
> and offer equivalent access to the Corresponding Source in the same way"

This means providing a download link in the binary distribution is sufficient.

## Important Notes

### Source Retention
- Keep source archives available as long as you distribute the binary
- AGPL doesn't specify a duration, but 3+ years is recommended

### What's Included in Source Archive
- All source code files
- Build scripts and configuration
- Third-party dependencies (submodules)
- Documentation
- Excludes: build artifacts, binaries, .git history

### Modified Versions
If you modify and distribute the software:
1. Include your modifications in the source archive
2. Add prominent notices of your changes
3. Update the source code link in SOURCE_CODE.txt
4. Maintain the same AGPL license

## Verification

After building, check that the files exist in the distribution:
```bash
ls astraplay/usr/share/licenses/astraplay/COPYING        # License file exists
ls astraplay/usr/share/licenses/astraplay/SOURCE_CODE.txt # Source info exists
cat astraplay/usr/share/licenses/astraplay/SOURCE_CODE.txt # URL is correct
```

The files are intentionally placed in a standard but less prominent location (`usr/share/licenses/astraplay/`) to comply with AGPL while not being immediately visible to end users.

## References

- AGPL-3.0 License: https://www.gnu.org/licenses/agpl-3.0.html
- GPL FAQ: https://www.gnu.org/licenses/gpl-faq.html
- Full license text: See `COPYING` in repository root

