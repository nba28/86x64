// f01: throw a scalar (int), catch by value, same frame. The catch verifies the
// value; output via printf + exit(0) (exit flushes stdio cleanly — unlike
// fflush(stdout), which hits the separate ___stdoutp truncation gap).
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
