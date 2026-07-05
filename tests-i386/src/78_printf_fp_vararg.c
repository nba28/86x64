/* 78_printf_fp_vararg — FLOATING-POINT varargs through the inline-variadic printf
 * family. The register trampoline (vararg-conv-t.asm) only distributed converted
 * args into GP registers rdi..r9 with al=0, never placing a double into an xmm
 * register, so `%g`/`%f` read garbage (0 / a denormal); and the fortified
 * __snprintf_chk had NO shim at all -> native i386-cdecl over-pop. The family is
 * now routed through the FP-capable va_list path (build_native_va_list -> native
 * v* / __vsnprintf_chk). Exercises __snprintf_chk (a double) and plain printf
 * (double + int interleaved). exit(0), stdout compared. */
#include <stdio.h>
#include <string.h>
extern void exit(int status);
int main(void){
   char a[64], b[64];
   snprintf(a, sizeof a, "%.2g", 512.0);   /* double via fortified __snprintf_chk */
   snprintf(b, sizeof b, "%d", 512);        /* int   via fortified __snprintf_chk */
   printf("A=[%s] B=[%s]\n", a, b);
   printf("D=%.2g I=%d\n", 512.0, 512);     /* double + int via plain printf */
   exit(0);
}
