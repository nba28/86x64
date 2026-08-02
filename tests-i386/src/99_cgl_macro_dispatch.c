/* 99_cgl_macro_dispatch.c — the <OpenGL/CGLMacro.h> DIRECT-DISPATCH contract.
 *
 * Halo CE (and every 32-bit Mac game with a real renderer) is compiled with
 * <OpenGL/CGLMacro.h>, which #defines every glXxx() to bypass the entry point and
 * go straight through the context object:
 *
 *     #define glGetString(name)  (*(cgl_ctx)->disp.get_string)((cgl_ctx)->rend, name)
 *
 * so such an app NEVER CALLS A GL SYMBOL — it dereferences its AGLContext (which
 * IS a CGLContextObj) and jumps through a slot of the embedded GLIFunctionDispatch
 * table. Handed a proxy arena handle instead of a real context object,
 * `->disp.get_string` reads unrelated arena bytes = 0 and the app executes
 * `jmp *0` (measured live in Halo: rip=0, call site Halo.dylib+0x3a09a6, with
 * arg1 = 0x1F03 = GL_EXTENSIONS).
 *
 * This fixture runs that idiom VERBATIM, including reading the slot at the
 * hardcoded byte offset a 2003 compiler baked in, and calling through it. It
 * asserts a REAL HARDWARE FACT (GL_MAX_TEXTURE_SIZE > 0 read back through the
 * dispatch slot), so it cannot pass inertly: that value can only appear if the
 * slot held a live thunk AND that thunk marshalled the i386 cdecl frame — with
 * the leading `rend` dropped and the caller's stack left balanced — into the real
 * glGetIntegerv.
 *
 * It probes BOTH get_integerv (slot 104, result written into a caller-supplied
 * i386 buffer) and get_string (slot 117, the slot Halo actually faulted on).
 * get_string RETURNS a `const GLubyte *` and a native GL string lives above 4GB,
 * so it additionally exercises abigen's C-string return bounce
 * (`_x64_cstr_ret_low`, abigen.cc's Char_S/Char_U pointee-return arm): the value
 * the i386 side receives must be a NON-ZERO pointer BELOW 4GB whose bytes are
 * readable — the exact thing Halo does next (`strcpy(local, glGetString(
 * GL_EXTENSIONS))` then `strstr` for "GL_EXT_framebuffer_object" / "NVIDIA").
 * That emit was reverted once before over a NASM convergence problem, so it is
 * worth a standing assertion.
 *
 * ⚠The GL enums here are built ARITHMETICALLY from constants below 0x1000. A
 * literal `movl $0x1f00,(%esp)` would be rewritten by the core translator's
 * stack-arg pointer-immediate heuristic (instruction.cc ~L1197): 0x1F00 aliases a
 * real address inside this small fixture's own __TEXT,__cstring, so the INTEGER
 * CONSTANT gets relocated to a string pointer and GL answers GL_INVALID_ENUM.
 * That is a separate, still-open CORE defect (it does not affect Halo, whose
 * 0x1F0x lands in the header/load-command gap with no blob — verified on the
 * deployed Halo.dylib); constants below 0x1000 are inside __PAGEZERO, which the
 * heuristic always skips, so the arithmetic dodges it without hiding it.
 *
 * ⚠It must never JUMP through the slot when the slot is 0 — that is the very crash
 * being guarded, and a crashing fixture reports nothing. So the call is gated on
 * the slot being non-zero and the arm is distinguished by what is PRINTED.
 *
 * The context is built through AGL rather than CGL because that is the path the
 * real 32-bit apps use (and the one 99_agl_window_drawable already proves works
 * headless in this environment). AGL is not in the i386 sysroot, so the whole
 * surface is an undefined dynamic_lookup import bound at translate time by
 * static-interpose to libabiconv's ___<sym> shims.
 */

#include <stdio.h>
#include <stdint.h>

typedef void *AGLPixelFormat;
typedef void *AGLContext;
typedef unsigned char GLboolean;

extern AGLPixelFormat aglChoosePixelFormat(const void *gdevs, int ndev, const int *attrs);
extern AGLContext     aglCreateContext(AGLPixelFormat pix, AGLContext share);
extern GLboolean      aglDestroyPixelFormat(AGLPixelFormat pix);
extern GLboolean      aglDestroyContext(AGLContext ctx);
extern GLboolean      aglSetCurrentContext(AGLContext ctx);

#define AGL_NONE          0
#define AGL_RGBA          4
#define AGL_DOUBLEBUFFER  5
#define AGL_DEPTH_SIZE   12
#define AGL_ACCELERATED  73

