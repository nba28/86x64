/* steam_bridge_fake.c — a NATIVE x86_64 stand-in for libsteam_api, for the
 * steam-bridge guard (99_steam_bridge.c). Interface objects with the real
 * Itanium vtable slots of the three methods the fixture calls. */
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>

struct ip20 { uint32_t w[5]; };

static uint64_t get_steam_id(void *self) { (void)self; return 0x0110000100000042ULL; }
static float get_volume(void *self) { (void)self; return 0.75f; }
static struct ip20 get_public_ip(void *self) {
   (void)self;
   struct ip20 r = {{0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u, 0x55555555u}};
   return r;
}

/* steamclient's strings live above 4GB: so does this one */
static const char *get_persona_name(void *self) {
   (void)self;
   static char *hi;
   if (!hi) {
      hi = mmap((void *)0x700000000000ULL, 4096, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANON, -1, 0);
      strcpy(hi, "persona-42");
   }
   return hi;
}

static void *user_vt[40], *music_vt[40], *gs_vt[40], *friends_vt[80];
static struct { void **vt; } user = {user_vt}, music = {music_vt}, gs = {gs_vt},
                             friends = {friends_vt};

void *SteamInternal_FindOrCreateUserInterface(int32_t h, const char *ver) {
   (void)h;
   user_vt[2] = (void *)get_steam_id;
   music_vt[8] = (void *)get_volume;
   friends_vt[0] = (void *)get_persona_name;
   if (!strcmp(ver, "SteamUser021")) return &user;
   if (!strcmp(ver, "STEAMMUSIC_INTERFACE_VERSION001")) return &music;
   if (!strcmp(ver, "SteamFriends017")) return &friends;
   return NULL;
}

void *SteamInternal_FindOrCreateGameServerInterface(int32_t h, const char *ver) {
   (void)h;
   gs_vt[33] = (void *)get_public_ip;
   return strcmp(ver, "SteamGameServer014") ? NULL : &gs;
}
