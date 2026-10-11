#!/bin/bash
set -xe

# ============================================================================
# Local GitHub Actions Test Script
# ============================================================================
# Purpose: Test the GitHub Actions build workflow locally on Steam Deck
#          without pushing to GitHub or waiting for CI/CD.
#
# What it does:
#   - Mimics the GitHub Actions build process (build-appimage-x64.yml)
#   - Runs entirely in the same Docker container as GitHub Actions
#   - Produces identical outputs: astraplay.AppImage and astraplay.zip
#   - Saves artifacts to build-output/ for local testing
#
# Key differences from GitHub Actions:
#   - No upload (saves locally instead)
#   - No artifact uploads (files stay in build-output/)
#   - Faster feedback loop for testing changes
#
# Usage: ./scripts/build-local-test.sh
#        Wait ~15-20 minutes, check build-output/ for results
# ============================================================================

cd "$(dirname "$0")/.."

# Clean build-output (use podman if permission denied from previous run)
if [ -d build-output ]; then
  rm -rf build-output 2>/dev/null || podman run --rm -v "`pwd`/build-output:/output" docker.io/streetpea/chiaki-ng-builder:qt6.9 /bin/bash -c "rm -rf /output/*"
fi
mkdir -p build-output

# Build in container - work in /tmp, copy outputs to /output
podman run --rm \
  -v "`pwd`:/source" \
  -v "`pwd`/build-output:/output" \
  --device /dev/fuse \
  --cap-add SYS_ADMIN \
  -t docker.io/streetpea/chiaki-ng-builder:qt6.9 \
  /bin/bash -c "
    set -xe
    
    # Install zip for packaging
    sudo apt-get update -qq && sudo apt-get install -y zip
    
    # Copy source to /tmp (avoids mounting issues)
    cd /tmp
    cp -r /source chiaki-build
    cd chiaki-build
    
    # Add QmlWebEngine import
    cp scripts/qtwebengine_import.qml gui/src/qml/
    
    # Run build (appimage/ created in chiaki-build/appimage/)
    sudo -E scripts/build-appimage.sh /tmp/appdir
    
    # Copy outputs to mounted /output with correct permissions
    sudo cp appimage/astraplay.AppImage /output/
    sudo cp -r appimage/astraplay /output/
    sudo chown -R $(id -u):$(id -g) /output
    sudo chmod -R u+rwX,go+rX /output
    
    # Zip astraplay inside container (avoids host permission issues)
    cd /output
    zip -r astraplay.zip astraplay
    rm -rf astraplay
    
    echo '=== Build outputs ==='
    ls -lh /output/
  "

echo ""
echo "============================================"
echo "Build complete! Artifacts in build-output/"
ls -lh build-output/
echo "  - astraplay.AppImage"
echo "  - astraplay.zip"
echo "============================================"
