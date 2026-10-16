#!/bin/sh
# Build a libsqlite3.dylib (3.46.1) that runs on macOS 10.15. Bun dlopen()s the
# leaf name "libsqlite3.dylib" at runtime; the 10.15 system copy (3.28) is too
# old for the RETURNING syntax opencode uses.
#
# The feature flags below match the options the original build embedded (see
# `PRAGMA compile_options`): FTS3/FTS5, JSON, RTREE, math functions, etc.
#
# Usage: ./build-sqlite.sh [build-dir] [output]   (defaults: ./build, <build>/libsqlite3.dylib)
set -eu

DIR=$(cd "$(dirname "$0")" && pwd)
BUILD=${1:-"$DIR/build"}
OUT=${2:-"$BUILD/libsqlite3.dylib"}
SRC="$BUILD/sqlite-amalgamation-3460100/sqlite3.c"

if [ ! -f "$SRC" ]; then
    echo "SQLite amalgamation not found at $SRC; run ./download.sh first" >&2
    exit 1
fi

clang -dynamiclib -o "$OUT" "$SRC" \
    -target x86_64-apple-macos10.15 \
    -DSQLITE_ENABLE_COLUMN_METADATA \
    -DSQLITE_ENABLE_DBSTAT_VTAB \
    -DSQLITE_ENABLE_FTS3 \
    -DSQLITE_ENABLE_FTS3_PARENTHESIS \
    -DSQLITE_ENABLE_FTS5 \
    -DSQLITE_ENABLE_LOAD_EXTENSION \
    -DSQLITE_ENABLE_MATH_FUNCTIONS \
    -DSQLITE_ENABLE_RTREE \
    -DSQLITE_ENABLE_STMTVTAB \
    -DSQLITE_THREADSAFE=1

codesign --force --sign - "$OUT"
echo "built $OUT"
