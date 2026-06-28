# C++ exception unwinding validation set (f01–f05)

Fixtures for `throw`/`catch`/`rethrow`/unwind-through-destructor across
**translated i386 frames** (libabiconv `eh_shim.c` + `eh_tramp.asm`). Run with
`./run_eh.sh` (kept out of `make cpp` so the green cpp suite is unaffected until
the core PC map lands).

| fixture | exercises | status |
|---|---|---|
| f01_throw_int     | scalar throw, catch-by-value, same frame | **GREEN** (suite `51_eh_throw_int`) |
| f02_throw_object  | class object, catch-by-value | **GREEN** (suite `52_eh_throw_object`) |
| f04_rethrow       | inner catch, `throw;`, outer catch (cross-frame) | **GREEN** (suite `53_eh_rethrow`) |
| f05_unwind_dtor   | local dtor runs via cleanup landing pad during unwind | **GREEN** (suite `54_eh_unwind_dtor`) |
| f03_catch_by_ref  | throw Derived, catch Base& (inheritance) | BLOCKED — see "residual blocker" below |

The four single-catch fixtures (f01/f02/f04/f05) are wired into `make cpp` as
`tests-i386/src/5{1..4}_eh_*.cc` so the translated-frame unwinder is guarded
against regression. f03 stays here (not in the suite) until the residual
blocker lands.

## Two walls

- **Wall 1 — ABI mismatch (DONE).** The i386 binary imports `__cxa_throw`,
  `__cxa_begin_catch`, `__cxa_end_catch`, `__cxa_allocate_exception`,
  `__gxx_personality_v0`, `_Unwind_Resume`. Before this work they bound to the
  native x86_64 libstdc++/libunwind and crashed on the i386 cdecl call frame
  (over-pop / fused PC). Now they are i386-discipline shims in `eh_shim.c`
  (binds redirected by static-interpose), so every fixture is *callable*.

- **Wall 2 — stale CFI / original↔translated PC map (DONE).** macho-tool now
  emits `__DATA,__86x64_pcmap` (per-instruction `{trans_off,orig_off}`) +
  `__DATA,__86x64_ehlsda` (`{trans_func,orig_func,lsda_off}` per EH function);
  the libabiconv unwinder maps each translated return-PC back to its original
  i386 PC, reads the original i386 LSDA to find the landing pad, maps it forward,
  and resumes with `rax=exc / rdx=selector`. The four single-catch fixtures
  unwind end-to-end across translated frames (verified GREEN).

Expected `expected/*.txt` are the FINAL post-unwind outputs.

## Residual blocker (f03 + multi-catch discrimination)

The unwinder/personality logic is correct, but two cases still fail on these
*modern* (`clang++ -arch i386`, LC_DYLD_INFO, ASLR-slid) test binaries — NOT in
the EH code but in a pre-existing **runtime internal-pointer relocation gap** for
modern translated images (objc_slide.c / xrel; the `c95151a` "extend xrel lifting
to MODERN binaries' i386 4-byte-field DATA external-relocs" follow-up):

- **f03 (catch Base& for a thrown Derived)** reaches the catch correctly, then
  SIGSEGVs on the virtual call `b.who()`: the thrown object's vtable
  **first virtual-function slot reads 0 at runtime** (the slide pass zeroes it
  while it correctly slides the adjacent typeinfo/dtor slots — verified in lldb:
  `who`@vtable+8 = 0, dtor@+12/+16 slid). General virtual-dispatch bug, not EH.

- **Multi-catch type discrimination** (e.g. `throw 42` under
  `catch(double)` then `catch(int)`) picks the WRONG handler: the RTTI **TType
  typeinfo `__got` references read 0**, so `type_caught_by` sees `catch_ti==0`
  and treats every clause as `catch(...)`, matching the first action in the
  chain. The personality already returns the right selector when `catch_ti`
  resolves (confirmed: it selects ttype_index 2 = `int` correctly *when reached*);
  it is starved of a non-zero `catch_ti`. The i386-layout RTTI public-base walk
  (shared with `__dynamic_cast`, cxx_shim.c `ti_kind_of`/`do_find_public_src`)
  is the remaining EH-side piece, but it can only be exercised once `catch_ti`
  resolves — i.e. after the internal-pointer gap above is fixed.

Both manifest only for ASLR-slid *modern* binaries; *classic* pinned targets
(Civ IV / Halo, slide 0, typeinfo xrel already lifted) should not hit them.
