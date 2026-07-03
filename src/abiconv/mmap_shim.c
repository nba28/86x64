/*
 * mmap_shim.c — low-4GB anonymous mmap for the translated i386 program.
 *
 * WHY THIS EXISTS (Civ IV s25)
 * ---------------------------
 * A 32-bit program that manages its own memory with mmap (Civ IV's Win32-compat
 * VirtualAlloc -> mmap; also common in game engines and JITs) has two problems
 * running as translated x86_64 on modern macOS:
 *
 *  1. FLAGS.  The i386-era code requests an anonymous mapping with MAP_ANON
 *     (0x1000) but WITHOUT MAP_PRIVATE/MAP_SHARED — the old Darwin kernel
 *     accepted that; the modern kernel returns EINVAL (verified: mmap(0,len,
 *     PROT_RW,0x1000,-1,0) -> -1/EINVAL on macOS 15).  So every such mmap FAILS.
 *
 *  2. HIGH ADDRESS + RETURN WRAP.  Even with correct flags, the real mmap hands
 *     back an address well above 4 GB, which the i386 program truncates into its
 *     32-bit pointer slots.  abigen's generated ___mmap bridge tries to keep the
 *     return representable by WRAPPING any >4GB result into a low-4GB proxy
 *     handle (x64_objc_wrap).  But mmap does not return an opaque handle — it
 *     returns RAW MEMORY the program dereferences directly (writes its buddy-
 *     allocator freelist nodes into it), so a proxy handle is unusable.  Worse,
 *     the FAILED sentinel (-1) also has its high bits set, so the wrap turns -1
 *     into a nonzero proxy — the program's `p == MAP_FAILED` check never fires,
 *     it uses the bogus pointer, corrupts its heap, and a later allocation
 *     returns NULL.  (Civ IV s25: GMemory::Alloc -> BuddyAllocSystemChunk ->
 *     VirtualAlloc -> mmap fails -> masked -> GMemory::Alloc returns 0 -> a
 *     global GMutex's GMutexImpl is constructed at `this`==0 -> the base
 *     GAcquireInterface subobject ctor stores its vtable at this+0xc==0xc ->
 *     SIGSEGV @ 0xc.)
 *
 * FIX.  Define _mmap / _munmap here.  abigen's ___mmap/___munmap bridges do the
 * i386->x86_64 arg lift and then `call _mmap`/`_munmap`; defining those symbols
 * intercepts the call — the exact mechanism malloc_shim.c uses for malloc/free.
 * For the case the 32-bit program needs — an ANONYMOUS, non-executable mapping
 * with no fixed address — we serve it from a region reserved BELOW 4 GB (so the
 * returned pointer fits a 32-bit slot and is real, dereferenceable memory) and
 * repair the missing sharing flag.  Because the result is low-4GB, abigen's
 * return-wrap sees high bits == 0 and passes the real pointer straight through.
 *
 * Everything else falls through to the real mmap: file-backed mappings (fd>=0)
 * need the real file; executable / MAP_JIT mappings (libabiconv's own init-stub
 * JIT, PROT_EXEC) need real RWX/JIT memory; and a MAP_FIXED request to a
 * specific address must honor the caller's placement.  This keeps the shim to
 * ONE job (low-4GB anonymous data mappings) and leaves the kernel's mmap
 * untouched for native dylibs (two-level namespace: their mmap binds stay on
 * libSystem; only libabiconv's own references and abigen's `call _mmap` resolve
 * here).
 *
 * Universal: triggers on the structural shape (a 32-bit program taking an
 * anonymous data mapping it will hold 32-bit pointers into), never on an app
 * name.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <os/lock.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>

/* Low-4GB band for anonymous mmap regions. Kept clear of everything else the
 * runtime places low:
 *   translated images + bundled frameworks : low, typically < 0x10000000
 *   objc_shim proxy/shadow arenas + malloc heap : [0x80000000, 0xF0000000)
 *   init stack                              : [0x87000000, 0x87800000)
 * so [0x10000000, 0x80000000) (~1.75 GB) is the widest contiguous gap. Each
 * mapping is placed with mach_vm_allocate + VM_FLAGS_FIXED, which FAILS (rather
 * than clobbers) if a slot is already taken — a dyld-placed framework that
 * happens to sit in the band is simply skipped. */
