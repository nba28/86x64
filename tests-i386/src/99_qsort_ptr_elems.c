/* 99_qsort_ptr_elems — an i386 qsort comparator must receive pointers to real
 * elements. Native qsort compares against temporary copies on its own (high)
 * stack; the generic callback bridge bounced those without knowing the element
 * width, so a comparator that dereferences the element (PvZ: **a) read garbage.
 * ON: sorted -> exit 42. OFF (M64_NO_QSORT_SHIM=1 at RUN time): crash or
 * unsorted. */
#include <stdlib.h>

static int cmp(const void *a, const void *b)
{
   const int x = **(int * const *)a, y = **(int * const *)b;
   return x < y ? -1 : x > y;
}

int main(void)
{
   /* heap arrays and many equal keys, like PvZ's (n=217): the shapes that make
    * native qsort compare against its own stack temporaries */
   enum { N = 217 };
   int *vals = malloc(N * sizeof *vals);
   int **ptrs = malloc(N * sizeof *ptrs);
   unsigned s = 12345;
   for (int i = 0; i < N; ++i) { s = s * 1103515245u + 12345u; vals[i] = (int)(s >> 8) % 7; ptrs[i] = &vals[i]; }
   qsort(ptrs, N, sizeof ptrs[0], cmp);
   for (int i = 1; i < N; ++i) { if (*ptrs[i - 1] > *ptrs[i]) { exit(1); } }
   exit(42);
}
