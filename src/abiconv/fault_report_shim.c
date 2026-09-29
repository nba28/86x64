// fault_report_shim.c — ONE job: make a fatal fault in translated i386 code SPEAK.
//
// ── Why this exists ─────────────────────────────────────────────────────────────
// A translated i386 process that dies of EXC_BAD_ACCESS gives us almost nothing to
// work with:
//
//   * The OS crash report keeps only ~3 frames and its unwinder cannot walk a
//     translated i386 frame at all, so the caller is lost. When the fault is
//     `rip = 0` (a call through a null function pointer — the signature of an
//     unbound lazy stub, an unpopulated vtable slot, or a callback that was never
//     installed) EVERY frame is lost, because there is no code at the pc to
//     unwind from.
//   * The user may never see the report: an app with its own top-level handler
//     eats the signal first, and a process launched from a shell writes nothing
//     to the terminal.
//
// But the fault CONTEXT still holds the answer. For a `rip = 0` fault the `call`
// that jumped there has already pushed its return address, so **[rsp] IS the call
// site**, verbatim. Translated i386 code pushes a 4-byte return address into a
// low-4GB stack, so the site is recoverable exactly, and printing it as
// `<image>+0xOFFSET` makes it directly disassemblable offline.
//
// So: catch the fault, dump the machine state and the top of the stack with every
// plausible code address resolved against the loaded images, then chain to
// whatever handler was there before so the process still dies exactly as it would
// have. Purely additive — this changes no behaviour and decides nothing.
//
// ── Why it is OFF by default ────────────────────────────────────────────────────
// ⚠ Some translator bugs in this project are ANY-OBSERVER HEISENBUGS: they depend
// on the byte content of the stack below rsp, so anything that writes there —
// lldb, or even our own fprintf tracing — perturbs them out of existence. A fault
// handler runs on the crashing thread and would do exactly that. It is therefore
// armed only by an explicit `M64_FAULT_REPORT=1`, and any finding it reports must
// be re-checked against a run with it OFF (the fault must still reproduce).
//
// Structural, not app-specific: it triggers on "the process took a fatal fault",
// never on who the process is.
//
// Env:
//   M64_FAULT_REPORT=1   arm the handler (SIGSEGV + SIGBUS)
//   M64_FAULT_REPORT_WORDS=N   how many 4-byte stack slots to dump (default 48)
//   M64_FAULT_CHAIN="rbp+8,+0xfd8,+0x10"
//                        walk a POINTER CHAIN at fault time and hexdump the end.
//                        step 0 is <reg>[+/-off] -> deref; each later step adds
//                        its offset to the previous VALUE and derefs again. The
//                        wrong value is usually several hops upstream of the
//                        faulting instruction and in no register at all.
//   M64_FAULT_CHAIN_BYTES=N    bytes to hexdump at the chain end (default 64)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <dlfcn.h>
#include <stdint.h>
#include <mach/mach.h>
#include <pthread.h>
#include <mach/mach_vm.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <sys/ucontext.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <pthread/introspection.h>
#include "dyld_image_list.h"

/* objc_shim.c: 1 + description when the address is a proxy-arena handle. */
int x64_objc_arena_describe(uint64_t addr, char *buf, size_t n);

static struct sigaction g_prev_segv, g_prev_bus, g_prev_trap, g_prev_ill;
static int g_words = 48;

/* ── THE IMAGE TABLE MUST BE A SNAPSHOT, NOT A LIVE DYLD QUERY ─────────────────
 * fr_image_for() used to call `_dyld_image_count` / `_dyld_get_image_header` /
 * `_dyld_get_image_name` — none of them async-signal-safe, all of them touching
 * dyld's own state — for rip, rsp, rbp AND for every one of the 48 stack slots
 * AND again for the loaded-image listing: ~51 re-entries into dyld per fault,
 * from inside a signal handler. In this project that is a pointed risk rather
 * than a theoretical one: the fault class this reporter exists for is raised
 * from inside a LAZY BIND (a bridge's `call` through an unbound stub runs
 * through dyld), so the handler can be entered with dyld mid-operation and the
 * very first symbolisation asks it a question.
 *
 * THE RULE, the same one objc_shim.c's mem_readable() / metaclass_probe_safe()
 * already follow: A PROBE MUST NEVER CRASH. Whatever a diagnostic touches at
 * fault time must be data it captured BEFORE the fault. The image table is
 * therefore snapshotted outside signal context — once when the reporter arms,
 * and thereafter from dyld's own add-image callback — and the handler reads
 * nothing but that snapshot. Zero dyld calls, zero locks.
 *
 * Names come from the image's own LC_ID_DYLIB, a pure header walk over already
 * mapped memory, so the add-image path needs no dyld call either. fr_arm_paths()
 * then upgrades them to full filesystem paths at arm time, where calling dyld is
 * perfectly safe and the extra context is worth having.
 *
 * ⚠HONEST LIMIT, recorded so nobody re-derives it: I could NOT synthesise a
 * context in which the live `_dyld_*` calls actually fail. Faulting inside a
 * dlopen'd initializer, inside a dyld add-image callback, and with another
 * thread holding the loader lock across a 3s initializer ALL produced complete
 * reports with the live path. dyld4's legacy accessors appear not to contend
 * there. So this is async-signal-safety hardening, NOT a fix for an observed
 * failure — and it is deliberately NOT what fault_report_once_test.sh asserts.
 * (I did at one point believe the reporter re-faulted inside its own dladdr and
 * masked the original fault; that was a misreading of the multi-copy chain
 * below, and it is wrong. See fr_install.)
 *
 * Kill switch M64_FAULT_REPORT_UNSAFE_SYMS=1 restores the live-dyld lookup. */
/* ⚠1024 WAS NOT ENOUGH AND FAILING WAS SILENT. Portal 2 loads ~1000 system
 * images before it dlopens its own modules, so the table filled with system
 * frameworks and every late Portal 2 module -- datacache, vphysics,
 * materialsystem, engine, shaderapidx9, vguimatsurface -- was dropped. The
 * handler then attributed a fault inside one of them to the nearest EARLIER
 * image: `rip = libvstdlib+0xd44faf6`, a 222 MB offset into a 622 KB dylib,
 * which reads like a real answer and sent me to the wrong image. */
