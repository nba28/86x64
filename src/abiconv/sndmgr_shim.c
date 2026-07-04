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
#include <stdlib.h>   /* malloc == libabiconv low-4GB heap (malloc_shim.c) */

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
// the channel ALLOCATOR. Report noErr and hand back a persistent zeroed dummy channel from the
// low-4GB heap. Returning an error here fails apps that GATE launch on SndNewChannel succeeding
// — Halo's "CantAllocSndChannel" capability self-check does `if (SndNewChannel(...) != noErr)
// { alert; _exit; }` (0x2aaa1a), so notEnoughHardwareErr would hard-quit it. A dummy channel is
// safe: our Snd* commands (SndPlay / SndDoCommand / SndDoImmediate / SndDisposeChannel /
// SndChannelStatus) no-op regardless of the channel pointer, and the block is zeroed so any
// field deref (e.g. an app reading chan->qLength before Dispose) reads benign 0s. Allocated
// once and kept for process lifetime (a few hundred bytes). Civ IV (real audio = bundled
// OpenAL) now takes the classic "sound OK" path but only issues the no-op Snd* commands, so no
// bogus-channel deref occurs — the earlier NULL/error behavior is superseded.
// ★This was the 1 Sound Manager symbol missed by the original probe: it is in CarbonSound's
// SYMTAB (which the nm-based bundled-export subtraction wrongly cleared) but the bind targets
// Carbon, where it is removed — the same symtab-vs-trie trap as _NewMovieFromDataRef.
uint32_t shim_SndNewChannel(uint32_t *args) {
    uint32_t *chan = (uint32_t *)PTR(0);
    static uint32_t g_dummy;   // low-4GB SndChannel handle, 0 until first alloc
    if (!g_dummy) {
        void *p = malloc(512);            // libabiconv low-4GB heap
        if (p) { memset(p, 0, 512); g_dummy = (uint32_t)(uintptr_t)p; }
    }
    if (chan) *chan = g_dummy;             // non-NULL, zeroed, safe to deref/Dispose
    return SND_NO_ERR;
}
