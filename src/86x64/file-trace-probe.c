/*
 * file-trace-probe.c — name every file a translated app actually tries to open.
 *
 * WHY THIS EXISTS (Portal 2 2026-09-13)
 * -------------------------------------
 * libabiconv's POSIX_TRACE covers dlopen/dlsym/open/fcntl only, and Source's
 * CBaseFileSystem does NOT use open(2): filesystem_stdio.dylib imports
 * ___fopen / ___stat / ___fread / ___fseek (abigen bridges). So a POSIX_TRACE
 * run showed exactly ONE open() -- a lock file -- and no game content at all,
 * which reads as "the filesystem never tried" when in truth the trace was blind
 * to the entire path. ⚠ An absence in a trace is only evidence if the trace
 * covers the call the code actually makes; check the IMPORTS first (nm -mu).
 *
 * WHY AN INTERPOSE PROBE RATHER THAN A SHIM IN libabiconv: two-level namespace
 * means a definition inside libabiconv does not rebind anyone else's imports,
 * and libabiconv's OWN __interpose entries do not rebind libabiconv's own stubs
 * either (see src/86x64/interpose.c's note). A SEPARATE inserted image's
 * __DATA,__interpose DOES rebind libabiconv's bridges, which is exactly what we
 * need to see the abigen fopen/stat calls. It also keeps a pure diagnostic out
 * of the shipped runtime.
 *
 * usage:
 *   run-portal2.sh --insert .../file-trace-probe.dylib [--env FTRACE_LOG=/path]
 *     FTRACE_LOG=<path>   write there instead of stderr
 *     FTRACE_FAILONLY=1   log ONLY failures (the usual question: what is missing)
 *     FTRACE_FILTER=<s>   log only paths containing <s>
 *
 * build (MUST be x86_64 or dyld ignores it SILENTLY for a Rosetta target):
 *   clang -arch x86_64 -dynamiclib -o file-trace-probe.dylib file-trace-probe.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

typedef struct { const void *replacement; const void *replacee; } interpose_t;
#define INTERPOSE(new, old) \
   __attribute__((used)) static const interpose_t interpose_##old \
   __attribute__((section("__DATA,__interpose"))) = \
      { (const void *)(unsigned long)&new, (const void *)(unsigned long)&old }

static FILE *g_out;
static int   g_failonly;
static const char *g_filter;
static int   g_ready;

/* Re-entrancy guard: our own fopen of the log file would recurse forever, and a
 * failing log open must not look like an application file access. */
static __thread int g_in;

static void ft_init(void) {
   if (g_ready) return;
   g_ready = 1;
   g_in = 1;
   const char *p = getenv("FTRACE_LOG");
   if (p && *p) g_out = fopen(p, "w");
   if (!g_out) g_out = stderr;
   g_failonly = getenv("FTRACE_FAILONLY") ? 1 : 0;
   g_filter = getenv("FTRACE_FILTER");
   fprintf(g_out, "[ftrace] armed: fopen/freopen/open/stat/lstat/access%s%s\n",
           g_failonly ? " (failures only)" : "",
           g_filter ? " filter=" : "");
   if (g_filter) fprintf(g_out, "[ftrace] filter=%s\n", g_filter);
   fflush(g_out);
   g_in = 0;
}

/* ★Always log an ARM line (above) and always say whether a call FAILED. A probe
 * that prints nothing is indistinguishable from a probe that never loaded. */
static void ft_log(const char *fn, const char *path, int ok, long extra) {
   if (g_in) return;
   g_in = 1;
   ft_init();
   if (!path) path = "(null)";
   if (g_filter && !strstr(path, g_filter)) { g_in = 0; return; }
   if (g_failonly && ok) { g_in = 0; return; }
   if (ok)
      fprintf(g_out, "[ftrace] %-7s %s\n", fn, path);
   else
      fprintf(g_out, "[ftrace] %-7s %s   >>> FAILED errno=%ld (%s)\n",
              fn, path, extra, strerror((int)extra));
   fflush(g_out);
   g_in = 0;
}

static FILE *ft_fopen(const char *path, const char *mode) {
   FILE *r = fopen(path, mode);
   ft_log("fopen", path, r != NULL, (long)errno);
   return r;
}
INTERPOSE(ft_fopen, fopen);

static FILE *ft_freopen(const char *path, const char *mode, FILE *s) {
   FILE *r = freopen(path, mode, s);
   ft_log("freopen", path, r != NULL, (long)errno);
   return r;
}
INTERPOSE(ft_freopen, freopen);

static int ft_open(const char *path, int flags, ...) {
   /* mode is only consulted for O_CREAT; passing it unconditionally is safe on
    * Darwin's varargs open. */
   mode_t m = 0;
   if (flags & O_CREAT) {
      __builtin_va_list ap;
      __builtin_va_start(ap, flags);
      m = (mode_t)__builtin_va_arg(ap, int);
      __builtin_va_end(ap);
   }
   int r = open(path, flags, m);
   ft_log("open", path, r >= 0, (long)errno);
   return r;
}
INTERPOSE(ft_open, open);

static int ft_stat(const char *path, struct stat *sb) {
   int r = stat(path, sb);
   ft_log("stat", path, r == 0, (long)errno);
   return r;
}
INTERPOSE(ft_stat, stat);

static int ft_lstat(const char *path, struct stat *sb) {
   int r = lstat(path, sb);
   ft_log("lstat", path, r == 0, (long)errno);
   return r;
}
INTERPOSE(ft_lstat, lstat);

static int ft_access(const char *path, int mode) {
   int r = access(path, mode);
   ft_log("access", path, r == 0, (long)errno);
   return r;
}
INTERPOSE(ft_access, access);

__attribute__((constructor))
static void ft_ctor(void) { ft_init(); }
