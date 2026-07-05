// rm_shim.c — classic Resource Manager HANDLE marshalling (Civ IV / Halo s35).
//
// The Resource Manager (Get1Resource / GetResource / ...) lives in modern
// CarbonCore, so abigen forwards to it — but a Resource Manager HANDLE is a
// native `Ptr*` living at a >4GB address, and abigen's return marshalling
// truncates it into the i386 caller's 4-byte Handle slot. The translated code
// then does `master = *Handle; data = *master` on a garbage low-4GB address and
// faults (Civ IV / Halo s35: QuickTime HereIsWarholResFile -> Get1Resource('vers')
// -> `movl (%rdi),%eax; movl (%rax),%ecx` on the truncated Handle). Symmetrically,
// abigen passes the translated program's low Handle straight to a native
// Handle-TAKING RM call, which then derefs a low-4GB address as its master ptr.
//
// Fix: hand-shim the Handle-boundary RM entries so a Handle round-trips through
// a LOW-4GB arena Handle (the carbon_memory.c model: a 4-byte master pointer ->
// a low-4GB block). A Handle-RETURNING call forwards to native, then WRAPS the
// native Handle into a fresh low Handle holding a COPY of the resource bytes
// (so the translated `**Handle` deref reads real low-4GB data), recording
// low<->native. A Handle-TAKING call UNWRAPS the low Handle back to its native
// Handle before forwarding. Release/Detach drop the mapping (+ free the low copy
// on release). Memory-Manager ops (GetHandleSize/HLock/HUnlock/DisposeHandle)
// already operate on these low Handles via carbon_memory.c — the wrapped Handle
// IS a carbon_memory Handle, so they compose for free.
//
// These entries are otherwise abigen-generated; custom.syms lists them so abigen
// does NOT also emit the truncating version. Wired ___<Name> -> _shim_<Name> via
// maptable_tramp.asm. UNIVERSAL: any translated i386 Carbon app that reads
// resources through the Resource Manager benefits (not QuickTime-specific).

#include "carbon_shim.h"          // cm_new_handle/_block/_size/_dispose, i386_ptr, to_i386
#include <dlfcn.h>
#include <string.h>
#include <stdint.h>
#include <os/lock.h>

static void *rm_nsym(const char *n) { return dlsym(RTLD_DEFAULT, n); }
// Native RM/MM function-pointer types (typedefs so a cached static can carry
// the type without the illegal `T(*)(...) name` spelling).
typedef void   *(*rm_get2_t)(uint32_t, int16_t);   // Get1Resource(ResType, short)
typedef long    (*rm_size_t)(void *);              // GetHandleSize / SizeResource
typedef void    (*rm_void1_t)(void *);             // LoadResource/Release/Detach
typedef int16_t (*rm_s16_1_t)(void *);             // GetResAttrs / HomeResFile
typedef void    (*rm_resinfo_t)(void *, void *, void *, void *);
// Resolve a native RM symbol once into a typed cached pointer.
#define RM_N(var, ty, name) \
   static ty var = 0; if (!var) { var = (ty)rm_nsym(name); }

// low-4GB Handle  <->  native Handle map (resources are few; a small linear
// table under an unfair lock suffices).
#define RM_MAP_MAX 8192
static struct { uint32_t low; void *nat; } g_rm[RM_MAP_MAX];
static uint32_t       g_rmn;
static os_unfair_lock g_rmlk = OS_UNFAIR_LOCK_INIT;

static void rm_record(uint32_t low, void *nat) {
   if (!low) return;
   os_unfair_lock_lock(&g_rmlk);
   for (uint32_t i = 0; i < g_rmn; i++) {
      if (g_rm[i].low == low) { g_rm[i].nat = nat; os_unfair_lock_unlock(&g_rmlk); return; }
   }
   if (g_rmn < RM_MAP_MAX) { g_rm[g_rmn].low = low; g_rm[g_rmn].nat = nat; g_rmn++; }
   os_unfair_lock_unlock(&g_rmlk);
}
static void *rm_native(uint32_t low) {
   void *r = 0;
   os_unfair_lock_lock(&g_rmlk);
   for (uint32_t i = 0; i < g_rmn; i++) { if (g_rm[i].low == low) { r = g_rm[i].nat; break; } }
   os_unfair_lock_unlock(&g_rmlk);
   return r;
}
static void rm_forget(uint32_t low) {
   os_unfair_lock_lock(&g_rmlk);
   for (uint32_t i = 0; i < g_rmn; i++) {
      if (g_rm[i].low == low) { g_rm[i] = g_rm[--g_rmn]; break; }
   }
   os_unfair_lock_unlock(&g_rmlk);
}

