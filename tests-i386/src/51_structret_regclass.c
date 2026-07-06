/*
 * 51_structret_regclass — abigen REGISTER-CLASS struct RETURN paired with a
 * by-value struct ARG (the completion of the C1/C2 byval work).
 *
 * abigen's C1 byval-arg support conservatively SKIPPED any function that both
 * takes a by-value struct arg AND returns a struct, unless the return was the
 * homogeneous-FP >16-byte MEMORY sret (fp_sret). That over-skipped the whole
 * register-class-return family, whose return needs NO hidden sret pointer and
 * NO i386 arg shift:
 *   - fp_reg  : CGPoint/CGSize returned in xmm0:xmm1, narrowed to i386 eax:edx
 *               (CGPointApplyAffineTransform / CGSizeApplyAffineTransform,
 *               CGContextConvertPointToUserSpace, ...).
 *   - int_reg : an all-integer aggregate <=8 bytes on i386 (eax:edx) / <=16 on
 *               x86_64 (rax:rdx) — NSRange/CFRange, and UnsignedWide/AbsoluteTime
 *               (a single x86_64 eightbyte in rax that must be split rax[63:32]
 *               -> edx for the i386 8-byte eax:edx pair).
 *
 * This test exercises the fp_reg case against real CoreGraphics functions that
 * take a CGPoint/CGSize BY VALUE and a CGAffineTransform BY VALUE (MEMORY class)
 * and RETURN a CGPoint/CGSize in registers — a marshalling bug in either the
 * byval arg widening or the fp_reg return narrow flips an exactly-representable
 * result. CoreGraphics is not in the i386 sysroot, so these are undefined
 * dynamic_lookup imports the pipeline's static-interpose redirects to libabiconv's
 * ___CG* shims (see the Makefile rule), exactly as for a real binary.
 *
 * Validation by EXIT CODE (float printf varargs are a separate known gap): six
 * independent checks, each one bit; all-correct == 63.
 */
extern void exit(int status);

typedef float CGFloat;                                 /* i386: CGFloat is float */
typedef struct { CGFloat x; CGFloat y; }            CGPoint;
typedef struct { CGFloat width; CGFloat height; }   CGSize;
typedef struct { CGFloat a, b, c, d, tx, ty; }      CGAffineTransform;

extern CGPoint CGPointApplyAffineTransform(CGPoint point, CGAffineTransform t);
extern CGSize  CGSizeApplyAffineTransform(CGSize size, CGAffineTransform t);

int main(void) {
   const CGAffineTransform ident  = {1, 0, 0, 1, 0, 0};
   const CGAffineTransform scale  = {2, 0, 0, 3, 0, 0};
   const CGAffineTransform xlate  = {1, 0, 0, 1, 5, 7};

   /* fp_reg return + two byval args (CGPoint 8B SSE-class, CGAffineTransform
    * 24B MEMORY-class). Point (2,3): */
   CGPoint p = {2.0f, 3.0f};

   CGPoint pi = CGPointApplyAffineTransform(p, ident);   /* -> (2,3) */
   int t1 = (pi.x == 2.0f && pi.y == 3.0f) ? 1 : 0;

   CGPoint ps = CGPointApplyAffineTransform(p, scale);   /* -> (4,9) */
   int t2 = (ps.x == 4.0f && ps.y == 9.0f) ? 1 : 0;

   CGPoint pt = CGPointApplyAffineTransform(p, xlate);   /* -> (7,10) */
   int t3 = (pt.x == 7.0f && pt.y == 10.0f) ? 1 : 0;

   /* CGSize: a translation does NOT affect a size (only the linear part). */
   CGSize s = {2.0f, 3.0f};

   CGSize si = CGSizeApplyAffineTransform(s, ident);     /* -> (2,3) */
   int t4 = (si.width == 2.0f && si.height == 3.0f) ? 1 : 0;

   CGSize ss = CGSizeApplyAffineTransform(s, scale);     /* -> (4,9) */
   int t5 = (ss.width == 4.0f && ss.height == 9.0f) ? 1 : 0;

   CGSize st = CGSizeApplyAffineTransform(s, xlate);     /* -> (2,3) unchanged */
   int t6 = (st.width == 2.0f && st.height == 3.0f) ? 1 : 0;

   exit(t1*1 + t2*2 + t3*4 + t4*8 + t5*16 + t6*32);      /* all correct == 63 */
   return 0;
}
