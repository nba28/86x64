/* Does i386 Darwin make the CALLEE pop the hidden sret pointer (ret $4)?
   G is 16 bytes, mixed INTEGER+SSE — exactly the CFGregorianDate shape. */
typedef struct { int year; signed char mo, d, h, mi; double second; } G;
extern G f(double at, const void *tz);
G call_it(double at, const void *tz) { return f(at, tz); }
G be_callee(double at, const void *tz) { G g; g.year = (int)at; g.mo = g.d = g.h = g.mi = 1; g.second = at; (void)tz; return g; }
