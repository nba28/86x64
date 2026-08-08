/*
 * mp_memory_shim.c — Carbon Multiprocessing Services' MEMORY allocators on
 * libabiconv's low-4GB heap.
 *
 * ONE JOB: MPAllocateAligned / MPAllocate / MPFree / MPGetAllocatedBlockSize.
 * Nothing else in MP Services (tasks, queues, semaphores, timers) belongs here.
 *
 * WHY (measured, Halo CE 2026-08-08, see the Halo AGL deploy journal):
 * these live in CarbonCore and are in abigen's consider-set, so abigen generated
 * a marshalling bridge for each. `LogicalAddress` is `typedef void *`, so the
 * return hits abigen's generic void*-return rule (abigen.cc:1644-1656):
 *
 *     mov rdi, rax / shr rdi, 32 / jz .cfretlow / call _x64_objc_wrap
 *
 * i.e. "a native pointer above 4GB is minted as a 32-bit PROXY HANDLE". That is
 * correct for an OPAQUE REFERENCE the app only ever hands back to us (the rule
 * exists for CFArrayGetValueAtIndex / CFDictionaryGetValue). It is catastrophic
 * for an ALLOCATOR: a proxy handle is an 8-byte slot in the shared proxy arena,
 * and the app treats the returned address as N bytes of its own memory.
 *
 * MEASURED consequence: Halo asked for 64KB blocks and received arena handles
 * 8 BYTES APART (`[wrap] slot=4279 handle=0x808095b8 real=0x7faf78008010`,
 * `slot=4280 handle=0x808095c0 real=0x7faf78018010`, ...). It wrote its audio
 * data straight over the live handle table, then zero-filled the tail with
 * memset(buf, 0, 0x3390); the memset bridge's arg unwrap saw an address inside
 * [g_arena_base, g_arena_end), read that (still virgin, zero) slot and passed 0
 * as the destination -> SIGSEGV writing to 0x0 with err=0x6.
 *
 * WHY malloc() does not already explode from the SAME abigen rule: ___malloc,
 * ___calloc, ___realloc, ___valloc and the whole ___malloc_type_* family carry
 * the identical conditional wrap (1068 `.cfretlow` shims in one libabiconv).
 * They survive only BY ACCIDENT — libinterpose forces allocations requested by a
 * LOW caller into the low 4GB (interpose.c:59 CALLER_IS_LOW()), so their high
 * half is zero and the `jz` skips the wrap. MPAllocateAligned allocates inside
 * NATIVE CarbonCore (a HIGH caller), so the forcing does not apply, it returns
 * 0x7faf7..., and the wrap fires. That asymmetry is the whole bug.
 *
 * THE UNIVERSAL RULE THIS FILE ENCODES:
 *   ★A proxy handle is only ever valid for a pointer the app never DEREFERENCES.
 *    An allocator's return is memory the app will read, write, index and
 *    suballocate, so it must be REAL memory that is 32-bit representable.
 * The trigger is the API's memory-allocation CONTRACT, never an app name: any
 * i386 Carbon program calling MP Services gets this, and Apple's own deprecation
 * note for MPAllocateAligned is literally "Use aligned_alloc()".
 *
 * Reached through mp_memory_tramp.asm's MTSHIM -> ___<sym> exports; the four
 * symbols are listed in custom.syms so abigen emits no competing definition
 * (custom.syms is abigen's `-i` ignore list — see CMakeLists.txt:228-229,265).
 *
 * KILL SWITCH: M64_NO_MP_LOW_HEAP=1 restores the OLD behaviour exactly (call
 * native MPAllocateAligned, then x64_objc_wrap the >4GB result), which
 * reproduces the corruption. That is the guard's OFF arm.
 *
 * ⚠RETRANSLATE-CLASS: static-interpose rewrites binds at TRANSLATE time, so an
 * already-translated importer keeps its native MP binds. Exporting these is not
 * enough on its own — the importing binary must be re-translated.
 */

#include "carbon_shim.h"
#include <string.h>

extern char *getenv(const char *);

