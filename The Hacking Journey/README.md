# Running opencode 1.19.2 on macOS 10.15 (Catalina)

The official opencode **v1.19.2** `darwin-x64` binary is built for macOS 13+
(`minos 13.0`, `sdk 15.2`). This directory contains everything needed to make it
run on **macOS 10.15.7 (Intel x86_64)** — CLI, TUI, and `run` — without touching
any system file.

It is a collection of shell/Python build scripts, a small C++ shim
(`shim.cpp`), and documentation. The `reproduce.sh` script downloads the
official binary and its sources, applies the patches, and produces a
self-contained install directory (by default `~/opencode-cli-10.15`).

> Everything is applied to *copies* of the upstream binary and to libraries we
> build ourselves. Nothing is written outside the chosen install directory
> (plus the PATH line you add). Deleting the install directory fully uninstalls.

## TL;DR

```sh
./reproduce.sh
export PATH="$HOME/opencode-cli-10.15:$PATH"
opencode --version      # 1.19.2
opencode                # TUI
opencode run "hello"
```

## Prerequisites

- macOS 10.15.x on Intel (x86_64).
- Xcode Command Line Tools (`clang`, `codesign`, `otool`, `nm`).
- `curl`, `unzip`, `tar`, `python3`.
- Network access (to fetch the official binary + ICU/SQLite sources).

## What breaks, and why

The binary is a single-file **Bun** application (Bun embeds JavaScriptCore,
which links the *system* ICU dynamically). Five independent problems surface on
10.15:

1. **`minos 13.0`** — dyld refuses to launch a Mach-O built for a newer OS.
2. **Missing libSystem symbols** — the binary imports APIs introduced in
   macOS 11 that do not exist in 10.15's `/usr/lib/libSystem.B.dylib`:
   `___ulock_wait2`, `_preadv$NOCANCEL`, `_pwritev$NOCANCEL`,
   `_pthread_self_is_exiting_np`.
3. **Missing ICU symbols** — the binary imports ICU APIs newer than the 10.15
   system `libicucore` (which is **ICU 64.2**): `ubrk_clone`,
   `ucal_getHostTimeZone`, `ucal_getTimeZoneOffsetFromLocal`,
   `udtitvfmt_formatCalendarToResult`, `ulistfmt_openForType`,
   `uplrules_selectForRange`, and the whole `unumrf_*` number-range API.
4. **New ICU number skeleton syntax** — JSC emits ICU 67+ skeletons such as
   `integer-width/*0`, which ICU 64.2's parser rejects, making every
   `new Intl.NumberFormat(...)` throw (`Failed to initialize NumberFormat`) and
   breaking the TUI.
5. **Old SQLite** — Bun `dlopen`s `libsqlite3.dylib` at runtime. 10.15 ships
   SQLite 3.28, which lacks `RETURNING`.
6. **TUI native library** — the TUI embeds a prebuilt `libopentui.dylib` (the
   `@opentui/core` native library, built for macOS 13) that is extracted to a
   temp file and `dlopen`ed at startup; it needs `__ulock_wait2`.

## The strategy

- **Shim (`shim.dylib`)** — a small dylib that
  - provides the missing macOS 11 LibreSSL/libSystem functions on top of 10.15
    APIs,
  - implements the missing ICU functions as forwarders over the *system*
    ICU 64.2 C/C++ API,
  - **re-exports** `/usr/lib/libicucore.A.dylib` and
    `/usr/lib/libSystem.B.dylib`, so everything else still resolves normally,
  - rewrites ICU 67+ number skeletons to 64.2 syntax.
- **Binary patch** — `patch.py` edits `opencode.bin` in place:
  - rewrites the main executable's `LC_BUILD_VERSION` to `minos 10.15`,
  - repurposes the executable's `libicucore.A.dylib` load command to
    `@executable_path/shim.dylib` (the strings are the same length) and rebinds
    the 17 imports the shim supplies to that ordinal,
  - locates the embedded, uncompressed `libopentui.dylib` blob, lowers its
    `minos` to 10.15 and rewrites its `libSystem.B.dylib` dependency to
    `@executable_path/shim.dylib`.
