# opencode CLI for macOS 10.15 (Catalina)

A patched build of the official opencode **v1.19.2** `darwin-x64` binary that
runs on macOS 10.15.7. Everything lives in this directory and is self-contained;
deleting the directory fully uninstalls it.

## Usage

```
opencode            # requires this directory on PATH (already added to ~/.zshrc)
opencode run "hi"
opencode --version
```

Always launch through this directory's `opencode` **wrapper script**, not
`opencode.bin` directly: the wrapper sets `DYLD_LIBRARY_PATH` so the bundled Bun
runtime loads the newer SQLite shipped here.

## Contents

| File               | Purpose                                                        |
| ------------------ | -------------------------------------------------------------- |
| `opencode`         | Launcher (sets `DYLD_LIBRARY_PATH`, execs `opencode.bin`)      |
| `opencode.bin`     | Official v1.19.2 darwin-x64 binary, patched (see below)        |
| `shim.dylib`       | Compatibility shim for macOS 11+ / newer ICU symbols           |
| `libsqlite3.dylib` | SQLite 3.46.1, built here for 10.15                            |
| `patch.py`         | Reproducible patcher for `opencode.bin` (minos, ICU, opentui)  |
| `shim.cpp`         | Source of `shim.dylib`                                          |

## Why it needs patching

The official binary is built for macOS 13+. Four things break on 10.15:

1. **`minos 13.0`.** The Mach-O load command `LC_BUILD_VERSION` was rewritten to
   `10.15.0` so dyld accepts the binary.
2. **Missing symbols.** The binary imports macOS 11+ libSystem APIs and ICU
   (65–69) APIs absent from 10.15's system `libicucore` (ICU 64.2). `shim.dylib`
   supplies them:
   - libSystem: `__ulock_wait2`, `preadv$NOCANCEL`, `pwritev$NOCANCEL`,
     `pthread_self_is_exiting_np` — reimplemented on top of 10.15 APIs.
   - ICU: `ubrk_clone`, `ucal_getHostTimeZone`, `ucal_getTimeZoneOffsetFromLocal`,
     `udtitvfmt_formatCalendarToResult`, `ulistfmt_openForType`,
     `uplrules_selectForRange`, and the `unumrf_*` number-range formatter API —
     implemented as forwarders over the ICU 64.2 C++ API. The binary was edited
     so its `libicucore` load entry points at `@executable_path/shim.dylib`,
     which re-exports the system ICU.
3. **Old SQLite.** Bun `dlopen`s `libsqlite3.dylib` at runtime. macOS 10.15 ships
   SQLite 3.28, which lacks `RETURNING` (used by opencode for session storage).
   The wrapper points the loader at the bundled SQLite 3.46.1 first.
4. **TUI native library (`libopentui.dylib`).** The TUI embeds a prebuilt
   `@opentui` native library that is extracted to a temp file and `dlopen`ed at
   startup. It is built for macOS 13 and imports `__ulock_wait2`. The embedded
   copy inside `opencode.bin` was patched in place: its `minos` lowered to
   `10.15`, and its `libSystem.B.dylib` dependency rewritten to
   `@executable_path/shim.dylib`, so the shim both satisfies `__ulock_wait2` and
   re-exports libSystem.

Two runtime incompatibilities are handled inside `shim.dylib`:

- **Number skeletons.** JSC emits ICU 67+ skeleton syntax (e.g.
  `integer-width/*0`) that the 10.15 ICU rejects with a syntax error, which made
  every `new Intl.NumberFormat(...)` throw (`Failed to initialize NumberFormat`)
  and broke the TUI. The shim rewrites the newer tokens (`integer-width/*N` →
  `integer-width/N`, `unit/...` → `measure-unit/...`) and retries.
- **Missing ICU entry points** listed above.


## Limitations

- The opencode **desktop app** is not covered; its Electron build requires
  macOS 12+.
- `Intl.PluralRules.selectRange` is approximated from the range's first number
  (10.15's ICU has no range plural data); ICU date-interval and list formatting
  are fully functional.

## Uninstall

```
rm -rf ~/opencode-cli-10.15
# then remove the PATH line referencing opencode-cli-10.15 from ~/.zshrc
```

## Rebuilding

Recreate `opencode.bin` from the official v1.19.2 `darwin-x64` binary:

```
python3 patch.py <official-opencode> opencode.bin
codesign --force --sign - opencode.bin
```

Rebuild `shim.dylib` (needs the ICU 64.2 headers under `icu64/`; see the
`clang++` command in `shim.cpp`'s history — it links against the system
libicucore and libSystem, both re-exported):

```
clang++ -dynamiclib -o shim.dylib shim.cpp \
  -I icu64/icu/source/common -I icu64/icu/source/i18n \
  -DU_DISABLE_RENAMING=1 -std=c++17 -fvisibility=default \
  -Wl,-reexport_library,/usr/lib/libicucore.A.dylib \
  -Wl,-reexport_library,/usr/lib/libSystem.B.dylib \
  -Wl,-install_name,@rpath/shim.dylib
codesign --force --sign - shim.dylib
```

## License

MIT. This project is an unofficial patched build and is not affiliated with or
endorsed by opencode. The bundled `opencode.bin` derives from the MIT-licensed
opencode project; see `LICENSE` and `NOTICE`.

## Credits

The porting work in this repository (binary patching, the compatibility shim,
the SQLite build, and the accompanying scripts) was done by
[opencode](https://opencode.ai) — the AI coding agent — using the
`opencode/deepseek-v4.1-flash-free` model.

