// carbon_fsspec_shim.c — graceful shims for the dead FSSpec-based Carbon File Manager API.
//
// The classic Mac OS File Manager had two generations of file references:
//   * FSSpec  {volume refNum, directory ID, HFS name} — the original HFS model, and
//   * FSRef   (opaque 80-byte token) — its Carbon-era replacement.
// Apple removed the FSSpec generation entirely on modern macOS (the symbols are gone and
// their declarations were stripped from <CoreServices/.../Files.h>), while the FSRef
// generation survives for binary compatibility. abigen therefore already generates proper
// ABI shims for every FSRef function iPhoto imports (FSGetCatalogInfo, FSOpenFork, FSReadFork,
// FSPathMakeRef, FSMakeFSRefUnicode, ...) from the live header — but it cannot shim the three
// FSSpec functions iPhoto still references, because there is no declaration to read and no
// native symbol to call.
//
// Reached from translated i386-cdecl code through a bare indirect stub, a *missing* symbol's
// lazy pointer stays null, so the stub jumps through 0 -> rip=0 SIGSEGV (e.g. iPhoto's
// -[IP_FilePath pathForFSSpec:] -> FSpMakeFSRef while -[ArchiveDocument startDeviceManagers]
// probes legacy device/PhotoCD volumes). FSSpec semantics (a volume refNum + HFS dir ID) have
// no meaning on a modern volume anyway, so each shim returns a benign File Manager error; the
// caller takes its normal "no such file/volume" path (e.g. pathForFSSpec: returns nil).
//
// Wired through the MTSHIM trampoline (maptable_tramp.asm): rdi -> &i386 args[0], return in eax.

#include <stdint.h>

// Classic File Manager OSErr codes (Files.h / MacErrors.h), stable across all of Mac history.
#define FM_FNF_ERR   (-43)   // fnfErr   — file not found
#define FM_NSV_ERR   (-35)   // nsvErr   — no such volume

// OSErr FSpMakeFSRef(const FSSpec *source, FSRef *newRef);
// FSSpec -> FSRef is impossible without the dead HFS volume model. Report file-not-found;
// the caller treats the FSSpec as unresolvable (e.g. pathForFSSpec: -> nil).
int shim_FSpMakeFSRef(uint32_t *args) { (void)args; return FM_FNF_ERR; }

// OSErr FSMakeFSSpec(SInt16 vRefNum, SInt32 dirID, ConstStr255Param fileName, FSSpec *spec);
// There are no FSSpec volumes to resolve against; report no-such-volume.
int shim_FSMakeFSSpec(uint32_t *args) { (void)args; return FM_NSV_ERR; }

// ResFileRefNum FSpOpenResFile(const FSSpec *spec, SignedByte permission);
// Resource forks addressed by FSSpec are gone; -1 is the canonical "could not open" refNum.
int shim_FSpOpenResFile(uint32_t *args) { (void)args; return -1; }
