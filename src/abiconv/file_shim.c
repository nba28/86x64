/*
 * file_shim.c — FILE-taking libc functions, with awareness of the wrapper's
 * shim FILE structs.
 *
 * The translated i386 binary's `__stderrp`/`__stdoutp`/`__stdinp` no longer
 * point at libsystem's real FILE globals (those live in high 4 GB and don't
 * survive i386's 32-bit pointer reads). The wrapper allocates a handful of
 * shim FILE structs in low 4 GB and patches the translated dylib's
 * __nl_symbol_ptr slots to point at them. The i386 code passes a shim
 * FILE* opaquely to libc functions like fprintf and fwrite.
 *
 * Each abiconv shim (`___fprintf` etc.) does the i386→x86_64 ABI lift and
 * then `call _fprintf`. By providing a `_fprintf` definition here we
 * intercept that call: if the FILE* arg is a shim, we substitute the real
 * libsystem FILE* (stored at offset 0 of the shim); otherwise we pass
 * through. The real libsystem functions are reached via dlsym(RTLD_NEXT)
 * to avoid the infinite recursion that calling `fprintf(...)` would cause.
 *
 * Coverage: every FILE*-taking libc entry point a target might call has to
 * be intercepted, or the raw shim struct reaches the real libc and faults.
 * The set below covers the standard <stdio.h> surface (output, input,
 * positioning, status, open/close/buffering). FILE*-returning openers
 * (fopen/fdopen/freopen) wrap their result in a fresh shim so the 32-bit
 * caller gets a usable low-4GB pointer.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>

struct shim_FILE {
   void *real_fp;
   uint32_t magic;
   int fd;
   char _pad[112];
};
#define SHIM_FILE_MAGIC 0x68690a55

extern struct shim_FILE *_86x64_stderr_shim __attribute__((weak));

static size_t (*real_fwrite)(const void *, size_t, size_t, FILE *);
static int    (*real_fputs)(const char *, FILE *);
static int    (*real_fputc)(int, FILE *);
static int    (*real_putc)(int, FILE *);
static size_t (*real_fread)(void *, size_t, size_t, FILE *);
static char  *(*real_fgets)(char *, int, FILE *);
static int    (*real_fgetc)(FILE *);
static int    (*real_getc)(FILE *);
static int    (*real_ungetc)(int, FILE *);
static int    (*real_feof)(FILE *);
static int    (*real_ferror)(FILE *);
static void   (*real_clearerr)(FILE *);
static int    (*real_fileno)(FILE *);
static int    (*real_fclose)(FILE *);
static int    (*real_fflush)(FILE *);
static int    (*real_fseek)(FILE *, long, int);
static long   (*real_ftell)(FILE *);
static int    (*real_fseeko)(FILE *, off_t, int);
static off_t  (*real_ftello)(FILE *);
static void   (*real_rewind)(FILE *);
static int    (*real_fgetpos)(FILE *, fpos_t *);
static int    (*real_fsetpos)(FILE *, const fpos_t *);
static int    (*real_setvbuf)(FILE *, char *, int, size_t);
static void   (*real_setbuf)(FILE *, char *);
static FILE  *(*real_fopen)(const char *, const char *);
static FILE  *(*real_fdopen)(int, const char *);
static FILE  *(*real_freopen)(const char *, const char *, FILE *);
static int    (*real_vfprintf)(FILE *, const char *, va_list);
static int    (*real_vfscanf)(FILE *, const char *, va_list);

/* Resolve one libc symbol for the pass-through pointers. RTLD_NEXT is the
 * canonical interposer idiom but it searches only the images AFTER the
 * CALLING image in dyld's order — for a co-located libabiconv copy loaded
 * late (the libabiconv multi-copy gotcha: bundles carry one per directory),
 * libSystem can precede it and RTLD_NEXT returns NULL. The wrappers then
 * `call NULL` on first use (observed: unwrap_obj_arg's OBJC_BRIDGE_TRACE
 * fprintf in Civ IV's second copy -> rip=0 SIGSEGV). Fall back to an
 * explicit libSystem handle — NOT RTLD_DEFAULT, which can find THIS image's
 * own interposing definition first and recurse. */
static void *file_shim_dl(const char *name) {
   void *p = dlsym(RTLD_NEXT, name);
   if (!p) {
      static void *libsystem;
      if (!libsystem) {
         libsystem = dlopen("/usr/lib/libSystem.B.dylib",
                            RTLD_LAZY | RTLD_NOLOAD);
      }
      if (!libsystem) {   /* NOLOAD can miss; libSystem is always loadable */
         libsystem = dlopen("/usr/lib/libSystem.B.dylib", RTLD_LAZY);
      }
      if (libsystem) { p = dlsym(libsystem, name); }
   }
   return p;
}

