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
// MTSHIM64 (halo_tramp.asm) is the same but returns the C uint64_t in i386 edx:eax (high:low).

#include <stdint.h>

// Native libsystem_kernel entry; fills the 8-byte {numer,denom} struct at *info and returns
// kern_return_t (0 = KERN_SUCCESS). Declared locally to keep the file header-light; the call
// resolves to the native _mach_timebase_info (our export is the 3-underscore form).
extern int mach_timebase_info(void *info);

uint32_t shim_mach_timebase_info(uint32_t *args) {
    void *info = (void *)(uintptr_t)args[0];   // i386 low-4GB pointer to the out struct
    return (uint32_t)mach_timebase_info(info);
}

// ---- mach_absolute_time(void) -> uint64_t ----
// The 2nd Halo init wall (after mach_timebase_info): a present, non-removed libSystem symbol
// abigen never declared (mach/mach_time.h is not in includes.h), so it bound DIRECT-to-native.
// It takes no args, but the native x86_64 entry's 8-byte `ret` over-pops the translated i386
// 4-byte cdecl return frame -> the popped return addr fuses with an adjacent i386-stack qword
// into a bogus PC (lldb: pc=0xffffbc..0103e843, r11=mach_absolute_time). The MTSHIM64
// trampoline alone fixes the crash (it unwinds the i386 4-byte frame correctly); returning via
// MTSHIM64 (edx:eax) also keeps the FULL 64-bit monotonic counter intact (i386 64-bit return
// convention) instead of truncating the high half to stale edx. Pure passthrough = exact.
extern uint64_t mach_absolute_time(void);

uint64_t shim_mach_absolute_time(uint32_t *args) {
    (void)args;                       // no i386 arguments
    return mach_absolute_time();
}
