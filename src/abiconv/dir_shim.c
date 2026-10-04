/*
 * dir_shim.c — opendir/readdir/closedir for i386 callers. ONE job: give the
 * 32-bit caller a DIR* and a struct dirent it can hold.
 *
 * Native opendir returns libc's DIR (above 4GB): the truncated pointer was
 * unmapped and readdir died locking DIR->__dd_lock (Portal 2 libmilesx86,
 * sound init). And classic i386 `readdir` (no $INODE64) returns the OLD
 * dirent: u32 d_ino, u16 d_reclen, u8 d_type, u8 d_namlen, char d_name[256].
 * The caller gets a low wrapper owning the native DIR plus one converted entry
 * (valid until the next readdir/closedir on that stream, as POSIX allows).
 * The $INODE64 variants keep their abigen bridges (native layout).
 *
 * scandir/alphasort (classic) build the same i386 dirents: native scandir handed
 * the i386 select/compar callbacks native 64-bit dirent pointers, truncated to
 * 32 bits (Portal 2 filesystem_stdio FS_FindNextFile -> SIGSEGV in alphasort).
 *
 * Kill switch M64_NO_DIR_SHIM=1 hands back the raw native DIR and dirent (the old
 * bridge). Guard tests-i386 dir-handle.
 */
#include <dirent.h>
#include <malloc/malloc.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cb_bridge.h"

#define P(v) ((void *)(uintptr_t)(uint32_t)(v))
#define DIR_MAGIC 0x44495233u   /* 'DIR3' */

struct dirent32 { uint32_t d_ino; uint16_t d_reclen; uint8_t d_type, d_namlen; char d_name[256]; };
_Static_assert(sizeof(struct dirent32) == 264, "i386 struct dirent");

struct dir32 { uint32_t magic; DIR *real; struct dirent32 ent; };

static int raw(void) {
   static int r = -1;
   if (r < 0) r = getenv("M64_NO_DIR_SHIM") != NULL;
   return r;
}

/* DIR *opendir(const char *) */
uint32_t shim_opendir(uint32_t *a) {
   DIR *d = opendir(P(a[0]));
   if (!d || raw()) return (uint32_t)(uintptr_t)d;
   struct dir32 *w = calloc(1, sizeof *w);              /* libabiconv calloc: low heap */
   if (!w) { closedir(d); return 0; }
   w->magic = DIR_MAGIC; w->real = d;
   return (uint32_t)(uintptr_t)w;
}

static void to_dirent32(struct dirent32 *d, const struct dirent *e) {
   size_t n = e->d_namlen < 255 ? e->d_namlen : 255;
   d->d_ino = (uint32_t)e->d_ino;
   d->d_reclen = sizeof *d;
   d->d_type = e->d_type;
   d->d_namlen = (uint8_t)n;
   memcpy(d->d_name, e->d_name, n);
   d->d_name[n] = 0;
}

static struct dir32 *dir_of(uint32_t h) {
   struct dir32 *w = P(h);
   return (w && w->magic == DIR_MAGIC) ? w : NULL;
}

/* struct dirent *readdir(DIR *) */
uint32_t shim_readdir(uint32_t *a) {
   struct dir32 *w = dir_of(a[0]);
   if (!w) return (uint32_t)(uintptr_t)readdir(P(a[0]));
   struct dirent *e = readdir(w->real);
   if (!e) return 0;
   to_dirent32(&w->ent, e);
   return (uint32_t)(uintptr_t)&w->ent;
}

/* int closedir(DIR *) */
uint32_t shim_closedir(uint32_t *a) {
   struct dir32 *w = dir_of(a[0]);
   if (!w) return (uint32_t)closedir(P(a[0]));
   int r = closedir(w->real);
   w->magic = 0;
   free(w);
   return (uint32_t)r;
}

/* int alphasort(const struct dirent **, const struct dirent **) — i386 dirents */
uint32_t shim_alphasort(uint32_t *a) {
   const struct dirent32 *x = P(*(uint32_t *)P(a[0])), *y = P(*(uint32_t *)P(a[1]));
   return (uint32_t)strcoll(x->d_name, y->d_name);
}

static const x64_cb_sig k_sig1 = { 1, CBR_I32SX, { CBA_I32 }, { 0 } };
static const x64_cb_sig k_sig2 = { 2, CBR_I32SX, { CBA_I32, CBA_I32 }, { 0 } };

typedef struct { uint32_t *list; int32_t (*cmp)(uint64_t, uint64_t); } sctx;

/* the i386 comparator only ever sees pointers into the (low) namelist */
static int by_slot(void *vc, const void *pa, const void *pb) {
   const sctx *c = vc;
   return c->cmp((uint32_t)(uintptr_t)(c->list + *(const uint32_t *)pa),
                 (uint32_t)(uintptr_t)(c->list + *(const uint32_t *)pb));
}

/* int scandir(const char *, struct dirent ***, int (*select)(const struct dirent *),
 *             int (*compar)(const struct dirent **, const struct dirent **))
 * The namelist and every entry come from the low heap: the caller free()s them. */
uint32_t shim_scandir(uint32_t *a) {
   if (raw()) {
      struct dirent **l64 = NULL;
      int n = scandir(P(a[0]), &l64,
                      a[2] ? (void *)(uintptr_t)x64_cb_wrap(a[2], &k_sig1) : NULL,
                      a[3] ? (void *)(uintptr_t)x64_cb_wrap(a[3], &k_sig2) : NULL);
      *(uint32_t *)P(a[1]) = (uint32_t)(uintptr_t)l64;
      return (uint32_t)n;
   }
   DIR *d = opendir(P(a[0]));
   if (!d) return (uint32_t)-1;
   int32_t (*sel)(uint64_t) = a[2] ? (void *)(uintptr_t)x64_cb_wrap(a[2], &k_sig1) : NULL;
   int32_t (*cmp)(uint64_t, uint64_t) = a[3] ? (void *)(uintptr_t)x64_cb_wrap(a[3], &k_sig2) : NULL;
   uint32_t *list = NULL, n = 0, cap = 0;
   for (struct dirent *e; (e = readdir(d)); ) {
      struct dirent32 *x = malloc(sizeof *x);
      if (!x) goto fail;
      to_dirent32(x, e);
      if (sel && !sel((uint32_t)(uintptr_t)x)) { free(x); continue; }
      if (n == cap) {
         uint32_t *grown = realloc(list, (cap = cap ? cap * 2 : 32) * sizeof *list);
         if (!grown) { free(x); goto fail; }
         list = grown;
      }
      list[n++] = (uint32_t)(uintptr_t)x;
   }
   closedir(d);
   if (cmp && n > 1) {
      malloc_zone_t *z = malloc_default_zone();
      uint32_t *idx = malloc_zone_malloc(z, n * sizeof *idx * 2);
      if (idx) {
         uint32_t *sorted = idx + n;
         for (uint32_t i = 0; i < n; ++i) idx[i] = i;
         sctx c = { list, cmp };
         qsort_r(idx, n, sizeof *idx, &c, by_slot);
         for (uint32_t i = 0; i < n; ++i) sorted[i] = list[idx[i]];
         memcpy(list, sorted, n * sizeof *list);
         malloc_zone_free(z, idx);
      }
   }
   *(uint32_t *)P(a[1]) = (uint32_t)(uintptr_t)list;
   return n;
fail:
   closedir(d);
   while (n) free(P(list[--n]));
   free(list);
   *(uint32_t *)P(a[1]) = 0;
   return (uint32_t)-1;
}
