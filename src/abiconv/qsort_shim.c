/*
 * qsort_shim.c — ONE job: qsort with an i386 comparator.
 *
 * libc's qsort is an introsort: on a bad pivot sequence it falls back to
 * heapsort, which compares against TEMPORARY elements it mallocs itself —
 * above 4 GB. The callback bridge hands comparator args to i386 code as 32-bit
 * words, so those temporaries arrived truncated (0x6000_01aeb870 ->
 * 0x01aeb870) and a comparator that dereferences its element read garbage
 * (PvZ's sound-instance sort, **a -> SIGSEGV; only on inputs that trip the
 * fallback). Here native qsort_r sorts an INDEX array and the i386 comparator
 * only ever sees pointers into the caller's own (low) array; the result is
 * applied with one permutation. Kill switch M64_NO_QSORT_SHIM=1 (plain
 * native qsort through the callback bridge).
 * ABI: MTSHIM (rdi -> &i386 args[0]); symbol in custom.syms.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cb_bridge.h"

/* two plain words in, int out */
static const x64_cb_sig k_sig2 = { 2, CBR_I32SX, { CBA_I32 }, { 0 } };

typedef struct {
   uint8_t *base;
   uint32_t width;
   int32_t (*cmp)(uint64_t, uint64_t);
} qctx;

static int by_index(void *vc, const void *pa, const void *pb)
{
   const qctx *c = vc;
   const uint32_t ia = *(const uint32_t *)pa, ib = *(const uint32_t *)pb;
   return c->cmp((uint32_t)(uintptr_t)(c->base + (size_t)ia * c->width),
                 (uint32_t)(uintptr_t)(c->base + (size_t)ib * c->width));
}

/* void qsort(void *base, size_t n, size_t width, int (*cmp)(const void *, const void *)) */
void shim_qsort(uint32_t *a)
{
   uint8_t *base = (uint8_t *)(uintptr_t)a[0];
   const uint32_t n = a[1], w = a[2];
   int32_t (*cmp)(uint64_t, uint64_t) = (void *)(uintptr_t)x64_cb_wrap(a[3], &k_sig2);
   if (!base || n < 2 || !w || !cmp) { return; }
   static int off = -1;
   if (off < 0) { off = getenv("M64_NO_QSORT_SHIM") != NULL; }
   if (off) { qsort(base, n, w, (int (*)(const void *, const void *))cmp); return; }
   uint32_t *idx = malloc((size_t)n * sizeof *idx);
   uint8_t  *tmp = malloc((size_t)n * w);
   if (!idx || !tmp) {   /* degrade to the bridge rather than leave it unsorted */
      free(idx); free(tmp);
      qsort(base, n, w, (int (*)(const void *, const void *))cmp);
      return;
   }
   for (uint32_t i = 0; i < n; ++i) { idx[i] = i; }
   qctx c = { base, w, cmp };
   qsort_r(idx, n, sizeof *idx, &c, by_index);
   for (uint32_t i = 0; i < n; ++i) { memcpy(tmp + (size_t)i * w, base + (size_t)idx[i] * w, w); }
   memcpy(base, tmp, (size_t)n * w);
   free(idx); free(tmp);
}
