/*
 * malloc_shim.c — low-4GB heap for the translated i386 program.
 *
 * The i386 target stores pointers in 32-bit slots, so every value malloc
 * hands back must fit in 32 bits. macOS's real malloc returns addresses
 * well above 4 GB; the i386 code truncates them and then faults on the
 * bogus low-32 pointer (photocd's fread destination buffer, for one).
 *
 * abiconv's autogen shims (`___malloc` etc.) do the i386->x86_64 ABI lift
 * and then `call _malloc` / `_free` / `_calloc` / `_realloc`. Defining
 * those symbols here intercepts the call — the same mechanism file_shim.c
 * uses for the stdio functions — and satisfies the request from a region
 * reserved below 4 GB.
 *
 * The allocator is deliberately minimal: a bump pointer over reserved
 * regions, plus a single first-fit free list so high-churn uniform-size
 * allocations get recycled. Freed blocks are never split or coalesced. The
 * translated targets are short-lived, so this trades a little high-water
 * memory for simplicity and the absence of a fragmentation class of bug.
 *
 * Thread-safety: all entry points take a single os_unfair_lock. Real GUI
 * targets (iPhoto, Tessera) spin up worker threads, so the older
 * single-threaded assumption corrupted the heap; the lock is mandatory.
 * Reserves grow on demand (additional regions) so large apps don't hit a
 * hard ceiling, and free() validates the pointer belongs to one of our
 * regions before touching it (a stray/double free from the translated
 * program would otherwise corrupt the free list).
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <os/lock.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>

/* Reserve the heap inside the wrapper's low-4GB window [0x80000000,
 * 0xF0000000). Start the scan above the bottom 128 MB so the wrapper's
 * small shim-FILE / scratch allocations (which cluster at the base) don't
 * fragment the search. Regions are reserved, not committed — macOS faults
 * pages in on first touch, so an oversized reservation is free. */
#define HEAP_SCAN_LO  0x88000000UL
#define HEAP_SCAN_HI  0xF0000000UL
#define HEAP_SIZE     (768UL * 1024 * 1024)
#define HEAP_STEP     (16UL * 1024 * 1024)
#define MAX_REGIONS   16

/* 16-byte block header; payload (returned to caller) follows, 16-aligned.
 * `size` is the payload capacity. `next` links free blocks. */
struct block {
   uint64_t size;
   struct block *next;
};

struct region {
   char *base;
   char *cur;
   char *end;
};

static struct region g_regions[MAX_REGIONS];
static int           g_nregions = 0;
static struct block *free_list;
static os_unfair_lock g_lock = OS_UNFAIR_LOCK_INIT;

/* Reserve one more low-4GB region. Caller holds g_lock. Returns the new
 * region or NULL if the address window is exhausted. */
static struct region *add_region(void) {
   if (g_nregions >= MAX_REGIONS) {
      return NULL;
   }
   for (uintptr_t a = HEAP_SCAN_LO; a + HEAP_SIZE <= HEAP_SCAN_HI;
        a += HEAP_STEP) {
      mach_vm_address_t addr = a;
      if (mach_vm_allocate(mach_task_self(), &addr, HEAP_SIZE,
                           VM_FLAGS_FIXED) == KERN_SUCCESS) {
         struct region *r = &g_regions[g_nregions++];
         r->base = (char *)(uintptr_t)addr;
         r->cur  = r->base;
         r->end  = r->base + HEAP_SIZE;
         return r;
      }
   }
   return NULL;
}

static size_t round_up(size_t n, size_t a) {
   return (n + (a - 1)) & ~(a - 1);
}

/* True if p points into the payload area of some region we own. */
static int owned(const void *p) {
   uintptr_t a = (uintptr_t)p;
   for (int i = 0; i < g_nregions; ++i) {
      if (a > (uintptr_t)g_regions[i].base
          && a <= (uintptr_t)g_regions[i].end) {
         return 1;
      }
   }
   return 0;
}

/* Carve a fresh block of `cap` payload bytes whose payload is aligned to
 * `align` (>=16). Caller holds g_lock. Writes the block header immediately
 * before the returned payload so free()/realloc() recover it at p-sizeof. */
static void *bump(size_t cap, size_t align) {
   if (align < 16) align = 16;
   for (int attempt = 0; attempt < 2; ++attempt) {
      for (int i = 0; i < g_nregions; ++i) {
         struct region *r = &g_regions[i];
         uintptr_t cur = (uintptr_t)r->cur;
         /* payload must clear a header and satisfy alignment */
         uintptr_t payload = round_up(cur + sizeof(struct block), align);
         uintptr_t next = payload + cap;
         if (next <= (uintptr_t)r->end) {
            struct block *b = (struct block *)(payload - sizeof(struct block));
            b->size = cap;
            b->next = NULL;
            r->cur = (char *)next;
            return (void *)payload;
         }
      }
      /* No region had room — reserve another and retry once. */
      if (!add_region()) break;
   }
   return NULL;
}

