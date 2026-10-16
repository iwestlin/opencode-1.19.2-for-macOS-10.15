"""Produce a macOS 10.15-compatible opencode.bin from the official v1.19.2
darwin-x64 binary.

Usage: python3 patch.py <official-binary> opencode.bin

The patcher performs four in-place edits:

1. Rewrites the main executable's LC_BUILD_VERSION minos to 10.15.0.
2. Repurposes the executable's libicucore.A.dylib load command to
   @executable_path/shim.dylib (same length) and rebinds every shim-provided
   import to that ordinal.
3. Locates the embedded, uncompressed libopentui.dylib blob, lowers its minos
   to 10.15.0, and rewrites its libSystem.B.dylib dependency to
   @executable_path/shim.dylib so the shim can supply __ulock_wait2 and
   re-export libSystem.
"""

import struct
import sys

NEW_NAME = b"@executable_path/shim.dylib"
OLD_NAME = b"/usr/lib/libicucore.A.dylib"
MINOS_1015 = (10 << 16) | (15 << 8) | 0
SHIM_ORDINAL = 1  # the libicucore LC_LOAD_DYLIB slot, repurposed for the shim

TARGETS = {
    # macOS 11+ libSystem APIs
    "___ulock_wait2",
    "_preadv$NOCANCEL",
    "_pwritev$NOCANCEL",
    "_pthread_self_is_exiting_np",
    # ICU APIs newer than the 10.15 system libicucore (ICU 64.2)
    "_ubrk_clone",
    "_ucal_getHostTimeZone",
    "_ucal_getTimeZoneOffsetFromLocal",
    "_udtitvfmt_formatCalendarToResult",
    "_ulistfmt_openForType",
    "_uplrules_selectForRange",
    "_unumrf_openForSkeletonWithCollapseAndIdentityFallback",
    "_unumrf_openResult",
    "_unumrf_formatDoubleRange",
    "_unumrf_formatDecimalRange",
    "_unumrf_resultAsValue",
    "_unumrf_close",
    "_unumrf_closeResult",
}

# Offsets within the embedded libopentui.dylib blob.
OPENTUI_INSTALL_NAME_OFF = 1840
OPENTUI_MINOS_OFF = 1892
OPENTUI_LIBSYSTEM_OFF = 2264
OPENTUI_LIBSYSTEM_LEN = 32


def patch_main(data):
    ncmds = struct.unpack_from("<I", data, 16)[0]
    off = 32
    chained_fixups = None
    renamed = False
    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack_from("<II", data, off)
        if cmd == 0x80000034:  # LC_DYLD_CHAINED_FIXUPS
            chained_fixups = struct.unpack_from("<II", data, off + 8)[0]
        elif cmd == 0x32:  # LC_BUILD_VERSION
            struct.pack_into("<I", data, off + 12, MINOS_1015)
        elif cmd == 0xC:  # LC_LOAD_DYLIB
            name_off = struct.unpack_from("<I", data, off + 8)[0]
            end = data.index(b"\0", off + name_off)
            if bytes(data[off + name_off : end]) == OLD_NAME:
                assert len(NEW_NAME) == len(OLD_NAME)
                data[off + name_off : off + name_off + len(NEW_NAME)] = NEW_NAME
                data[end] = 0
                renamed = True
        off += cmdsize
    assert renamed, "libicucore load command not found"
    assert chained_fixups is not None, "no chained fixups"

    _ver, _starts, imports, symbols, count, ifmt, _sfmt = struct.unpack_from("<7I", data, chained_fixups)
    entsize = {1: 4, 2: 8, 3: 16}[ifmt]
    patched = 0
    for i in range(count):
        eo = chained_fixups + imports + i * entsize
        word = struct.unpack_from("<I", data, eo)[0]
        ns = chained_fixups + symbols + (word >> 9)
        name = bytes(data[ns : data.index(b"\0", ns)]).decode()
        if name in TARGETS:
            struct.pack_into("<I", data, eo, (word & ~0xFF) | SHIM_ORDINAL)
            patched += 1
    assert patched == len(TARGETS), f"expected {len(TARGETS)} imports, patched {patched}"
    return patched


def patch_opentui(data):
    anchor = data.find(b"@rpath/libopentui.dylib")
    assert anchor >= 0, "embedded libopentui not found"
    blob = anchor - OPENTUI_INSTALL_NAME_OFF
    assert data[blob : blob + 4] == b"\xcf\xfa\xed\xfe", "no Mach-O at blob start"
    struct.pack_into("<I", data, blob + OPENTUI_MINOS_OFF, MINOS_1015)
    path_off = blob + OPENTUI_LIBSYSTEM_OFF
    cur = bytes(data[path_off : path_off + OPENTUI_LIBSYSTEM_LEN]).split(b"\0")[0]
    assert cur == b"/usr/lib/libSystem.B.dylib", f"unexpected libSystem path {cur!r}"
    assert len(NEW_NAME) + 1 <= OPENTUI_LIBSYSTEM_LEN, "new path too long"
    data[path_off : path_off + OPENTUI_LIBSYSTEM_LEN] = NEW_NAME + b"\0" * (OPENTUI_LIBSYSTEM_LEN - len(NEW_NAME))


def main():
    src, dst = sys.argv[1], sys.argv[2]
    data = bytearray(open(src, "rb").read())
    patched = patch_main(data)
    patch_opentui(data)
    open(dst, "wb").write(data)
    print(f"wrote {dst}: minos 10.15, shim renamed, {patched} imports rebound, embedded libopentui patched")


if __name__ == "__main__":
    main()
