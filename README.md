# 86x64

A static binary translator that turns 32-bit i386 Mach-O programs (executables,
dylibs, whole `.app` bundles) into x86_64, so legacy Mac apps that stopped
running with macOS 10.15 run again — on Apple Silicon under Rosetta 2.

It is not an emulator: each i386 instruction is rewritten once, ahead of time,
and a runtime library bridges the 32-bit program to today's 64-bit system
frameworks (Objective-C, CoreFoundation, AppKit, and reimplementations of
removed APIs such as Carbon, QuickDraw, QuickTime and AGL).

## Layout

| Path | What |
|---|---|
| `src/core`, `include/core` | Mach-O parser/emitter and the per-instruction i386→x86_64 rewriter (XED) |
| `src/macho-tool` | `macho-tool`, the low-level engine (`rebasify`, `transform`, `convert`, `print`, …) |
| `src/abiconv` | `libabiconv.dylib`: ObjC forward/reverse bridge, generated C-function bridges (`abigen`), hand-written shims for removed frameworks, the low-4GB heap |
| `src/86x64` | `m64` (the driver), `86x64.sh` (per-binary pipeline), the launch wrapper, diagnostics and `probes/` |
| `tests-i386` | i386 fixtures translated and run end to end, plus A/B regression guards |

## Build

Needs Xcode command line tools, `nasm`, and LLVM/libclang (for `abigen`).

```
git submodule update --init
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

## Use

```
m64 translate Some.app          # translate every i386 binary in a bundle, then shim + sign
m64 retranslate Some.app        # after a translator (core) change
m64 build abiconv && m64 resync Some.app   # after a runtime-library change
```

`m64 <command> -h` lists the rest (run, sign, forks, macho passthrough, …).

## Tests

```
cd tests-i386
make            # core fixtures      make objc / make cpp   # ObjC / C++ fixtures
make <guard>    # one named A/B regression guard (see the Makefile)
```

Modern `ld` no longer links i386, so fixtures link with Snow Leopard's ld64-95
against an i386 sysroot staged in `/tmp/i386-sysroot` (see `tests-i386/README.md`).

## Credits

Originally created by Nicholas Mosier ([nmosier/86x64](https://github.com/nmosier/86x64)).
Thanks to michaeljclark for https://github.com/michaeljclark/libSystem-mmap.
