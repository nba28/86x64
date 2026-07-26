/* 99_sprintf_malloc_strcpy — Halo CE renderer-check failure-message path.
 *
 * When Halo's GPU capability check fails (CGDisplayIDToOpenGLDisplayMask ->
 * CGLQueryRendererInfo -> CGLDescribeRenderer), it builds the error string for
 * its Carbon nag dialog with exactly this composite (Halo.dylib +0x39683e):
 *
 *     sprintf(localbuf, "Win32 error #%d", code);   // localbuf is 0x41c bytes
 *     len = strlen(localbuf);                       // inline repne scasb
 *     p   = malloc(len + 1);
 *     strcpy(p, localbuf);
 *     *out = p;
 *
 * and then hands *out to CFStringCreateWithCString(NULL, *out,
 * kCFStringEncodingASCII). In the real app *out arrived as 0x3c and native
 * CoreFoundation faulted dereferencing it.
 *
 * sprintf, malloc and strcpy are ALL bridged libabiconv shims. The existing
 * printf guards (08_printf_4args, 77_positional_format, 78_printf_fp_vararg,
 * 79_star_format) cover FORMATTING alone and pass; what none of them cover is
 * this handoff: format into an i386 STACK buffer, size it, heap-copy it, and
 * carry the heap pointer out through a caller-supplied out-parameter.
 *
 * CoreFoundation is deliberately NOT used here (it is not in the base i386
 * sysroot, and it is only the victim): if the pointer and bytes round-trip,
 * the fault lies elsewhere; if they do not, this reproduces the blocker in
 * isolation. Output must stay deterministic, so the pointer VALUE is never
 * printed -- only derived predicates.
 */
/* Halo is a 2006 binary: it calls PLAIN _sprintf/_strcpy with no fortify and no
 * stack guard. A modern clang would substitute ___sprintf_chk/___strcpy_chk and
 * reference ___stack_chk_guard, exercising the wrong shims entirely (and, as
 * first written, this test died before main at rip=0x1). Match the target. */
#undef _FORTIFY_SOURCE
#define _FORTIFY_SOURCE 0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Mirrors Halo's helper: formats into a large local buffer, then returns a
 * malloc'd copy through an out-parameter. Kept non-inline so the stack buffer
 * really is a separate frame that dies on return, as in the original. */
/* Launder a pointer so the compiler cannot know the buffer's size. Without this
 * a modern clang emits the _FORTIFY_SOURCE variants ___sprintf_chk/___strcpy_chk
 * and a stack-protector guard -- symbols Halo's 2006 binary never references, so
 * the test would exercise the wrong shims entirely (and, as first written, it
 * died before main with rip=0x1). Halo calls plain _sprintf/_strcpy. */
__attribute__((noinline))
static char *opaque(char *p) { __asm__ volatile("" : "+r"(p)); return p; }

__attribute__((noinline, no_stack_protector))
static void build_msg(int code, char **out)
{
    char localbuf[1052];                       /* Halo's frame reserves 0x41c */
    sprintf(opaque(localbuf), "Win32 error #%d", code);
    size_t len = strlen(opaque(localbuf));
    char *p = (char *)malloc(len + 1);
    if (!p) { *out = NULL; return; }
    strcpy(opaque(p), opaque(localbuf));
    *out = p;
}

__attribute__((no_stack_protector))
int main(void)
{
    char *msg = NULL;
    build_msg(1300, &msg);

    /* 1. the out-parameter actually came back */
    printf("nonnull=%d\n", msg != NULL);
    /* 2. and is not a sub-page value -- the exact failure seen in Halo (0x3c) */
    printf("subpage=%d\n", msg != NULL && (uintptr_t)msg < 0x1000);
    /* 3. and is not a TRUNCATED 64-bit heap pointer: on a translated i386 image
     *    every valid pointer must fit in 32 bits, so a malloc result that had
     *    its high half chopped would still be non-nil but the BYTES below fail. */
    if (msg) {
        printf("msg=%s\n", msg);
        printf("len=%d\n", (int)strlen(msg));
        printf("match=%d\n", strcmp(msg, "Win32 error #1300") == 0);
        free(msg);
    }

    /* 4. same composite with a %s conversion: Halo's dialog strings are built
     *    through this path too, and a literal "%s" reaching the screen was
     *    previously blamed on Halo rather than on the format shim. */
    char buf2[256];
    sprintf(opaque(buf2), "Msg%s", "-ok");
    printf("fmt_s=%s\n", buf2);

    return 0;
}

/* ---------------------------------------------------------------------------
 * STATUS 2026-07-26: NOT YET A VALID REPRO -- deliberately has no
 * expected/99_sprintf_malloc_strcpy.txt, so the suite SKIPs it rather than
 * going red on a result that proves nothing.
 *
 * The translated binary dies BEFORE main: EXC_BAD_ACCESS at rip=0x1, one frame
 * under dyld`start (crashlog 99_sprintf_malloc_strcpy.x86_64-2026-07-26-173746).
 * That is a launch failure of THIS fixture, not Halo's CFStringCreateWithCString
 * fault, so nothing about the sprintf/malloc/strcpy chain has been demonstrated
 * either way.
 *
 * Already ruled out: _FORTIFY_SOURCE and the stack protector. As first written
 * the object imported ___sprintf_chk/___strcpy_chk/___stack_chk_guard -- symbols
 * Halo's 2006 binary never references. After #define _FORTIFY_SOURCE 0 plus
 * no_stack_protector the imports are exactly _sprintf/_strcpy/_malloc/_strlen/
 * _printf/_free/_strcmp, matching Halo, and the pre-main crash is UNCHANGED.
 *
 * Next: bisect the import set (78_printf_fp_vararg and 08_printf_4args launch
 * fine, so printf alone is not it -- suspect _malloc/_free/_strcmp) and check
 * whether rip=0x1 is an unbound __jt_tramp slot or a bad init trampoline.
 * ------------------------------------------------------------------------- */
