/*
 * 24_libm_float_return — libm float/double return-value ABI bridge.
 *
 * i386 cdecl returns float/double on the x87 stack (st0); x86_64 SysV returns
 * them in xmm0. abigen's generated C shims marshalled args correctly but never
 * converted the RETURN: a float/double-returning shim left the result in xmm0,
 * so the i386 caller read garbage from st0. Worse, the libm family
 * (floorf/ceilf/sqrt/floor/...) had no shim at all and bound to native
 * libSystem, where the native 8-byte `ret` over-pops the i386 4-byte return
 * push -> fused/bogus PC (the iPhoto -[MWLoadingView _updateProgressOrigin] ->
 * floorf crash).
 *
 * FIX (universal): (a) libsystem_m + <math.h> feed abigen so the libm family
 * gets shims; (b) abigen converts a CXType_Float/CXType_Double return xmm0 ->
 * st0 (movss/movsd [scratch],xmm0; fld dword/qword [scratch]).
 *
 * `volatile` inputs defeat constant-folding; the Makefile compiles this with
 * -fno-builtin so floorf/ceilf/sqrt/floor are real library calls (clang would
 * otherwise inline them as SSE intrinsics). Validation via exit code (float
 * printf varargs are a separate known gap). With correct float/double arg
 * marshalling AND the xmm0->st0 return conversion:
 *   floorf(3.7)=3  ceilf(3.2)=4  sqrt(16.0)=4  floor(9.8)=9
 *   exit = 3*1 + 4*4 + 4*9 + 9*13 = 3 + 16 + 36 + 117 = 172
 * A wrong arg or an un-converted return changes the code; a missing libm shim
 * crashes outright (native 8-byte-ret over-pop).
 */
extern void   exit(int status);
extern float  floorf(float);
extern float  ceilf(float);
extern double sqrt(double);
extern double floor(double);

int main(void) {
   volatile float  f1 = 3.7f;
   volatile float  f2 = 3.2f;
   volatile double d1 = 16.0;
   volatile double d2 = 9.8;

   int a = (int)floorf(f1);   /* float  arg + float  return -> 3 */
   int b = (int)ceilf(f2);    /* float  arg + float  return -> 4 */
   int c = (int)sqrt(d1);     /* double arg + double return -> 4 */
   int d = (int)floor(d2);    /* double arg + double return -> 9 */

   exit(a * 1 + b * 4 + c * 9 + d * 13);   /* 172 */
   return 0;
}
