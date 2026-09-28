/* objc_send_bench.m — cost of an app->system ObjC send through the forward
 * bridge. Build + translate + run: bash tests-i386/bench/run_objc_send_bench.sh.
 * The same source built natively (x86_64) is the baseline. Not a test: numbers
 * move with power state and core type, compare only same-condition runs. */
#import <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

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
   printf("sink %lu\n", (unsigned long)(sink & 1));
   [pool release];
   exit(0);
}