void *malloc(size_t n) {
   if (n == 0) n = 16;
   const size_t cap = round_up(n, 16);
   os_unfair_lock_lock(&g_lock);
   if (g_nregions == 0 && !add_region()) {
      os_unfair_lock_unlock(&g_lock);
      errno = ENOMEM;
      return NULL;
   }
   /* first-fit reuse (16-aligned blocks always satisfy default alignment) */
   for (struct block **pp = &free_list; *pp != NULL; pp = &(*pp)->next) {
      if ((*pp)->size >= cap) {
         struct block *b = *pp;
         *pp = b->next;
         os_unfair_lock_unlock(&g_lock);
         return (char *)b + sizeof(struct block);
      }
   }
   void *p = bump(cap, 16);
   os_unfair_lock_unlock(&g_lock);
   if (!p) errno = ENOMEM;
   return p;
}

void free(void *p) {
   if (p == NULL) return;
   os_unfair_lock_lock(&g_lock);
   /* Ignore pointers that aren't ours: a stray or double free from the
    * translated program must not be allowed to corrupt the free list. */
   if (!owned(p)) {
      os_unfair_lock_unlock(&g_lock);
      return;
   }
   struct block *b = (struct block *)((char *)p - sizeof(struct block));
   b->next = free_list;
   free_list = b;
   os_unfair_lock_unlock(&g_lock);
}

void *calloc(size_t count, size_t size) {
   size_t n;
   if (__builtin_mul_overflow(count, size, &n)) {
      errno = ENOMEM;
      return NULL;
   }
   void *p = malloc(n);
   if (p != NULL) {
      memset(p, 0, n);
   }
   return p;
}

void *realloc(void *p, size_t n) {
   if (p == NULL) {
      return malloc(n);
   }
   if (n == 0) {
      free(p);
      return malloc(0);
   }
   /* If it isn't ours, we can't safely inspect its header; allocate fresh.
    * (We can't know the old size to copy, so this only preserves data for
    * our own pointers — which is all the translated program should pass.) */
   os_unfair_lock_lock(&g_lock);
   int mine = owned(p);
   uint64_t oldcap = 0;
   if (mine) oldcap = ((struct block *)((char *)p - sizeof(struct block)))->size;
   os_unfair_lock_unlock(&g_lock);

   if (mine && oldcap >= round_up(n, 16)) {
      return p;   /* current block already big enough */
   }
   void *np = malloc(n);
   if (np != NULL && mine) {
      memcpy(np, p, oldcap < n ? oldcap : n);
      free(p);
   }
   return np;
}

/* reallocf: like realloc but frees the original on failure (BSD/macOS). */
void *reallocf(void *p, size_t n) {
   void *np = realloc(p, n);
   if (!np && p) free(p);
   return np;
}

/* Aligned allocators. These bypass the free list (recycled blocks may not
 * meet the requested alignment) and bump a freshly aligned block. */
int posix_memalign(void **out, size_t align, size_t n) {
   if (out == NULL) return EINVAL;
   /* alignment must be a power of two and a multiple of sizeof(void*) */
   if (align < sizeof(void *) || (align & (align - 1)) != 0) {
      return EINVAL;
   }
   if (n == 0) { *out = NULL; return 0; }
   os_unfair_lock_lock(&g_lock);
   if (g_nregions == 0) add_region();
   void *p = bump(round_up(n, 16), align);
   os_unfair_lock_unlock(&g_lock);
   if (!p) return ENOMEM;
   *out = p;
   return 0;
}

void *aligned_alloc(size_t align, size_t n) {
   void *p = NULL;
   if (posix_memalign(&p, align, n) != 0) return NULL;
   return p;
}

void *valloc(size_t n) {
   void *p = NULL;
   if (posix_memalign(&p, 4096, n) != 0) return NULL;
   return p;
}

/* malloc_size / malloc_good_size: Foundation and CF occasionally query the
 * usable size of a block. Report the payload capacity for our pointers, 0
 * for anything we didn't allocate. */
size_t malloc_size(const void *p) {
   if (p == NULL) return 0;
   os_unfair_lock_lock(&g_lock);
   size_t sz = 0;
   if (owned(p)) {
      sz = (size_t)((struct block *)((char *)p - sizeof(struct block)))->size;
   }
   os_unfair_lock_unlock(&g_lock);
   return sz;
}

size_t malloc_good_size(size_t n) {
   return round_up(n == 0 ? 16 : n, 16);
}
