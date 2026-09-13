/*
 * gl-call-probe.c — DIAGNOSTIC-ONLY. ONE job: make an ASYNCHRONOUS GPU-driver
 * crash name the GL call that caused it.
 *
 * THE PROBLEM THIS EXISTS FOR. Portal 2's translated renderer dies with
 * SIGSEGV err=0x14 (instruction fetch) at a wild 64-bit rip, and the faulting
 * stack is
 *      libdispatch -> libobjc -> Metal -> libGFXShared -> <garbage>
 * i.e. a block on Metal's device-dispatch queue. The thread that submitted the
 * bad work returned long before. ⇒ THE FAULTING STACK CAN NEVER NAME THE
 * CULPRIT, and no amount of re-reading it will help. Apple's own validation
 * (MTL_DEBUG_LAYER=1, MTL_SHADER_VALIDATION=1) reports no API misuse, so Metal
 * is not being handed an invalid argument — it is following something already
 * corrupt.
 *
 * The only way out of that shape is to make the failure SYNCHRONOUS, which is
 * what this does, two ways at once:
 *
 *   1. glGetError AFTER EVERY CALL. A driver that is about to be asked to do
 *      something impossible usually says so first. Reporting the error against
 *      the NAME OF THE CALL THAT RAISED IT converts "something went wrong
 *      somewhere" into a single line. (Source calls glGetError itself, so an
 *      error it checks for and handles is drained before we see it; anything we
 *      report is an error Source did NOT check.)
 *
 *   2. AN ARGUMENT SANITY CHECK on the sizing calls. The two bugs already found
 *      in this target were both a translated INTEGER that became garbage — a
 *      count that turned into an image address, and a constant relocated as a
 *      pointer. A bad width/height/samples reaching glRenderbufferStorage* or
 *      glBlitFramebuffer is exactly that failure mode, and it is also precisely
 *      the kind of argument a driver acts on LATER, on its own queue. So
 *      implausible dimensions are flagged at the call site even when GL raises
 *      no error at all.
 *
 * Plus a ring of the last GL calls, dumped from a SIGSEGV/SIGBUS handler that
 * CHAINS to whatever was installed before it (libabiconv's fault reporter), so
 * the last calls before the crash are named even if nothing looked wrong.
 *
 * WHY A SEPARATE DYLIB, not a libabiconv shim: it is a temporary instrument for
 * ONE question and must be removable by deleting a file and dropping an env var
 * (the one-shim-one-job rule). It interposes the REAL GL entry points, which
 * works because abigen's `___glXxx` bridge reaches OpenGL through an ordinary
 * symbol stub — so it needs no cooperation from the shim layer and no rebuild of
 * anything else. Distinct from halo-gl-texprobe.c, which asks about texture
 * geometry and vertex strides; different question, different surface.
 *
 * ── WHAT IT ESTABLISHED, and WHAT IS STILL UNKNOWN (2026-09-13) ───────────────
 * FOUND, reproducible:
 *   - Portal 2 dies inside its FIRST CGLQueryRendererInfo(0x1), BEFORE it issues a
 *     single GL call (the GL ring was empty on the first run — that empty ring is
 *     the finding, and it is invisible without an instrument, because "no GL calls
 *     happened" looks exactly like "the probe did not work").
 *   - The crash lands on a libdispatch worker running Metal's device-dispatch
 *     queue, jumping to a garbage 64-bit pointer — the shape of a corrupted block
 *     invoke pointer, and ASYNCHRONOUS to the caller.
 *   - ★GLPROBE_SELFTEST=1 CLEARS THE WALL: doing that one call from native code in
 *     this dylib's constructor makes the app's own identical call SUCCEED, after
 *     which Portal 2 completes full renderer enumeration (2 renderers, ~20
 *     properties each) and survives the watchdog. So it is the FIRST, one-time
 *     initialisation that fails, not the call.
 *
 * FALSIFIED — do not re-test any of these:
 *   1. The argument. CGLQueryRendererInfo(0x1) is correct: 0x1 IS this machine's
 *      CGDisplayIDToOpenGLDisplayMask, and probes/cglprobe.c returns err=0 and 2
 *      renderers for it.
 *   2. Rosetta. The standalone probe is x86_64 under Rosetta too, and succeeds.
 *   3. Translation/bridging as such. The translated i386 guard
 *      99_cgl_renderer_check makes this exact call through the bridge and PASSES.
 *   4. Stack size. 1021 KiB of headroom remained; the same call succeeds natively
 *      on a 256 KiB thread stack.
 *   5. Stack address. A low-4GB stack is not fatal: after the selftest the app's
 *      own call on that same low stack succeeds.
 *   6. Stale interposition (the iWeb libxml2 class). dyld_info -fixups shows all
 *      13 CGL and 30 GL imports bound to libabiconv bridges, none left native.
 *   7. "The driver is mapped below 4GB." AGXMetalG16X loads below 4GB in BOTH the
 *      failing (0x190c1000) and succeeding (0x3f24000) runs.
 *
 * ★NARROWED (GLPROBE_PRELOAD=1): the fragile step is METAL DEVICE CREATION, not
 * anything in CGL. Calling MTLCreateSystemDefaultDevice() alone from this native
 * constructor — touching no CGL at all — is enough to make Portal 2's own
 * CGLQueryRendererInfo succeed and the process survive. So the target is "the
 * one-time creation of the Metal device / load-and-init of the AGX driver",
 * which is far more specific and more fixable than "the first GL call".
 *
 * ⚠ ONE CONFOUND REMAINS, and it must be separated before anyone builds a fix.
 * Loading Metal early also CHANGES WHERE THE DRIVER LANDS: AGXMetalG16X maps at
 * 0x4271000 / 0x3f24000 when loaded early (BELOW our mmap band) and at
 * 0x190c1000 when loaded late (INSIDE the band [0x10000000,0x80000000), because
 * by then the space under 0x10000000 is full of translated images). So "early
 * native caller" and "driver outside our band" are confounded in every
 * succeeding run. The experiment that separates them is to make the driver load
 * LATE but outside the band — e.g. reserve the whole band up front so dyld is
 * forced to place it above 4GB — and see whether the app's call then succeeds.
 *
 * ⛔ ALSO FALSIFIED — OUR ADD-IMAGE HANDLER DOES NOT SCRIBBLE THE DRIVER. This is
 * the most natural theory to reach for (a native GPU bundle lands in the low band,
 * and our runtime mutates images as they load), and it is wrong: all three
 * per-image mutators gate correctly and skip it, because AGXMetalG16X's preferred
 * __TEXT vmaddr is 0x0.
 *   - wrap_mod_init_funcs (objc_slide.c) — gated on image_links_libabiconv().
 *   - _86x64_import_repair — deliberately NOT gated on that, but returns early
 *     unless text_base == IR_TRANSLATED_TEXT_BASE.
 *   - fixup_translated_dylib_slots (wrapper_setup.c) — requires
 *     seg->vmaddr >= TRANSLATED_DYLIB_VMADDR (0x10000000).
 *
 * STILL OPEN: why the first-touch fails. The one surviving difference between the
 * two AGX placements is that the failing one lands INSIDE the mmap band
 * [0x10000000,0x80000000) our allocators use, while the succeeding one is below
 * it. Whether that is causal is untested. ⇒ Next: check whether anything of ours
 * hands out or reserves memory overlapping the driver's mapping, and treat "a
 * native late-dlopen'd bundle placed inside our claimed band" as a hazard in its
 * own right regardless of this crash.
 *
 * ⚠ IT PERTURBS TIMING BY DESIGN. glGetError forces the driver to settle its
 * error state, so a race may move or vanish under it. A crash that disappears
 * here is itself a finding (report it, do not celebrate it).
 *
 * Covers the 30 GL entry points Portal 2's shaderapidx9.dylib imports (no other
 * module imports any) AND the 13 CGL entry points it uses.
 *
 * ★THE CGL HALF IS WHY THIS TOOL FOUND ANYTHING. The first run with only the GL
 * surface instrumented reported an EMPTY ring: Portal 2 crashes before it issues a
 * single GL command. That is itself the finding — the fault is in CONTEXT AND
 * RENDERER SETUP, not in rendering — and it is exactly the sort of negative result
 * that is invisible without an instrument, because "no GL calls happened" looks
 * identical to "the probe did not work". (It did: it logged its arm line.)
 * CGLQueryRendererInfo/CGLDescribeRenderer instantiate the GPU drivers, which is
 * what spins up Metal's device-dispatch queue in the first place.
 *
 * env:
 *   GLPROBE_LOG=<path>  write there instead of stderr
 *   GLPROBE_ALL=1       log EVERY call, not just errors/implausible args
 *   GLPROBE_RING=<n>    ring depth (default 64)
 *
 * build: clang -arch x86_64 -dynamiclib -O1 -Wall -DGL_SILENCE_DEPRECATION \
 *              -framework OpenGL -o gl-call-probe.dylib gl-call-probe.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLRenderers.h>

/* ---- output ------------------------------------------------------------- */
static FILE *g_out;
static int   g_all;

