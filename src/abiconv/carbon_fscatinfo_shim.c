/*
 * carbon_fscatinfo_shim.c — ONE job: marshal `FSCatalogInfo` across the i386 /
 * x86_64 boundary, INCLUDING the classic File Manager entry points that fill an
 * ARRAY of them.
 *
 * ── WHY THE GENERIC BRIDGE CANNOT DO THIS ───────────────────────────────────
 *
 * `FSCatalogInfo` is one of the very few classic structs whose two layouts
 * genuinely DIFFER, because `FSPermissionInfo` (CarbonCore/Files.h, inside
 * `#pragma pack(push, 2)`) ends with a POINTER:
 *
 *     struct FSPermissionInfo { UInt32 userID, groupID; UInt8 reserved1,
 *                               userAccess; UInt16 mode;
 *                               FSFileSecurityRef fileSec; };   <- 4B / 8B
 *
 * MEASURED with the real headers: sizeof(FSCatalogInfo) is 144 on i386 and 148
 * on x86_64; everything up to `permissions.fileSec` (offset 68) is byte
 * identical, and everything after it is byte identical too, just shifted by 4.
 * `FSRef` (80), `FSSpec` (70) and `HFSUniStr255` (512) are identical on both, so
 * only this one struct needs converting.
 *
 * abigen sees the differing layouts, correctly declines `record_is_layout_
 * identical()`, and routes `FSCatalogInfo *` to typeconv.cc's deep-copy path.
 * That path stages the pointee in a scratch slot of the shim's own frame sized
 * `sizeof_type(pointee, x86_64)` — room for exactly ONE struct. For the
 * single-item APIs that is right. For the BULK APIs it is catastrophic:
 *
 *     FSGetCatalogInfoBulk(iter, maximumObjects, &actual, ..., catalogInfos,
 *                          refs, specs, names)
 *
 * fills `actual` (up to `maximumObjects`) entries. Handed a one-element bounce
 * buffer, the NATIVE callee writes actual*148 bytes into it, overruns the shim's
 * 880-byte frame, and scribbles straight up through `rbp` into the i386 caller's
 * own locals.
 *
 * MEASURED on Civilization IV (2026-08-05, M64_FAULT_REPORT):
 *   SIGSEGV at `Civilization IV.dylib+0x136c6e`, `movzwl (%rdx),%eax` with
 *   rdx = 0, reading `HFSUniStr255.length` in a directory-walk loop. The two
 *   `operator new[]` buffers the walk had just allocated AND null-checked
 *   (0x2400 bytes of FSCatalogInfo, 0x8000 of HFSUniStr255) both read back as 0
 *   from the caller's frame, while `actualObjects` came back a correct 11 and
 *   the return was a correct errFSNoMoreItems. Between the null-check and the
 *   crash the only call is FSGetCatalogInfoBulk: 11 * 148 = 1628 bytes into an
 *   880-byte frame.
 *
 * This is STRUCTURAL, not a Civ quirk: any classic app that enumerates a
 * directory smashes its own stack. The second-order defect is just as general —
 * abigen's field walk advances by `sizeof_type(UTCDateTime)` = 12 (the natural
 * size) rather than the packed 8, so it places `contentModDate` onwards at the
 * wrong offsets and mis-copies every date/size/valence field even in the
 * single-item `FSGetCatalogInfo`.
 *
 * ── WHAT THIS MODULE DOES ───────────────────────────────────────────────────
 *
 * It owns the whole `FSCatalogInfo`-taking family and converts N entries, where
 * N is what the API itself says it filled:
 *
 *   FSGetCatalogInfoBulk        out, N = *actualObjects
 *   FSCatalogSearch             out, N = *actualObjects
 *   FSGetCatalogInfo            out, N = 1
 *   FSSetCatalogInfo            in,  N = 1
 *   FSCreateFileUnicode         in,  N = 1
 *   FSCreateDirectoryUnicode    in,  N = 1
 *   FSCreateFileAndOpenForkUnicode  in, N = 1
 *
 * The offsets are DERIVED from the live headers with offsetof/sizeof, so they
 * follow Apple's struct rather than a transcription of it.
 *
 * `ItemCount` is `unsigned long`: 4 bytes on i386, 8 on x86_64, so every
 * `ItemCount` value and out-param is widened/narrowed explicitly here too.
 *
 * ★A failing shim must leave every out-param DEFINED. Every path below zeroes
 * the caller's arrays and `*actualObjects` before it can return an error, so no
 * caller ever reads its own uninitialised stack back as catalog data.
 *
 *   kill switch : M64_NO_FSCATINFO=1  — decline: out-params zeroed and defined,
 *                 `*actualObjects` = 0, `paramErr` returned. NOT "call through",
 *                 because calling through is the smashing path this exists to
 *                 remove.
 *   trace       : ABICONV_FSCATINFO_TRACE=1
 *
 * Wired through carbon_fscatinfo_tramp.asm (MTSHIM): rdi -> &i386 args[0],
 * return in eax. i386 arg slots are 4 bytes; a translated pointer is a 32-bit
 * low-4GB address, directly dereferenceable from here.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <CoreServices/CoreServices.h>

#include "carbon_shim.h"      /* i386_ptr() */

