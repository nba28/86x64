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
#include <mach-o/dyld.h>
#include <sys/ucontext.h>

static struct sigaction g_prev_segv, g_prev_bus;
static int g_words = 48;

/* Resolve an address to "<image>+0xOFF" using the dyld image table. dladdr()
 * only knows symbols and is useless for a translated image with a stripped
 * symbol table, but the image BASE is always known, and base+offset is exactly
 * what an offline disassembler wants. */
static int fr_image_for(uint64_t v, char *out, size_t n) {
   uint32_t cnt = _dyld_image_count();
   uint64_t best_base = 0;
   const char *best_name = NULL;
   for (uint32_t i = 0; i < cnt; i++) {
      const struct mach_header *mh = _dyld_get_image_header(i);
      if (!mh) continue;
      uint64_t base = (uint64_t)(uintptr_t)mh;
      if (v >= base && v - base < 0x40000000ULL) {   /* within 1GB of a base */
         if (base > best_base) { best_base = base; best_name = _dyld_get_image_name(i); }
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

static void fr_print_addr(const char *label, uint64_t v) {
   char img[256];
   if (fr_image_for(v, img, sizeof(img)))
      fprintf(stderr, "   %-6s = 0x%016llx   %s\n", label, (unsigned long long)v, img);
   else
      fprintf(stderr, "   %-6s = 0x%016llx\n", label, (unsigned long long)v);
}

static void fr_handler(int sig, siginfo_t *info, void *uctx) {
   ucontext_t *uc = (ucontext_t *)uctx;
   void *fault = info ? info->si_addr : NULL;

   fprintf(stderr, "\n[fault] ================ FATAL FAULT ================\n");
   fprintf(stderr, "[fault] signal=%d (%s)  si_code=%d  fault addr=%p\n",
           sig, sig == SIGSEGV ? "SIGSEGV" : sig == SIGBUS ? "SIGBUS" : "?",
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

      /* Chase the upstream object graph if the caller described it. */
      const char *chain = getenv("M64_FAULT_CHAIN");
      if (chain && *chain) fr_walk_chain(ss, chain);
   }

   fprintf(stderr, "[fault] loaded images (low-4GB, i.e. translated):\n");
   {
      uint32_t cnt = _dyld_image_count();
      for (uint32_t i = 0; i < cnt; i++) {
         const struct mach_header *mh = _dyld_get_image_header(i);
         if (!mh || (uint64_t)(uintptr_t)mh >= 0x100000000ULL) continue;
         fprintf(stderr, "   base=0x%08llx  %s\n",
                 (unsigned long long)(uintptr_t)mh, _dyld_get_image_name(i));
      }
   }
   fprintf(stderr, "[fault] ================ END ================\n");

chain:
   fflush(stderr);
   /* Chain to whatever was installed before us so the process dies EXACTLY as it
    * would have (including producing the OS crash report). Pure diagnostic. */
   {
      struct sigaction *prev = (sig == SIGBUS) ? &g_prev_bus : &g_prev_segv;
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

__attribute__((constructor))
static void fr_install(void) {
   if (!getenv("M64_FAULT_REPORT")) return;
   const char *w = getenv("M64_FAULT_REPORT_WORDS");
   if (w) { int n = atoi(w); if (n > 0 && n <= 4096) g_words = n; }

   struct sigaction sa;
   memset(&sa, 0, sizeof(sa));
   sa.sa_sigaction = fr_handler;
   /* SA_ONSTACK: a fault caused by stack exhaustion cannot be reported on the
    * stack that overflowed. NODEFER so a fault inside the handler still dies. */
   sa.sa_flags = SA_SIGINFO | SA_NODEFER | SA_ONSTACK;
   sigemptyset(&sa.sa_mask);
   sigaction(SIGSEGV, &sa, &g_prev_segv);
   sigaction(SIGBUS,  &sa, &g_prev_bus);
   fprintf(stderr, "[fault] armed: SIGSEGV/SIGBUS reporter (M64_FAULT_REPORT), "
                   "%d stack slots\n", g_words);
   fflush(stderr);
}
