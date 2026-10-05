/* 99_getsegbyname_absent — getsegbyname of a segment the executable does not
 * have must return NULL to the i386 caller and return to it. The raw native
 * bind popped an 8-byte return address (Call of Duty 4 `__BOOKKEEPING`).
 * ON exits 42. OFF (M64_NO_GETSEGBYNAME=1, run time): the raw call -> crash. */
extern void exit(int);
extern int printf(const char *, ...);
const void *getsegbyname(const char *);
int main(void) {
   const void *s = getsegbyname("__BOOKKEEPING");
   printf("seg=%p\n", s);
   exit(s == 0 ? 42 : 1);
}
