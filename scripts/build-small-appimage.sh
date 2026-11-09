#!/bin/bash

set -xe

STEAMWORKS="${CHIAKI_ENABLE_STEAMWORKS:-OFF}"

if [ "$(uname -m)" = "aarch64" ]
then
    export GCC_STRING="gcc_arm64"
else
    export GCC_STRING="gcc_64"
fi

export PATH="${QT_PATH}/${QT_VERSION}/${GCC_STRING}/bin:$PATH"


# sometimes there are errors in linuxdeploy in docker/podman when the appdir is on a mount
appdir=${1:-`pwd`/appimage/appdir}

export PATH="`pwd`/appimage/protoc/bin:$PATH"
cd build_appimage 
qt-cmake \
	-GNinja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCHIAKI_ENABLE_TESTS=ON \
	-DCHIAKI_ENABLE_GUI=ON \
	-DCHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER=ON \
	-DCHIAKI_ENABLE_STEAMWORKS="${STEAMWORKS}" \
	-DCMAKE_INSTALL_PREFIX=/usr \
	..
cd ..

# purge leftover proto/nanopb_pb2.py which may have been created with another protobuf version
rm -fv third-party/nanopb/generator/proto/nanopb_pb2.py

ninja -C build_appimage
build_appimage/test/chiaki-unit

DESTDIR="${appdir}" ninja -C build_appimage install
cd appimage

fetch_tool() {
    local url="$1"
    local dest="$2"
    local min_size="${3:-200000}"
    if [ -x "$dest" ] && [ $(wc -c < "$dest") -gt "$min_size" ]; then
        return 0
    fi
    for attempt in 1 2 3 4 5; do
        echo "Downloading $dest (attempt $attempt/5)..."
        rm -f "$dest"
        if curl -fSL --retry 3 --retry-delay 3 --connect-timeout 15 --max-time 180 "$url" -o "$dest"; then
            if [ -f "$dest" ] && [ $(wc -c < "$dest") -gt "$min_size" ]; then
                chmod +x "$dest"
                echo "$dest downloaded and verified successfully."
                return 0
            fi
        fi
        echo "Download of $dest failed or file smaller than $min_size bytes. Retrying in $((attempt * 5))s..."
        rm -f "$dest"
        sleep $((attempt * 5))
    done
    echo "ERROR: Failed to download $dest from $url" >&2
    return 1
}

export ARCH="$(uname -m)"
fetch_tool "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${ARCH}.AppImage" "linuxdeploy-${ARCH}.AppImage" 500000
fetch_tool "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-${ARCH}.AppImage" "linuxdeploy-plugin-qt-${ARCH}.AppImage" 500000

export LD_LIBRARY_PATH="${QT_PATH}/${QT_VERSION}/${GCC_STRING}/lib:$(pwd)/../build_appimage/third-party/cpp-steam-tools:$LD_LIBRARY_PATH"
if [ "${STEAMWORKS}" = "ON" ]; then
    export LD_LIBRARY_PATH="$(pwd)/../third-party/steamworks/steamworks_sdk/redistributable_bin/linux64:${LD_LIBRARY_PATH}"
fi
export QML_SOURCES_PATHS="$(pwd)/../gui/src/qml"
if [ "$(uname -m)" = "aarch64" ]
then
    fetch_tool "https://github.com/multiarch/qemu-user-static/releases/download/v7.2.0-1/qemu-aarch64-static" "qemu-aarch64-static" 500000
    fetch_tool "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-aarch64.AppImage" "appimagetool-aarch64.AppImage" 500000
    ./qemu-aarch64-static ./linuxdeploy-${ARCH}.AppImage \
        --appdir="${appdir}" \
        -e "${appdir}/usr/bin/astraplay" \
        -d "${appdir}/usr/share/applications/io.github.nileshchakraborty.AstraPlay.desktop" \
        --exclude-library='libva*' \
        --exclude-library='libvulkan*' \
        --exclude-library='libhidapi*' \
        --exclude-library='libssl*' \
        --exclude-library='libcrypto*'
    # Exclude OpenSSL libraries: Qt 6.9 expects OpenSSL 3.x at runtime, but the build container
    # (Ubuntu 20.04) only has OpenSSL 1.1.1f. By excluding these, the AppImage uses the system's
    # OpenSSL (3.x on modern distros like Steam Deck), avoiding Qt TLS backend version mismatch.
    ./qemu-aarch64-static ./linuxdeploy-plugin-qt-${ARCH}.AppImage --appdir="${appdir}"
    ./qemu-aarch64-static ./appimagetool-aarch64.AppImage "${appdir}"
else
    ./linuxdeploy-${ARCH}.AppImage \
        --appdir="${appdir}" \
        -e "${appdir}/usr/bin/astraplay" \
        -d "${appdir}/usr/share/applications/io.github.nileshchakraborty.AstraPlay.desktop" \
        --plugin qt \
        --exclude-library='libva*' \
        --exclude-library='libvulkan*' \
        --exclude-library='libhidapi*' \
        --exclude-library='libssl*' \
        --exclude-library='libcrypto*' \
        --output appimage
    # Exclude OpenSSL libraries: Qt 6.9 expects OpenSSL 3.x at runtime, but the build container
    # (Ubuntu 20.04) only has OpenSSL 1.1.1f. By excluding these, the AppImage uses the system's
    # OpenSSL (3.x on modern distros like Steam Deck), avoiding Qt TLS backend version mismatch.
fi

# appimagetool names output from the .desktop Name= field (AstraPlay).
if [ -f "AstraPlay-${ARCH}.AppImage" ]; then
    mv "AstraPlay-${ARCH}.AppImage" astraplay.AppImage
else
    echo "ERROR: expected AstraPlay-${ARCH}.AppImage; found:" >&2
    ls -1 *.AppImage >&2 || true
    exit 1
fi
