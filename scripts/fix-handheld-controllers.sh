#!/usr/bin/env bash
# ==============================================================================
# AstraPlay Controller & Handheld Workaround Utility
# ==============================================================================
# Troubleshoots, tests, and repairs gamepad input across:
#   1. Handhelds & Waydroid: ROG Xbox Ally X, Steam Deck, Bazzite, SteamOS (LXC)
#   2. Android Devices: Physical phones/tablets with Backbone One, Razer Kishi
#   3. iOS / iPhone: Backbone One & MFi controllers (verification & guidance)
#
# Usage:
#   ./scripts/fix-handheld-controllers.sh [options]
#
# Options:
#   --check             Run non-destructive diagnostics only
#   --fix               Apply automatic uevent, udev, and shortcut remediations
#   --allyx             Target remote ROG Xbox Ally X (steamos.local or ALLYX_HOST)
#   --remote <host>     Target any remote handheld over SSH
#   --android / --adb   Inspect connected Android device via ADB
#   --ios / --iphone    Display iOS & Backbone One setup verification guide
#   --help              Display this help menu
# ==============================================================================

set -euo pipefail

# Text formatting
BOLD="\033[1m"
GREEN="\033[0;32m"
YELLOW="\033[1;33m"
RED="\033[0;31m"
BLUE="\033[0;34m"
CYAN="\033[0;36m"
NC="\033[0m"

info()    { echo -e "${BLUE}[INFO]${NC} $*"; }
success() { echo -e "${GREEN}[OK]${NC} $*"; }
warn()    { echo -e "${YELLOW}[WARN]${NC} $*"; }
error()   { echo -e "${RED}[ERROR]${NC} $*"; }
header()  { echo -e "\n${BOLD}${CYAN}=== $* ===${NC}\n"; }

ACTION="fix"
REMOTE_HOST=""
REMOTE_USER="${ALLYX_USER:-nilesh}"
TARGET_MODE="local"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --check)
            ACTION="check"
            shift
            ;;
        --fix)
            ACTION="fix"
            shift
            ;;
        --allyx)
            REMOTE_HOST="${ALLYX_HOST:-steamos.local}"
            TARGET_MODE="remote"
            shift
            ;;
        --remote)
            REMOTE_HOST="$2"
            TARGET_MODE="remote"
            shift 2
            ;;
        --user)
            REMOTE_USER="$2"
            shift 2
            ;;
        --android|--adb)
            TARGET_MODE="android"
            shift
            ;;
        --ios|--iphone)
            TARGET_MODE="ios"
            shift
            ;;
        --help|-h)
            echo "AstraPlay Controller & Handheld Workaround Utility"
            echo "Usage: $0 [--check|--fix] [--allyx|--remote <host>|--android|--ios]"
            exit 0
            ;;
        *)
            warn "Unknown argument: $1"
            shift
            ;;
    esac
done

header "AstraPlay Controller & Handheld Diagnostic Tool"

# ==============================================================================
# Mode: iOS / iPhone Backbone One Guidance
# ==============================================================================
if [[ "$TARGET_MODE" == "ios" ]]; then
    header "iPhone / iOS & Backbone One Controller Guide"
    echo -e "${GREEN}TIP:${NC} AstraPlay includes built-in ${BOLD}Controller Diagnostics & Troubleshooting${NC}!"
    echo -e "Open AstraPlay on your iPhone: ${BOLD}Settings -> Gamepad Tuning & Remapping -> Controller Diagnostics & Troubleshooting${NC}"
    echo -e "to test buttons live and inspect connected GCControllers.\n"
    echo -e "AstraPlay on iOS uses Apple's ${BOLD}GameController (GCController)${NC} framework."
    echo -e "To ensure Backbone One or mobile telescopic controllers function with zero button capture issues:\n"
    
    echo -e "${BOLD}1. iOS Game Controller Settings:${NC}"
    echo "   • Open Settings -> General -> Game Controller."
    echo "   • Verify that 'Backbone One' appears under Connected Controllers."
    echo "   • Keep Profiles set to 'Default' to prevent button conflicts."
    echo ""
    echo -e "${BOLD}2. Suppress Backbone App Capture Gestures:${NC}"
    echo "   • Open the official 'Backbone' app on your iPhone."
    echo "   • In Backbone Settings, disable 'Launch Backbone App on Button Press'."
    echo "   • AstraPlay automatically requests '.disabled' preferredSystemGestureState"
    echo "     on the Home/Options/Menu buttons so they pass directly to the PS5/PS4 console."
    echo ""
    echo -e "${BOLD}3. DualSense Touchpad Emulation:${NC}"
    echo "   • Because mobile telescopic controllers lack a physical trackpad,"
    echo "     AstraPlay supports on-screen touchpad gestures."
    echo "   • In AstraPlay iOS Settings, ensure 'Touchpad Emulation' is enabled"
    echo "     (allows top-screen swipes or floating touchpad buttons for games like Ghost of Tsushima)."
    echo ""
    echo -e "${BOLD}4. Audio & Headset:${NC}"
    echo "   • If connecting a 3.5mm headset directly to your Backbone One, iOS routes"
    echo "     audio over the Lightning/USB-C connection automatically."
    exit 0
