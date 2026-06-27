// f01: throw a scalar (int), catch by value, same frame. The minimal end-to-end
// exception. Exercises __cxa_allocate_exception/__cxa_throw/__cxa_begin_catch/
// __cxa_end_catch + intra-frame landing-pad resume with rax=exc / rdx=selector.
#include <cstdio>
#include <cstdlib>
int main() {
  try {
    printf("before throw\n");
    throw 42;
    printf("after throw (unreachable)\n");
  } catch (int x) {
    printf("caught int %d\n", x);
  }
  printf("after catch\n");
  exit(0);
}
