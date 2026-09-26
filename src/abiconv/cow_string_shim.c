/*
 * cow_string_shim.c — i386 GCC-4.x libstdc++ copy-on-write std::string /
 * std::wstring surface, reimplemented in the i386 (4-byte-field) COW layout
 * inside libabiconv.
 *
 * WHY THIS EXISTS
 * ---------------
 * A GCC-4-built i386 C++ target (Civ IV, Halo, Front Row, Portal 2...) makes
 * *out-of-line* calls to std::basic_string members that live in
 * /usr/lib/libstdc++.6.dylib.  Left un-shimmed, those binds resolve RAW to the
 * NATIVE x86_64 libstdc++.6 present in the shared cache.  Two independent ABI
 * breaks make that fatal:
 *
 *   1. Calling convention.  The i386 caller passes `this` and every argument on
 *      the stack (cdecl, 4-byte slots); native x86_64 reads them from registers
 *      (rdi/rsi/...).  A default-constructed string ctor arrives with rdi=0 and
 *      the native ctor writes through it -> NULL deref (Civ IV static-init crash
 *      inside std::basic_string()::+0xf, CvLSystem::s_szErrorMessage).
 *
 *   2. Object layout.  The i386 COW string is { uint32_t _M_p } (4 bytes) whose
 *      _M_p points past a 12-byte _Rep header {u32 length; u32 capacity;
 *      i32 refcount}.  Native libstdc++ uses 8-byte _Rep fields and an 8-byte
 *      _M_p.  Even a perfect arg marshal would still corrupt the object.  So the
 *      surface cannot be *forwarded*; it must be *reimplemented* in i386 layout.
 *
 * The reimplementation is faithful to GNU libstdc++-v3's documented COW model
 * (verified field-by-field against the SL 10.6 i386 libstdc++.6.0.9.dylib
 * disassembly: _Rep header at _M_p-12, size()=[_M_p-0xc], end()=_M_p+len*cw,
 * _M_leak deep-copies only when refcount>0 and short-circuits on the shared
 * empty rep and on an already-leaked (<0) rep, substr/operator+ return by value
 * with the Darwin-i386 callee-pops-hidden-ptr sret, non-const begin/operator[]
 * leak, list-node hook/unhook/transfer/swap on {u32 next; u32 prev}).
 *
 * Reached from translated i386-cdecl code via the trampolines in
 * cow_string_tramp.asm:  rdi -> &args[0] (4-byte cdecl slots), uint32_t result
 * in eax; the sret variant additionally callee-pops the 4-byte hidden result
 * pointer.  i386 pointers are 32-bit low-4GB addresses used directly after
 * zero-extension.  ALL allocation is plain malloc/free — libabiconv's malloc IS
 * the low-4GB heap the i386 world lives on (same as cxx_shim.c operator new).
 *
 * UNIVERSAL: keyed on the character width (cw = 1 narrow / 4 wide) and the COW
 * rep structure, never on any app.  Serves every GCC-4.x C++ i386 target.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

/* ---- the i386 COW rep header, immediately before the character data ---- */
struct Rep {
    uint32_t length;    /* _M_length  (character count, NOT bytes)  @ _M_p-12 */
    uint32_t capacity;  /* _M_capacity(character count)            @ _M_p-8  */
    int32_t  refcount;  /* _M_refcount: 0=unique, N>0=N+1 owners,  @ _M_p-4  */
                        /*              -1=leaked/unshareable                */
};

#define NPOS ((uint32_t)-1)

static struct Rep *rep_of(uint32_t p)      { return (struct Rep *)(uintptr_t)(p - 12); }
static uint8_t    *bytes_of(uint32_t p)     { return (uint8_t *)(uintptr_t)p; }
static uint32_t    data_ptr(struct Rep *r)  { return (uint32_t)(uintptr_t)((uint8_t *)r + 12); }

/* character length of a NUL-terminated i386 string buffer (cw-agnostic) */
static uint32_t chr_len(uint32_t s, int cw) {
    uint32_t n = 0;
    if (cw == 1) { const uint8_t *p = bytes_of(s); while (p[n]) n++; }
    else { const uint32_t *p = (const uint32_t *)(uintptr_t)s; while (p[n]) n++; }
    return n;
}
/* equality of one character at byte offsets a,b */
static int chr_eq(const uint8_t *a, const uint8_t *b, int cw) {
    return cw == 1 ? (*a == *b) : (*(const uint32_t *)a == *(const uint32_t *)b);
}
/* three-way order of one character (unsigned char narrow, wchar_t/int wide) */
static int chr_cmp(const uint8_t *a, const uint8_t *b, int cw) {
    if (cw == 1) {
        int x = *a, y = *b; return x < y ? -1 : (x > y ? 1 : 0);
    }
    int32_t x = *(const int32_t *)a, y = *(const int32_t *)b;   /* wchar_t is signed int */
    return x < y ? -1 : (x > y ? 1 : 0);
}

/* ---- the shared, never-freed empty rep (one per width), low-4GB ---- */
/*
 * EXPORTED DATA SHADOWS of libstdc++'s _S_empty_rep_storage (narrow + wide).
 *
 * GCC-4.x inlines the COW fast paths (default ctor, _M_dispose/_M_refcopy
 * empty-rep identity checks) into the TARGET's own code, referencing the
 * DATA symbol _S_empty_rep_storage directly through a non-lazy pointer slot.
 * Left un-shadowed, dyld binds that 8-byte slot to the NATIVE x86_64
 * libstdc++ in the shared cache (>4GB) and the translated i386 32-bit
 * `movl slot(%rip),%reg` reads a TRUNCATED low-32 address -> deref faults
 * (Civ IV Steam: first static initializer, slot 0x11e5e100 held
 * 0x00007ff955019680, code deref'd 0x55019680). Exporting the storage here
 * (C identifier ___ZNSs... assembles to ____ZNSs..., which static-interpose
 * redirects the target's __ZNSs... import to) keeps the bind below 4GB.
 *
 * Layout is GNU's: size_type[(sizeof(_Rep_base)+sizeof(_CharT)+
 * sizeof(size_type)-1)/sizeof(size_type)] = 16 zeroed bytes on i386 for both
 * widths — {length=0, capacity=0, refcount=0} header + a NUL char at +12.
 * It must stay all-zero and never be freed; the guarded GNU fast paths (and
 * our rep_dispose/rep_grab below) never mutate the empty rep.
 */
uint64_t ___ZNSs4_Rep20_S_empty_rep_storageE[2] = {0, 0};
uint64_t ___ZNSbIwSt11char_traitsIwESaIwEE4_Rep20_S_empty_rep_storageE[2] = {0, 0};

static pthread_mutex_t g_empty_mu = PTHREAD_MUTEX_INITIALIZER;
static uint32_t g_empty_narrow = 0, g_empty_wide = 0;

static uint32_t empty_data(int cw) {
    /* Use the EXPORTED storage: translated images that inline the COW fast
     * paths reference _S_empty_rep_storage directly through their (static-
     * interposed) data bind, so the shim functions must share the SAME rep
     * for the `p == empty` identity checks to hold across both worlds.
     * libabiconv is mapped below 4GB (wrapper layout invariant, same as the
     * init-stack trampoline); if it ever is not, fall back to a malloc'd rep
     * (the shim malloc is the low-4GB heap) — inlined readers see the bound
     * shadow either way. */
    uintptr_t s = (uintptr_t)(cw == 1
        ? (void *)___ZNSs4_Rep20_S_empty_rep_storageE
        : (void *)___ZNSbIwSt11char_traitsIwESaIwEE4_Rep20_S_empty_rep_storageE);
    if (!(s >> 32)) return (uint32_t)s + 12;
    uint32_t *slot = (cw == 1) ? &g_empty_narrow : &g_empty_wide;
    uint32_t v = __atomic_load_n(slot, __ATOMIC_ACQUIRE);
    if (v) return v;
    pthread_mutex_lock(&g_empty_mu);
    v = *slot;
    if (!v) {
        struct Rep *r = (struct Rep *)malloc(12 + cw);   /* header + one NUL char */
        r->length = 0; r->capacity = 0; r->refcount = 0;
        memset((uint8_t *)r + 12, 0, cw);
        v = data_ptr(r);
        __atomic_store_n(slot, v, __ATOMIC_RELEASE);
    }
    pthread_mutex_unlock(&g_empty_mu);
    return v;
}