#define MMAP_BAND_LO   0x10000000UL
#define MMAP_BAND_HI   0x80000000UL
#define MMAP_STEP      (2UL * 1024 * 1024)     /* scan stride when a slot is busy */
#define PAGE_SZ        0x1000UL

/* Live mapping table (for munmap). Small: the buddy/system allocators these
 * targets use grow the heap in a bounded number of large chunks. Overflow just
 * means munmap of an untracked (leaked) region is ignored — leak-over-corrupt,
 * matching malloc_shim's free() policy for non-owned pointers. */
#define MAX_MAPS 4096
struct map_rec { uint64_t base; uint64_t len; };

struct mmap_ctrl {
   uint64_t       magic;
   os_unfair_lock lock;
   uint64_t       next_hint;      /* rolling placement cursor within the band */
   int            nmaps;
   struct map_rec maps[MAX_MAPS];
};

#define MMAP_CTRL_ENV   "ABICONV_MMAP_CTRL"
#define MMAP_CTRL_MAGIC 0x38365836344d4d50ULL  /* "86X64MMP" */

static struct mmap_ctrl *g_mc = NULL;
static os_unfair_lock g_init_lock = OS_UNFAIR_LOCK_INIT;

/* Real mmap/munmap, resolved past our own definitions. */
static void *(*real_mmap)(void *, size_t, int, int, int, off_t) = NULL;
static int   (*real_munmap)(void *, size_t) = NULL;

static void resolve_real(void) {
   if (!real_mmap)  real_mmap  = dlsym(RTLD_NEXT, "mmap");
   if (!real_munmap) real_munmap = dlsym(RTLD_NEXT, "munmap");
}

/* Attach to (or create) the process-wide control block. Shared across the many
 * co-located libabiconv copies exactly like malloc_shim's heap_ctrl: the band
 * is a single resource, so all copies must place through one cursor/table.
 * Reserved with mach_vm_allocate (never malloc) to avoid any allocator cycle. */
static void mmap_ctrl_init(void) {
   if (g_mc) { return; }
   os_unfair_lock_lock(&g_init_lock);
   if (g_mc) { os_unfair_lock_unlock(&g_init_lock); return; }
   const char *e = getenv(MMAP_CTRL_ENV);
   if (e && *e) {
      struct mmap_ctrl *c = (struct mmap_ctrl *)(uintptr_t)strtoull(e, NULL, 16);
      volatile uint64_t *mp = &c->magic;
      for (int i = 0; i < 1000000 && *mp != MMAP_CTRL_MAGIC; ++i) { }
      if (*mp == MMAP_CTRL_MAGIC) {
         g_mc = c;
         os_unfair_lock_unlock(&g_init_lock);
         return;
      }
   }
   mach_vm_address_t addr = 0;
   if (mach_vm_allocate(mach_task_self(), &addr, sizeof(struct mmap_ctrl),
                        VM_FLAGS_ANYWHERE) != KERN_SUCCESS) {
      os_unfair_lock_unlock(&g_init_lock);
      return;
   }
   struct mmap_ctrl *c = (struct mmap_ctrl *)(uintptr_t)addr;
   c->lock = OS_UNFAIR_LOCK_INIT;
   c->next_hint = MMAP_BAND_LO;
   __sync_synchronize();
   c->magic = MMAP_CTRL_MAGIC;
   g_mc = c;
   char buf[32];
   snprintf(buf, sizeof buf, "0x%llx", (unsigned long long)(uintptr_t)c);
   setenv(MMAP_CTRL_ENV, buf, 1);
   os_unfair_lock_unlock(&g_init_lock);
}

