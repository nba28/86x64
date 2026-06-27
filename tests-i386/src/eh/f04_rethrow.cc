// f04: catch in an inner frame, RETHROW, catch in the outer frame. Exercises
// __cxa_rethrow + CROSS-FRAME unwinding (inner -> main) and the caught-exception
// stack.
#include <cstdio>
#include <unistd.h>
static void inner() {
  try {
    throw 99;
  } catch (int) {
    printf("inner caught, rethrow\n");
    throw;
  }
}
int main() {
  try {
    inner();
  } catch (int x) {
    printf("outer caught %d\n", x);
  }
  printf("done\n");
  fflush(stdout);
  _exit(0);
}