#define FR_MAX_IMAGES 4096
#define FR_NAME_MAX   192
/* `span` is the image's mapped extent. Attribution used a flat 1 GiB window, so
 * ANY address above an image base looked like it belonged to it; with the real
 * span an address in no known image is reported as unknown instead of being
 * dressed up as a plausible offset. */
typedef struct { uint64_t base; uint64_t span; char name[FR_NAME_MAX]; } fr_img;
static fr_img g_imgs[FR_MAX_IMAGES];
/* sig_atomic_t + "publish the count LAST": the handler may read this while an
 * add-image callback is mid-append, so an entry is only visible once fully
 * written. Appending is the only mutation and entries are never moved. */
static volatile sig_atomic_t g_nimgs;
static int g_unsafe_syms;          /* kill switch, read once at arm time */

/* Copy keeping the TAIL, not the head. ⚠MEASURED on the real Civ artifact: a
 * head-truncating copy silently destroys the answer. Bundle paths are long —
 * ".../steamapps/common/Sid Meier's Civilization IV 34440/Civilization IV.app/
 * Contents/MacOS/libabiconv.dylib" — so a head-first cut at FR_NAME_MAX landed
 * mid-path and the basename after the last surviving '/' came out as
 * "Civilization I". Every stack slot was then attributed to the wrong image
 * with a right-looking offset: worse than no name, because it reads as a fact.
 * The tail is the identifying part, so keep that and mark the cut. */
static void fr_str_copy(char *dst, size_t n, const char *src) {
   if (!src || n == 0) { if (n) dst[0] = '\0'; return; }
   size_t len = strlen(src);
   if (len < n) { memcpy(dst, src, len + 1); return; }
   dst[0] = '~';                                  /* visibly truncated */
   memcpy(dst + 1, src + len - (n - 2), n - 2);
   dst[n - 1] = '\0';
}

/* The image's own name, straight out of its LC_ID_DYLIB. Header walk only — no
 * dyld, no allocation, safe from any context including an add-image callback. */
static void fr_name_from_header(const struct mach_header *mh, char *out, size_t n) {
   out[0] = '\0';
   if (!mh) return;
   const struct mach_header_64 *h = (const struct mach_header_64 *)mh;
   if (h->magic != MH_MAGIC_64) { fr_str_copy(out, n, "<image>"); return; }
   if (h->filetype == MH_EXECUTE) { fr_str_copy(out, n, "<main-executable>"); return; }
   const struct load_command *lc = (const struct load_command *)(h + 1);
   for (uint32_t i = 0; i < h->ncmds; i++) {
      if (lc->cmdsize < sizeof *lc) break;
      if (lc->cmd == LC_ID_DYLIB) {
         const struct dylib_command *dc = (const struct dylib_command *)lc;
         if (dc->dylib.name.offset < dc->cmdsize) {
            const char *p = (const char *)lc + dc->dylib.name.offset;
            const char *slash = strrchr(p, '/');
            fr_str_copy(out, n, slash ? slash + 1 : p);
            return;
         }
      }
      lc = (const struct load_command *)((const char *)lc + lc->cmdsize);
   }
   fr_str_copy(out, n, "<image>");
}

/* dyld add-image callback. Runs in ordinary context (dyld calls it for every
 * already-loaded image at registration, then once per later load), so the
 * snapshot stays current without the handler ever asking dyld anything. */
static void fr_add_image(const struct mach_header *mh, intptr_t slide) {
   (void)slide;
   int n = (int)g_nimgs;
   if (!mh) return;
   if (n >= FR_MAX_IMAGES) {
      /* NEVER drop an image silently again: a missing entry does not produce a
       * missing answer, it produces a WRONG one. */
      static int warned = 0;
      if (!warned) {
         warned = 1;
         fprintf(stderr, "[fault] WARNING: image table full at %d entries; "
                         "later images will be UNATTRIBUTED (raise "
                         "FR_MAX_IMAGES)\n", FR_MAX_IMAGES);
         fflush(stderr);
      }
      return;
   }
   g_imgs[n].base = (uint64_t)(uintptr_t)mh;
   /* Mapped extent, from the image's own segments -- no dyld call. */
   {
      uint64_t base = (uint64_t)(uintptr_t)mh, top = base;
      const struct mach_header_64 *m64 = (const struct mach_header_64 *)mh;
      if (m64->magic == MH_MAGIC_64) {
         /* Highest slid segment end. vmaddr is the PREFERRED address, so the
          * live end is vmaddr+slide+vmsize -- which is why `slide` is taken
          * rather than ignored here. */
         const uint8_t *p = (const uint8_t *)(m64 + 1);
         for (uint32_t i = 0; i < m64->ncmds; i++) {
            const struct load_command *lc = (const struct load_command *)p;
            if (lc->cmdsize == 0) break;
            if (lc->cmd == LC_SEGMENT_64) {
               const struct segment_command_64 *sg =
                  (const struct segment_command_64 *)p;
               uint64_t e = (uint64_t)((intptr_t)sg->vmaddr + slide) + sg->vmsize;
               if (e > top) top = e;
            }
            p += lc->cmdsize;
         }
      }
      g_imgs[n].span = (top > base) ? (top - base) : 0x1000;
   }
   fr_name_from_header(mh, g_imgs[n].name, FR_NAME_MAX);
   g_nimgs = n + 1;                    /* publish only once fully written */
}

/* Upgrade the snapshot's names to full paths. Arm time only — NEVER from the
 * handler; this is the call that must not happen at fault time. */
static void fr_arm_paths(void) {
   /* Via dyld_image_list, not dyld's indexed API: arm time is not a safe time
    * either. Arming can happen while some image is still mid-load, and querying
    * an in-flight entry aborts the process (dyld_image_list.c). infoArray is also
    * strictly better here on the reporter's own terms -- it is a plain array read,
    * where the dyld calls re-enter dyld. */
   uint32_t cnt = x64_img_count();
   for (uint32_t i = 0; i < cnt; i++) {
      const struct mach_header *mh = x64_img_header(i);
      const char *nm = x64_img_path(i);
      if (!mh || !nm) continue;
      uint64_t base = (uint64_t)(uintptr_t)mh;
      for (int k = 0; k < (int)g_nimgs; k++)
         if (g_imgs[k].base == base) { fr_str_copy(g_imgs[k].name, FR_NAME_MAX, nm); break; }
   }
}