static void probe_init(void) {
   const char *p = getenv("GLPROBE_LOG");
   g_out = NULL;
   if (p && *p) g_out = fopen(p, "w");
   if (!g_out) g_out = stderr;
   g_all = getenv("GLPROBE_ALL") ? 1 : 0;
}
#define LOG(...) do { \
      if (!g_out) probe_init(); \
      fprintf(g_out, "[glprobe] " __VA_ARGS__); fflush(g_out); \
   } while (0)

/* ---- ring of recent calls ---------------------------------------------- */
/* Async-safe on the dump side: only already-constant strings plus one small
 * integer per entry, written with write(2). */
#define RING_MAX 512
static const char *g_ring[RING_MAX];
static long        g_ring_arg[RING_MAX];
static unsigned    g_ring_n = 64;
static volatile unsigned g_ring_pos;

static void ring_put(const char *name, long a) {
   unsigned i = g_ring_pos++ % (g_ring_n ? g_ring_n : 1);
   g_ring[i] = name;
   g_ring_arg[i] = a;
}

static void wr(const char *s) {
   ssize_t r; size_t n = strlen(s);
   while (n) { r = write(2, s, n); if (r <= 0) { if (errno == EINTR) continue; break; }
               s += r; n -= (size_t)r; }
}
static void wr_dec(long v) {
   char b[24]; int i = 23; b[i--] = 0;
   int neg = v < 0; unsigned long u = neg ? (unsigned long)(-v) : (unsigned long)v;
   if (!u) b[i--] = '0';
   while (u && i >= 0) { b[i--] = (char)('0' + (u % 10)); u /= 10; }
   if (neg && i >= 0) b[i--] = '-';
   wr(&b[i + 1]);
}

