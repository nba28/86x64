/*
 * carbon_datetime_shim.c — i386 glue for the classic Date & Time Utilities,
 * which modern macOS REMOVED. The implementation is shimdb/impl/datetime.c
 * (linked hidden; native callers get it through shimgen).
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
 *
 * ★Rule A (see carbon_ui_shim.c / the FSSpec fix 2394051): a shim that declines
 * must still leave EVERY out-param DEFINED. A caller reading its own
 * uninitialised stack as a date is exactly the class of bug that cost us the
 * Halo nil-CFStringRef hunt, so the kill-switch arm zeroes rather than skips.
 */

#include "carbon_shim.h"
#include <stdlib.h>
#include <string.h>

void SecondsToDate(uint32_t secs, void *dateTimeRec);
void DateToSeconds(const void *dateTimeRec, uint32_t *secs);
void LongSecondsToDate(const int64_t *lsecs, int16_t *longDateRec);
void LongDateToSeconds(const int16_t *longDateRec, int64_t *lsecs);

static int datetime_enabled(void)
{
   static int v = -1;
   if (v < 0) {
      const char *e = getenv("M64_NO_CARBON_DATETIME");
      v = !(e && *e && *e != '0');
   }
   return v;
}

/* OFF arm: out-params zeroed, no conversion (what the app saw before). */
uint32_t shim_SecondsToDate(uint32_t *a)
{
   void *d = i386_ptr(a[1]);
   if (!datetime_enabled()) { if (d) memset(d, 0, 7 * sizeof(int16_t)); return 0; }
   SecondsToDate(a[0], d);
   return 0;
}
uint32_t shim_DateToSeconds(uint32_t *a)
{
   uint32_t *out = (uint32_t *)i386_ptr(a[1]);
   if (!datetime_enabled()) { if (out) *out = 0; return 0; }
   DateToSeconds(i386_ptr(a[0]), out);
   return 0;
}
uint32_t shim_LongSecondsToDate(uint32_t *a)
{
   int16_t *d = (int16_t *)i386_ptr(a[1]);
   if (!datetime_enabled()) { if (d) memset(d, 0, 14 * sizeof(int16_t)); return 0; }
   LongSecondsToDate((const int64_t *)i386_ptr(a[0]), d);
   return 0;
}
uint32_t shim_LongDateToSeconds(uint32_t *a)
{
   int64_t *out = (int64_t *)i386_ptr(a[1]);
   if (!datetime_enabled()) { if (out) *out = 0; return 0; }
   LongDateToSeconds((const int16_t *)i386_ptr(a[0]), out);
   return 0;
}
