/*
 * carbon_memory.c — the classic Mac OS Memory Manager (Handle / Ptr) on
 * libabiconv's low-4GB heap.
 *
 * WHY: NewHandle/NewPtr/DisposeHandle/GetHandleSize/... live in CarbonCore and
 * are in abigen's consider-set, so abigen generates shims that call the NATIVE
 * Memory Manager. On modern macOS that either returns a HIGH (>4GB) pointer —
 * which truncates into the i386 caller's 4-byte Handle/Ptr slot and corrupts
 * everything downstream — or is a dead stub. Either way classic Handles break.
 *
 * A classic Handle is a pointer to a "master pointer": `Handle == Ptr*`. The
 * i386 caller does `Ptr data = *handle;` (a 4-byte read) to reach the block, so
 * BOTH the master pointer and the block must be 32-bit-representable. We:
 *   - allocate the master pointer (a 4-byte slot) from the low-4GB heap,
 *   - allocate the block from the low-4GB heap with a 16-byte header in front
 *     holding the exact requested size (so GetHandleSize is exact, not the
 *     malloc-rounded capacity),
 *   - store the block address (low-4GB) into the master pointer.
 * The Handle handed to i386 is the master-pointer address; *Handle yields the
 * block. Handles never relocate in this implementation (the heap is a bump
 * allocator), so HLock/HUnlock — if ever imported — are no-ops.
 *
 * UNIVERSAL: every i386 Carbon app that allocates Handles/Ptrs benefits; this
 * is not iPhoto-specific. Reached via the ___NewHandle / ___DisposeHandle /
 * ___GetHandleSize ... trampolines in maptable_tramp.asm (static-interpose
 * redirects the binds).
 */

#include "carbon_shim.h"
#include <string.h>

/* libabiconv's low-4GB allocator (malloc_shim.c). Declared, not <stdlib.h>'d,
 * so we are unambiguous that these are the shim heap, not native libc. */
extern void *malloc(size_t);
extern void *calloc(size_t, size_t);
extern void *realloc(void *, size_t);
extern void  free(void *);

/* 16-byte header in front of every Handle block: keeps the user payload
 * 16-aligned (malloc returns 16-aligned) and records the exact logical size. */
#define HBLK_MAGIC 0x484e444cu          /* 'HNDL' */
struct hblk_hdr {
   uint32_t size;                       /* exact logical size requested */
   uint32_t magic;
   uint32_t pad0, pad1;
};

/* ---- internal API used by carbon_component.c / quicktime_image.c -------- */

uint32_t cm_new_handle(uint32_t size, int clear)
{
   uint32_t *mp = (uint32_t *)malloc(sizeof(uint32_t));     /* master pointer */
   if (!mp)
      return 0;
   char *real = (char *)malloc(sizeof(struct hblk_hdr) + size);
   if (!real) { free(mp); return 0; }

   struct hblk_hdr *h = (struct hblk_hdr *)real;
   h->size  = size;
   h->magic = HBLK_MAGIC;
   char *user = real + sizeof(struct hblk_hdr);
   if (clear && size)
      memset(user, 0, size);

   *mp = to_i386(user);
   return to_i386(mp);
}

void *cm_handle_block(uint32_t hdl)
{
   if (!hdl) return NULL;
   uint32_t *mp = (uint32_t *)i386_ptr(hdl);
   return i386_ptr(*mp);
}

uint32_t cm_handle_size(uint32_t hdl)
{
   void *user = cm_handle_block(hdl);
   if (!user) return 0;
   struct hblk_hdr *h = (struct hblk_hdr *)((char *)user - sizeof(struct hblk_hdr));
   return h->magic == HBLK_MAGIC ? h->size : 0;
}

void cm_dispose_handle(uint32_t hdl)
{
   if (!hdl) return;
   uint32_t *mp = (uint32_t *)i386_ptr(hdl);
   void *user = i386_ptr(*mp);
   if (user)
      free((char *)user - sizeof(struct hblk_hdr));
   free(mp);
}

/* ---- i386-cdecl entry points (maptable_tramp.asm) ----------------------- */
/* Each shim_X reads i386 stack-arg slots from a[] and returns its 4-byte
 * result in eax. Handle/Ptr/Size are all 4-byte on i386. */

/* Handle NewHandle(Size logicalSize); */
uint32_t shim_NewHandle(uint32_t *a)      { return cm_new_handle(a[0], 0); }
/* Handle NewHandleClear(Size logicalSize); */
uint32_t shim_NewHandleClear(uint32_t *a) { return cm_new_handle(a[0], 1); }

/* void DisposeHandle(Handle h); */
uint32_t shim_DisposeHandle(uint32_t *a)  { cm_dispose_handle(a[0]); return 0; }

/* Size GetHandleSize(Handle h); */
uint32_t shim_GetHandleSize(uint32_t *a)  { return cm_handle_size(a[0]); }

/* void SetHandleSize(Handle h, Size newSize); (OSErr reported via MemError) */
uint32_t shim_SetHandleSize(uint32_t *a)
{
   uint32_t hdl = a[0], newSize = a[1];
   if (!hdl) return cmParamErr;
   uint32_t *mp = (uint32_t *)i386_ptr(hdl);
   void *user = i386_ptr(*mp);
   if (!user) return cmParamErr;
   char *real = (char *)user - sizeof(struct hblk_hdr);
   char *nreal = (char *)realloc(real, sizeof(struct hblk_hdr) + newSize);
   if (!nreal) return cmMemFullErr;
   struct hblk_hdr *h = (struct hblk_hdr *)nreal;
   h->size  = newSize;
   h->magic = HBLK_MAGIC;
   *mp = to_i386(nreal + sizeof(struct hblk_hdr));
   return 0;
}

/* Ptr NewPtr(Size byteCount); — a nonrelocatable block (no master pointer). */
uint32_t shim_NewPtr(uint32_t *a)
{
   void *p = malloc(a[0] ? a[0] : 1);
   return to_i386(p);
}
/* Ptr NewPtrClear(Size byteCount); */
uint32_t shim_NewPtrClear(uint32_t *a)
{
   uint32_t n = a[0] ? a[0] : 1;
   void *p = calloc(1, n);
   return to_i386(p);
}
/* void DisposePtr(Ptr p); */
uint32_t shim_DisposePtr(uint32_t *a)     { free(i386_ptr(a[0])); return 0; }
