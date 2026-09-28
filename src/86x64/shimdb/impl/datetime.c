/* shimdb curated implementations: the classic Date & Time Utilities
 * (SecondsToDate / DateToSeconds / LongSecondsToDate / LongDateToSeconds),
 * removed from modern macOS. Seconds count from 1904-01-01 local time.
 * DateTimeRec and LongDateRec are records of 16-bit fields and LongDateTime is
 * a 64-bit count, so the layout is the same for i386 and x86_64 callers;
 * libabiconv's i386 glue (carbon_datetime_shim.c) calls this same code.
 * Out-params are always written, even when the conversion fails. */
#include <stdint.h>
#include <string.h>
#include <time.h>

#define MAC_EPOCH_DELTA 2082844800LL          /* 1904-01-01 -> 1970-01-01 */

typedef struct { int16_t year, month, day, hour, minute, second, dayOfWeek; } DateTimeRec;
enum { LD_ERA, LD_YEAR, LD_MONTH, LD_DAY, LD_HOUR, LD_MINUTE, LD_SECOND,
       LD_DAYOFWEEK, LD_DAYOFYEAR, LD_WEEKOFYEAR, LD_PM, LD_COUNT = 14 };

static int mac_secs_to_tm(int64_t secs, struct tm *out) {
    time_t t = (time_t)(secs - MAC_EPOCH_DELTA);
    return localtime_r(&t, out) != NULL;
}

static int64_t tm_to_mac_secs(int year, int mon, int day, int hour, int min, int sec) {
    struct tm tm;
    memset(&tm, 0, sizeof tm);
    tm.tm_year = year - 1900; tm.tm_mon = mon - 1; tm.tm_mday = day;
    tm.tm_hour = hour; tm.tm_min = min; tm.tm_sec = sec;
    tm.tm_isdst = -1;                         /* mktime decides DST for that date */
    time_t t = mktime(&tm);
    return t == (time_t)-1 ? 0 : (int64_t)t + MAC_EPOCH_DELTA;
}

void SecondsToDate(uint32_t secs, DateTimeRec *d) {
    struct tm tm;
    if (!d) { return; }
    memset(d, 0, sizeof *d);
    if (!mac_secs_to_tm(secs, &tm)) { return; }
    d->year = (int16_t)(tm.tm_year + 1900); d->month = (int16_t)(tm.tm_mon + 1);
    d->day = (int16_t)tm.tm_mday; d->hour = (int16_t)tm.tm_hour;
    d->minute = (int16_t)tm.tm_min; d->second = (int16_t)tm.tm_sec;
    d->dayOfWeek = (int16_t)(tm.tm_wday + 1);   /* classic: Sunday == 1 */
}

void DateToSeconds(const DateTimeRec *d, uint32_t *secs) {
    if (!secs) { return; }
    *secs = d ? (uint32_t)tm_to_mac_secs(d->year, d->month, d->day,
                                         d->hour, d->minute, d->second) : 0;
}

void LongSecondsToDate(const int64_t *lsecs, int16_t *ld) {
    struct tm tm;
    if (!ld) { return; }
    memset(ld, 0, LD_COUNT * sizeof *ld);
    if (!lsecs || !mac_secs_to_tm(*lsecs, &tm)) { return; }
    ld[LD_ERA] = 0;                            /* AD */
    ld[LD_YEAR] = (int16_t)(tm.tm_year + 1900); ld[LD_MONTH] = (int16_t)(tm.tm_mon + 1);
    ld[LD_DAY] = (int16_t)tm.tm_mday; ld[LD_HOUR] = (int16_t)tm.tm_hour;
    ld[LD_MINUTE] = (int16_t)tm.tm_min; ld[LD_SECOND] = (int16_t)tm.tm_sec;
    ld[LD_DAYOFWEEK] = (int16_t)(tm.tm_wday + 1);
    ld[LD_DAYOFYEAR] = (int16_t)(tm.tm_yday + 1);
    ld[LD_WEEKOFYEAR] = (int16_t)((tm.tm_yday + 7 - tm.tm_wday) / 7 + 1);
    ld[LD_PM] = (int16_t)(tm.tm_hour >= 12);
}

void LongDateToSeconds(const int16_t *ld, int64_t *lsecs) {
    if (!lsecs) { return; }
    *lsecs = ld ? tm_to_mac_secs(ld[LD_YEAR], ld[LD_MONTH], ld[LD_DAY],
                                 ld[LD_HOUR], ld[LD_MINUTE], ld[LD_SECOND]) : 0;
}
