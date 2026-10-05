/* 99_cod4_dead_imports — Call of Duty 4's (and its Bink's) imports that 64-bit
 * macOS removed outright: each bound to an abigen bridge that jumped to NULL, or
 * went raw to libSystem with i386 stack words, or was a stub that left its
 * out-params as stack garbage.
 *
 *   MaxBlock / TempNewHandle / TempHLock / TempHUnlock / TempDisposeHandle
 *       Bink's Mac allocator (TempNewHandle went native: a >4GB Handle).
 *   GetSharedLibrary / FindSymbol    Bink's InterfaceLib fallback (CFM).
 *   KLGetCurrentKeyboardLayout / KLGetKeyboardLayoutProperty   startup.
 *   mbrtowc / wcrtomb                the translated libstdc++'s codecvt.
 *   GetCurrentScrap / GetScrapFlavorSize   console paste.
 *   GetComponentVersion              its 3D-mixer AU version check.
 *   NewMenu                          its Window menu.
 *   GetAvailableWindowPositioningBounds    window placement.
 *   OpenRgn / CloseRgn               a framed rect recorded as a clip region.
 *
 * ON exits 42 (every answer defined and correct). The defects were the binds
 * themselves, so the OFF side is a bind check in the Makefile target: every
 * name binds libabiconv's ___<name>, none binds a native.
 */
extern int  printf(const char *, ...);
extern void exit(int);

typedef short OSErr;
typedef char **Handle;
int    MaxBlock(void);
Handle TempNewHandle(int size, OSErr *err);
void   TempHLock(Handle h, OSErr *err);
void   TempHUnlock(Handle h, OSErr *err);
void   TempDisposeHandle(Handle h, OSErr *err);
OSErr  GetSharedLibrary(const unsigned char *name, unsigned int arch, unsigned int opts,
                        void **conn, void **mainAddr, unsigned char *errMsg);
OSErr  FindSymbol(void *conn, const unsigned char *name, void **addr, unsigned char *cls);
int    KLGetCurrentKeyboardLayout(void **kl);
int    KLGetKeyboardLayoutProperty(void *kl, unsigned int tag, const void **value);
unsigned long mbrtowc(int *wc, const char *s, unsigned long n, void *st);
unsigned long wcrtomb(char *s, int wc, void *st);
int    GetCurrentScrap(void **scrap);
int    GetScrapFlavorSize(void *scrap, unsigned int flavor, int *size);
void  *FindNextComponent(void *prev, unsigned int *desc);
void  *OpenComponent(void *comp);
int    GetComponentVersion(void *inst);
int    CloseComponent(void *inst);
void  *NewMenu(short id, const unsigned char *title);
void  *GetMainDevice(void);
int    GetAvailableWindowPositioningBounds(void *gd, short *rect);
int    NewGWorld(void **gw, short depth, const short *bounds, void *ct, void *gd, unsigned int flags);
void   SetGWorld(void *port, void *gd);
void  *NewRgn(void);
void   OpenRgn(void);
void   FrameRect(const short *r);
void   CloseRgn(void *rgn);
short *GetRegionBounds(void *rgn, short *bounds);

int main(void) {
   OSErr e = 1;
   int step = 0, wc = 0, sz = 77;
   char mb[8] = { 0 };
   unsigned char msg[4] = { 9, 9, 9, 9 }, cls = 9;
   void *conn = (void *)1, *mainp = (void *)1, *addr = (void *)1, *kl = 0, *scrap = 0;
   const void *grp = (const void *)-1;
   static char st[128];                      /* mbstate_t */

   if (MaxBlock() < 0x10000000) { step = 1; goto out; }
   Handle h = TempNewHandle(64, &e);
   if (!h || e || !*h) { step = 2; goto out; }
   e = 1; TempHLock(h, &e);
   if (e) { step = 3; goto out; }
   (*h)[63] = 'x';
   e = 1; TempHUnlock(h, &e); if (e) { step = 4; goto out; }
   e = 1; TempDisposeHandle(h, &e); if (e) { step = 4; goto out; }
   if (GetSharedLibrary((const unsigned char *)"\x0c" "InterfaceLib", 0x70777063 /*'pwpc'*/, 1,
                        &conn, &mainp, msg) != -2804 || conn || mainp || msg[0]) { step = 5; goto out; }
   if (FindSymbol(0, (const unsigned char *)"\x09" "NewPtrSys", &addr, &cls) != -2805 || addr || cls) { step = 6; goto out; }
   if (KLGetCurrentKeyboardLayout(&kl) || !kl) { step = 7; goto out; }
   if (KLGetKeyboardLayoutProperty(kl, 6 /*kKLGroupIdentifier*/, &grp) || (unsigned long)grp > 255) { step = 8; goto out; }
   if (mbrtowc(&wc, "A", 1, st) != 1 || wc != 'A' || wcrtomb(mb, 'B', st) != 1 || mb[0] != 'B') { step = 9; goto out; }
   if (GetCurrentScrap(&scrap) || !scrap) { step = 10; goto out; }
   if (GetScrapFlavorSize(scrap, 0x58585858 /*'XXXX'*/, &sz) != -102 || sz != 0) { step = 11; goto out; }
   unsigned int mixer[5] = { 0x61756d78 /*'aumx'*/, 0x33646d78 /*'3dmx'*/, 0x6170706c /*'appl'*/, 0, 0 };
   void *comp = FindNextComponent(0, mixer), *inst = comp ? OpenComponent(comp) : 0;
   int ver = inst ? GetComponentVersion(inst) : 0;
   if (inst) CloseComponent(inst);
   if (ver < 0x20000) { step = 12; goto out; }
   if (!NewMenu(200, (const unsigned char *)"\x06" "Window")) { step = 13; goto out; }
   short avail[4] = { 0, 0, 0, 0 };
   if (GetAvailableWindowPositioningBounds(GetMainDevice(), avail) ||
       avail[2] <= avail[0] || avail[3] <= avail[1]) { step = 14; goto out; }
   void *gw = 0, *rgn;
   static const short port[4] = { 0, 0, 64, 64 }, frame[4] = { 5, 6, 30, 40 };
   short rb[4] = { 0, 0, 0, 0 };
   if (NewGWorld(&gw, 32, port, 0, 0, 0) || !gw) { step = 15; goto out; }
   SetGWorld(gw, 0);
   rgn = NewRgn();
   OpenRgn(); FrameRect(frame); CloseRgn(rgn);
   GetRegionBounds(rgn, rb);
   if (rb[0] != 5 || rb[1] != 6 || rb[2] != 30 || rb[3] != 40) { step = 16; goto out; }
   step = 42;
out:
   printf("step=%d e=%d kl=%p group=%ld wc=%d sz=%d\n", step, e, kl, (long)grp, wc, sz);
   exit(step);
}