/* ---- error check ------------------------------------------------------- */
/* glGetError is NOT interposed, so this reaches the real one directly. */
static const char *gl_err_name(GLenum e) {
   switch (e) {
      case GL_INVALID_ENUM:      return "GL_INVALID_ENUM";
      case GL_INVALID_VALUE:     return "GL_INVALID_VALUE";
      case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
      case GL_STACK_OVERFLOW:    return "GL_STACK_OVERFLOW";
      case GL_STACK_UNDERFLOW:   return "GL_STACK_UNDERFLOW";
      case GL_OUT_OF_MEMORY:     return "GL_OUT_OF_MEMORY";
      case 0x0506:               return "GL_INVALID_FRAMEBUFFER_OPERATION";
      default:                   return "GL_ERROR_?";
   }
}
static void check(const char *who) {
   GLenum e = glGetError();
   if (e != GL_NO_ERROR)
      LOG("*** %s raised %s (0x%x)\n", who, gl_err_name(e), e);
}

/* A dimension the hardware could never have been asked for on purpose. The
 * point is not to guess a real limit — it is to catch a value that is obviously
 * not a dimension at all, which is what a mistranslated integer looks like. */
static int bad_dim(GLsizei v)     { return v < 0 || v > 65536; }
static int bad_samples(GLsizei v) { return v < 0 || v > 64; }

#define ENTER(name, a) ring_put(name, (long)(a))
#define NOTE(...)      do { if (g_all) LOG(__VA_ARGS__); } while (0)

/* ---- interposers ------------------------------------------------------- */
static void p_clear(GLbitfield m) {
   ENTER("glClear", m); NOTE("glClear mask=%#x\n", m);
   glClear(m); check("glClear");
}
static void p_clearcolor(GLclampf r, GLclampf g, GLclampf b, GLclampf a) {
   ENTER("glClearColor", 0); NOTE("glClearColor %g %g %g %g\n", r, g, b, a);
   glClearColor(r, g, b, a); check("glClearColor");
}
static void p_viewport(GLint x, GLint y, GLsizei w, GLsizei h) {
   ENTER("glViewport", w);
   if (g_all || bad_dim(w) || bad_dim(h))
      LOG("%sglViewport x=%d y=%d w=%d h=%d\n",
          (bad_dim(w) || bad_dim(h)) ? "*** IMPLAUSIBLE " : "", x, y, w, h);
   glViewport(x, y, w, h); check("glViewport");
}
static void p_scissor(GLint x, GLint y, GLsizei w, GLsizei h) {
   ENTER("glScissor", w);
   if (g_all || bad_dim(w) || bad_dim(h))
      LOG("%sglScissor x=%d y=%d w=%d h=%d\n",
          (bad_dim(w) || bad_dim(h)) ? "*** IMPLAUSIBLE " : "", x, y, w, h);
   glScissor(x, y, w, h); check("glScissor");
}
static void p_enable(GLenum c)  { ENTER("glEnable", c);  NOTE("glEnable %#x\n", c);  glEnable(c);  check("glEnable"); }
static void p_disable(GLenum c) { ENTER("glDisable", c); NOTE("glDisable %#x\n", c); glDisable(c); check("glDisable"); }
static void p_depthmask(GLboolean f) { ENTER("glDepthMask", f); NOTE("glDepthMask %d\n", f); glDepthMask(f); check("glDepthMask"); }
static void p_activetexture(GLenum t) { ENTER("glActiveTexture", t); NOTE("glActiveTexture %#x\n", t); glActiveTexture(t); check("glActiveTexture"); }
static void p_bindtexture(GLenum t, GLuint n) { ENTER("glBindTexture", n); NOTE("glBindTexture target=%#x name=%u\n", t, n); glBindTexture(t, n); check("glBindTexture"); }
static void p_useprogram(GLuint pr) { ENTER("glUseProgram", pr); NOTE("glUseProgram %u\n", pr); glUseProgram(pr); check("glUseProgram"); }
static void p_finish(void) { ENTER("glFinish", 0); NOTE("glFinish\n"); glFinish(); check("glFinish"); }
static void p_begin(GLenum m) { ENTER("glBegin", m); NOTE("glBegin %#x\n", m); glBegin(m); }
static void p_end(void) { ENTER("glEnd", 0); NOTE("glEnd\n"); glEnd(); check("glEnd"); }
static void p_vertex3f(GLfloat x, GLfloat y, GLfloat z) { ENTER("glVertex3f", 0); glVertex3f(x, y, z); }
static void p_texcoord2f(GLfloat s, GLfloat t) { ENTER("glTexCoord2f", 0); glTexCoord2f(s, t); }
static void p_getintegerv(GLenum p, GLint *v) {
   ENTER("glGetIntegerv", p); NOTE("glGetIntegerv %#x -> %p\n", p, (void *)v);
   glGetIntegerv(p, v); check("glGetIntegerv");
}
static const GLubyte *p_getstring(GLenum n) {
   ENTER("glGetString", n);
   const GLubyte *s = glGetString(n);
   NOTE("glGetString %#x -> %s\n", n, s ? (const char *)s : "(null)");
   check("glGetString");
   return s;
}
static void p_gettexlevelparameteriv(GLenum t, GLint l, GLenum p, GLint *v) {
   ENTER("glGetTexLevelParameteriv", p);
   NOTE("glGetTexLevelParameteriv target=%#x level=%d pname=%#x\n", t, l, p);
   glGetTexLevelParameteriv(t, l, p, v); check("glGetTexLevelParameteriv");
}

