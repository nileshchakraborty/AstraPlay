#!/usr/bin/env bash
# =============================================================================
# AstraPlay Automated Version & Tag Bumper
# Checks existing git tags and GitHub releases. If the current version tag
# already exists, it automatically bumps the patch version, updates
# CMakeLists.txt, creates the Android fastlane changelog, and pushes the bump.
#
# Usage:
#   scripts/auto-bump-tag.sh [--ci] [--push] [--dry-run] [custom-version]
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$REPO_ROOT"

CMAKELIST="$REPO_ROOT/CMakeLists.txt"
if [ ! -f "$CMAKELIST" ]; then
    echo "ERROR: CMakeLists.txt not found at $CMAKELIST" >&2
    exit 1
fi

CURRENT_MAJOR=$(grep '^set(CHIAKI_VERSION_MAJOR' "$CMAKELIST" | awk '{print $2}' | tr -d ')')
CURRENT_MINOR=$(grep '^set(CHIAKI_VERSION_MINOR' "$CMAKELIST" | awk '{print $2}' | tr -d ')')
CURRENT_PATCH=$(grep '^set(CHIAKI_VERSION_PATCH' "$CMAKELIST" | awk '{print $2}' | tr -d ')')
CURRENT_VER="${CURRENT_MAJOR}.${CURRENT_MINOR}.${CURRENT_PATCH}"
CURRENT_TAG="v${CURRENT_VER}"

IS_CI=false
DO_PUSH=false
DRY_RUN=false
CUSTOM_TAG="${CUSTOM_TAG:-}"

for arg in "$@"; do
    case "$arg" in
        --ci)
            IS_CI=true
            DO_PUSH=true
            ;;
        --push)
            DO_PUSH=true
            ;;
        --dry-run)
            DRY_RUN=true
            ;;
        v[0-9]*.[0-9]*.[0-9]*|[0-9]*.[0-9]*.[0-9]*)
            CUSTOM_TAG="$arg"
            ;;
        *)
            echo "Unknown argument: $arg"
            echo "Usage: $0 [--ci] [--push] [--dry-run] [vX.Y.Z]"
            exit 1
            ;;
    esac
done

# 1. Fetch tags from origin so local view is up to date
git fetch --tags origin 2>/dev/null || true

# 2. Check if a tag exists (in local repo, remote repo, or GitHub release)
tag_exists() {
    local t="$1"
    if git rev-parse -q --verify "refs/tags/${t}" >/dev/null 2>&1; then
        return 0
    fi
    if git ls-remote --tags origin "refs/tags/${t}" 2>/dev/null | grep -q "${t}"; then
        return 0
    fi
    if command -v gh >/dev/null 2>&1 && gh release view "$t" >/dev/null 2>&1; then
        return 0
    fi
    return 1
}

TARGET_MAJOR="$CURRENT_MAJOR"
TARGET_MINOR="$CURRENT_MINOR"
TARGET_PATCH="$CURRENT_PATCH"
BUMPED=false

if [ -n "$CUSTOM_TAG" ]; then
    CLEAN_TAG="${CUSTOM_TAG#v}"
    TARGET_MAJOR=$(echo "$CLEAN_TAG" | cut -d. -f1)
    TARGET_MINOR=$(echo "$CLEAN_TAG" | cut -d. -f2)
    TARGET_PATCH=$(echo "$CLEAN_TAG" | cut -d. -f3)
    TARGET_VER="${TARGET_MAJOR}.${TARGET_MINOR}.${TARGET_PATCH}"
    TARGET_TAG="v${TARGET_VER}"
    if [ "$TARGET_VER" != "$CURRENT_VER" ]; then
        BUMPED=true
    fi
    echo "Using custom requested release version: $TARGET_TAG"
else
    if tag_exists "$CURRENT_TAG"; then
        echo "Tag '$CURRENT_TAG' already exists in repository or GitHub releases."
        echo "Auto-bumping patch version..."

        # Find highest patch number in existing tags for this MAJOR.MINOR
        LATEST_TAG=$(git tag -l "v${CURRENT_MAJOR}.${CURRENT_MINOR}.*" 2>/dev/null | sort -V | tail -n 1 || echo "")
        
        MAX_PATCH="$CURRENT_PATCH"
        if [ -n "$LATEST_TAG" ]; then
            TAG_PATCH=$(echo "$LATEST_TAG" | sed -E "s/^v${CURRENT_MAJOR}\.${CURRENT_MINOR}\.//" | grep -Eo '^[0-9]+' || echo "$CURRENT_PATCH")
            if [ "$TAG_PATCH" -gt "$MAX_PATCH" ]; then
                MAX_PATCH="$TAG_PATCH"
            fi
        fi

        TARGET_PATCH=$((MAX_PATCH + 1))
        TARGET_VER="${CURRENT_MAJOR}.${CURRENT_MINOR}.${TARGET_PATCH}"
        TARGET_TAG="v${TARGET_VER}"
        BUMPED=true
        echo "Auto-bumped to next available version: $CURRENT_TAG -> $TARGET_TAG"
    else
        TARGET_VER="$CURRENT_VER"
        TARGET_TAG="$CURRENT_TAG"
        echo "Tag '$CURRENT_TAG' is new and unreleased. No version bump needed."
    fi