- **SQLite** — build SQLite 3.46.1 from the amalgamation for
  `x86_64-apple-macos10.15`; the launcher points `DYLD_LIBRARY_PATH` at it.

## Files

| File | Purpose |
| ---- | ------- |
| `reproduce.sh` | End-to-end: download, build shim + sqlite, patch, install |
| `download.sh` | Fetch official binary, ICU 64.2 sources, SQLite amalgamation |
| `patch.py` | Consolidated binary patcher (minos + ICU rebind + libopentui blob) |
| `build-shim.sh` | Build `shim.dylib` from `shim.cpp` |
| `build-sqlite.sh` | Build `libsqlite3.dylib` (3.46.1) |
| `shim.cpp` | Source of the compatibility shim |
| `launcher` | Runtime wrapper installed as `opencode` |
| `tests/test-shim.c` | Exercises every shim entry point directly |
| `tests/test-numberformat.c` | Probes ICU number skeletons for compatibility |
| `tools/imports.py` | Dump a Mach-O's chained-fixup imports (with ordinals) |
| `tools/patch_minos.py` | Historical: only the main-binary minOS patch |
| `tools/patch_binary.py` | Historical: ICU rename + import rebind |
| `tools/patch_opentui.py` | Historical: embedded libopentui blob patch |
| `build/` | Scratch dir for downloads and build outputs (gitignored) |

The `tools/patch_*.py` scripts are the incremental steps that `patch.py`
consolidates; they are kept for reference and diagnostics.

## Step by step

### 1. Download inputs

```sh
./download.sh            # -> build/opencode, build/icu, build/sqlite-amalgamation-3460100
```

- `opencode` is the official v1.19.2 darwin-x64 executable
  (`https://github.com/anomalyco/opencode/releases/download/v1.19.2/opencode-darwin-x64.zip`).
- ICU **64.2** is used only for its headers; the shim links against the system
  `libicucore` at runtime, so the headers must match the 10.15 ABI.
- SQLite **3.46.1** amalgamation.

### 2. Build the shim

```sh
./build-shim.sh          # -> build/shim.dylib
```

Two details are essential:

- `-DU_DISABLE_RENAMING=1`. macOS ships ICU compiled with symbol renaming
  disabled, so the C++ namespace is `icu` (not `icu_64`) and the C symbols have
  no version suffix. The shim must be compiled the same way or nothing links.
- `-reexport_library` for both `libicucore.A.dylib` and `libSystem.B.dylib`.
  This is what lets the shim stand in for those libraries in the loader.

### 3. Build SQLite

```sh
./build-sqlite.sh        # -> build/libsqlite3.dylib
```

### 4. Patch the binary

```sh
python3 patch.py build/opencode build/opencode.bin
codesign --force --sign - build/opencode.bin
```

### 5. Install

```sh
mkdir -p ~/opencode-cli-10.15
cp launcher ~/opencode-cli-10.15/opencode
cp build/opencode.bin build/shim.dylib build/libsqlite3.dylib ~/opencode-cli-10.15/
chmod +x ~/opencode-cli-10.15/opencode ~/opencode-cli-10.15/opencode.bin
export PATH="$HOME/opencode-cli-10.15:$PATH"
```

`reproduce.sh` performs steps 1–5 in one go.

## How the pieces work

### Missing symbols: the shim

The shim defines the 17 symbols the binary imports that 10.15 lacks:

- **libSystem**: `__ulock_wait2` delegates to the 10.15 `__ulock_wait` (its
  `flags2` argument is dropped); `preadv$NOCANCEL`/`pwritev$NOCANCEL` are
  rebuilt on `pread`/`pwrite`; `pthread_self_is_exiting_np` returns 0.
