/* 99_carbon_datetime — do the classic Date & Time Utilities still convert, or
 * does the call jump to NULL?
 *
 * WHY THIS EXISTS. SecondsToDate / DateToSeconds / LongSecondsToDate /
 * LongDateToSeconds were REMOVED from modern macOS (measured: a dlsym for each
 * fails on macOS 26.6). abigen still emits a bridge for them, the translate
 * pipeline weakens the now-dangling bind to NULL, and the bridge's call jumps
 * to 0 — an unrecoverable `rip = 0`. MEASURED on Halo CE (2026-08-03): once the
 * CFBundle-fnptr fix carried it past the wild-FILE* crash it died exactly here,
 * with M64_FAULT_REPORT naming ___SecondsToDate.l1 as the call site.
 *
 * The assertions are deliberately TIMEZONE-INDEPENDENT. The classic epoch is
 * 1904-01-01 in LOCAL time, so the calendar fields for a given seconds value
 * depend on the machine's timezone and cannot be hardcoded. What must hold
 * everywhere is that the conversion ROUND-TRIPS and that the fields are
 * internally consistent — which is also exactly what a caller relies on.
 *
 * ARMS: the ON side must round-trip. cfstring/datetime_test.sh adds the OFF side
 * via M64_NO_CARBON_DATETIME=1, where every out-param must come back ZEROED —
 * not garbage. That is rule A: a shim that declines still leaves its out-params
 * DEFINED, because a caller reading its own uninitialised stack as a date is the
 * bug class that cost us the Halo nil-CFStringRef hunt.
 */
extern int  printf(const char *, ...);
extern void exit(int);

typedef unsigned int  UInt32;
typedef short         SInt16;
struct DateTimeRec { SInt16 year, month, day, hour, minute, second, dayOfWeek; };
typedef long long LongDateTime;
struct LongDateRec { SInt16 f[14]; };   /* era year month day hour min sec dow ... */

extern void SecondsToDate(UInt32, struct DateTimeRec *);
extern void DateToSeconds(const struct DateTimeRec *, UInt32 *);
extern void LongSecondsToDate(const LongDateTime *, struct LongDateRec *);
extern void LongDateToSeconds(const struct LongDateRec *, LongDateTime *);

int main(void)
{
   /* A value in the classic era with no special properties. */
   const UInt32 s0 = 3000000000u;

   struct DateTimeRec d;
   d.year = -1; d.month = -1; d.dayOfWeek = -1;      /* poison, must be overwritten */
   SecondsToDate(s0, &d);
   printf("survived=1\n");                            /* reaching here = no jump to 0 */
   printf("year_sane=%d\n", d.year > 1904 && d.year < 2200);
   printf("month_sane=%d\n", d.month >= 1 && d.month <= 12);
   printf("day_sane=%d\n", d.day >= 1 && d.day <= 31);
   printf("dow_sane=%d\n", d.dayOfWeek >= 1 && d.dayOfWeek <= 7);

   UInt32 s1 = 0;
   DateToSeconds(&d, &s1);
   printf("roundtrip=%d\n", s1 == s0);

   LongDateTime ls = (LongDateTime)s0, ls2 = 0;
   struct LongDateRec ld;
   for (int i = 0; i < 14; i++) ld.f[i] = -1;
   LongSecondsToDate(&ls, &ld);
   printf("long_year=%d\n", ld.f[1] == d.year);        /* same instant, same year */
   printf("long_month=%d\n", ld.f[2] == d.month);
   printf("long_dow=%d\n", ld.f[7] == d.dayOfWeek);
   printf("long_doy=%d\n", ld.f[8] >= 1 && ld.f[8] <= 366);
   printf("long_pm=%d\n", ld.f[10] == (d.hour >= 12));
   LongDateToSeconds(&ld, &ls2);
   printf("long_roundtrip=%d\n", ls2 == ls);

   printf("done=1\n");
   exit(0);   /* the 86x64.sh wrapper enters _main via jmp: no return frame */
}
