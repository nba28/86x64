/*
 * tm_shim.c — struct tm in the i386 layout.
 *
 * i386 struct tm is 44 bytes (9 ints, 4-byte tm_gmtoff, 4-byte tm_zone); the
 * native one is 56. The abigen bridge passed both directions through raw:
 * localtime/gmtime/ctime returned a pointer to libSystem's static storage,
 * truncated to 32 bits (Portal 2's save list read zeros: "Sunday, Jan 0
 * 12:00 AM" for every save), and localtime_r/gmtime_r/mktime wrote 56 bytes
 * into the caller's 44.
 *
 * Static-result calls rebuild the answer in a per-thread low-memory buffer
 * (the classic contract: the next call overwrites it); the rest convert the
 * caller's struct in and out. tm_zone points at an interned low copy of the
 * zone name. Kill M64_NO_TM_MARSHAL (the raw native behaviour); guard
 * tm-marshal.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

struct tm32 { int32_t f[9]; int32_t gmtoff; uint32_t zone; };

static int marshal_off(void) {
    static int v = -1;
    if (v < 0) v = getenv("M64_NO_TM_MARSHAL") != NULL;
    return v;
}

/* Zone names live forever (tm_zone outlives the call): intern each once in
 * the low heap (libabiconv's malloc). */
static uint32_t zone_low(const char *z) {
    static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
    static char *names[16];   /* ponytail: 16 distinct zones, then NULL tm_zone */
    if (!z) return 0;
    uint32_t r = 0;
    pthread_mutex_lock(&mu);
    for (int i = 0; i < 16; i++) {
        if (!names[i]) {   /* not strdup: libSystem's would allocate high */
            const size_t n = strlen(z) + 1;
            if ((names[i] = malloc(n))) memcpy(names[i], z, n);
        }
        if (names[i] && strcmp(names[i], z) == 0) { r = (uint32_t)(uintptr_t)names[i]; break; }
    }
    pthread_mutex_unlock(&mu);
    return r;
}

static void tm_out(struct tm32 *d, const struct tm *s) {
    const int *f = &s->tm_sec;
    for (int i = 0; i < 9; i++) d->f[i] = f[i];
    d->gmtoff = (int32_t)s->tm_gmtoff;
    d->zone = zone_low(s->tm_zone);
}
static void tm_in(struct tm *d, const struct tm32 *s) {
    memset(d, 0, sizeof *d);
    int *f = &d->tm_sec;
    for (int i = 0; i < 9; i++) f[i] = s->f[i];
    d->tm_gmtoff = s->gmtoff;
    d->tm_zone = (char *)(uintptr_t)s->zone;
}

#define P(i) ((void *)(uintptr_t)a[(i)])
static time_t t_in(uint32_t p) { return p ? (time_t)*(const int32_t *)(uintptr_t)p : 0; }

static uint32_t tm_static(struct tm *(*fn_r)(const time_t *, struct tm *), uint32_t tp) {
    static __thread struct tm32 *buf;
    struct tm n;
    if (!tp) return 0;
    if (!buf && !(buf = malloc(sizeof *buf))) return 0;
    const time_t t = t_in(tp);
    if (!fn_r(&t, &n)) return 0;
    tm_out(buf, &n);
    return (uint32_t)(uintptr_t)buf;
}

/* struct tm *localtime(const time_t *) */
uint32_t shim_localtime(uint32_t *a) {
    if (marshal_off()) { const time_t t = t_in(a[0]); return (uint32_t)(uintptr_t)localtime(&t); }
    return tm_static(localtime_r, a[0]);
}
/* struct tm *gmtime(const time_t *) */
uint32_t shim_gmtime(uint32_t *a) {
    if (marshal_off()) { const time_t t = t_in(a[0]); return (uint32_t)(uintptr_t)gmtime(&t); }
    return tm_static(gmtime_r, a[0]);
}

static uint32_t tm_r(struct tm *(*fn_r)(const time_t *, struct tm *), uint32_t *a) {
    struct tm n;
    const time_t t = t_in(a[0]);
    if (!a[1]) return 0;
    if (marshal_off()) { return fn_r(&t, (struct tm *)P(1)) ? a[1] : 0; }
    if (!fn_r(&t, &n)) return 0;
    tm_out((struct tm32 *)P(1), &n);
    return a[1];
}
/* struct tm *localtime_r(const time_t *, struct tm *) */
uint32_t shim_localtime_r(uint32_t *a) { return tm_r(localtime_r, a); }
/* struct tm *gmtime_r(const time_t *, struct tm *) */
uint32_t shim_gmtime_r(uint32_t *a) { return tm_r(gmtime_r, a); }

/* time_t mktime(struct tm *) — normalizes the caller's struct in place */
uint32_t shim_mktime(uint32_t *a) {
    if (marshal_off()) return (uint32_t)mktime((struct tm *)P(0));
    struct tm n;
    tm_in(&n, (const struct tm32 *)P(0));
    const time_t t = mktime(&n);
    tm_out((struct tm32 *)P(0), &n);
    return (uint32_t)t;
}

/* size_t strftime(char *, size_t, const char *, const struct tm *) */
uint32_t shim_strftime(uint32_t *a) {
    if (marshal_off()) return (uint32_t)strftime(P(0), a[1], P(2), (const struct tm *)P(3));
    struct tm n;
    tm_in(&n, (const struct tm32 *)P(3));
    return (uint32_t)strftime(P(0), a[1], P(2), &n);
}

/* char *ctime(const time_t *) — static string, 26 bytes */
uint32_t shim_ctime(uint32_t *a) {
    static __thread char *buf;
    const time_t t = t_in(a[0]);
    if (marshal_off()) return (uint32_t)(uintptr_t)ctime(&t);
    if (!a[0] || (!buf && !(buf = malloc(26)))) return 0;
    return ctime_r(&t, buf) ? (uint32_t)(uintptr_t)buf : 0;
}
