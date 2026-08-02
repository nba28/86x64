/* 98_glstring_probe.c — SCRATCH DIAGNOSTIC (not a guard; deleted before merge).
 *
 * Measures whether ___glGetString can (a) receive its GLenum uncorrupted and
 * (b) hand its `const GLubyte *` back to i386 code. Two call paths:
 *   [1] the plain generated bridge (glGetString(name))
 *   [2] the CGLMacro dispatch slot 117 (what Halo actually does)
 * Called repeatedly so a "first call only" (lazy-stub) defect is visible.
 */

#include <stdio.h>
#include <stdint.h>

typedef void *AGLPixelFormat;
typedef void *AGLContext;
typedef unsigned char GLboolean;

extern AGLPixelFormat aglChoosePixelFormat(const void *gdevs, int ndev, const int *attrs);
extern AGLContext     aglCreateContext(AGLPixelFormat pix, AGLContext share);
extern GLboolean      aglDestroyPixelFormat(AGLPixelFormat pix);
extern GLboolean      aglSetCurrentContext(AGLContext ctx);
extern const unsigned char *glGetString(unsigned int name);
extern unsigned int   glGetError(void);
extern void           glGetIntegerv(unsigned int pname, int *params);

#define AGL_NONE          0
#define AGL_RGBA          4
#define AGL_DOUBLEBUFFER  5
#define AGL_DEPTH_SIZE   12
#define AGL_ACCELERATED  73

#define GL_VENDOR      0x1F00
#define GL_RENDERER    0x1F01
#define GL_VERSION     0x1F02
#define GL_EXTENSIONS  0x1F03
#define GL_MAX_TEXTURE_SIZE 0x0D33

#define GET_STRING_OFF   0x1D8
#define REND_OFF         0x00

typedef const unsigned char *(*get_string_fn)(unsigned int rend, unsigned int name);

static void show(const char *tag, const unsigned char *p)
{
   unsigned int err = glGetError();
   printf("%-16s ptr=0x%08x err=%u", tag, (unsigned int)(uintptr_t)p, err);
   if (p) {
      /* read the first bytes — this is the strcpy Halo does */
      char buf[64];
      int i = 0;
      for (; i < 63 && p[i]; ++i) buf[i] = (char)p[i];
      buf[i] = 0;
      printf(" str=\"%s\"", buf);
   }
   printf("\n");
}

int main(void)
{
   setvbuf(stdout, NULL, _IONBF, 0);

   int attrs[] = { AGL_RGBA, AGL_DOUBLEBUFFER, AGL_DEPTH_SIZE, 24, AGL_ACCELERATED, AGL_NONE };
   AGLPixelFormat pix = aglChoosePixelFormat(0, 0, attrs);
   AGLContext ctx = pix ? aglCreateContext(pix, 0) : 0;
   if (pix) aglDestroyPixelFormat(pix);
   printf("ctx=%d\n", ctx ? 1 : 0);
   if (!ctx) return 0;
   aglSetCurrentContext(ctx);

   int maxtex = -1;
   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxtex);
   printf("maxtex=%d\n", maxtex);

   /* volatile: keeps the enum out of a `movl $imm,(%esp)` stack-arg store, so
    * the core translator's pointer-immediate heuristic cannot relocate it.
    * This is what Halo's own call sites look like after translation (measured:
    * all 7 keep $0x1f0X), so this arm is the one that models the real target. */
   /* Build the enum ARITHMETICALLY from constants below 0x1000 (inside
    * __PAGEZERO, which the heuristic always skips) so no `movl $0x1f0X` ever
    * appears -- otherwise the heuristic relocates it and we measure the wrong
    * defect. */
   volatile unsigned int a = 0x0F00u, b = 0x0100u;
   volatile unsigned int e;
   const unsigned int V = a + b * 16u;            /* 0x1F00 GL_VENDOR */
   e = V;     show("V direct#1 VEND", glGetString(e));
   e = V;     show("V direct#2 VEND", glGetString(e));
   e = V + 1; show("V direct RENDER", glGetString(e));
   e = V + 2; show("V direct VERSN ", glGetString(e));
   e = V + 3; show("V direct EXTNS ", glGetString(e));

   show("direct#1 VENDOR",   glGetString(GL_VENDOR));
   show("direct#2 VENDOR",   glGetString(GL_VENDOR));
   show("direct  RENDERER",  glGetString(GL_RENDERER));
   show("direct  VERSION",   glGetString(GL_VERSION));
   show("direct  EXTENSION", glGetString(GL_EXTENSIONS));

   const unsigned char *base = (const unsigned char *)ctx;
   unsigned int rend = *(const unsigned int *)(base + REND_OFF);
   unsigned int slot = *(const unsigned int *)(base + GET_STRING_OFF);
   printf("slot=0x%08x rend=0x%08x\n", slot, rend);
   if (slot) {
      get_string_fn gs = (get_string_fn)(uintptr_t)slot;
      e = V;     show("V disp#1 VENDR", gs(rend, e));
      e = V;     show("V disp#2 VENDR", gs(rend, e));
      e = V + 3; show("V disp EXTNS  ", gs(rend, e));
      show("disp#1 VENDOR",  gs(rend, GL_VENDOR));
      show("disp#2 VENDOR",  gs(rend, GL_VENDOR));
      show("disp  EXTENSION", gs(rend, GL_EXTENSIONS));
   }
   return 0;
}