/* ---- rep lifecycle ---- */
static uint32_t pick_cap(uint32_t newlen, uint32_t oldcap) {
    uint32_t grow = oldcap + (oldcap >> 1);   /* 1.5x amortised growth */
    return newlen > grow ? newlen : grow;
}
static struct Rep *rep_alloc(uint32_t capacity, int cw) {
    struct Rep *r = (struct Rep *)malloc(12 + (size_t)(capacity + 1) * cw);
    r->length = 0; r->capacity = capacity; r->refcount = 0;
    return r;
}
static void set_len_term(struct Rep *r, uint32_t len, int cw) {
    r->length = len;
    memset((uint8_t *)r + 12 + (size_t)len * cw, 0, cw);   /* NUL terminator */
}
/* dispose one reference; free when the last owner drops it (empty rep never). */
static void rep_dispose(uint32_t p, int cw) {
    if (p == empty_data(cw)) return;
    struct Rep *r = rep_of(p);
    if (__atomic_fetch_add(&r->refcount, -1, __ATOMIC_ACQ_REL) <= 0)
        free(r);
}
/* _M_grab: share (++refcount) unless the source is leaked (<0) — then clone. */
static uint32_t rep_grab(uint32_t p, int cw) {
    if (p == empty_data(cw)) return p;
    struct Rep *r = rep_of(p);
    if (__atomic_load_n(&r->refcount, __ATOMIC_ACQUIRE) < 0) {
        uint32_t len = r->length;
        struct Rep *nr = rep_alloc(len, cw);
        memcpy((uint8_t *)nr + 12, bytes_of(p), (size_t)len * cw);
        set_len_term(nr, len, cw);
        return data_ptr(nr);
    }
    __atomic_fetch_add(&r->refcount, 1, __ATOMIC_ACQ_REL);
    return p;
}

/* ---- object (this) accessors: obj[0] = &{u32 _M_p} ---- */
static uint32_t *obj_at(uint32_t *a, int i) { return (uint32_t *)(uintptr_t)a[i]; }

/*
 * Core mutation: replace [pos, pos+n1) of *obj with n2 chars from `src`.
 * `src` is copied into a private temp first, so it may safely alias *obj's own
 * buffer (self-append / self-replace) across the in-place move or realloc.
 * Makes the rep unique (COW) and grows capacity as needed.  Returns new _M_p.
 */
static uint32_t str_replace_raw(uint32_t *obj, uint32_t pos, uint32_t n1,
                                uint32_t src, uint32_t n2, int cw) {
    uint32_t p = *obj;
    struct Rep *r = rep_of(p);
    uint32_t len = r->length;
    uint32_t ep = empty_data(cw);
    if (pos > len) pos = len;
    if (n1 > len - pos) n1 = len - pos;
    uint32_t newlen = len - n1 + n2;

    void *tmp = NULL;
    if (n2 && src) { tmp = malloc((size_t)n2 * cw); memcpy(tmp, bytes_of(src), (size_t)n2 * cw); }

    int shared = (p != ep) && (__atomic_load_n(&r->refcount, __ATOMIC_ACQUIRE) > 0);
    if (p != ep && !shared && r->capacity >= newlen) {
        /* in place */
        uint8_t *d = bytes_of(p);
        memmove(d + (size_t)(pos + n2) * cw, d + (size_t)(pos + n1) * cw,
                (size_t)(len - pos - n1) * cw);
        if (tmp) memcpy(d + (size_t)pos * cw, tmp, (size_t)n2 * cw);
        set_len_term(r, newlen, cw);
        free(tmp);
        return p;
    }
    /* reallocate into a fresh unique rep */
    struct Rep *nr = rep_alloc(pick_cap(newlen, r->capacity), cw);
    uint8_t *nd = (uint8_t *)nr + 12;
    uint8_t *od = bytes_of(p);
    memcpy(nd, od, (size_t)pos * cw);                                   /* head  */
    if (tmp) memcpy(nd + (size_t)pos * cw, tmp, (size_t)n2 * cw);       /* middle*/
    memcpy(nd + (size_t)(pos + n2) * cw, od + (size_t)(pos + n1) * cw,  /* tail  */
           (size_t)(len - pos - n1) * cw);
    set_len_term(nr, newlen, cw);
    uint32_t np = data_ptr(nr);
    rep_dispose(p, cw);
    *obj = np;
    free(tmp);
    return np;
}

/* ensure a unique rep with capacity >= req (reserve) */
static void str_reserve(uint32_t *obj, uint32_t req, int cw) {
    uint32_t p = *obj, ep = empty_data(cw);
    struct Rep *r = rep_of(p);
    uint32_t len = r->length;
    if (req < len) req = len;
    int shared = (p != ep) && (__atomic_load_n(&r->refcount, __ATOMIC_ACQUIRE) > 0);
    if (p != ep && !shared && r->capacity >= req) return;
    struct Rep *nr = rep_alloc(req, cw);
    memcpy((uint8_t *)nr + 12, bytes_of(p), (size_t)len * cw);
    set_len_term(nr, len, cw);
    rep_dispose(p, cw);
    *obj = data_ptr(nr);
}

/* _M_leak: unshareable mutation prep for non-const begin/end/operator[]. */
static uint32_t str_leak(uint32_t *obj, int cw) {
    uint32_t p = *obj, ep = empty_data(cw);
    if (p == ep) return p;                       /* empty rep: begin==end, no leak */
    struct Rep *r = rep_of(p);
    if (__atomic_load_n(&r->refcount, __ATOMIC_ACQUIRE) > 0) {
        uint32_t len = r->length;
        struct Rep *nr = rep_alloc(len, cw);
        memcpy((uint8_t *)nr + 12, bytes_of(p), (size_t)len * cw);
        set_len_term(nr, len, cw);
        rep_dispose(p, cw);
        p = data_ptr(nr);
        *obj = p;
        r = nr;
    }
    r->refcount = -1;                            /* mark leaked */
    return p;
}

/* build a fresh string object at *obj from a raw buffer */
static void str_init_buf(uint32_t *obj, uint32_t src, uint32_t n, int cw) {
    if (n == 0) { *obj = empty_data(cw); return; }
    struct Rep *r = rep_alloc(n, cw);
    memcpy((uint8_t *)r + 12, bytes_of(src), (size_t)n * cw);
    set_len_term(r, n, cw);
    *obj = data_ptr(r);
}
static void str_init_fill(uint32_t *obj, uint32_t n, uint32_t c, int cw) {
    if (n == 0) { *obj = empty_data(cw); return; }
    struct Rep *r = rep_alloc(n, cw);
    uint8_t *d = (uint8_t *)r + 12;
    if (cw == 1) memset(d, (int)(c & 0xff), n);
    else { uint32_t *w = (uint32_t *)d; for (uint32_t i = 0; i < n; i++) w[i] = c; }
    set_len_term(r, n, cw);
    *obj = data_ptr(r);
}

/* ================= generic (width-keyed) member implementations ============ */

