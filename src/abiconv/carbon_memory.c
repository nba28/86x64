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
#include "gap.h"
extern char *getenv(const char *);

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

/* Ptr blocks carry the SAME hblk_hdr as Handle blocks. They did not before,
 * but GetPtrSize/SetPtrSize (added with the CSMem* family below) need the exact
 * logical size, and our low-4GB allocator has no malloc_size() to ask. The
 * header is invisible to the i386 caller, which only ever sees `user`.
 * ★Every Ptr allocation and every Ptr free MUST go through this pair — a
 * free() of the user pointer would be off by sizeof(struct hblk_hdr). */
uint32_t cm_new_ptr(uint32_t size, int clear)
{
   uint32_t n = size ? size : 1;
   char *real = (char *)malloc(sizeof(struct hblk_hdr) + n);
   if (!real) return 0;
   struct hblk_hdr *h = (struct hblk_hdr *)real;
   h->size = n; h->magic = HBLK_MAGIC;
   char *user = real + sizeof(struct hblk_hdr);
   if (clear) memset(user, 0, n);
   return to_i386(user);
}

uint32_t cm_ptr_size(uint32_t p)
{
   void *user = i386_ptr(p);
   if (!user) return 0;
   struct hblk_hdr *h = (struct hblk_hdr *)((char *)user - sizeof(struct hblk_hdr));
   return h->magic == HBLK_MAGIC ? h->size : 0;
}

void cm_dispose_ptr(uint32_t p)
{
   void *user = i386_ptr(p);
   if (user) free((char *)user - sizeof(struct hblk_hdr));
}

/* ---- i386-cdecl entry points (maptable_tramp.asm) ----------------------- */
/* Each shim_X reads i386 stack-arg slots from a[] and returns its 4-byte
 * result in eax. Handle/Ptr/Size are all 4-byte on i386. */

/* MemError(): the classic Memory Manager reports void/pointer calls through a
 * last-error value the caller reads NEXT. It was an abigen bridge to the NATIVE
 * MemError, which never sees these shims, so it always said noErr. MEASURED
 * (Halo "new campaign", 2026-09-29, M64_HEAP_GUARD): Halo's realloc wrapper
 * calls SetPtrSize to grow, checks MemError, and only on an error falls back to
 * allocate+copy. SetPtrSize refused (a Ptr cannot move) but MemError said
 * noErr, so Halo memset 8 bytes past the 16-byte block and corrupted the next
 * heap header; the crash surfaced much later inside malloc. Every call below
 * that reports through MemError sets it; per thread, as the caller reads it
 * right after on the same thread. */
static __thread int32_t g_memerr;
static uint32_t me(uint32_t err) { g_memerr = (int16_t)err; return err; }
static uint32_t me_ptr(uint32_t p) { g_memerr = p ? 0 : cmMemFullErr; return p; }
/* OSErr MemError(void); */
uint32_t shim_MemError(uint32_t *a) { (void)a; return (uint32_t)g_memerr; }

/* Handle NewHandle(Size logicalSize); */
uint32_t shim_NewHandle(uint32_t *a)      { return me_ptr(cm_new_handle(a[0], 0)); }
/* Handle NewHandleClear(Size logicalSize); */
uint32_t shim_NewHandleClear(uint32_t *a) { return me_ptr(cm_new_handle(a[0], 1)); }

/* void DisposeHandle(Handle h); */
uint32_t shim_DisposeHandle(uint32_t *a)  { cm_dispose_handle(a[0]); return me(0); }

/* Size GetHandleSize(Handle h); */
uint32_t shim_GetHandleSize(uint32_t *a)  { return cm_handle_size(a[0]); }

