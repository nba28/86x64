/* 99_memerr — does MemError() report what our Memory Manager just did?
 * Halo's realloc wrapper grows with SetPtrSize and falls back to allocate+copy
 * only when MemError() says it failed. MemError was a bridge to the NATIVE
 * Memory Manager, which never sees our shims: it said noErr, Halo wrote past
 * the block and corrupted the heap ("new campaign" crash, 2026-09-29). */
extern int  printf(const char *, ...);
extern void exit(int);
extern char *NewPtr(long);
extern void  SetPtrSize(char *, long);
extern short MemError(void);
int main(void)
{
   char *p = NewPtr(16);
   short e0 = MemError();
   SetPtrSize(p, 4096);            /* a Ptr cannot move: this must fail */
   short e1 = MemError();
   SetPtrSize(p, 8);               /* shrinking always succeeds */
   short e2 = MemError();
   printf("newptr_ok=%d grow_reports_error=%d shrink_ok=%d\n", p && e0 == 0, e1 != 0, e2 == 0);
   exit(0);
}
