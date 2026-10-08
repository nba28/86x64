## What and why

<!-- What does this change, and which problem does it fix? Link the issue if there is one. -->

## Root cause

<!-- For fixes: what was actually going wrong, and which i386 pattern or ABI shape triggers it. -->

## How it was verified

<!-- Paste the `make check` summary line, and say which app(s) you tested on. -->

## Checklist

- [ ] `cd tests-i386 && make check` passes, or every failure is explained as environmental.
- [ ] Fixes come with a two-arm guard: ON passes, and OFF (`M64_NO_<FIX>=1`) reproduces the bug. I ran it standalone and checked both arms.
- [ ] Core (`src/core`) changes: I compared translations of real binaries before and after, and explain every difference that isn't layout noise.
- [ ] The fix triggers on a structural property (an ABI shape, a Mach-O invariant, an encoding), not on an app name, address or offset.
- [ ] Any new shim that returns a constant uses `GAP_STUB(args)` or has a reasoned entry in `src/86x64/coverage-audit.ok`.
- [ ] No generated files, build output, IDE settings, application binaries, game data or SDK files are included.
