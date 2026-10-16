#!/bin/sh
# Fetch the inputs needed to build the macOS 10.15 compatibility layer:
#   - the official opencode darwin-x64 binary (v1.19.2 by default)
#   - the ICU 64.2 source tarball (headers must match 10.15's libicucore)
#   - the SQLite amalgamation (3.46.1)
#
# Usage: ./download.sh [dest-dir]        (default: ./build)
set -eu

DIR=$(cd "$(dirname "$0")" && pwd)
DEST=${1:-"$DIR/build"}
VERSION=${OPENCODE_VERSION:-1.19.2}

mkdir -p "$DEST"
cd "$DEST"

fetch() { # url filename
    if [ -f "$2" ]; then
        echo "have $2"
        return
    fi
    echo "downloading $2"
    curl -fL --retry 3 -o "$2" "$1"
}

fetch "https://github.com/anomalyco/opencode/releases/download/v${VERSION}/opencode-darwin-x64.zip" opencode-darwin-x64.zip
fetch "https://github.com/unicode-org/icu/releases/download/release-64-2/icu4c-64_2-src.tgz" icu4c-64_2-src.tgz
fetch "https://sqlite.org/2024/sqlite-amalgamation-3460100.zip" sqlite-amalgamation-3460100.zip

[ -f opencode ] || unzip -o opencode-darwin-x64.zip
[ -d icu ] || tar xzf icu4c-64_2-src.tgz
[ -d sqlite-amalgamation-3460100 ] || unzip -o sqlite-amalgamation-3460100.zip

echo "inputs ready in $DEST"
