// keymgr_shim.c — i386->x86_64 ABI bridge for the legacy keymgr process-wide
// pointer store + DWARF2 EH-section registration that classic i386 C++ binaries
// (the libgcc/libstdc++ static EH runtime) call from their static initializers.
//
// WHY THIS EXISTS — an ABI trap, NOT a removed symbol:
// The _keymgr_* family IS still present in modern macOS (libkeymgr.dylib), so the
// bind SUCCEEDS and static-interpose leaves it NATIVE (it only redirects symbols
// for which libabiconv exports a __-prefixed trampoline). But the caller is
// TRANSLATED i386 code using cdecl (args on the stack), while native x86_64
// keymgr reads its args from rdi/rsi. With no ABI bridge the 2nd argument — the
// `void **result` out-pointer of _keymgr_get_and_lock_processwide_ptr_2 — arrives
// as garbage/NULL, and keymgr stores the keyed pointer through it:
//
//     _keymgr_get_and_lock_processwide_ptr_2+53:  movq %rax,(%rbx)   ; rbx=result
//
// => EXC_BAD_ACCESS deep in libkeymgr during the translated crt's EH-frame
// registration. This was Civ IV's "NULL+0x70 static-init crash": the faulting
// address (0x0 / 0x30 / 0x70 across runs) is just `rbx` = the garbage out-pointer,
// and the .ips register fingerprint (rax=0, rdx=4, r9=4, r11=0x60, rsi=0) matches
// this frame exactly. It is NOT a Carbon/QuickDraw/Sound shim returning NULL.
//
// FIX: give keymgr a libabiconv MTSHIM trampoline (maptable_tramp.asm) so the
// bind is redirected and the i386 cdecl args are marshalled correctly, then
// implement keymgr's documented semantics here rather than forwarding to native
// keymgr. Implementing it ourselves is correct because (a) the i386 stores are
// 32-bit low-4GB pointers the native 64-bit keymgr node table is not laid out
// for, and (b) 86x64 resolves C++ exceptions through its OWN translated-frame
// unwinder + PC-map, so keymgr's DWARF2 FDE object list is vestigial under
// translation (the native __keymgr_dwarf2_register_sections is itself a no-op on
// modern macOS).
//
// MTSHIM convention: rdi -> &i386 args[0] (4-byte cdecl slots); uint32_t in eax.
// Every stored value is an i386 32-bit pointer (low-4GB), kept as a uint32_t.

#include <stdint.h>
#include <pthread.h>

#define ARG(n) (args[(n)])

// keymgr only ever uses a handful of distinct keys (the EH-context / new- /
// terminate-handler keys 1..5 and the GCC3 DWARF2 object-list key, observed key
// 0x400). A tiny linear (key,value) table is ample and works for ANY key value
// (real keymgr keys are sparse, not dense indices). The table is protected
// per-call so its structure stays consistent under concurrent appends; we
// deliberately do NOT hold the lock across the documented get-and-lock /
// set-and-unlock pairing — nothing in 86x64 contends for these process-wide
// slots, so a held lock would only risk a deadlock if a translated error path
// skipped the matching unlock.
#define KEYMGR_MAX_KEYS 64
static pthread_mutex_t g_keymgr_lock = PTHREAD_MUTEX_INITIALIZER;
static struct { uint32_t key; uint32_t val; int used; } g_keymgr[KEYMGR_MAX_KEYS];

static uint32_t keymgr_get(uint32_t key) {
    uint32_t v = 0;
    pthread_mutex_lock(&g_keymgr_lock);
    for (int i = 0; i < KEYMGR_MAX_KEYS; i++) {
        if (g_keymgr[i].used && g_keymgr[i].key == key) { v = g_keymgr[i].val; break; }
    }
    pthread_mutex_unlock(&g_keymgr_lock);
    return v;
}

static void keymgr_set(uint32_t key, uint32_t val) {
    pthread_mutex_lock(&g_keymgr_lock);
    int slot = -1;
    for (int i = 0; i < KEYMGR_MAX_KEYS; i++) {
        if (g_keymgr[i].used && g_keymgr[i].key == key) { slot = i; break; }
        if (slot < 0 && !g_keymgr[i].used) { slot = i; }   // first free slot
    }
    if (slot >= 0) {
        g_keymgr[slot].key = key; g_keymgr[slot].val = val; g_keymgr[slot].used = 1;
    }
    pthread_mutex_unlock(&g_keymgr_lock);
}

// void *_keymgr_get_and_lock_processwide_ptr(unsigned int key)
uint32_t shim_keymgr_get_and_lock_processwide_ptr(uint32_t *args) {
    return keymgr_get(ARG(0));
}

// int _keymgr_get_and_lock_processwide_ptr_2(unsigned int key, void **result)
// Writes the keyed pointer into *result (an i386 32-bit out-pointer) and returns
// 0 (success). Returning 0 — not an error — is required so the caller proceeds to
// read *result (which is now the well-defined keyed value, initially NULL).
uint32_t shim_keymgr_get_and_lock_processwide_ptr_2(uint32_t *args) {
    uint32_t *result = (uint32_t *)(uintptr_t)ARG(1);
    if (result) { *result = keymgr_get(ARG(0)); }
    return 0;
}

// int _keymgr_set_and_unlock_processwide_ptr(unsigned int key, void *ptr)
uint32_t shim_keymgr_set_and_unlock_processwide_ptr(uint32_t *args) {
    keymgr_set(ARG(0), ARG(1));
    return 0;
}

// void __keymgr_dwarf2_register_sections(...) — classic DWARF2 EH-frame section
// registration. A no-op on modern macOS (the native symbol is a 4-instruction
// no-op) and unnecessary under 86x64 (own unwinder/PC-map). No-op.
uint32_t shim_keymgr_dwarf2_register_sections(uint32_t *args) {
    (void)args; return 0;
}
