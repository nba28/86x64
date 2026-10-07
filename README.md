# 86x64

**A static binary translator that rewrites 32-bit i386 Mach-O programs into
x86_64 Mach-O, so Mac software that stopped working with macOS 10.15 Catalina
runs again — natively on Intel Macs and under Rosetta 2 on Apple Silicon.**

86x64 is not an emulator and not a virtual machine. Every i386 instruction is
decoded and re-emitted as x86_64 **once, ahead of time**. The output is an
ordinary 64-bit executable or dylib that the system loader runs directly. A
runtime library, `libabiconv.dylib`, sits at the boundary between the
translated 32-bit code and today's 64-bit system frameworks: it marshals
calling conventions and data layouts, keeps 32-bit pointers valid, bridges the
Objective-C runtime in both directions, and re-implements APIs that Apple
removed from 64-bit macOS (Carbon, QuickDraw, the QuickTime C API, AGL, the
classic Sound Manager, and others).

> **Status: experimental, under active development.** Real applications and
> games run, but every new app tends to surface new gaps. Expect to read crash
> reports. Contributions that turn a crash into a minimal regression test are
> the most valuable kind (see [Contributing](#contributing)).

---

## Table of contents

1. [What works today](#what-works-today)
2. [How it works](#how-it-works)
3. [Repository layout](#repository-layout)
4. [Requirements](#requirements)
5. [Building](#building)
6. [Translating and running an app](#translating-and-running-an-app)
7. [Debugging a translated app](#debugging-a-translated-app)
8. [The test suite (`tests-i386`)](#the-test-suite-tests-i386)
9. [Contributing](#contributing)
10. [Engineering rules](#engineering-rules)
11. [Recurring bug families](#recurring-bug-families)
12. [Known gaps and good places to start](#known-gaps-and-good-places-to-start)
13. [Legal notes](#legal-notes)
14. [License](#license)
15. [Credits](#credits)

---

## What works today

Status of the main test targets as of October 2026. "Works" means verified by
a person using the translated app, not only "it launches".

| Target | State |
|---|---|
| **Plants vs. Zombies** | Fully playable: rendering, music, sound effects, quit. Open: windowed mode draws offset inside its window. |
| **Quinn** (Tetris clone) | Single player, high scores, LAN server/join and multiplayer work. Open: a reflection shade, one button. |
| **Portal 2** (Source engine, non-bundle game tree) | Chapter 1 gameplay works, including the Bink intro video, captions and voice lines; the Steam bridge is live. Open: a crash on quit inside CEF, window frame offset, the `SteamGameStats001` interface. |
| **Halo: Combat Evolved** | Main menu and sound. Open: window geometry, music (`ov_open_callbacks`), some input. |
| **Call of Duty 4** | Key dialog accepted, audio works, the GL window opens. Open: a data-word relocation in the core that breaks a renderer capability check. |
| **QuickTime Player 7** | H.264 playback through a Movie Toolbox → AVFoundation bridge. |
| **Civilization IV** | Reaches "Init Python"; parked. |
| **iPhoto, iWeb, Numbers, Pages** | Parked at various startup blockers (see [Known gaps](#known-gaps-and-good-places-to-start)). |

Software that only ever shipped for PowerPC cannot be translated: 86x64 needs
an i386 slice.

---

## How it works

86x64 has three cooperating components.

### 1. The core translator — `macho-tool` (C++, `src/core`, `src/macho-tool`)

`macho-tool` parses a Mach-O file into a graph of "blobs" (load commands,
sections, symbols, relocations, `__LINKEDIT` streams), rewrites every
instruction from the 32-bit to the 64-bit encoding using
[Intel XED](https://github.com/intelxed/xed), lays the image out again, and
emits a new file. The hard part is not the instruction set; it is knowing
**which 32-bit values are pointers**. An i386 binary stores addresses as plain
32-bit immediates and displacements, in position-independent code (PIC)
anchored on `call $+5; pop %reg`, in jump tables, and in data. All of these
must be found and relocated when the layout changes, and integers that merely
look like addresses must be left alone.

Key files:

| File | Responsibility |
|---|---|
| `src/core/instruction.cc` | Per-instruction XED decode → transform → encode; pointer-immediate and memory-displacement relocation; operand-width guards. Most translator bugs live here. |
| `src/core/section.cc` | The `__text` linear sweep, PIC-anchor detection, jump-table detection, data-section pointer-vs-constant classification, and the `__86x64_pcmap` / `__86x64_xrel` / `__86x64_cpin` sections the translator adds. |
| `src/core/lc.cc` | Load commands (a parser returning `nullptr` drops a command). |
| `src/core/archive.cc` | The whole-image model. |
| `src/core/dyldinfo.cc`, `rebase_info.cc`, `symtab.cc`, `export_info.cc`, `linkedit.cc` | `__LINKEDIT`: binds, rebases, symbols, export trie. Classic images with only `LC_DYSYMTAB` relocations get a synthesized `LC_DYLD_INFO`. |
| `src/core/resolve.cc`, `opcodes.cc` | Address → blob resolution; opcode tables. |

`macho-tool` subcommands (`src/macho-tool`): `rebasify`, `transform`,
`modify`, `convert`, `print`, `noop`, `change-deps`.

The translator writes `__86x64_pcmap`, a table that maps every translated
instruction back to its original i386 address. All the crash-analysis tools
depend on it.

### 2. The runtime — `libabiconv.dylib` and friends (`src/abiconv`, `src/86x64`)

Every translated image links `libabiconv.dylib`. The runtime code is split
into three components, and each piece of code belongs to exactly one of them:

| Component | Owns | Examples |
|---|---|---|
| **libabiconv** — the ABI boundary | Everything that exists *because the caller is translated i386 code*: calling-convention and struct-layout marshalling, the low-4 GB memory model, the Objective-C bridge, image slide/initialisation, lazy binding, the fault reporter, and re-implementations of APIs that exist **only** for 32-bit code. | `objc_shim.c`, `objc_slide.c`, `malloc_shim.c`, the generated `abiconv.asm` bridges, `carbon_*.c`, `qd_*.c`, `quicktime_*`, `agl_*`, `snd_*` |
| **shimdb / shimgen** — missing symbols | Native x86_64 implementations of symbols that *native* x86_64 code in an app bundle binds but modern macOS removed or made private. Per app, `shimgen.py` links only the implementations that app binds, and generates loud stubs for the rest. | `src/86x64/shimdb/impl/*.c|*.m`, `src/86x64/shimgen.py` |
| **libinterpose** — process-wide contracts | `__DATA,__interpose` policy that must hold for native frameworks and libabiconv alike: low-4 GB placement of `mmap`/`vm_allocate`, pthread stack answers, restoring old contracts of still-live APIs that legacy code relies on. Kept tiny and early-loading. | `src/86x64/interpose.c` |

**Which home does a removed API belong in?** Use one compile check: is the
symbol declared for x86_64 in the macOS 10.6 SDK
(`clang -arch x86_64 -isysroot <10.6 SDK> -fsyntax-only`)?

- **No** (guarded by `#if !__LP64__`: FSSpec, Alias Manager, `PB*` calls,
  Memory Manager, QuickDraw, Window/Control/Dialog Manager, classic Sound
  Manager, the QuickTime C API) → **libabiconv**. Only i386 code can ever call
  it, and its data structures use i386 layouts.
- **Yes, but modern macOS dropped it or made it private** → **shimdb/impl**,
  written against the native ABI.

Important runtime mechanisms:

- **Low-4 GB memory model.** i386 code stores pointers in 32-bit slots, so the
  runtime keeps the translated program's heap (`malloc_shim.c`, a heap at
  `[0x88000000, 0xF0000000)`), the stacks it runs native→i386 callbacks on,
  and a proxy/handle arena below 4 GB. Native objects that live above 4 GB
  are represented to i386 code by 32-bit handles.
- **Generated C bridges (`abigen`).** `src/abiconv/abigen.cc` uses libclang to
  read the system headers (`includes.h`, plus a legacy pass over the 10.6 SDK
  in `includes_legacy.h`) and generates about 13,000 C-function bridges into
  `abiconv.asm`. A bridge converts the i386 stack-based call into an x86_64
  register call and converts the result back. **A function with no prototype
  gets no bridge**, and the i386 code then calls the native function raw,
  which crashes (see [bug families](#recurring-bug-families)).
- **Hand shims.** Shapes abigen can't express (callbacks, varargs, `va_list`,
  struct returns, out-parameters with pointer fields) and re-implemented
  removed APIs are written by hand as `*_shim.c` files. Each hand shim has a
  trampoline (the `MTSHIM` macro in a `*_tramp.asm` file) and an entry in
  `src/abiconv/custom.syms`, so abigen skips that symbol.
- **Objective-C bridge.** Forward calls (i386 → native objects), reverse calls
  (native → i386 methods and blocks), registration of legacy (ObjC 1) classes
  with per-instance i386 shadows, and IBOutlet bridging.
- **Static interposition.** The pipeline rebinds each imported `_X` in a
  translated image to libabiconv's `___X` bridge (`static-interpose.sh`).
  Bridges therefore live in the `___X` namespace.

### 3. The driver — `m64` and the pipeline (`src/86x64`)

`src/86x64/m64` is a Python CLI over the whole process. `86x64.sh` is the
per-binary pipeline:

```
i386 Mach-O
  → macho-tool rebasify          # make every pointer explicit
  → macho-tool transform         # M32 → M64, instruction by instruction
  → insert libabiconv load command
  → static-interpose             # bind _X → libabiconv ___X
  → dyld_stub_binder rebind
  → macho-tool convert --archive DYLIB
  → drop duplicate LC_ID_DYLIB
  → [executables] link a small wrapper exec (libwrapper.a, entry _main_wrapper)
x86_64 Mach-O
```

A translated main image is based at `__TEXT` = `0x10000000`.

`m64` adds discovery and deployment around it. It translates **only i386-only
binaries**: a binary that already has an x86_64 slice is always kept native,
because it runs directly under Rosetta and, unlike a translated image, can
export Objective-C classes to other images.

---

## Repository layout

| Path | What |
|---|---|
| `src/core`, `include/core` | Mach-O model and the i386 → x86_64 rewriter |
| `src/macho-tool`, `include/macho-tool` | The `macho-tool` CLI |
| `src/abiconv`, `include/abiconv` | `libabiconv.dylib`: bridges, hand shims, ObjC bridge, low-4 GB heap, fault reporter, and `abigen` (the bridge generator) |
| `src/86x64/m64` | The driver CLI |
| `src/86x64/86x64.sh`, `translate-bundle`, `static-interpose.sh` | The per-binary and per-bundle pipelines |
| `src/86x64/interpose.c`, `wrapper.asm`, `wrapper_setup.c` | `libinterpose.dylib` and the wrapper executable |
| `src/86x64/shimgen.py`, `src/86x64/shimdb/` | Missing-symbol shim generator and its curated implementation pool |
| `src/86x64/shims/` | Self-contained helper shims (for example the Miles MP3 provider, built on minimp3) |
| `src/86x64/paths.sh`, `m64_paths.py`, `cmake/paths.cmake` | The configurable local paths (see [Local paths](#local-paths)) |
| `src/86x64/*.py`, `exit-trace-probe.c` | Diagnostics: `fault-symbolize.py`, `pcmap-diff.py`, `coverage-audit.py`, `unbridged-native-calls.py`, `bridge-shadow-check.py`, `weakdef-coalesce-scan.py`, `diff-digest.py`, … |
| `src/86x64/probes/` | Probes written for one investigation (see its README). They live here, never in the runtime libraries |
| `tests-i386/` | End-to-end fixtures (`src/NN_*.{c,m,cc,s,asm}` + `expected/`) and the A/B regression guards in its `Makefile` |
| `extern/` | Git submodules: Intel XED and mbuild |
| `cmake/`, `scripts/` | Build helpers, shell completion |

Generated files that are **not** sources and must never be committed:
`src/abiconv/abiconv.asm`, `abiconv_legacy.asm`, the derived `*.syms`
files (`abiconv_all.syms`, `abiconv_import.syms`, `abiconv_legacy.syms`,
`abiconv_handshim.syms`, `custom_all.syms`), `includes_auto.h`,
`src/86x64/shimdb/generated/` and `shimdb/observed.json`. They are in
`.gitignore`. By contrast `src/abiconv/custom.syms` and `data_shadows.syms`
are hand-maintained sources.

---

## Requirements

**Host.** macOS on Apple Silicon (with Rosetta 2) or Intel. The build tools
are built as **x86_64** (`CMAKE_OSX_ARCHITECTURES=x86_64`) because their
outputs are x86_64 and libclang has to match; on Apple Silicon they run under
Rosetta.

**Build dependencies.**

- Xcode Command Line Tools (`clang`, `ld`, `lipo`, `codesign`, `otool`, `dyld_info`)
- CMake ≥ 3.10
- NASM (the generated `abiconv.asm` is about 600k lines; NASM 3.x works, see
  the note in `CMakeLists.txt` about `label-redef-late`)
- An **x86_64** LLVM with libclang at `/usr/local/opt/llvm`, which is where
  Homebrew running under Rosetta (`arch -x86_64 /usr/local/bin/brew install llvm`)
  installs it. `abigen` links against it, and XED's build uses its `llvm-ar`.
- Python 3

**Test-suite dependencies.** Modern Apple linkers can no longer link i386, and
modern SDKs contain no i386 libraries. The fixtures are therefore built
against a Snow Leopard (10.6) sysroot:

- A **Mac OS X 10.6 Snow Leopard install image**, mounted at
  `/Volumes/Mac OS X Install DVD`. The sysroot targets extract the i386
  slices of `libSystem` and friends from it.
- The **macOS 10.6 SDK** (`M64_SDK106`), for headers.
- **Snow Leopard's `ld64-95`** behind an `ld-i386` wrapper (`M64_I386_LD`). It
  is extracted from the install image's `DeveloperToolsCLI.pkg`. The wrapper
  drops `-no_pie` and renames `-macos_version_min` to `-macosx_version_min`.

Where these live is configurable; see [Local paths](#local-paths) below and
[`tests-i386/README.md`](tests-i386/README.md) for the full test setup.

You must own legitimate copies of the Apple software and of any application
you translate. Nothing from those products is, or may be, committed to this
repository.

### Local paths

Everything outside the repository (the SDK, the i386 linker, your app
originals and the translated output) is found through a small set of `M64_*`
variables. All of them derive from **`M64_WORKSPACE`** (default
`~/projects`), so most people set one variable, or none if they use the
default layout:

```
$M64_WORKSPACE/                     # default: ~/projects
├── Library/                        # M64_LIBRARY
│   ├── SDKs/MacOSX10.6.sdk/        # M64_SDK106      headers for the i386 sysroot and abigen's legacy pass
│   ├── Toolchains/sl-ld64/ld-i386  # M64_I386_LD     Snow Leopard linker wrapper
│   └── Frameworks/                 # M64_FRAMEWORKS  reusable translated frameworks (e.g. QuickTime)
└── translations/
    ├── Apps32/                     # M64_APPS32      pristine i386 originals, read-only
    └── Apps64/                     # M64_APPS64      translated output
```

Any variable can also be set on its own, for example an SDK somewhere else:

```sh
# in ~/.zshrc
export M64_WORKSPACE="$HOME/dev/86x64-workspace"
export M64_SDK106="/Volumes/SDKs/MacOSX10.6.sdk"   # optional: override just one
```

**`m64 paths`** prints every variable, its resolved value, whether it was set
or defaulted, and whether the path exists. The same names and defaults are
used by the shell scripts (`src/86x64/paths.sh`), the Python tools
(`src/86x64/m64_paths.py`), CMake (`cmake/paths.cmake`) and the test
`Makefile`. CMake caches the values on its first configure, so after changing
one either pass `-DM64_<NAME>=...` or configure a fresh build directory.

The repository itself can live anywhere; nothing assumes it is inside
`M64_WORKSPACE`.

`M64_APPS32` matters for the runtime build: abigen seeds its bridge set from
the imports of the i386 executables it finds there (`ABICONV_MODERN_TARGETS`
in `src/abiconv/CMakeLists.txt`). Missing apps are skipped, but a build that
sees none has a much smaller bridge surface.

---

## Building

```sh
git clone --recurse-submodules <this repo> 86x64
cd 86x64
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

If you cloned without submodules, run `git submodule update --init` first.

Build single targets with `cmake --build build --target macho-tool` or
`--target abiconv`. The `m64` equivalent is `m64 build macho-tool` or
`m64 build abiconv`.

Build notes:

- **Never pipe a build into `head`** (`cmake --build build | head`). SIGPIPE
  kills the build midway and leaves stale objects that look current.
- Don't hand-delete `src/abiconv/abiconv.asm`. It is a slow-to-regenerate
  build artifact.
- If abigen reports skipped functions, check `grep "abigen: skipping"` in the
  build log. Skipped functions get no bridge (unions by value, `va_list`,
  `long double`, …).
- `-DABICONV_O0=ON` builds libabiconv at `-O0`.

Put `m64` on your `PATH`, for example
`ln -s "$PWD/src/86x64/m64" /usr/local/bin/m64`. `m64` resolves the build
artifacts relative to its own location, so a symlink to the checkout you
built is enough. In a second git worktree, run `src/86x64/configure-worktree.sh`
to give that worktree its own build tree, then invoke `./src/86x64/m64` from
inside it. The global `m64` would otherwise deploy the main checkout's
libabiconv.

---

## Translating and running an app

Always work on a **copy**. Keep the pristine i386 original read-only and
translate into a separate output location. `m64` keeps i386 backups under
`~/.86x64-backups/` so a retranslate never needs the original again.

```sh
m64 translate Some.app        # discover and translate every i386-only binary, then shim, consolidate the runtime and sign
m64 run Some.app              # launch
```

### `m64` commands

| Command | Purpose |
|---|---|
| `translate <app\|binary>` | Discover and translate i386-only binaries (`--only`/`--skip RELPATH`, `--list`, `-j`, `-k`, `-n`). Translate the whole **bundle** when you want a launchable app: a single binary translated alone does not get the bundle's dependency graph rewritten. |
| `retranslate <app>` | Redo only binaries 86x64 produced earlier, from their i386 backups. Use after a **core** change. |
| `resync <app>` | Copy the freshly built `libabiconv.dylib` and `libinterpose.dylib` over every copy in the bundle and re-sign. Use after a **runtime-only** change. |
| `reinterpose <app>` | Re-run static interposition against the current libabiconv, so existing translations bind **newly added** shims without a full retranslate. |
| `stale-check <app>` | Find translated binaries that still bind a native symbol raw although a shim for it now exists. Exits non-zero if any are found. |
| `consolidate-runtime <app>` | Keep one libabiconv/libinterpose per bundle and point every image at it. dyld loads each copy at a different path as a separate image with separate globals. |
| `tree <dir>` | Translate a non-bundle directory tree (e.g. a Source-engine game) out of place, with runtime copy, path patching and signing. |
| `vendor <app>` | Copy missing external framework dependencies into a bundle. |
| `abi-libs <app\|dir>` | Redirect binds on a system C++ runtime dylib (e.g. `libstdc++.6.dylib`) to a translated i386-ABI replacement. |
| `shim <app\|binary>` | Generate shims for native symbols removed from modern macOS (shimgen). |
| `rpath-fix`, `sign`, `deploy` | Individual post-translation steps. `deploy` runs rpath → shim → resync → sign → clear saved state. |
| `forks <app>` | List or restore classic Mac resource forks. |
| `run`, `debug` | Launch a translated app, optionally under lldb (`debug` = `run --exc`, which traps ObjC exceptions). |
| `build [target] [--resync APP]` | CMake build, optionally followed by a resync. |
| `paths` | Show the local paths (`M64_*`) and whether each exists. |
| `macho …` | Pass-through to `macho-tool`. |

Run `m64 <command> -h` for full options.

### Which deploy step after a change?

Picking the wrong step silently tests stale code.

| You changed… | Do |
|---|---|
| `src/core` / `macho-tool` | `m64 retranslate <app>` (then re-run any library redirect steps a fresh translate resets) |
| `src/abiconv` only | `m64 build abiconv && m64 resync <app>` |
| Added a new libabiconv export | `m64 reinterpose <app>` |

Verify deployed files **by content** (`strings`, `nm`, `otool -L`), never by
UUID or `cmp`: signing changes the bytes and UUIDs are not content-derived.
Don't rebuild `macho-tool` while a translation is running
(`pgrep -f "macho-tool -- transform"`), or the batch picks up a mixed binary.
Don't resync while the app is running: overwriting a loaded dylib kills the
process.

### Non-bundle programs

Games such as Source-engine titles ship as a directory tree, not an `.app`.
Translate them out of place into a parallel directory with `m64 tree`, and
never write into the original `bin/osx32`. `src/86x64/portal2-translate.sh`
and `src/86x64/run-portal2.sh` are the worked example.

---

## Debugging a translated app

- **Run with the fault reporter:** `M64_FAULT_REPORT=1`. On a crash, libabiconv
  prints the fault context, the registers and a walk of the i386 frame-pointer
  chain. The OS crash report keeps only a few frames and cannot unwind through
  translated i386 frames.
- **Symbolize the log:**
  `src/86x64/fault-symbolize.py --orig <i386 dir> --trans <x86_64 dir> <log>`.
- **Prefer the fault reporter to lldb.** lldb breaks apps that use
  `__DATA,__interpose`, and its timing changes hide races.
- **Targets are singletons.** Before relaunching, `pkill -9 -x <ProcessName>`.
  Never use `pkill -f`: it also matches and kills running translations.
- **Bind questions** ("what does this import resolve to?"): use
  `dyld_info -fixups`, never `nm -m` or `otool -Iv`.
- **Crash reports** (`.ips`): frame 0 often resolves to no image. Map `rip`
  using `usedImages` and the image's `__86x64_pcmap`. In stripped images the
  nearest symbol name is often wrong; trust offsets and load bases from the
  same run.
- **Non-determinism across runs** usually means a pointer whose value depends
  on ASLR or allocator placement: the >4 GB truncation family below.
- **Silent gaps are logged.** Every constant-return shim is marked
  `GAP_STUB(args)` and reports its first hit:
  `abiconv: GAP (silent no-op reached) stub X <image>+0xOFF`. Raw calls into
  native code that bypass a bridge report `raw <sym> <caller>`. Each run writes
  a reach ledger to `$TMPDIR/86x64-reach/`. `src/86x64/coverage-audit.py --reach`
  ranks the reached gaps, and that ranking is the work queue. `M64_GAP=0`
  silences the reports and `M64_GAP=abort` aborts on the first hit.
- Other useful environment variables: `OBJC_BRIDGE_TRACE=1`, `ABICONV_DEBUG`,
  `ABICONV_OBJC_SLIDE_VERBOSE`, `WRAPPER_DEBUG`, `M64_HEAP_GUARD`.
- **Kill switches.** Core and runtime fixes come with an `M64_NO_<FIX>=1`
  environment switch that restores the old behaviour, for example
  `M64_NO_PIC_BACK_EDGE` or `M64_NO_CGL_FULLSCREEN_BRIDGE`. To check whether a
  fix causes a new problem, flip its switch and compare.

---

## The test suite (`tests-i386`)

Each fixture is a small i386 program. The suite compiles and links it for
i386 against a Snow Leopard sysroot, translates it with the real pipeline,
runs the x86_64 result, and diffs its output and exit code against
`expected/<name>.txt`. **Guards** are named Makefile targets that each pin
down one fixed bug, in both directions:

- **ON** (default): the fixed behaviour.
- **OFF** (`M64_NO_<FIX>=1`): the kill switch reproduces the original bug.

The OFF arm is what proves the guard really exercises the fix.

```sh
cd tests-i386
make sysroot && make sysroot-objc && make sysroot-cpp   # once per boot, with the SL install image mounted
make check          # every suite + every non-GUI guard, each under a deadline (~12 min)
```

**Run `make check` before every commit.** Setup, all the targets, how to
write fixtures and guards, and troubleshooting are in
[`tests-i386/README.md`](tests-i386/README.md).

---

## Contributing

Contributions are welcome: bug reports with crash logs, new regression
fixtures, runtime shims for removed APIs, core translator fixes, tooling and
documentation.

### Workflow

1. **Open an issue first** for anything bigger than a small fix. Include the
   app and version, the macOS version and machine, the symbolized fault log
   (`M64_FAULT_REPORT=1` + `fault-symbolize.py`), and what `coverage-audit.py
   --reach` reports for the run.
2. **Reproduce hard bugs as a minimal fixture** in `tests-i386/` before fixing
   them. A fixture has a known expected output and a seconds-long
   build → translate → run → diff loop, and it remains as a permanent
   regression guard. Confirm that it **fails the same way** before your fix.
   Most existing fixtures started as a real crash.
3. **Fix the mechanism, not the symptom.** See
   [Engineering rules](#engineering-rules).
4. **Add a kill switch and a two-arm guard** (see above).
5. **For core (`src/core`) changes, measure the blast radius.** Translate a
   corpus of real i386 binaries with the old and the new `macho-tool`, list
   both outputs' `__text` with layout-dependent operands normalised, and
   **explain every hunk that is not layout noise** in your pull request. Core
   heuristics are validated on real apps; a harmless-looking refactor once
   mistranslated 64 SSE instructions that no fixture covered. The
   maintainer's corpus harness is not in this repository yet. Describe the
   corpus you used.
6. **Run `cd tests-i386 && make check`**. It must be fully green, or every red
   target must be explained as environmental.
7. **Open a pull request** with a focused diff.

### Commit messages

Use the existing style: a short `component: what changed (why / which
symptom)` subject, then a body that explains the root cause, the fix, and how
it was verified. Common prefixes: `core:`, `abiconv:`, `shims:`, `abigen:`,
`m64:`, `86x64.sh:`, `tests-i386:`.

```
core: even function entries after noreturn calls

<root cause: what the translator did, which i386 idiom triggers it>
<the fix and why it is structural>
Guard: make pic-anchor-noreturn-join (ON passes, OFF reproduces).
make check: all green.
```

Stage files by name. Never commit generated files (see
[Repository layout](#repository-layout)), build output, IDE settings, or any
part of a commercial application, SDK or game.

### Code style

Match the surrounding code: naming, comment density, idioms. Comments in this
codebase explain **why**: the root cause, the measured evidence and the
rejected alternatives. Keep that up for non-obvious decisions. C++17 for the
core, C (and Objective-C where AppKit is involved) for the runtime, NASM for
trampolines, Python 3 and POSIX shell for tooling.

### Licensing of contributions

The project does not have a license yet (see [License](#license)). Only submit
code you wrote yourself. Once a license is adopted, it will apply to
contributions made from then on.

---

## Engineering rules

These rules come from bugs that cost days. Reviewers will hold pull requests
to them.

1. **Trigger fixes on structure, never on an application name.** A fix keys
   on an ABI shape, a Mach-O layout invariant or an instruction encoding, never
   on "this is app X", a library name, an address or an offset. 86x64 is a
   general-purpose translator.
2. **One shim, one job.** Each shim, dylib or tool does one thing. A new
   behaviour gets its own unit (and its own kill switch) instead of a branch
   inside an unrelated shim. Reusable shims go in libabiconv or
   `shimdb/impl`, never somewhere app-private.
3. **Put the code in the right runtime component** (see the table in
   [How it works](#2-the-runtime--libabiconvdylib-and-friends-srcabiconv-src86x64)).
   Probe code for one investigation goes in `src/86x64/probes/`.
4. **Silent gaps must be loud.** A shim that returns a constant either carries
   `GAP_STUB(args)` or gets a line with a reason in
   `src/86x64/coverage-audit.ok`. Never write a silent `return 0`. Implement
   only what a real run **reached** (`coverage-audit.py --reach`), not
   everything an app imports.
5. **Functionality first.** A no-op is the last resort, used only for
   surface that is genuinely dead on modern macOS, and it is documented as
   such. Prefer calling the live native API when it survived.
6. **A failing shim leaves every out-parameter defined.**
7. **A shim may misreport what the system says, but must never make the app
   save false data.** It is fine to tell an app what it needs to hear about
   the system. It is not fine to cause the app to write wrong values into its
   preferences or documents.
8. **Inside libabiconv, `malloc` is the low-4 GB i386 heap.** Use
   `malloc_zone_*` for native memory. Don't `free()` an i386-heap pointer
   natively, and redirect APIs that adopt a caller's buffer (`…NoCopy…`,
   `freeWhenDone:YES`) to their copying form.
9. **No per-call `getenv` or other expensive lookups on hot paths.** Every
   translated↔native call crosses a bridge. Use `KNOB("NAME")` (cached per
   call site) or a `static` cache. Avoid per-call `objc_getClassList`, symbol
   table walks, and uncached `class_getInstanceMethod`.
10. **Validate untrusted input; skip speculative generality.** 86x64 parses
    arbitrary Mach-O files produced by toolchains that have been dead for 15
    years. Malformed structure is a normal operating condition, so bounds
    checks and loud fallbacks stay. Unused configuration knobs and single-use
    abstractions do not.
11. **Consolidate rather than accrete.** Before adding a code path, read its
    siblings. Duplicated logic on the path you are fixing counts as part of
    the bug. Every memory-operand rewrite in the core goes through one
    lowering path.
12. **Apps should look like they did on their original macOS**, including on
    Retina: points everywhere, authentic 1x GL, and the app's own original
    windows and resources. Fix this generically, not with per-app presenters.
13. **Read `references/` before touching opcodes or Mach-O structures** (keep
    your own copy of the Mach-O and Intel SDM references there; the directory
    is ignored).

---

## Recurring bug families

Most new crashes are one of these. Recognising the shape saves hours.

1. **Over-pop / fused PC.** An i386 `call` pushes 4 bytes. If a native callee
   was bound raw (no bridge), it `ret`s 8, giving
   `rip = garbage_high | valid_low`. Fix: route the symbol through a libabiconv
   bridge. A missing prototype means a raw call; audit with
   `unbridged-native-calls.py`.
2. **>4 GB pointer truncation.** A native pointer lands in a 4-byte i386 slot
   (raw buffer accessors, `FILE *`, `__IMPORT,__pointers`, CF handles). Fix: a
   handle/arena wrapper or a low-4 GB copy. Tell-tale sign: non-determinism.
3. **PIC anchors and jump tables** (`section.cc`): a displacement anchored on a
   `call $+5; pop` base not recomputed after re-layout, or a jump table not
   claimed.
4. **Pointer vs constant misclassification.** An integer relocated as if it
   were an address, or an address left raw (stale immediates across re-layout,
   misaligned or zerofill targets).
5. **ABI marshalling width gaps.** `CGFloat` float ↔ double, struct returns
   (sret vs register class), varargs, out-parameters with function-pointer
   fields.
6. **Removed frameworks.** A bridge whose native target no longer exists
   jumps to NULL. Fix: re-implement on the modern substrate.
7. **Cross-ABI identity.** Handles that must round-trip (`pthread_t`,
   `FILE *`, `EventRef`). Translated images must not take part in C++
   weak-definition coalescing with native images.
8. **Ownership across the two heaps** (see rule 8 above).
9. **Runtime pools that never shrink.** Symptom: a crash only after long
   uptime.
10. **`va_list`.** An i386 `va_list` is a pointer to 4-byte slots, not a
    `__va_list_tag`.
11. **Multiple runtime copies.** Bundles that ship several copies of libabiconv
    get several sets of globals in one process. Per-process state needs
    cross-copy coordination; `consolidate-runtime` avoids the problem.

---

## Known gaps and good places to start

A partial list of open problems:

- **Core:** some PIC tables in very long functions are still lost;
  `macho-tool modify --insert` can corrupt `__text` on a freshly de-signed
  input; the PIC "orphan" rule still regresses a few corpus sites.
- **abigen / marshalling:** libc routines that call an i386 comparator on
  their own temporaries (`heapsort`, `mergesort`, `qsort_r`); callback
  arguments typed `const void *`; header-derived struct sizes that disagree
  with the runtime (`struct dirent`); ~200 functions abigen skips per build.
- **Carbon / legacy APIs:** MLTE (text views) has no implementation; refNum
  file calls, Desktop Manager, Icon Services suites, Font Manager/ATS,
  QuickDraw regions and text, and the List Manager are unimplemented.
- **Removed frameworks:** QuickTime Movie Toolbox **authoring** → AVFoundation
  (iMovie editing/export); ATSUI → CoreText; the classic Print Manager;
  classic ColorSync; Component Manager codecs.
- **Steam:** i386 virtual dispatch into the modern native `ISteamClient`
  needs a Steamworks-header-driven vtable bridge.
- **Runtime:** no frame cap in CGL fullscreen games; one-display QuickDraw
  model; unexplained `malloc_shim` growth during long sessions.
- **Tooling:** `shimgen` stubs some private symbols that still exist;
  classic type/creator info is discarded on fork-less files.
- **Horizon:** an arm64-native backend for a post-Rosetta world.

New fixtures for already-fixed bugs that lack a behavioural guard (for
example cross-ABI weak-def coalescing, or an uninitialised co-located
libabiconv copy) are also very welcome.

---

## Legal notes

86x64 is an interoperability tool. Translate only software you are licensed
to use. This repository contains no Apple or third-party application code,
and contributions must not add any: no binaries, SDK files, frameworks, game
data or proprietary headers. Product names are used only to describe
compatibility; all trademarks belong to their respective owners.

---

## License

86x64 does not have an open-source license yet. It builds on the original
86x64 by Nicholas Mosier, which was published without a license, so its code
remains the copyright of its author. A license will be added once its terms
can cover the whole codebase.

Third-party components keep their own licenses:

- [Intel XED](https://github.com/intelxed/xed) and
  [mbuild](https://github.com/intelxed/mbuild) (git submodules in `extern/`):
  Apache License 2.0.
- [minimp3](https://github.com/lieff/minimp3)
  (`src/86x64/shims/miles_mp3/minimp3.h`): CC0 1.0, see
  `src/86x64/shims/miles_mp3/LICENSE.minimp3`.

## Credits

- Originally created by Nicholas Mosier —
  [nmosier/86x64](https://github.com/nmosier/86x64).
- Thanks to michaeljclark for
  [libSystem-mmap](https://github.com/michaeljclark/libSystem-mmap).