/* void SetHandleSize(Handle h, Size newSize); (OSErr reported via MemError) */
uint32_t shim_SetHandleSize(uint32_t *a)
{
   uint32_t hdl = a[0], newSize = a[1];
   if (!hdl) return me(cmParamErr);
   uint32_t *mp = (uint32_t *)i386_ptr(hdl);
   void *user = i386_ptr(*mp);
   if (!user) return me(cmParamErr);
   char *real = (char *)user - sizeof(struct hblk_hdr);
   char *nreal = (char *)realloc(real, sizeof(struct hblk_hdr) + newSize);
   if (!nreal) return me(cmMemFullErr);
   struct hblk_hdr *h = (struct hblk_hdr *)nreal;
   h->size  = newSize;
   h->magic = HBLK_MAGIC;
   *mp = to_i386(nreal + sizeof(struct hblk_hdr));
   return me(0);
}

/* Ptr blocks carry the SAME 16-byte header as Handle blocks so GetPtrSize is
 * exact and SetPtrSize can realloc. The i386 caller only ever sees `user`. */

/* Ptr NewPtr(Size byteCount); — a nonrelocatable block (no master pointer). */
uint32_t shim_NewPtr(uint32_t *a)         { return me_ptr(cm_new_ptr(a[0], 0)); }
/* Ptr NewPtrClear(Size byteCount); */
uint32_t shim_NewPtrClear(uint32_t *a)    { return me_ptr(cm_new_ptr(a[0], 1)); }
/* void DisposePtr(Ptr p); */
uint32_t shim_DisposePtr(uint32_t *a)     { cm_dispose_ptr(a[0]); return me(0); }

/* ======================================================================== *
 *  The rest of the classic Memory Manager, and CarbonCore's CSMem* twins.
 *
 *  WHY THIS SECTION EXISTS. Everything above was reached only under the
 *  classic names. CarbonCore also exports the SAME functions as `CSMem*`, and
 *  that is what CarbonCore itself and the classic QuickTime.framework call
 *  internally: the translated QuickTime imports 22 of them. None were shimmed,
 *  so they bound straight to the native 64-bit Memory Manager with i386-layout
 *  arguments — the exact defect this file's header describes, in both
 *  directions. CSMemNewHandle handed the i386 caller a truncated >4GB Handle,
 *  and CSMemDisposeHandle dereferenced an i386 4-byte master pointer as 8
 *  bytes. MEASURED (probes/badfree.m, Civilization IV 2026-08-03): the pointer
 *  native CSMemDisposePtr tried to free was 0x0000_0037_9000_ff9c — low half a
 *  real i386 address, high half adjacent heap garbage — and libmalloc aborted
 *  the process.
 *
 *  So the CSMem* entry points are aliases onto this same implementation, and
 *  the family is completed here rather than left half-covered: a partial family
 *  only moves the abort to the next unshimmed entry point.
 *
 *  ⚠RETRANSLATE-CLASS. static-interpose rewrites binds at TRANSLATE time, so an
 *  already-translated framework keeps its native binds; exporting these is not
 *  enough on its own, the importer must be re-translated.
 * ======================================================================== */

/* Handles never relocate in this implementation (see the header), so the
 * lock/purge state is a value we store and hand back rather than something the
 * heap has to honour. A classic caller uses HGetState/HSetState only to
 * save-and-restore around code that might move memory; since nothing moves,
 * round-tripping the byte is the complete and correct behaviour. */
static uint8_t cm_state_of(uint32_t hdl) { (void)hdl; return 0; }

/* Handle NewEmptyHandle(void); — a master pointer with a NULL block. */
uint32_t shim_NewEmptyHandle(uint32_t *a)
{
   (void)a;
   uint32_t *mp = (uint32_t *)malloc(sizeof(uint32_t));
   if (!mp) return me_ptr(0);
   *mp = 0;
   return me_ptr(to_i386(mp));
}

/* void EmptyHandle(Handle h); — free the block, KEEP the master pointer.
 * The Handle stays valid and *h becomes NULL, which is the whole point: a
 * caller can ReallocateHandle it later. */
uint32_t shim_EmptyHandle(uint32_t *a)
{
   uint32_t hdl = a[0];
   if (!hdl) return me(0);
   uint32_t *mp = (uint32_t *)i386_ptr(hdl);
   void *user = i386_ptr(*mp);
   if (user) free((char *)user - sizeof(struct hblk_hdr));
   *mp = 0;
   return me(0);
}