/* Resolve an address to "<image>+0xOFF". dladdr() only knows symbols and is
 * useless for a translated image with a stripped symbol table, but the image
 * BASE is always known, and base+offset is exactly what an offline disassembler
 * wants. Reads ONLY the pre-fault snapshot (see the note above). */
static int fr_image_for(uint64_t v, char *out, size_t n) {
   uint64_t best_base = 0;
   const char *best_name = NULL;

   if (g_unsafe_syms) {
      /* KILL SWITCH — the pre-fix behaviour: ask dyld live, from inside a signal
       * handler, while dyld may hold its own lock. Reproduces the masking.
       * ⚠ These two kill-switch blocks are the ONLY places in the runtime that may
       * still call dyld's indexed image APIs: reproducing the unsafe behaviour is
       * their entire purpose. Do not "fix" them to use dyld_image_list. */
      uint32_t cnt = _dyld_image_count();
      for (uint32_t i = 0; i < cnt; i++) {
         const struct mach_header *mh = _dyld_get_image_header(i);
         if (!mh) continue;
         uint64_t base = (uint64_t)(uintptr_t)mh;
         if (v >= base && v - base < 0x40000000ULL)
            if (base > best_base) { best_base = base; best_name = _dyld_get_image_name(i); }
      }
   } else {
      int cnt = (int)g_nimgs;
      for (int i = 0; i < cnt; i++) {
         uint64_t base = g_imgs[i].base;
         uint64_t span = g_imgs[i].span ? g_imgs[i].span : 0x40000000ULL;
         if (v >= base && v - base < span)
            if (base > best_base) { best_base = base; best_name = g_imgs[i].name; }
      }
   }
   if (!best_name) return 0;
   const char *slash = strrchr(best_name, '/');
   snprintf(out, n, "%s+0x%llx", slash ? slash + 1 : best_name,
            (unsigned long long)(v - best_base));
   return 1;
}

/* Read `n` bytes from the faulting process defensively — the address we are
 * chasing is very often exactly the one that is unmapped. */
static int fr_read(uint64_t addr, void *dst, size_t n) {
   vm_size_t got = 0;
   return vm_read_overwrite(mach_task_self(), (vm_address_t)(uintptr_t)addr,
                            (vm_size_t)n, (vm_address_t)(uintptr_t)dst,
                            &got) == KERN_SUCCESS && got == n;
}

/* ── M64_FAULT_CHAIN: walk a POINTER CHAIN at fault time ───────────────────────
 * A register dump names the faulting instruction, but the value that is actually
 * wrong is usually several dereferences upstream — e.g. Halo's audio fault reads
 * an index as `u16 at (*(*(S+0xfd8)+0x10))[i*8]`, where S is arg0. None of S, the
 * intermediate object, or the record array appears in any register at the fault.
 *
 * Spec:  M64_FAULT_CHAIN="rbp+8,+0xfd8,+0x10"
 *   step 0 is <reg>[+/-<off>]  -> address; we print *address (a 4-byte i386 slot)
 *   each later step adds its offset to the PREVIOUS VALUE and dereferences again
 * After the last step we hexdump M64_FAULT_CHAIN_BYTES (default 64) at the final
 * value, so record arrays can be inspected directly.
 *
 * ★It runs ONLY inside the fault handler, so it adds ZERO pre-fault overhead and
 * cannot perturb a timing- or stack-content-sensitive heisenbug the way ordinary
 * tracing does — which is the only reason it is usable on Halo at all.
 * Structural: it takes a chain spec, and knows nothing about any app. */
static uint64_t fr_reg_by_name(x86_thread_state64_t *ss, const char *n, size_t len) {
   struct { const char *n; uint64_t v; } r[] = {
      {"rax",ss->__rax},{"rbx",ss->__rbx},{"rcx",ss->__rcx},{"rdx",ss->__rdx},
      {"rsi",ss->__rsi},{"rdi",ss->__rdi},{"rbp",ss->__rbp},{"rsp",ss->__rsp},
      {"r8",ss->__r8},{"r9",ss->__r9},{"r10",ss->__r10},{"r11",ss->__r11},
      {"r12",ss->__r12},{"r13",ss->__r13},{"r14",ss->__r14},{"r15",ss->__r15},
      {"rip",ss->__rip},
   };
   for (size_t i = 0; i < sizeof r / sizeof r[0]; i++)
      if (strlen(r[i].n) == len && strncmp(r[i].n, n, len) == 0) return r[i].v;
   return 0;
}

static void fr_walk_chain(x86_thread_state64_t *ss, const char *spec) {
   char buf[256];
   snprintf(buf, sizeof buf, "%s", spec);
   fprintf(stderr, "[fault] chain walk (M64_FAULT_CHAIN=%s):\n", spec);

   uint64_t addr = 0, val = 0;
   int step = 0;
   for (char *tok = strtok(buf, ","); tok; tok = strtok(NULL, ","), step++) {
      while (*tok == ' ') tok++;
      long off = 0;
      if (step == 0) {
         const char *p = tok;
         size_t rl = 0;
         while (p[rl] && p[rl] != '+' && p[rl] != '-' && p[rl] != ' ') rl++;
         uint64_t base = fr_reg_by_name(ss, p, rl);
         if (!base) { fprintf(stderr, "   step0: unknown register '%s'\n", tok); return; }
         off = (p[rl] == '+' || p[rl] == '-') ? strtol(p + rl, NULL, 0) : 0;
         addr = base + (uint64_t)off;
         fprintf(stderr, "   step0  %.*s%+ld -> addr 0x%llx", (int)rl, p, off,
                 (unsigned long long)addr);
      } else {
         off = strtol(tok, NULL, 0);
         addr = val + (uint64_t)off;
         fprintf(stderr, "   step%-2d %+ld -> addr 0x%llx", step, off,
                 (unsigned long long)addr);
      }
      uint32_t w = 0;
      if (!fr_read(addr, &w, sizeof w)) {
         fprintf(stderr, "  <UNREADABLE>\n");
         return;
      }
      val = w;
      char img[256];
      if (w && fr_image_for((uint64_t)w, img, sizeof img))
         fprintf(stderr, "  = 0x%08x   %s\n", w, img);
      else
         fprintf(stderr, "  = 0x%08x\n", w);
   }

   const char *bs = getenv("M64_FAULT_CHAIN_BYTES");
   int nb = bs ? atoi(bs) : 64;
   if (nb <= 0 || nb > 512) nb = 64;
   fprintf(stderr, "[fault] hexdump %d bytes at final value 0x%llx:\n",
           nb, (unsigned long long)val);
   for (int i = 0; i < nb; i += 16) {
      unsigned char row[16];
      if (!fr_read(val + (uint64_t)i, row, sizeof row)) {
         fprintf(stderr, "   +0x%03x <unreadable>\n", i);
         break;
      }
      fprintf(stderr, "   +0x%03x ", i);
      for (int j = 0; j < 16; j++) fprintf(stderr, "%02x%s", row[j], (j % 2) ? " " : "");
      fprintf(stderr, "  u16:");
      for (int j = 0; j < 16; j += 2)
         fprintf(stderr, " %u", (unsigned)(row[j] | (row[j + 1] << 8)));
      fprintf(stderr, "\n");
   }
}

