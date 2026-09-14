/*
 * 99_sigaction_stack_balance — a bridge must POP the caller's 4-byte return
 * address.
 *
 * A translated i386 caller pushes a 4-BYTE return address, so an i386-callable
 * bridge must end with `mov r11d,[rsp]; add rsp,4; jmp r11`. The hand-written
 * `src/abiconv/sigaction.asm` left out the `add rsp,4` (so did exec.asm and
 * getopt.asm). The caller then returns with esp 4 LOW — silently, for the rest
 * of that frame's life — until its own epilogue pops one word short and `ret`
 * jumps to whatever sat below the return address, which is the saved ebp.
 *
 * MEASURED, Portal 2: `CProcessUtils::Init` (libvstdlib) installs a SIGCHLD
 * handler with sigaction, and its own `ret` then jumped to the saved ebp —
 * `rip == r11`, both inside the i386 stack region (`prot=rw-`, `err=0x15`),
 * with the real return address `CAppSystemGroup::InitSystems+0x3e` sitting 4
 * bytes above. Static check: `src/86x64/bridge-epilogue-scan.py`.
 *
 * THE SHAPE MATTERS: the imbalance is invisible at the call itself. It only
 * kills the frame that made the call, at ITS epilogue — so the test calls
 * sigaction inside a helper that then has to return normally and hand back a
 * value the caller checks. A helper with saved registers and locals is the
 * point; a leaf that tail-exits would survive the bug.
 *
 * WARNING: ends with exit(), never `return`. These tests link `-e _main` with
 * no crt0 and the 86x64.sh wrapper enters _main via `jmp`, so there is no
 * return address to `ret` to.
 */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

static volatile int g_hits;

static void on_usr1(int sig) { (void)sig; g_hits++; }   /* never raised here */

/* Deliberately non-leaf: locals + a callee-saved register live across the
 * bridge call, so a 4-byte imbalance corrupts THIS frame's epilogue. */
static int __attribute__((noinline)) install(int sig, int *witness)
{
   struct sigaction act, old;
   memset(&act, 0, sizeof act);
   act.sa_handler = on_usr1;
   sigemptyset(&act.sa_mask);
   act.sa_flags = 0;

   if (sigaction(sig, &act, &old) != 0) { return 7; }
   /* query-only form: act == NULL is a separate path through the bridge */
   if (sigaction(sig, NULL, &old) != 0) { return 8; }
   if (old.sa_handler != on_usr1) { return 9; }

   *witness += 1;
   return 0;                       /* the `ret` here is what the bug breaks */
}

int main(void)
{
   int witness = 0;

   /* Several times: the imbalance accumulates 4 bytes per call, so if one
    * round happens to survive, a later one will not. */
   for (int i = 0; i < 4; ++i) {
      int rc = install(SIGUSR1, &witness);
      if (rc != 0) { printf("install failed rc=%d\n", rc); exit(rc); }
   }
   if (witness != 4) { printf("witness %d, expected 4\n", witness); exit(10); }

   /* NOTE: this guard deliberately does NOT raise the signal. Delivering one
    * to a translated handler is a SEPARATE, still-open gap (the kernel calls
    * the i386 function pointer directly with an 8-byte return address) — see
    * the known-gaps list. Mixing the two would make a red here ambiguous. */
   printf("sigaction balanced\n");
   exit(42);
}