/* OSErr ReallocateHandle(Handle h, Size newSize); — give an (often emptied)
 * Handle a fresh block of newSize. Unlike SetHandleSize the old contents are
 * explicitly NOT preserved, so this allocates rather than reallocs. */
uint32_t shim_ReallocateHandle(uint32_t *a)
{
   uint32_t hdl = a[0], newSize = a[1];
   if (!hdl) return me(cmParamErr);
   uint32_t *mp = (uint32_t *)i386_ptr(hdl);
   void *old = i386_ptr(*mp);
   if (old) free((char *)old - sizeof(struct hblk_hdr));
   *mp = 0;
   char *real = (char *)malloc(sizeof(struct hblk_hdr) + newSize);
   if (!real) return me(cmMemFullErr);
   struct hblk_hdr *h = (struct hblk_hdr *)real;
   h->size = newSize; h->magic = HBLK_MAGIC;
   *mp = to_i386(real + sizeof(struct hblk_hdr));
   return me(0);
}

/* Handle RecoverHandle(Ptr p); — given a block, find its Handle.
 * Our blocks carry no back-pointer to the master pointer, and classic callers
 * use this almost exclusively on a Ptr they got from *h moments earlier. There
 * is no correct answer we can synthesise, so return 0 (the classic failure
 * value) rather than a fabricated Handle that would corrupt on dispose.
 * Recorded as a known gap; if a target is ever MEASURED depending on this, the
 * fix is a back-pointer field in struct hblk_hdr (there are two spare words). */
uint32_t shim_RecoverHandle(uint32_t *a)  { GAP_STUB(a); return 0; }

/* Size GetPtrSize(Ptr p); */
uint32_t shim_GetPtrSize(uint32_t *a)     { return cm_ptr_size(a[0]); }

/* void SetPtrSize(Ptr p, Size newSize); (OSErr via MemError) — a Ptr is
 * nonrelocatable, so unlike SetHandleSize we cannot move it. Grow in place if
 * the header's capacity allows, else report memFullErr, which is exactly what
 * the classic Memory Manager did when the following block was in use. */
uint32_t shim_SetPtrSize(uint32_t *a)
{
   uint32_t p = a[0], newSize = a[1];
   void *user = i386_ptr(p);
   if (!user) return me(cmParamErr);
   struct hblk_hdr *h = (struct hblk_hdr *)((char *)user - sizeof(struct hblk_hdr));
   if (h->magic != HBLK_MAGIC) return me(cmParamErr);
   if (newSize <= h->size) { h->size = newSize; return me(0); }
   return me(cmMemFullErr);
}

/* OSErr PtrToHand(const void *srcPtr, Handle *dstHndl, Size size); */
uint32_t shim_PtrToHand(uint32_t *a)
{
   uint32_t src = a[0], size = a[2];
   uint32_t *out = (uint32_t *)i386_ptr(a[1]);
   if (out) *out = 0;                    /* rule A: define the out-param first */
   uint32_t hdl = cm_new_handle(size, 0);
   if (!hdl) return me(cmMemFullErr);
   if (size && src) memcpy(cm_handle_block(hdl), i386_ptr(src), size);
   if (out) *out = hdl;
   return me(0);
}

/* OSErr PtrToXHand(const void *srcPtr, Handle dstHndl, Size size); — copy into
 * an EXISTING handle, resizing it. */
uint32_t shim_PtrToXHand(uint32_t *a)
{
   uint32_t src = a[0], hdl = a[1], size = a[2];
   if (!hdl) return me(cmParamErr);
   uint32_t rz[2]; rz[0] = hdl; rz[1] = size;
   uint32_t err = shim_SetHandleSize(rz);
   if (err) return me(err);
   if (size && src) memcpy(cm_handle_block(hdl), i386_ptr(src), size);
   return me(0);
}

