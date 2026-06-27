/*
 * 39_xml_memget_alloc — regression for the libxml2 deprecated allocator-
 * override shim (xml_shim.c ___xmlMemGet) and its callable allocator
 * trampolines.
 *
 * Modern macOS libxml2 removed the global custom-allocator API (deprecated
 * since 15.4): native xmlMemGet() returns -1, so the abigen-generated shim
 * does too. Legacy iWork/iLife (SFAXMLMemoryManager) calls xmlMemGet to
 * RETRIEVE the four current allocators, ASSERTS it returned 0, STORES the four
 * function pointers, and later INVOKES them — so the shim must (a) report
 * success and (b) hand back four CALLABLE low-4GB function pointers (a native
 * 8-byte allocator address truncates into the i386 4-byte slot; NULL faults).
 *
 * libxml2 isn't in the i386 sysroot, so `xmlMemGet` is an undefined
 * dynamic_lookup import (see the Makefile rule); the 86x64 pipeline's
 * static-interpose redirects it to libabiconv's ___xmlMemGet, exactly as it
 * does for a real binary's libxml2 binds. This exercises the WHOLE path:
 * the shim returns 0, writes four trampolines, and each trampoline — invoked
 * here through the stored i386 function pointer — actually malloc/realloc/
 * strdup/free on libabiconv's low-4GB heap.
 *
 * Pointers are inherently 32-bit (i386), so the guard is FUNCTIONAL: a broken
 * shim returns rc=-1, or hands back NULL/uncallable pointers that crash when
 * invoked, or returns memory that doesn't survive realloc — all visible in the
 * output below.
 */

extern int printf(const char *, ...);
extern void exit(int status);   /* exit() flushes stdio (unlike _exit) so the
                                 * printf output below is captured by the harness */

typedef void  (*xmlFreeFunc)(void *mem);
typedef void *(*xmlMallocFunc)(unsigned long size);
typedef void *(*xmlReallocFunc)(void *mem, unsigned long size);
typedef char *(*xmlStrdupFunc)(const char *str);

extern int xmlMemGet(xmlFreeFunc *freeFunc, xmlMallocFunc *mallocFunc,
                     xmlReallocFunc *reallocFunc, xmlStrdupFunc *strdupFunc);

int main(void) {
   xmlFreeFunc    xfree    = 0;
   xmlMallocFunc  xmalloc  = 0;
   xmlReallocFunc xrealloc = 0;
   xmlStrdupFunc  xstrdup  = 0;

   int rc = xmlMemGet(&xfree, &xmalloc, &xrealloc, &xstrdup);
   printf("xmlMemGet rc=%d\n", rc);
   printf("allocators nonnull=%d\n",
          (xfree && xmalloc && xrealloc && xstrdup) ? 1 : 0);

   /* malloc + write + read back */
   char *p = (char *)xmalloc(16);
   printf("malloc nonnull=%d\n", p ? 1 : 0);
   int i;
   for (i = 0; i < 15; i++)
      p[i] = (char)('A' + i);
   p[15] = 0;
   printf("buf=%s\n", p);

   /* realloc preserves the contents */
   char *q = (char *)xrealloc(p, 64);
   printf("realloc nonnull=%d\n", q ? 1 : 0);
   printf("buf2=%s\n", q);

   /* strdup copies a NUL-terminated string */
   char *d = xstrdup("xml-strdup-ok");
   printf("strdup nonnull=%d\n", d ? 1 : 0);
   printf("dup=%s\n", d);

   /* free must not crash (both heap pointers belong to libabiconv) */
   xfree(q);
   xfree(d);
   printf("freed ok\n");

   exit(0);
}