fi

# ==============================================================================
# Mode: Android Device via ADB
# ==============================================================================
if [[ "$TARGET_MODE" == "android" ]]; then
    header "Android Controller Diagnostics (via ADB)"
    echo -e "${GREEN}TIP:${NC} AstraPlay Android includes built-in ${BOLD}Controller Diagnostics & Troubleshooting${NC}!"
    echo -e "Open AstraPlay on your device: ${BOLD}Settings -> Gamepad Tuning & Remapping -> Controller Diagnostics & Troubleshooting${NC}"
    echo -e "to inspect gamepads and detect accessibility conflicts without using ADB.\n"
    if ! command -v adb >/dev/null 2>&1; then
        error "ADB command not found on host. Please install android-tools or connect via local shell."
        exit 1
    fi

    DEVICE_STATE=$(adb get-state 2>/dev/null || echo "offline")
    if [[ "$DEVICE_STATE" != "device" ]]; then
        error "No authorized Android device found via ADB. Ensure USB debugging is active."
        exit 1
    fi

    MODEL=$(adb shell getprop ro.product.model | tr -d '\r')
    ANDROID_VER=$(adb shell getprop ro.build.version.release | tr -d '\r')
    success "Connected to Android device: ${BOLD}${MODEL}${NC} (Android ${ANDROID_VER})"

    info "Querying connected input devices..."
    INPUT_DUMP=$(adb shell dumpsys input | tr -d '\r' || true)

    if echo "$INPUT_DUMP" | grep -qi "Backbone"; then
        success "Backbone One controller detected on Android!"
    elif echo "$INPUT_DUMP" | grep -qiE "Kishi|GameSir|Ally|DualSense|Xbox"; then
        success "Supported physical gamepad detected on Android."
    else
        warn "No standard branded gamepad detected by name in dumpsys input."
    fi

    echo ""
    info "Active Input Devices reported by Android EventHub:"
    adb shell "cat /proc/bus/input/devices | grep -E 'N: Name|P: Phys|S: Sysfs|H: Handlers'" | while read -r line; do
        echo "  $line"
    done

    echo ""
    info "Checking for conflicting Accessibility Services (which may swallow gamepad keys):"
    ACCESSIBILITY=$(adb shell settings get secure enabled_accessibility_services | tr -d '\r' || true)
    if [[ -n "$ACCESSIBILITY" && "$ACCESSIBILITY" != "null" ]]; then
        warn "Active accessibility services found: ${ACCESSIBILITY}"
        echo "  If certain buttons (like Home or D-pad) don't reach AstraPlay, try temporarily"
        echo "  disabling accessibility overlays in Android Settings -> Accessibility."
    else
        success "No third-party accessibility key-interceptors detected."
    fi

    exit 0
fi

# ==============================================================================
# Mode: Handheld / Linux (Local or Remote over SSH)
# ==============================================================================
RUN_PREFIX=""
if [[ "$TARGET_MODE" == "remote" ]]; then
    header "Connecting to Remote Handheld (${REMOTE_USER}@${REMOTE_HOST})"
    if ! ssh -o BatchMode=yes -o ConnectTimeout=5 "${REMOTE_USER}@${REMOTE_HOST}" "echo connected" >/dev/null 2>&1; then
        error "Could not connect to ${REMOTE_USER}@${REMOTE_HOST} via SSH."
        echo "Ensure the device is powered on and connected to the local network."
        exit 1
    fi
    success "Connected to ${REMOTE_HOST} successfully."
    RUN_PREFIX="ssh -o BatchMode=yes ${REMOTE_USER}@${REMOTE_HOST}"
fi

run_cmd() {
    if [[ -n "$RUN_PREFIX" ]]; then
        $RUN_PREFIX "$@"
    else
        eval "$@"
    fi
}

info "Probing handheld environment and input subsystem..."

# 1. Inspect Host Input Devices
header "Host Controller Hardware Inspection"
HOST_INPUT=$(run_cmd "cat /proc/bus/input/devices | grep -E 'N: Name|H: Handlers' || true")
echo "$HOST_INPUT"

if echo "$HOST_INPUT" | grep -qi "Ally"; then
    success "ROG Xbox Ally X Hardware Controller identified on host."
elif echo "$HOST_INPUT" | grep -qi "Backbone"; then
    success "Backbone One Controller identified on host."
elif echo "$HOST_INPUT" | grep -qiE "Steam|Xbox|DualSense"; then
    success "Virtual / Physical Gamepad identified on host."
else
    warn "No physical gamepad clearly identified in /proc/bus/input/devices."
fi

