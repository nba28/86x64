/* 72_ctype_rune_locale — inline <ctype.h> classification via __DefaultRuneLocale.
 *
 * The i386 <ctype.h> classifiers isalnum/isdigit/islower/isupper/isprint/isgraph
 * expand (via __istype) to an INLINE read of
 * `__DefaultRuneLocale.__runetype[c]` for chars < 0x80 — indexing the classic
 * __IMPORT,__pointers non-lazy slot bound to the 64-bit libSystem
 * `__DefaultRuneLocale`. A 4-byte `movl` on that slot keeps only the low 32
 * bits of the >4GB symbol address -> garbage table base -> SIGSEGV on the
 * `movl 52(%eax,%c,4)` index (52 = 0x34 = the i386 __runetype offset). This is
 * the Quinn Settings crash: -[KeyTypeCell isEntryAcceptable:] validating a
 * typed key ('a') read the _CTYPE_R (print, bit 0x40000) runetype bit and
 * faulted at low32(&table)+0x34+'a'*4 (crashlog Quinn-2026-07-04-005805.ips,
 * fault 0x5b061150).
 *
 * Fix (objc_slide.c patch_import_pointers + i386_rune_build): redirect the slot
 * to a low-4GB i386-layout copy of the rune table seeded from the host
 * _DefaultRuneLocale, so the i386 `movl` reads a valid <4GB table base. The
 * classifier results (native _CTYPE_* bits are ABI-stable) then match libc.
 * isprint below is the exact bit Quinn's isEntryAcceptable: reads.
 *
 * volatile chars defeat constant-folding so the inline table read runs.
 * write()+_exit() sidestep the stdio/atexit teardown (whose own over-pop in the
 * suite's manual `-e _main` link is a separate, unrelated issue). Pre-fix:
 * SIGSEGV (exit 139) at the first classifier. Post-fix: prints the ok line.
 */
#include <ctype.h>
#include <string.h>
#include <unistd.h>

int main(void) {
   volatile int a = 'a', z = '0', A = 'A', ctl = 0x01, sp = ' ';
   int ok = isalnum(a) && !isalnum(sp) && isdigit(z) && !isdigit(a)
            && islower(a) && !islower(A) && isupper(A) && !isupper(a)
            && isprint(a) && !isprint(ctl) && isgraph(a) && !isgraph(sp);
   const char *msg = ok ? "ctype rune classify ok\n"
                        : "ctype rune classify MISMATCH\n";
   write(1, msg, strlen(msg));
   _exit(ok ? 0 : 1);
}
