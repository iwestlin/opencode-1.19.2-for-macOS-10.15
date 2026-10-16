#!/bin/sh
# End-to-end reproduction of the macOS 10.15 (Catalina) compatibility layer for
# the official opencode v1.19.2 darwin-x64 binary.
#
# Usage:
#   ./reproduce.sh
#   BUILD=/somewhere INSTALL=/somewhere ./reproduce.sh
#
# Requires: macOS, Xcode command line tools (clang), curl, unzip, tar, python3.
set -eu

DIR=$(cd "$(dirname "$0")" && pwd)
BUILD=${BUILD:-"$DIR/build"}
INSTALL=${INSTALL:-"$HOME/opencode-cli-10.15"}

"$DIR/download.sh" "$BUILD"
"$DIR/build-shim.sh" "$BUILD"
"$DIR/build-sqlite.sh" "$BUILD"

echo "patching binary"
python3 "$DIR/patch.py" "$BUILD/opencode" "$BUILD/opencode.bin"
codesign --force --sign - "$BUILD/opencode.bin"

echo "installing to $INSTALL"
mkdir -p "$INSTALL"
cp "$DIR/launcher" "$INSTALL/opencode"
cp "$BUILD/opencode.bin" "$INSTALL/opencode.bin"
cp "$BUILD/shim.dylib" "$INSTALL/shim.dylib"
cp "$BUILD/libsqlite3.dylib" "$INSTALL/libsqlite3.dylib"
chmod +x "$INSTALL/opencode" "$INSTALL/opencode.bin"

echo
echo "Verifying..."
PATH="$INSTALL:$PATH" "$INSTALL/opencode" --version

cat <<EOF

Done. The install is self-contained in:
  $INSTALL

Add it to your PATH, then run opencode:
  export PATH="$INSTALL:\$PATH"

Note: always launch through $INSTALL/opencode (the wrapper), not opencode.bin
directly, so the bundled SQLite is found.
EOF
