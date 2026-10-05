/* 99_tm_marshal — struct tm comes back in the i386 layout (tm_shim.c).
 *
 * gmtime/localtime/ctime returned libSystem's static native struct truncated to
 * 32 bits (Portal 2's save list: "Sunday, Jan 0 12:00 AM" for every save), and
 * gmtime_r/mktime wrote the 56-byte native struct into the caller's 44 bytes.
 * t = 365 days = 1971-01-01 00:00:00 UTC, the same on any Mac.
 * ON exits 42. OFF: M64_NO_TM_MARSHAL=1 (run time) -> wrong field (1..6) or a fault.
 */
#include <time.h>
extern int  printf(const char *, ...);
extern void exit(int);
extern int  strcmp(const char *, const char *);
extern unsigned long strlen(const char *);

int main(void) {
   int step = 0;
   time_t t = 365 * 86400;
   struct tm *g = gmtime(&t);
   if (!g || g->tm_year != 71 || g->tm_mon != 0 || g->tm_mday != 1 || g->tm_wday != 5) { step = 1; goto out; }
   struct { struct tm tm; int canary; } r;
   r.canary = 0x5a5a5a5a;
   if (!gmtime_r(&t, &r.tm) || r.tm.tm_year != 71 || r.canary != 0x5a5a5a5a) { step = 2; goto out; }
   char s[32];
   if (strftime(s, sizeof s, "%Y-%m-%d %H:%M", &r.tm) != 16 || strcmp(s, "1971-01-01 00:00")) { step = 3; goto out; }
   struct tm *l = localtime(&t);
   if (!l || l->tm_year < 70 || l->tm_year > 71 || !l->tm_zone) { step = 4; goto out; }
   struct { struct tm tm; int canary; } m;
   m.tm = *l; m.canary = 0x5a5a5a5a;
   if (mktime(&m.tm) != t || m.canary != 0x5a5a5a5a) { step = 5; goto out; }
   const char *c = ctime(&t);
   if (!c || strlen(c) != 25) { step = 6; goto out; }
   step = 42;
out:
   printf("step=%d\n", step);
   exit(step);
}