/* ---- FBO / renderbuffer: the sizing surface, where a garbage integer bites -- */
static void p_bindfb(GLenum t, GLuint fb) { ENTER("glBindFramebufferEXT", fb); NOTE("glBindFramebufferEXT target=%#x fb=%u\n", t, fb); glBindFramebufferEXT(t, fb); check("glBindFramebufferEXT"); }
static void p_bindrb(GLenum t, GLuint rb) { ENTER("glBindRenderbufferEXT", rb); NOTE("glBindRenderbufferEXT target=%#x rb=%u\n", t, rb); glBindRenderbufferEXT(t, rb); check("glBindRenderbufferEXT"); }
static void p_genfb(GLsizei n, GLuint *ids) {
   ENTER("glGenFramebuffersEXT", n);
   if (g_all || n < 0 || n > 4096) LOG("%sglGenFramebuffersEXT n=%d\n", (n < 0 || n > 4096) ? "*** IMPLAUSIBLE " : "", n);
   glGenFramebuffersEXT(n, ids); check("glGenFramebuffersEXT");
}
static void p_genrb(GLsizei n, GLuint *ids) {
   ENTER("glGenRenderbuffersEXT", n);
   if (g_all || n < 0 || n > 4096) LOG("%sglGenRenderbuffersEXT n=%d\n", (n < 0 || n > 4096) ? "*** IMPLAUSIBLE " : "", n);
   glGenRenderbuffersEXT(n, ids); check("glGenRenderbuffersEXT");
}
static void p_delfb(GLsizei n, const GLuint *ids) { ENTER("glDeleteFramebuffersEXT", n); NOTE("glDeleteFramebuffersEXT n=%d\n", n); glDeleteFramebuffersEXT(n, ids); check("glDeleteFramebuffersEXT"); }
static void p_delrb(GLsizei n, const GLuint *ids) { ENTER("glDeleteRenderbuffersEXT", n); NOTE("glDeleteRenderbuffersEXT n=%d\n", n); glDeleteRenderbuffersEXT(n, ids); check("glDeleteRenderbuffersEXT"); }
static void p_fbrb(GLenum t, GLenum at, GLenum rbt, GLuint rb) { ENTER("glFramebufferRenderbufferEXT", rb); NOTE("glFramebufferRenderbufferEXT target=%#x att=%#x rb=%u\n", t, at, rb); glFramebufferRenderbufferEXT(t, at, rbt, rb); check("glFramebufferRenderbufferEXT"); }
static void p_fbtex2d(GLenum t, GLenum at, GLenum tt, GLuint tex, GLint lv) { ENTER("glFramebufferTexture2DEXT", tex); NOTE("glFramebufferTexture2DEXT target=%#x att=%#x tex=%u level=%d\n", t, at, tex, lv); glFramebufferTexture2DEXT(t, at, tt, tex, lv); check("glFramebufferTexture2DEXT"); }

static void p_rbstorage(GLenum t, GLenum fmt, GLsizei w, GLsizei h) {
   ENTER("glRenderbufferStorageEXT", w);
   int bad = bad_dim(w) || bad_dim(h);
   if (g_all || bad)
      LOG("%sglRenderbufferStorageEXT target=%#x fmt=%#x w=%d h=%d\n",
          bad ? "*** IMPLAUSIBLE " : "", t, fmt, w, h);
   glRenderbufferStorageEXT(t, fmt, w, h); check("glRenderbufferStorageEXT");
}
static void p_rbstorage_ms(GLenum t, GLsizei s, GLenum fmt, GLsizei w, GLsizei h) {
   ENTER("glRenderbufferStorageMultisampleEXT", w);
   int bad = bad_dim(w) || bad_dim(h) || bad_samples(s);
   if (g_all || bad)
      LOG("%sglRenderbufferStorageMultisampleEXT target=%#x samples=%d fmt=%#x w=%d h=%d\n",
          bad ? "*** IMPLAUSIBLE " : "", t, s, fmt, w, h);
   glRenderbufferStorageMultisampleEXT(t, s, fmt, w, h);
   check("glRenderbufferStorageMultisampleEXT");
}
static void p_blitfb(GLint sx0, GLint sy0, GLint sx1, GLint sy1,
                     GLint dx0, GLint dy0, GLint dx1, GLint dy1,
                     GLbitfield mask, GLenum filter) {
   ENTER("glBlitFramebufferEXT", sx1);
   int bad = bad_dim(sx1 - sx0 > 0 ? sx1 - sx0 : 0) ||
             bad_dim(dx1 - dx0 > 0 ? dx1 - dx0 : 0);
   if (g_all || bad)
      LOG("%sglBlitFramebufferEXT src=(%d,%d)-(%d,%d) dst=(%d,%d)-(%d,%d) mask=%#x filter=%#x\n",
          bad ? "*** IMPLAUSIBLE " : "", sx0, sy0, sx1, sy1, dx0, dy0, dx1, dy1, mask, filter);
   glBlitFramebufferEXT(sx0, sy0, sx1, sy1, dx0, dy0, dx1, dy1, mask, filter);
   check("glBlitFramebufferEXT");
}

