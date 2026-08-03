/* 99_csmem_handle_family — does a classic Handle survive CarbonCore's CSMem*
 * entry points, or does it come back as a truncated 64-bit pointer?
 *
 * WHY THIS EXISTS. A classic Handle is `Ptr*` — a pointer to a "master
 * pointer". On i386 both are 4 bytes, so `Ptr data = *h;` is a 4-byte read.
 * CarbonCore exports the whole Memory Manager TWICE: under its classic names
 * (NewHandle, DisposeHandle, ...) and under a `CSMem` prefix, and the CSMem
 * spelling is what CarbonCore itself and the classic QuickTime.framework call
 * internally — the translated QuickTime imports 22 of them.
 *
 * carbon_memory.c implemented the classic names on libabiconv's low-4GB heap
 * precisely because the native 64-bit Memory Manager returns pointers that do
 * not fit an i386 slot. The CSMem half was never covered, so it bound straight
 * to native CarbonCore and broke in BOTH directions:
 *
 *   out : CSMemNewHandle returns a >4GB Handle, truncated into the 4-byte slot
 *   in  : CSMemDisposeHandle dereferences an i386 Handle as 8 bytes, reading a
 *         real 4-byte master pointer PLUS 4 bytes of adjacent heap garbage
 *
 * MEASURED (probes/badfree.m, Civilization IV 2026-08-03): native CSMemDisposePtr
 * was handed 0x0000_0037_9000_ff9c — low half a real i386 address, high half
 * garbage — and libmalloc aborted the process from inside
 * QTNewDataReferenceFromFSRef_priv.
 *
 * This guard exercises the family as a real caller uses it, because a defect
 * can hit one entry point and spare another:
 *
 *   [1] allocate + round-trip through the master pointer   (the core defect)
 *   [2] GetHandleSize is EXACT, not the malloc-rounded capacity
 *   [3] HandToHand copy, then mutate the copy               (independence)
 *   [4] HandAndHand concatenation                           (grow + append)
 *   [5] PtrToHand                                           (Ptr -> Handle)
 *   [6] EmptyHandle + ReallocateHandle                      (empty/refill)
 *   [7] Munger search-and-replace, including a GROWING replace
 *   [8] NewPtr / GetPtrSize / DisposePtr                    (the Ptr half)
 *
 * ARMS. This fixture is the ON side and must exit 0. csmem_handle_test.sh adds
 * the OFF side by re-running the same binary under M64_NO_CSMEM_SHIM=1, which
 * forwards the allocation core to the native Memory Manager — case [1] must
 * then fail or die, because a >4GB Handle cannot survive the 4-byte slot.
 *
 * Prints one key=value per line so the arms can be diffed mechanically.
 */

extern int  printf(const char *, ...);
extern void exit(int status);

typedef char *Ptr;
typedef Ptr  *Handle;
typedef long  Size;
typedef short OSErr;

/* Reached through libabiconv's ___CSMem* trampolines (carbon_memory_tramp.asm);
 * static-interpose binds these at translate time. Declared here because the
 * i386 sysroot has no CarbonCore headers. */
extern Handle CSMemNewHandle(Size);
extern Handle CSMemNewHandleClear(Size);
extern void   CSMemDisposeHandle(Handle);
extern Size   CSMemGetHandleSize(Handle);
extern OSErr  CSMemSetHandleSize(Handle, Size);
extern OSErr  CSMemHandToHand(Handle *);
extern OSErr  CSMemHandAndHand(Handle, Handle);
extern OSErr  CSMemPtrToHand(const void *, Handle *, Size);
extern void   CSMemEmptyHandle(Handle);
extern OSErr  CSMemReallocateHandle(Handle, Size);
extern long   CSMemMunger(Handle, long, const void *, long, const void *, long);
extern Ptr    CSMemNewPtr(Size);
extern Size   CSMemGetPtrSize(Ptr);
extern void   CSMemDisposePtr(Ptr);

