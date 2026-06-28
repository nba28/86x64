// 54_eh_unwind_dtor: UNWIND THROUGH A DESTRUCTOR — f()'s local Guard dtor must
// run via its cleanup landing pad during the unwind (translated _Unwind_Resume
// continuation), then control reaches main's catch. Mirrors src/eh/f05_unwind_dtor.cc.
#include <cstdio>
#include <cstdlib>
struct Guard { ~Guard() { printf("guard dtor\n"); } };
static void f() { Guard g; throw 5; }
int main() {
  try { f(); }
  catch (int x) { printf("caught %d\n", x); }
  printf("done\n");
  exit(0);
}