/* struct _CGLContextObject in the 32-bit layout: a 4-byte GLIContext `rend`
 * followed by GLIFunctionDispatch BY VALUE. get_string is dispatch field 117, so
 * its byte offset is 4 + 117*4 = 0x1D8 — the exact offset in Halo's faulting
 * instruction `movl 0x1d8(%rsi),%eax`. */
#define REND_OFF          0x00
#define GET_STRING_OFF    0x1D8    /* field 117: the slot Halo faulted on       */
#define GET_INTEGERV_OFF  0x1A4    /* field 104: void (rend, GLenum, GLint *)   */

#define GL_MAX_TEXTURE_SIZE 0x0D33   /* < 0x1000: safe as a literal */

typedef void (*get_integerv_fn)(unsigned int rend, unsigned int name, int *params);
typedef const unsigned char *(*get_string_fn)(unsigned int rend, unsigned int name);

int main(void)
{
   /* Unbuffered: the disarmed arm is EXPECTED to be a step away from a fault, and
    * buffered stdout would be lost with the process, leaving both arms looking
    * identically empty. The guard's whole value is in what gets printed. */
   setvbuf(stdout, NULL, _IONBF, 0);

   int attrs[] = { AGL_RGBA, AGL_DOUBLEBUFFER, AGL_DEPTH_SIZE, 24, AGL_ACCELERATED, AGL_NONE };
   AGLPixelFormat pix = aglChoosePixelFormat(0, 0, attrs);
   printf("pixfmt=%d\n", pix ? 1 : 0);

   AGLContext ctx = pix ? aglCreateContext(pix, 0) : 0;
   if (pix) aglDestroyPixelFormat(pix);
   printf("ctx=%d\n", ctx ? 1 : 0);
   if (!ctx) { printf("slot=0\nvendor=0\nversion=0\n"); return 0; }

   aglSetCurrentContext(ctx);

   /* THE IDIOM: reach into the context exactly as CGLMacro.h does. */
   const unsigned char *base = (const unsigned char *)ctx;
   unsigned int rend = *(const unsigned int *)(base + REND_OFF);
   unsigned int slot = *(const unsigned int *)(base + GET_STRING_OFF);
   unsigned int islot = *(const unsigned int *)(base + GET_INTEGERV_OFF);

   printf("slot=%d\n", slot ? 1 : 0);
   printf("islot=%d\n", islot ? 1 : 0);

   if (!islot || !slot) {
      /* What the app does next is `jmp *0`. Do NOT reproduce that — report it, so
       * the disarmed arm yields evidence rather than a corpse. */
      printf("maxtex=0\nglstr=0\n");
      return 0;
   }

   /* Call it the CGLMacro way, twice, so a thunk that leaked stack per call would
    * show up rather than being masked by a single lucky invocation. */
   get_integerv_fn get_integerv = (get_integerv_fn)(uintptr_t)islot;
   int maxtex = -1, maxtex2 = -1;
   get_integerv(rend, GL_MAX_TEXTURE_SIZE, &maxtex);
   get_integerv(rend, GL_MAX_TEXTURE_SIZE, &maxtex2);
   printf("maxtex=%d\n", (maxtex > 0 && maxtex2 == maxtex) ? 1 : 0);
   if (maxtex > 0) printf("MAX_TEXTURE_SIZE=%d\n", maxtex);

   /* THE STRING HALF: get_string through the same slot machinery. The result
    * must be a non-zero pointer BELOW 4GB (i.e. representable in the i386 slot
    * at all) whose bytes are readable and non-empty — Halo strcpy's it. */
   volatile unsigned int lo = 0x0F00u, hi = 0x0100u;
   const unsigned int GL_VENDOR_ = lo + hi * 16u;        /* 0x1F00 */
   get_string_fn get_string = (get_string_fn)(uintptr_t)slot;
   const unsigned char *vs = get_string(rend, GL_VENDOR_);
   int ok = 0;
   char vbuf[64];
   vbuf[0] = 0;
   if (vs != 0 && (unsigned long long)(uintptr_t)vs < 0x100000000ULL) {
      int i = 0;
      for (; i < 63 && vs[i]; ++i) vbuf[i] = (char)vs[i];
      vbuf[i] = 0;
      ok = (i > 0);
   }
   printf("glstr=%d\n", ok);
   if (ok) printf("GL_VENDOR=%s\n", vbuf);

   /* No aglDestroyContext: tearing the context down faults inside native AGL in
    * BOTH arms (i.e. independently of anything under test here), and a fixture
    * that dies after printing its verdict is a flaky fixture. Process exit
    * reclaims it. */
   return 0;
}
