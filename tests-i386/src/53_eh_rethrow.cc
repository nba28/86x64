// 53_eh_rethrow: inner catch, RETHROW (`throw;`), outer catch — cross-frame
// unwind through the caught-exception stack. Mirrors src/eh/f04_rethrow.cc.
#include <cstdio>
#include <cstdlib>
static void inner() {
  try { throw 99; }
  catch (int) { printf("inner caught, rethrow\n"); throw; }
}
int main() {
  try { inner(); }
  catch (int x) { printf("outer caught %d\n", x); }
  printf("done\n");
  exit(0);
}