/* libabiconv's low-4GB allocator (malloc_shim.c). Declared, not <stdlib.h>'d,
 * so it is unambiguous that these are the shim heap, not native libc. */
extern void  *malloc(size_t);
extern void   free(void *);
extern size_t malloc_size(const void *);

/* The native implementations, used ONLY by the kill switch. */
extern void *MPAllocateAligned(unsigned long size, unsigned char alignment,
                               unsigned int options);
extern void *MPAllocate(unsigned long size);
extern void  MPFree(void *object);
extern unsigned long MPGetAllocatedBlockSize(void *object);

/* objc_shim.c — the proxy-handle minter the old generated bridge called. */
extern uint32_t x64_objc_wrap(uint64_t real);

/* ---- Multiprocessing.h constants (SDK header, verified not remembered) ---- */
#define kMPMaxAllocSize        (1024L * 1024 * 1024)   /* 1GB */
#define kMPAllocateClearMask   0x0001u
/* alignment is a POWER-OF-TWO EXPONENT: 0 = default, 3 = 8B, 4 = 16B, 5 = 32B,
 * 10 = 1024B, 12 = 4096B, kMPAllocateMaxAlignment = 16 (64KB). 254 and 255 are
 * "pseudo values, converted at runtime" = VM page and interlock (cache line). */
#define kMPAllocateMaxAlignment      16
#define kMPAllocateVMPageAligned    254
#define kMPAllocateInterlockAligned 255

/* Kill switch, resolved once. */
static int mp_native_passthrough(void)
{
   static int cached = -1;
   if (cached < 0) { cached = getenv("M64_NO_MP_LOW_HEAP") != NULL; }
   return cached;
}

/* 16-byte header in front of every block. `delta` lets MPFree recover the
 * malloc base after an over-aligned payload was shifted forward, so every block
 * — however aligned — goes back on malloc_shim.c's FREE LIST and is reclaimed.
 * ★This is deliberately NOT posix_memalign(): that path bumps a fresh block and
 * bypasses the free list (malloc_shim.c:329-331), so a program that allocates
 * and frees aligned blocks in a loop would never reclaim any of them — the same
 * non-reclaiming-arena defect that produced the wild `FILE*` (see
 * deploy journal, `shim_FILE` low-4GB arena). MPFree must actually free. */
#define MPBLK_MAGIC 0x4d50424bu                /* 'MPBK' */
struct mpblk_hdr {
   uint32_t size;                              /* exact logical size requested */
   uint32_t magic;
   uint32_t delta;                             /* payload - malloc base */
   uint32_t pad;
};

/* Decode MPAllocateAligned's alignment byte into a byte count.
 * ★Deliberately conservative: the result is CLAMPED to [16, 4096] and we never
 * return LESS than the exponent asks for, so the block is always at least as
 * aligned as the caller requested. (Our heap's malloc already returns 16-byte
 * aligned memory, which satisfies every alignment up to and including 16B —
 * that covers kMPAllocateDefaultAligned, 8-byte and AltiVec/16-byte, i.e. the
 * overwhelming majority of real calls, with no over-allocation at all.) */
static size_t mp_alignment_bytes(unsigned char alignment)
{
   size_t a;
   if (alignment == kMPAllocateVMPageAligned ||
       alignment == kMPAllocateInterlockAligned) {
      a = 4096;                                /* page / cache-line pseudo values */
   } else if (alignment == 0) {
      a = 16;                                  /* kMPAllocateDefaultAligned */
   } else if (alignment > kMPAllocateMaxAlignment) {
      a = 4096;                                /* out of range -> clamp up, never down */
   } else {
      a = (size_t)1 << alignment;              /* the documented exponent */
   }
   if (a < 16)   { a = 16; }
   if (a > 4096) { a = 4096; }
   return a;
}