/* --- construction --- */
static uint32_t g_ctor_default(uint32_t *a, int cw) { *obj_at(a, 0) = empty_data(cw); return a[0]; }
static uint32_t g_ctor_copy(uint32_t *a, int cw)    { *obj_at(a, 0) = rep_grab(*obj_at(a, 1), cw); return a[0]; }
static uint32_t g_ctor_cstr(uint32_t *a, int cw)    { str_init_buf(obj_at(a, 0), a[1], chr_len(a[1], cw), cw); return a[0]; }
static uint32_t g_ctor_buf(uint32_t *a, int cw)     { str_init_buf(obj_at(a, 0), a[1], a[2], cw); return a[0]; }
static uint32_t g_ctor_fill(uint32_t *a, int cw)    { str_init_fill(obj_at(a, 0), a[1], a[2], cw); return a[0]; }
/* ctor(this, const string& other, pos, n) */
static uint32_t g_ctor_substr(uint32_t *a, int cw) {
    uint32_t sp = *obj_at(a, 1); struct Rep *r = rep_of(sp);
    uint32_t pos = a[2], n = a[3], len = r->length;
    if (pos > len) pos = len;
    if (n > len - pos) n = len - pos;
    str_init_buf(obj_at(a, 0), sp + pos * cw, n, cw);
    return a[0];
}

/* --- destruction --- */
static uint32_t g_dtor(uint32_t *a, int cw) { rep_dispose(*obj_at(a, 0), cw); return 0; }

/* --- assignment (operator= / assign) --- */
static uint32_t g_assign_str(uint32_t *a, int cw) {   /* grab source then drop old (self-assign safe) */
    uint32_t *obj = obj_at(a, 0);
    uint32_t newp = rep_grab(*obj_at(a, 1), cw);
    uint32_t oldp = *obj;
    *obj = newp;
    rep_dispose(oldp, cw);
    return a[0];
}
static uint32_t g_assign_cstr(uint32_t *a, int cw) {
    uint32_t *obj = obj_at(a, 0);
    str_replace_raw(obj, 0, rep_of(*obj)->length, a[1], chr_len(a[1], cw), cw);
    return a[0];
}
static uint32_t g_assign_buf(uint32_t *a, int cw) {
    uint32_t *obj = obj_at(a, 0);
    str_replace_raw(obj, 0, rep_of(*obj)->length, a[1], a[2], cw);
    return a[0];
}
/* assign(this, const string& other, pos, n) — wstring only */
static uint32_t g_assign_substr(uint32_t *a, int cw) {
    uint32_t *obj = obj_at(a, 0);
    uint32_t sp = *obj_at(a, 1); struct Rep *sr = rep_of(sp);
    uint32_t pos = a[2], n = a[3], slen = sr->length;
    if (pos > slen) pos = slen;
    if (n > slen - pos) n = slen - pos;
    str_replace_raw(obj, 0, rep_of(*obj)->length, sp + pos * cw, n, cw);
    return a[0];
}

/* --- element / iterator access --- */
static uint32_t g_index(uint32_t *a, int cw) { uint32_t p = str_leak(obj_at(a, 0), cw); return p + a[1] * cw; }
static uint32_t g_begin(uint32_t *a, int cw) { return str_leak(obj_at(a, 0), cw); }
static uint32_t g_end(uint32_t *a, int cw)   { uint32_t p = str_leak(obj_at(a, 0), cw); return p + rep_of(p)->length * cw; }
static uint32_t g_cbegin(uint32_t *a, int cw){ (void)cw; return *obj_at(a, 0); }
static uint32_t g_cend(uint32_t *a, int cw)  { uint32_t p = *obj_at(a, 0); return p + rep_of(p)->length * cw; }
static uint32_t g_cstr(uint32_t *a, int cw)  { (void)cw; return *obj_at(a, 0); }
static uint32_t g_size(uint32_t *a, int cw)  { (void)cw; return rep_of(*obj_at(a, 0))->length; }

/* --- clear --- */
static uint32_t g_clear(uint32_t *a, int cw) {
    uint32_t *obj = obj_at(a, 0);
    uint32_t oldp = *obj;
    *obj = empty_data(cw);
    rep_dispose(oldp, cw);
    return 0;
}

/* --- erase --- */
static uint32_t g_erase_range(uint32_t *a, int cw) {     /* erase(pos, n) -> string& */
    str_replace_raw(obj_at(a, 0), a[1], a[2], 0, 0, cw);
    return a[0];
}
static uint32_t g_erase_iter(uint32_t *a, int cw) {      /* erase(iterator) -> iterator */
    uint32_t *obj = obj_at(a, 0);
    uint32_t pos = (a[1] - *obj) / cw;
    uint32_t np = str_replace_raw(obj, pos, 1, 0, 0, cw);
    return np + pos * cw;
}

/* --- append --- */
static uint32_t g_append_cstr(uint32_t *a, int cw) {
    uint32_t *obj = obj_at(a, 0);
    str_replace_raw(obj, rep_of(*obj)->length, 0, a[1], chr_len(a[1], cw), cw);
    return a[0];
}
static uint32_t g_append_str(uint32_t *a, int cw) {
    uint32_t *obj = obj_at(a, 0);
    uint32_t sp = *obj_at(a, 1);
    str_replace_raw(obj, rep_of(*obj)->length, 0, sp, rep_of(sp)->length, cw);
    return a[0];
}

/* --- resize / reserve / push_back --- */
static uint32_t g_resize(uint32_t *a, int cw) {          /* resize(n, c) */
    uint32_t *obj = obj_at(a, 0);
    uint32_t n = a[1], c = a[2], len = rep_of(*obj)->length;
    if (n < len) { str_replace_raw(obj, n, len - n, 0, 0, cw); }
    else if (n > len) {
        uint32_t add = n - len;
        struct Rep *fr = rep_alloc(add, cw);
        uint8_t *fd = (uint8_t *)fr + 12;
        if (cw == 1) memset(fd, (int)(c & 0xff), add);
        else { uint32_t *w = (uint32_t *)fd; for (uint32_t i = 0; i < add; i++) w[i] = c; }
        str_replace_raw(obj, len, 0, data_ptr(fr), add, cw);
        free(fr);
    }
    return a[0];
}
static uint32_t g_reserve(uint32_t *a, int cw)   { str_reserve(obj_at(a, 0), a[1], cw); return a[0]; }
static uint32_t g_push_back(uint32_t *a, int cw) {
    uint32_t *obj = obj_at(a, 0);
    uint32_t cbuf = a[1];
    str_replace_raw(obj, rep_of(*obj)->length, 0, (uint32_t)(uintptr_t)&cbuf, 1, cw);
    return 0;
}

/* --- replace --- */
static uint32_t g_replace_pos_cstr(uint32_t *a, int cw) {   /* replace(pos,n1,const char*) */
    str_replace_raw(obj_at(a, 0), a[1], a[2], a[3], chr_len(a[3], cw), cw);
    return a[0];
}
static uint32_t g_replace_pos_str(uint32_t *a, int cw) {    /* replace(pos,n1,const string&) */
    uint32_t sp = *obj_at(a, 3);
    str_replace_raw(obj_at(a, 0), a[1], a[2], sp, rep_of(sp)->length, cw);
    return a[0];
}
static uint32_t g_replace_iter(uint32_t *a, int cw) {       /* replace(i1,i2,k1,k2) */
    uint32_t *obj = obj_at(a, 0);
    uint32_t data = *obj;
    uint32_t pos = (a[1] - data) / cw;
    uint32_t n1  = (a[2] - a[1]) / cw;
    uint32_t n2  = (a[4] - a[3]) / cw;
    str_replace_raw(obj, pos, n1, a[3], n2, cw);
    return a[0];
}