- **ICU**: forwarders over the ICU 64.2 C++ API, e.g. `ucal_getHostTimeZone`
  falls back to `ucal_getDefaultTimeZone`; `ubrk_clone` dispatches to the
  virtual `BreakIterator::clone()`.
- **`unumrf_*`** (the ICU 68 range formatter): 10.15 has no C range API, so the
  shim builds `FormattedNumberRange` objects. The tricky part is that callers
  hand the result back to the *system* `ufmtval_getString`, which validates a
  magic number via `IcuCApiHelper<UFormattedValue, UFormattedValueImpl, ...>`.
  The shim therefore derives its result object from `icu::UFormattedValueImpl`
  so the system validator accepts it.
- **Fluent range setters** (`numberFormatterBoth`, `collapse`,
  `identityFallback`): 10.15 does not export these `NumberRangeFormatterSettings`
  methods, so the shim defines them (they can reach the private constructor that
  propagates the range locale).

### Rebinding imports to the shim

The binary resolves ICU symbols from its `libicucore.A.dylib` load command.
`patch.py` renames that command to `@executable_path/shim.dylib` (same byte
length) so the slot now points at the shim, then walks the chained-fixup import
table and changes the ordinal of each of the 17 imports to that slot. Because
the shim re-exports libicucore, every other ICU symbol still resolves.

### Number skeletons

Wrapping `unumf_openForSkeletonAndLocale` in the shim lets us intercept JSC's
skeleton. When ICU 64.2 rejects it (`U_NUMBER_SKELETON_SYNTAX_ERROR`, 65811),
the shim rewrites the newer tokens and retries:

- `integer-width/*N` → `integer-width/N`
- `unit/...` → `measure-unit/...`

(Set `OPENCODE_SHIM_TRACE=1` to log sanitizer activity to `/tmp/shim-trace.log`.)

### The embedded TUI library

The TUI's native `libopentui.dylib` is stored *uncompressed* inside the
executable as an embedded asset, extracted to a temp file, and `dlopen`ed.
Because the temp name is randomized per run, patching the extracted file is not
an option. `patch.py` instead edits the embedded copy: it lowers the blob's
`minos` and repoints the blob's `libSystem.B.dylib` dependency at
`@executable_path/shim.dylib`, so the shim both supplies `__ulock_wait2` and
keeps re-exporting libSystem for the blob.

### SQLite

Bun opens `libsqlite3.dylib` by leaf name. The launcher prepends the install
directory to `DYLD_LIBRARY_PATH`, so our 3.46.1 build wins over the system 3.28.

## Verifying

```sh
# Shim entry points, independent of opencode:
cd build
clang -o test-shim ../tests/test-shim.c -licucore -ldl && ./test-shim

# Number-skeleton probes:
clang -o test-numberformat ../tests/test-numberformat.c -licucore && ./test-numberformat
```

Useful diagnostics while investigating:

- `tools/imports.py <macho>` prints imports with their library ordinals.
- `DYLD_PRINT_LIBRARIES=1 ./opencode run "hi"` shows which libraries load.
- `strings -a opencode.bin | rg "Failed to initialize NumberFormat"` locates the
  JavaScriptCore message (it is a JSC string, not opencode's).

## Known limitations

- **Desktop app** is out of scope: its Electron build requires macOS 12+.
- `Intl.PluralRules.selectRange` is approximated from the range's first number
  (10.15's ICU has no range plural data). Date-interval and list formatting are
  fully functional.
- The patches are tied to the **v1.19.2 darwin-x64** binary layout (load-command
  order and the embedded blob offset). Other versions may need the offsets in
  `patch.py` re-derived.
- `DYLD_LIBRARY_PATH` is set only inside the launcher process; nothing is added
  to your global environment.

## Uninstall

```sh
rm -rf ~/opencode-cli-10.15
# remove the PATH line referencing opencode-cli-10.15 from ~/.zshrc
```