static int eq(const char *a, const char *b, int n)
{
   for (int i = 0; i < n; i++) if (a[i] != b[i]) return 0;
   return 1;
}
static int len_of(const char *s) { int n = 0; while (s[n]) n++; return n; }
static void cp(char *d, const char *s, int n) { for (int i = 0; i < n; i++) d[i] = s[i]; }

int main(void)
{
   /* [1] allocate and round-trip through the master pointer. If the Handle came
    * from the native 64-bit heap this deref reads a truncated pointer. */
   Handle h = CSMemNewHandle(16);
   printf("h_nonnull=%d\n", h != 0);
   if (!h) { printf("fatal=1\n"); exit(1); }
   cp(*h, "ABCDEFGHIJKLMNOP", 16);
   printf("roundtrip=%d\n", eq(*h, "ABCDEFGHIJKLMNOP", 16));

   /* [2] the size must be EXACT — a caller that trusts a malloc-rounded
    * capacity walks off the end of its own data. */
   printf("size_exact=%d\n", CSMemGetHandleSize(h) == 16);

   /* [3] HandToHand replaces the slot with an independent COPY. */
   Handle c = h;
   printf("h2h_err=%d\n", CSMemHandToHand(&c) == 0);
   printf("h2h_distinct=%d\n", c != h);
   printf("h2h_content=%d\n", eq(*c, "ABCDEFGHIJKLMNOP", 16));
   (*c)[0] = 'z';
   printf("h2h_indep=%d\n", (*h)[0] == 'A');   /* mutating the copy must not touch h */

   /* [4] HandAndHand appends hand1 onto hand2, growing hand2. */
   printf("hah_err=%d\n", CSMemHandAndHand(h, c) == 0);
   printf("hah_size=%d\n", CSMemGetHandleSize(c) == 32);
   printf("hah_tail=%d\n", eq(*c + 16, "ABCDEFGHIJKLMNOP", 16));

   /* [5] PtrToHand builds a Handle from raw bytes. */
   Handle p2h = 0;
   printf("p2h_err=%d\n", CSMemPtrToHand("wxyz", &p2h, 4) == 0);
   printf("p2h_ok=%d\n", p2h != 0 && CSMemGetHandleSize(p2h) == 4 && eq(*p2h, "wxyz", 4));

   /* [6] EmptyHandle frees the block but KEEPS the Handle valid, so
    * ReallocateHandle can refill it — the classic purge/refill idiom. */
   CSMemEmptyHandle(p2h);
   printf("empty_null=%d\n", *p2h == 0);
   printf("realloc_err=%d\n", CSMemReallocateHandle(p2h, 8) == 0);
   printf("realloc_ok=%d\n", *p2h != 0 && CSMemGetHandleSize(p2h) == 8);

   /* [7] Munger: replace "CDE" with "12345" — a GROWING replace, which must
    * move the tail and resize the handle rather than overwrite past the end. */
   Handle m = CSMemNewHandle(8);
   cp(*m, "ABCDEFGH", 8);
   long at = CSMemMunger(m, 0, "CDE", 3, "12345", 5);
   printf("munger_at=%d\n", at == 2);
   printf("munger_size=%d\n", CSMemGetHandleSize(m) == 10);
   printf("munger_content=%d\n", eq(*m, "AB12345FGH", 10));

   /* [8] the Ptr half. */
   Ptr p = CSMemNewPtr(24);
   printf("ptr_nonnull=%d\n", p != 0);
   if (p) { cp(p, "0123456789", 10); printf("ptr_size=%d\n", CSMemGetPtrSize(p) == 24); }

   CSMemDisposePtr(p);
   CSMemDisposeHandle(m);
   CSMemDisposeHandle(p2h);
   CSMemDisposeHandle(c);
   CSMemDisposeHandle(h);
   printf("disposed=1\n");   /* reaching here at all means no malloc abort */

   (void)len_of;
   printf("done=1\n");
   exit(0);   /* the 86x64.sh wrapper enters _main via jmp: no return frame */
}
