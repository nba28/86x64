// 28_weak_bind — guard the LC_DYLD_INFO weak_bind translation (commit 1363242).
//
// C++ weak-coalesced symbols (vtables / typeinfo / template instantiations /
// inline functions) are emitted into the Mach-O weak_bind opcode table so dyld
// can coalesce duplicate definitions across images. Before 1363242 the
// translator passed those opcodes through verbatim, leaving their
// SET_SEGMENT_AND_OFFSET targets pointing at the OLD i386 segment layout — so
// dyld walked bogus offsets and bus-errored at load (the Front Row blocker,
// the FrontRow C++ weak-bind notes). 1363242 translates the table to the x86_64 layout.
//
// This test forces a NON-EMPTY weak_bind table (verify: `otool -l <i386> | grep
// weak_bind` -> weak_bind_size > 0) via a polymorphic class hierarchy (weak_odr
// vtables) plus explicit template instantiations (weak_odr symbols), then
// exercises virtual dispatch so a mis-translated weak_bind table would crash or
// print garbage. It is deliberately self-contained — no virtual destructor, no
// new/delete, no exceptions, no RTTI (-fno-rtti -fno-exceptions) — so it links
// against libSystem only and runs with NO libstdc++ at load (modern macOS ships
// none). See tests-i386/Makefile `cpp` / `sysroot-cpp` targets.

#include <cstdio>
#include <cstdlib>

struct Shape {
   virtual int sides() const { return 0; }
   virtual int area() const { return 0; }
};
struct Square : Shape {
   int s;
   Square(int x) : s(x) {}
   int sides() const override { return 4; }
   int area() const override { return s * s; }
};
struct Triangle : Shape {
   int b, h;
   Triangle(int B, int H) : b(B), h(H) {}
   int sides() const override { return 3; }
   int area() const override { return b * h / 2; }
};

template <class T>
struct Box {
   T v;
   int twice() const { return v.area() * 2; }
   int per() const { return v.sides() * v.area(); }
};

template <class T>
int sum_areas(T *const *a, int n) {
   int t = 0;
   for (int i = 0; i < n; i++) {
      t += a[i]->area();
   }
   return t;
}

/* Explicit instantiations -> weak-coalesced (weak_odr) symbols in weak_bind. */
template struct Box<Square>;
template struct Box<Triangle>;
template int sum_areas<Shape>(Shape *const *, int);

int main() {
   Square sq(5);
   Triangle tr(6, 4);
   Shape *shapes[2] = {&sq, &tr};
   for (int i = 0; i < 2; i++) {
      printf("shape %d sides=%d area=%d\n", i, shapes[i]->sides(), shapes[i]->area());
   }
   Box<Square> bs{Square(3)};
   Box<Triangle> bt{Triangle(4, 4)};
   printf("box sq twice=%d per=%d\n", bs.twice(), bs.per());
   printf("box tr twice=%d per=%d\n", bt.twice(), bt.per());
   printf("sum=%d\n", sum_areas<Shape>(shapes, 2));
   /* exit() (not _exit()) so stdio buffers flush; the wrapper enters _main via
      jmp so a bare `return` has no return address to unwind to. */
   exit(0);
}
