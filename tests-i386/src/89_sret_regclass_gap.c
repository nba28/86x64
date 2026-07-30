/*
 * 89_sret_regclass_gap — abigen emits NO hidden-sret slot for a by-value struct
 * return that is MEMORY on i386 but REGISTER on x86_64.
 *
 * THE GAP (measured with ABIGEN_SRET_GAP_TRACE=1: 5 functions in the modern
 * pass). i386 cdecl returns any struct larger than the eax:edx pair through a
 * hidden buffer pointer passed as the implicit FIRST stack arg — and, verified
 * from real `clang -arch i386 -O1 -S` codegen, the CALLEE POPS IT (`retl $4`).
 * x86_64 SysV returns any aggregate <= 16 bytes in REGISTERS instead. All three
 * of abigen's struct-return classifiers decline that combination:
 *   fp_sret_return()       needs homogeneous-FP AND x86_64 > 16
 *   fp_reg_return()        needs homogeneous-FP AND a real i386 size <= 8
 *   int_reg_struct_return() rejects i386 sizeof > 8, and again on any SSE eightbyte
 * so it falls through to a path that assumes NO hidden pointer. Two consequences,
 * and this test checks BOTH because either alone can hide the other:
 *
 *   (1) every declared argument is read 4 bytes LOW (the hidden pointer occupies
 *       the first i386 arg slot that abigen never accounted for), and the return
 *       conversion is wrong;
 *   (2) the bridge is the i386 CALLEE, so its epilogue must pop 8 (return address
 *       + hidden pointer). Popping 4 leaves the caller's stack 4 bytes off, which
 *       corrupts FAR from the call site — invisible to a value-only check.
 *
 * Shapes covered (the gap spans three distinct x86_64 return classes, which is
 * why one fix-up instruction cannot serve):
 *   lldiv_t {long long, long long}   16B — all-INTEGER, x86_64 rax:rdx
 *   CFGregorianDate {i32,i8,i8,i8,i8,double} 16B — MIXED, x86_64 rax + xmm0
 * (The homogeneous-FP member of the family, ___sincos_stret, is a libm internal
 * with no stable i386 declaration and is deliberately not called here.)
 *
 * CFAbsoluteTime 0.0 is exactly 2001-01-01 00:00:00 GMT, so with a NULL time
 * zone every field is a fixed constant — no clock dependence.
 *
 * Validation by EXIT CODE: six independent one-bit checks, all-correct == 63.
 */
extern void exit(int status);

typedef long long          SInt64;
typedef int                SInt32;
typedef signed char        SInt8;
typedef double             CFAbsoluteTime;
typedef const void        *CFTimeZoneRef;

typedef struct { long long quot; long long rem; } lldiv_t;
extern lldiv_t lldiv(long long, long long);

/* Real CoreFoundation layout: 4 + 1 + 1 + 1 + 1 (+ 0 pad) + 8 = 16 bytes,
 * offsetof(second) == 8 (measured with clang, not recalled). */
typedef struct {
   SInt32 year;
   SInt8  month;
   SInt8  day;
   SInt8  hour;
   SInt8  minute;
   double second;
} CFGregorianDate;

extern CFGregorianDate CFAbsoluteTimeGetGregorianDate(CFAbsoluteTime, CFTimeZoneRef);

int main(void) {
   /* ---- shape A: all-INTEGER 16-byte return (x86_64 rax:rdx) ---------------- */
   lldiv_t d = lldiv(17LL, 5LL);
   int t1 = (d.quot == 3LL) ? 1 : 0;
   int t2 = (d.rem  == 2LL) ? 1 : 0;

   /* ---- shape B: MIXED INTEGER+SSE 16-byte return (x86_64 rax + xmm0) ------
    * If the hidden sret pointer is unaccounted for, the declared args are read
    * 4 bytes low: the bridge picks up the HIGH half of the `at` double as its
    * first argument. That is exactly the live Civ IV crash. */
   CFGregorianDate g = CFAbsoluteTimeGetGregorianDate(0.0, 0);
   int t3 = (g.year == 2001 && g.month == 1 && g.day == 1) ? 1 : 0;
   int t4 = (g.hour == 0 && g.minute == 0 && g.second == 0.0) ? 1 : 0;

   /* ---- the STACK-BALANCE assertion ---------------------------------------
    * A value check alone can pass while the epilogue pops the wrong amount.
    * Call each in a loop and compare esp across it: a 4-byte-per-call error
    * accumulates to 256 bytes over 64 calls, so this cannot pass by luck.
    * Captured with volatile asm around the loop, never inside it. */
   unsigned esp_before_a, esp_after_a, esp_before_b, esp_after_b;
   volatile long long acc = 0;
   volatile double sec_acc = 0.0;

   __asm__ volatile ("movl %%esp, %k0" : "=r"(esp_before_a));
   for (int i = 0; i < 64; i++) {
      lldiv_t q = lldiv(100LL + i, 7LL);
      acc += q.quot + q.rem;
   }
   __asm__ volatile ("movl %%esp, %k0" : "=r"(esp_after_a));
   int t5 = (esp_before_a == esp_after_a) ? 1 : 0;

   __asm__ volatile ("movl %%esp, %k0" : "=r"(esp_before_b));
   for (int i = 0; i < 64; i++) {
      CFGregorianDate gg = CFAbsoluteTimeGetGregorianDate(0.0, 0);
      sec_acc += gg.second + gg.year;
   }
   __asm__ volatile ("movl %%esp, %k0" : "=r"(esp_after_b));
   int t6 = (esp_before_b == esp_after_b) ? 1 : 0;

   exit(t1*1 + t2*2 + t3*4 + t4*8 + t5*16 + t6*32);   /* all correct == 63 */
   return 0;
}
