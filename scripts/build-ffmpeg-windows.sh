#!/bin/bash

export SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
cd $(dirname "${BASH_SOURCE[0]}")/..
cd "./$1"
shift
ROOT="`pwd`"

TAG=n7.1

clone_ffmpeg() {
  local target_tag="$1"
  for attempt in 1 2 3; do
    echo "Cloning FFmpeg (attempt $attempt/3)..."
    if git clone https://github.com/FFmpeg/FFmpeg.git --depth 1 -b "$target_tag" ffmpeg; then
      return 0
    fi
    echo "GitHub mirror failed, attempting upstream git.ffmpeg.org..."
    if git clone https://git.ffmpeg.org/ffmpeg.git --depth 1 -b "$target_tag" ffmpeg; then
      return 0
    fi
    echo "Clone failed on attempt $attempt, waiting 10s..."
    rm -rf ffmpeg
    sleep 10
  done
  return 1
}

clone_ffmpeg "$TAG" && cd ffmpeg || exit 1
git apply ${SCRIPT_DIR}/flatpak/0001-vulkan-ignore-frames-without-hw-context.patch || exit 1
./configure --disable-all --enable-shared --enable-ffmpeg --enable-avformat --enable-avcodec --enable-decoder=h264 --enable-decoder=hevc --enable-hwaccel=h264_vulkan --enable-hwaccel=hevc_vulkan --enable-hwaccel=h264_d3d11va --enable-hwaccel=hevc_d3d11va --prefix=/mingw64 "$@" || exit 1
make -j4 || exit 1
make install || exit 1