/* ── WHAT IS THIS MEMORY? ──────────────────────────────────────────────────────
 * An address that resolves to NO loaded image used to be the end of the trail.
 * That is exactly the case worth digging into, because the honest "no image"
 * answer means either a wild pointer or a mapping nobody attributed — and the
 * kernel already knows which. `mach_vm_region` names the containing region's
 * base, size, protection and share mode, so an unattributed executable mapping
 * (a stale/hand-made image copy) is immediately distinguishable from a data
 * arena, from a guard page, and from genuinely unmapped space.
 *
 * Fault-safe: one Mach trap, no allocation, no dyld lock. */
static void fr_print_region(const char *label, uint64_t v) {
   mach_vm_address_t a = (mach_vm_address_t)v;
   mach_vm_size_t sz = 0;
   vm_region_basic_info_data_64_t bi;
   mach_msg_type_number_t cnt = VM_REGION_BASIC_INFO_COUNT_64;
   mach_port_t obj = MACH_PORT_NULL;

   if (mach_vm_region(mach_task_self(), &a, &sz, VM_REGION_BASIC_INFO_64,
                      (vm_region_info_t)&bi, &cnt, &obj) != KERN_SUCCESS) {
      fprintf(stderr, "[fault] %s 0x%llx: NO REGION AT OR ABOVE THIS ADDRESS\n",
              label, (unsigned long long)v);
      return;
   }
   if (v < (uint64_t)a) {
      /* mach_vm_region rounds UP to the next region, so v itself is in a hole. */
      fprintf(stderr, "[fault] %s 0x%llx: UNMAPPED (next region starts 0x%llx, "
                      "+0x%llx away)\n", label, (unsigned long long)v,
              (unsigned long long)a, (unsigned long long)((uint64_t)a - v));
      return;
   }
   fprintf(stderr, "[fault] %s 0x%llx: region [0x%llx,0x%llx) size=0x%llx "
                   "prot=%c%c%c max=%c%c%c %s%s\n",
           label, (unsigned long long)v, (unsigned long long)a,
           (unsigned long long)(a + sz), (unsigned long long)sz,
           (bi.protection & VM_PROT_READ) ? 'r' : '-',
           (bi.protection & VM_PROT_WRITE) ? 'w' : '-',
           (bi.protection & VM_PROT_EXECUTE) ? 'x' : '-',
           (bi.max_protection & VM_PROT_READ) ? 'r' : '-',
           (bi.max_protection & VM_PROT_WRITE) ? 'w' : '-',
           (bi.max_protection & VM_PROT_EXECUTE) ? 'x' : '-',
           bi.shared ? "shared" : "private",
           bi.reserved ? " reserved" : "");
}

/* Raw bytes at the faulting instruction. With no image to name it, the bytes
 * themselves are the identity: they can be matched against a translated dylib
 * on disk to prove which image a stray mapping is a copy of. */
static void fr_print_code(uint64_t rip) {
   unsigned char b[32];
   /* Start a little BEFORE rip: the preceding bytes distinguish a translated
    * call/anchor sequence from ordinary code (see the anchor-vs-frame rule). */
   uint64_t lo = rip >= 16 ? rip - 16 : rip;
   if (!fr_read(lo, b, sizeof b)) {
      fprintf(stderr, "[fault] code at rip: unreadable\n");
      return;
   }
   fprintf(stderr, "[fault] code 0x%llx (rip-16 .. rip+15):", (unsigned long long)lo);
   for (size_t i = 0; i < sizeof b; i++)
      fprintf(stderr, "%s%02x", i == 16 ? " |" : " ", b[i]);
   fprintf(stderr, "\n");
}

static void fr_print_addr(const char *label, uint64_t v) {
   char img[256];
   if (fr_image_for(v, img, sizeof(img)))
      fprintf(stderr, "   %-6s = 0x%016llx   %s\n", label, (unsigned long long)v, img);
   else
      fprintf(stderr, "   %-6s = 0x%016llx\n", label, (unsigned long long)v);
}

/* ── NESTED-FAULT GUARD ───────────────────────────────────────────────────────
 * SA_NODEFER means a fault raised INSIDE this handler re-enters it. That is
 * deliberate (a wedged handler must still die) but it made the failure silent:
 * on Civ the handler recursed three times and the process aborted with the
 * ORIGINAL fault never printed — a masking bug, not just a crash.
 *
 * A second entry now degrades instead of recursing: one async-signal-safe line
 * naming the nested fault, then straight to the default disposition. Whatever
 * the first pass already wrote survives, so a partial report beats no report.
 * write(2) rather than fprintf: stdio takes a lock we may already hold. */
static volatile sig_atomic_t g_depth;

static void fr_write_lit(const char *s, size_t n) {
   ssize_t r; do { r = write(2, s, n); } while (r < 0 && errno == EINTR);
}