/* Proxy arena (objc_shim.c): a native pointer that lives above 4GB cannot be
 * handed to i386 code raw, so it travels as a 32-bit arena handle.
 *
 * ★The FSIterator is one of these. It is minted by the (abigen-generated)
 * FSOpenIterator bridge, which stores the native >4GB iterator into the
 * caller's 4-byte slot as a handle; the abigen FSGetCatalogInfoBulk bridge this
 * module replaces resolved it with `_86x64_unwrap_obj_arg` before the call
 * (libabiconv+0xd330b). Passing the raw handle through instead makes the native
 * call fail with errFSIteratorNotSupported (-1424) — measured. */
extern uint64_t _86x64_unwrap_obj_arg(uint32_t handle);
extern uint32_t x64_objc_wrap(uint64_t real);

#define FM_NO_ERR       0
#define FM_PARAM_ERR  (-50)   /* paramErr */

/* ── the layout delta, derived from the headers ───────────────────────────── */

/* bytes before `permissions.fileSec` — identical on both ABIs */
#define FSCI_HEAD  (offsetof(FSCatalogInfo, permissions) + offsetof(FSPermissionInfo, fileSec))
/* bytes after it — identical on both ABIs, shifted by the pointer-width delta */
#define FSCI_TAIL  (sizeof(FSCatalogInfo) - (FSCI_HEAD + sizeof(FSFileSecurityRef)))
#define FSCI_SZ64  (sizeof(FSCatalogInfo))          /* 148 */
#define FSCI_SZ32  (FSCI_HEAD + 4 + FSCI_TAIL)      /* 144 */

/* Compile-time proof that the ONLY difference is the fileSec pointer width; if
 * Apple ever changes the struct this fails the build instead of corrupting a
 * caller's buffer silently. */
_Static_assert(sizeof(FSFileSecurityRef) == 8, "x86_64 FSFileSecurityRef must be 8 bytes");
_Static_assert(FSCI_SZ64 == FSCI_SZ32 + 4, "FSCatalogInfo delta must be exactly the pointer width");
_Static_assert(sizeof(FSRef) == 80 && sizeof(HFSUniStr255) == 512,
               "FSRef/HFSUniStr255 must be layout-identical (no conversion emitted for them)");

/* ── kill switch / trace ──────────────────────────────────────────────────── */

static int fscatinfo_enabled(void)
{
   static int c = -1;
   if (c < 0) { const char *e = getenv("M64_NO_FSCATINFO"); c = !(e && *e && *e != '0'); }
   return c;
}
static int fscatinfo_trace(void)
{
   static int c = -1;
   if (c < 0) { const char *e = getenv("ABICONV_FSCATINFO_TRACE"); c = (e && *e && *e != '0'); }
   return c;
}

/* ── conversion ───────────────────────────────────────────────────────────── */

/* i386 image -> x86_64 image (arguments going IN) */
static void fsci_in(void *dst64, const void *src32)
{
   const char *s = (const char *)src32;
   char *d = (char *)dst64;
   memcpy(d, s, FSCI_HEAD);
   uint32_t h = 0;
   memcpy(&h, s + FSCI_HEAD, 4);
   uint64_t p = h ? _86x64_unwrap_obj_arg(h) : 0;
   memcpy(d + FSCI_HEAD, &p, 8);
   memcpy(d + FSCI_HEAD + 8, s + FSCI_HEAD + 4, FSCI_TAIL);
}