/* Report the CALLING THREAD'S STACK at the entry to the calls that dive into the
 * GPU driver. A translated thread runs on a LOW-4GB stack (i386 code must be able
 * to hold a pointer to it in 4 bytes), and a native framework called from
 * translated code inherits that stack. Metal's device/driver initialisation is
 * stack-hungry, so "how much headroom is left" is a first-class question here --
 * and it is the one difference between our call and the same call from a plain
 * native program, which succeeds. */
/* WHERE IS THE GL/GPU STACK MAPPED? A translated process reserves the low 4GB for
 * i386-representable pointers, so by the time a game dlopens its renderer the low
 * band is crowded and dyld places late-arriving NATIVE dylibs wherever it can --
 * including below 4GB. Portal 2's failing runs put AGXMetalG16X at 0x190c1000 /
 * 0x19fc5000 / 0x1a01f000, all under 4GB. Whether that is incidental or causal is
 * the open question, so record it for every run: the successful (pre-warmed) case
 * loads the same frameworks EARLY, when the band is still empty. */
static void where_is_gl(const char *when) {
   struct { const char *what; void *sym; } probes[] = {
      { "OpenGL (CGLQueryRendererInfo)", (void *)(uintptr_t)CGLQueryRendererInfo },
      { "libGL  (glGetString)",          (void *)(uintptr_t)glGetString },
   };
   for (unsigned i = 0; i < sizeof probes / sizeof *probes; i++) {
      Dl_info di;
      if (probes[i].sym && dladdr(probes[i].sym, &di) && di.dli_fbase) {
         unsigned long b = (unsigned long)(uintptr_t)di.dli_fbase;
         LOG("%s: %-30s base=%#lx%s\n", when, probes[i].what, b,
             b < 0x100000000UL ? "   <<< BELOW 4GB" : "");
      }
   }
   /* Metal and the AGX driver are only present once something has used them. */
   void *h = dlopen("/System/Library/Frameworks/Metal.framework/Metal", RTLD_NOLOAD);
   if (h) {
      void *sym = dlsym(h, "MTLCreateSystemDefaultDevice");
      Dl_info di;
      if (sym && dladdr(sym, &di) && di.dli_fbase) {
         unsigned long b = (unsigned long)(uintptr_t)di.dli_fbase;
         LOG("%s: %-30s base=%#lx%s\n", when, "Metal", b,
             b < 0x100000000UL ? "   <<< BELOW 4GB" : "");
      }
   } else {
      LOG("%s: Metal not loaded yet\n", when);
   }
   /* The AGX driver is a runtime-dlopen'd BUNDLE, not shared-cache resident, so
    * dyld places it in whatever hole it finds -- and in a translated process the
    * holes are in the low-4GB band we claim. Report where it landed. ⚠Uses dyld's
    * indexed image APIs, which our RUNTIME must never do (an image mid-load is
    * unqueryable and dyld aborts the process); it is tolerable here only because
    * this is a diagnostic probe calling it with no dlopen in flight. */
   uint32_t n = _dyld_image_count();
   for (uint32_t i = 0; i < n; i++) {
      const char *nm = _dyld_get_image_name(i);
      if (!nm || !strstr(nm, "AGX")) continue;
      unsigned long b = (unsigned long)(uintptr_t)_dyld_get_image_header(i);
      const char *leaf = strrchr(nm, '/');
      LOG("%s: %-30s base=%#lx%s\n", when, leaf ? leaf + 1 : nm, b,
          b < 0x100000000UL ? "   <<< BELOW 4GB, i.e. inside our low band" : "");
   }
}

static void stack_report(const char *who) {
   char here;
   void  *hi  = pthread_get_stackaddr_np(pthread_self());   /* highest address */
   size_t sz  = pthread_get_stacksize_np(pthread_self());
   uintptr_t lo = (uintptr_t)hi - sz;
   uintptr_t sp = (uintptr_t)&here;
   LOG("%s: stack [%#lx,%#lx) size=%zu KiB  sp=%#lx  headroom=%ld KiB%s\n",
       who, (unsigned long)lo, (unsigned long)hi, sz / 1024,
       (unsigned long)sp, (long)((sp - lo) / 1024),
       (sp < lo || sp > (uintptr_t)hi) ? "   <<< sp is OUTSIDE the reported stack!" : "");
}

/* ---- CGL: context and renderer setup, which runs BEFORE any GL call ----- */
/* CGL returns a CGLError rather than setting the GL error state, so these report
 * the return code directly and do not call glGetError (there may be no current
 * context yet, which would itself be an error). */
static void cgl_note(const char *who, CGLError e) {
   if (e != kCGLNoError)
      LOG("*** %s returned CGLError %d (%s)\n", who, (int)e, CGLErrorString(e));
   else
      NOTE("%s ok\n", who);
}

