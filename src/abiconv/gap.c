// gap.c — ONE job: report the first hit of a silent gap (see gap.h).
//
// A shim that answers "success" without doing the work, or a native it resolves
// by name that this OS no longer exports, fails SILENTLY: the app carries on with
// a zero and breaks somewhere far away (Halo's checkbox read 0 for a day because
// Get/SetControl32BitValue are gone). On its first hit each site
//   * writes one stderr line: kind, symbol, and the caller as <image>+0xOFFSET
//     (the i386 return address, fault-symbolize-able);
//   * appends the same line to the reach ledger
//     $TMPDIR/86x64-reach/<prog>.<pid>.txt — what REAL runs reach, which is the
//     work queue (coverage-audit.py --reach <dir>); imported-but-unreached gaps
//     stay as they are.
// Kinds: stub (constant-return shim), dlsym (by-name lookup found nothing),
// raw (a translated image calls a NATIVE function with no bridge: the 4-byte
// push vs 8-byte ret family, the unbridged native-call bug), byname (an i386
// dlsym/CFBundle lookup got the generic int-only marshalling thunk, no bridge).
// raw is caught by a one-shot stub on every call slot that holds a native target
// once the image is bound (x64_gap_arm_raw_slots + gap_tramp.asm): dyld4 binds
// even lazy slots eagerly at load, so the lazy binder never sees them.
// Env (read once): M64_GAP=0 silences everything and arms no stubs,
// M64_GAP=abort aborts after reporting. Correct no-ops are not marked GAP_STUB at all; they
// are listed in src/86x64/coverage-audit.ok with the reason.
// Raw write(2), no stdio/malloc: libabiconv's malloc is the i386 heap shim.
#include "gap.h"
#include "dyld_image_list.h"
#include <dlfcn.h>
#include <mach-o/loader.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int gap_mode(void) {   // 0 off, 1 report, 2 report + abort
   static int mode = -1;
   if (mode < 0) {
      const char *e = getenv("M64_GAP");
      mode = !e ? 1 : !strcmp(e, "0") ? 0 : !strcmp(e, "abort") ? 2 : 1;
   }
   return mode;
}

void x64_gap_hit(const char *kind, const char *sym, const char *who, uint32_t caller) {
   const int mode = gap_mode();
   if (!mode) return;

   char where[256] = "";
   Dl_info di;
   if (caller && dladdr((void *)(uintptr_t)caller, &di) && di.dli_fname) {
      const char *b = strrchr(di.dli_fname, '/');
      snprintf(where, sizeof where, "%s+0x%lx", b ? b + 1 : di.dli_fname,
               (unsigned long)((uintptr_t)caller - (uintptr_t)di.dli_fbase));
   } else if (caller) {
      snprintf(where, sizeof where, "0x%x", caller);
   } else if (who) {
      snprintf(where, sizeof where, "%s", who);
   }
   char line[512];
   int n = snprintf(line, sizeof line, "%s %s %s\n", kind, sym, where);
   if (n <= 0) return;
   if ((size_t)n >= sizeof line) n = sizeof line - 1;

   char msg[600];
   int m = snprintf(msg, sizeof msg, "abiconv: GAP (silent no-op reached) %s", line);
   if (m > 0) write(2, msg, (size_t)m < sizeof msg ? (size_t)m : sizeof msg - 1);

   const char *tmp = getenv("TMPDIR");
   char path[1024];
   snprintf(path, sizeof path, "%s/86x64-reach", tmp && *tmp ? tmp : "/tmp");
   mkdir(path, 0755);
   size_t l = strlen(path);
   snprintf(path + l, sizeof path - l, "/%s.%d.txt", getprogname(), getpid());
   int fd = open(path, O_WRONLY | O_APPEND | O_CREAT, 0644);
   if (fd >= 0) { write(fd, line, (size_t)n); close(fd); }

   if (mode == 2) abort();
}

