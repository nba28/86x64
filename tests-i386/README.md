# tests-i386 — translator regression suite

Small i386 Mach-O programs that exercise specific 86x64-translator features
end-to-end. Each test builds an i386 binary, translates it via the full
`86x64.sh` pipeline, runs the resulting x86_64 binary, and diffs stdout
against `expected/<name>.txt`.

The original `test32.asm` from the upstream repo covered single-binary
CLI patterns but no framework-linked or PIC-anchor patterns; this suite
adds tests for the bugs we actually hit translating iPhoto / Tessera /
photocd.

## Setup

Requires the Snow Leopard install image mounted at
`/Volumes/Mac OS X Install DVD` (modern Xcode CLT no longer ships an i386
slice of libSystem). Then once:

```
make sysroot   # builds /tmp/i386-sysroot from the SL DVD
```

## Running

```
make           # builds + translates + runs + diffs all tests
make build/04_pic_data_read.x86_64   # one test, build only
```

Each test produces in `build/`:

| File                 | Meaning                            |
|----------------------|------------------------------------|
| `<name>.o`           | nasm/clang i386 object             |
| `<name>.i386`        | linked i386 executable             |
| `<name>.x86_64`      | wrapper exec from 86x64.sh         |
| `<name>.x86_64.dylib`| translated code (where bugs live)  |
| `<name>.actual`      | stdout from running the wrapper    |

## Test conventions

- Source: nasm `.asm` or C `.c`; entry symbol is `_main`.
- End every `_main` with `call _exit` (the wrapper enters via `jmp`, so
  there's no return address on the stack — `ret` would SIGSEGV).
- Expected output: `expected/<name>.txt`, ending with `exit_code: N`
  (the runner appends the exit code as the last line of `.actual`).
- Naming: `NN_short_description.{asm,c}` — `NN` orders the suite but
  doesn't carry semantics.

## Current tests

| #  | Test                | Status   | What it catches                                  |
|----|---------------------|----------|--------------------------------------------------|
| 01 | hello               | PASS     | `c7 04 24 imm32` stack-arg ptr; basic call/exit  |
| 02 | return_value        | PASS     | i386 call thunk preserves eax across return      |
| 03 | stack_imm_args      | PASS     | benign int constants in `c7 04/44/84 24` aren't mis-relocated |
| 04 | pic_data_read       | **FAIL** | `call $+0; pop %ebx; mov disp(%ebx)` PIC pattern — disp not recomputed after layout shift. **The Tessera root cause.** |
| 05 | func_ptr_table      | **FAIL** | `call [mem32]` becomes `callq *(%rip)` (8-byte read) — reads two packed 4-byte function pointers as one bogus 64-bit address. |
| 06 | recursion           | PASS     | fib(10) — sustained call/ret thunk under stack pressure |

## Adding a test

1. Write `src/NN_name.asm` (or `.c`). Be specific about what translator
   behavior it exercises — comment with the i386 pattern and the
   expected x86_64 output.
2. Compute the expected stdout by reading the assembly. Save as
   `expected/NN_name.txt`, ending with `exit_code: N`.
3. `make NN_name.x86_64` to build, then `build/NN_name.x86_64 > /tmp/x`
   to sanity-check, then `make` to run the whole suite.
4. If the test exposes a translator bug, leave it failing and document
   the bug in `project_86x64_translator_gaps.md`.