/* x86_64 image -> i386 image (out-params coming BACK) */
static void fsci_out(void *dst32, const void *src64)
{
   const char *s = (const char *)src64;
   char *d = (char *)dst32;
   memcpy(d, s, FSCI_HEAD);
   uint64_t p = 0;
   memcpy(&p, s + FSCI_HEAD, 8);
   /* Only a genuinely >4GB pointer needs an arena handle; a low value (almost
    * always 0, since fileSec is filled only for kFSCatInfoFSFileSecurityRef)
    * passes through unchanged — the same rule convert_cf_ptr uses. */
   uint32_t h = (p >= 0x100000000ULL) ? x64_objc_wrap(p) : (uint32_t)p;
   memcpy(d + FSCI_HEAD, &h, 4);
   memcpy(d + FSCI_HEAD + 4, s + FSCI_HEAD + 8, FSCI_TAIL);
}

static void fsci_in_n(void *dst64, const void *src32, size_t n)
{
   for (size_t i = 0; i < n; ++i)
      fsci_in((char *)dst64 + i * FSCI_SZ64, (const char *)src32 + i * FSCI_SZ32);
}
static void fsci_out_n(void *dst32, const void *src64, size_t n)
{
   for (size_t i = 0; i < n; ++i)
      fsci_out((char *)dst32 + i * FSCI_SZ32, (const char *)src64 + i * FSCI_SZ64);
}

/* ── the two ARRAY entry points ───────────────────────────────────────────── */

/* Shared body for FSGetCatalogInfoBulk and FSCatalogSearch: everything after
 * `maximumObjects` is identical between them, and it is the array handling —
 * not the iterator/criteria half — that the generic bridge gets wrong.
 *
 *   ci32/refs32/specs32/names32 are i386 pointers (may be 0 = "not wanted").
 *   `call_native` performs the one API-specific call.
 */
struct bulk_args {
   uint32_t max;          /* maximumObjects, from the i386 4-byte slot */
   uint32_t actual32;     /* i386 ItemCount*   */
   uint32_t changed32;    /* i386 Boolean*     — 1 byte, identical, passed raw */
   uint32_t whichInfo;
   uint32_t ci32, refs32, specs32, names32;
};

static void bulk_define_outputs(const struct bulk_args *b)
{
   /* ★rule A: every out-param defined before any early return. */
   if (b->actual32)  *(uint32_t *)i386_ptr(b->actual32) = 0;
   if (b->changed32) *(uint8_t *)i386_ptr(b->changed32) = 0;
   if (b->ci32)    memset(i386_ptr(b->ci32),    0, (size_t)b->max * FSCI_SZ32);
   if (b->refs32)  memset(i386_ptr(b->refs32),  0, (size_t)b->max * sizeof(FSRef));
   if (b->specs32) memset(i386_ptr(b->specs32), 0, (size_t)b->max * sizeof(FSSpec));
   if (b->names32) memset(i386_ptr(b->names32), 0, (size_t)b->max * sizeof(HFSUniStr255));
}

/* Runs the call with a properly sized native FSCatalogInfo array and converts
 * back exactly the number of entries the API reports. */