// ---- raw native calls ------------------------------------------------------

int x64_gap_i386_callable(const void *addr) {
   static const void *self;
   Dl_info di;
   if (!self && dladdr((void *)(uintptr_t)&x64_gap_i386_callable, &di)) self = di.dli_fbase;
   if (!dladdr(addr, &di) || !di.dli_fbase) return 1;   // JIT stub / unplaceable: not ours to judge
   // any libabiconv copy is the boundary itself (a bundle may map several)
   return di.dli_fbase == self || (di.dli_fname && strstr(di.dli_fname, "libabiconv"))
          || x64_img_is_translated(di.dli_fbase);
}

struct gap_raw { const char *name; uint64_t *slot; uint64_t target; };
extern char x64_gap_raw_tramp[];

void x64_gap_raw_hit(struct gap_raw *r, uint32_t caller) {
   *r->slot = r->target;                     // later calls go straight to the native
   x64_gap_hit("raw", r->name, 0, caller);
}

// 24-byte stub + its 24-byte record, carved from RWX chunks (same recipe as
// objc_slide.c make_init_stub). Never freed: one per raw slot, a few hundred.
static struct gap_raw *raw_stub(const char *name, uint64_t *slot, uint64_t target) {
   static uint8_t *region; static size_t used, cap;
   if (!region || used + 48 > cap) {
      void *m = mmap(NULL, 1 << 16, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANON | MAP_JIT, -1, 0);
      if (m == MAP_FAILED)
         m = mmap(NULL, 1 << 16, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANON, -1, 0);
      if (m == MAP_FAILED) return NULL;
      region = m; used = 0; cap = 1 << 16;
   }
   uint8_t *s = region + used;
   used += 48;
   struct gap_raw *r = (struct gap_raw *)(s + 24);
   r->name = name; r->slot = slot; r->target = target;
   uint64_t rec = (uint64_t)(uintptr_t)r, tramp = (uint64_t)(uintptr_t)x64_gap_raw_tramp;
   s[0] = 0x49; s[1] = 0xBB; memcpy(s + 2, &rec, 8);                        // movabs r11, rec
   s[10] = 0xFF; s[11] = 0x25; memset(s + 12, 0, 4); memcpy(s + 16, &tramp, 8); // jmp [rip+0]
   return r;
}

// Called once per TRANSLATED image at add-image time (objc_slide.c), after dyld
// bound it: every call slot already holding a native target gets a one-shot stub.
// A slot still on the image's own stub helper (unresolved; import_repair.c) is
// translated code and left alone.
void x64_gap_arm_raw_slots(const struct mach_header_64 *mh, intptr_t slide) {
   if (!gap_mode()) return;
   const uint8_t *p = (const uint8_t *)(mh + 1);
   for (uint32_t i = 0; i < mh->ncmds; i++, p += ((const struct load_command *)p)->cmdsize) {
      const struct segment_command_64 *sg = (const void *)p;
      if (sg->cmd != LC_SEGMENT_64 || !(sg->initprot & VM_PROT_WRITE)) continue;
      const struct section_64 *sc = (const void *)(sg + 1);
      for (uint32_t k = 0; k < sg->nsects; k++, sc++) {
         if (strncmp(sc->sectname, "__la_symbol_ptr", 16) && strncmp(sc->sectname, "__jt_ptrs", 16)) continue;
         uint64_t *slot = (uint64_t *)(uintptr_t)(sc->addr + slide);
         for (uint64_t n = sc->size / 8; n--; slot++) {
            const void *t = (const void *)(uintptr_t)*slot;
            Dl_info di;
            if (!t || x64_gap_i386_callable(t) || !dladdr(t, &di) || !di.dli_sname) continue;
            struct gap_raw *r = raw_stub(di.dli_sname, slot, *slot);
            if (r) *slot = (uint64_t)(uintptr_t)r - 24;
         }
      }
   }
}
