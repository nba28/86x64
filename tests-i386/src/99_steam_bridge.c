/*
 * 99_steam_bridge — the Steamworks bridge (src/abiconv/steam_bridge.c +
 * steam_tramp.asm): i386 code calls a NATIVE x86_64 libsteam_api interface
 * through a low-4GB proxy vtable, and every result comes back in the i386
 * return convention.
 *
 * Portal 2 died with "Steam is not running": its libsteam_api's x86_64 slice
 * talks to the real Steam, but its interface objects live above 4GB and use
 * the x86_64 ABI. The bridge hands the game proxies whose vtable slots are
 * i386-callable stubs. This fixture drives the three return shapes against a
 * fake native lib (steam_bridge_fake.c, loaded via ABICONV_STEAM_API):
 *   SteamUser021 #2 GetSteamID  -> uint64 CSteamID in edx:eax
 *   STEAMMUSIC_INTERFACE_VERSION001 #8 GetVolume -> float in st0
 *   SteamGameServer014 #33 GetPublicIP -> 20-byte struct via the hidden
 *       pointer; the callee pops it (`ret $4`), so %esp must not drift.
 *
 * Exit 42 = all three right. 3 = no interfaces (the OFF arm: no native lib).
 * 10 + bits = a wrong value (1 id, 2 float, 4 struct, 8 esp drift).
 */
#include <stdint.h>
#include <stdlib.h>

typedef struct { uint32_t w[5]; } ip20;
struct obj { void **vt; };

extern void *SteamInternal_FindOrCreateUserInterface(int32_t, const char *);
extern void *SteamInternal_FindOrCreateGameServerInterface(int32_t, const char *);

typedef uint64_t (*sid_fn)(void *);
typedef float (*vol_fn)(void *);
typedef ip20 (*ip_fn)(void *);

int main(void) {
   struct obj *u = SteamInternal_FindOrCreateUserInterface(1, "SteamUser021");
   struct obj *m = SteamInternal_FindOrCreateUserInterface(1, "STEAMMUSIC_INTERFACE_VERSION001");
   struct obj *g = SteamInternal_FindOrCreateGameServerInterface(1, "SteamGameServer014");
   if (!u || !m || !g) exit(3);

   uint64_t id = ((sid_fn)u->vt[2])(u);
   float v = ((vol_fn)m->vt[8])(m);
   uint32_t e0, e1;
   __asm__ volatile("movl %%esp, %0" : "=r"(e0));
   ip20 ip = ((ip_fn)g->vt[33])(g);
   __asm__ volatile("movl %%esp, %0" : "=r"(e1));

   int bad = 0;
   if (id != 0x0110000100000042ULL) bad |= 1;
   if (v != 0.75f) bad |= 2;
   if (ip.w[0] != 0x11111111u || ip.w[4] != 0x55555555u) bad |= 4;
   if (e0 != e1) bad |= 8;
   exit(bad ? 10 + bad : 42);
}
