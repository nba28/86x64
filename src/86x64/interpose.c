#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <os/lock.h>
#include <mach/mach_init.h>
#include <mach/vm_map.h>
#include <mach/vm_param.h>
#include <mach/mach_vm.h>

/*
 * The original interpose just *hinted* a low address to mmap. macOS 11+
 * ignores mmap hints (without MAP_FIXED) under ASLR and routinely returns
 * addresses far above 4 GB. The wrapper's stack-pointer setup truncates
 * rsp to 32 bits, so a high mmap result lands rsp on unmapped memory and
 * the program faults before even reaching _main.
 *
 * Fix: when the caller passes an anywhere-mapping (addr==0 / VM_FLAGS_ANYWHERE)
 * we walk the low 4 GB looking for a free hole and pin the allocation there
 * with MAP_FIXED / VM_FLAGS_FIXED. This guarantees the result is
 * 32-bit-truncation-safe.
 *
 * THREAD SAFETY (critical): these interposers replace mmap/vm_* PROCESS-WIDE,
 * so they run on behalf of real system libraries too (libdispatch, libxpc,
 * libmalloc), which allocate concurrently from many worker threads. The
 * earlier version mutated `map_base` with no lock and used FIXED with no
 * collision retry, so racing allocations clobbered each other and produced
 * corrupted dispatch/XPC state (a livelock spinning in
 * __DISPATCH_WAIT_FOR_ENQUEUER__ during a synchronous CFPreferences XPC
 * round-trip). Everything that touches `map_base` now runs under
 * `map_lock`, and every placement probes for a free slot and retries on
 * collision. os_unfair_lock is safe this early (libplatform is up before
 * any image initializer) and does not busy-burn under contention.
 */
#define LOW_REGION_BASE 0x080000000UL  /* 2 GB - start probing here */
#define LOW_REGION_END  0x0F0000000UL  /* below kernel-reserved zones */

static os_unfair_lock map_lock = OS_UNFAIR_LOCK_INIT;
static uintptr_t map_base = LOW_REGION_BASE;

/*
 * Only OUR code's anywhere-allocations need to be pinned low-4GB (it stores
 * results in 32-bit-truncated pointers). The wrapper exec, the translated
 * dylibs, and libabiconv are ALL mapped below 4 GB (that is the whole point);
 * real system frameworks live high in the dyld shared cache (~0x7ff8........).
 * So the caller's return address cleanly says who is asking: force low only
 * when a low-mapped (our) caller asks. Forcing real frameworks' allocations
 * low is unnecessary AND harmful — it crams libdispatch/libxpc/libmalloc
 * (which run pure 64-bit code) into our narrow window, where their worker
 * thread stacks and message buffers contend/collide and wedge a synchronous
 * XPC round-trip in __DISPATCH_WAIT_FOR_ENQUEUER__. Must be evaluated with
 * __builtin_return_address(0) INSIDE each interposer (a helper would just see
 * the interposer itself, which is always low).
 */
#define CALLER_IS_LOW() \
	((uintptr_t)__builtin_return_address(0) < 0x100000000UL)

#ifndef round_page
__attribute__((__always_inline__)) static uintptr_t round_page(uintptr_t x)
{
	return ((x + PAGE_SIZE - 1UL) & ~(PAGE_SIZE - 1UL));
}
#endif

/* Advance map_base past a just-claimed [addr, addr+len) region (leaving a
 * one-page guard gap). Caller holds map_lock. */
static void advance_map_base(uintptr_t addr, size_t aligned_len)
{
	map_base = round_page(addr + aligned_len + PAGE_SIZE);
}

/*
 * mmap an anywhere-mapping into the low 4 GB. MAP_FIXED silently OVERWRITES
 * whatever is already mapped at the target, so we cannot probe by mmap alone:
 * we first reserve a candidate slot with mach_vm_allocate (which fails
 * cleanly if occupied), then mmap MAP_FIXED over the reservation.
 */
static void *
mmap_into_low_4gb(size_t len, int prot, int flags, int fd, off_t offset)
{
	const size_t aligned_len = round_page(len);
	os_unfair_lock_lock(&map_lock);
	for (uintptr_t a = map_base; a + aligned_len <= LOW_REGION_END;
	     a += round_page(aligned_len + PAGE_SIZE))
	{
		mach_vm_address_t addr = a;
		kern_return_t kr = mach_vm_allocate(mach_task_self(),
		                                    &addr, aligned_len,
		                                    VM_FLAGS_FIXED);
		if (kr != KERN_SUCCESS) continue;   /* occupied — probe next */
		/* Reserved. Replace with the user's actual mapping. */
		void *rv = mmap((void *)(uintptr_t)addr, len, prot,
		                flags | MAP_FIXED, fd, offset);
		if (rv == MAP_FAILED) {
			mach_vm_deallocate(mach_task_self(), addr, aligned_len);
			continue;
		}
		advance_map_base((uintptr_t)rv, aligned_len);
		os_unfair_lock_unlock(&map_lock);
		return rv;
	}
	os_unfair_lock_unlock(&map_lock);
	errno = ENOMEM;
	return MAP_FAILED;
}

static void*
__mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
	if (CALLER_IS_LOW() && addr == NULL && !(flags & MAP_FIXED)) {
		void *rv = mmap_into_low_4gb(len, prot, flags, fd, offset);
		if (rv != MAP_FAILED) return rv;
		/* fall through and try the standard call so the caller sees
		 * a "real" mmap failure rather than our bookkeeping one. */
	}
	return mmap(addr, len, prot, flags, fd, offset);
}

