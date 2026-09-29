/* 99_display_point_modes — does every mode list a translated app can read
 * describe the desktop in POINTS, the way 10.7.4-10.13 showed a non-Retina app?
 *
 * WHY THIS EXISTS. A resolution menu built from a pixel-size or empty list
 * makes the app create windows bigger than the desktop: Halo's Display Manager
 * list was EMPTY, it fell back to its own table and opened 2560x1440 on a
 * 1512x982-point screen (2026-09-29); CGDisplayCopyAllDisplayModes(dpy, NULL)
 * returns only 1x modes of 1920x1200 and up there. The pre-Mojave contract:
 * only point modes that fit, plus the classic 640x480/800x600/1024x768.
 *
 * Checks, for the Display Manager (Halo), CGDisplayAvailableModes (Portal 2)
 * and CGDisplayCopyAllDisplayModes (Portal 2, iPhoto): the list is non-empty,
 * every mode fits the desktop, and the DM iterator gets a well-formed record
 * tree (resolution, 32bpp depth VPBlock whose bounds match). OFF arm
 * (M64_NO_DISPLAY_FULLSCREEN_BRIDGE=1): the old lists (DM empty, CG oversized).
 */
extern int  printf(const char *, ...);
extern void exit(int);
typedef unsigned int u32; typedef unsigned short u16; typedef const void *CFTypeRef;
extern u32 CGMainDisplayID(void);
extern unsigned long CGDisplayPixelsWide(u32), CGDisplayPixelsHigh(u32);
extern short DMNewDisplayModeList(u32 id, u32 flags, u32 rsv, u32 *count, void **list);
extern short DMGetIndexedDisplayModeFromList(void *list, u32 idx, u32 rsv, void *upp, void *user);
extern short DMDisposeList(void *list);
extern void *NewDMDisplayModeListIteratorUPP(void *proc);
extern CFTypeRef CGDisplayAvailableModes(u32);
extern CFTypeRef CGDisplayCopyAllDisplayModes(u32, CFTypeRef);
extern long CFArrayGetCount(CFTypeRef);
extern CFTypeRef CFArrayGetValueAtIndex(CFTypeRef, long);
extern CFTypeRef CFDictionaryGetValue(CFTypeRef, CFTypeRef);
extern unsigned char CFNumberGetValue(CFTypeRef, long type, void *out);
extern CFTypeRef __CFStringMakeConstantString(const char *);
extern unsigned long CGDisplayModeGetWidth(CFTypeRef), CGDisplayModeGetHeight(CFTypeRef);

static u32 g_dw, g_dh, g_seen, g_fit, g_tree, g_has800;
static void iter(void *user, u32 idx, const unsigned char *e)
{
   (void)user; (void)idx;
   const unsigned char *res = *(unsigned char *const *)(e + 8);
   const unsigned char *blk = *(unsigned char *const *)(e + 16);
   u32 w = *(const u32 *)(res + 8), h = *(const u32 *)(res + 12);
   const unsigned char *dinfo = *(unsigned char *const *)(blk + 4);
   const unsigned char *vpb = *(unsigned char *const *)(dinfo + 4);
   g_seen++;
   g_fit += w <= g_dw && h <= g_dh;
   g_tree += *(const u32 *)blk == 1 && *(const u16 *)(vpb + 32) == 32 &&
             *(const u16 *)(vpb + 12) == w && *(const u16 *)(vpb + 10) == h;
   g_has800 |= w == 800 && h == 600;
}

int main(void)
{
   u32 d = CGMainDisplayID();
   g_dw = (u32)CGDisplayPixelsWide(d); g_dh = (u32)CGDisplayPixelsHigh(d);
   u32 n = 0; void *list = 0;
   DMNewDisplayModeList(d, 0, 0, &n, &list);
   void *upp = NewDMDisplayModeListIteratorUPP((void *)iter);
   for (u32 i = 0; i < n; i++) DMGetIndexedDisplayModeFromList(list, i, 0, upp, 0);
   if (list) DMDisposeList(list);
   printf("dm_count=%d dm_fit=%d dm_tree=%d dm_800=%d\n", n > 0, g_seen == n && g_fit == n,
          g_seen == n && g_tree == n, g_has800);

   CFTypeRef kw = __CFStringMakeConstantString("Width"), kh = __CFStringMakeConstantString("Height");
   CFTypeRef av = CGDisplayAvailableModes(d);
   long an = av ? CFArrayGetCount(av) : 0, afit = 0;
   for (long i = 0; i < an; i++) {
      CFTypeRef m = CFArrayGetValueAtIndex(av, i); int w = 0, h = 0;
      CFNumberGetValue(CFDictionaryGetValue(m, kw), 9 /* kCFNumberIntType */, &w);
      CFNumberGetValue(CFDictionaryGetValue(m, kh), 9, &h);
      afit += (u32)w <= g_dw && (u32)h <= g_dh;
   }
   printf("avail_count=%d avail_fit=%d\n", an > 0, an > 0 && afit == an);

   CFTypeRef all = CGDisplayCopyAllDisplayModes(d, 0);
   long cn = all ? CFArrayGetCount(all) : 0, cfit = 0;
   for (long i = 0; i < cn; i++) {
      CFTypeRef m = CFArrayGetValueAtIndex(all, i);
      cfit += CGDisplayModeGetWidth(m) <= g_dw && CGDisplayModeGetHeight(m) <= g_dh;
   }
   printf("copyall_count=%d copyall_fit=%d\n", cn > 0, cn > 0 && cfit == cn);
   printf("desktop=%dx%d dm=%d avail=%ld copyall=%ld\n", g_dw, g_dh, n, an, cn);
   exit(0);   /* the 86x64.sh wrapper enters _main via jmp: no return frame */
}
