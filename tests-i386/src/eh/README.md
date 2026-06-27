# C++ exception unwinding validation set (f01–f05)

Fixtures for `throw`/`catch`/`rethrow`/unwind-through-destructor across
**translated i386 frames** (libabiconv `eh_shim.c` + `eh_tramp.asm`). Run with
`./run_eh.sh` (kept out of `make cpp` so the green cpp suite is unaffected until
the core PC map lands).

| fixture | exercises | needs |
|---|---|---|
| f01_throw_int     | scalar throw, catch-by-value, same frame | Wall 2 (PC map) |
| f02_throw_object  | class object, catch-by-value | Wall 2 |
| f03_catch_by_ref  | throw Derived, catch Base& (inheritance) | Wall 2 + RTTI inheritance walk |
| f04_rethrow       | inner catch, `throw;`, outer catch (cross-frame) | Wall 2 |
| f05_unwind_dtor   | local dtor runs via cleanup landing pad during unwind | Wall 2 |

## Two walls

- **Wall 1 — ABI mismatch (DONE).** The i386 binary imports `__cxa_throw`,
  `__cxa_begin_catch`, `__cxa_end_catch`, `__cxa_allocate_exception`,
  `__gxx_personality_v0`, `_Unwind_Resume`. Before this work they bound to the
  native x86_64 libstdc++/libunwind and crashed on the i386 cdecl call frame
  (over-pop / fused PC). Now they are i386-discipline shims in `eh_shim.c`
  (binds redirected by static-interpose), so every fixture is *callable*.

- **Wall 2 — stale CFI (pending core PC map).** macho-tool copies the original
  i386 `__eh_frame`/`__gcc_except_tab` through opaquely, so every offset/range in
  them is in the ORIGINAL i386 address space while the code now lives at
  different x86_64 addresses. The libabiconv unwinder + personality
  (`eh_shim.c`) is complete except for the one primitive it cannot synthesize at
  runtime: the original↔translated PC map. That must be emitted by macho-tool as
  `__DATA,__86x64_pcmap` (per-instruction `{trans_off,orig_off}`) +
  `__DATA,__86x64_ehlsda` (`{trans_func,orig_func,lsda_off}` per EH function).
  Until then a throw cleanly **terminates** (`[eh] terminate: no translator PC
  map …`, exit 134 = std::terminate) instead of crashing.

Expected `expected/*.txt` are the FINAL post-unwind outputs; the set goes green
once the PC map lands (f03 also needs the RTTI inheritance walk).
