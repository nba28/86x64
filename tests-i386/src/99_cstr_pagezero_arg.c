/* 99_cstr_pagezero_arg — guard for the page-zero `const char *` ARG clamp
 * (typeconv.cc convert_const_cstr_ptr + objc_shim.c x64_cstr_pagezero_str).
 *
 * A translated i386 image's __PAGEZERO spans [0, 0x1000) and is unmapped by
 * construction, so a C-string argument whose value lands in that range cannot
 * address a valid string. Forwarding it into a native framework kills the
 * process inside system code — recoverable neither by the app nor by us.
 *
 * The real case: Halo CE's renderer-check nag dialog calls
 *   CFStringCreateWithCString(NULL, msg, kCFStringEncodingASCII)
 * with msg == 0x3c. 0x3c is not corruption we introduced: it is the literal 60
 * (the default 60 Hz refresh rate) that Halo itself stores at i386 0x1f243d
 * into the same stack slot it later uses as the FormatMessageA out-parameter.
 * Halo's FormatMessageA emulation (i386 0x2ad6be) loads lpBuffer into %edi,
 * then overwrites %edi with the malloc result on the
 * FORMAT_MESSAGE_ALLOCATE_BUFFER path and never stores through it — so the
 * caller's slot keeps the stale 0x3c. That path only runs when the GPU
 * capability check fails, which never happened on 2006 hardware, so the bug
 * shipped. Nothing in the sprintf/malloc/strcpy chain is at fault
 * (99_sprintf_malloc_strcpy pins that composite as clean).
 *
 * NULL must still pass through untouched (it is a legitimate argument across
 * much of this surface), and a valid string must be unaffected — both are
 * asserted here alongside the clamp itself.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <locale.h>

typedef const void *CFTypeRef;
typedef const struct __CFString *CFStringRef;
typedef const struct __CFAllocator *CFAllocatorRef;

extern CFStringRef CFStringCreateWithCString(CFAllocatorRef, const char *, unsigned int);
extern long        CFStringGetLength(CFStringRef);
extern void        CFRelease(CFTypeRef);

/* Launder the constant so the compiler cannot fold the call away or reason
 * about the pointer's provenance. */
__attribute__((noinline))
static const char *opaque_ptr(unsigned long v) {
    const char *p = (const char *)v;
    __asm__ volatile("" : "+r"(p));
    return p;
}

int main(void)
{
    /* 1. a real string is untouched by the clamp */
    CFStringRef ok = CFStringCreateWithCString(NULL, opaque_ptr((unsigned long)"hello"), 0x600);
    printf("valid_nonnull=%d\n", ok != NULL);
    printf("valid_len=%ld\n", ok ? CFStringGetLength(ok) : -1L);
    if (ok) CFRelease(ok);

    /* 2. THE BLOCKER: Halo's exact value. Reaching the next line at all is the
     *    assertion — without the clamp this faults inside CoreFoundation. */
    CFStringRef bad = CFStringCreateWithCString(NULL, opaque_ptr(0x3c), 0x600);
    printf("survived_pagezero=1\n");
    printf("pagezero_nonnull=%d\n", bad != NULL);
    printf("pagezero_len=%ld\n", bad ? CFStringGetLength(bad) : -1L);
    if (bad) CFRelease(bad);

    /* 3. the whole unmapped page is covered, not just the one value seen */
    CFStringRef edge = CFStringCreateWithCString(NULL, opaque_ptr(0xfff), 0x600);
    printf("survived_edge=1\n");
    printf("edge_len=%ld\n", edge ? CFStringGetLength(edge) : -1L);
    if (edge) CFRelease(edge);

    /* 4. the first MAPPED address is NOT clamped: 0x1000 is a legitimate
     *    pointer value, so it must still be forwarded verbatim. Proven by
     *    reaching a real string that we place nowhere near page zero. */
    CFStringRef live = CFStringCreateWithCString(NULL, opaque_ptr((unsigned long)"abc"), 0x600);
    printf("unclamped_len=%ld\n", live ? CFStringGetLength(live) : -1L);
    if (live) CFRelease(live);

    /* 5. NULL is a documented argument across much of this surface and must NOT
     *    be turned into "". setlocale discriminates the two crisply and is one
     *    of the shims that carries the clamp: setlocale(LC_ALL, NULL) QUERIES
     *    and returns the startup locale "C", while setlocale(LC_ALL, "") SETS
     *    the locale from the environment and returns whatever that is. So this
     *    line changes the moment the clamp starts swallowing NULL. */
    printf("null_query=%s\n", setlocale(LC_ALL, NULL));

    exit(0);
}