/* ── dladdr CALL RING ──────────────────────────────────────────────────────────
 * WHY. Civ IV dies with SIGSEGV `fault addr=0x56` inside what symbolizes as
 * `dyld4::APIs::dladdr+635`, and the register shape says the ARGUMENT is bad,
 * not dyld: the faulting access is `[rbx+8]` with `rbx=0x4e` (0x4e+8 = 0x56),
 * and `rsi=8` — `dladdr(const void *addr, Dl_info *info)` takes `info` in rsi.
 * An `info` of 8, or an `addr` of 0x4e, would produce exactly this. What is
 * missing is WHO passed it.
 *
 * ⚠WHY NOT fprintf PER CALL, and why not lldb. MEASURED 2026-08-09: this bug is
 * a RACE. Unhosted it reproduces 11 times out of 11; under lldb it does not
 * reproduce at all (0 of 2 runs) and the app instead runs FURTHER than it ever
 * has — past the launcher into the intro movie, still alive at 240s. Anything
 * that serialises threads hides it. A write(2)/fprintf on every dladdr is such a
 * thing, and it would fail in the worst way: a clean run that looks like proof.
 *
 * So the call path does NO I/O and takes NO lock — one relaxed atomic increment
 * and three stores into a power-of-two ring. The ring is dumped from the fault
 * handler, which already symbolizes from the pre-fault image snapshot (task #40)
 * and so is itself safe. `__builtin_return_address(0)` is a REAL caller, not a
 * nearest-preceding-symbol guess — the distinction that has cost this session
 * two detours.
 *
 * ★If the ring is EMPTY at the fault, the crashing dladdr is not ours, and
 * `dladdr+635` was a nearest-symbol answer for some inlined dyld internal. The
 * interpose settles the question either way.
 *
 * Kill switch: recording is OFF unless M64_TRACE_DLADDR=1 (the dump also needs
 * M64_FAULT_REPORT=1, since the fault handler is what prints it). */
#define DR_RING 512                       /* power of two: index masks cheaply */
typedef struct { const void *addr; const void *info; const void *ret; } dr_ent;
static dr_ent g_dr[DR_RING];
static volatile long g_dr_seq;            /* total calls; & (DR_RING-1) = slot */
static int g_trace_dladdr;
static int (*g_real_dladdr)(const void *, Dl_info *);

/* libabiconv's OWN definition of dladdr. A definition inside this image wins for
 * every call site inside this image, so it catches all of our candidate callers
 * (dlsym_shim_for, nslot_repair.c, import_repair.c, lazy_bind.c) without editing
 * any of them — and anything else that reaches it. Forwards to the real one via
 * RTLD_NEXT, resolved once. */
int dladdr(const void *addr, Dl_info *info) {
   if (g_trace_dladdr) {
      long i = __atomic_fetch_add(&g_dr_seq, 1, __ATOMIC_RELAXED);
      dr_ent *e = &g_dr[(unsigned long)i & (DR_RING - 1)];
      e->addr = addr;
      e->info = info;
      e->ret  = __builtin_return_address(0);
   }
   int (*real)(const void *, Dl_info *) = g_real_dladdr;
   if (!real) {
      real = (int (*)(const void *, Dl_info *))dlsym(RTLD_NEXT, "dladdr");
      g_real_dladdr = real;
      if (!real) { return 0; }
   }
   return real(addr, info);
}

/* Dump the ring newest-first. Called only from the fault handler. */
/* The reverse-bridge ObjC call ring (callring_shim.c, ABICONV_CALLRING) records
 * {selector, receiver class, RETURN class} per thread. It armed itself only for
 * SIGILL, because a Swift bounds-trap was what motivated it — but the question it
 * answers is signal-agnostic, and it is exactly the question a SIGSEGV inside a
 * system framework poses: the unwinder cannot cross the i386-frame bridge, so the
 * last sends the translated caller made are the only evidence of what it handed
 * the framework. Inert unless ABICONV_CALLRING is set. */
extern int  _86x64_callring_enabled(void);
extern void _86x64_callring_dump_all(uint32_t crashing_tid);

static void fr_dump_callring(void) {
   if (!_86x64_callring_enabled()) return;
   fprintf(stderr, "[fault] reverse-bridge ObjC call ring "
                   "(newest last; ABICONV_CALLRING):\n");
   _86x64_callring_dump_all(pthread_mach_thread_np(pthread_self()));
}

static void fr_dump_dladdr_ring(void) {
   if (!g_trace_dladdr) { return; }
   long seq = g_dr_seq;
   fprintf(stderr, "[fault] dladdr ring (%ld calls total, newest first):\n", seq);
   if (seq == 0) {
      fprintf(stderr, "   EMPTY — no dladdr call came through libabiconv, so the\n"
                      "   faulting one is NOT ours (and 'dladdr+635' was a nearest\n"
                      "   preceding symbol, not the real function).\n");
      return;
   }
   long n = seq < DR_RING ? seq : DR_RING;
   if (n > 24) { n = 24; }
   for (long k = 1; k <= n; k++) {
      dr_ent *e = &g_dr[(unsigned long)(seq - k) & (DR_RING - 1)];
      char img[256];
      const char *where = fr_image_for((uint64_t)(uintptr_t)e->ret, img, sizeof img)
                          ? img : "?";
      /* ★ Flag the shape we are hunting: a Dl_info* or addr small enough to be
       * an integer rather than a pointer is the bad argument. */
      const char *bad = ((uintptr_t)e->info < 0x10000 || (uintptr_t)e->addr < 0x10000)
                        ? "   <<< NOT A POINTER" : "";
      fprintf(stderr, "   -%-2ld addr=%-18p info=%-18p  from %s%s\n",
              k, e->addr, e->info, where, bad);
   }
}

