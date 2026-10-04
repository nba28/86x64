/* objc_send_bench.m — cost of an app->system ObjC send through the forward
 * bridge. Build + translate + run: bash tests-i386/bench/run_objc_send_bench.sh.
 * The same source built natively (x86_64) is the baseline. Not a test: numbers
 * move with power state and core type, compare only same-condition runs. */
#import <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <string.h>

/* A LEGACY (translated) class: native Foundation calls its -compare: through
 * the reverse bridge while sorting. */
@interface BenchItem : NSObject { @public int v; }
- (NSComparisonResult)compare:(BenchItem *)o;
@end
@implementation BenchItem
- (NSComparisonResult)compare:(BenchItem *)o {
   return v < o->v ? NSOrderedAscending : v > o->v ? NSOrderedDescending : NSOrderedSame;
}
@end
static unsigned long g_cmp_calls;

static double now_ns(void) {
   struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
   return t.tv_sec * 1e9 + t.tv_nsec;
}
/* BENCH_N (default 200000) and BENCH_ONLY (a label substring) from the
 * environment: argv does not reach a translated program. */
static int N = 200000;
static const char *only;
#define BENCH(label, body) do {                                   \
      if (only && !strstr(label, only)) break;                       \
      double t0 = now_ns();                                          \
      for (int i = 0; i < N; ++i) { body; }                          \
      printf("%-28s %8.1f ns/send\n", label, (now_ns() - t0) / N);   \
   } while (0)

int main(void) {
   if (getenv("BENCH_N")) { N = atoi(getenv("BENCH_N")); }
   only = getenv("BENCH_ONLY");
   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
   NSString *s = @"hello, world";
   NSArray *a = [NSArray arrayWithObjects:@"a", @"b", @"c", nil];
   NSMutableDictionary *d = [NSMutableDictionary dictionary];
   volatile unsigned long sink = 0;
   BENCH("[str length]",               sink += [s length]);
   BENCH("[arr objectAtIndex:]",       sink += (unsigned long)[a objectAtIndex:i % 3]);
   BENCH("[str hasPrefix:]",           sink += [s hasPrefix:@"hel"]);
   BENCH("[dict setObject:forKey:]",   [d setObject:s forKey:@"k"]);
   NSAutoreleasePool *p2 = [[NSAutoreleasePool alloc] init];
   BENCH("[NSNumber numberWithDouble:]", sink += (unsigned long)[NSNumber numberWithDouble:i * 0.5]);
   [p2 release];
   /* plain C calls through the abigen bridge (a togl GL call is this shape) */
   long (*volatile p_labs)(long) = labs;
   BENCH("C labs()",                   sink += p_labs(i));
   BENCH("C CFArrayGetCount()",        sink += CFArrayGetCount((CFArrayRef)a));
   {
      /* heap buffers, the common case (a static buffer lies inside an image
       * and still takes the CFSTR probe in the void* unwrap) */
      char *src = malloc(64), *dst = malloc(64);
      memset(src, 1, 64);
      void *(*volatile p_memcpy)(void *, const void *, size_t) = memcpy;
      BENCH("C memcpy 64B",           p_memcpy(dst, src, 64));
      free(src); free(dst);
   }
   {
      /* pthread shims: i386 mutex/key layouts differ from native */
      static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
      static pthread_key_t key;
      pthread_key_create(&key, NULL);
      pthread_setspecific(key, &key);
      BENCH("pthread_mutex lock+unlock",  { pthread_mutex_lock(&mu); pthread_mutex_unlock(&mu); });
      BENCH("pthread_getspecific",        sink += (unsigned long)pthread_getspecific(key));
      BENCH("pthread_self",               sink += (unsigned long)pthread_self());
   }
   /* the low-4GB heap shim: a fresh heap, then after a mixed-size free list */
   BENCH("malloc+free 32B",            { void *volatile q = malloc(32); free(q); });
   {
      enum { K = 4096 };
      static void *keep[K];
      for (int j = 0; j < K; ++j) { keep[j] = malloc(16 + (j * 37) % 4000); }
      for (int j = 0; j < K; j += 2) { free(keep[j]); }   /* holes of every size */
      BENCH("malloc+free 3000B (holes)", { void *volatile q = malloc(3000); free(q); });
      for (int j = 1; j < K; j += 2) { free(keep[j]); }
   }
   if (!only || strstr("reverse compare:", only)) {
      /* native -> app: count the compares a sort makes, time per compare */
      NSAutoreleasePool *p3 = [[NSAutoreleasePool alloc] init];
      NSMutableArray *items = [NSMutableArray array];
      for (int i = 0; i < 4096; ++i) {
         BenchItem *b = [[BenchItem alloc] init]; b->v = (int)((i * 2654435761u) >> 20);
         [items addObject:b]; [b release];
      }
      const int rounds = N / 50000 > 0 ? N / 50000 : 1;
      double t0 = now_ns(), calls = 0;
      for (int r = 0; r < rounds; ++r) {
         NSArray *sorted = [items sortedArrayUsingSelector:@selector(compare:)];
         calls += 4096.0 * 12;   /* ~n log2 n compares */
         sink += [sorted count];
      }
      printf("%-28s %8.1f ns/call (approx)\n", "reverse compare:", (now_ns() - t0) / calls);
      [p3 release];
   }
   printf("sink %lu\n", (unsigned long)(sink & 1));
   [pool release];
   exit(0);
}
