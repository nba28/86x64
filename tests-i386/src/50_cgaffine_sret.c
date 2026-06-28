/*
 * 50_cgaffine_sret — abigen FP-by-value-struct RETURN marshalling (sret), the
 * CGAffineTransform builder family (GAP 2 residual #1, the companion to test 49).
 *
 * A CGAffineTransform is 6 CGFloats. CGFloat is a 4-byte `float` on i386 and an
 * 8-byte `double` on x86_64, so the struct is 24 bytes on i386 and 48 bytes on
 * x86_64 — the SysV MEMORY class on both ABIs, returned through a hidden,
 * caller-allocated pointer:
 *   - i386 cdecl: the caller passes the return-buffer address as the implicit
 *     FIRST stack arg and the callee returns that pointer in eax.
 *   - x86_64 SysV: the caller passes it in rdi (the sret register) and the callee
 *     returns it in rax, having written the 6 doubles to that buffer.
 * abigen previously SKIPPED any FP-struct function with a by-value struct return
 * (CGAffineTransform* and CGRect-returning CG/NS geometry), so these never got a
 * shim. abigen now: reserves a native 48-byte return buffer, points rdi at it,
 * marshals the real declared args (the i386 hidden sret slot shifts them one
 * slot; the first int arg shifts off rdi to rsi), calls native, then NARROWS each
 * x86_64 double back to a 4-byte CGFloat (cvtsd2ss) in the i386 caller's buffer
 * and returns that pointer in eax.
 *
 * This also exercises the companion fix: a SCALAR CGFloat arg (sx/sy/tx/ty/angle)
 * is read as a 4-byte i386 float and widened to a double (cvtss2sd), not read as
 * an 8-byte double from the 4-byte i386 slot (the old scalar-CGFloat bug).
 *
 * Coverage:
 *   - scalar-CGFloat args only        : CGAffineTransformMakeScale / MakeTranslation
 *   - FP-struct (MEMORY) arg + scalars: CGAffineTransformTranslate
 *   - FP-struct (MEMORY) args only    : CGAffineTransformConcat, CGAffineTransformInvert
 * All values are exactly representable in BOTH float and double, so a widening or
 * narrowing bug flips a check (or crashes). CoreGraphics is not in the i386
 * sysroot, so these are undefined dynamic_lookup imports that the pipeline's
 * static-interpose redirects to libabiconv's ___CGAffineTransform* shims, exactly
 * as for a real binary. Validation is by EXIT CODE: five independent checks, each
 * one bit; all-correct == 31.
 */
extern void exit(int status);

typedef float CGFloat;                               /* i386: CGFloat is float */
typedef struct { CGFloat a, b, c, d, tx, ty; } CGAffineTransform;

extern CGAffineTransform CGAffineTransformMakeScale(CGFloat sx, CGFloat sy);
extern CGAffineTransform CGAffineTransformMakeTranslation(CGFloat tx, CGFloat ty);
extern CGAffineTransform CGAffineTransformTranslate(CGAffineTransform t,
                                                    CGFloat tx, CGFloat ty);
extern CGAffineTransform CGAffineTransformConcat(CGAffineTransform t1,
                                                 CGAffineTransform t2);
extern CGAffineTransform CGAffineTransformInvert(CGAffineTransform t);

int main(void) {
   const CGAffineTransform ident = {1, 0, 0, 1, 0, 0};

   /* 1) scalar-CGFloat args, sret return: scale(2,3) -> {2,0,0,3,0,0} */
   CGAffineTransform s = CGAffineTransformMakeScale(2.0f, 3.0f);
   int t1 = (s.a == 2.0f && s.b == 0.0f && s.c == 0.0f &&
             s.d == 3.0f && s.tx == 0.0f && s.ty == 0.0f) ? 1 : 0;

   /* 2) scalar-CGFloat args, sret return: translate(5,7) -> {1,0,0,1,5,7} */
   CGAffineTransform m = CGAffineTransformMakeTranslation(5.0f, 7.0f);
   int t2 = (m.a == 1.0f && m.b == 0.0f && m.c == 0.0f &&
             m.d == 1.0f && m.tx == 5.0f && m.ty == 7.0f) ? 1 : 0;

   /* 3) MEMORY-class struct arg + 2 scalar CGFloats, sret return:
    *    Translate(identity, 5, 7) -> {1,0,0,1,5,7} */
   CGAffineTransform tr = CGAffineTransformTranslate(ident, 5.0f, 7.0f);
   int t3 = (tr.a == 1.0f && tr.b == 0.0f && tr.c == 0.0f &&
             tr.d == 1.0f && tr.tx == 5.0f && tr.ty == 7.0f) ? 1 : 0;

   /* 4) two MEMORY-class struct args, sret return: Concat(A, identity) == A
    *    (identity is the convention-independent neutral element) */
   CGAffineTransform cc = CGAffineTransformConcat(s, ident);
   int t4 = (cc.a == 2.0f && cc.b == 0.0f && cc.c == 0.0f &&
             cc.d == 3.0f && cc.tx == 0.0f && cc.ty == 0.0f) ? 1 : 0;

   /* 5) one MEMORY-class struct arg, sret return: invert(scale(2,4)) ->
    *    scale(0.5, 0.25) = {0.5,0,0,0.25,0,0} (0.5/0.25 exact in float) */
   CGAffineTransform sc = CGAffineTransformMakeScale(2.0f, 4.0f);
   CGAffineTransform inv = CGAffineTransformInvert(sc);
   int t5 = (inv.a == 0.5f && inv.b == 0.0f && inv.c == 0.0f &&
             inv.d == 0.25f && inv.tx == 0.0f && inv.ty == 0.0f) ? 1 : 0;

   exit(t1*1 + t2*2 + t3*4 + t4*8 + t5*16);   /* all correct == 31 */
   return 0;
}
