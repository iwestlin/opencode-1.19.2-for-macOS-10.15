import struct
import sys

src, dst = sys.argv[1], sys.argv[2]
NEW_NAME = b"@executable_path/shim.dylib"
OLD_NAME = b"/usr/lib/libicucore.A.dylib"
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
SHIM_ORDINAL = 1  # libicucore LC_LOAD_DYLIB slot, repurposed for shim

data = bytearray(open(src, "rb").read())

ncmds = struct.unpack_from("<I", data, 16)[0]
off = 32
cf = None
for _ in range(ncmds):
    cmd, cmdsize = struct.unpack_from("<II", data, off)
    if cmd == 0x80000034:  # LC_DYLD_CHAINED_FIXUPS
        cf = struct.unpack_from("<II", data, off + 8)[0]
    if cmd == 0xC:  # LC_LOAD_DYLIB
        name_off = struct.unpack_from("<I", data, off + 8)[0]
        end = data.index(b"\0", off + name_off)
        name = bytes(data[off + name_off : end])
        if name == OLD_NAME:
            assert len(NEW_NAME) == len(OLD_NAME), "name length mismatch"
            data[off + name_off : off + name_off + len(NEW_NAME)] = NEW_NAME
            data[end] = 0
            print(f"repurposed LC_LOAD_DYLIB: {OLD_NAME.decode()} -> {NEW_NAME.decode()}")
    off += cmdsize

ver, starts, imports, symbols, count, ifmt, sfmt = struct.unpack_from("<7I", data, cf)
entsize = {1: 4, 2: 8, 3: 16}[ifmt]
patched = 0
for i in range(count):
    eo = cf + imports + i * entsize
    word = struct.unpack_from("<I", data, eo)[0]
    ns = cf + symbols + (word >> 9)
    name = data[ns : data.index(b"\0", ns)].decode()
    if name in TARGETS:
        newword = (word & ~0xFF) | SHIM_ORDINAL
        struct.pack_into("<I", data, eo, newword)
        patched += 1
        print(f"patched [{i}] {name}: ordinal {word & 0xFF} -> {SHIM_ORDINAL}")

open(dst, "wb").write(data)
print(f"done: {patched} imports patched")
