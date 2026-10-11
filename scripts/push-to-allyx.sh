#!/usr/bin/env bash
# Push the latest Pylux Windows build to ROG Xbox Ally X
set -euo pipefail

ALLYX_HOST="${ALLYX_HOST:-allyx.local}"
ALLYX_USER="${ALLYX_USER:-niles}"
REPO="nileshchakraborty/Pylux"

echo "=== AstraPlay Ally X Deployment ==="

# Check connection
echo "Checking connection to ${ALLYX_USER}@${ALLYX_HOST}..."
if ! ssh -o BatchMode=yes -o ConnectTimeout=5 "${ALLYX_USER}@${ALLYX_HOST}" "echo connected" >/dev/null 2>&1; then
    echo "WARNING: Could not connect to ${ALLYX_HOST} via SSH."
    echo "Make sure the Ally X is powered on, booted into Windows, and connected to the local network."
    exit 1
fi

echo "Connected successfully to ${ALLYX_HOST}!"

TEMP_DIR=$(mktemp -d)
trap 'rm -rf "$TEMP_DIR"' EXIT

echo "Fetching latest release assets from GitHub (${REPO})..."
gh release download --repo "$REPO" --dir "$TEMP_DIR" --pattern "*.msi" || true
gh release download --repo "$REPO" --dir "$TEMP_DIR" --pattern "*.exe" || true

MSI_FILE=$(find "$TEMP_DIR" -type f -name "*.msi" | head -n 1 || true)
EXE_FILE=$(find "$TEMP_DIR" -type f \( -name "astraplay*.exe" -o -name "pylux*.exe" \) | head -n 1 || true)

if [ -n "$MSI_FILE" ]; then
    FILENAME=$(basename "$MSI_FILE")
    echo "Deploying MSI installer: $FILENAME"
    scp "$MSI_FILE" "${ALLYX_USER}@${ALLYX_HOST}:Downloads/${FILENAME}"
    echo "Executing silent MSI install on Ally X..."
    ssh "${ALLYX_USER}@${ALLYX_HOST}" "msiexec /i %USERPROFILE%\\Downloads\\${FILENAME} /qn /norestart ALLUSERS=1"
    echo "MSI installation triggered successfully."
elif [ -n "$EXE_FILE" ]; then
    FILENAME=$(basename "$EXE_FILE")
    echo "Deploying executable: $FILENAME"
    scp "$EXE_FILE" "${ALLYX_USER}@${ALLYX_HOST}:Downloads/${FILENAME}"
    echo "Copied to Downloads on Ally X."
else
    echo "No release binary found in latest GitHub release."
    exit 1
fi

echo "=== Deployment to Ally X complete! ==="
