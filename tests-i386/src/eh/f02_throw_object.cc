// f02: throw a class object, catch BY VALUE. The catch copy-constructs from the
// exception object (adjustedPtr == object). Exercises the same path as f01 plus
// a non-scalar exception object.
#include <cstdio>
#include <cstdlib>
struct E { int code; E(int c) : code(c) {} };
int main() {
  try {
    throw E(7);
  } catch (E e) {
    printf("caught E code=%d\n", e.code);
  }
  printf("done\n");
  exit(0);
}