/* --- search (const, no leak) --- */
static uint32_t find_buf(uint32_t data, uint32_t len, uint32_t s, uint32_t n, uint32_t pos, int cw) {
    if (n == 0) return pos <= len ? pos : NPOS;
    if (n > len) return NPOS;
    for (uint32_t i = pos; i + n <= len; i++) {
        uint32_t k = 0;
        while (k < n && chr_eq(bytes_of(data) + (size_t)(i + k) * cw, bytes_of(s) + (size_t)k * cw, cw)) k++;
        if (k == n) return i;
    }
    return NPOS;
}
static uint32_t rfind_buf(uint32_t data, uint32_t len, uint32_t s, uint32_t n, uint32_t pos, int cw) {
    if (n > len) return NPOS;
    uint32_t last = len - n;
    if (pos < last) last = pos;
    for (uint32_t i = last + 1; i-- > 0; ) {
        uint32_t k = 0;
        while (k < n && chr_eq(bytes_of(data) + (size_t)(i + k) * cw, bytes_of(s) + (size_t)k * cw, cw)) k++;
        if (k == n) return i;
        if (i == 0) break;
    }
    return NPOS;
}
static int in_set(const uint8_t *ch, uint32_t set, uint32_t nset, int cw) {
    for (uint32_t j = 0; j < nset; j++)
        if (chr_eq(ch, bytes_of(set) + (size_t)j * cw, cw)) return 1;
    return 0;
}
static uint32_t ffo_buf(uint32_t data, uint32_t len, uint32_t set, uint32_t nset, uint32_t pos, int cw) {
    for (uint32_t i = pos; i < len; i++)
        if (in_set(bytes_of(data) + (size_t)i * cw, set, nset, cw)) return i;
    return NPOS;
}
static uint32_t ffno_buf(uint32_t data, uint32_t len, uint32_t set, uint32_t nset, uint32_t pos, int cw) {
    for (uint32_t i = pos; i < len; i++)
        if (!in_set(bytes_of(data) + (size_t)i * cw, set, nset, cw)) return i;
    return NPOS;
}

static uint32_t g_find_cstr(uint32_t *a, int cw) { uint32_t p = *obj_at(a, 0); return find_buf(p, rep_of(p)->length, a[1], chr_len(a[1], cw), a[2], cw); }
static uint32_t g_find_str(uint32_t *a, int cw)  { uint32_t p = *obj_at(a, 0), s = *obj_at(a, 1); return find_buf(p, rep_of(p)->length, s, rep_of(s)->length, a[2], cw); }
static uint32_t g_find_ch(uint32_t *a, int cw)   { uint32_t p = *obj_at(a, 0); uint32_t c = a[1]; return find_buf(p, rep_of(p)->length, (uint32_t)(uintptr_t)&c, 1, a[2], cw); }
static uint32_t g_rfind_cstr(uint32_t *a, int cw){ uint32_t p = *obj_at(a, 0); return rfind_buf(p, rep_of(p)->length, a[1], chr_len(a[1], cw), a[2], cw); }
static uint32_t g_rfind_ch(uint32_t *a, int cw)  { uint32_t p = *obj_at(a, 0); uint32_t c = a[1]; return rfind_buf(p, rep_of(p)->length, (uint32_t)(uintptr_t)&c, 1, a[2], cw); }
static uint32_t g_ffo_cstr(uint32_t *a, int cw)  { uint32_t p = *obj_at(a, 0); return ffo_buf(p, rep_of(p)->length, a[1], chr_len(a[1], cw), a[2], cw); }
static uint32_t g_ffo_str(uint32_t *a, int cw)   { uint32_t p = *obj_at(a, 0), s = *obj_at(a, 1); return ffo_buf(p, rep_of(p)->length, s, rep_of(s)->length, a[2], cw); }
static uint32_t g_ffno_cstr(uint32_t *a, int cw) { uint32_t p = *obj_at(a, 0); return ffno_buf(p, rep_of(p)->length, a[1], chr_len(a[1], cw), a[2], cw); }
static uint32_t g_ffno_str(uint32_t *a, int cw)  { uint32_t p = *obj_at(a, 0), s = *obj_at(a, 1); return ffno_buf(p, rep_of(p)->length, s, rep_of(s)->length, a[2], cw); }

/* 3-arg buffer search variants: (const char* s, pos, n) — note the set/needle
 * LENGTH is the LAST i386 slot (a[3]), pos the middle one (a[2]). */
static uint32_t g_find_buf3(uint32_t *a, int cw) { uint32_t p = *obj_at(a, 0); return find_buf(p, rep_of(p)->length, a[1], a[3], a[2], cw); }
static uint32_t g_ffo_buf3(uint32_t *a, int cw)  { uint32_t p = *obj_at(a, 0); return ffo_buf(p, rep_of(p)->length, a[1], a[3], a[2], cw); }
static uint32_t g_ffno_buf3(uint32_t *a, int cw) { uint32_t p = *obj_at(a, 0); return ffno_buf(p, rep_of(p)->length, a[1], a[3], a[2], cw); }

/* --- out-of-line COW internals (called from the target's inlined header
 *     fast paths; `this` conventions per GNU libstdc++-v3 basic_string) --- */
/* _Rep::_M_dispose(const alloc&): `this` IS the rep HEADER (not the string
 * object) — its char data starts at this+12. */
static uint32_t g_rep_dispose_member(uint32_t *a, int cw) { rep_dispose(a[0] + 12, cw); return 0; }
/* _M_leak_hard(): make the rep unique, then mark it leaked — str_leak. */
static uint32_t g_leak_hard(uint32_t *a, int cw) { str_leak(obj_at(a, 0), cw); return 0; }
/* _M_mutate(pos, n1, n2): make unique + open an n2-char UNINITIALIZED gap at
 * pos (the caller fills it). src=0 keeps the gap uninitialized. */
static uint32_t g_mutate(uint32_t *a, int cw) { str_replace_raw(obj_at(a, 0), a[1], a[2], 0, a[3], cw); return 0; }
/* _M_replace_aux(pos, n1, n2, c): replace [pos,pos+n1) with n2 copies of c. */
static uint32_t g_replace_aux(uint32_t *a, int cw) {
    uint32_t n2 = a[3], c = a[4];
    void *fill = NULL;
    if (n2) {
        fill = malloc((size_t)n2 * cw);
        if (cw == 1) memset(fill, (int)(c & 0xff), n2);
        else { uint32_t *w = (uint32_t *)fill; for (uint32_t i = 0; i < n2; i++) w[i] = c; }
    }
    str_replace_raw(obj_at(a, 0), a[1], a[2], (uint32_t)(uintptr_t)fill, n2, cw);
    free(fill);
    return a[0];
}
/* swap(string&): stateless allocators — just exchange the two _M_p words. */
static uint32_t g_swap(uint32_t *a, int cw) {
    (void)cw;
    uint32_t *o1 = obj_at(a, 0), *o2 = obj_at(a, 1);
    uint32_t t = *o1; *o1 = *o2; *o2 = t;
    return 0;
}
/* append(const char* s, n) */
static uint32_t g_append_buf(uint32_t *a, int cw) {
    uint32_t *obj = obj_at(a, 0);
    str_replace_raw(obj, rep_of(*obj)->length, 0, a[1], a[2], cw);
    return a[0];
}
/* replace(pos, n1, const char* s, n2) */
static uint32_t g_replace_pos_buf(uint32_t *a, int cw) {
    str_replace_raw(obj_at(a, 0), a[1], a[2], a[3], a[4], cw);
    return a[0];
}
/* CONST operator[](n): no leak, direct read pointer. */
static uint32_t g_cindex(uint32_t *a, int cw) { return *obj_at(a, 0) + a[1] * cw; }