static int32_t bulk_run(const struct bulk_args *b,
                        OSErr (*call_native)(void *ctx, ItemCount max,
                                             ItemCount *actual, Boolean *changed,
                                             FSCatalogInfoBitmap which,
                                             FSCatalogInfo *ci, FSRef *refs,
                                             FSSpec *specs, HFSUniStr255 *names),
                        void *ctx, const char *what)
{
   bulk_define_outputs(b);
   if (!fscatinfo_enabled()) return FM_PARAM_ERR;

   /* HEAP, not the shim frame: `max` is the caller's choice (Civ asks for 64,
    * i.e. 9472 bytes) and a stack buffer is exactly the mistake being fixed. */
   FSCatalogInfo *ci64 = NULL;
   if (b->ci32 && b->max) {
      ci64 = (FSCatalogInfo *)calloc((size_t)b->max, FSCI_SZ64);
      if (!ci64) return FM_PARAM_ERR;          /* out-params already defined */
   }

   ItemCount actual = 0;
   Boolean   changed = 0;
   OSErr err = call_native(ctx, (ItemCount)b->max, &actual,
                           b->changed32 ? &changed : NULL,
                           (FSCatalogInfoBitmap)b->whichInfo,
                           ci64,
                           /* layout-identical: the caller's own low-4GB buffers */
                           b->refs32  ? (FSRef *)i386_ptr(b->refs32)   : NULL,
                           b->specs32 ? (FSSpec *)i386_ptr(b->specs32) : NULL,
                           b->names32 ? (HFSUniStr255 *)i386_ptr(b->names32) : NULL);

   /* Never trust the callee to stay within maximumObjects. */
   if (actual > (ItemCount)b->max) actual = (ItemCount)b->max;

   if (ci64) fsci_out_n(i386_ptr(b->ci32), ci64, (size_t)actual);
   if (b->actual32)  *(uint32_t *)i386_ptr(b->actual32) = (uint32_t)actual;
   if (b->changed32) *(uint8_t *)i386_ptr(b->changed32) = changed;

   if (fscatinfo_trace())
      fprintf(stderr, "[fscatinfo] %s max=%u actual=%llu err=%d ci=%s\n",
              what, b->max, (unsigned long long)actual, (int)err,
              b->ci32 ? "yes" : "no");

   free(ci64);
   return (int32_t)err;
}

struct getbulk_ctx { FSIterator iter; };
static OSErr call_getbulk(void *ctx, ItemCount max, ItemCount *actual,
                          Boolean *changed, FSCatalogInfoBitmap which,
                          FSCatalogInfo *ci, FSRef *refs, FSSpec *specs,
                          HFSUniStr255 *names)
{
   return FSGetCatalogInfoBulk(((struct getbulk_ctx *)ctx)->iter, max, actual,
                               changed, which, ci, refs, specs, names);
}

/* OSErr FSGetCatalogInfoBulk(FSIterator, ItemCount maximumObjects,
 *                            ItemCount *actualObjects, Boolean *containerChanged,
 *                            FSCatalogInfoBitmap, FSCatalogInfo *, FSRef *,
 *                            FSSpecPtr, HFSUniStr255 *); */
uint32_t shim_FSGetCatalogInfoBulk(uint32_t *a)
{
   struct bulk_args b = { a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8] };
   struct getbulk_ctx ctx = { (FSIterator)(uintptr_t)_86x64_unwrap_obj_arg(a[0]) };
   return (uint32_t)bulk_run(&b, call_getbulk, &ctx, "FSGetCatalogInfoBulk");
}

/* FSSearchParams holds three pointers, so its i386 and x86_64 layouts differ
 * too; stage a native copy with the two FSCatalogInfo criteria converted. */
struct search_ctx { FSIterator iter; uint32_t crit32; FSCatalogInfo lo, hi; };

static OSErr call_search(void *ctx_, ItemCount max, ItemCount *actual,
                         Boolean *changed, FSCatalogInfoBitmap which,
                         FSCatalogInfo *ci, FSRef *refs, FSSpec *specs,
                         HFSUniStr255 *names)
{
   struct search_ctx *c = (struct search_ctx *)ctx_;
   FSSearchParams sp;
   memset(&sp, 0, sizeof sp);
   if (c->crit32) {
      /* i386 FSSearchParams: Duration searchTime @0, OptionBits searchBits @4,
       * HFSUniStr255 *searchName @8, FSCatalogInfo *searchInfo1 @12,
       * *searchInfo2 @16 — all 4-byte slots. */
      const uint32_t *s = (const uint32_t *)i386_ptr(c->crit32);
      sp.searchTime = (Duration)(int32_t)s[0];
      sp.searchBits = (OptionBits)s[1];
      sp.searchName = s[2] ? (HFSUniStr255 *)i386_ptr(s[2]) : NULL;  /* identical layout */
      if (s[3]) { fsci_in(&c->lo, i386_ptr(s[3])); sp.searchInfo1 = &c->lo; }
      if (s[4]) { fsci_in(&c->hi, i386_ptr(s[4])); sp.searchInfo2 = &c->hi; }
   }
   return FSCatalogSearch(c->iter, &sp, max, actual, changed, which,
                          ci, refs, specs, names);
}

