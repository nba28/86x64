// 51_eh_throw_int: scalar throw, catch BY VALUE, same frame — the minimal C++
// exception across translated i386->x86_64 frames. Exercises the libabiconv
// translated-frame unwinder (eh_shim.c + eh_tramp.asm) driven by the core
// __DATA,__86x64_pcmap / __86x64_ehlsda maps + the original i386 LSDA.
// Mirrors src/eh/f01_throw_int.cc; wired into `make cpp` as a regression guard.
// exit(0) flushes stdio cleanly (vs fflush, which hits the ___stdoutp gap).
#include <cstdio>
#include <cstdlib>
int main() {
  try {
    printf("before throw\n");
    throw 42;
    printf("UNREACHABLE\n"); exit(2);
  } catch (int x) {
    if (x != 42) { printf("WRONG %d\n", x); exit(1); }
    printf("caught int %d\n", x);
  }
  printf("after catch\n");
  exit(0);
}