// Wrap a native Handle into a low-4GB Handle holding a copy of the resource
// bytes. A NULL native Handle (resource not found) -> 0, as the caller expects.
// DEFENSIVE: only copy `size` bytes when *nH and size are self-consistent — a
// bogus/huge GetHandleSize or a NULL master (purged/unloaded resource) must NOT
// drive a wild memcpy that corrupts the low-4GB heap (which surfaces later as
// non-deterministic faults in unrelated code — the app's own allocator, a
// foreign abigen shim). An implausible size yields an empty but valid, recorded
// low Handle so the caller can still LoadResource / release it.
#define RM_MAX_RES (64u * 1024u * 1024u)         // 64MB sanity cap on one resource
static uint32_t rm_wrap(void *nH) {
   if (!nH) return 0;
   RM_N(getsize, rm_size_t, "GetHandleSize");
   long size = getsize ? getsize(nH) : 0;
   void *master = *(void **)nH;                 // *Handle = master ptr = data
   uint32_t nbytes = (size > 0 && (uint64_t)size <= RM_MAX_RES && master)
                     ? (uint32_t)size : 0;
   uint32_t low = cm_new_handle(nbytes, 0);
   if (low && nbytes) { memcpy(cm_handle_block(low), master, nbytes); }
   rm_record(low, nH);
   return low;
}
static void *rm_unwrap(uint32_t low) {          // low Handle -> native Handle
   void *n = rm_native(low);
   return n ? n : i386_ptr(low);                // unrecorded: pass through as-is
}

// ---- Handle-RETURNING: forward + wrap ------------------------------------
// Handle Get1Resource(ResType, short); GetResource(ResType, short);
// Get1IndResource(ResType, short index); GetIndResource(ResType, short index).
#define RM_GET2(NAME)                                                        \
   uint32_t shim_##NAME(uint32_t *a) {                                       \
      RM_N(fn, rm_get2_t, #NAME);                         \
      if (!fn) { return 0; }                                                 \
      return rm_wrap(fn(a[0], (int16_t)a[1]));                               \
   }
RM_GET2(Get1Resource)
RM_GET2(GetResource)
RM_GET2(Get1IndResource)
RM_GET2(GetIndResource)

// ---- Handle-TAKING: unwrap + forward -------------------------------------
// void LoadResource(Handle) — reloads a purged resource; our low copy already
// holds the data, so refresh it from the (re)loaded native Handle.
uint32_t shim_LoadResource(uint32_t *a) {
   RM_N(fn, rm_void1_t, "LoadResource");
   void *nH = rm_native(a[0]);
   if (fn && nH) {
      fn(nH);
      RM_N(getsize, rm_size_t, "GetHandleSize");
      long size = getsize ? getsize(nH) : 0;
      void *master = *(void **)nH;
      if (master && size > 0 && (uint32_t)size <= cm_handle_size(a[0])) {
         memcpy(cm_handle_block(a[0]), master, (size_t)size);
      }
   }
   return 0;
}
// long SizeResource(Handle) — size on disk; report our copy's logical size.
uint32_t shim_SizeResource(uint32_t *a) {
   RM_N(fn, rm_size_t, "SizeResource");
   void *nH = rm_native(a[0]);
   if (fn && nH) { return (uint32_t)fn(nH); }
   return cm_handle_size(a[0]);
}
// short GetResAttrs(Handle)
uint32_t shim_GetResAttrs(uint32_t *a) {
   RM_N(fn, rm_s16_1_t, "GetResAttrs");
   void *nH = rm_native(a[0]);
   return (fn && nH) ? (uint32_t)(uint16_t)fn(nH) : 0;
}
// short HomeResFile(Handle)
uint32_t shim_HomeResFile(uint32_t *a) {
   RM_N(fn, rm_s16_1_t, "HomeResFile");
   void *nH = rm_native(a[0]);
   return (fn && nH) ? (uint32_t)(int32_t)(int16_t)fn(nH) : (uint32_t)-1;
}
// void GetResInfo(Handle, short* theID, ResType* theType, Str255 name);
// out-params are low-4GB pointers native writes through directly.
uint32_t shim_GetResInfo(uint32_t *a) {
   RM_N(fn, rm_resinfo_t, "GetResInfo");
   void *nH = rm_native(a[0]);
   if (fn && nH) { fn(nH, i386_ptr(a[1]), i386_ptr(a[2]), i386_ptr(a[3])); }
   return 0;
}
// void DetachResource(Handle) — the resource Handle survives its res file;
// forward to native, drop the mapping but KEEP the low copy (now owned by the
// caller as a standalone Handle).
uint32_t shim_DetachResource(uint32_t *a) {
   RM_N(fn, rm_void1_t, "DetachResource");
   void *nH = rm_native(a[0]);
   if (fn && nH) { fn(nH); }
   rm_forget(a[0]);
   return 0;
}
// void ReleaseResource(Handle) — release + free the low copy.
uint32_t shim_ReleaseResource(uint32_t *a) {
   RM_N(fn, rm_void1_t, "ReleaseResource");
   void *nH = rm_native(a[0]);
   if (fn && nH) { fn(nH); }
   rm_forget(a[0]);
   cm_dispose_handle(a[0]);
   return 0;
}
