/* 99_nib_edit_field — a native nib window's IBCarbonEditText is a real, typable
 * classic edit field that honours the app's key filter.
 *
 * Call of Duty 4's key-code dialog: CreateWindowFromNib succeeds natively, but
 * IBCarbonRuntime maps every IBCarbonEditText to an inert UserPane, so the five
 * boxes could not be typed into. The app then drives each box the classic way:
 * GetControlByID({'Item',n}), GetControlKind == 'eutx' before focusing it,
 * SetControlData(kControlEditTextKeyFilterTag) with a filter that upper-cases,
 * rejects bad characters and reads GetControlDataSize/'sele' to know when the box
 * is full. This drives the same calls headlessly (the window is never shown):
 * HandleControlKey stands in for a keystroke.
 *
 * argv[1] = a bundle holding Key.nib (window "KeyCode", edit field {'Item',1}).
 * ON exits 42. OFF: M64_NO_NIB_EDIT_SWAP=1 (run time) -> the UserPane -> exit 3.
 */
extern int  printf(const char *, ...);
extern void exit(int);
extern unsigned long strlen(const char *);

typedef int OSStatus;
typedef struct { unsigned int signature; int id; } ControlID;
typedef struct { unsigned int signature, kind; } ControlKind;
const void *CFStringCreateWithCString(const void *alloc, const char *s, unsigned int enc);
const void *CFURLCreateFromFileSystemRepresentation(const void *, const char *, long, unsigned char);
const void *CFBundleCreate(const void *, const void *);
OSStatus CreateNibReferenceWithCFBundle(const void *bundle, const void *name, void **nib);
OSStatus CreateWindowFromNib(void *nib, const void *name, void **win);
OSStatus GetControlByID(void *win, const ControlID *id, void **ctl);
OSStatus GetControlKind(void *ctl, ControlKind *kind);
void    *NewControlKeyFilterUPP(void *proc);
OSStatus SetControlData(void *ctl, short part, unsigned int tag, int size, const void *data);
OSStatus GetControlData(void *ctl, short part, unsigned int tag, int max, void *data, int *actual);
OSStatus GetControlDataSize(void *ctl, short part, unsigned int tag, int *size);
short    HandleControlKey(void *ctl, short keyCode, short charCode, unsigned int mods);
void     DisposeWindow(void *win);

static int g_calls;
/* ControlKeyFilterResult: upper-case letters, block digits. */
static short filt(void *ctl, short *key, short *ch, unsigned short *mods) {
   (void)ctl; (void)key; (void)mods;
   g_calls++;
   if (*ch >= '0' && *ch <= '9') return 0;              /* kControlKeyFilterBlockKey */
   if (*ch >= 'a' && *ch <= 'z') *ch = (short)(*ch - 32);
   return 1;                                             /* kControlKeyFilterPassKey  */
}

int main(int argc, char **argv) {
   void *nib = 0, *win = 0, *ctl = 0, *upp;
   ControlID id = { 0x4974656d /*'Item'*/, 1 };
   ControlKind kind = { 0, 0 };
   char text[16] = { 0 };
   short sel[2] = { -1, -1 };
   int step = 0, act = 0, sz = -1;
   if (argc < 2) exit(1);
   const void *url = CFURLCreateFromFileSystemRepresentation(0, argv[1], (long)strlen(argv[1]), 1);
   const void *bundle = url ? CFBundleCreate(0, url) : 0;
   if (!bundle || CreateNibReferenceWithCFBundle(bundle, CFStringCreateWithCString(0, "Key", 0x08000100), &nib) ||
       CreateWindowFromNib(nib, CFStringCreateWithCString(0, "KeyCode", 0x08000100), &win) || !win) { step = 1; goto out; }
   if (GetControlByID(win, &id, &ctl) || !ctl) { step = 2; goto out; }
   if (GetControlKind(ctl, &kind) || kind.kind != 0x65757478 /*'eutx'*/) { step = 3; goto out; }
   upp = NewControlKeyFilterUPP((void *)filt);
   if (upp != (void *)filt || SetControlData(ctl, 0, 0x666c7472 /*'fltr'*/, 4, &upp)) { step = 4; goto out; }
   HandleControlKey(ctl, 0, 'a', 0);
   HandleControlKey(ctl, 0, '5', 0);                     /* blocked by the filter */
   HandleControlKey(ctl, 0, 'b', 0);
   if (g_calls != 3) { step = 5; goto out; }
   if (GetControlDataSize(ctl, 0, 0x74657874 /*'text'*/, &sz) || sz != 2) { step = 6; goto out; }
   if (GetControlData(ctl, 0, 0x74657874, sizeof text - 1, text, &act) || act != 2 ||
       text[0] != 'A' || text[1] != 'B') { step = 7; goto out; }
   if (GetControlData(ctl, 0, 0x73656c65 /*'sele'*/, 4, sel, &act) || sel[0] != 2 || sel[1] != 2) { step = 8; goto out; }
   step = 42;
out:
   printf("step=%d kind=0x%x calls=%d size=%d text=%s sel=%d,%d\n",
          step, kind.kind, g_calls, sz, text, sel[0], sel[1]);
   if (win) DisposeWindow(win);
   exit(step);
}
