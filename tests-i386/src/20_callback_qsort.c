/* 20_callback_qsort.c — generic fn-ptr callback bridging (22nd blocker).
 *
 * qsort's abigen shim used to hand the native qsort a pointer to an
 * uninitialized stack temp as the comparator; the first compare jumped into
 * stale stack bytes. With the callback bridge, the comparator argument is
 * bound to a native trampoline that re-enters this i386 function through
 * _86x64_call_i386. Also exercises atexit (zero-arg void callback fired by
 * the native runtime at exit, after main's i386 frame is gone). */

/* forward decls instead of includes: i386-sysroot has no C headers */
typedef unsigned long size_t;
extern int puts(const char *s);
extern int printf(const char *fmt, ...);
extern void qsort(void *base, size_t nmemb, size_t size,
                  int (*compar)(const void *, const void *));
extern int atexit(void (*func)(void));
extern void exit(int status);

static int ncalls;

static int cmp_int(const void *a, const void *b) {
   ++ncalls;
   int x = *(const int *)a;
   int y = *(const int *)b;
   return (x > y) - (x < y);
}

static void byebye(void) {
   puts("atexit callback ran");
}

int main(void) {
   int v[8] = { 42, 7, 99, -3, 0, 18, 7, 64 };
   int i;

   atexit(byebye);

   qsort(v, 8, sizeof v[0], cmp_int);

   for (i = 0; i < 8; ++i) {
      printf("%d\n", v[i]);
   }
   if (ncalls > 0) {
      puts("comparator called");
   }

   exit(0);
}
