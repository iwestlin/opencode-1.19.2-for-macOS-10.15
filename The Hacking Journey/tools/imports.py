import struct
import sys

path = sys.argv[1]
data = bytearray(open(path, "rb").read())

ncmds = struct.unpack_from("<I", data, 16)[0]
off = 32
cf = None
load_dylibs = []
for _ in range(ncmds):
    cmd, cmdsize = struct.unpack_from("<II", data, off)
    if cmd == 0x80000034:  # LC_DYLD_CHAINED_FIXUPS
        dataoff, datasize = struct.unpack_from("<II", data, off + 8)
        cf = dataoff
    if cmd == 0xC:  # LC_LOAD_DYLIB
        name_off = struct.unpack_from("<I", data, off + 8)[0]
        end = data.index(b"\0", off + name_off)
        load_dylibs.append(data[off + name_off : end].decode())
    off += cmdsize

if cf is None:
    raise SystemExit("no chained fixups")

ver, starts, imports, symbols, count, ifmt, sfmt = struct.unpack_from("<7I", data, cf)
print(f"fixups@{cf:#x} ver={ver} imports_off={imports} symbols_off={symbols} count={count} fmt={ifmt} symfmt={sfmt}")
print("load dylibs:", load_dylibs)

entsize = {1: 4, 2: 8, 3: 16}.get(ifmt)
for i in range(count):
    eo = cf + imports + i * entsize
    if ifmt == 1:
        word = struct.unpack_from("<I", data, eo)[0]
        ordinal = word & 0xFF
        nameoff = word >> 9
    elif ifmt == 2:
        word = struct.unpack_from("<I", data, eo)[0]
        ordinal = word & 0xFF
        nameoff = word >> 9
    else:
        word = struct.unpack_from("<Q", data, eo)[0]
        ordinal = word & 0xFFFF
        nameoff = word >> 32
    ns = cf + symbols + nameoff
    name = data[ns : data.index(b"\0", ns)].decode()
    print(f"[{i}] ordinal={ordinal} name={name}")