static CGLError p_choosepf(const CGLPixelFormatAttribute *attrs,
                           CGLPixelFormatObj *pix, GLint *npix) {
   ENTER("CGLChoosePixelFormat", 0);
   if (g_all && attrs) {
      LOG("CGLChoosePixelFormat attrs=");
      for (int i = 0; i < 64 && attrs[i]; i++) fprintf(g_out, "%d ", (int)attrs[i]);
      fprintf(g_out, "\n"); fflush(g_out);
   }
   CGLError e = CGLChoosePixelFormat(attrs, pix, npix);
   cgl_note("CGLChoosePixelFormat", e);
   if (e == kCGLNoError)
      NOTE("CGLChoosePixelFormat -> pix=%p npix=%d\n",
           (void *)(pix ? *pix : NULL), npix ? (int)*npix : -1);
   return e;
}
static CGLError p_createctx(CGLPixelFormatObj pix, CGLContextObj share,
                            CGLContextObj *ctx) {
   ENTER("CGLCreateContext", 0);
   CGLError e = CGLCreateContext(pix, share, ctx);
   cgl_note("CGLCreateContext", e);
   if (e == kCGLNoError)
      LOG("CGLCreateContext -> ctx=%p\n", (void *)(ctx ? *ctx : NULL));
   return e;
}
static CGLError p_destroyctx(CGLContextObj c) { ENTER("CGLDestroyContext", 0); CGLError e = CGLDestroyContext(c); cgl_note("CGLDestroyContext", e); return e; }
static CGLError p_destroypf(CGLPixelFormatObj p) { ENTER("CGLDestroyPixelFormat", 0); CGLError e = CGLDestroyPixelFormat(p); cgl_note("CGLDestroyPixelFormat", e); return e; }
static CGLError p_setcurrent(CGLContextObj c) {
   ENTER("CGLSetCurrentContext", 0);
   stack_report("CGLSetCurrentContext");
   NOTE("CGLSetCurrentContext ctx=%p\n", (void *)c);
   CGLError e = CGLSetCurrentContext(c); cgl_note("CGLSetCurrentContext", e); return e;
}
static CGLContextObj p_getcurrent(void) {
   ENTER("CGLGetCurrentContext", 0);
   CGLContextObj c = CGLGetCurrentContext();
   NOTE("CGLGetCurrentContext -> %p\n", (void *)c);
   return c;
}
static CGLError p_enable_cgl(CGLContextObj c, CGLContextEnable e2) { ENTER("CGLEnable", e2); NOTE("CGLEnable ctx=%p what=%d\n", (void *)c, (int)e2); CGLError e = CGLEnable(c, e2); cgl_note("CGLEnable", e); return e; }
static CGLError p_disable_cgl(CGLContextObj c, CGLContextEnable e2) { ENTER("CGLDisable", e2); NOTE("CGLDisable ctx=%p what=%d\n", (void *)c, (int)e2); CGLError e = CGLDisable(c, e2); cgl_note("CGLDisable", e); return e; }
static CGLError p_setparam(CGLContextObj c, CGLContextParameter pn, const GLint *v) { ENTER("CGLSetParameter", pn); NOTE("CGLSetParameter ctx=%p param=%d val=%d\n", (void *)c, (int)pn, v ? (int)*v : -1); CGLError e = CGLSetParameter(c, pn, v); cgl_note("CGLSetParameter", e); return e; }
static CGLError p_getparam(CGLContextObj c, CGLContextParameter pn, GLint *v) { ENTER("CGLGetParameter", pn); CGLError e = CGLGetParameter(c, pn, v); cgl_note("CGLGetParameter", e); NOTE("CGLGetParameter ctx=%p param=%d -> %d\n", (void *)c, (int)pn, v ? (int)*v : -1); return e; }

/* ★ Renderer enumeration is the prime suspect: it instantiates every GPU driver,
 * which is what creates Metal's device-dispatch queue. */

static CGLError p_queryrend(GLuint display_mask, CGLRendererInfoObj *rend, GLint *nrend) {
   ENTER("CGLQueryRendererInfo", display_mask);
   stack_report("CGLQueryRendererInfo");
   where_is_gl("app-call");
   LOG("CGLQueryRendererInfo display_mask=%#x ...\n", display_mask);
   CGLError e = CGLQueryRendererInfo(display_mask, rend, nrend);
   cgl_note("CGLQueryRendererInfo", e);
   LOG("CGLQueryRendererInfo -> rend=%p nrend=%d\n",
       (void *)(rend ? *rend : NULL), nrend ? (int)*nrend : -1);
   return e;
}
static CGLError p_describerend(CGLRendererInfoObj rend, GLint idx,
                               CGLRendererProperty prop, GLint *val) {
   ENTER("CGLDescribeRenderer", prop);
   LOG("CGLDescribeRenderer rend=%p idx=%d prop=%d ...\n", (void *)rend, (int)idx, (int)prop);
   CGLError e = CGLDescribeRenderer(rend, idx, prop, val);
   cgl_note("CGLDescribeRenderer", e);
   LOG("CGLDescribeRenderer prop=%d -> %d\n", (int)prop, val ? (int)*val : -1);
   return e;
}
static CGLError p_destroyrend(CGLRendererInfoObj r) { ENTER("CGLDestroyRendererInfo", 0); CGLError e = CGLDestroyRendererInfo(r); cgl_note("CGLDestroyRendererInfo", e); return e; }

/* ---- crash hook: name the last GL calls -------------------------------- */
static struct sigaction g_prev_segv, g_prev_bus;

