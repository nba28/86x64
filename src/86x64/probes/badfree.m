/* badfree.m — catch the exact pointer behind a malloc abort, and say WHOSE it is.
 *
 * WHY THIS EXISTS. After the CFRange clamp (f1d4ee4) let Civilization IV past its
 * NSRangeException, it dies further in with a malloc abort inside QuickTime:
 *
 *     abort <- malloc_report <- malloc_vreport
 *          <- CarbonCore CSMemDisposePtr <- CSMemDisposeHandle
 *          <- QuickTime QTNewDataReferenceFromFSRef_priv
 *
 * and the diagnostic string malloc would normally print is nowhere to be found:
 * not in the .ips `asi` (which carries only "abort() called"), not on the
 * process's stderr under `open --stderr`, and not in os_log. So the one fact
 * that decides the diagnosis — WHICH pointer, and where it came from — is
 * unavailable from the report.
 *
 * This dylib recovers it by interposing free() and inspecting the pointer BEFORE
 * libmalloc gets a chance to abort:
 *
 *   * malloc_size(p) == 0  =>  p was never handed out by this malloc zone. That
 *     is the abort condition, caught one frame early and with the pointer value
 *     in hand.
 *
 *   * ★p < 4GB  =>  p is an I386-SIDE pointer. libabiconv keeps translated-world
 *     allocations in the low 4GB (malloc_shim.c) because the 32-bit code can only
 *     hold 4-byte pointers; native frameworks allocate above it. So a sub-4GB
 *     pointer reaching native CarbonCore's disposer means a Handle crossed from
 *     OUR world into native cleanup — the allocator-mismatch hypothesis — and it
 *     would be freed by the wrong allocator even if malloc did not notice.
 *     carbon_memory.c hands out `user` pointers that sit sizeof(struct hblk_hdr)
 *     BEYOND the real malloc block, so a native free(user) can never match.
 *
 *   * neither  =>  the pointer is a legitimate native block being double-freed or
 *     freed after corruption, and the defect is upstream of this call.
 *
 * The distinction matters because it picks the fix: a low-4GB pointer means our
 * Handle representation is leaking into native code, while a valid native
 * pointer means something scribbled on the heap earlier and this is only where
 * malloc noticed.
 *
 * BADFREE_SURVIVE=1 additionally SKIPS the bad free instead of calling through.
 * That is a diagnostic, not a fix: it says whether this is one isolated bad
 * pointer (the app then runs on) or the first of a systemic mismatch (it will
 * abort again immediately). Never ship behaviour derived from it without
 * fixing the actual ownership bug.
 *
 * Build (must be x86_64 — the translated app is x86_64 under Rosetta):
 *   clang -arch x86_64 -dynamiclib -framework Foundation -o badfree.dylib badfree.m
 * Use:
 *   open --env DYLD_INSERT_LIBRARIES=/path/badfree.dylib --stderr /tmp/log <App>
 */

#import <Foundation/Foundation.h>
#include <malloc/malloc.h>
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BF(...) do { fprintf(stderr, "[badfree] " __VA_ARGS__); } while (0)

static int survive(void)
{
   static int v = -1;
   if (v < 0) { const char *e = getenv("BADFREE_SURVIVE"); v = (e && *e && *e != '0'); }
   return v;
}

static void dump_frames(void)
{
   void *fr[24];
   int n = backtrace(fr, 24);
   char **sym = backtrace_symbols(fr, n);
   if (!sym) return;
   /* skip 0/1 (this function and the interpose itself) */
   for (int i = 2; i < n && i < 14; i++) BF("    %s\n", sym[i]);
   free(sym);   /* the REAL free: we are past the interpose for this pointer */
}

static void bf_free(void *p)
{
   if (p) {
      size_t sz = malloc_size(p);
      if (sz == 0) {
         const int low = ((uintptr_t)p < 0x100000000ULL);
         BF("*** BAD FREE *** p=%p  malloc_size=0  %s\n", p,
            low ? "★LOW 4GB — an i386-side pointer reached a NATIVE disposer"
                : "not a live block of this zone (double free / corruption)");
         dump_frames();
         if (survive()) { BF("    BADFREE_SURVIVE=1 — skipping this free\n"); return; }
         BF("    calling through; libmalloc will abort now\n");
      }
   }
   free(p);
}

__attribute__((used)) static struct {
   const void *replacement;
   const void *replacee;
} interposers[] __attribute__((section("__DATA,__interpose"))) = {
   { (const void *)&bf_free, (const void *)&free },
};

__attribute__((constructor)) static void bf_init(void)
{
   setvbuf(stderr, NULL, _IONBF, 0);
   BF("loaded — watching free() for non-live pointers%s\n",
      survive() ? " (SURVIVE mode: bad frees are skipped)" : "");
}
