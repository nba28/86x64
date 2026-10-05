/* 99_ctrl_range_refcon — a control's classic range and refCon survive the trip.
 *
 * Call of Duty 4's key-code boxes keep their 4-character limit in the control
 * maximum (SetControlMaximum) and their character validator in the refCon
 * (SetControlReference); the key filter reads both back on every keystroke. The
 * classic wrappers were removed from 64-bit HIToolbox and our stubs dropped the
 * writes (GetControlMaximum answered 1, GetControlReference 0), so every box was
 * "full" after one key and the validator vanished. HIView's own range and a
 * control property carry them now.
 *
 * ON exits 42. OFF: M64_NO_CTRL_RANGE=1 (run time) -> the old constants -> exit 2.
 */
extern int  printf(const char *, ...);
extern void exit(int);

typedef int OSStatus;
OSStatus HIObjectCreate(const void *classID, void *event, void **out);
const void *CFStringCreateWithCString(const void *alloc, const char *s, unsigned int enc);
void  SetControlMaximum(void *c, short v);
short GetControlMaximum(void *c);
void  SetControlMinimum(void *c, short v);
short GetControlMinimum(void *c);
void  SetControl32BitMaximum(void *c, int v);
int   GetControl32BitMaximum(void *c);
void  SetControlReference(void *c, int v);
int   GetControlReference(void *c);

int main(void) {
   void *v = 0;
   int step = 0;
   const void *cls = CFStringCreateWithCString(0, "com.apple.hiview", 0x08000100 /*UTF8*/);
   if (HIObjectCreate(cls, 0, &v) || !v) { step = 1; goto out; }
   SetControlMinimum(v, 2);
   SetControlMaximum(v, 4);
   if (GetControlMaximum(v) != 4 || GetControlMinimum(v) != 2) { step = 2; goto out; }
   SetControl32BitMaximum(v, 70000);
   if (GetControl32BitMaximum(v) != 70000) { step = 3; goto out; }
   SetControlReference(v, 0x1234abcd);
   if (GetControlReference(v) != 0x1234abcd) { step = 4; goto out; }
   step = 42;
out:
   printf("step=%d max=%d min=%d ref=0x%x\n", step, v ? GetControlMaximum(v) : -1,
          v ? GetControlMinimum(v) : -1, v ? GetControlReference(v) : 0);
   exit(step);
}
