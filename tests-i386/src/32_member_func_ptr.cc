// 31_member_func_ptr — guard even-alignment of translated function entries
// (function-alignment fix in Section::Build / SectionBlob::func_entry).
//
// The Itanium C++ ABI encodes a pointer-to-member-function as a pair
// {ptr, adj}: when ptr's low bit is 0 it is a direct (non-virtual) function
// address; when the low bit is 1 it is (vtable_byte_offset + 1), i.e. a
// virtual call.  This tagging assumes every function entry is at least 2-byte
// aligned, which real i386/x86_64 toolchains guarantee.  The translator,
// however, used to pack translated functions byte-tight, so a non-virtual
// member function frequently landed at an ODD address: &Class::method then had
// its low bit set, the dispatch sequence misread it as a virtual thunk, and
// dereferenced a bogus vtable slot -> SIGSEGV (originally f09_member_func_ptr).
//
// This test takes the address of several NON-VIRTUAL member functions (whose
// translated entries must therefore be even) and calls through them.  A single
// odd-addressed entry makes one of the calls jump through garbage and the test
// crashes or prints the wrong value.  It is deliberately self-contained — no
// virtual functions, no RTTI, no exceptions (-fno-rtti -fno-exceptions) — so it
// links against libSystem only and needs no libstdc++ at load.

#include <cstdio>
#include <cstdlib>

struct Math {
   int base;
   Math(int b) : base(b) {}
   /* Several entries of differing sizes so at least one lands at an odd
    * address under the old byte-tight layout. */
   int add(int x) { return base + x; }
   int sub(int x) { return base - x; }
   int mul(int x) { return base * x; }
   int neg() { return -base; }
   int madd(int x, int y) { return base + x + y; }
};

typedef int (Math::*Fn1)(int);
typedef int (Math::*Fn0)();
typedef int (Math::*Fn2)(int, int);

/* Call through a pmf from a separate function so the call cannot be
 * devirtualised/inlined away. */
static int apply1(Math *m, Fn1 f, int x) { return (m->*f)(x); }

int main() {
   Math m(10);

   Fn1 padd = &Math::add;
   Fn1 psub = &Math::sub;
   Fn1 pmul = &Math::mul;
   Fn0 pneg = &Math::neg;
   Fn2 pmadd = &Math::madd;

   int a = (m.*padd)(3);     /* 13 */
   int s = (m.*psub)(4);     /* 6  */
   int u = (m.*pmul)(5);     /* 50 */
   int n = (m.*pneg)();      /* -10 */
   int d = (m.*pmadd)(1, 2); /* 13 */
   int v = apply1(&m, padd, 7); /* 17 */

   printf("add=%d\n", a);
   printf("sub=%d\n", s);
   printf("mul=%d\n", u);
   printf("neg=%d\n", n);
   printf("madd=%d\n", d);
   printf("apply=%d\n", v);

   if (a != 13 || s != 6 || u != 50 || n != -10 || d != 13 || v != 17) {
      printf("FAIL\n");
      exit(1);
   }
   printf("ok\n");
   /* exit() (not return): the wrapper enters _main via jmp (no return addr). */
   exit(0);
}
