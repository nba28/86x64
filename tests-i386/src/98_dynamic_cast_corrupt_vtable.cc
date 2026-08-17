/* 98_dynamic_cast_corrupt_vtable — a dynamic_cast through an UNREADABLE vtable
 * must fail the cast, not fault the process.
 *
 * WHY THIS EXISTS. Halo crashed on roughly 1 launch in 3 with SIGSEGV /
 * KERN_INVALID_ADDRESS at 0x3c290c28 (Halo-2026-08-17-235241.ips). The .ips
 * BACKTRACE was useless — frame #0 resolved to no loaded image and another frame
 * carried imageOffset 0xFFFFFFFFFFFFFFFF — and only resolving `rip` against
 * usedImages identified the site: inside `_ld32s`, called from
 * `shim_dynamic_cast`. That is `ld32s(vtable - 8)` with vtable = 0x3c290c30.
 *
 * shim_dynamic_cast's three ENTRY loads (the object's vtable word, then the
 * vtable_prefix at -8 and -4) were raw dereferences of pointers the TARGET
 * supplies, guarded only against NULL. Any garbage object pointer — a stale one,
 * or an object freed while a cast is in flight, which is what an intermittent
 * 1-in-3 failure looks like — took the whole process down inside our shim.
 *
 * Returning NULL is the CONTRACT, not a workaround: __dynamic_cast may return
 * NULL for a failed cast, a cast through a corrupt vtable is already undefined
 * behaviour in the target, and faulting inside a shim is strictly worse than
 * reporting the failure. It is also what shim_dynamic_cast already does when it
 * cannot recognise a typeinfo ("fail the cast rather than risk a wild read").
 *
 * THE ADDRESS IS UNMAPPED BY CONSTRUCTION. Hard-coding Halo's 0x3c290c30 would
 * make this test depend on that page happening to be unmapped. The fake vtable
 * instead points into PAGE ZERO, which no process ever maps — that is precisely
 * why a NULL dereference faults — so `vtable - 8` is guaranteed unreadable on
 * any machine and any run.
 *
 * ⚠An earlier draft mmap'd a page, munmap'd it, and used its address. That was
 * WRONG on two counts and is worth recording: `(uint32_t)(uintptr_t)page`
 * TRUNCATES mmap's 64-bit result, so the address handed to the shim was not the
 * page that had been unmapped — it came out as 0x10000000, the translated
 * image's own base — and the process then died returning into it. The test
 * appeared to fail while the fix was working correctly. Do not reach for mmap
 * when a constant will do.
 *
 * ARM 2 IS THE POINT. A guard that only proves "NULL is returned" could pass by
 * making every cast fail, so a REAL cast must still succeed in the same run.
 *
 * A/B: M64_NO_RTTI_SAFE_READ=1 restores the raw loads; that arm SEGFAULTS. See
 * dynamic_cast_corrupt_vtable_test.sh.
 */
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <typeinfo>


struct Base    { virtual ~Base() {} virtual void f() {} };
struct Derived : Base { void f() override {} };

extern "C" void *__dynamic_cast(const void *sub, const void *src,
                                const void *dst, long src2dst);

int main(void) {
   /* Line-buffer stdout: the OFF arm dies mid-run, and block buffering would
    * swallow everything printed before the fault, making an A/B unreadable. */
   setvbuf(stdout, 0, _IOLBF, 0);

   /* A non-NULL "vtable" whose prefix reads (-8, -4) land in PAGE ZERO, which
    * is never mapped. Non-NULL so the existing `if (!vtable)` path cannot claim
    * the credit — this must be caught by the READ failing, not by a null test. */
   uint32_t fake_obj[1];
   fake_obj[0] = 0x40;

   void *r = __dynamic_cast(fake_obj, &typeid(Base), &typeid(Derived), -2);
   printf("corrupt-vtable  %s\n", r == 0 ? "ok" : "FAIL(non-null)");

   /* Control: a genuine cast must STILL work, so this guard cannot be satisfied
    * by a change that simply fails every cast. */
   Derived d;
   Base *b = &d;
   Derived *dd = dynamic_cast<Derived *>(b);
   printf("real-cast       %s\n", dd == &d ? "ok" : "FAIL");

   /* And a cast that must legitimately fail still returns NULL. */
   Base plain;
   Derived *nd = dynamic_cast<Derived *>(&plain);
   printf("must-fail       %s\n", nd == 0 ? "ok" : "FAIL(non-null)");

   /* ⚠exit(), NOT `return`: the 86x64.sh wrapper enters _main via `jmp`, so
    * there is NO return frame — returning from main jumps to garbage (it
    * surfaces as EXC_BAD_ACCESS at address 0x1) and the test scores 139.
    * Same reason 97_dynamic_cast_crosscast ends this way. */
   exit(0);
}
