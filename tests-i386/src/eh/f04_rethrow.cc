// f04: inner catch, RETHROW, outer catch (cross-frame unwind + caught stack).
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
