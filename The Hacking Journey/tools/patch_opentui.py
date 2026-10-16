import os
import struct
import sys

BIN = os.path.expanduser("~/opencode-cli-10.15/opencode.bin")
INSTALL_NAME = b"@rpath/libopentui.dylib"
INSTALL_NAME_OFF_IN_BLOB = 1840
MINOS_OFF_IN_BLOB = 1892
LIBSYSTEM_PATH_OFF_IN_BLOB = 2264
LIBSYSTEM_PATH_LEN = 32
NEW_PATH = b"@executable_path/shim.dylib"
NEW_MINOS = (10 << 16) | (15 << 8) | 0

data = bytearray(open(BIN, "rb").read())

anchor = data.find(INSTALL_NAME)
if anchor < 0:
    sys.exit("libopentui install name not found")
blob_start = anchor - INSTALL_NAME_OFF_IN_BLOB
if data[blob_start:blob_start + 4] != b"\xcf\xfa\xed\xfe":
    sys.exit("no Mach-O header at computed blob start")

minos_off = blob_start + MINOS_OFF_IN_BLOB
cur = struct.unpack_from("<I", data, minos_off)[0]
print("blob_start=0x%x minos=%d.%d.%d" % (blob_start, cur >> 16, (cur >> 8) & 0xFF, cur & 0xFF))
struct.pack_into("<I", data, minos_off, NEW_MINOS)

path_off = blob_start + LIBSYSTEM_PATH_OFF_IN_BLOB
cur_path = bytes(data[path_off:path_off + LIBSYSTEM_PATH_LEN]).split(b"\0")[0]
print("libSystem path was %r" % cur_path.decode())
if cur_path != b"/usr/lib/libSystem.B.dylib":
    sys.exit("unexpected libSystem path")
if len(NEW_PATH) + 1 > LIBSYSTEM_PATH_LEN:
    sys.exit("new path too long")
data[path_off:path_off + LIBSYSTEM_PATH_LEN] = NEW_PATH + b"\0" * (LIBSYSTEM_PATH_LEN - len(NEW_PATH))

open(BIN, "wb").write(data)
print("patched embedded libopentui: minos -> 10.15, libSystem -> %s" % NEW_PATH.decode())
