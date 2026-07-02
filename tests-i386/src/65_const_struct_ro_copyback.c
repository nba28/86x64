/* 65_const_struct_ro_copyback.c — regression guard for the abigen
 * const-pointee copy-back SIGBUS (typeconv.cc convert_pointer).
 *
 * An abigen-generated shim deep-copies a struct-pointer arg into a bounce
 * buffer (i386/x86_64 layouts differ when the struct has `long` fields) and,
 * after the native call, used to copy the staged struct back through the
 * ORIGINAL caller pointer — unconditionally, even for a `const struct *`
 * argument. A `static const` struct lives in __TEXT,__const, which is mapped
 * read-only, so that copy-back store faults: SIGBUS KERN_PROTECTION_FAILURE.
 * This is exactly how Halo crashed inside ___InstallEventHandler: its
 * `static const EventTypeSpec` list sits in __TEXT,__const and the shim's
 * post-call copy-back wrote the staged entry back through `inList`
 * (crash rip = ___InstallEventHandler.4+0x27 `mov [r11], eax`,
 * rax='cmds' = kEventClassCommand, Halo-2026-07-02-111511.ips).
 *
 * The fix skips the copy-back for const-qualified pointees (nothing to copy
 * back: the callee contracts not to modify them) while still advancing the
 * bounce-buffer cursor so LATER staged args keep their forward-pass offsets.
 * This test pins both halves with one call:
 *   setitimer(ITIMER_REAL, &g_new /* const, RO * /, &old /* out-param * /)
 *   - &g_new: const struct in __TEXT,__const -> pre-fix SIGBUS on copy-back,
 *     post-fix untouched.
 *   - &old:   NON-const out-param staged AFTER the const arg -> its copy-back
 *     must still fire AND read the correct staging slot (cursor lockstep).
 *   - getitimer round-trip proves the RO value actually reached the kernel
 *     through the forward staging copy (marshalling still correct).
 * itimerval has i386 4-byte `long` fields (8 on x86_64), so this arg cannot
 * take the layout-identical passthrough route — it exercises the deep-copy
 * path the crash was on. */

extern int printf(const char *, ...);
extern void exit(int);

struct my_timeval { long tv_sec; long tv_usec; };
struct my_itimerval { struct my_timeval it_interval, it_value; };
extern int setitimer(int, const struct my_itimerval *, struct my_itimerval *);
extern int getitimer(int, struct my_itimerval *);

#define ITIMER_REAL 0

/* __TEXT,__const = read-only at runtime; the pre-fix copy-back store through
 * this pointer is the deterministic SIGBUS. */
__attribute__((section("__TEXT,__const")))
static const struct my_itimerval g_new = { {0, 0}, {3600, 0} };
__attribute__((section("__TEXT,__const")))
static const struct my_itimerval g_zero = { {0, 0}, {0, 0} };

int main(void) {
   struct my_itimerval old  = { {7, 7}, {7, 7} };
   struct my_itimerval back = { {0, 0}, {0, 0} };

   int rc1 = setitimer(ITIMER_REAL, &g_new, &old);   /* const RO in + out-param */
   int rc2 = getitimer(ITIMER_REAL, &back);          /* read the armed value back */
   setitimer(ITIMER_REAL, &g_zero, 0);               /* disarm (RO again, NULL out) */

   int ok = rc1 == 0 && rc2 == 0
         /* forward staging copy delivered the RO value to the kernel */
         && back.it_value.tv_sec > 3590 && back.it_value.tv_sec <= 3600
         /* non-const out-param copy-back fired and read the RIGHT staging
          * slot (previous timer was unarmed -> all zeros, not the 7s) */
         && old.it_value.tv_sec == 0 && old.it_interval.tv_sec == 0;

   printf("const-struct copy-back: %s\n", ok ? "ok" : "FAIL");
   exit(0);
}
