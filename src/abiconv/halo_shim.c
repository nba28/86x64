// halo_shim.c — single-purpose i386→x86_64 ABI-conversion shims for native libSystem
// functions Halo (MacSoft 2.0.4) calls during early init that abigen's consider set never
// covered (ABICONV_LEGACY_TARGETS lists only Civ IV), so they bind DIRECTLY to the native
// x86_64 implementation and the i386-cdecl call mismatches the System V ABI.
//
// ---- mach_timebase_info(mach_timebase_info_t info) ----
// The native x86_64 entry reads its single out-struct pointer from %rdi, but the translated
// i386 caller passes it as a 4-byte cdecl STACK slot — so native %rdi is 0 and the function's
// `movq %xmm0,(%rbx)` (rbx := rdi) writes the timebase to address 0x0: EXC_BAD_ACCESS at 0x0,
// faulting at mach_timebase_info+62. Confirmed live under lldb during slide_objc's RUN_INITS
// pass (the keystone already loaded Halo past all 52 removed-Carbon walls — no SIGILL fired).
// This shim marshals the i386 frame: args[0] = the low-4GB pointer to the out struct, then
// calls the native entry with the System V ABI. mach_timebase_info_data_t = {uint32_t numer;
// uint32_t denom} is an identical 8-byte layout on i386 and x86_64, so the passthrough is exact
// (a smart/real fix, not a stub).
//
// NOTE (maintainer): this is a UNIVERSAL libSystem gap, not Halo-specific — any i386 target
// calling mach_timebase_info hits it. The clean root cure is abigen coverage (add Halo, or the
// symbol, to the consider set). Kept here as a single-purpose hand shim to unblock Halo without
// touching the shared abigen/CarbonShim lanes.
//
// MTSHIM convention (halo_tramp.asm): rdi -> &i386 args[0] (4-byte cdecl slots); uint32_t in eax.

#include <stdint.h>

// Native libsystem_kernel entry; fills the 8-byte {numer,denom} struct at *info and returns
// kern_return_t (0 = KERN_SUCCESS). Declared locally to keep the file header-light; the call
// resolves to the native _mach_timebase_info (our export is the 3-underscore form).
extern int mach_timebase_info(void *info);

uint32_t shim_mach_timebase_info(uint32_t *args) {
    void *info = (void *)(uintptr_t)args[0];   // i386 low-4GB pointer to the out struct
    return (uint32_t)mach_timebase_info(info);
}
