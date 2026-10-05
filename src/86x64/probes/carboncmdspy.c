/* carboncmdspy.c — why does a translated Carbon dialog ignore its buttons?
 * Interposes the NATIVE Carbon event calls libabiconv's bridges make and logs
 * only those whose caller is libabiconv (= on the translated app's behalf):
 * the handler InstallEventHandler gets, whether that handler runs (it asks
 * GetEventClass/Kind first), what GetEventParameter returns for each request,
 * and whether QuitAppModalLoopForWindow is ever reached. Wraps no callback.
 *   clang -arch x86_64 -dynamiclib -framework Carbon -o /tmp/carboncmdspy.dylib carboncmdspy.c
 *   DYLD_INSERT_LIBRARIES=/tmp/carboncmdspy.dylib <App>.app/Contents/MacOS/<App>  */
#include <Carbon/Carbon.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

static int from_abiconv(void *ra) {
   Dl_info d;
   return dladdr(ra, &d) && d.dli_fname && strstr(d.dli_fname, "libabiconv");
}
static const char *fcc(UInt32 v, char *b) {
   for (int i = 0; i < 4; i++) { char c = (char)(v >> (24 - 8 * i)); b[i] = (c >= 32 && c < 127) ? c : '.'; }
   b[4] = 0; return b;
}
#define RA __builtin_return_address(0)
/* still exported on 64-bit, no longer declared */
OSStatus RunAppModalLoopForWindow(WindowRef);
OSStatus QuitAppModalLoopForWindow(WindowRef);

static OSStatus s_InstallEventHandler(EventTargetRef t, EventHandlerUPP h, ItemCount n,
                                      const EventTypeSpec *l, void *ud, EventHandlerRef *out) {
   OSStatus r = InstallEventHandler(t, h, n, l, ud, out);
   if (from_abiconv(RA)) {
      Dl_info d; char a[5], k[5];
      int ok = dladdr((void *)h, &d);
      fprintf(stderr, "[spy] InstallEventHandler target=%p handler=%p (%s+0x%lx) n=%lu ->%d :",
              (void *)t, (void *)h, ok && d.dli_fname ? strrchr(d.dli_fname, '/') + 1 : "?",
              ok ? (unsigned long)((char *)h - (char *)d.dli_fbase) : 0, (unsigned long)n, (int)r);
      for (ItemCount i = 0; l && i < n && i < 8; i++)
         fprintf(stderr, " {%s,%u}", fcc(l[i].eventClass, a), (unsigned)l[i].eventKind), (void)k;
      fprintf(stderr, "\n");
   }
   return r;
}
static UInt32 s_GetEventClass(EventRef e) {
   UInt32 c = GetEventClass(e);
   if (from_abiconv(RA)) { char a[5]; fprintf(stderr, "[spy] GetEventClass -> %s\n", fcc(c, a)); }
   return c;
}
static UInt32 s_GetEventKind(EventRef e) {
   UInt32 k = GetEventKind(e);
   if (from_abiconv(RA)) fprintf(stderr, "[spy] GetEventKind -> %u\n", (unsigned)k);
   return k;
}
static OSStatus s_GetEventParameter(EventRef e, EventParamName n, EventParamType t, EventParamType *at,
                                    ByteCount sz, ByteCount *osz, void *out) {
   OSStatus r = GetEventParameter(e, n, t, at, sz, osz, out);
   if (from_abiconv(RA)) {
      char a[5], b[5], c[5];
      fprintf(stderr, "[spy] GetEventParameter name=%s type=%s size=%lu -> %d", fcc(n, a), fcc(t, b),
              (unsigned long)sz, (int)r);
      if (r == noErr && t == typeHICommand && out && sz >= 8)
         fprintf(stderr, " commandID=%s", fcc(((HICommand *)out)->commandID, c));
      fprintf(stderr, "\n");
   }
   return r;
}
static OSStatus s_RunAppModalLoopForWindow(WindowRef w) {
   if (from_abiconv(RA)) fprintf(stderr, "[spy] RunAppModalLoopForWindow %p enter\n", (void *)w);
   OSStatus r = RunAppModalLoopForWindow(w);
   fprintf(stderr, "[spy] RunAppModalLoopForWindow %p -> %d\n", (void *)w, (int)r);
   return r;
}
static OSStatus s_QuitAppModalLoopForWindow(WindowRef w) {
   OSStatus r = QuitAppModalLoopForWindow(w);
   if (from_abiconv(RA)) fprintf(stderr, "[spy] QuitAppModalLoopForWindow %p -> %d\n", (void *)w, (int)r);
   return r;
}

__attribute__((used, section("__DATA,__interpose"))) static struct { const void *n, *o; } interposers[] = {
   { (void *)s_InstallEventHandler, (void *)InstallEventHandler },
   { (void *)s_GetEventClass, (void *)GetEventClass },
   { (void *)s_GetEventKind, (void *)GetEventKind },
   { (void *)s_GetEventParameter, (void *)GetEventParameter },
   { (void *)s_RunAppModalLoopForWindow, (void *)RunAppModalLoopForWindow },
   { (void *)s_QuitAppModalLoopForWindow, (void *)QuitAppModalLoopForWindow },
};
