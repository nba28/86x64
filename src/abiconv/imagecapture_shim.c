// imagecapture_shim.c — graceful shims for the legacy ImageCapture (ICA) Carbon C API.
//
// The ICA host API (ICAGetDeviceList / ICAGetNthChild / ICACopyObjectThumbnail / ...) is a
// dead Carbon C API on modern macOS: Apple emptied its public header (ImageCapture.h is now
// just an include guard) and replaced it with the ImageCaptureCore ObjC framework, though the
// raw C symbols survive in Carbon's ImageCapture sub-framework for binary compatibility.
//
// Two reasons we must shim these rather than let the bind stay on native Carbon:
//   1. STRUCTURAL (the reason it crashes): each ICA function is reached from translated
//      i386-cdecl code through a bare `ff 25` indirect stub with no ABI bridge. The native
//      x86_64 callee's `ret` pops 8 bytes while the i386 caller pushed only a 4-byte return
//      address, so the `ret` fuses the return address with the adjacent stack word into a
//      garbage program counter (a "fused-PC" SIGSEGV). This is the same class as the
//      unshimmed sqlite3_open / pthread / OSAtomic crashes — a native callee over-popping
//      the 4-byte i386 frame.
//   2. SEMANTIC: we do not support live camera/scanner capture. iPhoto enumerates connected
//      devices through this API during library open (-[ArchiveDocument startDeviceManagers]).
//
// Every ICA host entry point has the same shape:
//      ICAError f(SomePB *pb, ICACompletion completion);   // 2 pointer args, 4-byte OSStatus
// We ignore both args and return a benign "no device" error. The caller records the error and
// proceeds down its graceful device-enumeration-failed path (no devices shown), exactly as it
// would on a Mac with no camera attached.
//
// Wired through the MTSHIM trampoline (maptable_tramp.asm): on entry rdi -> &i386 args[0]
// (each arg a 4-byte dword), the C impl returns the i386 return value in eax.

#include <stdint.h>

// kICADeviceNotFoundErr lives in the old ImageCapture error space (negative OSStatus). Any
// non-zero value sends the caller down its "device enumeration failed" branch; we use the
// canonical device-not-found code so a trace reads sensibly.
#define ICA_DEVICE_NOT_FOUND (-9920)

static inline int ica_no_device(void) { return ICA_DEVICE_NOT_FOUND; }

int shim_ICAGetDeviceList(uint32_t *args)                 { (void)args; return ica_no_device(); }
int shim_ICAGetChildCount(uint32_t *args)                 { (void)args; return ica_no_device(); }
int shim_ICAGetNthChild(uint32_t *args)                   { (void)args; return ica_no_device(); }
int shim_ICAGetPropertyByType(uint32_t *args)             { (void)args; return ica_no_device(); }
int shim_ICAGetPropertyData(uint32_t *args)               { (void)args; return ica_no_device(); }
int shim_ICACopyObjectPropertyDictionary(uint32_t *args)  { (void)args; return ica_no_device(); }
int shim_ICACopyObjectThumbnail(uint32_t *args)           { (void)args; return ica_no_device(); }
int shim_ICADownloadFile(uint32_t *args)                  { (void)args; return ica_no_device(); }
int shim_ICAObjectSendMessage(uint32_t *args)             { (void)args; return ica_no_device(); }
int shim_ICARegisterEventNotification(uint32_t *args)     { (void)args; return ica_no_device(); }