/* OSErr HandToHand(Handle *theHndl); — replace *theHndl with a COPY. */
uint32_t shim_HandToHand(uint32_t *a)
{
   uint32_t *slot = (uint32_t *)i386_ptr(a[0]);
   if (!slot) return me(cmParamErr);
   uint32_t src = *slot;
   if (!src) return me(cmParamErr);
   uint32_t size = cm_handle_size(src);
   uint32_t dst = cm_new_handle(size, 0);
   if (!dst) return me(cmMemFullErr);
   if (size) memcpy(cm_handle_block(dst), cm_handle_block(src), size);
   *slot = dst;
   return me(0);
}

/* OSErr HandAndHand(Handle hand1, Handle hand2); — append hand1 onto hand2. */
uint32_t shim_HandAndHand(uint32_t *a)
{
   uint32_t h1 = a[0], h2 = a[1];
   if (!h1 || !h2) return me(cmParamErr);
   uint32_t n1 = cm_handle_size(h1), n2 = cm_handle_size(h2);
   uint32_t rz[2]; rz[0] = h2; rz[1] = n1 + n2;
   uint32_t err = shim_SetHandleSize(rz);
   if (err) return me(err);
   if (n1) memcpy((char *)cm_handle_block(h2) + n2, cm_handle_block(h1), n1);
   return me(0);
}

/* OSErr PtrAndHand(const void *ptr1, Handle hand2, Size size); */
uint32_t shim_PtrAndHand(uint32_t *a)
{
   uint32_t src = a[0], h2 = a[1], size = a[2];
   if (!h2) return me(cmParamErr);
   uint32_t n2 = cm_handle_size(h2);
   uint32_t rz[2]; rz[0] = h2; rz[1] = n2 + size;
   uint32_t err = shim_SetHandleSize(rz);
   if (err) return me(err);
   if (size && src) memcpy((char *)cm_handle_block(h2) + n2, i386_ptr(src), size);
   return me(0);
}

/* char HGetState(Handle h); / void HSetState(Handle h, char flags); */
uint32_t shim_HGetState(uint32_t *a)      { return cm_state_of(a[0]); }
uint32_t shim_HSetState(uint32_t *a)      { GAP_STUB(a); return 0; }

/* long Munger(Handle h, long offset, const void *ptr1, long len1,
 *             const void *ptr2, long len2);
 * The classic search-and-replace primitive. Two modes, per Inside Macintosh:
 *   ptr1 == NULL : REPLACE len1 bytes at `offset` with ptr2/len2 (no search).
 *   ptr1 != NULL : SEARCH from `offset` for the len1-byte pattern; if found,
 *                  replace it with ptr2/len2 and return the match offset.
 * In both modes a NULL ptr2 with len2 == 0 is a pure DELETE. Returns the offset
 * of the change, or a negative error if the pattern was not found. */
uint32_t shim_Munger(uint32_t *a)
{
   uint32_t hdl = a[0];
   int32_t  off = (int32_t)a[1];
   uint32_t p1 = a[2]; int32_t len1 = (int32_t)a[3];
   uint32_t p2 = a[4]; int32_t len2 = (int32_t)a[5];
   if (!hdl) return (uint32_t)cmParamErr;

   int32_t size = (int32_t)cm_handle_size(hdl);
   char *base = (char *)cm_handle_block(hdl);
   if (!base || off < 0 || off > size) return (uint32_t)cmParamErr;

   int32_t at = off, cut = len1;
   if (p1) {                                   /* search mode */
      const char *pat = (const char *)i386_ptr(p1);
      at = -1;
      for (int32_t i = off; len1 > 0 && i + len1 <= size; i++)
         if (memcmp(base + i, pat, (size_t)len1) == 0) { at = i; break; }
      if (at < 0) return (uint32_t)-1;          /* not found */
      cut = len1;
   }
   if (cut < 0) cut = 0;
   if (at + cut > size) cut = size - at;
   if (len2 < 0) len2 = 0;

   int32_t nsize = size - cut + len2;
   int32_t tail  = size - at - cut;             /* bytes after the cut */

   if (nsize > size) {                          /* grow BEFORE moving the tail */
      uint32_t rz[2]; rz[0] = hdl; rz[1] = (uint32_t)nsize;
      if (shim_SetHandleSize(rz)) return (uint32_t)cmMemFullErr;
      base = (char *)cm_handle_block(hdl);      /* realloc may have moved it */
   }
   if (tail > 0) memmove(base + at + len2, base + at + cut, (size_t)tail);
   if (len2 > 0 && p2) memcpy(base + at, i386_ptr(p2), (size_t)len2);
   if (nsize < size) {                          /* shrink AFTER moving the tail */
      uint32_t rz[2]; rz[0] = hdl; rz[1] = (uint32_t)nsize;
      shim_SetHandleSize(rz);
   }
   return (uint32_t)at;
}

