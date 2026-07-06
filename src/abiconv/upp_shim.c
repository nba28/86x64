// upp_shim.c — Universal Procedure Pointer New/Dispose shims (the CORRECT modern impl).
//
// On Carbon for OS X, a UPP is just the procedure pointer itself: NewXxxUPP(proc) is defined
// to return proc unchanged, and DisposeXxxUPP(upp) frees nothing. These wrapper symbols were
// dropped from 64-bit/modern macOS, but their semantics are trivial and exact — so this is a
// real identity/no-op implementation, not a degraded stub. Returning the passed-in proc
// pointer (a 32-bit low-4GB address into translated code) keeps any later invocation routing
// through the same callback path the original i386 code expected.
//
// MTSHIM convention: rdi -> &i386 args[0]; return the (32-bit) proc pointer in eax.

#include <stdint.h>

// New<Kind>UPP(procPtr) -> procPtr   (identity)
uint32_t shim_NewControlUserPaneDrawUPP(uint32_t *args)     { return args[0]; }
uint32_t shim_NewControlUserPaneHitTestUPP(uint32_t *args)  { return args[0]; }
uint32_t shim_NewControlUserPaneTrackingUPP(uint32_t *args) { return args[0]; }
uint32_t shim_NewEventHandlerUPP(uint32_t *args)            { return args[0]; }
uint32_t shim_NewEventLoopTimerUPP(uint32_t *args)          { return args[0]; }
uint32_t shim_NewSndCallBackUPP(uint32_t *args)             { return args[0]; }
uint32_t shim_NewControlActionUPP(uint32_t *args)          { return args[0]; }

// Dispose<Kind>UPP(upp) -> nothing to free
void shim_DisposeControlUserPaneDrawUPP(uint32_t *args)     { (void)args; }
void shim_DisposeControlActionUPP(uint32_t *args)          { (void)args; }
void shim_DisposeEventLoopTimerUPP(uint32_t *args)         { (void)args; }
void shim_DisposeControlUserPaneHitTestUPP(uint32_t *args)  { (void)args; }
void shim_DisposeControlUserPaneTrackingUPP(uint32_t *args) { (void)args; }
void shim_DisposeEventHandlerUPP(uint32_t *args)            { (void)args; }
void shim_DisposeSndCallBackUPP(uint32_t *args)            { (void)args; }