/* OSErr FSCatalogSearch(FSIterator, const FSSearchParams *, ItemCount,
 *                       ItemCount *, Boolean *, FSCatalogInfoBitmap,
 *                       FSCatalogInfo *, FSRef *, FSSpecPtr, HFSUniStr255 *); */
uint32_t shim_FSCatalogSearch(uint32_t *a)
{
   struct bulk_args b = { a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9] };
   struct search_ctx ctx;
   memset(&ctx, 0, sizeof ctx);
   ctx.iter = (FSIterator)(uintptr_t)_86x64_unwrap_obj_arg(a[0]);
   ctx.crit32 = a[1];
   return (uint32_t)bulk_run(&b, call_search, &ctx, "FSCatalogSearch");
}

/* ── the single-item entry points ─────────────────────────────────────────── */

/* OSErr FSGetCatalogInfo(const FSRef *, FSCatalogInfoBitmap, FSCatalogInfo *,
 *                        HFSUniStr255 *outName, FSSpecPtr, FSRef *parentRef); */
uint32_t shim_FSGetCatalogInfo(uint32_t *a)
{
   FSCatalogInfo ci;
   const uint32_t ci32 = a[2];

   /* ★rule A first, so an early return can never leave stack garbage behind. */
   if (ci32) memset(i386_ptr(ci32), 0, FSCI_SZ32);
   if (a[3]) memset(i386_ptr(a[3]), 0, sizeof(HFSUniStr255));
   if (a[4]) memset(i386_ptr(a[4]), 0, sizeof(FSSpec));
   if (a[5]) memset(i386_ptr(a[5]), 0, sizeof(FSRef));
   if (!a[0] || !fscatinfo_enabled()) return FM_PARAM_ERR;

   memset(&ci, 0, sizeof ci);
   OSErr err = FSGetCatalogInfo((const FSRef *)i386_ptr(a[0]),
                                (FSCatalogInfoBitmap)a[1],
                                ci32 ? &ci : NULL,
                                a[3] ? (HFSUniStr255 *)i386_ptr(a[3]) : NULL,
                                a[4] ? (FSSpec *)i386_ptr(a[4]) : NULL,
                                a[5] ? (FSRef *)i386_ptr(a[5]) : NULL);
   if (ci32) fsci_out(i386_ptr(ci32), &ci);
   if (fscatinfo_trace())
      fprintf(stderr, "[fscatinfo] FSGetCatalogInfo which=0x%x err=%d\n", a[1], (int)err);
   return (uint32_t)(int32_t)err;
}

/* OSErr FSSetCatalogInfo(const FSRef *, FSCatalogInfoBitmap, const FSCatalogInfo *); */
uint32_t shim_FSSetCatalogInfo(uint32_t *a)
{
   FSCatalogInfo ci;
   if (!a[0] || !a[2] || !fscatinfo_enabled()) return FM_PARAM_ERR;
   fsci_in(&ci, i386_ptr(a[2]));
   OSErr err = FSSetCatalogInfo((const FSRef *)i386_ptr(a[0]),
                                (FSCatalogInfoBitmap)a[1], &ci);
   if (fscatinfo_trace())
      fprintf(stderr, "[fscatinfo] FSSetCatalogInfo which=0x%x err=%d\n", a[1], (int)err);
   return (uint32_t)(int32_t)err;
}

/* OSErr FSCreateFileUnicode(const FSRef *parentRef, UniCharCount nameLength,
 *                           const UniChar *name, FSCatalogInfoBitmap,
 *                           const FSCatalogInfo *, FSRef *newRef, FSSpecPtr); */
