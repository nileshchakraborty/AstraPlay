#!/bin/bash

set -e

cd $(dirname "${BASH_SOURCE[0]}")/..
cd "./$1"
ROOT="`pwd`"

if [ "$(uname -m)" = "aarch64" ]
then
    export ARCH_SUFFIX="aarch_64"
else
    export ARCH_SUFFIX="x86_64"
fi

URL="https://github.com/protocolbuffers/protobuf/releases/download/v3.9.1/protoc-3.9.1-linux-${ARCH_SUFFIX}.zip"

DOWNLOADED=false
for attempt in 1 2 3 4 5; do
    echo "Fetching protoc archive (attempt $attempt/5)..."
    rm -f protoc.zip
    if curl -fSL --retry 5 --retry-delay 5 --connect-timeout 20 --max-time 180 "$URL" -o protoc.zip && unzip -t protoc.zip >/dev/null 2>&1; then
        echo "protoc archive verified successfully!"
        DOWNLOADED=true
        break
    fi
    echo "Attempt $attempt failed or archive was corrupt. Retrying in $((attempt * 5))s..."
    rm -f protoc.zip
    sleep $((attempt * 5))
done

if [ "$DOWNLOADED" != true ]; then
    echo "ERROR: Failed to download valid protoc archive from $URL" >&2
    exit 1
fi

unzip -o protoc.zip -d protoc
rm -f protoc.zip