static kern_return_t
__vm_allocate(vm_map_t task, vm_address_t *addr, vm_size_t size, int flags)
{
	if (CALLER_IS_LOW() && (flags & VM_FLAGS_ANYWHERE) && task == mach_task_self()) {
		const size_t aligned_len = round_page(size);
		const int fixed = flags & ~VM_FLAGS_ANYWHERE;
		os_unfair_lock_lock(&map_lock);
		for (uintptr_t a = map_base; a + aligned_len <= LOW_REGION_END;
		     a += round_page(aligned_len + PAGE_SIZE))
		{
			vm_address_t cand = a;
			kern_return_t kr = vm_allocate(task, &cand, size, fixed);
			if (kr != KERN_SUCCESS) continue;   /* occupied — probe next */
			advance_map_base((uintptr_t)cand, aligned_len);
			os_unfair_lock_unlock(&map_lock);
			*addr = cand;
			return KERN_SUCCESS;
		}
		os_unfair_lock_unlock(&map_lock);
		/* low window exhausted — let the real call place it anywhere */
	}
	return vm_allocate(task, addr, size, flags);
}

static kern_return_t
__mach_vm_allocate(vm_map_t task, mach_vm_address_t *addr, vm_size_t size, int flags)
{
	if (CALLER_IS_LOW() && (flags & VM_FLAGS_ANYWHERE) && task == mach_task_self()) {
		const size_t aligned_len = round_page(size);
		const int fixed = flags & ~VM_FLAGS_ANYWHERE;
		os_unfair_lock_lock(&map_lock);
		for (uintptr_t a = map_base; a + aligned_len <= LOW_REGION_END;
		     a += round_page(aligned_len + PAGE_SIZE))
		{
			mach_vm_address_t cand = a;
			kern_return_t kr = mach_vm_allocate(task, &cand, size, fixed);
			if (kr != KERN_SUCCESS) continue;
			advance_map_base((uintptr_t)cand, aligned_len);
			os_unfair_lock_unlock(&map_lock);
			*addr = cand;
			return KERN_SUCCESS;
		}
		os_unfair_lock_unlock(&map_lock);
	}
	return mach_vm_allocate(task, addr, size, flags);
}

static kern_return_t
__vm_map(vm_map_t task, vm_address_t *addr, vm_size_t size, vm_offset_t mask,
	int flags, mem_entry_name_port_t object, memory_object_offset_t offset, boolean_t copy,
	vm_prot_t cur_protection, vm_prot_t max_protection, vm_inherit_t inheritance)
{
	if (CALLER_IS_LOW() && (flags & VM_FLAGS_ANYWHERE) && task == mach_task_self()) {
		const size_t aligned_len = round_page(size);
		const int fixed = flags & ~VM_FLAGS_ANYWHERE;
		os_unfair_lock_lock(&map_lock);
		for (uintptr_t a = map_base; a + aligned_len <= LOW_REGION_END;
		     a += round_page(aligned_len + PAGE_SIZE))
		{
			vm_address_t cand = a;
			kern_return_t kr = vm_map(task, &cand, size, mask, fixed,
				object, offset, copy, cur_protection, max_protection,
				inheritance);
			if (kr != KERN_SUCCESS) continue;
			advance_map_base((uintptr_t)cand, aligned_len);
			os_unfair_lock_unlock(&map_lock);
			*addr = cand;
			return KERN_SUCCESS;
		}
		os_unfair_lock_unlock(&map_lock);
	}
	return vm_map(task, addr, size, mask, flags, object, offset, copy,
		cur_protection, max_protection, inheritance);
}

static kern_return_t
__mach_vm_map(vm_map_t task, mach_vm_address_t *addr, vm_size_t size, mach_vm_offset_t mask,
	int flags, mem_entry_name_port_t object, memory_object_offset_t offset, boolean_t copy,
	vm_prot_t cur_protection, vm_prot_t max_protection, vm_inherit_t inheritance)
{
	if (CALLER_IS_LOW() && (flags & VM_FLAGS_ANYWHERE) && task == mach_task_self()) {
		const size_t aligned_len = round_page(size);
		const int fixed = flags & ~VM_FLAGS_ANYWHERE;
		os_unfair_lock_lock(&map_lock);
		for (uintptr_t a = map_base; a + aligned_len <= LOW_REGION_END;
		     a += round_page(aligned_len + PAGE_SIZE))
		{
			mach_vm_address_t cand = a;
			kern_return_t kr = mach_vm_map(task, &cand, size, mask, fixed,
				object, offset, copy, cur_protection, max_protection,
				inheritance);
			if (kr != KERN_SUCCESS) continue;
			advance_map_base((uintptr_t)cand, aligned_len);
			os_unfair_lock_unlock(&map_lock);
			*addr = cand;
			return KERN_SUCCESS;
		}
		os_unfair_lock_unlock(&map_lock);
	}
	return mach_vm_map(task, addr, size, mask, flags, object, offset, copy,
		cur_protection, max_protection, inheritance);
}

typedef struct { const void* replacement; const void* replacee; } interpose_t;

__attribute__((used)) static const interpose_t __interposers[]
__attribute__ ((section("__DATA, __interpose"))) = {
	{ (void *)__mmap,             (void *)mmap },
	{ (void *)__vm_allocate,      (void *)vm_allocate },
	{ (void *)__vm_map,           (void *)vm_map },
	{ (void *)__mach_vm_allocate, (void *)mach_vm_allocate },
	{ (void *)__mach_vm_map,      (void *)mach_vm_map },
};
