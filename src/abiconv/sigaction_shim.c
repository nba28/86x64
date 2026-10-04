/*
 * sigaction_shim.c — sigaction for i386 callers. ONE job: the handler is i386
 * code, so the kernel's _sigtramp must reach it through a native callback
 * trampoline (cb_bridge.c), exactly like the generated signal() bridge does.
 *
 * The old hand-asm bridge copied the handler pointer raw: the native
 * _sigtramp then CALLED i386 code, whose 4-byte `ret` popped half of the
 * native return address. Portal 2: engine DisplaySystemVersion's popen child
 * exited, SIGCHLD reached a game handler, and the return landed at
 * 0x1b2233bd (the low half of _sigtramp+29) in the heap (SIGBUS).
 *
 * *oact reports the ORIGINAL i386 handler (a per-signal record of what we
 * wrapped): callers compare it (`old.sa_handler == mine`), and passing it back
 * in re-wraps it to the same trampoline. Guard 99_sigaction_stack_balance.
 * ponytail: an SA_SIGINFO handler gets the native siginfo_t; its leading
 * fields (si_signo..si_status) match i386, the pointer fields after do not; and
 * the low copy is malloc'd in signal context (not async-signal-safe: a signal
 * landing inside malloc could deadlock — add a preallocated pool if one does).
 * Kill switch M64_NO_SIGACTION_WRAP=1; guard tests-i386 sigaction-wrap.
 */
#include <signal.h>
#include <sys/ucontext.h>
#include <stdint.h>
#include <stdlib.h>
#include "cb_bridge.h"

#define P(v) ((void *)(uintptr_t)(uint32_t)(v))

static const x64_cb_sig sig_plain   = { 1, CBR_VOID, { CBA_I32 }, { 0 } };
/* siginfo_t / ucontext_t live in the kernel's signal frame, which may sit above
 * 4GB: the trampoline hands the i386 handler LOW copies (CBA_PTR_REC). */
static const x64_cb_sig sig_siginfo = { 3, CBR_VOID, { CBA_I32, CBA_PTR_REC, CBA_PTR_REC },
                                        { 0, sizeof(siginfo_t), sizeof(ucontext_t) } };

/* per signal: the i386 handler we wrapped and the trampoline that stands in */
static uint32_t g_orig32[NSIG];
static uint64_t g_tramp[NSIG];

/* int sigaction(int sig, const struct sigaction *act, struct sigaction *oact) */
uint32_t shim_sigaction(uint32_t *a) {
   static int raw = -1;
   if (raw < 0) raw = getenv("M64_NO_SIGACTION_WRAP") != NULL;
   const uint32_t *act32 = P(a[1]);
   uint32_t *oact32 = P(a[2]);
   struct sigaction act = {0}, oact = {0};
   if (act32) {
      uint32_t h = act32[0];
      act.sa_mask = act32[1];
      act.sa_flags = (int)act32[2];
      uint64_t nh = h;
      if (!raw && h != 0 && h != 1 && h != 0xffffffffu)       /* not SIG_DFL/SIG_IGN/SIG_ERR */
         nh = x64_cb_wrap(h, (act.sa_flags & SA_SIGINFO) ? &sig_siginfo : &sig_plain);
      act.sa_sigaction = (void (*)(int, siginfo_t *, void *))(uintptr_t)nh;
   }
   const int sig = (int)a[0];
   int r = sigaction(sig, act32 ? &act : NULL, oact32 ? &oact : NULL);
   if (r == 0 && oact32) {
      uint64_t oh = (uint64_t)(uintptr_t)oact.sa_sigaction;
      if (sig > 0 && sig < NSIG && oh != 0 && oh == g_tramp[sig]) { oh = g_orig32[sig]; }
      oact32[0] = (uint32_t)oh;
      oact32[1] = oact.sa_mask;
      oact32[2] = (uint32_t)oact.sa_flags;
   }
   if (r == 0 && act32 && sig > 0 && sig < NSIG) {
      g_orig32[sig] = act32[0];
      g_tramp[sig] = (uint64_t)(uintptr_t)act.sa_sigaction;
   }
   return (uint32_t)r;
}
