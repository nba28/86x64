# tests-i386: the end-to-end regression suite

Every test here is a small **i386 program**. The suite compiles and links it
for i386, translates it with the real pipeline (`src/86x64/86x64.sh`, the same
path `m64 translate` uses), runs the x86_64 result, and compares what it
printed and its exit code with `expected/<name>.txt`.

On top of the plain fixtures there are **guards**: named Makefile targets that
each pin down one bug that was fixed, and prove the fix in both directions
(see [Guards](#guards)).

```
make check            # everything except the on-screen guards (~12 min)
```

---

## Contents of this directory

| Path | What |
|---|---|
| `src/NN_name.{asm,s,c}` | **Core** fixtures (`make`) |
| `src/NN_name.m` | **Objective-C** fixtures (`make objc`) |
| `src/NN_name.cc` | **C++** fixtures (`make cpp`) |
| `src/*_fixture.c`, `src/*.s` without a number, `src/guard/` | Inputs used only by a particular guard script |
| `src/eh/` | Standalone C++ exception-unwinding set with its own runner (`run_eh.sh`, see its README) |
| `expected/NN_name.txt` | Expected stdout of each numbered fixture, ending with `exit_code: N` |
| `Makefile` | Suites, sysroot staging, and the inline guard targets |
| `*_test.sh` | Guards too involved to write inline in the Makefile (one script per guard, called from its Makefile target) |
| `check.sh` | Runs every suite and every guard under a deadline and prints a summary (`make check`) |
| `canonical_matrix.sh` | Checks that classic dylibs translated with a synthesized `LC_DYLD_INFO` are accepted by the stock cctools |
| `set_section.py`, `text_addr_of.py` | Helpers fixtures use to shape or inspect their i386 input |
| `bench/` | ObjC message-send microbenchmark: translated vs native |
| `build/` | All output; ignored by git |

`NN` only orders the suite; it has no meaning beyond that, and numbers repeat.

---

## Setup

Modern Apple linkers no longer link i386, and modern SDKs no longer contain
i386 libraries. The fixtures are therefore built against a **Snow Leopard
(10.6) sysroot** staged in `/tmp`.

### 1. Tools and local paths

You need the main project built (`cmake --build build`, see the top-level
README), plus three things from Snow Leopard. Their locations come from the
`M64_*` path variables shared by the whole project; run `m64 paths` to see
what is set and what exists.

| Variable | Default | Needed for |
|---|---|---|
| `M64_SDK106` | `$M64_WORKSPACE/Library/SDKs/MacOSX10.6.sdk` | Headers for the i386 sysroot; the 10.5 `crt1` some fixtures link |
| `M64_I386_LD` | `$M64_WORKSPACE/Library/Toolchains/sl-ld64/ld-i386` | Snow Leopard's `ld64-95` behind a small wrapper. Without it the Makefile falls back to `ld`, which cannot link i386 on current macOS |
| `SL_DVD` | `/Volumes/Mac OS X Install DVD` | The mounted Snow Leopard install image the sysroot libraries come from |

`M64_WORKSPACE` defaults to `~/projects`. Set it once to move everything, or
set an individual variable. Both the environment and the make command line
work: `make M64_SDK106=/path/to/MacOSX10.6.sdk`. `I386_SYSROOT` (default
`/tmp/i386-sysroot`) can be overridden the same way.

The `ld-i386` wrapper runs Snow Leopard's `ld64-95` (extracted from the
install image's `DeveloperToolsCLI.pkg`; its x86_64 slice runs under Rosetta).
The wrapper drops `-no_pie` and renames `-macos_version_min` to
`-macosx_version_min`.

### 2. Stage the sysroot (once, and again after every reboot)

```sh
hdiutil attach "<Snow Leopard install image>.iso" -readonly -nobrowse
make sysroot && make sysroot-objc && make sysroot-cpp
ls /tmp/i386-sysroot/usr/lib        # must list libSystem.dylib and libSystem.B.dylib
```

`make sysroot-gl` additionally stages the OpenGL/ApplicationServices
frameworks, which one GL fixture needs.

> **`make sysroot` exits 0 even when the install image is not mounted.** You
> then get a sysroot with headers but no libraries, and every fixture fails to
> link. Always check `usr/lib` as shown above.

---

## Running

| Command | Runs |
|---|---|
| `make` | All core fixtures (build, translate, run, diff) |
| `make objc` / `make cpp` | The Objective-C / C++ fixtures |
| `make <guard-name>` | One guard, e.g. `make jt-back-edge-join` |
| `make check` | Every suite and every guard, each under a deadline. Skips the on-screen guards |
| `make check-gui` | The same, **including** guards that open windows or switch the display to fullscreen. They take over the screen while they run |
| `bash check.sh <target>…` | A chosen subset, with the same deadlines and summary |
| `make list` | The fixture names in each suite |
| `make build-only` | Build and translate the core fixtures without running them |
| `make build/NN_name.x86_64` | Build and translate one fixture |
| `make clean` | Delete `build/` |

`make check` prints one line per target and a summary, writes each target's
output to `build/check/<target>.log`, and exits non-zero if anything failed.
Never run two `make check`s at once: they share `build/`.

Deadlines: each fixture run is killed after `FIXTURE_DEADLINE` seconds
(default 30) and counts as exit code 137. `check.sh` gives each target
`CHECK_DEADLINE` seconds (default 600). A hang therefore shows up as a
failure instead of blocking the run.

### What a fixture run produces

For a fixture `NN_name`, `build/` ends up with:

| File | Meaning |
|---|---|
| `NN_name.o`, `NN_name.i386` | The i386 object and linked executable |
| `NN_name.x86_64` | The translated program: a small wrapper executable… |
| `NN_name.x86_64.dylib` | …and the translated code it loads. Translator bugs live here |
| `NN_name.actual` | What it printed, plus a final `exit_code: N` line; diffed against `expected/` |
| `libabiconv.dylib` | A copy of the runtime the fixtures use. `make` refreshes it whenever the main build's copy is newer; a guard script run directly uses whatever copy is there |

---

## Guards

A guard is a fixture plus a Makefile target that asserts **both arms** of a
fix:

- **ON** (the default): the fixed behaviour, usually a specific exit code
  such as 42 or an expected line of output.
- **OFF**: the same binary run with the fix's kill switch, an environment
  variable named `M64_NO_<FIX>=1`. This must reproduce the original bug.

The OFF arm is what proves that the guard really exercises the fix. A guard
whose two arms both pass tests nothing. A typical inline guard:

```make
scandir-sort: $(BUILD_DIR)/99_scandir_sort.x86_64
	@$(BUILD_DIR)/99_scandir_sort.x86_64 >/dev/null 2>&1; on=$$?; \
	M64_NO_DIR_SHIM=1 $(BUILD_DIR)/99_scandir_sort.x86_64 >/dev/null 2>&1; off=$$?; \
	[ $$on = 42 ] && [ $$off != 42 ] && echo "scandir-sort: PASS (on=$$on off=$$off)" \
	  || { echo "scandir-sort: FAIL (on=$$on off=$$off)"; exit 1; }
```

Guards that need more than that (several inputs, linker tricks, inspecting
the translated binary) live in a `*_test.sh` script that their Makefile
target calls. Guards whose inputs are missing (for example a real
application under `M64_APPS64`) print **SKIP** and pass.

Guards that put windows or a fullscreen switch on the real screen are listed
in the `GUI=` variable in `check.sh`. `make check` skips them, while
`make check-gui` or naming them explicitly runs them.

---

## Adding a fixture

1. Write `src/NN_name.c` (or `.s`, `.asm`, `.m`, `.cc`). Say in a comment
   which i386 pattern or ABI shape it exercises and how the failure looks.
2. Follow the entry conventions. Fixtures link with `-e _main`, so `main` is
   the raw entry point and no C runtime `start` sits behind it:
   - finish with `exit(N)`; never `return` from `main`;
   - read knobs from `getenv`, because **`argv` does not arrive**;
   - print with `printf`/`puts` and let the exit code carry the verdict.
3. Write `expected/NN_name.txt`: the exact expected stdout, ending with
   `exit_code: N`.
4. Run it alone: `make build/NN_name.x86_64 && build/NN_name.x86_64; echo $?`,
   then the whole suite.
5. For a fixture written to reproduce a bug: make sure it **fails the same
   way before your fix**, then fix the translator or runtime until it passes.

All files under `src/` are tracked, including `.s` files: the repository-wide
`*.s` ignore rule has an exception for this directory.

## Adding a guard

1. Give the fix a kill switch: an `M64_NO_<FIX>` environment variable that
   restores the old behaviour, read once (cached), not on every call.
2. Write the fixture as above, then add a Makefile target named after the
   bug, with a comment explaining the bug, the fix and what each arm expects.
   `check.sh` finds new targets automatically.
3. Run it standalone and **check both arms by hand at least once**: ON
   passes, and OFF reproduces the bug. A guard can pass while inert, for
   example when a crash discards the buffered output it was looking for.
4. If it opens windows or changes the display, add it to `GUI=` in
   `check.sh`.

---

## Troubleshooting

| Symptom | Usual cause |
|---|---|
| Every fixture fails to link (`library not found for -lSystem`), or `objc 0/N` | The sysroot was staged without the install image mounted. Mount it and rerun the `sysroot` targets |
| The whole C++ suite reports link failures | `make sysroot-cpp` was not run |
| Some fixtures fail to compile with missing headers | `/tmp/i386-sysroot/usr/include` is incomplete. Delete it and rerun `make sysroot` (it copies the headers from `M64_SDK106`) |
| `linking for i386 is no longer supported` | `M64_I386_LD` doesn't point at the Snow Leopard linker wrapper, so the Makefile fell back to the system `ld` |
| A fixture passes or fails for no apparent reason after a big change | A stale `build/*.x86_64` from an earlier build. Run `rm -f build/*.x86_64` and rebuild |
| A guard script tests old runtime behaviour | `build/libabiconv.dylib` is older than the build. `make build/libabiconv.dylib` refreshes it |
| The suite was green yesterday and is red today | `/tmp` was wiped by a reboot. Stage the sysroot again |
