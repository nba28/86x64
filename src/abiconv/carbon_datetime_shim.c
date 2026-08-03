/*
 * carbon_datetime_shim.c — ONE job: the classic Date & Time Utilities, which
 * modern macOS REMOVED.
 *
 * WHY. SecondsToDate / DateToSeconds / LongSecondsToDate / LongDateToSeconds
 * are gone from CoreServices — MEASURED, not assumed: a dlsym for each raises
 * AttributeError on macOS 26.6, alongside GetDateTime. abigen still emits a
 * bridge for them (they are in the consider set via the 10.6 headers), the
 * translate pipeline weakens the now-dangling bind to NULL, and the bridge's
 * `call` therefore jumps to 0.
 *
 * MEASURED on Halo CE (2026-08-03), once the CFBundle-fnptr fix carried it past
 * the wild-FILE* crash: `rip = 0`, and M64_FAULT_REPORT's [rsp] read named the
 * call site as ___SecondsToDate.l1, with rdi = 0xe6965ba3 (a Mac-epoch seconds
 * value) and rsi = 0x814df99e (an i386 stack DateTimeRec). A jump to NULL is
 * unrecoverable, so every classic app that formats a date dies here.
 *
 * ── THE CLASSIC CONTRACT ────────────────────────────────────────────────────
 * The classic epoch is 1904-01-01 00:00:00 in LOCAL time (not UTC — this is the
 * detail that silently shifts every converted date by the timezone offset if
 * you reach for gmtime instead). The Unix epoch is 2082844800 seconds later.
 *
 *   DateTimeRec  = 7 x SInt16 : year month day hour minute second dayOfWeek
 *   LongDateRec  = 14 x SInt16: era year month day hour minute second dayOfWeek
 *                               dayOfYear weekOfYear pm res1 res2 res3
 *   LongDateTime = SInt64 seconds (passed BY POINTER, so no split-slot issue)
 *
 * Classic dayOfWeek is 1-based with Sunday == 1, i.e. tm_wday + 1; dayOfYear is
 * tm_yday + 1; `pm` is 1 from noon on; `era` is 0 for AD.
 *
 * UNIVERSAL: every classic app that converts or displays a date reaches these.
 * Nothing here is Halo-specific.
 *
 *   kill switch : M64_NO_CARBON_DATETIME=1  (restores the pre-fix behaviour by
 *                 leaving the out-params ZEROED and returning — NOT by calling
 *                 through, because the native symbol does not exist and the
 *                 call would jump to NULL. The OFF arm is therefore "the
 *                 conversion does not happen", which is what the app saw.)
 *   trace       : ABICONV_DATETIME_TRACE=1
 *
 * ★Rule A (see carbon_ui_shim.c / the FSSpec fix 2394051): a shim that declines
 * must still leave EVERY out-param DEFINED. A caller reading its own
 * uninitialised stack as a date is exactly the class of bug that cost us the
 * Halo nil-CFStringRef hunt, so the kill-switch arm zeroes rather than skips.
 */

#include "carbon_shim.h"
#include <string.h>
#include <time.h>
/* stdio/stdlib by header, NOT hand-declared: on macOS `stderr` is a macro for
 * __stderrp, so `extern void *stderr;` mints an undefined _stderr that nothing
 * defines and every client of libabiconv then fails to load. */
#include <stdio.h>
#include <stdlib.h>

/* Seconds between 1904-01-01 and 1970-01-01. */
#define MAC_EPOCH_DELTA 2082844800LL

static int datetime_enabled(void)
{
   static int v = -1;
   if (v < 0) {
      const char *e = getenv("M64_NO_CARBON_DATETIME");
      v = !(e && *e && *e != '0');
   }
   return v;
}

static int datetime_trace(void)
{
   static int v = -1;
   if (v < 0) {
      const char *e = getenv("ABICONV_DATETIME_TRACE");
      v = (e && *e && *e != '0');
   }
   return v;
}

/* Fill a struct tm from classic seconds. Classic uses LOCAL time. */
static int mac_secs_to_tm(int64_t secs, struct tm *out)
{
   time_t t = (time_t)(secs - MAC_EPOCH_DELTA);
   return localtime_r(&t, out) != NULL;
}

/* Inverse: a classic broken-down local time back to classic seconds. */
static int64_t tm_to_mac_secs(int year, int mon, int day, int hour, int min, int sec)
{
   struct tm tm;
   memset(&tm, 0, sizeof tm);
   tm.tm_year  = year - 1900;
   tm.tm_mon   = mon - 1;
   tm.tm_mday  = day;
   tm.tm_hour  = hour;
   tm.tm_min   = min;
   tm.tm_sec   = sec;
   tm.tm_isdst = -1;              /* let mktime decide DST for that date */
   time_t t = mktime(&tm);
   if (t == (time_t)-1) return 0;
   return (int64_t)t + MAC_EPOCH_DELTA;
}

/* ---- DateTimeRec (7 x SInt16) ------------------------------------------ */

static void fill_datetimerec(int16_t *d, const struct tm *tm)
{
   d[0] = (int16_t)(tm->tm_year + 1900);
   d[1] = (int16_t)(tm->tm_mon + 1);
   d[2] = (int16_t)tm->tm_mday;
   d[3] = (int16_t)tm->tm_hour;
   d[4] = (int16_t)tm->tm_min;
   d[5] = (int16_t)tm->tm_sec;
   d[6] = (int16_t)(tm->tm_wday + 1);        /* classic: Sunday == 1 */
}

