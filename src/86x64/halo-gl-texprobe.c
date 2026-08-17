/* halo-gl-texprobe.c — DIAGNOSTIC-ONLY: log every texture upload and the
 * pixel-store state in force when it happens.
 *
 * WHY A SEPARATE DYLIB. Per the one-shim-one-job rule this does not belong in
 * libabiconv: it is a temporary instrument for ONE question, and it must be
 * removable by deleting a file and dropping an env var. It is inserted with
 * DYLD_INSERT_LIBRARIES and interposes the real GL entry points, which works
 * because the abigen bridge `___glTexSubImage2D` reaches OpenGL through an
 * ordinary symbol stub — so this needs no cooperation from the shim layer and
 * no rebuild of anything else.
 *
 * THE QUESTION. Halo's ANIMATED menu background textures render as a DIAGONAL
 * SHEAR while pre-rendered stills, the logo and all text are correct. A shear is
 * the signature of a ROW-PITCH mismatch: rows written assuming pitch P into a
 * surface of pitch P' displace row n by n*(P-P'). The candidates are
 *   (a) wrong width/height/format reaching glTex(Sub)Image2D,
 *   (b) a GL_UNPACK_* pixel-store setting that is wrong or never applied,
 *   (c) correct GL parameters but data laid out wrong by a mistranslated
 *       decode/copy loop upstream — in which case (a) and (b) look perfect.
 * Logging the arguments AND the unpack state separates (a)/(b) from (c) in one
 * run, which is the whole point: (c) needs a completely different hunt.
 *
 * ⚠It also matters that the pre-rendered stills are the CONTROL. If the broken
 * and working textures come through the same call with the same unpack state,
 * that is itself the finding.
 *
 * Build + run:  bash src/86x64/halo-gl-texprobe.sh
 */
#include <OpenGL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *out;
static int   budget = 400;   /* a menu re-uploads constantly; keep it readable */

static void tp_open(void) {
   if (out) return;
   const char *p = getenv("HALO_TEXPROBE_LOG");
   out = p ? fopen(p, "w") : stderr;
   if (!out) out = stderr;
}

/* The pixel-store state that decides how a row is walked. If any of these is
 * unexpected, the shear is explained without looking at the data at all. */
static void tp_unpack(char *buf, size_t n) {
   GLint row = -1, align = -1, skipp = -1, skipr = -1;
   glGetIntegerv(GL_UNPACK_ROW_LENGTH,  &row);
   glGetIntegerv(GL_UNPACK_ALIGNMENT,   &align);
   glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skipp);
   glGetIntegerv(GL_UNPACK_SKIP_ROWS,   &skipr);
   snprintf(buf, n, "unpack{row_len=%d align=%d skip_px=%d skip_rows=%d}",
            row, align, skipp, skipr);
}

/* Announce at load. Without this, an empty log is ambiguous: it could mean the
 * uploads bypass these symbols (interesting) or that DYLD_INSERT_LIBRARIES was
 * ignored because of library validation (not interesting, and a different fix).
 * A probe that cannot tell those apart wastes a launch. */
__attribute__((constructor)) static void tp_hello(void) {
   tp_open();
   fprintf(out, "[tex] probe loaded (interposing TexImage2D/TexSubImage2D/"
                "CompressedTexImage2D/PixelStorei)\n");
   fflush(out);
}

static const char *fmt_name(GLenum f) {
   switch (f) {
      case GL_RGBA: return "RGBA"; case GL_RGB: return "RGB";
      case GL_BGRA: return "BGRA"; case GL_LUMINANCE: return "LUM";
      case GL_LUMINANCE_ALPHA: return "LUM_A"; case GL_ALPHA: return "ALPHA";
      default: return "?";
   }
}

#define TP_LOG(...) do { tp_open(); if (budget > 0) { budget--; \
   fprintf(out, __VA_ARGS__); fflush(out); } } while (0)

static void tp_teximage2d(GLenum t, GLint l, GLint ifmt, GLsizei w, GLsizei h,
                          GLint b, GLenum f, GLenum ty, const GLvoid *px) {
   char u[160] = "";
   if (budget > 0) tp_unpack(u, sizeof u);   /* glGetIntegerv is a sync point */
   TP_LOG("[tex] TexImage2D    lvl=%d ifmt=%#x %dx%d border=%d fmt=%#x(%s) type=%#x px=%p %s\n",
          l, ifmt, w, h, b, f, fmt_name(f), ty, px, u);
   glTexImage2D(t, l, ifmt, w, h, b, f, ty, px);
}

