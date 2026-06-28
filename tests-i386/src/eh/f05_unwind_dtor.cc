// f05: UNWIND THROUGH A DESTRUCTOR — f()'s local Guard dtor must run via its
// cleanup landing pad during the unwind, then control reaches main's catch.
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
