/* 99_nib_key_route -- keys reach a rebuilt nib window's self-drawn edit field.
 *
 * Call of Duty 4's key-code dialog: HIToolbox will not focus our self-drawn
 * IBCarbonEditText (SetKeyboardFocus -> errCouldntSetFocus), so every keystroke
 * landed at the window and typing went nowhere. carbon_classic_widgets.c
 * ccw_route_keys_to_fields installs a RawKeyDown handler on the event dispatcher
 * that sends printable keys to the field WE focused, as a TextInput event.
 *
 * This focuses {'Item',1} (kEventControlSetFocusPart, what a click or
 * AdvanceKeyboardFocus ends up doing), sends synthesized RawKeyDown events for
 * "ABCD" to GetEventDispatcherTarget(), then reads the field back into a 4-byte
 * buffer exactly as CoD4 does (raw bytes, no terminator).
 *
 * argv[1] = a bundle holding Key.nib (see nib_edit_field_test.sh, which drives
 * this too). A real, unshown HIToolbox window: GUI guard.
 * ON exits 42; OFF = M64_NO_NIB_KEY_ROUTE=1 (the keys go nowhere -> exit 6).
 */
extern int  printf(const char *, ...);
extern void exit(int);
extern unsigned long strlen(const char *);

typedef int OSStatus;
typedef struct { unsigned int signature; int id; } ControlID;
const void *CFStringCreateWithCString(const void *alloc, const char *s, unsigned int enc);
const void *CFURLCreateFromFileSystemRepresentation(const void *, const char *, long, unsigned char);
const void *CFBundleCreate(const void *, const void *);
OSStatus CreateNibReferenceWithCFBundle(const void *bundle, const void *name, void **nib);
OSStatus CreateWindowFromNib(void *nib, const void *name, void **win);
OSStatus GetControlByID(void *win, const ControlID *id, void **ctl);
OSStatus GetControlData(void *ctl, short part, unsigned int tag, int max, void *data, int *actual);
OSStatus CreateEvent(const void *alloc, unsigned int cls, unsigned int kind, double when,
                     unsigned int attrs, void **ev);
OSStatus SetEventParameter(void *ev, unsigned int name, unsigned int type, int size, const void *data);
void     ReleaseEvent(void *ev);
void    *GetEventDispatcherTarget(void);
void    *GetControlEventTarget(void *ctl);
OSStatus SendEventToEventTarget(void *ev, void *target);
OSStatus SetUserFocusWindow(void *win);
void     DisposeWindow(void *win);

/* Build an event of (cls, kind), attach one parameter (and 'kmod' when given), send it. */
static int send_event(unsigned int cls, unsigned int kind, void *target,
                      unsigned int pname, unsigned int ptype, int psize, const void *pdata,
                      const void *mods) {
   void *ev = 0;
   if (CreateEvent(0, cls, kind, 0, 0, &ev) || !ev) return -1;
   SetEventParameter(ev, pname, ptype, psize, pdata);
   if (mods) SetEventParameter(ev, 0x6b6d6f64 /*'kmod'*/, 0x6d61676e /*'magn'*/, 4, mods);
   OSStatus st = SendEventToEventTarget(ev, target);
   ReleaseEvent(ev);
   return st;
}

int main(int argc, char **argv) {
   void *nib = 0, *win = 0, *ctl = 0;
   ControlID id = { 0x4974656d /*'Item'*/, 1 };
   char text[5] = { 0 };
   short part = 1;
   unsigned int mods = 0;
   int step = 0, act = -1;
   if (argc < 2) exit(1);
   const void *url = CFURLCreateFromFileSystemRepresentation(0, argv[1], (long)strlen(argv[1]), 1);
   const void *bundle = url ? CFBundleCreate(0, url) : 0;
   if (!bundle || CreateNibReferenceWithCFBundle(bundle, CFStringCreateWithCString(0, "Key", 0x08000100), &nib) ||
       CreateWindowFromNib(nib, CFStringCreateWithCString(0, "KeyCode", 0x08000100), &win) || !win) { step = 1; goto out; }
   if (GetControlByID(win, &id, &ctl) || !ctl) { step = 2; goto out; }
   SetUserFocusWindow(win);   /* the router finds the field through the user-focus window */
   /* focus the field: kEventClassControl (4013 = SetFocusPart), 'cprt' = 1 */
   if (send_event(0x636e746c /*'cntl'*/, 4013, GetControlEventTarget(ctl),
                  0x63707274 /*'cprt'*/, 0x63707274, 2, &part, 0)) { step = 3; goto out; }
   for (const char *p = "ABCD"; *p; p++)
      if (send_event(0x6b657962 /*'keyb'*/, 1 /*RawKeyDown*/, GetEventDispatcherTarget(),
                     0x6b636872 /*'kchr'*/, 0x54455854 /*'TEXT'*/, 1, p, &mods) == -1) { step = 4; goto out; }
   if (GetControlData(ctl, 0, 0x74657874 /*'text'*/, 4, text, &act)) { step = 5; goto out; }
   if (text[0] != 'A' || text[1] != 'B' || text[2] != 'C' || text[3] != 'D') { step = 6; goto out; }
   step = 42;
out:
   printf("step=%d act=%d text=%c%c%c%c\n", step, act, text[0] ? text[0] : '-', text[1] ? text[1] : '-',
          text[2] ? text[2] : '-', text[3] ? text[3] : '-');
   if (win) DisposeWindow(win);
   exit(step);
}