# 2. Check Waydroid Container Status
header "Waydroid Container Status"
WAYDROID_ACTIVE=$(run_cmd "systemctl is-active waydroid-container 2>/dev/null || echo 'inactive'")
info "waydroid-container.service state: ${BOLD}${WAYDROID_ACTIVE}${NC}"

WAYDROID_PROP_FILE="/var/lib/waydroid/waydroid_base.prop"
PROP_EXISTS=$(run_cmd "test -f ${WAYDROID_PROP_FILE} && echo 'yes' || echo 'no'")

if [[ "$PROP_EXISTS" == "yes" ]]; then
    HAS_UDEV=$(run_cmd "grep -q 'persist.waydroid.udev=true' ${WAYDROID_PROP_FILE} && echo 'yes' || echo 'no'")
    HAS_UEVENT=$(run_cmd "grep -q 'persist.waydroid.uevent=true' ${WAYDROID_PROP_FILE} && echo 'yes' || echo 'no'")
    
    info "Waydroid Hardware Passthrough Configuration:"
    echo "  • persist.waydroid.udev   : $HAS_UDEV"
    echo "  • persist.waydroid.uevent : $HAS_UEVENT"

    if [[ "$ACTION" == "fix" ]]; then
        if [[ "$HAS_UDEV" != "yes" || "$HAS_UEVENT" != "yes" ]]; then
            info "Applying missing udev/uevent properties to ${WAYDROID_PROP_FILE}..."
            run_cmd "pkexec sh -c 'echo -e \"persist.waydroid.udev=true\npersist.waydroid.uevent=true\" >> ${WAYDROID_PROP_FILE}'"
            success "Updated ${WAYDROID_PROP_FILE}."
        fi
    fi
else
    warn "${WAYDROID_PROP_FILE} not found. (Waydroid may not be initialized yet)."
fi

# 3. Check Steam Shortcut Controller Template
header "Steam Shortcut Template Check"
USER_DESKTOP="/home/${REMOTE_USER}/.local/share/applications/Waydroid.desktop"
DESKTOP_EXISTS=$(run_cmd "test -f ${USER_DESKTOP} && echo 'yes' || echo 'no'")

if [[ "$DESKTOP_EXISTS" == "yes" ]]; then
    CURRENT_TEMPLATE=$(run_cmd "grep 'X-Steam-Controller-Template' ${USER_DESKTOP} || echo 'None'")
    info "Current Steam Controller Template in Waydroid.desktop: ${BOLD}${CURRENT_TEMPLATE}${NC}"

    if echo "$CURRENT_TEMPLATE" | grep -qi "Desktop"; then
        warn "Waydroid is configured to use the 'Desktop' template in Steam."
        warn "In Desktop mode, the Ally X joysticks/buttons act as a MOUSE/KEYBOARD instead of a gamepad!"
        
        if [[ "$ACTION" == "fix" ]]; then
            info "Updating ${USER_DESKTOP} to use 'Gamepad' template..."
            run_cmd "sed -i 's/X-Steam-Controller-Template=Desktop/X-Steam-Controller-Template=Gamepad/g' ${USER_DESKTOP}"
            success "Updated Waydroid.desktop template to Gamepad."
        else
            echo "  Tip: Change Steam Controller layout for Waydroid in Steam Gaming Mode to 'Gamepad'."
        fi
    else
        success "Steam Controller template is already set to Gamepad."
    fi
fi

# 4. Trigger Controller Event Discovery
if [[ "$ACTION" == "fix" ]]; then
    header "Executing Controller Device Discovery Refresh"
    info "Broadcasting 'add' uevent to all virtual and physical input event nodes..."
    
    # Try the Bazzite-integrated helper first, or fall back to sysfs uevent broadcast
    HAS_HELPER=$(run_cmd "test -x /usr/libexec/waydroid-fix-controllers && echo 'yes' || echo 'no'")
    if [[ "$HAS_HELPER" == "yes" ]]; then
        run_cmd "pkexec /usr/libexec/waydroid-fix-controllers"
        success "Ran /usr/libexec/waydroid-fix-controllers successfully."
    else
        run_cmd "echo add | pkexec tee /sys/devices/virtual/input/input*/event*/uevent >/dev/null 2>&1 || true"
        success "Broadcasted add event via /sys/devices/virtual/input/*/uevent."
    fi
fi

header "Diagnostic & Remediation Summary"
success "Controller checks and remediations complete."
echo -e "If running Waydroid in Steam Gaming Mode:"
echo -e "  1. Launch Waydroid from your Steam Library."
echo -e "  2. In Steam Controller Settings for Waydroid, ensure the layout is ${BOLD}Gamepad${NC}."
echo -e "  3. Launch AstraPlay Android inside Waydroid. The app will log active gamepad attachment."
echo -e "  4. Use the ${BOLD}Command Center / Armoury Crate${NC} button to toggle Gamepad Mode if needed."