uint32_t shim_FSCreateFileUnicode(uint32_t *a)
{
   FSCatalogInfo ci;
   if (a[5]) memset(i386_ptr(a[5]), 0, sizeof(FSRef));    /* rule A */
   if (a[6]) memset(i386_ptr(a[6]), 0, sizeof(FSSpec));
   if (!a[0] || !fscatinfo_enabled()) return FM_PARAM_ERR;
   if (a[4]) fsci_in(&ci, i386_ptr(a[4]));
   OSErr err = FSCreateFileUnicode((const FSRef *)i386_ptr(a[0]),
                                   (UniCharCount)a[1],
                                   (const UniChar *)i386_ptr(a[2]),
                                   (FSCatalogInfoBitmap)a[3],
                                   a[4] ? &ci : NULL,
                                   a[5] ? (FSRef *)i386_ptr(a[5]) : NULL,
                                   a[6] ? (FSSpec *)i386_ptr(a[6]) : NULL);
   if (fscatinfo_trace())
      fprintf(stderr, "[fscatinfo] FSCreateFileUnicode err=%d\n", (int)err);
   return (uint32_t)(int32_t)err;
}

/* OSErr FSCreateDirectoryUnicode(const FSRef *, UniCharCount, const UniChar *,
 *                                FSCatalogInfoBitmap, const FSCatalogInfo *,
 *                                FSRef *newRef, FSSpecPtr, UInt32 *newDirID); */
uint32_t shim_FSCreateDirectoryUnicode(uint32_t *a)
{
   FSCatalogInfo ci;
   if (a[5]) memset(i386_ptr(a[5]), 0, sizeof(FSRef));    /* rule A */
   if (a[6]) memset(i386_ptr(a[6]), 0, sizeof(FSSpec));
   if (a[7]) *(uint32_t *)i386_ptr(a[7]) = 0;
   if (!a[0] || !fscatinfo_enabled()) return FM_PARAM_ERR;
   if (a[4]) fsci_in(&ci, i386_ptr(a[4]));
   UInt32 newDirID = 0;
   OSErr err = FSCreateDirectoryUnicode((const FSRef *)i386_ptr(a[0]),
                                        (UniCharCount)a[1],
                                        (const UniChar *)i386_ptr(a[2]),
                                        (FSCatalogInfoBitmap)a[3],
                                        a[4] ? &ci : NULL,
                                        a[5] ? (FSRef *)i386_ptr(a[5]) : NULL,
                                        a[6] ? (FSSpec *)i386_ptr(a[6]) : NULL,
                                        a[7] ? &newDirID : NULL);
   if (a[7]) *(uint32_t *)i386_ptr(a[7]) = (uint32_t)newDirID;
   if (fscatinfo_trace())
      fprintf(stderr, "[fscatinfo] FSCreateDirectoryUnicode err=%d dirID=%u\n",
              (int)err, (unsigned)newDirID);
   return (uint32_t)(int32_t)err;
}

/* OSStatus FSCreateFileAndOpenForkUnicode(const FSRef *, UniCharCount,
 *          const UniChar *, FSCatalogInfoBitmap, const FSCatalogInfo *,
 *          UniCharCount forkNameLength, const UniChar *forkName,
 *          SInt8 permissions, FSIORefNum *forkRefNum, FSRef *newRef); */
uint32_t shim_FSCreateFileAndOpenForkUnicode(uint32_t *a)
{
   FSCatalogInfo ci;
   if (a[8]) *(int16_t *)i386_ptr(a[8]) = 0;              /* rule A */
   if (a[9]) memset(i386_ptr(a[9]), 0, sizeof(FSRef));
   if (!a[0] || !fscatinfo_enabled()) return FM_PARAM_ERR;
   if (a[4]) fsci_in(&ci, i386_ptr(a[4]));
   FSIORefNum fork = 0;
   OSStatus err = FSCreateFileAndOpenForkUnicode(
                       (const FSRef *)i386_ptr(a[0]), (UniCharCount)a[1],
                       (const UniChar *)i386_ptr(a[2]), (FSCatalogInfoBitmap)a[3],
                       a[4] ? &ci : NULL, (UniCharCount)a[5],
                       a[6] ? (const UniChar *)i386_ptr(a[6]) : NULL,
                       (SInt8)a[7], a[8] ? &fork : NULL,
                       a[9] ? (FSRef *)i386_ptr(a[9]) : NULL);
   if (a[8]) *(int16_t *)i386_ptr(a[8]) = (int16_t)fork;
   if (fscatinfo_trace())
      fprintf(stderr, "[fscatinfo] FSCreateFileAndOpenForkUnicode err=%d\n", (int)err);
   return (uint32_t)(int32_t)err;
}
