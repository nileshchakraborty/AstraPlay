#!/bin/bash

set -xe

# sometimes there are errors in linuxdeploy in docker/podman when the appdir is on a mount
appdir=${1:-`pwd`/appimage/appdir}

mkdir appimage

pip3 install --user protobuf==3.19.5
scripts/fetch-protoc.sh appimage
export PATH="`pwd`/appimage/protoc/bin:$PATH"
scripts/build-ffmpeg.sh appimage
scripts/build-sdl2.sh appimage

mkdir build_appimage
cd build_appimage 
cmake \
	-GNinja \
	-DCMAKE_BUILD_TYPE=Release \
	"-DCMAKE_PREFIX_PATH=`pwd`/../appimage/ffmpeg-prefix;`pwd`/../appimage/sdl2-prefix;/opt/qt512" \
	-DCHIAKI_ENABLE_TESTS=ON \
	-DCHIAKI_ENABLE_GUI=ON \
	-DCHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER=ON \
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

fetch_tool "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" "linuxdeploy-x86_64.AppImage" 500000
fetch_tool "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage" "linuxdeploy-plugin-qt-x86_64.AppImage" 500000
set +e
source /opt/qt515/bin/qt515-env.sh
set -e

export LD_LIBRARY_PATH="`pwd`/sdl2-prefix/lib:$LD_LIBRARY_PATH"
export EXTRA_QT_PLUGINS=opengl

./linuxdeploy-x86_64.AppImage --appdir="${appdir}" -e "${appdir}/usr/bin/astraplay" -d "${appdir}/usr/share/applications/io.github.nileshchakraborty.AstraPlay.desktop" --plugin qt --output appimage
# appimagetool names output from the .desktop Name= field (AstraPlay).
if [ -f "AstraPlay-x86_64.AppImage" ]; then
    mv "AstraPlay-x86_64.AppImage" astraplay-compatibility.AppImage
elif [ -f "astraplay-x86_64.AppImage" ]; then
    mv "astraplay-x86_64.AppImage" astraplay-compatibility.AppImage
else
    echo "ERROR: expected AstraPlay-x86_64.AppImage; found:" >&2
    ls -1 *.AppImage >&2 || true
    exit 1
fi
