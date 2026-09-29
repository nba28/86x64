/* 99_heap_guard — does M64_HEAP_GUARD=1 fault AT an overrun / use-after-free
 * write, instead of letting it corrupt the heap for someone else to trip on?
 * (Halo "new campaign" 2026-09-29: a free block's `next` read 6, written 8
 * bytes past its neighbour's end; the crash surfaced much later in malloc.)
 * mode=over: write 8 bytes past a 48-byte block. mode=uaf: write into a freed
 * block. Prints "survived=1" only if the write did NOT fault. */
extern void exit(int);
extern void *malloc(unsigned long);
extern void free(void *);
extern char *getenv(const char *);
extern long write(int, const void *, unsigned long);   /* unbuffered: the ON arm dies */
int main(void)
{
   const char *m = getenv("GUARD_MODE");
   volatile unsigned char *p = malloc(48);
   write(1, "allocated=1\n", 12);
   if (m && m[0] == 'o') { p[48 + 8] = 6; }
   else if (m && m[0] == 'u') { free((void *)p); p[0] = 6; }
   write(1, "survived=1\n", 11);
   exit(0);
}
