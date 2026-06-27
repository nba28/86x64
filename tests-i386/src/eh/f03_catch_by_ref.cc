// f03: throw Derived, catch Base& (catch BY REFERENCE across an inheritance
// edge). Beyond exact-type matching: needs type_info::__do_catch hierarchy walk
// + the base-subobject pointer adjustment (coordinated with the RTTI agent's
// i386 __dynamic_cast / typeinfo work). Wall-2 + RTTI.
#include <cstdio>
#include <unistd.h>
struct Base { virtual const char* who() const { return "Base"; } virtual ~Base() {} };
struct Derived : Base { const char* who() const { return "Derived"; } };
int main() {
  try {
    throw Derived();
  } catch (Base& b) {
    printf("caught base-ref who=%s\n", b.who());
  }
  printf("done\n");
  fflush(stdout);
  _exit(0);
}
