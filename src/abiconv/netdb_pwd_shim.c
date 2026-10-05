/*
 * netdb_pwd_shim.c — libc lookups that return a pointer to a static struct, in
 * the i386 layout.
 *
 * gethostbyname & co. (struct hostent / servent / protoent) and getpwuid & co.
 * (struct passwd / group) hand back a pointer to libSystem's static storage. The
 * abigen bridge returned that native x86_64 struct as-is, truncated to 32 bits,
 * and the i386 caller read it with 4-byte pointer fields: Portal 2's
 * NET_GetLocalAddress -> gethostbyname("<host>.local") read h_addr_list from
 * native offset 0x10, which is h_addrtype (AF_INET = 2), and dereferenced 2.
 * It only bit once the machine's own name resolved.
 *
 * Each call rebuilds the answer in the i386 layout, strings and lists included,
 * in a per-thread low-memory buffer per struct kind: the classic contract (the
 * next call of the same kind overwrites it) with the pointers the caller can
 * reach. Kill M64_NO_NETDB_MARSHAL (the raw native pointer); guard
 * netdb-pwd-marshal.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <pwd.h>
#include <grp.h>
#include "gap.h"

#define S(i) ((const char *)(uintptr_t)a[(i)])

static int marshal_off(void) {
    static int v = -1;
    if (v < 0) v = getenv("M64_NO_NETDB_MARSHAL") != NULL;
    return v;
}

/* A per-thread bump buffer below 4GB (libabiconv's malloc is the low heap). */
#define LO_CAP 0x10000   /* ponytail: fixed 64KB per kind; a group with more members fails loudly */
struct lo { char *base; size_t used; int full; };
static void *lo_alloc(struct lo *b, size_t n) {
    n = (n + 3) & ~(size_t)3;
    if (!b->base && !(b->base = (char *)malloc(LO_CAP))) { b->full = 1; return NULL; }
    if (b->used + n > LO_CAP) { b->full = 1; return NULL; }
    void *p = b->base + b->used;
    b->used += n;
    return p;
}
static uint32_t lo_bytes(struct lo *b, const void *src, size_t n) {
    void *p = src ? lo_alloc(b, n) : NULL;
    if (!p) return 0;
    memcpy(p, src, n);
    return (uint32_t)(uintptr_t)p;
}
static uint32_t lo_str(struct lo *b, const char *s) { return s ? lo_bytes(b, s, strlen(s) + 1) : 0; }
/* A NULL-terminated vector: of C strings (len 0) or of len-byte records. */
static uint32_t lo_vec(struct lo *b, char *const *v, size_t len) {
    size_t n = 0;
    if (!v) return 0;
    while (v[n]) n++;
    uint32_t *out = (uint32_t *)lo_alloc(b, (n + 1) * 4);
    if (!out) return 0;
    for (size_t i = 0; i < n; i++) out[i] = len ? lo_bytes(b, v[i], len) : lo_str(b, v[i]);
    out[n] = 0;
    return (uint32_t)(uintptr_t)out;
}
static uint32_t lo_done(struct lo *b, void *rec, const char *what) {
    if (!b->full) return (uint32_t)(uintptr_t)rec;
    GAP_ONCE("overflow", what, 0, 0);
    return 0;
}

/* ---- struct hostent: h_name, h_aliases, h_addrtype, h_length, h_addr_list ---- */
static uint32_t put_hostent(const struct hostent *h) {
    static __thread struct lo b;
    if (!h) return 0;
    b.used = 0; b.full = 0;
    uint32_t *r = (uint32_t *)lo_alloc(&b, 5 * 4);
    if (!r) return lo_done(&b, NULL, "hostent");
    r[0] = lo_str(&b, h->h_name);
    r[1] = lo_vec(&b, h->h_aliases, 0);
    r[2] = (uint32_t)h->h_addrtype;
    r[3] = (uint32_t)h->h_length;
    r[4] = lo_vec(&b, h->h_addr_list, (size_t)h->h_length);
    return lo_done(&b, r, "hostent");
}
/* struct hostent *gethostbyname(const char *) */
uint32_t shim_gethostbyname(uint32_t *a) {
    struct hostent *h = gethostbyname(S(0));
    return marshal_off() ? (uint32_t)(uintptr_t)h : put_hostent(h);
}
/* struct hostent *gethostbyaddr(const void *, socklen_t, int) */
uint32_t shim_gethostbyaddr(uint32_t *a) {
    struct hostent *h = gethostbyaddr((const void *)(uintptr_t)a[0], (socklen_t)a[1], (int)a[2]);
    return marshal_off() ? (uint32_t)(uintptr_t)h : put_hostent(h);
}