__attribute__((constructor))
static void file_shim_init(void) {
   real_fwrite    = file_shim_dl("fwrite");
   real_fputs     = file_shim_dl("fputs");
   real_fputc     = file_shim_dl("fputc");
   real_putc      = file_shim_dl("putc");
   real_fread     = file_shim_dl("fread");
   real_fgets     = file_shim_dl("fgets");
   real_fgetc     = file_shim_dl("fgetc");
   real_getc      = file_shim_dl("getc");
   real_ungetc    = file_shim_dl("ungetc");
   real_feof      = file_shim_dl("feof");
   real_ferror    = file_shim_dl("ferror");
   real_clearerr  = file_shim_dl("clearerr");
   real_fileno    = file_shim_dl("fileno");
   real_fclose    = file_shim_dl("fclose");
   real_fflush    = file_shim_dl("fflush");
   real_fseek     = file_shim_dl("fseek");
   real_ftell     = file_shim_dl("ftell");
   real_fseeko    = file_shim_dl("fseeko");
   real_ftello    = file_shim_dl("ftello");
   real_rewind    = file_shim_dl("rewind");
   real_fgetpos   = file_shim_dl("fgetpos");
   real_fsetpos   = file_shim_dl("fsetpos");
   real_setvbuf   = file_shim_dl("setvbuf");
   real_setbuf    = file_shim_dl("setbuf");
   real_fopen     = file_shim_dl("fopen");
   real_fdopen    = file_shim_dl("fdopen");
   real_freopen   = file_shim_dl("freopen");
   real_vfprintf  = file_shim_dl("vfprintf");
   real_vfscanf   = file_shim_dl("vfscanf");
   if (getenv("ABICONV_DEBUG")) {
      dprintf(2, "abiconv: file_shim_init real_fwrite=%p real_vfprintf=%p\n",
              (void *)real_fwrite, (void *)real_vfprintf);
   }
}

/* Lazy re-init: called from resolve_file (the chokepoint every FILE*-taking
 * wrapper passes through) and the FILE-returning openers, so a call that
 * precedes this copy's constructor (or a constructor whose RTLD_NEXT round
 * yielded NULLs) never jumps through a NULL pass-through pointer. Cheap:
 * one NULL test on the hot path. */
static void file_shim_ensure(void) {
   if (!real_vfprintf) { file_shim_init(); }
}

/* True if fp is one of our low-4GB shim FILE structs. Guards the magic read
 * against obviously-bad pointers (null page / misalignment) so a garbage
 * FILE* can't fault us before reaching the real libc. */
static int is_shim(FILE *fp) {
   uintptr_t addr = (uintptr_t)fp;
   if (addr < 0x1000 || addr >= 0x100000000UL) return 0;
   if (addr & 0x3) return 0;
   return ((struct shim_FILE *)fp)->magic == SHIM_FILE_MAGIC;
}

/*
 * If fp is a shim, return the real libsystem FILE *. Anything else
 * (NULL, a real high-4GB pointer, an unrecognized low-4GB pointer) is
 * returned unchanged.
 */
static FILE *resolve_file(FILE *fp) {
   file_shim_ensure();
   if (is_shim(fp)) return (FILE *)((struct shim_FILE *)fp)->real_fp;
   return fp;
}

/* Wrap a real FILE* in a fresh low-4GB shim so a 32-bit caller can hold it.
 * Falls back to returning the raw pointer if no low memory is available. */
extern void *_86x64_alloc_low_4gb(size_t) __attribute__((weak));

static FILE *wrap_file(FILE *real) {
   if (!real) return NULL;
   void *region = _86x64_alloc_low_4gb ? _86x64_alloc_low_4gb(sizeof(struct shim_FILE)) : NULL;
   if (!region) return real;   /* caller may truncate, but no better option */
   struct shim_FILE *shim = (struct shim_FILE *)region;
   shim->real_fp = real;
   shim->magic = SHIM_FILE_MAGIC;
   shim->fd = fileno(real);
   return (FILE *)shim;
}

/* ----- output ------------------------------------------------------------- */

/* NOTE: every wrapper calls file_shim_ensure() as its FIRST statement (not
 * just inside resolve_file): C argument-evaluation order is unspecified, so
 * `real_vfprintf(resolve_file(fp), ...)` may load the (still-NULL) function
 * pointer into a register BEFORE resolve_file's lazy init runs — observed as
 * `call 0` (rip=0) with a correctly-resolved FILE* already in rdi. */
int fprintf(FILE *fp, const char *fmt, ...) {
   file_shim_ensure();
   va_list ap; va_start(ap, fmt);
   int r = real_vfprintf(resolve_file(fp), fmt, ap);
   va_end(ap);
   return r;
}

int vfprintf(FILE *fp, const char *fmt, va_list ap) {
   file_shim_ensure();
   return real_vfprintf(resolve_file(fp), fmt, ap);
}

size_t fwrite(const void *buf, size_t size, size_t n, FILE *fp) {
   file_shim_ensure();
   return real_fwrite(buf, size, n, resolve_file(fp));
}

int fputs(const char *s, FILE *fp) { file_shim_ensure(); return real_fputs(s, resolve_file(fp)); }
int fputc(int c, FILE *fp)         { file_shim_ensure(); return real_fputc(c, resolve_file(fp)); }
int putc(int c, FILE *fp)          { file_shim_ensure(); return real_putc(c, resolve_file(fp)); }

/* ----- input -------------------------------------------------------------- */

