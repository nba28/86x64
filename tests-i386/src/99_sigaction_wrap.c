/*
 * 99_sigaction_wrap — a handler installed with sigaction() is i386 code and
 * must be reached through a native callback trampoline (sigaction_shim.c).
 * The old bridge passed it raw: the kernel's _sigtramp called i386 code and its
 * 4-byte `ret` landed on half of _sigtramp's return address (Portal 2: SIGCHLD
 * after engine DisplaySystemVersion's popen child exited).
 *
 * Exit 42 = both handlers ran (plain and SA_SIGINFO) and oact round-trips.
 * Kill switch M64_NO_SIGACTION_WRAP=1 (run time): a fault or a wrong exit.
 */
#include <signal.h>
#include <stdlib.h>

static volatile int hits;
static void on_usr1(int sig) { if (sig == SIGUSR1) hits += 1; }
static void on_usr2(int sig, siginfo_t *si, void *uc) {
   (void)uc;
   if (sig == SIGUSR2 && si && si->si_signo == SIGUSR2) hits += 10;
}

int main(void) {
   struct sigaction sa = {0}, old = {0};
   sa.sa_handler = on_usr1;
   if (sigaction(SIGUSR1, &sa, 0) != 0) exit(3);
   sa.sa_sigaction = on_usr2;
   sa.sa_flags = SA_SIGINFO;
   if (sigaction(SIGUSR2, &sa, 0) != 0) exit(4);
   raise(SIGUSR1);
   raise(SIGUSR2);
   if (hits != 11) exit(5);
   /* the handed-back handler, installed again, still reaches on_usr1 */
   if (sigaction(SIGUSR1, 0, &old) != 0 || !old.sa_handler) exit(6);
   if (sigaction(SIGUSR1, &old, 0) != 0) exit(7);
   raise(SIGUSR1);
   exit(hits == 12 ? 42 : 8);
}