/* --- compare --- */
static int cmp_buf(uint32_t d1, uint32_t l1, uint32_t d2, uint32_t l2, int cw) {
    uint32_t n = l1 < l2 ? l1 : l2;
    for (uint32_t i = 0; i < n; i++) {
        int r = chr_cmp(bytes_of(d1) + (size_t)i * cw, bytes_of(d2) + (size_t)i * cw, cw);
        if (r) return r;
    }
    return l1 < l2 ? -1 : (l1 > l2 ? 1 : 0);
}
static uint32_t g_compare_str(uint32_t *a, int cw)  { uint32_t p = *obj_at(a, 0), s = *obj_at(a, 1); return (uint32_t)cmp_buf(p, rep_of(p)->length, s, rep_of(s)->length, cw); }
static uint32_t g_compare_cstr(uint32_t *a, int cw) { uint32_t p = *obj_at(a, 0); return (uint32_t)cmp_buf(p, rep_of(p)->length, a[1], chr_len(a[1], cw), cw); }
static uint32_t g_compare_sub_cstr(uint32_t *a, int cw) {   /* compare(pos,n1,const char*,n2) */
    uint32_t p = *obj_at(a, 0); uint32_t len = rep_of(p)->length;
    uint32_t pos = a[1], n1 = a[2], s = a[3], n2 = a[4];
    if (pos > len) pos = len;
    if (n1 > len - pos) n1 = len - pos;
    return (uint32_t)cmp_buf(p + pos * cw, n1, s, n2, cw);
}

/* --- substr (BY VALUE: a[0]=hidden sret ptr, a[1]=this, a[2]=pos, a[3]=n) --- */
static uint32_t g_substr(uint32_t *a, int cw) {
    uint32_t *dst = (uint32_t *)(uintptr_t)a[0];
    uint32_t sp = *obj_at(a, 1); struct Rep *r = rep_of(sp);
    uint32_t pos = a[2], n = a[3], len = r->length;
    if (pos > len) pos = len;
    if (n > len - pos) n = len - pos;
    str_init_buf(dst, sp + pos * cw, n, cw);
    return a[0];                          /* return the sret buffer in eax */
}

/* --- operator+ (free fn, BY VALUE: a[0]=sret) --- */
static uint32_t opplus_build(uint32_t dst, uint32_t d1, uint32_t l1, uint32_t d2, uint32_t l2, int cw) {
    uint32_t total = l1 + l2;
    if (total == 0) { *(uint32_t *)(uintptr_t)dst = empty_data(cw); return dst; }
    struct Rep *r = rep_alloc(total, cw);
    uint8_t *nd = (uint8_t *)r + 12;
    memcpy(nd, bytes_of(d1), (size_t)l1 * cw);
    memcpy(nd + (size_t)l1 * cw, bytes_of(d2), (size_t)l2 * cw);
    set_len_term(r, total, cw);
    *(uint32_t *)(uintptr_t)dst = data_ptr(r);
    return dst;
}

/* --- reverse set search (find_last_of / find_last_not_of) --- */
static uint32_t flo_buf(uint32_t data, uint32_t len, uint32_t set, uint32_t nset, uint32_t pos, int cw, int want) {
    if (len == 0) return NPOS;
    uint32_t i = pos < len - 1 ? pos : len - 1;
    for (;; i--) {
        if (in_set(bytes_of(data) + (size_t)i * cw, set, nset, cw) == want) return i;
        if (i == 0) return NPOS;
    }
}
static uint32_t g_flo_buf3(uint32_t *a, int cw)  { uint32_t p = *obj_at(a, 0); return flo_buf(p, rep_of(p)->length, a[1], a[3], a[2], cw, 1); }
static uint32_t g_flno_buf3(uint32_t *a, int cw) { uint32_t p = *obj_at(a, 0); return flo_buf(p, rep_of(p)->length, a[1], a[3], a[2], cw, 0); }
static uint32_t g_ffno_ch(uint32_t *a, int cw)   { uint32_t p = *obj_at(a, 0); uint32_t c = a[1]; return ffno_buf(p, rep_of(p)->length, (uint32_t)(uintptr_t)&c, 1, a[2], cw); }
static uint32_t g_rfind_buf3(uint32_t *a, int cw){ uint32_t p = *obj_at(a, 0); return rfind_buf(p, rep_of(p)->length, a[1], a[3], a[2], cw); }

/* compare(pos, n1, const string&) */
static uint32_t g_compare_sub_str(uint32_t *a, int cw) {
    uint32_t p = *obj_at(a, 0), s = *obj_at(a, 3); uint32_t len = rep_of(p)->length;
    uint32_t pos = a[1], n1 = a[2];
    if (pos > len) pos = len;
    if (n1 > len - pos) n1 = len - pos;
    return (uint32_t)cmp_buf(p + pos * cw, n1, s, rep_of(s)->length, cw);
}

/* at(n): libstdc++ throws std::out_of_range, which cannot be raised into i386
 * code from here. ponytail: abort loudly instead; bridge the throw if a target
 * is ever seen relying on catching it. */
static uint32_t at_check(uint32_t p, uint32_t n, int cw) {
    if (n >= rep_of(p)->length) {
        fprintf(stderr, "[cow_string] basic_string::at(%u) out of range (size %u)\n",
                n, rep_of(p)->length);
        fflush(stderr);
        abort();
    }
    return p + n * cw;
}
static uint32_t g_cat(uint32_t *a, int cw) { return at_check(*obj_at(a, 0), a[1], cw); }
static uint32_t g_at(uint32_t *a, int cw)  { return at_check(str_leak(obj_at(a, 0), cw), a[1], cw); }

/* copy(char* s, n, pos) const -> chars copied */
static uint32_t g_copy(uint32_t *a, int cw) {
    uint32_t p = *obj_at(a, 0), len = rep_of(p)->length, n = a[2], pos = a[3];
    if (pos > len) pos = len;
    if (n > len - pos) n = len - pos;
    memcpy(bytes_of(a[1]), bytes_of(p + pos * cw), (size_t)n * cw);
    return n;
}
static uint32_t g_empty(uint32_t *a, int cw) { (void)cw; return rep_of(*obj_at(a, 0))->length == 0; }

/* append(n, c) / resize(n) / replace(i1, i2, n, c) / insert variants */
static uint32_t g_append_fill(uint32_t *a, int cw) {
    uint32_t *obj = obj_at(a, 0);
    uint32_t b[5] = { a[0], rep_of(*obj)->length, 0, a[1], a[2] };
    g_replace_aux(b, cw);
    return a[0];
}
static uint32_t g_resize_nul(uint32_t *a, int cw) { uint32_t b[3] = { a[0], a[1], 0 }; return g_resize(b, cw); }
static uint32_t g_replace_iter_fill(uint32_t *a, int cw) {
    uint32_t data = *obj_at(a, 0);
    uint32_t b[5] = { a[0], (a[1] - data) / cw, (a[2] - a[1]) / cw, a[3], a[4] };
    return g_replace_aux(b, cw);
}
static uint32_t g_append_substr(uint32_t *a, int cw) {   /* append(const string&, pos, n) */
    uint32_t *obj = obj_at(a, 0), s = *obj_at(a, 1), slen = rep_of(s)->length;
    uint32_t pos = a[2], n = a[3];
    if (pos > slen) pos = slen;
    if (n > slen - pos) n = slen - pos;
    str_replace_raw(obj, rep_of(*obj)->length, 0, s + pos * cw, n, cw);
    return a[0];
}
static uint32_t g_insert_buf(uint32_t *a, int cw) {      /* insert(pos, const char*, n) */
    str_replace_raw(obj_at(a, 0), a[1], 0, a[2], a[3], cw);
    return a[0];
}
static uint32_t g_insert_substr(uint32_t *a, int cw) {   /* insert(pos1, const string&, pos2, n) */
    uint32_t s = *obj_at(a, 2), slen = rep_of(s)->length, pos2 = a[3], n = a[4];
    if (pos2 > slen) pos2 = slen;
    if (n > slen - pos2) n = slen - pos2;
    str_replace_raw(obj_at(a, 0), a[1], 0, s + pos2 * cw, n, cw);
    return a[0];
}
static uint32_t g_assign_ch(uint32_t *a, int cw) {        /* operator=(char) */
    uint32_t *obj = obj_at(a, 0), c = a[1];
    str_replace_raw(obj, 0, rep_of(*obj)->length, (uint32_t)(uintptr_t)&c, 1, cw);
    return a[0];
}
static uint32_t g_pluseq_ch(uint32_t *a, int cw) { g_push_back(a, cw); return a[0]; }