/* The one allocation path. Returns an i386-representable payload address. */
static uint32_t mp_allocate(uint32_t size, unsigned char alignment,
                            uint32_t options)
{
   size_t align = mp_alignment_bytes(alignment);
   size_t n = size ? (size_t)size : 1;         /* a 0-byte request still yields a block */

   if (n > (size_t)kMPMaxAllocSize) { return 0; }   /* documented ceiling */

   /* malloc() is 16-aligned already, so only a stricter request needs slack. */
   size_t slack = (align > 16) ? (align - 1) : 0;
   char *base = (char *)malloc(sizeof(struct mpblk_hdr) + n + slack);
   if (!base) { return 0; }

   uintptr_t first = (uintptr_t)base + sizeof(struct mpblk_hdr);
   uintptr_t user  = (first + (align - 1)) & ~(uintptr_t)(align - 1);
   /* user >= first >= base + 16, so the header always fits directly in front. */
   struct mpblk_hdr *h =
      (struct mpblk_hdr *)(user - sizeof(struct mpblk_hdr));
   h->size  = size;
   h->magic = MPBLK_MAGIC;
   h->delta = (uint32_t)(user - (uintptr_t)base);
   h->pad   = 0;

   if (options & kMPAllocateClearMask) { memset((void *)user, 0, n); }

   return to_i386((const void *)user);
}

/* Recover our header from a payload the app hands back, or NULL if this is not
 * one of ours (a block allocated before the shim existed, or a stray pointer).
 * malloc_size() reports 0 for anything our heap did not allocate, so a foreign
 * pointer can never be walked backwards into unmapped memory. */
static struct mpblk_hdr *mp_hdr(uint32_t p)
{
   if (!p) { return NULL; }
   void *user = i386_ptr(p);
   struct mpblk_hdr *h =
      (struct mpblk_hdr *)((char *)user - sizeof(struct mpblk_hdr));
   if (h->magic != MPBLK_MAGIC) { return NULL; }
   if (malloc_size((const char *)user - h->delta) == 0) { return NULL; }
   return h;
}

/* ---- i386-cdecl entry points (mp_memory_tramp.asm) ---------------------- */
/* Each shim_X reads i386 stack-arg slots from a[] and returns its 4-byte
 * result in eax. LogicalAddress and ByteCount are both 4-byte on i386. */

/* LogicalAddress MPAllocateAligned(ByteCount size, UInt8 alignment,
 *                                  OptionBits options); */
uint32_t shim_MPAllocateAligned(uint32_t *a)
{
   if (mp_native_passthrough()) {
      void *r = MPAllocateAligned((unsigned long)a[0],
                                  (unsigned char)a[1], (unsigned int)a[2]);
      /* Reproduce the DEFECT exactly as abigen's bridge did. */
      return ((uint64_t)(uintptr_t)r >> 32)
                ? x64_objc_wrap((uint64_t)(uintptr_t)r)
                : (uint32_t)(uintptr_t)r;
   }
   return mp_allocate(a[0], (unsigned char)a[1], a[2]);
}

/* LogicalAddress MPAllocate(ByteCount size); */
uint32_t shim_MPAllocate(uint32_t *a)
{
   if (mp_native_passthrough()) {
      void *r = MPAllocate((unsigned long)a[0]);
      return ((uint64_t)(uintptr_t)r >> 32)
                ? x64_objc_wrap((uint64_t)(uintptr_t)r)
                : (uint32_t)(uintptr_t)r;
   }
   return mp_allocate(a[0], 0, 0);
}

/* void MPFree(LogicalAddress object); */
uint32_t shim_MPFree(uint32_t *a)
{
   if (mp_native_passthrough()) { MPFree(i386_ptr(a[0])); return 0; }
   struct mpblk_hdr *h = mp_hdr(a[0]);
   if (h) {
      h->magic = 0;                            /* poison: a double free is inert */
      free((char *)i386_ptr(a[0]) - h->delta);
   }
   return 0;
}

/* ByteCount MPGetAllocatedBlockSize(LogicalAddress object); */
uint32_t shim_MPGetAllocatedBlockSize(uint32_t *a)
{
   if (mp_native_passthrough()) {
      return (uint32_t)MPGetAllocatedBlockSize(i386_ptr(a[0]));
   }
   struct mpblk_hdr *h = mp_hdr(a[0]);
   return h ? h->size : 0;
}