static void fr_handler(int sig, siginfo_t *info, void *uctx) {
   ucontext_t *uc = (ucontext_t *)uctx;
   void *fault = info ? info->si_addr : NULL;

   if (g_depth) {
      static const char m[] =
         "\n[fault] * NESTED FAULT INSIDE THE REPORTER - degrading; the report\n"
         "[fault]   above is truncated but is the ORIGINAL fault. (If this fires\n"
         "[fault]   without M64_FAULT_REPORT_UNSAFE_SYMS=1, something the handler\n"
         "[fault]   touches is not fault-safe -- fix that, do not chase the app.)\n";
      fr_write_lit(m, sizeof m - 1);
      signal(sig, SIG_DFL);
      raise(sig);
      return;
   }
   g_depth = 1;

   fprintf(stderr, "\n[fault] ================ FATAL FAULT ================\n");
   fprintf(stderr, "[fault] signal=%d (%s)  si_code=%d  fault addr=%p\n",
           sig, sig == SIGSEGV ? "SIGSEGV" : sig == SIGBUS ? "SIGBUS" : sig == SIGTRAP ? "SIGTRAP" : sig == SIGILL ? "SIGILL" : "?",
           info ? info->si_code : 0, fault);

   if (!uc || !uc->uc_mcontext) {
      fprintf(stderr, "[fault] no machine context available\n");
      goto chain;
   }
   {
      x86_thread_state64_t *ss = &uc->uc_mcontext->__ss;
      fprintf(stderr, "[fault] thread state:\n");
      fr_print_addr("rip", ss->__rip);
      fr_print_addr("rsp", ss->__rsp);
      fr_print_addr("rbp", ss->__rbp);
      fprintf(stderr,
              "   rax=0x%llx rbx=0x%llx rcx=0x%llx rdx=0x%llx\n"
              "   rsi=0x%llx rdi=0x%llx  r8=0x%llx  r9=0x%llx\n"
              "   r10=0x%llx r11=0x%llx r12=0x%llx r13=0x%llx\n"
              "   r14=0x%llx r15=0x%llx rflags=0x%llx err=0x%llx\n",
              (unsigned long long)ss->__rax, (unsigned long long)ss->__rbx,
              (unsigned long long)ss->__rcx, (unsigned long long)ss->__rdx,
              (unsigned long long)ss->__rsi, (unsigned long long)ss->__rdi,
              (unsigned long long)ss->__r8,  (unsigned long long)ss->__r9,
              (unsigned long long)ss->__r10, (unsigned long long)ss->__r11,
              (unsigned long long)ss->__r12, (unsigned long long)ss->__r13,
              (unsigned long long)ss->__r14, (unsigned long long)ss->__r15,
              (unsigned long long)ss->__rflags,
              (unsigned long long)uc->uc_mcontext->__es.__err);

      /* ★A raw PROXY-ARENA HANDLE that escaped into native code is invisible in
       * a register dump: the receiver is an opaque low address and the "Class"
       * libobjc derives from it is a plausible heap pointer, because a handle is
       * the ADDRESS OF A SLOT holding the real object and libobjc reads that
       * slot as the isa. Label any register that is one, and say what it wraps —
       * that turns "0x80809770 crashed objc_msgSend" into the object's class
       * name, which is what identifies the leaking path. (Portal 2's GL-probe
       * teardown: a handle reached the native autorelease pool and faulted on
       * drain. Quinn: NSInvocation -retainArguments.) Inert when no arena
       * exists, so a non-ObjC target prints nothing extra. */
      {
         static const char *const rn[] = {
            "rax","rbx","rcx","rdx","rsi","rdi","r8","r9",
            "r10","r11","r12","r13","r14","r15" };
         const uint64_t rv[] = {
            ss->__rax, ss->__rbx, ss->__rcx, ss->__rdx, ss->__rsi, ss->__rdi,
            ss->__r8,  ss->__r9,  ss->__r10, ss->__r11, ss->__r12, ss->__r13,
            ss->__r14, ss->__r15 };
         char d[160];
         for (unsigned i = 0; i < sizeof rv / sizeof rv[0]; ++i) {
            if (x64_objc_arena_describe(rv[i], d, sizeof d)) {
               fprintf(stderr, "[fault] %-4s 0x%llx: %s\n",
                       rn[i], (unsigned long long)rv[i], d);
            }
         }
         if (fault && x64_objc_arena_describe((uint64_t)(uintptr_t)fault,
                                              d, sizeof d)) {
            fprintf(stderr, "[fault] fault 0x%llx: %s\n",
                    (unsigned long long)(uintptr_t)fault, d);
         }
      }

      /* An address in NO image is the interesting case, not a dead end: ask the
       * kernel what the mapping actually is, and let the instruction bytes
       * identify it. Printed for rip and for the faulting address, which are
       * usually in different regions (executing here, writing there). */
      fr_print_region("rip   ", ss->__rip);
      fr_print_code(ss->__rip);
      if (fault)
         fr_print_region("fault ", (uint64_t)(uintptr_t)fault);

      /* ★ The point of the whole file. On a `rip == 0` fault the call that got us
       * here already pushed its return address, so the top of the stack names the
       * call site. Translated i386 code pushes 4 bytes, native x86_64 pushes 8, so
       * dump 4-byte slots and resolve EVERY one that lands inside a loaded image —
       * the real return address is then obvious by inspection, and no assumption
       * about which ABI pushed it has to be baked in. */
      if (ss->__rip == 0)
         fprintf(stderr, "[fault] ★ rip == 0: CALL THROUGH A NULL POINTER. "
                         "[rsp] is the call site.\n");
      fprintf(stderr, "[fault] stack from rsp (4-byte slots, low->high):\n");
      volatile uint32_t *sp = (volatile uint32_t *)(uintptr_t)ss->__rsp;
      for (int i = 0; i < g_words; i++) {
         uint32_t w;
         /* Read defensively: the stack itself may be what is unmapped. */
         vm_size_t got = 0;
         if (vm_read_overwrite(mach_task_self(),
                               (vm_address_t)(uintptr_t)(sp + i),
                               sizeof(w), (vm_address_t)(uintptr_t)&w,
                               &got) != KERN_SUCCESS || got != sizeof(w)) {
            fprintf(stderr, "   [rsp+%3d] <unreadable>\n", i * 4);
            continue;
         }
         char img[256];
         if (w && fr_image_for((uint64_t)w, img, sizeof(img)))
            fprintf(stderr, "   [rsp+%3d] 0x%08x   %s\n", i * 4, w, img);
         else
            fprintf(stderr, "   [rsp+%3d] 0x%08x\n", i * 4, w);
      }

      /* ── i386 FRAME-POINTER CHAIN ─────────────────────────────────────────
       * When rsp has run away from rbp — a runaway indirect call, a bad `sub
       * esp`, a stack overflow — the window at rsp holds nothing but the
       * wreckage, and the only surviving evidence of WHO got us here is the
       * frame chain hanging off rbp. Translated i386 code keeps the classic
       * `push ebp; mov ebp,esp` chain with 4-byte links, so [ebp] is the
       * caller's ebp and [ebp+4] is its return address. Walk it and name each
       * one. Cheap, and it works when the unwinder and the crash report do not
       * (Portal 2, 2026-09-14: rsp 16 MB below rbp, rip mid-instruction). */
      fprintf(stderr, "[fault] i386 frame chain from rbp (4-byte links):\n");
      {
         uint32_t fp = (uint32_t)ss->__rbp;
         int level = 0, printed = 0;
         for (; level < 32; level++) {
            uint32_t saved = 0, ret = 0;
            if (!fr_read((uint64_t)fp, &saved, sizeof saved) ||
                !fr_read((uint64_t)fp + 4, &ret, sizeof ret)) {
               fprintf(stderr, "   #%-2d ebp=0x%08x  <unreadable>\n", level, fp);
               break;
            }
            char img[256];
            if (ret && fr_image_for((uint64_t)ret, img, sizeof img))
               fprintf(stderr, "   #%-2d ebp=0x%08x  ret=0x%08x   %s\n",
                       level, fp, ret, img);
            else
               fprintf(stderr, "   #%-2d ebp=0x%08x  ret=0x%08x\n",
                       level, fp, ret);
            printed++;
            /* A frame chain only ever grows upward; anything else is garbage,
             * not a shorter stack, so say so rather than chasing it. */
            if (saved <= fp) {
               fprintf(stderr, "   (chain ends: saved ebp 0x%08x does not grow "
                               "upward)\n", saved);
               break;
            }
            fp = saved;
         }
         if (!printed)
            fprintf(stderr, "   (rbp is not a readable frame pointer)\n");
      }

      /* ── NATIVE 8-BYTE RETURN ADDRESSES ───────────────────────────────────
       * The 4-byte scan above exists because a translated call pushes a 4-byte
       * i386-granular return address even on the native stack. But the crashes
       * that most need a backtrace are the ones where translated code called
       * INTO a system framework and the fault happened there: the frame chain is
       * unusable (rbp=0 in Portal 2's renderer fault), the crash report names no
       * image above the signal trampoline, and rip is in no region at all. Those
       * frames are ordinary 8-byte native return addresses, and the 4-byte scan
       * shows them only as unresolved halves — `0x186028b2` on one line and
       * `0x00007ff8` on the next.
       *
       * So scan the same window again as 8-byte slots and resolve each against
       * every loaded image, high ones included. It is a heuristic sweep, not an
       * unwind: it lists candidates in stack order, and stale values from earlier
       * frames appear alongside live ones. That is still enough to name the
       * framework and the call path, which is what the unwinder could not do. */
      fprintf(stderr, "[fault] native 8-byte return-address candidates "
                      "(heuristic, stack order):\n");
      {
         int shown = 0;
         const uint64_t base = ss->__rsp & ~(uint64_t)7;
         for (int i = 0; i < g_words / 2; i++) {
            uint64_t v = 0;
            if (!fr_read(base + (uint64_t)i * 8, &v, sizeof v)) continue;
            /* Below 4GB is the translated world, already covered above. */
            if (v < 0x100000000ULL) continue;
            char img[256];
            if (!fr_image_for(v, img, sizeof(img))) continue;
            fprintf(stderr, "   [rsp+%3d] 0x%016llx   %s\n",
                    i * 8, (unsigned long long)v, img);
            shown++;
         }
         if (!shown)
            fprintf(stderr, "   (none — no high address on this stack resolved "
                            "to a loaded image)\n");
      }

      /* Chase the upstream object graph if the caller described it. */
      const char *chain = getenv("M64_FAULT_CHAIN");
      if (chain && *chain) fr_walk_chain(ss, chain);
   }

   fr_dump_dladdr_ring();
   fr_dump_callring();

   fprintf(stderr, "[fault] loaded images (low-4GB, i.e. translated):\n");
   if (g_unsafe_syms) {
      uint32_t cnt = _dyld_image_count();          /* kill switch: live dyld */
      for (uint32_t i = 0; i < cnt; i++) {
         const struct mach_header *mh = _dyld_get_image_header(i);
         if (!mh || (uint64_t)(uintptr_t)mh >= 0x100000000ULL) continue;
         fprintf(stderr, "   base=0x%08llx  %s\n",
                 (unsigned long long)(uintptr_t)mh, _dyld_get_image_name(i));
      }
   } else {
      int cnt = (int)g_nimgs;                      /* the pre-fault snapshot */
      for (int i = 0; i < cnt; i++) {
         if (g_imgs[i].base >= 0x100000000ULL) continue;
         fprintf(stderr, "   base=0x%08llx  %s\n",
                 (unsigned long long)g_imgs[i].base, g_imgs[i].name);
      }
   }
   fprintf(stderr, "[fault] ================ END ================\n");
   g_depth = 0;

chain:
   fflush(stderr);
   /* Chain to whatever was installed before us so the process dies EXACTLY as it
    * would have (including producing the OS crash report). Pure diagnostic. */
   {
      struct sigaction *prev = (sig == SIGBUS) ? &g_prev_bus :
                               (sig == SIGTRAP) ? &g_prev_trap :
                               (sig == SIGILL)  ? &g_prev_ill  : &g_prev_segv;
      if ((prev->sa_flags & SA_SIGINFO) && prev->sa_sigaction) {
         prev->sa_sigaction(sig, info, uctx);
         return;
      }
      if (prev->sa_handler && prev->sa_handler != SIG_DFL &&
          prev->sa_handler != SIG_IGN) {
         prev->sa_handler(sig);
         return;
      }
   }
   signal(sig, SIG_DFL);
   raise(sig);
}