/* --- static COW internals called from inlined header code --- */
/* _Rep::_S_create(capacity, old_capacity, alloc) -> _Rep* (header). Growth
 * rule per libstdc++-v3: a request just past the old capacity doubles it. */
static uint32_t g_S_create(uint32_t *a, int cw) {
    uint32_t cap = a[0], old = a[1];
    if (cap > old && cap < 2 * old) cap = 2 * old;
    return (uint32_t)(uintptr_t)rep_alloc(cap, cw);
}
/* _Rep::_M_destroy(alloc): `this` IS the header; free it (never the empty rep). */
static uint32_t g_M_destroy(uint32_t *a, int cw) {
    if (a[0] + 12 != empty_data(cw)) free((void *)(uintptr_t)a[0]);
    return 0;
}
/* _S_construct(n, c, alloc) -> _M_p of a fresh rep holding n copies of c. */
static uint32_t g_S_construct_fill(uint32_t *a, int cw) {
    uint32_t p;
    str_init_fill(&p, a[0], a[1], cw);
    return p;
}

/* ======================= exported width-specific shims ===================== */
/* NARROW std::string (Ss) */
uint32_t shim_Ss_ctor_default(uint32_t *a){ return g_ctor_default(a, 1); }
uint32_t shim_Ss_ctor_copy(uint32_t *a)   { return g_ctor_copy(a, 1); }
uint32_t shim_Ss_ctor_cstr(uint32_t *a)   { return g_ctor_cstr(a, 1); }
uint32_t shim_Ss_ctor_buf(uint32_t *a)    { return g_ctor_buf(a, 1); }
uint32_t shim_Ss_ctor_fill(uint32_t *a)   { return g_ctor_fill(a, 1); }
uint32_t shim_Ss_dtor(uint32_t *a)        { return g_dtor(a, 1); }
uint32_t shim_Ss_assign_op(uint32_t *a)   { return g_assign_str(a, 1); }
uint32_t shim_Ss_assign_str(uint32_t *a)  { return g_assign_str(a, 1); }
uint32_t shim_Ss_assign_cstr(uint32_t *a) { return g_assign_cstr(a, 1); }
uint32_t shim_Ss_assign_buf(uint32_t *a)  { return g_assign_buf(a, 1); }
uint32_t shim_Ss_index(uint32_t *a)       { return g_index(a, 1); }
uint32_t shim_Ss_begin(uint32_t *a)       { return g_begin(a, 1); }
uint32_t shim_Ss_end(uint32_t *a)         { return g_end(a, 1); }
uint32_t shim_Ss_cbegin(uint32_t *a)      { return g_cbegin(a, 1); }
uint32_t shim_Ss_cend(uint32_t *a)        { return g_cend(a, 1); }
uint32_t shim_Ss_cstr(uint32_t *a)        { return g_cstr(a, 1); }
uint32_t shim_Ss_size(uint32_t *a)        { return g_size(a, 1); }
uint32_t shim_Ss_clear(uint32_t *a)       { return g_clear(a, 1); }
uint32_t shim_Ss_erase_range(uint32_t *a) { return g_erase_range(a, 1); }
uint32_t shim_Ss_erase_iter(uint32_t *a)  { return g_erase_iter(a, 1); }
uint32_t shim_Ss_append_cstr(uint32_t *a) { return g_append_cstr(a, 1); }
uint32_t shim_Ss_append_str(uint32_t *a)  { return g_append_str(a, 1); }
uint32_t shim_Ss_resize(uint32_t *a)      { return g_resize(a, 1); }
uint32_t shim_Ss_reserve(uint32_t *a)     { return g_reserve(a, 1); }
uint32_t shim_Ss_push_back(uint32_t *a)   { return g_push_back(a, 1); }
uint32_t shim_Ss_replace_pos_cstr(uint32_t *a){ return g_replace_pos_cstr(a, 1); }
uint32_t shim_Ss_replace_pos_str(uint32_t *a) { return g_replace_pos_str(a, 1); }
uint32_t shim_Ss_replace_iter(uint32_t *a)    { return g_replace_iter(a, 1); }
uint32_t shim_Ss_find_cstr(uint32_t *a)   { return g_find_cstr(a, 1); }
uint32_t shim_Ss_find_str(uint32_t *a)    { return g_find_str(a, 1); }
uint32_t shim_Ss_find_ch(uint32_t *a)     { return g_find_ch(a, 1); }
uint32_t shim_Ss_rfind_cstr(uint32_t *a)  { return g_rfind_cstr(a, 1); }
uint32_t shim_Ss_rfind_ch(uint32_t *a)    { return g_rfind_ch(a, 1); }
uint32_t shim_Ss_ffo_cstr(uint32_t *a)    { return g_ffo_cstr(a, 1); }
uint32_t shim_Ss_ffo_str(uint32_t *a)     { return g_ffo_str(a, 1); }
uint32_t shim_Ss_ffno_cstr(uint32_t *a)   { return g_ffno_cstr(a, 1); }
uint32_t shim_Ss_ffno_str(uint32_t *a)    { return g_ffno_str(a, 1); }
uint32_t shim_Ss_compare_cstr(uint32_t *a){ return g_compare_cstr(a, 1); }
uint32_t shim_Ss_compare_str(uint32_t *a) { return g_compare_str(a, 1); }
uint32_t shim_Ss_compare_sub_cstr(uint32_t *a){ return g_compare_sub_cstr(a, 1); }
uint32_t shim_Ss_substr(uint32_t *a)      { return g_substr(a, 1); }
uint32_t shim_Ss_rep_dispose(uint32_t *a) { return g_rep_dispose_member(a, 1); }
uint32_t shim_Ss_leak_hard(uint32_t *a)   { return g_leak_hard(a, 1); }
uint32_t shim_Ss_mutate(uint32_t *a)      { return g_mutate(a, 1); }
uint32_t shim_Ss_replace_aux(uint32_t *a) { return g_replace_aux(a, 1); }
uint32_t shim_Ss_swap(uint32_t *a)        { return g_swap(a, 1); }
uint32_t shim_Ss_append_buf(uint32_t *a)  { return g_append_buf(a, 1); }
uint32_t shim_Ss_replace_pos_buf(uint32_t *a){ return g_replace_pos_buf(a, 1); }
uint32_t shim_Ss_find_buf3(uint32_t *a)   { return g_find_buf3(a, 1); }
uint32_t shim_Ss_ffo_buf3(uint32_t *a)    { return g_ffo_buf3(a, 1); }
uint32_t shim_Ss_ffno_buf3(uint32_t *a)   { return g_ffno_buf3(a, 1); }
uint32_t shim_Ss_cindex(uint32_t *a)      { return g_cindex(a, 1); }

