/*
 * button_probe.c — does native Carbon `Button()` report a physical press inside
 * an x86_64 (Rosetta) process that does NOT pump the Carbon event queue?
 *
 * WHY: Halo's whole input poll is one function (translated 0x103b5048) that does
 *   GetKeys(&km); switch (id) { case 1: case 2: return Button() ? -32768 : 0;
 *                               default: return keymap bit; }
 * i.e. the KEYBOARD is read from the hardware KeyMap (which is why arrow keys
 * work) and the MOUSE BUTTON is read through classic `Button()`. Halo runs its
 * own render loop (sample: MPDelayUntil/MPYield/aglSwapBuffers), not
 * RunApplicationEventLoop, so if `Button()` reports the EVENT MANAGER's cached
 * state rather than the physical one it can never become true.
 *
 * Ground truth for "is the button physically down" =
 *   CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, 0).
 *
 * Build:  clang -arch x86_64 -o button_probe button_probe.c \
 *             -framework ApplicationServices
 * Run:    ./button_probe [seconds]     (press/hold the mouse button, or drive it
 *                                       synthetically, while it runs)
 */
#include <ApplicationServices/ApplicationServices.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

typedef unsigned char Boolean_t;
typedef Boolean_t (*button_fn)(void);
typedef uint32_t  (*gcebs_fn)(void);
typedef void      (*getkeys_fn)(uint32_t km[4]);

int main(int argc, char **argv)
{
   int secs = argc > 1 ? atoi(argv[1]) : 12;

   void *carbon = dlopen("/System/Library/Frameworks/Carbon.framework/Carbon", RTLD_LAZY);
   if (!carbon) carbon = dlopen("/System/Library/Frameworks/Carbon.framework/Versions/A/Carbon", RTLD_LAZY);
   printf("Carbon = %p (%s)\n", carbon, carbon ? "loaded" : dlerror());
   if (!carbon) return 1;

   button_fn  Button_ = (button_fn) dlsym(carbon, "Button");
   gcebs_fn   GCEBS_  = (gcebs_fn)  dlsym(carbon, "GetCurrentEventButtonState");
   getkeys_fn GetKeys_= (getkeys_fn)dlsym(carbon, "GetKeys");
   printf("Button=%p GetCurrentEventButtonState=%p GetKeys=%p\n",
          (void*)Button_, (void*)GCEBS_, (void*)GetKeys_);
   if (!Button_) return 1;

   printf("\n  t    Button()  GCEBS  CGsrc(combined)  CGsrc(HID)   keymap[0..3]\n");
   int prev = -1;
   for (int i = 0; i < secs * 10; i++) {
      Boolean_t b   = Button_();
      uint32_t  g   = GCEBS_ ? GCEBS_() : 0xffffffff;
      int cs = CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, kCGMouseButtonLeft);
      int hs = CGEventSourceButtonState(kCGEventSourceStateHIDSystemState,       kCGMouseButtonLeft);
      uint32_t km[4] = {0,0,0,0};
      if (GetKeys_) GetKeys_(km);
      int state = (b?1:0) | (cs?2:0) | (hs?4:0) | ((g&1)?8:0);
      if (state != prev || i % 10 == 0) {
         printf("%5.1f      %d       0x%x        %d              %d        %08x %08x %08x %08x\n",
                i / 10.0, b ? 1 : 0, g, cs, hs, km[0], km[1], km[2], km[3]);
         fflush(stdout);
         prev = state;
      }
      usleep(100000);
   }
   return 0;
}