static void dump_ring(int sig) {
   wr("\n[glprobe] ====== last GL calls before signal ");
   wr_dec(sig);
   wr(" (newest last) ======\n");
   unsigned n = g_ring_pos < g_ring_n ? g_ring_pos : g_ring_n;
   for (unsigned i = 0; i < n; i++) {
      unsigned idx = (g_ring_pos - n + i) % g_ring_n;
      if (!g_ring[idx]) continue;
      wr("[glprobe]   ");
      wr(g_ring[idx]);
      wr("  arg=");
      wr_dec(g_ring_arg[idx]);
      wr("\n");
   }
   wr("[glprobe] ====== end ======\n");
}

static void chain(struct sigaction *prev, int sig, siginfo_t *info, void *uc) {
   if (prev->sa_flags & SA_SIGINFO) {
      if (prev->sa_sigaction) { prev->sa_sigaction(sig, info, uc); return; }
   } else if (prev->sa_handler && prev->sa_handler != SIG_DFL &&
              prev->sa_handler != SIG_IGN) {
      prev->sa_handler(sig); return;
   }
   signal(sig, SIG_DFL);
   raise(sig);
}
static void on_segv(int sig, siginfo_t *info, void *uc) { dump_ring(sig); chain(&g_prev_segv, sig, info, uc); }
static void on_bus (int sig, siginfo_t *info, void *uc) { dump_ring(sig); chain(&g_prev_bus,  sig, info, uc); }

/* GLPROBE_SELFTEST=1 — call the suspect API from NATIVE code inside the TARGET
 * PROCESS, from this dylib's constructor, before any translated code has run.
 *
 * ★This is the bisection that a standalone probe cannot do. Portal 2's
 * CGLQueryRendererInfo(0x1) never returns, while the identical call in a plain
 * x86_64 program succeeds (err=0, 2 renderers) -- and that program is under
 * Rosetta too, so Rosetta is already excluded. What remains is the PROCESS: our
 * low-4GB malloc arena and mmap band, our libSystem interposers, the translated
 * images, and whatever state the app built before calling. Running the call here
 * separates "this process cannot do it at all" from "something the app did first
 * broke it", and those need completely different hunts. */
static void selftest(void) {
   if (!getenv("GLPROBE_SELFTEST")) return;
   LOG("SELFTEST: calling CGLQueryRendererInfo(0x1) from a NATIVE constructor, "
       "before any translated code runs\n");
   where_is_gl("SELFTEST/before");
   CGLRendererInfoObj info = NULL;
   GLint n = 0;
   CGLError e = CGLQueryRendererInfo(0x1, &info, &n);
   LOG("SELFTEST: -> err=%d (%s) nrend=%d info=%p\n",
       (int)e, CGLErrorString(e), (int)n, (void *)info);
   if (e == kCGLNoError && info) {
      GLint acc = -1;
      CGLDescribeRenderer(info, 0, kCGLRPAccelerated, &acc);
      LOG("SELFTEST: renderer 0 accelerated=%d\n", (int)acc);
      CGLDestroyRendererInfo(info);
   }
   where_is_gl("SELFTEST/after");
   LOG("SELFTEST: survived\n");
}

/* GLPROBE_PRELOAD=1 — narrow WHAT the fragile first-touch actually is. The
 * selftest above does a full CGLQueryRendererInfo, which internally creates a
 * context and spins up Metal's device dispatch, so it proves only that "some
 * first-touch on a native caller fixes it". This does the Metal half ALONE
 * (MTLCreateSystemDefaultDevice), touching no CGL. If the app's CGL call then
 * succeeds, the fragile step is Metal DEVICE CREATION, which is a far more
 * specific and more fixable target than "the first GL call". If it still
 * crashes, the fragile step is in CGL/OpenGL above Metal. */
/* GLPROBE_PRELOAD_MS=<ms> — do the preload from a DETACHED NATIVE THREAD after a
 * delay, instead of from the constructor. This separates the last two candidates:
 *   - if a LATE native preload still works, what matters is that the FIRST touch
 *     comes from a native caller, and the app's accumulated process state is
 *     irrelevant;
 *   - if a late one crashes where an early one worked, the process state the app
 *     builds up is what poisons Metal's initialisation.
 * It races the app (which reaches its own CGL call about 3 s in), so try a couple
 * of delays and read which one won from the log order. */
static void *preload_thread(void *arg) {
   long ms = (long)(intptr_t)arg;
   struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
   nanosleep(&ts, NULL);
   LOG("PRELOAD: (delayed %ld ms, native thread) "
       "MTLCreateSystemDefaultDevice()...\n", ms);
   void *h = dlopen("/System/Library/Frameworks/Metal.framework/Metal", RTLD_LAZY);
   if (!h) { LOG("PRELOAD: dlopen(Metal) failed: %s\n", dlerror()); return NULL; }
   void *(*mk)(void) = (void *(*)(void))dlsym(h, "MTLCreateSystemDefaultDevice");
   if (!mk) { LOG("PRELOAD: no MTLCreateSystemDefaultDevice\n"); return NULL; }
   void *dev = mk();
   LOG("PRELOAD: (delayed) -> device=%p\n", dev);
   where_is_gl("PRELOAD-delayed/after");
   return NULL;
}