/* Reserve `len` (page-rounded) somewhere in the low band. Caller holds lock. */
static void *band_reserve(uint64_t len) {
   uint64_t start = g_mc->next_hint;
   if (start < MMAP_BAND_LO) { start = MMAP_BAND_LO; }
   /* Two sweeps: from the rolling hint to the top, then from the band base up
    * to the hint, so freed holes below the cursor are reused. */
   for (int sweep = 0; sweep < 2; ++sweep) {
      uint64_t a  = (sweep == 0) ? start : MMAP_BAND_LO;
      uint64_t hi = (sweep == 0) ? MMAP_BAND_HI : start;
      for (; a + len <= hi; a += MMAP_STEP) {
         mach_vm_address_t addr = a;
         if (mach_vm_allocate(mach_task_self(), &addr, len,
                              VM_FLAGS_FIXED) == KERN_SUCCESS) {
            g_mc->next_hint = (uint64_t)addr + len;
            return (void *)(uintptr_t)addr;
         }
      }
   }
   return NULL;
}

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset) {
   resolve_real();

   /* Fall through to the real mmap for every case our low-4GB anonymous band
    * cannot / must not serve:
    *   - file-backed (fd >= 0): the real file mapping is required
    *   - MAP_JIT: genuine W^X JIT memory (libabiconv's own init-stub allocator)
    *   - MAP_FIXED at a specific address: honor the caller's placement
    * NOTE: a plain PROT_EXEC on an ANONYMOUS mapping is NOT a passthrough
    * trigger — a Win32-compat VirtualAlloc maps its DATA heap as
    * PAGE_EXECUTE_READWRITE (prot==7) even though it never executes there, and
    * that is precisely the Civ IV case we must serve low-4GB. Executable intent
    * is signalled by MAP_JIT, which we do pass through. A NULL real_mmap (dlsym
    * failed) is unrecoverable for the passthrough cases; return MAP_FAILED
    * rather than pretend. */
   int anon = (flags & MAP_ANON) != 0;
   if (!anon || fd >= 0 || (flags & MAP_JIT) ||
       ((flags & MAP_FIXED) && addr != NULL)) {
      if (!real_mmap) { errno = ENOMEM; return MAP_FAILED; }
      return real_mmap(addr, len, prot, flags, fd, offset);
   }

   if (len == 0) { errno = EINVAL; return MAP_FAILED; }
   uint64_t rlen = (len + PAGE_SZ - 1) & ~(PAGE_SZ - 1);

   mmap_ctrl_init();
   if (!g_mc) {
      /* Cannot track — but still keep the program alive with real memory rather
       * than a masked failure. It will be high-4GB (may truncate), but that is
       * strictly better than the -1-wrapped-to-handle corruption. */
      if (!real_mmap) { errno = ENOMEM; return MAP_FAILED; }
      if (!(flags & (MAP_PRIVATE | MAP_SHARED))) { flags |= MAP_PRIVATE; }
      return real_mmap(addr, len, prot, flags, fd, offset);
   }

   os_unfair_lock_lock(&g_mc->lock);
   void *p = band_reserve(rlen);
   if (p && g_mc->nmaps < MAX_MAPS) {
      g_mc->maps[g_mc->nmaps].base = (uint64_t)(uintptr_t)p;
      g_mc->maps[g_mc->nmaps].len  = rlen;
      g_mc->nmaps++;
   }
   os_unfair_lock_unlock(&g_mc->lock);

   if (!p) { errno = ENOMEM; return MAP_FAILED; }

   /* mach_vm_allocate maps RW by default. Best-effort apply the caller's
    * protection when it asks for something other than plain RW (e.g. a
    * PAGE_EXECUTE_READWRITE VirtualAlloc -> prot==7). Failure is non-fatal and
    * expected under the hardened runtime for RWX without MAP_JIT: the region
    * stays RW, which is all a data heap needs (the case we serve). */
   if (prot != 0 && prot != (PROT_READ | PROT_WRITE)) {
      (void)mprotect(p, rlen, prot);
   }
   return p;
}

int munmap(void *addr, size_t len) {
   resolve_real();
   mmap_ctrl_init();
   uint64_t a = (uint64_t)(uintptr_t)addr;

   if (g_mc) {
      os_unfair_lock_lock(&g_mc->lock);
      for (int i = 0; i < g_mc->nmaps; ++i) {
         if (g_mc->maps[i].base == a) {
            uint64_t rlen = g_mc->maps[i].len;
            /* remove (swap with last) */
            g_mc->maps[i] = g_mc->maps[--g_mc->nmaps];
            os_unfair_lock_unlock(&g_mc->lock);
            mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)a,
                               (mach_vm_size_t)rlen);
            return 0;
         }
      }
      os_unfair_lock_unlock(&g_mc->lock);
   }
   /* Not one of ours — defer to the real munmap (native/JIT/file mappings). */
   if (real_munmap) { return real_munmap(addr, len); }
   return 0;
}