/* Every OTHER thread needs its own alternate stack too: sigaltstack is
 * per-thread, so a stack overflow on a worker thread (Portal 2's engine threads)
 * died unreported. The introspection hook runs ON the new thread before its
 * start routine and again as it exits, for native and translated threads alike.
 * Threads that already existed at arm time are not covered. */
#define FR_THREAD_ALTSTACK (256 * 1024)
static pthread_introspection_hook_t g_prev_thread_hook;
static __thread void *t_altstack;

static void fr_thread_hook(unsigned int event, pthread_t thread, void *addr, size_t size) {
   if (event == PTHREAD_INTROSPECTION_THREAD_START && !t_altstack) {
      void *p = mmap(NULL, FR_THREAD_ALTSTACK, PROT_READ | PROT_WRITE,
                     MAP_ANON | MAP_PRIVATE, -1, 0);
      if (p != MAP_FAILED) {
         stack_t ss = { .ss_sp = p, .ss_size = FR_THREAD_ALTSTACK, .ss_flags = 0 };
         if (sigaltstack(&ss, NULL) == 0) { t_altstack = p; }
         else { munmap(p, FR_THREAD_ALTSTACK); }
      }
   } else if (event == PTHREAD_INTROSPECTION_THREAD_TERMINATE && t_altstack) {
      stack_t ss = { .ss_sp = NULL, .ss_size = 0, .ss_flags = SS_DISABLE };
      sigaltstack(&ss, NULL);
      munmap(t_altstack, FR_THREAD_ALTSTACK);
      t_altstack = NULL;
   }
   if (g_prev_thread_hook) { g_prev_thread_hook(event, thread, addr, size); }
}