static void preload(void) {
   const char *d = getenv("GLPROBE_PRELOAD_MS");
   if (d && *d) {
      long ms = strtol(d, NULL, 0);
      pthread_t t;
      if (pthread_create(&t, NULL, preload_thread, (void *)(intptr_t)ms) == 0) {
         pthread_detach(t);
         LOG("PRELOAD: scheduled in %ld ms on a native thread\n", ms);
      }
      return;
   }
   if (!getenv("GLPROBE_PRELOAD")) return;
   void *h = dlopen("/System/Library/Frameworks/Metal.framework/Metal", RTLD_LAZY);
   if (!h) { LOG("PRELOAD: dlopen(Metal) failed: %s\n", dlerror()); return; }
   void *(*mk)(void) = (void *(*)(void))dlsym(h, "MTLCreateSystemDefaultDevice");
   if (!mk) { LOG("PRELOAD: no MTLCreateSystemDefaultDevice\n"); return; }
   LOG("PRELOAD: MTLCreateSystemDefaultDevice() from a NATIVE constructor...\n");
   void *dev = mk();
   LOG("PRELOAD: -> device=%p\n", dev);
   where_is_gl("PRELOAD/after");
}

__attribute__((constructor))
static void probe_ctor(void) {
   probe_init();
   const char *r = getenv("GLPROBE_RING");
   if (r && *r) {
      long v = strtol(r, NULL, 0);
      if (v > 0 && v <= RING_MAX) g_ring_n = (unsigned)v;
   }
   /* Installed AFTER libabiconv's fault reporter (we are inserted, it is a
    * dependency of the app), so chaining reaches its report. */
   struct sigaction sa;
   memset(&sa, 0, sizeof sa);
   sa.sa_flags = SA_SIGINFO | SA_NODEFER;
   sigemptyset(&sa.sa_mask);
   sa.sa_sigaction = on_segv; sigaction(SIGSEGV, &sa, &g_prev_segv);
   sa.sa_sigaction = on_bus;  sigaction(SIGBUS,  &sa, &g_prev_bus);
   LOG("armed: %u-deep GL call ring + glGetError after every call%s\n",
       g_ring_n, g_all ? " (logging ALL calls)" : "");
   preload();
   selftest();
}

__attribute__((used)) static struct { const void *repl, *orig; }
interposers[] __attribute__((section("__DATA,__interpose"))) = {
   { (const void *)p_clear,                  (const void *)glClear },
   { (const void *)p_clearcolor,             (const void *)glClearColor },
   { (const void *)p_viewport,               (const void *)glViewport },
   { (const void *)p_scissor,                (const void *)glScissor },
   { (const void *)p_enable,                 (const void *)glEnable },
   { (const void *)p_disable,                (const void *)glDisable },
   { (const void *)p_depthmask,              (const void *)glDepthMask },
   { (const void *)p_activetexture,          (const void *)glActiveTexture },
   { (const void *)p_bindtexture,            (const void *)glBindTexture },
   { (const void *)p_useprogram,             (const void *)glUseProgram },
   { (const void *)p_finish,                 (const void *)glFinish },
   { (const void *)p_begin,                  (const void *)glBegin },
   { (const void *)p_end,                    (const void *)glEnd },
   { (const void *)p_vertex3f,               (const void *)glVertex3f },
   { (const void *)p_texcoord2f,             (const void *)glTexCoord2f },
   { (const void *)p_getintegerv,            (const void *)glGetIntegerv },
   { (const void *)p_getstring,              (const void *)glGetString },
   { (const void *)p_gettexlevelparameteriv, (const void *)glGetTexLevelParameteriv },
   { (const void *)p_bindfb,                 (const void *)glBindFramebufferEXT },
   { (const void *)p_bindrb,                 (const void *)glBindRenderbufferEXT },
   { (const void *)p_genfb,                  (const void *)glGenFramebuffersEXT },
   { (const void *)p_genrb,                  (const void *)glGenRenderbuffersEXT },
   { (const void *)p_delfb,                  (const void *)glDeleteFramebuffersEXT },
   { (const void *)p_delrb,                  (const void *)glDeleteRenderbuffersEXT },
   { (const void *)p_fbrb,                   (const void *)glFramebufferRenderbufferEXT },
   { (const void *)p_fbtex2d,                (const void *)glFramebufferTexture2DEXT },
   { (const void *)p_rbstorage,              (const void *)glRenderbufferStorageEXT },
   { (const void *)p_rbstorage_ms,           (const void *)glRenderbufferStorageMultisampleEXT },
   { (const void *)p_blitfb,                 (const void *)glBlitFramebufferEXT },
   { (const void *)p_choosepf,               (const void *)CGLChoosePixelFormat },
   { (const void *)p_createctx,              (const void *)CGLCreateContext },
   { (const void *)p_destroyctx,             (const void *)CGLDestroyContext },
   { (const void *)p_destroypf,              (const void *)CGLDestroyPixelFormat },
   { (const void *)p_setcurrent,             (const void *)CGLSetCurrentContext },
   { (const void *)p_getcurrent,             (const void *)CGLGetCurrentContext },
   { (const void *)p_enable_cgl,             (const void *)CGLEnable },
   { (const void *)p_disable_cgl,            (const void *)CGLDisable },
   { (const void *)p_setparam,               (const void *)CGLSetParameter },
   { (const void *)p_getparam,               (const void *)CGLGetParameter },
   { (const void *)p_queryrend,              (const void *)CGLQueryRendererInfo },
   { (const void *)p_describerend,           (const void *)CGLDescribeRenderer },
   { (const void *)p_destroyrend,            (const void *)CGLDestroyRendererInfo },
};
