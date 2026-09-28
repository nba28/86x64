/* 99_qsort_ptr_elems — an i386 qsort comparator must only ever see pointers
 * into the caller's array.
 *
 * libc's qsort is an introsort: on a bad pivot sequence it falls back to
 * heapsort, which compares against TEMPORARY elements it mallocs itself —
 * above 4 GB. The callback bridge passes comparator args as 32-bit words, so
 * those temporaries arrived truncated (0x6000_01aeb870 -> 0x01aeb870) and a
 * comparator that dereferences its element (PvZ's sound sort, **a) faulted.
 * KEYS below is McIlroy's antiqsort adversary input for n=100, which forces
 * that fallback (natively: 45 comparator args outside the array).
 * ON: sorted -> exit 42. OFF (M64_NO_QSORT_SHIM=1 at RUN time): crash or
 * unsorted. */
#include <stdlib.h>

static const int KEYS[] = {0,28,79,67,6,15,73,72,10,33,96,98,1,31,16,25,7,39,52,11,21,71,90,100,17,26,36,89,97,93,22,70,87,95,27,37,46,69,32,41,84,54,91,38,56,47,42,68,59,88,2,48,8,85,12,62,57,18,77,82,23,58,3,53,9,13,94,19,92,43,24,29,49,34,86,40,44,83,50,81,80,55,78,60,76,75,74,4,14,20,30,35,45,51,61,66,65,64,63,5};
enum { N = sizeof KEYS / sizeof KEYS[0] };

static int cmp(const void *a, const void *b)
{
   const int x = **(int * const *)a, y = **(int * const *)b;
   return x < y ? -1 : x > y;
}

int main(void)
{
   int *vals = malloc(N * sizeof *vals);
   int **ptrs = malloc(N * sizeof *ptrs);
   for (int i = 0; i < N; ++i) { vals[i] = KEYS[i]; ptrs[i] = &vals[i]; }
   qsort(ptrs, N, sizeof ptrs[0], cmp);
   for (int i = 1; i < N; ++i) { if (*ptrs[i - 1] > *ptrs[i]) { exit(1); } }
   exit(42);
}
