// f02: throw a class object, catch BY VALUE (copy-construct from the exception).
#include <cstdio>
#include <cstdlib>
struct E { int code; E(int c) : code(c) {} };
int main() {
  try { throw E(7); }
  catch (E e) { if (e.code != 7) { printf("WRONG\n"); exit(1); } printf("caught E code=%d\n", e.code); }
  printf("done\n");
  exit(0);
}