/* void SecondsToDate(UInt32 secs, DateTimeRec *d); */
uint32_t shim_SecondsToDate(uint32_t *a)
{
   int16_t *d = (int16_t *)i386_ptr(a[1]);
   if (!d) return 0;
   memset(d, 0, 7 * sizeof(int16_t));        /* rule A: defined even if we bail */
   if (!datetime_enabled()) return 0;

   struct tm tm;
   if (!mac_secs_to_tm((int64_t)(uint32_t)a[0], &tm)) return 0;
   fill_datetimerec(d, &tm);
   if (datetime_trace())
      fprintf(stderr, "[datetime] SecondsToDate(%u) -> %04d-%02d-%02d %02d:%02d:%02d dow=%d\n",
              a[0], d[0], d[1], d[2], d[3], d[4], d[5], d[6]);
   return 0;
}

/* void DateToSeconds(const DateTimeRec *d, UInt32 *secs); */
uint32_t shim_DateToSeconds(uint32_t *a)
{
   const int16_t *d = (const int16_t *)i386_ptr(a[0]);
   uint32_t *out = (uint32_t *)i386_ptr(a[1]);
   if (out) *out = 0;                        /* rule A */
   if (!d || !out || !datetime_enabled()) return 0;

   int64_t s = tm_to_mac_secs(d[0], d[1], d[2], d[3], d[4], d[5]);
   *out = (uint32_t)s;
   if (datetime_trace())
      fprintf(stderr, "[datetime] DateToSeconds(%04d-%02d-%02d %02d:%02d:%02d) -> %u\n",
              d[0], d[1], d[2], d[3], d[4], d[5], *out);
   return 0;
}

/* ---- LongDateRec (14 x SInt16) + LongDateTime (SInt64 by pointer) ------- */
enum { LD_ERA, LD_YEAR, LD_MONTH, LD_DAY, LD_HOUR, LD_MINUTE, LD_SECOND,
       LD_DAYOFWEEK, LD_DAYOFYEAR, LD_WEEKOFYEAR, LD_PM, LD_RES1, LD_RES2, LD_RES3,
       LD_COUNT };

/* void LongSecondsToDate(const LongDateTime *lSecs, LongDateRec *lDate); */
uint32_t shim_LongSecondsToDate(uint32_t *a)
{
   const int64_t *ls = (const int64_t *)i386_ptr(a[0]);
   int16_t *d = (int16_t *)i386_ptr(a[1]);
   if (!d) return 0;
   memset(d, 0, LD_COUNT * sizeof(int16_t)); /* rule A */
   if (!ls || !datetime_enabled()) return 0;

   struct tm tm;
   if (!mac_secs_to_tm(*ls, &tm)) return 0;
   d[LD_ERA]        = 0;                     /* AD */
   d[LD_YEAR]       = (int16_t)(tm.tm_year + 1900);
   d[LD_MONTH]      = (int16_t)(tm.tm_mon + 1);
   d[LD_DAY]        = (int16_t)tm.tm_mday;
   d[LD_HOUR]       = (int16_t)tm.tm_hour;
   d[LD_MINUTE]     = (int16_t)tm.tm_min;
   d[LD_SECOND]     = (int16_t)tm.tm_sec;
   d[LD_DAYOFWEEK]  = (int16_t)(tm.tm_wday + 1);
   d[LD_DAYOFYEAR]  = (int16_t)(tm.tm_yday + 1);
   /* Classic weeks start on Sunday and the week containing Jan 1 is week 1. */
   d[LD_WEEKOFYEAR] = (int16_t)((tm.tm_yday + 7 - tm.tm_wday) / 7 + 1);
   d[LD_PM]         = (int16_t)(tm.tm_hour >= 12);
   if (datetime_trace())
      fprintf(stderr, "[datetime] LongSecondsToDate(%lld) -> %04d-%02d-%02d %02d:%02d:%02d\n",
              (long long)*ls, d[LD_YEAR], d[LD_MONTH], d[LD_DAY],
              d[LD_HOUR], d[LD_MINUTE], d[LD_SECOND]);
   return 0;
}

/* void LongDateToSeconds(const LongDateRec *lDate, LongDateTime *lSecs); */
uint32_t shim_LongDateToSeconds(uint32_t *a)
{
   const int16_t *d = (const int16_t *)i386_ptr(a[0]);
   int64_t *out = (int64_t *)i386_ptr(a[1]);
   if (out) *out = 0;                        /* rule A */
   if (!d || !out || !datetime_enabled()) return 0;

   *out = tm_to_mac_secs(d[LD_YEAR], d[LD_MONTH], d[LD_DAY],
                         d[LD_HOUR], d[LD_MINUTE], d[LD_SECOND]);
   if (datetime_trace())
      fprintf(stderr, "[datetime] LongDateToSeconds(%04d-%02d-%02d %02d:%02d:%02d) -> %lld\n",
              d[LD_YEAR], d[LD_MONTH], d[LD_DAY], d[LD_HOUR], d[LD_MINUTE],
              d[LD_SECOND], (long long)*out);
   return 0;
}
