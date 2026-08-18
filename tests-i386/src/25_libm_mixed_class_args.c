/* 25_libm_mixed_class_args — libm bridges whose arguments span BOTH x86_64
 * argument classes, and one that writes through an INTEGER OUT-PARAM.
 *
 * WHY THESE TWO SPECIFICALLY. 24_libm_float_return already covers the simple
 * shapes — floorf/ceilf/sqrt/floor, every argument a float or double, one
 * class, no out-params — and it covers the xmm0->st0 RETURN conversion. What it
 * cannot see is argument CLASS ASSIGNMENT, because when every argument is an
 * SSE argument there is no assignment to get wrong.
 *
 *   double frexp(double value, int *exp);   SSE + INTEGER, and *exp is WRITTEN
 *   double ldexp(double x,     int  n);     SSE + INTEGER
 *
 * On i386 both are a flat little-endian stack image — 8 bytes of double then 4
 * bytes of int/pointer — and the callee returns in st0. On x86_64 SysV they
 * split across register FILES: value -> xmm0, exp/n -> rdi/edi, return in xmm0.
 * A bridge that walks the i386 stack and assigns registers POSITIONALLY rather
 * than BY CLASS puts the double where the integer belongs (or consumes an SSE
 * slot for an integer), and every result is then wrong.
 *
 * ★frexp is the sharpest test in the family because it is the only libm entry
 * here that WRITES BACK through a pointer. A bridge can get the return value
 * right and still never propagate the out-param, and an out-param that is
 * silently not written is invisible to any test that only checks return values.
 * That is why `e` is seeded with a sentinel rather than 0: an untouched
 * out-param must be distinguishable from one legitimately written as 0.
 *
 * WHY IT MATTERS BEYOND libm (Halo CE #46): Halo statically links
 * "Xiph.Org libVorbis I 20020717" and decodes its music from Ogg Vorbis, while
 * its sound effects are uncompressed. Vorbis's float<->fixed conversion leans on
 * ldexp/frexp. Measured: the SFX ring carries audio (whole-buffer peaks 1634 ..
 * 18602) while every one of 1508 samples of the 44100 Hz music ring peaked at
 * EXACTLY 0 with the buffer layout confirmed — a producer writing pure silence,
 * which is what a broken float scaling primitive would produce.
 *
 * ⚠This fixture does NOT assert that Halo's silence IS this bug. It closes a
 * real, independent coverage gap in the ABI bridge, and it is cheap enough to
 * settle the question without another launch. If it passes, the Vorbis-libm
 * theory is dead and the hunt moves on with one fewer live hypothesis.
 *
 * -fno-builtin (see the Makefile rule) so clang emits real calls instead of
 * constant-folding these at compile time.
 *
 * Validation by exit code AND by printing INTEGERS only — float printf varargs
 * are a separate known gap (see 78_printf_fp_vararg), so printing a double here
 * would test the wrong thing and could fail for an unrelated reason.
 *   0 = all correct
 *   1 = frexp mantissa wrong        2 = frexp OUT-PARAM wrong  <- the ABI case
 *   3 = ldexp wrong                 4 = pow wrong
 *   5 = exp/log wrong               6 = sin/cos/atan wrong
 *   7 = rint/floor wrong
 */
extern int    printf(const char *, ...);
extern void   exit(int status);
extern double frexp(double, int *);
extern double ldexp(double, int);
extern double pow(double, double);
extern double exp(double);
extern double log(double);
extern double sin(double);
extern double cos(double);
extern double atan(double);
extern double rint(double);
extern double floor(double);

/* Compare within a tolerance, in integer space: scale by 1000 and round. */
static int near(double got, double want) {
   double d = got - want;
   if (d < 0) { d = -d; }
   return d < 0.0005;
}

int main(void) {
   /* --- frexp: 12.0 = 0.75 * 2^4 ------------------------------------------ */
   volatile double v = 12.0;
   int e = -12345;                      /* sentinel: NOT a legal frexp result */
   double m = frexp(v, &e);
   printf("frexp mantissa*1000=%d exp=%d\n", (int)(m * 1000.0), e);
   if (!near(m, 0.75)) { printf("FAIL frexp mantissa\n"); exit(1); }
   if (e != 4) {
      printf("FAIL frexp out-param (%s)\n",
             e == -12345 ? "NEVER WRITTEN — the int* argument did not survive"
                         : "written but wrong");
      exit(2);
   }

   /* --- ldexp: 0.75 * 2^4 = 12.0 ------------------------------------------ */
   volatile double mm = 0.75;
   volatile int    ee = 4;
   double back = ldexp(mm, ee);
   printf("ldexp=%d\n", (int)back);
   if (!near(back, 12.0)) { printf("FAIL ldexp\n"); exit(3); }

   /* --- pow: two SSE args, the control for "same class" ------------------- */
   volatile double b = 2.0, p = 10.0;
   double pw = pow(b, p);
   printf("pow=%d\n", (int)pw);
   if (!near(pw, 1024.0)) { printf("FAIL pow\n"); exit(4); }

   volatile double one = 1.0, zero = 0.0;
   printf("exp0=%d log1=%d\n", (int)exp(zero), (int)log(one));
   if (!near(exp(zero), 1.0) || !near(log(one), 0.0)) { printf("FAIL exp/log\n"); exit(5); }

   printf("sin0=%d cos0=%d atan0=%d\n",
          (int)(sin(zero) * 1000.0), (int)(cos(zero) * 1000.0),
          (int)(atan(zero) * 1000.0));
   if (!near(sin(zero), 0.0) || !near(cos(zero), 1.0) || !near(atan(zero), 0.0)) {
      printf("FAIL sin/cos/atan\n"); exit(6);
   }

   volatile double r = 2.5, f = 9.8;
   printf("rint=%d floor=%d\n", (int)rint(r), (int)floor(f));
   if (!near(rint(r), 2.0) || !near(floor(f), 9.0)) { printf("FAIL rint/floor\n"); exit(7); }

   printf("all libm mixed-class args OK\n");
   exit(0);   /* the 86x64.sh wrapper enters _main via jmp: no return frame */
}
