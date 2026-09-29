/*
 * xml_shim.c — compatibility shims for libxml2's deprecated global
 * memory-allocator override (xmlMemSetup / xmlMemGet).
 *
 * Modern macOS libxml2 removed the global custom-allocator API (deprecated
 * since macOS 15.4 — "Memory allocation functions deprecated"): both
 * xmlMemSetup() and xmlMemGet() now hard-fail with -1. The abigen-generated
 * shims forward faithfully to native libxml2, so they too return -1.
 *
 * Legacy iWork/iLife installs/queries its own xmlMalloc/xmlFree/xmlRealloc/
 * xmlStrdup at startup and ASSERTS success. SFArchiving's SFAXMLMemoryManager
 * does this in two places, both hitting the deprecated API:
 *   - a helper that calls xmlMemSetup() and asserts == 0  -> shim_xmlMemSetup
 *   - +[SFAXMLMemoryManager createInitialManager] which calls
 *       xmlMemGet(&freeFunc, &mallocFunc, &reallocFunc, &strdupFunc)
 *     asserts == 0, STORES the four retrieved allocators in the manager, and
 *     later INVOKES them.  The createInitialManager assertion (SFAXMLMemory
 *     Manager.mm:60) is the one that fires at startup -> -[SFUAssertionHandler
 *     handleFailureInMethod:...] -> anti-debug ptrace() over-pop -> SIGSEGV.
 *
 * shim_xmlMemSetup (the simpler half): the legacy allocators these apps install
 * are plain malloc/free/realloc/strdup wrappers — semantically identical to
 * libxml2's own defaults — so NOT installing them and letting libxml2 keep its
 * defaults is behaviour-preserving. Report success (0), ignore the four ptrs.
 *
 * shim_xmlMemGet (the gate): must report success (0) AND write four CALLABLE
 * allocator function pointers into the caller's out-params, because the manager
 * STORES them and later INVOKES them with the i386 cdecl ABI. They cannot be
 * NULL, and they cannot be the native 8-byte allocator pointers (an i386 4-byte
 * slot truncates them). So we hand back four low-4GB trampolines (MTSHIM stubs
 * in maptable_tramp.asm) that marshal the i386 cdecl frame and forward to
 * libabiconv's OWN low-4GB heap (malloc_shim.c) — whose pointers ARE 32-bit
 * representable, which native malloc's are not.  Keeping allocation AND free on
 * the libabiconv heap is internally consistent for the manager's own buffers.
 *
 * All shims are reached through the ___xmlMemSetup / ___xmlMemGet trampolines
 * (the same export names abigen used), so static-interpose / resync route
 * already-deployed binaries to them. Entry ABI (MTSHIM): a = &i386_args[0],
 * 4-byte cdecl stack slots; the int return flows back in eax (4 bytes).
 *
 * Generic: any legacy i386 client of the removed xmlMemSetup/xmlMemGet override.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gap.h"

/* malloc/free/realloc resolve, same-image, to libabiconv's low-4GB heap
 * (malloc_shim.c), so every pointer handed back is 32-bit representable. */

/* ---- the four allocator trampolines' C bodies (MTSHIM C-impl ABI) ---------
 * Each receives a = &i386_args[0]; returns the 4-byte i386 value in eax. The
 * pointer args are low-4GB i386 pointers, zero-extended into the dword slots. */

int shim_xml_malloc(uint32_t *a) {
   void *p = malloc((size_t)a[0]);
   return (int)(uint32_t)(uintptr_t)p;
}

int shim_xml_free(uint32_t *a) {
   free((void *)(uintptr_t)a[0]);
   return 0;
}

int shim_xml_realloc(uint32_t *a) {
   void *p = realloc((void *)(uintptr_t)a[0], (size_t)a[1]);
   return (int)(uint32_t)(uintptr_t)p;
}

/* xmlStrdup duplicates a NUL-terminated string onto the low-4GB heap. */
int shim_xml_strdup(uint32_t *a) {
   const char *s = (const char *)(uintptr_t)a[0];
   if (!s)
      return 0;
   size_t n = strlen(s) + 1;
   char *p = (char *)malloc(n);
   if (p)
      memcpy(p, s, n);
   return (int)(uint32_t)(uintptr_t)p;
}

/* ---- the deprecated override API ------------------------------------------ */

int shim_xmlMemSetup(uint32_t *a) {
   GAP_STUB(a);        /* deprecated override: ignore the legacy allocators */
   return 0;       /* report success; libxml2 keeps its default allocators */
}

/* The MTSHIM trampolines whose addresses ___xmlMemGet hands out. Declared as
 * data so we can take their (low-4GB) addresses; they are never called as C
 * functions from here. */
extern char xml_free_tramp[], xml_malloc_tramp[], xml_realloc_tramp[],
            xml_strdup_tramp[];

/* xmlMemGet(xmlFreeFunc *freeFunc, xmlMallocFunc *mallocFunc,
 *           xmlReallocFunc *reallocFunc, xmlStrdupFunc *strdupFunc) */
int shim_xmlMemGet(uint32_t *a) {
   uint32_t *p_free    = (uint32_t *)(uintptr_t)a[0];
   uint32_t *p_malloc  = (uint32_t *)(uintptr_t)a[1];
   uint32_t *p_realloc = (uint32_t *)(uintptr_t)a[2];
   uint32_t *p_strdup  = (uint32_t *)(uintptr_t)a[3];

   uintptr_t t_free    = (uintptr_t)xml_free_tramp;
   uintptr_t t_malloc  = (uintptr_t)xml_malloc_tramp;
   uintptr_t t_realloc = (uintptr_t)xml_realloc_tramp;
   uintptr_t t_strdup  = (uintptr_t)xml_strdup_tramp;

   /* The trampolines live in libabiconv's __TEXT, which loads in the low-4GB
    * window (like every translated image — the dlsym/cb thunk pools rely on
    * the same), so the stored 32-bit pointers do not truncate. */

   if (p_free)    *p_free    = (uint32_t)t_free;
   if (p_malloc)  *p_malloc  = (uint32_t)t_malloc;
   if (p_realloc) *p_realloc = (uint32_t)t_realloc;
   if (p_strdup)  *p_strdup  = (uint32_t)t_strdup;

   return 0;       /* report success; the four trampolines are now stored */
}
