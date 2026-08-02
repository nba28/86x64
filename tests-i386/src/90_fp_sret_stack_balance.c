/*
 * 90_fp_sret_stack_balance — does the fp_sret shim family balance the i386
 * caller's stack?
 *
 * WHY THIS EXISTS. The i386 cdecl rule for a struct returned through a hidden,
 * caller-allocated buffer is that the CALLEE POPS THE HIDDEN POINTER — `retl $4`
 * — verified from real `clang -arch i386 -O1 -S` codegen, and the caller's own
 * post-call `addl` already excludes those 4 bytes. Our generated shim IS that
 * i386 callee, so its epilogue must remove 8 (return address + hidden pointer).
 *
 * That was fixed for the mem_reg_sret class (guard 89_sret_regclass_gap), but
 * `fp_sret` — the homogeneous-FP MEMORY-on-both-ABIs family, i.e. every
 * CGAffineTransform- and CGRect-returning geometry call — still pops only 4
 * (abigen.cc, the `mem_reg_sret ? "8" : "4"` epilogue). If that is wrong, every
 * such call leaks 4 bytes of the caller's stack, which does not corrupt anything
 * at the call site and so is invisible to a value check: test 50_cgaffine_sret
 * exercises this exact family and passes on values alone. It would instead
 * surface as damage far away and much later — the signature of a whole class of
 * hard-to-place crashes.
 *
 * So this guard deliberately checks ONLY the thing a value test cannot see: the
 * caller's ESP across a long run of calls. 64 iterations turn a 4-byte-per-call
 * error into a 256-byte displacement, which cannot pass by luck.
 *
 * CoreGraphics is not in the i386 sysroot, so CGAffineTransformMakeScale is an
 * undefined dynamic_lookup import that the pipeline's static-interpose redirects
 * into libabiconv's ___CGAffineTransform* shim — the real shim, exactly as a
 * translated app reaches it.
 *
 * Validation by EXIT CODE: four independent one-bit checks, all-correct == 15.
 *   bit 0  the returned value is still correct (guards against a "fix" that
 *          balances the stack by breaking the result)
 *   bit 1  ESP is balanced across 64 fp_sret calls        <-- the real subject
 *   bit 2  ESP is balanced across a nested/interleaved run
 *   bit 3  a second, differently-shaped fp_sret entry point also balances
 */
extern void exit(int status);

typedef float CGFloat;                               /* i386: CGFloat is float */
typedef struct { CGFloat a, b, c, d, tx, ty; } CGAffineTransform;

extern CGAffineTransform CGAffineTransformMakeScale(CGFloat sx, CGFloat sy);
extern CGAffineTransform CGAffineTransformMakeTranslation(CGFloat tx, CGFloat ty);

int main(void) {
   /* ---- bit 0: the value path still works (2.0/3.0 are exact in both) ---- */
   CGAffineTransform t = CGAffineTransformMakeScale(2.0f, 3.0f);
   int t1 = (t.a == 2.0f && t.d == 3.0f && t.b == 0.0f && t.c == 0.0f &&
             t.tx == 0.0f && t.ty == 0.0f) ? 1 : 0;

   /* ---- bit 1: ESP balance across a straight run of fp_sret calls -------- */
   unsigned esp_before, esp_after;
   volatile float acc = 0.0f;
   __asm__ volatile ("movl %%esp, %k0" : "=r"(esp_before));
   for (int i = 0; i < 64; i++) {
      CGAffineTransform s = CGAffineTransformMakeScale((float)(i + 1), 2.0f);
      acc += s.a;
   }
   __asm__ volatile ("movl %%esp, %k0" : "=r"(esp_after));
   int t2 = (esp_before == esp_after) ? 1 : 0;

   /* ---- bit 2: interleaved, so a drift cannot be masked by reuse of the
    * same stack slot in a tight identical loop --------------------------- */
   unsigned esp_b2, esp_a2;
   __asm__ volatile ("movl %%esp, %k0" : "=r"(esp_b2));
   for (int i = 0; i < 32; i++) {
      CGAffineTransform s = CGAffineTransformMakeScale((float)(i + 1), 1.0f);
      CGAffineTransform u = CGAffineTransformMakeTranslation(1.0f, (float)(i + 1));
      acc += s.a + u.ty;
   }
   __asm__ volatile ("movl %%esp, %k0" : "=r"(esp_a2));
   int t3 = (esp_b2 == esp_a2) ? 1 : 0;

   /* ---- bit 3: the other entry point on its own -------------------------- */
   unsigned esp_b3, esp_a3;
   __asm__ volatile ("movl %%esp, %k0" : "=r"(esp_b3));
   for (int i = 0; i < 64; i++) {
      CGAffineTransform u = CGAffineTransformMakeTranslation((float)(i + 1), 5.0f);
      acc += u.tx;
   }
   __asm__ volatile ("movl %%esp, %k0" : "=r"(esp_a3));
   int t4 = (esp_b3 == esp_a3) ? 1 : 0;

   exit(t1*1 + t2*2 + t3*4 + t4*8);                  /* all correct == 15 */
   return 0;
}