fi

ANDROID_VC=$((TARGET_MAJOR * 10000 + TARGET_MINOR * 100 + TARGET_PATCH))

echo "=========================================="
echo " AstraPlay Version Resolution"
echo "=========================================="
echo " Target Version:      $TARGET_VER"
echo " Target Tag:          $TARGET_TAG"
echo " Android versionCode: $ANDROID_VC"
echo " Bumped:              $BUMPED"
echo "=========================================="

if [ "$DRY_RUN" = true ]; then
    echo "Dry run completed. No files modified."
    exit 0
fi

# 3. Update CMakeLists.txt if version changed
if [ "$BUMPED" = true ]; then
    echo "Updating $CMAKELIST..."
    sed -i.bak -E \
        -e "s/^set\(CHIAKI_VERSION_MAJOR [0-9]+\)/set(CHIAKI_VERSION_MAJOR ${TARGET_MAJOR})/" \
        -e "s/^set\(CHIAKI_VERSION_MINOR [0-9]+\)/set(CHIAKI_VERSION_MINOR ${TARGET_MINOR})/" \
        -e "s/^set\(CHIAKI_VERSION_PATCH [0-9]+\)/set(CHIAKI_VERSION_PATCH ${TARGET_PATCH})/" \
        "$CMAKELIST"
    rm -f "${CMAKELIST}.bak"
fi

# 4. Prepare Android changelog for this versionCode
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
    cut -c 1-497 "$DEFAULT_CL" > "${DEFAULT_CL}.tmp"
    echo "..." >> "${DEFAULT_CL}.tmp"
    mv -f "${DEFAULT_CL}.tmp" "$DEFAULT_CL"
fi

if [ ! -f "$TARGET_CL" ]; then
    cp "$DEFAULT_CL" "$TARGET_CL"
    echo "Created Android changelog: $TARGET_CL"
fi

# 5. Commit and push if requested
if [ "$BUMPED" = true ] && [ "$DO_PUSH" = true ]; then
    git config user.name "${GIT_AUTHOR_NAME:-github-actions[bot]}"
    git config user.email "${GIT_AUTHOR_EMAIL:-github-actions[bot]@users.noreply.github.com}"
    
    CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "master")
    echo "Staging changes on branch '$CURRENT_BRANCH'..."
    git add "$CMAKELIST" "$TARGET_CL" "$DEFAULT_CL"
    
    if git diff --staged --quiet; then
        echo "No changes to commit."
    else
        COMMIT_MSG="chore(release): auto-bump version to ${TARGET_TAG} (android versionCode ${ANDROID_VC}) [skip ci]"
        git commit -m "$COMMIT_MSG"
        echo "Committed version bump."

        if [ "$IS_CI" = true ]; then
            echo "Pushing auto-bump commit to origin master..."
            git push origin master
            echo "Syncing origin dev branch..."
            git push origin master:dev 2>/dev/null || true
        else
            echo "Pushing '$CURRENT_BRANCH' to origin..."
            git push origin "$CURRENT_BRANCH"
        fi
    fi
fi

COMMIT_SHA=$(git rev-parse HEAD)

# 6. Output to GitHub Actions environment if running in workflow
if [ -n "${GITHUB_OUTPUT:-}" ]; then
    echo "version=$TARGET_VER" >> "$GITHUB_OUTPUT"
    echo "tag=$TARGET_TAG" >> "$GITHUB_OUTPUT"
    echo "version_code=$ANDROID_VC" >> "$GITHUB_OUTPUT"
    echo "bumped=$BUMPED" >> "$GITHUB_OUTPUT"
    echo "commit_sha=$COMMIT_SHA" >> "$GITHUB_OUTPUT"
fi

echo "Auto-bump completed successfully: $TARGET_TAG ($COMMIT_SHA)"