/* ---- CarbonCore's CSMem* twins: the same functions, the same code. -------
 * Thin forwarders rather than asm aliases so the mapping is greppable and a
 * future divergence (if CarbonCore ever differed from the classic entry point)
 * has an obvious place to live.
 *
 * KILL SWITCH M64_NO_CSMEM_SHIM=1 restores the pre-fix behaviour by forwarding
 * the ALLOCATION/DISPOSE core straight to the native 64-bit Memory Manager,
 * which is what these names bound to before this file covered them. That is the
 * OFF arm of the guard: native CSMemNewHandle returns a >4GB Handle which
 * truncates into the i386 caller's 4-byte slot, so the very first `*h`
 * dereference reads garbage. Only the core six are switchable — once allocation
 * comes from the native heap, every other entry point in the family is already
 * operating on a Handle we did not make, so a separate OFF path for them would
 * prove nothing extra. */
extern void *CSMemNewHandle(long);
extern void *CSMemNewHandleClear(long);
extern void  CSMemDisposeHandle(void *);
extern long  CSMemGetHandleSize(void *);
extern void *CSMemNewPtr(long);
extern void  CSMemDisposePtr(void *);

static int csmem_shim_enabled(void)
{
   static int v = -1;
   if (v < 0) {
      const char *e = getenv("M64_NO_CSMEM_SHIM");
      v = !(e && *e && *e != '0');
   }
   return v;
}

#define CSMEM_ALIAS(name) \
   uint32_t shim_CSMem##name(uint32_t *a) { return shim_##name(a); }

/* The six with a native OFF path, written out rather than macro-generated so
 * the forwarding is visible at the call site. to_i386() truncates exactly as
 * the unshimmed bind did — that truncation IS the defect being reproduced. */
uint32_t shim_CSMemNewHandle(uint32_t *a)
{
   if (!csmem_shim_enabled()) return to_i386(CSMemNewHandle((long)a[0]));
   return shim_NewHandle(a);
}
uint32_t shim_CSMemNewHandleClear(uint32_t *a)
{
   if (!csmem_shim_enabled()) return to_i386(CSMemNewHandleClear((long)a[0]));
   return shim_NewHandleClear(a);
}
uint32_t shim_CSMemDisposeHandle(uint32_t *a)
{
   if (!csmem_shim_enabled()) { CSMemDisposeHandle(i386_ptr(a[0])); return 0; }
   return shim_DisposeHandle(a);
}
uint32_t shim_CSMemGetHandleSize(uint32_t *a)
{
   if (!csmem_shim_enabled()) return (uint32_t)CSMemGetHandleSize(i386_ptr(a[0]));
   return shim_GetHandleSize(a);
}
uint32_t shim_CSMemNewPtr(uint32_t *a)
{
   if (!csmem_shim_enabled()) return to_i386(CSMemNewPtr((long)a[0]));
   return shim_NewPtr(a);
}
uint32_t shim_CSMemDisposePtr(uint32_t *a)
{
   if (!csmem_shim_enabled()) { CSMemDisposePtr(i386_ptr(a[0])); return 0; }
   return shim_DisposePtr(a);
}

