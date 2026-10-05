/* 99_stripped_pmf_even — guard: a STRIPPED image's function entries land at
 * even translated addresses (section.cc Parse1 func_entry; Portal 2 libcef
 * quit SIGSEGV rip=0).
 *
 * The Itanium pointer-to-member-function {ptr, adj} reads ptr's low bit as
 * "virtual: ptr-1 is a vtable byte offset". Build even-aligns func_entry blobs,
 * but func_entry came only from symbols, and a stripped image has none for its
 * local functions: half of libcef's entries were odd, so a non-virtual pmf
 * (base::RunnableMethod posted during CEF shutdown) dispatched through
 * vptr+addr-1 and called 0. This fixture dispatches exactly like that over a
 * dozen local functions of varied sizes; `strip -x` (the Makefile rule)
 * removes their symbols. Exit 42 = every call reached its function.
 * Kill switch M64_NO_DEAD_ENTRY_EVEN=1 (the OFF arm) reproduces the bug. */
#include <stdint.h>
#include <stdlib.h>

struct obj { const void *const *vptr; int v; };
typedef int (*fn)(struct obj *);
struct pmf { uintptr_t ptr; intptr_t adj; };

#define H(n, ...) static __attribute__((noinline)) int h##n(struct obj *o) { __VA_ARGS__ }
H(0, return o->v;)
H(1, return o->v + 1;)
H(2, return o->v * 3 - 1;)
H(3, int r = o->v; for (int i = 0; i < 3; ++i) { r += i; } return r;)
H(4, return o->v ^ 0x40000;)
H(5, return (o->v << 2) + 0x1234567;)
H(6, volatile int t = o->v; return t + 6;)
H(7, return o->v > 0 ? 7 : -7;)
H(8, return o->v - 0x100;)
H(9, int a = o->v, b = a * a; return a + b + 9;)
H(10, return (o->v & 0xff) | 0x1000;)
H(11, return -o->v;)

static __attribute__((noinline)) int invoke(struct obj *o, struct pmf f) {
   char *t = (char *)o + f.adj;
   fn p = (f.ptr & 1) ? *(const fn *)((const char *)*(const void *const **)t + f.ptr - 1) : (fn)f.ptr;
   return p((struct obj *)t);
}

/* Each pmf is built in code (PIC `leal hN-anchor`), as libcef builds them:
 * a constant table would land in reloc-less data, a different gate. */
#define T(n) ok += invoke(&o, (struct pmf){ (uintptr_t)h##n, 0 }) == h##n(&o);
int main(void) {
   static const void *const vtable[1] = { 0 };
   struct obj o = { vtable, 5 };
   int ok = 0;
   T(0) T(1) T(2) T(3) T(4) T(5) T(6) T(7) T(8) T(9) T(10) T(11)
   exit(ok == 12 ? 42 : 1);
}
