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
