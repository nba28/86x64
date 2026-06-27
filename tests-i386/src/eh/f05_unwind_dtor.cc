// f05: UNWIND THROUGH A DESTRUCTOR. f() has a local Guard; throwing past it must
// run Guard::~Guard via f()'s CLEANUP landing pad (reached by the unwinder, which
// then continues via _Unwind_Resume to main's catch). Exercises cleanup landing
// pads + cross-frame resume.
#include <cstdio>
#include <unistd.h>
struct Guard { ~Guard() { printf("guard dtor\n"); fflush(stdout); } };
static void f() {
  Guard g;
  throw 5;
}
int main() {
  try {
    f();
  } catch (int x) {
    printf("caught %d\n", x);
  }
  printf("done\n");
  fflush(stdout);
  _exit(0);
}
