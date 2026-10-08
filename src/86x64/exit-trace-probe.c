/* exit-trace-probe.c — ONE job: name WHO terminated the process, and with what status.
 *
 * For the failure mode "the translated app exits cleanly and says nothing", the
 * OS gives us no crash report (there was no fault) and no output (the app never
 * spewed). The one fact that identifies the bail-out is the RETURN ADDRESS of
 * whoever called exit(), which is still on the stack when the call arrives.
 *
 * So: interpose the termination entry points, resolve the immediate caller and
 * the whole callable backtrace against the loaded images, then terminate with
 * the SAME status so the observed behaviour is unchanged.
 *
 * Frames inside TRANSLATED code will not unwind (there is no x86_64 unwind info
 * for them), so the printed backtrace may stop at the first translated frame —
 * but the immediate caller, resolved via __builtin_return_address, is reported
 * separately and is the fact that matters. Feed a translated address back through
 * `src/86x64/pcmap-diff.py` to get the original i386 site.
 *
 * build:
 *   clang -dynamiclib -O0 -g -o /tmp/exit_trace.dylib src/86x64/exit-trace-probe.c
 * use:
 *   DYLD_INSERT_LIBRARIES=/tmp/exit_trace.dylib <app>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <execinfo.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/syscall.h>

static void describe(const char *label, void *addr) {
   Dl_info info;
   if (addr != NULL && dladdr(addr, &info) != 0 && info.dli_fname != NULL) {
      const char *base = strrchr(info.dli_fname, '/');
      base = base ? base + 1 : info.dli_fname;
      fprintf(stderr, "[exit-trace]   %s %p  %s+0x%lx  %s\n", label, addr, base,
              (unsigned long)((char *)addr - (char *)info.dli_fbase),
              info.dli_sname ? info.dli_sname : "(no symbol)");
   } else {
      /* No image owns it: a translated low-4GB address, or a wild pointer. */
      fprintf(stderr, "[exit-trace]   %s %p  <no image>\n", label, addr);
   }
}

/* Scan the stack for return addresses that land inside a LOADED IMAGE.
 *
 * backtrace() stops at the first translated frame — translated code carries no
 * x86_64 unwind info — so the frame that actually decided to exit is invisible to
 * it. But the `call` that reached the bridge still pushed its return address, and
 * translated code runs on a low-4GB stack, so the site is sitting in plain words
 * a short way up the stack. Resolve every plausible word against the images and
 * print the ones that belong to a NON-system image: that set contains the
 * translated call site. (Same principle as fault_report_shim.c, which recovers
 * the caller of a null-pointer call from [rsp].)
 */
static void scan_stack_for_sites(int words) {
   void **sp = (void **)__builtin_frame_address(0);
   fprintf(stderr, "[exit-trace] stack scan for in-image return addresses:\n");
   int found = 0;
   for (int i = 0; i < words; ++i) {
      void *w = sp[i];
      if (w == NULL) { continue; }
      Dl_info info;
      if (dladdr(w, &info) == 0 || info.dli_fname == NULL) { continue; }
      /* Only report sites in the APP's own images; the system frameworks and our
       * own probe are noise here. */
      if (strstr(info.dli_fname, "/usr/lib/") != NULL ||
          strstr(info.dli_fname, "/System/") != NULL ||
          strstr(info.dli_fname, "exit_trace") != NULL) { continue; }
      char lbl[24];
      snprintf(lbl, sizeof lbl, "[rsp+0x%x]", (unsigned)(i * sizeof(void *)));
      describe(lbl, w);
      ++found;
   }
   if (found == 0) { fprintf(stderr, "[exit-trace]   (no in-image words found)\n"); }
}

static void report(const char *who, int status, void *caller) {
   fprintf(stderr, "\n[exit-trace] ===== %s(%d) =====\n", who, status);
   describe("caller:", caller);

   void *bt[64];
   const int n = backtrace(bt, 64);
   fprintf(stderr, "[exit-trace] backtrace (%d frames):\n", n);
   for (int i = 0; i < n; ++i) {
      char lbl[16];
      snprintf(lbl, sizeof lbl, "#%-2d", i);
      describe(lbl, bt[i]);
   }
   {
      const char *w = getenv("EXIT_TRACE_WORDS");
      scan_stack_for_sites(w ? atoi(w) : 256);
   }
   fflush(stderr);
}

/* Terminate via the raw syscall: calling exit()/_exit() here would re-enter our
 * own interposed symbol. Status is preserved, so the caller sees what it would
 * have seen; atexit handlers are skipped, which is acceptable for a probe that
 * only runs on a process already committed to dying. */
static void finish(int status) {
   syscall(SYS_exit, status);
   __builtin_unreachable();
}

static void probe_exit(int status) {
   report("exit", status, __builtin_return_address(0));
   finish(status);
}

static void probe_uexit(int status) {
   report("_exit", status, __builtin_return_address(0));
   finish(status);
}

static void probe_Exit(int status) {
   report("_Exit", status, __builtin_return_address(0));
   finish(status);
}

static void probe_abort(void) {
   report("abort", 134, __builtin_return_address(0));
   finish(134);
}

__attribute__((used)) static struct {
   const void *replacement;
   const void *replacee;
} interposers[] __attribute__((section("__DATA,__interpose"))) = {
   { (const void *)&probe_exit,  (const void *)&exit  },
   { (const void *)&probe_uexit, (const void *)&_exit },
   { (const void *)&probe_Exit,  (const void *)&_Exit },
   { (const void *)&probe_abort, (const void *)&abort },
};
