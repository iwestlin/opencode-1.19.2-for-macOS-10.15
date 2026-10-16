#!/bin/sh
# Build shim.dylib, the compatibility shim that supplies the macOS 11+ libSystem
# and newer-ICU symbols the official binary needs. It re-exports the system
# libicucore and libSystem so the loader can route through it transparently.
#
# Usage: ./build-shim.sh [build-dir] [output]   (defaults: ./build, <build>/shim.dylib)
#
# Requires the ICU 64.2 source tree from ./download.sh.
set -eu

DIR=$(cd "$(dirname "$0")" && pwd)
BUILD=${1:-"$DIR/build"}
OUT=${2:-"$BUILD/shim.dylib"}
ICU="$BUILD/icu"

if [ ! -d "$ICU/source/common" ]; then
    echo "ICU source not found at $ICU; run ./download.sh first" >&2
    exit 1
fi

clang++ -dynamiclib -o "$OUT" "$DIR/shim.cpp" \
    -I "$ICU/source/common" -I "$ICU/source/i18n" \
    -DU_DISABLE_RENAMING=1 -std=c++17 -fvisibility=default \
    -mmacosx-version-min=10.15 \
    -Wl,-reexport_library,/usr/lib/libicucore.A.dylib \
    -Wl,-reexport_library,/usr/lib/libSystem.B.dylib \
    -Wl,-install_name,@rpath/shim.dylib

codesign --force --sign - "$OUT"
echo "built $OUT"
