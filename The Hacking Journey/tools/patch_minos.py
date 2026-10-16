import struct
import sys

path = sys.argv[1]
MINOS = (10 << 16) | (15 << 8) | 0  # 10.15.0

with open(path, "rb") as f:
    data = bytearray(f.read())

magic = struct.unpack_from("<I", data, 0)[0]
if magic == 0xFEEDFACF:  # 64-bit Mach-O
    ncmds_off, sizeof = 16, 32
elif magic == 0xBEBAFECA:  # fat
    raise SystemExit("fat binary; patch each arch slice instead")
else:
    raise SystemExit(f"unknown magic {magic:#x}")

ncmds = struct.unpack_from("<I", data, ncmds_off)[0]
off = 32
patched = []
for _ in range(ncmds):
    cmd, cmdsize = struct.unpack_from("<II", data, off)
    if cmd == 0x32:  # LC_BUILD_VERSION
        platform, minos, sdk = struct.unpack_from("<III", data, off + 8)
        struct.pack_into("<I", data, off + 12, MINOS)
        patched.append((platform, minos, sdk))
    off += cmdsize

with open(path, "wb") as f:
    f.write(data)

print(f"patched {len(patched)} LC_BUILD_VERSION -> minos 10.15.0", patched)
