#!/usr/bin/env bash
# =============================================================================
# AstraPlay Automated Release Helper
# Bumps version in CMakeLists.txt, validates changelogs (<500 chars),
# creates the Android versionCode changelog, and stages the release.
#
# Usage:
#   scripts/prepare-release.sh [patch|minor|major|X.Y.Z] [--push]
#
# Examples:
#   scripts/prepare-release.sh              # bumps patch: 2.12.20 -> 2.12.21
#   scripts/prepare-release.sh minor        # bumps minor: 2.12.20 -> 2.13.0
#   scripts/prepare-release.sh 2.13.1 --push # sets 2.13.1 and pushes to dev/master
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$REPO_ROOT"

# Ensure clean working tree or staged changes only in release files
CMAKELIST="$REPO_ROOT/CMakeLists.txt"
if [ ! -f "$CMAKELIST" ]; then
    echo "ERROR: CMakeLists.txt not found at $CMAKELIST" >&2
    exit 1
fi

CURRENT_MAJOR=$(grep '^set(CHIAKI_VERSION_MAJOR' "$CMAKELIST" | awk '{print $2}' | tr -d ')')
CURRENT_MINOR=$(grep '^set(CHIAKI_VERSION_MINOR' "$CMAKELIST" | awk '{print $2}' | tr -d ')')
CURRENT_PATCH=$(grep '^set(CHIAKI_VERSION_PATCH' "$CMAKELIST" | awk '{print $2}' | tr -d ')')
CURRENT_VER="${CURRENT_MAJOR}.${CURRENT_MINOR}.${CURRENT_PATCH}"

TARGET_BUMP="patch"
DO_PUSH=false

for arg in "$@"; do
    case "$arg" in
        patch|minor|major)
            TARGET_BUMP="$arg"
            ;;
        --push)
            DO_PUSH=true
            ;;
        [0-9]*.[0-9]*.[0-9]*)
            TARGET_BUMP="$arg"
            ;;
        *)
            echo "Unknown argument: $arg"
            echo "Usage: $0 [patch|minor|major|X.Y.Z] [--push]"
            exit 1
            ;;
    esac
done

if [[ "$TARGET_BUMP" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    NEW_MAJOR=$(echo "$TARGET_BUMP" | cut -d. -f1)
    NEW_MINOR=$(echo "$TARGET_BUMP" | cut -d. -f2)
    NEW_PATCH=$(echo "$TARGET_BUMP" | cut -d. -f3)
elif [ "$TARGET_BUMP" = "major" ]; then
    NEW_MAJOR=$((CURRENT_MAJOR + 1))
    NEW_MINOR=0
    NEW_PATCH=0
elif [ "$TARGET_BUMP" = "minor" ]; then
    NEW_MAJOR=$CURRENT_MAJOR
    NEW_MINOR=$((CURRENT_MINOR + 1))
    NEW_PATCH=0
else
    # Default: patch
    NEW_MAJOR=$CURRENT_MAJOR
    NEW_MINOR=$CURRENT_MINOR
    NEW_PATCH=$((CURRENT_PATCH + 1))
fi

NEW_VER="${NEW_MAJOR}.${NEW_MINOR}.${NEW_PATCH}"
ANDROID_VC=$((NEW_MAJOR * 10000 + NEW_MINOR * 100 + NEW_PATCH))

echo "=========================================="
echo " AstraPlay Release Preparation"
echo "=========================================="
echo " Current version:     v${CURRENT_VER}"
echo " New version:         v${NEW_VER}"
echo " Android versionCode: ${ANDROID_VC}"
echo "=========================================="

# 1. Update CMakeLists.txt
sed -i.bak -E \
    -e "s/^set\(CHIAKI_VERSION_MAJOR [0-9]+\)/set(CHIAKI_VERSION_MAJOR ${NEW_MAJOR})/" \
    -e "s/^set\(CHIAKI_VERSION_MINOR [0-9]+\)/set(CHIAKI_VERSION_MINOR ${NEW_MINOR})/" \
    -e "s/^set\(CHIAKI_VERSION_PATCH [0-9]+\)/set(CHIAKI_VERSION_PATCH ${NEW_PATCH})/" \
    "$CMAKELIST"
rm -f "${CMAKELIST}.bak"

# 2. Prepare Android changelog
CHANGELOG_DIR="$REPO_ROOT/android/fastlane/metadata/android/en-US/changelogs"
mkdir -p "$CHANGELOG_DIR"
DEFAULT_CL="$CHANGELOG_DIR/default.txt"
TARGET_CL="$CHANGELOG_DIR/${ANDROID_VC}.txt"

if [ ! -f "$DEFAULT_CL" ]; then
    echo "• Maintenance update and performance improvements." > "$DEFAULT_CL"
fi

# Ensure default.txt <= 500 characters
CL_LEN=$(wc -m < "$DEFAULT_CL" | tr -d ' ')
if [ "$CL_LEN" -gt 500 ]; then
    echo "WARNING: default.txt length is $CL_LEN (>500), truncating..."
    cut -c 1-497 "$DEFAULT_CL" > "${DEFAULT_CL}.tmp"
    echo "..." >> "${DEFAULT_CL}.tmp"
    mv -f "${DEFAULT_CL}.tmp" "$DEFAULT_CL"
fi

cp "$DEFAULT_CL" "$TARGET_CL"
echo "Prepared Android changelog: $TARGET_CL ($(wc -m < "$TARGET_CL" | tr -d ' ') chars)"

# 3. Git commit & push if requested
if [ "$DO_PUSH" = true ]; then
    CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
    echo "Committing version bump on branch '$CURRENT_BRANCH'..."
    git add "$CMAKELIST" "$TARGET_CL" "$DEFAULT_CL"
    git commit -m "chore(release): bump version to v${NEW_VER} (android versionCode ${ANDROID_VC})"
    
    echo "Pushing '$CURRENT_BRANCH' to origin..."
    git push origin "$CURRENT_BRANCH"
    
    if [ "$CURRENT_BRANCH" = "dev" ]; then
        echo "Fast-forwarding 'master' and pushing to origin..."
        git checkout master
        git merge dev --ff-only
        git push origin master
        git checkout dev
    fi
    echo "Successfully pushed v${NEW_VER}! Release workflow will trigger automatically."
else
    echo "Files updated locally. Review with: git diff"
    echo "To commit and push, run:"
    echo "  git commit -am 'chore(release): bump version to v${NEW_VER}' && git push origin dev"
fi