size_t fread(void *buf, size_t size, size_t n, FILE *fp) {
   file_shim_ensure();
   return real_fread(buf, size, n, resolve_file(fp));
}

char *fgets(char *buf, int n, FILE *fp) { file_shim_ensure(); return real_fgets(buf, n, resolve_file(fp)); }
int   fgetc(FILE *fp)                   { file_shim_ensure(); return real_fgetc(resolve_file(fp)); }
int   getc(FILE *fp)                    { file_shim_ensure(); return real_getc(resolve_file(fp)); }
int   ungetc(int c, FILE *fp)           { file_shim_ensure(); return real_ungetc(c, resolve_file(fp)); }

int fscanf(FILE *fp, const char *fmt, ...) {
   file_shim_ensure();
   va_list ap; va_start(ap, fmt);
   int r = real_vfscanf(resolve_file(fp), fmt, ap);
   va_end(ap);
   return r;
}

int vfscanf(FILE *fp, const char *fmt, va_list ap) {
   file_shim_ensure();
   return real_vfscanf(resolve_file(fp), fmt, ap);
}

/* ----- positioning -------------------------------------------------------- */

int   fseek(FILE *fp, long off, int whence)  { file_shim_ensure(); return real_fseek(resolve_file(fp), off, whence); }
long  ftell(FILE *fp)                        { file_shim_ensure(); return real_ftell(resolve_file(fp)); }
int   fseeko(FILE *fp, off_t off, int whence){ file_shim_ensure(); return real_fseeko(resolve_file(fp), off, whence); }
off_t ftello(FILE *fp)                       { file_shim_ensure(); return real_ftello(resolve_file(fp)); }
void  rewind(FILE *fp)                        { file_shim_ensure(); real_rewind(resolve_file(fp)); }
int   fgetpos(FILE *fp, fpos_t *pos)         { file_shim_ensure(); return real_fgetpos(resolve_file(fp), pos); }
int   fsetpos(FILE *fp, const fpos_t *pos)   { file_shim_ensure(); return real_fsetpos(resolve_file(fp), pos); }

/* ----- status ------------------------------------------------------------- */

/* fflush was the ONE resolved-but-never-wrapped entry point in this file:
 * the abigen ___fflush shim's `call _fflush` reached the real libc with the
 * raw low-4GB shim struct, whose zeroed pad reads as _extra == NULL ->
 * flockfile -> pthread_mutex_lock(&NULL->fl_mutex) -> EXC_BAD_ACCESS at 0x8.
 * Any translated `fflush(stdout/stderr/fopen'd FILE*)` crashed (fflush(NULL)
 * was safe: resolve_file passes NULL through = libc flush-all). Guarded by
 * tests-i386 90_fflush_file. */
int fflush(FILE *fp) { file_shim_ensure(); return real_fflush(resolve_file(fp)); }

int  feof(FILE *fp)     { file_shim_ensure(); return real_feof(resolve_file(fp)); }
int  ferror(FILE *fp)   { file_shim_ensure(); return real_ferror(resolve_file(fp)); }
void clearerr(FILE *fp) { file_shim_ensure(); real_clearerr(resolve_file(fp)); }

int fileno(FILE *fp) {
   file_shim_ensure();
   if (is_shim(fp)) return ((struct shim_FILE *)fp)->fd;
   return real_fileno(fp);
}

/* ----- buffering ---------------------------------------------------------- */

int  setvbuf(FILE *fp, char *buf, int mode, size_t size) {
   file_shim_ensure();
   return real_setvbuf(resolve_file(fp), buf, mode, size);
}
void setbuf(FILE *fp, char *buf) { file_shim_ensure(); real_setbuf(resolve_file(fp), buf); }

/* ----- open / close ------------------------------------------------------- */

int fclose(FILE *fp) {
   file_shim_ensure();
   /*
    * Don't allow closing the three stdio shims — that would close the
    * process's stderr/stdout/stdin fds and break subsequent output for the
    * wrapper itself. Pass-through for anything else.
    */
   if (is_shim(fp)) {
      struct shim_FILE *shim = (struct shim_FILE *)fp;
      if (shim->fd <= 2) return 0;        /* refuse to close stdio */
      return real_fclose((FILE *)shim->real_fp);
   }
   return real_fclose(fp);
}

FILE *fopen(const char *path, const char *mode) {
   file_shim_ensure();
   return wrap_file(real_fopen(path, mode));
}

FILE *fdopen(int fd, const char *mode) {
   file_shim_ensure();
   return wrap_file(real_fdopen(fd, mode));
}

FILE *freopen(const char *path, const char *mode, FILE *fp) {
   file_shim_ensure();
   FILE *newreal = real_freopen(path, mode, resolve_file(fp));
   if (!newreal) return NULL;
   if (is_shim(fp)) {
      /* Redirect the existing shim in place so the caller's stored handle
       * (e.g. a patched __stdoutp slot) follows the reopen. */
      struct shim_FILE *shim = (struct shim_FILE *)fp;
      shim->real_fp = newreal;
      shim->fd = fileno(newreal);
      return fp;
   }
   return wrap_file(newreal);
}