__attribute__((constructor))
static void fr_install(void) {
   if (!getenv("M64_FAULT_REPORT")) return;
   const char *w = getenv("M64_FAULT_REPORT_WORDS");
   if (w) { int n = atoi(w); if (n > 0 && n <= 4096) g_words = n; }
   g_unsafe_syms = getenv("M64_FAULT_REPORT_UNSAFE_SYMS") ? 1 : 0;
   g_trace_dladdr = getenv("M64_TRACE_DLADDR") ? 1 : 0;

   /* ── INSTALL ONCE PER PROCESS, NOT ONCE PER libabiconv COPY ────────────────
    * ★ MEASURED on Civilization IV 2026-08-08. A deployed bundle carries MANY
    * co-located libabiconv copies (38 in Civ's; THREE of them actually get
    * loaded: MacOS/, QuickTime.framework/, Python.framework/). Each copy has its
    * own constructor and its own statics, so each armed the reporter and each
    * saved the PREVIOUS handler — which was the previous copy's fr_handler. The
    * chain-to-previous at the end of fr_handler then walked that stack:
    *     [fault] armed: ... x3
    *     [fault] ==== FATAL FAULT ====  x3, byte-identical, ONE distinct rip
    * i.e. the same fault reported three times, and the process's real
    * disposition only reached after three full passes.
    *
    * ⚠I first read those three `fr_handler` frames in the .ips as the reporter
    * RE-FAULTING inside its own symbolisation and masking the original fault.
    * That was wrong: the reports are complete and identical, and the extra
    * frames are this chain, not recursion. Recording it because the wrong
    * reading sent me looking for a crash in the wrong tool. Same family as
    * the libabiconv multi-copy gotcha.
    *
    * Structural detection: if the handler already installed for SIGSEGV lives in
    * an image whose name contains "libabiconv", a sibling copy got here first —
    * leave its handler alone and install nothing. Keyed on Mach-O identity, not
    * on a count, a path, or an app. Kill switch M64_FAULT_REPORT_MULTI_INSTALL=1
    * restores the per-copy install so the duplication can be reproduced. */
   if (!getenv("M64_FAULT_REPORT_MULTI_INSTALL")) {
      struct sigaction cur;
      memset(&cur, 0, sizeof cur);
      if (sigaction(SIGSEGV, NULL, &cur) == 0) {
         void *h = (cur.sa_flags & SA_SIGINFO) ? (void *)cur.sa_sigaction
                                               : (void *)cur.sa_handler;
         Dl_info di;
         if (h && h != (void *)SIG_DFL && h != (void *)SIG_IGN &&
             dladdr(h, &di) && di.dli_fname && strstr(di.dli_fname, "libabiconv")) {
            fprintf(stderr, "[fault] already armed by %s — this libabiconv copy "
                            "stands down (one reporter per process)\n", di.dli_fname);
            fflush(stderr);
            return;
         }
      }
   }

   /* Capture the image table BEFORE arming. _dyld_register_func_for_add_image
    * calls back once for every image already loaded and then on each later load,
    * so this both seeds and maintains the snapshot; fr_arm_paths upgrades the
    * header-derived names to full paths while we are still in normal context. */
   _dyld_register_func_for_add_image(fr_add_image);
   fr_arm_paths();

   /* SA_ONSTACK does NOTHING without an alternate stack actually installed —
    * and without one, a fault whose rsp is off the end of the stack cannot be
    * reported at all: the kernel writes the signal frame below the broken rsp,
    * THAT write faults too, and all you ever see is the nested fault at
    * `_sigtramp`'s own `push rbp` (Portal 2, 2026-09-14). Static storage, not
    * malloc: the process is already crashing when this gets used. */
   static char altstack[512 * 1024] __attribute__((aligned(16)));
   stack_t ss;
   memset(&ss, 0, sizeof(ss));
   ss.ss_sp    = altstack;
   ss.ss_size  = sizeof(altstack);
   ss.ss_flags = 0;
   if (sigaltstack(&ss, NULL) != 0) {
      fprintf(stderr, "[fault] sigaltstack failed: %s — a fault on a broken "
                      "stack will not be reportable\n", strerror(errno));
   }

   if (!getenv("M64_NO_FAULT_THREAD_ALTSTACK")) {     /* guard OFF arm */
      g_prev_thread_hook = pthread_introspection_hook_install(fr_thread_hook);
   }

   struct sigaction sa;
   memset(&sa, 0, sizeof(sa));
   sa.sa_sigaction = fr_handler;
   /* SA_ONSTACK: a fault caused by stack exhaustion cannot be reported on the
    * stack that overflowed. NODEFER so a fault inside the handler still dies. */
   sa.sa_flags = SA_SIGINFO | SA_NODEFER | SA_ONSTACK;
   sigemptyset(&sa.sa_mask);
   sigaction(SIGSEGV, &sa, &g_prev_segv);
   sigaction(SIGBUS,  &sa, &g_prev_bus);
   /* SIGTRAP: an i386 assert -> CoreServices Debugger() / int3 kills the process
    * with EXIT=133 and no report at all otherwise (Portal 2 2026-09-22). */
   sigaction(SIGTRAP, &sa, &g_prev_trap);
   /* SIGILL: a jump into data/unmapped-looking bytes (Portal 2 2026-09-30,
    * EXIT=132 and no report). */
   sigaction(SIGILL, &sa, &g_prev_ill);
   fprintf(stderr, "[fault] armed: SIGSEGV/SIGBUS/SIGTRAP/SIGILL reporter (M64_FAULT_REPORT), "
                   "%d stack slots\n", g_words);
   fflush(stderr);
}
