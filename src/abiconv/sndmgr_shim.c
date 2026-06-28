// sndmgr_shim.c — graceful shims for the dead classic Sound Manager API.
//
// The Sound Manager (SndNewChannel/SndPlay/SndDoCommand/...) was removed from 64-bit/modern
// macOS. Civ IV's real audio runs through the bundled, translated OpenAL.framework; its
// Sound Manager references are a legacy/secondary path (system beeps, simple snd resources).
// We report benign success (noErr) so callers proceed without an error path, while no actual
// classic sound plays. A returned-error here is not warranted: these are fire-and-forget
// calls whose failure could spuriously abort an init sequence; silence is the correct
// degenerate behavior for removed audio hardware.
//
// MTSHIM convention: rdi -> &i386 args[0]; OSErr result in eax.

#include <stdint.h>
#include <string.h>

#define PTR(n) ((void *)(uintptr_t)args[(n)])
#define SND_NO_ERR (0)

uint32_t shim_SndPlay(uint32_t *args)          { (void)args; return SND_NO_ERR; }
uint32_t shim_SndDoCommand(uint32_t *args)     { (void)args; return SND_NO_ERR; }
uint32_t shim_SndDoImmediate(uint32_t *args)   { (void)args; return SND_NO_ERR; }
uint32_t shim_SndDisposeChannel(uint32_t *args){ (void)args; return SND_NO_ERR; }
void     shim_SysBeep(uint32_t *args)          { (void)args; }   // void; could NSBeep, but no-op

// SndChannelStatus(chan, theLength, SCStatusPtr theStatus): zero a 28-byte SCStatus so the
// caller reads a well-defined "idle, not busy" status rather than stack garbage.
uint32_t shim_SndChannelStatus(uint32_t *args) {
    void *st = PTR(2);
    if (st) memset(st, 0, 28);   // SCStatus is 28 bytes (Sound.h)
    return SND_NO_ERR;
}

// SndNewChannel(SndChannelPtr *chan, short synth, SInt32 init, SndCallBackUPP userRoutine):
// the channel ALLOCATOR. Unlike the fire-and-forget commands above it must hand back a valid
// SndChannelPtr; we cannot fabricate one (the caller stores it in a 32-bit slot and may deref
// it). So NULL the out channel and report no sound hardware — the caller takes its "no Sound
// Manager" path and never issues SndPlay/SndDoCommand on a bogus channel. Civ IV's real audio
// is the bundled OpenAL, so this disables only the dead classic path.
// ★This was the 1 Sound Manager symbol missed by the original probe: it is in CarbonSound's
// SYMTAB (which the nm-based bundled-export subtraction wrongly cleared) but the bind targets
// Carbon, where it is removed — the same symtab-vs-trie trap as _NewMovieFromDataRef.
#define SND_NO_HARDWARE (-201)   // notEnoughHardwareErr
uint32_t shim_SndNewChannel(uint32_t *args) {
    uint32_t *chan = (uint32_t *)PTR(0);
    if (chan) *chan = 0;          // out SndChannelPtr -> NULL
    return (uint32_t)SND_NO_HARDWARE;
}
