// f03: throw Derived, catch Base& (inheritance) — needs the RTTI type_info
// hierarchy walk (coordinated with the __dynamic_cast work). Wall-2 + RTTI.
#include <cstdio>
#include <cstdlib>
struct Base { virtual const char* who() const { return "Base"; } virtual ~Base() {} };
struct Derived : Base { const char* who() const { return "Derived"; } };
int main() {
  try { throw Derived(); }
  catch (Base& b) { printf("caught base-ref who=%s\n", b.who()); }
  printf("done\n");
  exit(0);
}
