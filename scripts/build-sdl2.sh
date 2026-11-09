#!/bin/bash

set -xe

cd $(dirname "${BASH_SOURCE[0]}")/..
cd "./$1"
ROOT="`pwd`"

SDL_VER=2.30.8
PRIMARY_URL=https://www.libsdl.org/release/SDL2-${SDL_VER}.tar.gz
FALLBACK_URL=https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VER}/SDL2-${SDL_VER}.tar.gz
FILE=SDL2-${SDL_VER}.tar.gz
DIR=SDL2-${SDL_VER}

if [ ! -d "$DIR" ] || [ ! -f "$DIR/CMakeLists.txt" ]; then
	rm -rf "$DIR"
	DOWNLOAD_OK=0
	for attempt in 1 2 3 4 5; do
		echo "Downloading SDL2 ${SDL_VER} (attempt $attempt/5)..."
		rm -f "$FILE"
		# Try official libsdl.org mirror first, then GitHub Releases
		if curl -fSL --retry 3 --retry-delay 3 --connect-timeout 15 --max-time 180 "$PRIMARY_URL" -o "$FILE" || \
		   curl -fSL --retry 3 --retry-delay 3 --connect-timeout 15 --max-time 180 "$FALLBACK_URL" -o "$FILE"; then
			if [ -f "$FILE" ] && [ $(wc -c < "$FILE") -gt 1000000 ] && tar -tzf "$FILE" >/dev/null 2>&1; then
				echo "SDL2 archive verified successfully."
				tar -xf "$FILE"
				DOWNLOAD_OK=1
				break
			fi
		fi
		echo "Attempt $attempt failed or archive corrupt. Retrying in $((attempt * 5))s..."
		rm -f "$FILE"
		sleep $((attempt * 5))
	done
	if [ "$DOWNLOAD_OK" -ne 1 ]; then
		echo "ERROR: Failed to download valid SDL2 ${SDL_VER} archive after 5 attempts." >&2
		exit 1
	fi
fi

cd "$DIR" || exit 1

mkdir -p build && cd build || exit 1
cmake \
	-DCMAKE_INSTALL_PREFIX="/usr" \
	-DSDL_ATOMIC=ON \
	-DSDL_AUDIO=ON \
	-DSDL_CPUINFO=ON \
	-DSDL_EVENTS=ON \
	-DSDL_FILE=OFF \
	-DSDL_FILESYSTEM=OFF \
	-DSDL_HAPTIC=ON \
	-DSDL_JOYSTICK=ON \
	-DSDL_LOADSO=ON \
	-DSDL_RENDER=OFF \
	-DSDL_SHARED=ON \
	-DSDL_STATIC=OFF \
	-DSDL_TEST=OFF \
	-DSDL_THREADS=ON \
	-DSDL_TIMERS=ON \
	-DSDL_VIDEO=OFF \
	-DSDL_OSS=OFF \
	-DSDL_ALSA=OFF \
	-DSDL_JACK=OFF \
	-DSDL_ESD=OFF \
	-DSDL_NAS=OFF \
	-DSDL_SNDIO=OFF \
	-DSDL_ARTS=OFF \
	-DSDL_PULSEAUDIO=OFF \
	-DSDL_PIPEWIRE=ON \
	..
# SDL_THREADS is not needed, but it doesn't compile without

make -j4
make install
