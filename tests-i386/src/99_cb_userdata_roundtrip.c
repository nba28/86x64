/* 99_cb_userdata_roundtrip — an event handler's userData must come back to the
 * i386 handler exactly as registered. The forward bridge unwraps a >4GB handle
 * to its real pointer; the callback bridge truncated it on the way back (Call
 * of Duty 4: QuitAppModalLoopForWindow got 0xb27440 for window 0x600000b27440,
 * OK/Quit did nothing). userData here is a CFString handle (native heap, >4GB).
 * ON exits 42. OFF (M64_NO_CB_PTR_WRAP=1, run time): truncated -> exit 1. */
extern void exit(int);
extern int printf(const char *, ...);
typedef int OSStatus;
typedef struct { unsigned eventClass, eventKind; } EventTypeSpec;
void *GetApplicationEventTarget(void);
void *NewEventHandlerUPP(void *proc);
OSStatus InstallEventHandler(void *target, void *upp, unsigned long n, const EventTypeSpec *l,
                             void *userData, void **outRef);
OSStatus CreateEvent(void *alloc, unsigned cls, unsigned kind, double when, unsigned flags, void **out);
OSStatus SendEventToEventTarget(void *e, void *target);
const void *CFStringCreateWithCString(const void *alloc, const char *s, unsigned enc);
static void *g_got; static int g_called;
static OSStatus handler(void *call, void *event, void *userData) {
   g_called = 1; g_got = userData; return 0;
}
int main(void) {
   const void *s = CFStringCreateWithCString(0, "userdata", 0x08000100);
   EventTypeSpec spec = { 'test', 1 };
   void *ev = 0;
   OSStatus r1 = InstallEventHandler(GetApplicationEventTarget(), NewEventHandlerUPP((void *)handler),
                                     1, &spec, (void *)s, 0);
   OSStatus r2 = CreateEvent(0, 'test', 1, 0.0, 0, &ev);
   OSStatus r3 = SendEventToEventTarget(ev, GetApplicationEventTarget());
   printf("install=%d create=%d send=%d called=%d registered=%p got=%p\n", r1, r2, r3, g_called, s, g_got);
   exit(g_called && g_got == (void *)s ? 42 : 1);
}
