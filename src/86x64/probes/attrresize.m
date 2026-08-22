/* attrresize.m — is the post-show resize panic universal, or an artifact of the
 * COMPOSITING attribute? If a non-compositing Carbon window can be resized after
 * showing, the whole "hold the show" line of attack is unnecessary. */
#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern OSStatus CreateNewWindow(WindowClass, WindowAttributes, const Rect *, WindowRef *);
extern void     ShowWindow(WindowRef);
extern OSStatus SetWindowBounds(WindowRef, WindowRegionCode, const Rect *);
extern OSStatus GetWindowBounds(WindowRef, WindowRegionCode, Rect *);

int main(int argc, char **argv) {
   [NSApplication sharedApplication];
   const unsigned cls   = argc > 1 ? (unsigned)strtoul(argv[1], NULL, 0) : kDocumentWindowClass;
   const unsigned attrs = argc > 2 ? (unsigned)strtoul(argv[2], NULL, 0) : 0;

   Rect r = { 100, 100, 100 + 600, 100 + 800 };
   WindowRef w = NULL;
   OSStatus st = CreateNewWindow(cls, attrs, &r, &w);
   printf("class=0x%x attrs=0x%08x -> CreateNewWindow st=%d w=%p\n",
          cls, attrs, (int)st, (void *)w);
   if (!w) { printf("RESULT: CREATE_FAILED\n"); return 2; }

   ShowWindow(w);
   printf("shown; now the post-show resize (fatal for compositing windows)...\n");
   fflush(stdout);

   Rect t = { 0, 0, 480, 640 };
   st = SetWindowBounds(w, kWindowContentRgn, &t);

   Rect got = { 0, 0, 0, 0 };
   GetWindowBounds(w, kWindowContentRgn, &got);
   printf("RESULT: SURVIVED st=%d now %dx%d\n",
          (int)st, got.right - got.left, got.bottom - got.top);
   return 0;
}