static void tp_texsubimage2d(GLenum t, GLint l, GLint x, GLint y, GLsizei w,
                             GLsizei h, GLenum f, GLenum ty, const GLvoid *px) {
   char u[160] = "";
   if (budget > 0) tp_unpack(u, sizeof u);
   TP_LOG("[tex] TexSubImage2D lvl=%d at(%d,%d) %dx%d fmt=%#x(%s) type=%#x px=%p %s\n",
          l, x, y, w, h, f, fmt_name(f), ty, px, u);
   glTexSubImage2D(t, l, x, y, w, h, f, ty, px);
}

/* ★OBJECTIVE CAPTURE (HALO_TEXPROBE_DUMP=<dir>).
 *
 * The client-storage experiment's verdict is VISUAL, which means it depends on
 * a human describing a screen. We can do better: dump the exact bytes Halo
 * hands to GL and decode them OFFLINE. If the blob decodes to a coherent image
 * at the declared dimensions, the data Halo produced is correct and the
 * corruption is downstream (driver / client storage). If the blob is ALREADY
 * sheared, the defect is upstream in translated Halo code and no GL-side change
 * can fix it. That is candidate (c), settled without anyone squinting at a
 * menu.
 *
 * Only level 0 and only the first few, since a mip chain adds nothing here and
 * the top level is the one large enough to see structure in. */
static void tp_dump(GLint l, GLenum ifmt, GLsizei w, GLsizei h, GLsizei sz,
                    const GLvoid *px) {
   static int n;
   const char *dir = getenv("HALO_TEXPROBE_DUMP");
   if (!dir || l != 0 || n >= 6 || !px || sz <= 0) return;
   char path[1024];
   snprintf(path, sizeof path, "%s/tex%d_%dx%d_fmt%x.dxt", dir, n, w, h, ifmt);
   FILE *f = fopen(path, "wb");
   if (!f) return;
   fwrite(px, 1, (size_t)sz, f);
   fclose(f);
   TP_LOG("[tex] DUMPED %s (%d bytes)\n", path, sz);
   n++;
}

static void tp_compressed_teximage2d(GLenum t, GLint l, GLenum ifmt, GLsizei w,
                                     GLsizei h, GLint b, GLsizei sz,
                                     const GLvoid *px) {
   tp_dump(l, ifmt, w, h, sz, px);
   /* Compressed uploads carry their own implied pitch, so a shear here would
    * mean the BLOCK layout is wrong rather than the row length. Worth telling
    * apart, hence a distinct line. */
   TP_LOG("[tex] CompressedTexImage2D lvl=%d ifmt=%#x %dx%d border=%d bytes=%d px=%p\n",
          l, ifmt, w, h, b, sz, px);
   glCompressedTexImage2D(t, l, ifmt, w, h, b, sz, px);
}

/* ★THE EXPERIMENT (HALO_TEXPROBE_NO_CLIENT_STORAGE=1).
 *
 * Every one of Halo's PixelStorei calls is GL_UNPACK_CLIENT_STORAGE_APPLE
 * (0x85b2), set to 1 — 92 times in the measured run. That extension tells the
 * driver NOT to copy the texture: it keeps a pointer into the application's own
 * memory and reads it later, on the app's promise that the buffer stays alive
 * and unmodified at a layout the driver can use directly.
 *
 * That promise is exactly the kind a TRANSLATED process may not keep. The data
 * lives in the i386 low-4GB shadow, reached through our mapping of
 * bitmaps.map; if its alignment, lifetime or backing differs from what the
 * driver assumes when it reads client memory directly, the result is corrupt
 * texels while every GL PARAMETER stays perfectly correct — which is precisely
 * what we measured (byte counts exact, geometry exact, unpack state default).
 *
 * Forcing the flag to 0 makes GL copy the data at call time instead. If the
 * textures then render correctly, client storage is the cause and the fix is a
 * real one; if they still shear, the extension is exonerated and the defect is
 * in the bytes themselves. Either way one look at the screen decides it. */
static void tp_pixelstorei(GLenum pname, GLint param) {
   static int force_off = -1;
   if (force_off < 0)
      force_off = getenv("HALO_TEXPROBE_NO_CLIENT_STORAGE") != NULL;
   if (force_off && pname == GL_UNPACK_CLIENT_STORAGE_APPLE && param != 0) {
      TP_LOG("[tex] PixelStorei    pname=%#x param=%d -> FORCED 0 (experiment)\n",
             pname, param);
      glPixelStorei(pname, 0);
      return;
   }
   TP_LOG("[tex] PixelStorei    pname=%#x param=%d\n", pname, param);
   glPixelStorei(pname, param);
}

__attribute__((used)) static struct { const void *repl, *orig; }
interposers[] __attribute__((section("__DATA,__interpose"))) = {
   { (const void *)tp_teximage2d,            (const void *)glTexImage2D },
   { (const void *)tp_texsubimage2d,         (const void *)glTexSubImage2D },
   { (const void *)tp_compressed_teximage2d, (const void *)glCompressedTexImage2D },
   { (const void *)tp_pixelstorei,           (const void *)glPixelStorei },
};
