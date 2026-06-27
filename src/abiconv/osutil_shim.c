// osutil_shim.c — REAL implementations of small classic OS utility calls Civ IV links that
// were dropped from 64-bit/modern macOS. Each of these has exact, well-defined semantics, so
// these are correct implementations (smart fix), not stubs:
//   * c2pstrcpy / p2cstrcpy — C<->Pascal string conversion (silent no-op would corrupt text)
//   * OTAtomicClearBit      — atomic clear-bit returning the previous bit (real concurrency)
//   * GetDateTime           — current time in classic Mac seconds-since-1904
//   * GetGlobalMouse/GetMouse — current cursor position via CoreGraphics
//
// MTSHIM convention: rdi -> &i386 args[0]; result in eax.

#include <stdint.h>
#include <string.h>
#include <time.h>

#define PTR(n) ((void *)(uintptr_t)args[(n)])

typedef struct { int16_t v, h; } QDPoint;

// ---- C <-> Pascal string copy (TextUtils.h) ----
// void c2pstrcpy(Str255 dst, const char *src): dst[0] = length, dst[1..] = chars (cap 255).
void shim_c2pstrcpy(uint32_t *args) {
    uint8_t *dst = (uint8_t *)PTR(0);
    const char *src = (const char *)PTR(1);
    if (!dst) return;
    size_t n = src ? strlen(src) : 0;
    if (n > 255) n = 255;
    dst[0] = (uint8_t)n;
    if (n) memcpy(dst + 1, src, n);
}
// void p2cstrcpy(char *dst, ConstStr255Param src): dst = src[1..len] + NUL.
void shim_p2cstrcpy(uint32_t *args) {
    char *dst = (char *)PTR(0);
    const uint8_t *src = (const uint8_t *)PTR(1);
    if (!dst) return;
    size_t n = src ? src[0] : 0;
    if (n) memcpy(dst, src + 1, n);
    dst[n] = '\0';
}

// ---- OTAtomicClearBit (OpenTransport.h) ----
// Boolean OTAtomicClearBit(UInt8 *bytePtr, OTByteCount bitNumber): atomically clear the bit
// at index bitNumber (0 = LSB of *bytePtr; >=8 indexes following bytes), return its prior
// value. A real atomic op — the classic code uses it for lock-free flags.
uint32_t shim_OTAtomicClearBit(uint32_t *args) {
    uint8_t *base = (uint8_t *)PTR(0);
    uint32_t bit = args[1];
    if (!base) return 0;
    uint8_t *p = base + (bit >> 3);
    uint8_t mask = (uint8_t)(1u << (bit & 7));
    uint8_t old = __atomic_fetch_and(p, (uint8_t)~mask, __ATOMIC_SEQ_CST);
    return (old & mask) ? 1u : 0u;
}

// ---- GetDateTime (DateTimeUtils.h) ----
// void GetDateTime(unsigned long *secs): classic Mac time = Unix time + 2082844800
// (seconds between 1904-01-01 and 1970-01-01).
#define MAC_EPOCH_DELTA 2082844800u
void shim_GetDateTime(uint32_t *args) {
    uint32_t *secs = (uint32_t *)PTR(0);
    if (secs) *secs = (uint32_t)((uint32_t)time(NULL) + MAC_EPOCH_DELTA);
}

// ---- GetGlobalMouse / GetMouse: current cursor position via CoreGraphics ----
// (CGEventCreate/CGEventGetLocation resolve from the x86_64 shared cache; declared locally to
// keep this file header-light.) With no GrafPort, local == global, so both return the global
// position. Falls back to (0,0) only if the event snapshot is unavailable.
typedef struct __CGEvent *CGEventRef;
typedef struct { double x, y; } CG_Point;
extern CGEventRef CGEventCreate(void *source);
extern CG_Point   CGEventGetLocation(CGEventRef event);
extern void       CFRelease(const void *cf);

static void fill_mouse(QDPoint *pt) {
    if (!pt) return;
    pt->v = 0; pt->h = 0;
    CGEventRef e = CGEventCreate(0);
    if (e) {
        CG_Point loc = CGEventGetLocation(e);
        pt->h = (int16_t)loc.x;   // horizontal = x
        pt->v = (int16_t)loc.y;   // vertical   = y
        CFRelease(e);
    }
}
void shim_GetGlobalMouse(uint32_t *args) { fill_mouse((QDPoint *)PTR(0)); }
void shim_GetMouse(uint32_t *args)       { fill_mouse((QDPoint *)PTR(0)); }
