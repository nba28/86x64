/*
 * 99_mp_allocate_buffer.c — Carbon Multiprocessing Services' allocator must
 * hand back REAL MEMORY, not a proxy handle.
 *
 * `MPAllocateAligned` returns `LogicalAddress` = `typedef void *`, so abigen's
 * generic void*-return rule (abigen.cc:1644-1656) minted a 32-bit PROXY ARENA
 * HANDLE for its >4GB native pointer. A proxy handle is an 8-byte slot in the
 * shared arena; the caller believes it owns N bytes there.
 *
 * MEASURED on Halo CE (2026-08-08): consecutive 64KB requests came back 8 BYTES
 * APART (`handle=0x808095b8`, `handle=0x808095c0`, ...). Halo wrote its audio
 * data over the live handle table and then crashed in memset — the memset
 * bridge unwrapped the "buffer" address back through the arena and passed 0.
 *
 * The decisive property, and the one this fixture pins, needs no knowledge of
 * arena internals: ★TWO N-BYTE ALLOCATIONS THAT ARE BOTH LIVE MUST BE AT LEAST
 * N BYTES APART, AND WRITING TO ONE MUST NOT CHANGE THE OTHER.★ That is checked
 * FIRST, before any bulk write, so the evidence is printed even if the OFF arm
 * later dies scribbling over the arena.
 *
 * Prints one key=value per line so the arms can be diffed mechanically.
 * ★No libc headers: the i386 sysroot's <stdio.h> pulls <_types.h>, which is not
 * present, and the fortified <string.h> memset resolves to ___memset_chk. Every
 * other fixture here declares what it needs by hand; this one does the same.
 */

extern int printf(const char *, ...);
/* fflush(NULL) flushes every stream and needs no FILE* data symbol. The OFF arm
 * is EXPECTED to die once it scribbles over the proxy arena, and a
 * block-buffered stdout would lose every line of evidence printed before the
 * fault — which is exactly what made this fixture look like it crashed on its
 * first call when it was in fact running much further. */
extern int fflush(void *);
extern void exit(int status);
#define FLUSH() fflush(0)

/* Reached through libabiconv's ___MP* trampolines (mp_memory_tramp.asm);
 * static-interpose binds these at translate time. Declared here because the
 * i386 sysroot has no CarbonCore headers. */
extern void         *MPAllocateAligned(unsigned long size, unsigned char align,
                                       unsigned int options);
extern void         *MPAllocate(unsigned long size);
extern void          MPFree(void *object);
extern unsigned long MPGetAllocatedBlockSize(void *object);

#define kMPAllocateClearMask        0x0001u
#define kMPAllocate16ByteAligned    4     /* exponent: 2^4  = 16   */
#define kMPAllocate4096ByteAligned  12    /* exponent: 2^12 = 4096 */

#define N 4096u                           /* block size used throughout */

static void ok(const char *k, int cond) { printf("%s=%d\n", k, cond ? 1 : 0); FLUSH(); }

/* Hand-rolled so nothing routes through a fortified libc bridge. */
static void fill(unsigned char *p, unsigned char v, unsigned n)
{
   for (unsigned i = 0; i < n; i++) { p[i] = v; }
}
static int all_are(const unsigned char *p, unsigned char v, unsigned n)
{
   for (unsigned i = 0; i < n; i++) { if (p[i] != v) { return 0; } }
   return 1;
}

int main(void)
{
   /* ---- the decisive pair, checked before any bulk write ---------------- */
   unsigned char *a = (unsigned char *)
      MPAllocateAligned(N, kMPAllocate16ByteAligned, 0);
   unsigned char *b = (unsigned char *)
      MPAllocateAligned(N, kMPAllocate16ByteAligned, 0);

   ok("a_nonnull", a != 0);
   ok("b_nonnull", b != 0);
   if (!a || !b) { printf("done=0\n"); FLUSH(); exit(1); }

   /* Both live at once, so their spans must not overlap. With the proxy-handle
    * defect this gap is 8 and the check fails. */
   unsigned long gap = (a < b) ? (unsigned long)(b - a) : (unsigned long)(a - b);
   printf("gap=%lu\n", gap); FLUSH();
   ok("distinct", gap >= N);

   /* Independence, demonstrated rather than inferred: stamp each block with a
    * different byte and require both survive. */
   fill(a, 0xA5, N);
   fill(b, 0x5C, N);
   ok("a_intact", all_are(a, 0xA5, N));
   ok("b_intact", all_are(b, 0x5C, N));

   /* ---- the block really is N bytes of our own memory ------------------- */
   ok("size_exact", MPGetAllocatedBlockSize(a) == N);

   /* Every byte is writable and reads back — a proxy handle gives 8. */
   int readback = 1;
   for (unsigned i = 0; i < N; i++) { a[i] = (unsigned char)(i * 31u + 7u); }
   for (unsigned i = 0; i < N; i++) {
      if (a[i] != (unsigned char)(i * 31u + 7u)) { readback = 0; break; }
   }
   ok("readback", readback);

   /* ---- alignment is honoured ------------------------------------------- */
   unsigned char *pg = (unsigned char *)
      MPAllocateAligned(N, kMPAllocate4096ByteAligned, 0);
   ok("pg_nonnull", pg != 0);
   ok("pg_aligned", pg && ((unsigned long)pg & 4095u) == 0);

   /* ---- kMPAllocateClearMask zeroes the block --------------------------- */
   unsigned char *z = (unsigned char *)
      MPAllocateAligned(N, kMPAllocate16ByteAligned, kMPAllocateClearMask);
   ok("cleared", z != 0 && all_are(z, 0, N));

   /* ---- MPAllocate (the unaligned sibling) ------------------------------ */
   unsigned char *m = (unsigned char *)MPAllocate(N);
   ok("mp_nonnull", m != 0);
   ok("mp_size", m != 0 && MPGetAllocatedBlockSize(m) == N);

   /* ---- MPFree must ACTUALLY FREE --------------------------------------- */
   /* Free a block and re-request the same size: a real free list hands the very
    * same address back. This is what distinguishes a reclaiming allocator from
    * a bump arena that leaks every block (the defect that produced the wild
    * FILE*), and it fails if the shim is ever "simplified" to posix_memalign,
    * which bypasses the free list by design (malloc_shim.c:329-331). */
   MPFree(z);
   unsigned char *z2 = (unsigned char *)
      MPAllocateAligned(N, kMPAllocate16ByteAligned, 0);
   ok("reclaim_same_addr", z2 == z);

   /* Sustained churn must not exhaust the heap. */
   int churn = 1;
   for (int i = 0; i < 512; i++) {
      void *t = MPAllocateAligned(N, kMPAllocate16ByteAligned, 0);
      if (!t) { churn = 0; break; }
      MPFree(t);
   }
   ok("churn", churn);

   MPFree(a); MPFree(b); MPFree(pg); MPFree(z2); MPFree(m);
   printf("done=1\n"); FLUSH();
   /* exit(), not `return`: the 86x64.sh wrapper enters _main via jmp, so there
    * is NO return frame -- returning pops garbage and jumps to it (rip=0x1). */
   exit(0);
}