/* The remaining sixteen: pure aliases (no separate OFF path -- see above). */
CSMEM_ALIAS(NewEmptyHandle)   CSMEM_ALIAS(EmptyHandle)
CSMEM_ALIAS(SetHandleSize)    CSMEM_ALIAS(ReallocateHandle)
CSMEM_ALIAS(RecoverHandle)    CSMEM_ALIAS(HandToHand)
CSMEM_ALIAS(HandAndHand)      CSMEM_ALIAS(PtrToHand)
CSMEM_ALIAS(PtrToXHand)       CSMEM_ALIAS(PtrAndHand)
CSMEM_ALIAS(Munger)           CSMEM_ALIAS(HGetState)
CSMEM_ALIAS(HSetState)        CSMEM_ALIAS(NewPtrClear)
CSMEM_ALIAS(GetPtrSize)       CSMEM_ALIAS(SetPtrSize)

/* ---- BlockMove / BlockZero: removed from macOS, and NOT optional ---------
 * find_null_jump_bridges.py flagged them as NULL-JUMP ORPHANS (the native
 * definitions are gone, so abigen's bridges jumped to 0), and almost every
 * classic app calls BlockMoveData. Size is a SIGNED 32-bit count on i386: a
 * count <= 0 is a no-op, never a 4 GB memmove. The "Uncached" cache hint means
 * nothing on x86_64, so those are the same copy. */
static void cm_block_move(uint32_t src, uint32_t dst, int32_t n)
{
   if (src && dst && n > 0) memmove(i386_ptr(dst), i386_ptr(src), (size_t)n);
}
static void cm_block_zero(uint32_t dst, int32_t n)
{
   if (dst && n > 0) memset(i386_ptr(dst), 0, (size_t)n);
}

uint32_t shim_BlockMove(uint32_t *a)             { cm_block_move(a[0], a[1], (int32_t)a[2]); return 0; }
uint32_t shim_BlockMoveData(uint32_t *a)         { cm_block_move(a[0], a[1], (int32_t)a[2]); return 0; }
uint32_t shim_BlockMoveUncached(uint32_t *a)     { cm_block_move(a[0], a[1], (int32_t)a[2]); return 0; }
uint32_t shim_BlockMoveDataUncached(uint32_t *a) { cm_block_move(a[0], a[1], (int32_t)a[2]); return 0; }
uint32_t shim_BlockZero(uint32_t *a)             { cm_block_zero(a[0], (int32_t)a[1]); return 0; }
uint32_t shim_BlockZeroData(uint32_t *a)         { cm_block_zero(a[0], (int32_t)a[1]); return 0; }

/* ---- Temp* (MultiFinder temporary memory) + MaxBlock --------------------
 * Bink's Mac allocator (libBinkMachOx86, Call of Duty 4 / Portal 2) asks
 * MaxBlock() whether a NewPtr of the request fits, else takes a TempNewHandle
 * and TempHLocks it. MaxBlock/TempH* were removed from 64-bit CarbonCore (a
 * NULL jump), and the surviving native TempNewHandle hands back a >4GB Handle
 * the i386 caller truncates. On OS X temporary memory IS the application heap,
 * so these are the ordinary Handle calls with the OSErr out-param defined. */
/* Size MaxBlock(void): the heap is unbounded; report the largest classic Size
 * a caller can round-trip without overflow, as OS X itself did. */
uint32_t shim_MaxBlock(uint32_t *a) { (void)a; return 0x7FFFFFF0u; }
/* Handle TempNewHandle(Size, OSErr *resultCode); */
uint32_t shim_TempNewHandle(uint32_t *a)
{
   uint32_t h = me_ptr(cm_new_handle(a[0], 0));
   if (a[1]) *(int16_t *)i386_ptr(a[1]) = (int16_t)g_memerr;
   return h;
}
/* void TempHLock / TempHUnlock / TempDisposeHandle(Handle, OSErr *resultCode);
 * our handles never move, so the locks only report success. */
static uint32_t temp_ok(uint32_t err) { if (err) *(int16_t *)i386_ptr(err) = 0; return me(0); }
uint32_t shim_TempHLock(uint32_t *a)         { return temp_ok(a[1]); }
uint32_t shim_TempHUnlock(uint32_t *a)       { return temp_ok(a[1]); }
uint32_t shim_TempDisposeHandle(uint32_t *a) { cm_dispose_handle(a[0]); return temp_ok(a[1]); }