/* ---- struct servent: s_name, s_aliases, s_port, s_proto ---- */
static uint32_t put_servent(const struct servent *s) {
    static __thread struct lo b;
    if (!s) return 0;
    b.used = 0; b.full = 0;
    uint32_t *r = (uint32_t *)lo_alloc(&b, 4 * 4);
    if (!r) return lo_done(&b, NULL, "servent");
    r[0] = lo_str(&b, s->s_name);
    r[1] = lo_vec(&b, s->s_aliases, 0);
    r[2] = (uint32_t)s->s_port;
    r[3] = lo_str(&b, s->s_proto);
    return lo_done(&b, r, "servent");
}
/* struct servent *getservbyname(const char *name, const char *proto) */
uint32_t shim_getservbyname(uint32_t *a) {
    struct servent *s = getservbyname(S(0), S(1));
    return marshal_off() ? (uint32_t)(uintptr_t)s : put_servent(s);
}
/* struct servent *getservbyport(int port, const char *proto) */
uint32_t shim_getservbyport(uint32_t *a) {
    struct servent *s = getservbyport((int)a[0], S(1));
    return marshal_off() ? (uint32_t)(uintptr_t)s : put_servent(s);
}

/* ---- struct protoent: p_name, p_aliases, p_proto ---- */
/* struct protoent *getprotobyname(const char *) */
uint32_t shim_getprotobyname(uint32_t *a) {
    static __thread struct lo b;
    struct protoent *p = getprotobyname(S(0));
    if (marshal_off()) return (uint32_t)(uintptr_t)p;
    if (!p) return 0;
    b.used = 0; b.full = 0;
    uint32_t *r = (uint32_t *)lo_alloc(&b, 3 * 4);
    if (!r) return lo_done(&b, NULL, "protoent");
    r[0] = lo_str(&b, p->p_name);
    r[1] = lo_vec(&b, p->p_aliases, 0);
    r[2] = (uint32_t)p->p_proto;
    return lo_done(&b, r, "protoent");
}

/* ---- struct passwd (i386: 10 words, time_t is 4 bytes) ----
 * pw_name, pw_passwd, pw_uid, pw_gid, pw_change, pw_class, pw_gecos, pw_dir,
 * pw_shell, pw_expire */
static uint32_t put_passwd(const struct passwd *p) {
    static __thread struct lo b;
    if (!p) return 0;
    b.used = 0; b.full = 0;
    uint32_t *r = (uint32_t *)lo_alloc(&b, 10 * 4);
    if (!r) return lo_done(&b, NULL, "passwd");
    r[0] = lo_str(&b, p->pw_name);
    r[1] = lo_str(&b, p->pw_passwd);
    r[2] = (uint32_t)p->pw_uid;
    r[3] = (uint32_t)p->pw_gid;
    r[4] = (uint32_t)p->pw_change;
    r[5] = lo_str(&b, p->pw_class);
    r[6] = lo_str(&b, p->pw_gecos);
    r[7] = lo_str(&b, p->pw_dir);
    r[8] = lo_str(&b, p->pw_shell);
    r[9] = (uint32_t)p->pw_expire;
    return lo_done(&b, r, "passwd");
}
uint32_t shim_getpwuid(uint32_t *a) {
    struct passwd *p = getpwuid((uid_t)a[0]);
    return marshal_off() ? (uint32_t)(uintptr_t)p : put_passwd(p);
}
uint32_t shim_getpwnam(uint32_t *a) {
    struct passwd *p = getpwnam(S(0));
    return marshal_off() ? (uint32_t)(uintptr_t)p : put_passwd(p);
}
uint32_t shim_getpwent(uint32_t *a) {
    (void)a;
    struct passwd *p = getpwent();
    return marshal_off() ? (uint32_t)(uintptr_t)p : put_passwd(p);
}

/* ---- struct group: gr_name, gr_passwd, gr_gid, gr_mem ---- */
static uint32_t put_group(const struct group *g) {
    static __thread struct lo b;
    if (!g) return 0;
    b.used = 0; b.full = 0;
    uint32_t *r = (uint32_t *)lo_alloc(&b, 4 * 4);
    if (!r) return lo_done(&b, NULL, "group");
    r[0] = lo_str(&b, g->gr_name);
    r[1] = lo_str(&b, g->gr_passwd);
    r[2] = (uint32_t)g->gr_gid;
    r[3] = lo_vec(&b, g->gr_mem, 0);
    return lo_done(&b, r, "group");
}
uint32_t shim_getgrgid(uint32_t *a) {
    struct group *g = getgrgid((gid_t)a[0]);
    return marshal_off() ? (uint32_t)(uintptr_t)g : put_group(g);
}
uint32_t shim_getgrnam(uint32_t *a) {
    struct group *g = getgrnam(S(0));
    return marshal_off() ? (uint32_t)(uintptr_t)g : put_group(g);
}
uint32_t shim_getgrent(uint32_t *a) {
    (void)a;
    struct group *g = getgrent();
    return marshal_off() ? (uint32_t)(uintptr_t)g : put_group(g);
}