/* WIDE std::wstring (Sb<wchar_t>) */
uint32_t shim_Sw_ctor_default(uint32_t *a){ return g_ctor_default(a, 4); }
uint32_t shim_Sw_ctor_copy(uint32_t *a)   { return g_ctor_copy(a, 4); }
uint32_t shim_Sw_ctor_cstr(uint32_t *a)   { return g_ctor_cstr(a, 4); }
uint32_t shim_Sw_ctor_fill(uint32_t *a)   { return g_ctor_fill(a, 4); }
uint32_t shim_Sw_ctor_substr(uint32_t *a) { return g_ctor_substr(a, 4); }
uint32_t shim_Sw_dtor(uint32_t *a)        { return g_dtor(a, 4); }
uint32_t shim_Sw_index(uint32_t *a)       { return g_index(a, 4); }
uint32_t shim_Sw_cstr(uint32_t *a)        { return g_cstr(a, 4); }
uint32_t shim_Sw_clear(uint32_t *a)       { return g_clear(a, 4); }
uint32_t shim_Sw_append_cstr(uint32_t *a) { return g_append_cstr(a, 4); }
uint32_t shim_Sw_append_str(uint32_t *a)  { return g_append_str(a, 4); }
uint32_t shim_Sw_assign_cstr(uint32_t *a) { return g_assign_cstr(a, 4); }
uint32_t shim_Sw_assign_buf(uint32_t *a)  { return g_assign_buf(a, 4); }
uint32_t shim_Sw_assign_str(uint32_t *a)  { return g_assign_str(a, 4); }
uint32_t shim_Sw_assign_substr(uint32_t *a){ return g_assign_substr(a, 4); }
uint32_t shim_Sw_replace_pos_cstr(uint32_t *a){ return g_replace_pos_cstr(a, 4); }
uint32_t shim_Sw_find_cstr(uint32_t *a)   { return g_find_cstr(a, 4); }
uint32_t shim_Sw_find_str(uint32_t *a)    { return g_find_str(a, 4); }
uint32_t shim_Sw_find_ch(uint32_t *a)     { return g_find_ch(a, 4); }
uint32_t shim_Sw_compare_cstr(uint32_t *a){ return g_compare_cstr(a, 4); }
uint32_t shim_Sw_compare_str(uint32_t *a) { return g_compare_str(a, 4); }
uint32_t shim_Sw_substr(uint32_t *a)      { return g_substr(a, 4); }
uint32_t shim_Sw_rep_dispose(uint32_t *a) { return g_rep_dispose_member(a, 4); }
uint32_t shim_Sw_leak_hard(uint32_t *a)   { return g_leak_hard(a, 4); }
uint32_t shim_Sw_mutate(uint32_t *a)      { return g_mutate(a, 4); }
uint32_t shim_Sw_find_buf3(uint32_t *a)   { return g_find_buf3(a, 4); }

/* narrow: remaining out-of-line members (Portal 2 libcef / libsteam) */
uint32_t shim_Ss_flo_ch(uint32_t *a)      { return g_rfind_ch(a, 1); }
uint32_t shim_Ss_flo_cstr(uint32_t *a)    { uint32_t b[4] = { a[0], a[1], a[2], chr_len(a[1], 1) }; return g_flo_buf3(b, 1); }
uint32_t shim_Ss_flo_buf3(uint32_t *a)    { return g_flo_buf3(a, 1); }
uint32_t shim_Ss_ffo_ch(uint32_t *a)      { return g_find_ch(a, 1); }
uint32_t shim_Ss_flno_buf3(uint32_t *a)   { return g_flno_buf3(a, 1); }
uint32_t shim_Ss_ffno_ch(uint32_t *a)     { return g_ffno_ch(a, 1); }
uint32_t shim_Ss_cat(uint32_t *a)         { return g_cat(a, 1); }
uint32_t shim_Ss_at(uint32_t *a)          { return g_at(a, 1); }
uint32_t shim_Ss_copy(uint32_t *a)        { return g_copy(a, 1); }
uint32_t shim_Ss_empty(uint32_t *a)       { return g_empty(a, 1); }
uint32_t shim_Ss_rfind_buf3(uint32_t *a)  { return g_rfind_buf3(a, 1); }
uint32_t shim_Ss_compare_sub_cstr3(uint32_t *a) { uint32_t b[5] = { a[0], a[1], a[2], a[3], chr_len(a[3], 1) }; return g_compare_sub_cstr(b, 1); }
uint32_t shim_Ss_compare_sub_str(uint32_t *a)   { return g_compare_sub_str(a, 1); }
uint32_t shim_Ss_S_construct(uint32_t *a) { return g_S_construct_fill(a, 1); }
uint32_t shim_Ss_M_destroy(uint32_t *a)   { return g_M_destroy(a, 1); }
uint32_t shim_Ss_S_create(uint32_t *a)    { return g_S_create(a, 1); }
uint32_t shim_Ss_append_fill(uint32_t *a) { return g_append_fill(a, 1); }
uint32_t shim_Ss_append_substr(uint32_t *a){ return g_append_substr(a, 1); }
uint32_t shim_Ss_insert_buf(uint32_t *a)  { return g_insert_buf(a, 1); }
uint32_t shim_Ss_insert_substr(uint32_t *a){ return g_insert_substr(a, 1); }
uint32_t shim_Ss_resize_nul(uint32_t *a)  { return g_resize_nul(a, 1); }
uint32_t shim_Ss_replace_iter_fill(uint32_t *a){ return g_replace_iter_fill(a, 1); }
uint32_t shim_Ss_assign_ch(uint32_t *a)   { return g_assign_ch(a, 1); }
uint32_t shim_Ss_pluseq_ch(uint32_t *a)   { return g_pluseq_ch(a, 1); }
uint32_t shim_Ss_ctor_substr(uint32_t *a) { return g_ctor_substr(a, 1); }

/* wide: remaining out-of-line members */
uint32_t shim_Sw_ffo_buf3(uint32_t *a)    { return g_ffo_buf3(a, 4); }
uint32_t shim_Sw_flno_buf3(uint32_t *a)   { return g_flno_buf3(a, 4); }
uint32_t shim_Sw_ffno_buf3(uint32_t *a)   { return g_ffno_buf3(a, 4); }
uint32_t shim_Sw_compare_sub_str(uint32_t *a) { return g_compare_sub_str(a, 4); }
uint32_t shim_Sw_S_construct(uint32_t *a) { return g_S_construct_fill(a, 4); }
uint32_t shim_Sw_M_destroy(uint32_t *a)   { return g_M_destroy(a, 4); }
uint32_t shim_Sw_S_create(uint32_t *a)    { return g_S_create(a, 4); }
uint32_t shim_Sw_append_buf(uint32_t *a)  { return g_append_buf(a, 4); }
uint32_t shim_Sw_resize(uint32_t *a)      { return g_resize(a, 4); }
uint32_t shim_Sw_reserve(uint32_t *a)     { return g_reserve(a, 4); }
uint32_t shim_Sw_ctor_buf(uint32_t *a)    { return g_ctor_buf(a, 4); }

/* operator+ (sret) */
uint32_t shim_Ss_opplus_cstr_str(uint32_t *a) {  /* (const char* lhs, const string& rhs) */
    uint32_t rhs = *obj_at(a, 2);
    return opplus_build(a[0], a[1], chr_len(a[1], 1), rhs, rep_of(rhs)->length, 1);
}
uint32_t shim_Ss_opplus_str_str(uint32_t *a) {   /* (const string& lhs, const string& rhs) */
    uint32_t lhs = *obj_at(a, 1), rhs = *obj_at(a, 2);
    return opplus_build(a[0], lhs, rep_of(lhs)->length, rhs, rep_of(rhs)->length, 1);
}
uint32_t shim_Sw_opplus_str_str(uint32_t *a) {   /* (const wstring& lhs, const wstring& rhs) */
    uint32_t lhs = *obj_at(a, 1), rhs = *obj_at(a, 2);
    return opplus_build(a[0], lhs, rep_of(lhs)->length, rhs, rep_of(rhs)->length, 4);
}

