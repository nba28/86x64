// 69_cpp_ios_base_init.cc — guard for the std::ios_base::Init over-pop fix
// (cxx_shim.c shim_ios_base_Init_ctor/dtor + the ____ZNSt8ios_base4Init* MTSHIMs).
//
// A per-translation-unit `static std::ios_base::Init __ioinit;` (what <iostream>
// injects) calls std::ios_base::Init::Init() = mangled __ZNSt8ios_base4InitC1Ev.
// In a translated i386 image that call reaches a __TEXT,__symbol_stub which,
// UNSHIMMED, binds to NATIVE libstdc++ (x86_64) — whose 8-byte `ret` OVER-POPS
// the translated i386 4-byte return frame, fusing two adjacent i386 stack slots
// into a garbage rip and crashing the FIRST static ctor (the iPhoto ctors-ON
// crash; universal to every C++ <iostream> program once run-now static ctors are
// the default). Routed through libabiconv, the MTSHIM trampoline runs the i386
// 4-byte-ret discipline (no over-pop) and still invokes the real native ctor, so
// static init COMPLETES.
//
// We declare std::ios_base::Init explicitly and instantiate a static object so
// the exact __ZNSt8ios_base4InitC1Ev/D1Ev calls are emitted regardless of the
// sysroot's <iostream> internals (it resolves to native -lstdc++). The guard:
// without the shim this process crashes at static-init (no output); with it,
// main runs and a SECOND global ctor sequenced AFTER g_ioinit is proven to have
// run to completion past ios_base::Init.

namespace std {
   class ios_base {
   public:
      class Init { public: Init(); ~Init(); };   // -> __ZNSt8ios_base4InitC1Ev / D1Ev (native libstdc++)
   };
}
static std::ios_base::Init g_ioinit;              // static ctor: the over-pop call site

struct Marker { int v; Marker() : v(4200) {} };
static Marker g_marker;                           // sequenced AFTER g_ioinit

extern "C" int printf(const char *, ...);
extern "C" void exit(int);

int main() {
   // Reaching here means ios_base::Init did NOT over-pop; marker==4242 means the
   // static ctors ran to completion.
   printf("ios_base ok marker=%d\n", g_marker.v + 42);
   exit(0);
}