/* __gnu_cxx::operator==(const __normal_iterator&, const __normal_iterator&) */
uint32_t shim_iter_eq(uint32_t *a) {
    return (*(uint32_t *)(uintptr_t)a[0] == *(uint32_t *)(uintptr_t)a[1]) ? 1u : 0u;
}

/* std::allocator<char/wchar_t> ctor/dtor: stateless -> no-op */
uint32_t shim_alloc_noop(uint32_t *a) { (void)a; return 0; }

/* __gnu_cxx::__exchange_and_add / __atomic_add(volatile _Atomic_word*, int):
 * the refcount atomics inlined COW code calls out of line (_M_dispose /
 * _M_refcopy). Raw-bound to native they over-pop the i386 return address. */
uint32_t shim_exchange_and_add(uint32_t *a) {
    return (uint32_t)__atomic_fetch_add((int32_t *)(uintptr_t)a[0], (int32_t)a[1], __ATOMIC_ACQ_REL);
}
uint32_t shim_atomic_add(uint32_t *a) {
    __atomic_fetch_add((int32_t *)(uintptr_t)a[0], (int32_t)a[1], __ATOMIC_ACQ_REL);
    return 0;
}

/* std::__throw_logic_error(const char*): an i386 C++ exception cannot be
 * raised from here. ponytail: abort loudly like shim_terminate; bridge the
 * throw if a target is seen catching it. */
uint32_t shim_throw_logic_error(uint32_t *a) {
    fprintf(stderr, "[cow_string] std::__throw_logic_error(\"%s\") in translated "
            "i386 code — aborting\n", a[0] ? (const char *)(uintptr_t)a[0] : "");
    fflush(stderr);
    abort();
    return 0;
}

/* std::terminate(): graceful, better than a raw native jump */
uint32_t shim_terminate(uint32_t *a) {
    (void)a;
    fprintf(stderr, "[cow_string] std::terminate() reached in translated i386 "
            "code — aborting\n");
    fflush(stderr);
    abort();
    return 0;
}

/* ===================== std::__detail::_List_node_base ====================== */
/* node_base layout: { uint32_t _M_next @0; uint32_t _M_prev @4 } (verified). */
struct lnode { uint32_t next; uint32_t prev; };
static struct lnode *LN(uint32_t p) { return (struct lnode *)(uintptr_t)p; }

/* hook(this, __position): insert `this` before `__position`. */
uint32_t shim_list_hook(uint32_t *a) {
    uint32_t self = a[0], pos = a[1];
    LN(self)->next = pos;
    LN(self)->prev = LN(pos)->prev;
    LN(LN(pos)->prev)->next = self;
    LN(pos)->prev = self;
    return 0;
}
/* unhook(this): splice `this` out of its list. */
uint32_t shim_list_unhook(uint32_t *a) {
    uint32_t self = a[0];
    uint32_t nx = LN(self)->next, pv = LN(self)->prev;
    LN(pv)->next = nx;
    LN(nx)->prev = pv;
    return 0;
}
/* transfer(this, __first, __last): move [__first,__last) before `this`. */
uint32_t shim_list_transfer(uint32_t *a) {
    uint32_t self = a[0], first = a[1], last = a[2];
    if (self == last) return 0;
    LN(LN(last)->prev)->next  = self;
    LN(LN(first)->prev)->next = last;
    uint32_t tmp = LN(self)->prev;
    LN(tmp)->next             = first;
    LN(self)->prev            = LN(last)->prev;
    LN(last)->prev            = LN(first)->prev;
    LN(first)->prev           = tmp;
    return 0;
}
/* swap(__x, __y): exchange two list rings (empty-ring self-referential fixups). */
uint32_t shim_list_swap(uint32_t *a) {
    uint32_t x = a[0], y = a[1];
    if (LN(x)->next != x) {
        if (LN(y)->next != y) {
            uint32_t tn = LN(x)->next, tp = LN(x)->prev;
            LN(x)->next = LN(y)->next; LN(x)->prev = LN(y)->prev;
            LN(y)->next = tn;          LN(y)->prev = tp;
            LN(LN(x)->next)->prev = x; LN(LN(x)->prev)->next = x;
            LN(LN(y)->next)->prev = y; LN(LN(y)->prev)->next = y;
        } else {
            LN(y)->next = LN(x)->next; LN(y)->prev = LN(x)->prev;
            LN(LN(y)->next)->prev = y; LN(LN(y)->prev)->next = y;
            LN(x)->next = x;           LN(x)->prev = x;
        }
    } else if (LN(y)->next != y) {
        LN(x)->next = LN(y)->next; LN(x)->prev = LN(y)->prev;
        LN(LN(x)->next)->prev = x; LN(LN(x)->prev)->next = x;
        LN(y)->next = y;           LN(y)->prev = y;
    }
    return 0;
}

/* PvZ (PopCap framework): members its imports reach that the Portal 2 / Civ
 * audits never needed. Same i386 COW layout helpers as above. */
static uint32_t g_erase_iters(uint32_t *a, int cw) {    /* erase(first, last) -> iterator */
    uint32_t *obj = obj_at(a, 0);
    uint32_t pos = (a[1] - *obj) / cw, n = (a[2] - a[1]) / cw;
    uint32_t np = str_replace_raw(obj, pos, n, 0, 0, cw);
    return np + pos * cw;
}
static uint32_t g_assign_fill(uint32_t *a, int cw) {    /* assign(n, c) */
    uint32_t b[5] = { a[0], 0, rep_of(*obj_at(a, 0))->length, a[1], a[2] };
    return g_replace_aux(b, cw);
}
static uint32_t g_insert_iter_ch(uint32_t *a, int cw) { /* insert(iterator, c) -> iterator */
    uint32_t *obj = obj_at(a, 0);
    uint32_t pos = (a[1] - *obj) / cw;
    uint32_t b[5] = { a[0], pos, 0, 1, a[2] };
    g_replace_aux(b, cw);
    return *obj + pos * cw;
}
uint32_t shim_Ss_erase_iters(uint32_t *a) { return g_erase_iters(a, 1); }
uint32_t shim_Ss_assign_fill(uint32_t *a) { return g_assign_fill(a, 1); }
uint32_t shim_Ss_insert_iter_ch(uint32_t *a) { return g_insert_iter_ch(a, 1); }
uint32_t shim_Sw_erase_range(uint32_t *a) { return g_erase_range(a, 4); }
uint32_t shim_Sw_push_back(uint32_t *a)   { return g_push_back(a, 4); }
/* const rbegin()/rend(): a one-pointer reverse_iterator, returned in eax
 * (Darwin i386 returns small trivially-copyable structs in registers). */
uint32_t shim_Sw_crbegin(uint32_t *a)     { return g_cend(a, 4); }
uint32_t shim_Sw_crend(uint32_t *a)       { return g_cbegin(a, 4); }
/* std::allocator<T> ctors/dtors: stateless, nothing to do. */
uint32_t shim_allocator_nop(uint32_t *a)  { return a[0]; }

/* Build a narrow i386 std::string into the slot at `obj` from a low-4GB
 * buffer. For shims that return strings by value (iostream_shim.c). */
void cow_str_init(uint32_t obj, uint32_t buf, uint32_t n) {
    str_init_buf((uint32_t *)(uintptr_t)obj, buf, n, 1);
}
