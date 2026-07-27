#import <Cocoa/Cocoa.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

static void shim_note(const char *sym) {
  static const char *seen[2048]; static int n;
  static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_lock(&mtx);
  for (int i = 0; i < n; i++)
    if (seen[i] == sym) { pthread_mutex_unlock(&mtx); return; }
  if (n < 2048) seen[n++] = sym;
  fprintf(stderr, "[shimauto:%s] %s: removed-OS symbol CALLED with no curated impl -- returning 0 (write one in shimdb/impl/ if its behavior matters)\n", "Carbon", sym);
  if (getenv("SHIMGEN_STUB_ABORT")) abort();
  pthread_mutex_unlock(&mtx);
}

long AreFloatingWindowsVisible(long a, long b, long c_, long d, long e, long f) __asm("_AreFloatingWindowsVisible");
long AreFloatingWindowsVisible(long a, long b, long c_, long d, long e, long f) { shim_note("_AreFloatingWindowsVisible"); return 0; }

long CalcVis(long a, long b, long c_, long d, long e, long f) __asm("_CalcVis");
long CalcVis(long a, long b, long c_, long d, long e, long f) { shim_note("_CalcVis"); return 0; }

long CalcVisBehind(long a, long b, long c_, long d, long e, long f) __asm("_CalcVisBehind");
long CalcVisBehind(long a, long b, long c_, long d, long e, long f) { shim_note("_CalcVisBehind"); return 0; }

long ChangeWindowPropertyAttributes(long a, long b, long c_, long d, long e, long f) __asm("_ChangeWindowPropertyAttributes");
long ChangeWindowPropertyAttributes(long a, long b, long c_, long d, long e, long f) { shim_note("_ChangeWindowPropertyAttributes"); return 0; }

long CheckUpdate(long a, long b, long c_, long d, long e, long f) __asm("_CheckUpdate");
long CheckUpdate(long a, long b, long c_, long d, long e, long f) { shim_note("_CheckUpdate"); return 0; }

long ClipAbove(long a, long b, long c_, long d, long e, long f) __asm("_ClipAbove");
long ClipAbove(long a, long b, long c_, long d, long e, long f) { shim_note("_ClipAbove"); return 0; }

long CloneWindow(long a, long b, long c_, long d, long e, long f) __asm("_CloneWindow");
long CloneWindow(long a, long b, long c_, long d, long e, long f) { shim_note("_CloneWindow"); return 0; }

long CopyWindowAlternateTitle(long a, long b, long c_, long d, long e, long f) __asm("_CopyWindowAlternateTitle");
long CopyWindowAlternateTitle(long a, long b, long c_, long d, long e, long f) { shim_note("_CopyWindowAlternateTitle"); return 0; }

long CreateWindowFromResource(long a, long b, long c_, long d, long e, long f) __asm("_CreateWindowFromResource");
long CreateWindowFromResource(long a, long b, long c_, long d, long e, long f) { shim_note("_CreateWindowFromResource"); return 0; }

long FindWindowOfClass(long a, long b, long c_, long d, long e, long f) __asm("_FindWindowOfClass");
long FindWindowOfClass(long a, long b, long c_, long d, long e, long f) { shim_note("_FindWindowOfClass"); return 0; }

long GetAvailableWindowPositioningBounds(long a, long b, long c_, long d, long e, long f) __asm("_GetAvailableWindowPositioningBounds");
long GetAvailableWindowPositioningBounds(long a, long b, long c_, long d, long e, long f) { shim_note("_GetAvailableWindowPositioningBounds"); return 0; }

long GetNewCWindow(long a, long b, long c_, long d, long e, long f) __asm("_GetNewCWindow");
long GetNewCWindow(long a, long b, long c_, long d, long e, long f) { shim_note("_GetNewCWindow"); return 0; }

long GetNewWindow(long a, long b, long c_, long d, long e, long f) __asm("_GetNewWindow");
long GetNewWindow(long a, long b, long c_, long d, long e, long f) { shim_note("_GetNewWindow"); return 0; }

long GetWTitle(long a, long b, long c_, long d, long e, long f) __asm("_GetWTitle");
long GetWTitle(long a, long b, long c_, long d, long e, long f) { shim_note("_GetWTitle"); return 0; }

long GetWindowContentColor(long a, long b, long c_, long d, long e, long f) __asm("_GetWindowContentColor");
long GetWindowContentColor(long a, long b, long c_, long d, long e, long f) { shim_note("_GetWindowContentColor"); return 0; }

long GetWindowContentPattern(long a, long b, long c_, long d, long e, long f) __asm("_GetWindowContentPattern");
long GetWindowContentPattern(long a, long b, long c_, long d, long e, long f) { shim_note("_GetWindowContentPattern"); return 0; }

long GetWindowGreatestAreaDevice(long a, long b, long c_, long d, long e, long f) __asm("_GetWindowGreatestAreaDevice");
long GetWindowGreatestAreaDevice(long a, long b, long c_, long d, long e, long f) { shim_note("_GetWindowGreatestAreaDevice"); return 0; }

long GetWindowOwnerCount(long a, long b, long c_, long d, long e, long f) __asm("_GetWindowOwnerCount");
long GetWindowOwnerCount(long a, long b, long c_, long d, long e, long f) { shim_note("_GetWindowOwnerCount"); return 0; }

long GetWindowPic(long a, long b, long c_, long d, long e, long f) __asm("_GetWindowPic");
long GetWindowPic(long a, long b, long c_, long d, long e, long f) { shim_note("_GetWindowPic"); return 0; }

long GetWindowPropertyAttributes(long a, long b, long c_, long d, long e, long f) __asm("_GetWindowPropertyAttributes");
long GetWindowPropertyAttributes(long a, long b, long c_, long d, long e, long f) { shim_note("_GetWindowPropertyAttributes"); return 0; }

long GetWindowProxyAlias(long a, long b, long c_, long d, long e, long f) __asm("_GetWindowProxyAlias");
long GetWindowProxyAlias(long a, long b, long c_, long d, long e, long f) { shim_note("_GetWindowProxyAlias"); return 0; }

long GetWindowProxyFSSpec(long a, long b, long c_, long d, long e, long f) __asm("_GetWindowProxyFSSpec");
long GetWindowProxyFSSpec(long a, long b, long c_, long d, long e, long f) { shim_note("_GetWindowProxyFSSpec"); return 0; }

long GetWindowProxyIcon(long a, long b, long c_, long d, long e, long f) __asm("_GetWindowProxyIcon");
long GetWindowProxyIcon(long a, long b, long c_, long d, long e, long f) { shim_note("_GetWindowProxyIcon"); return 0; }

long GetWindowRetainCount(long a, long b, long c_, long d, long e, long f) __asm("_GetWindowRetainCount");
long GetWindowRetainCount(long a, long b, long c_, long d, long e, long f) { shim_note("_GetWindowRetainCount"); return 0; }

long GetWindowStructurePort(long a, long b, long c_, long d, long e, long f) __asm("_GetWindowStructurePort");
long GetWindowStructurePort(long a, long b, long c_, long d, long e, long f) { shim_note("_GetWindowStructurePort"); return 0; }

long GetWindowStructureWidths(long a, long b, long c_, long d, long e, long f) __asm("_GetWindowStructureWidths");
long GetWindowStructureWidths(long a, long b, long c_, long d, long e, long f) { shim_note("_GetWindowStructureWidths"); return 0; }

long GetWindowUserState(long a, long b, long c_, long d, long e, long f) __asm("_GetWindowUserState");
long GetWindowUserState(long a, long b, long c_, long d, long e, long f) { shim_note("_GetWindowUserState"); return 0; }

long GetWindowWidgetHilite(long a, long b, long c_, long d, long e, long f) __asm("_GetWindowWidgetHilite");
long GetWindowWidgetHilite(long a, long b, long c_, long d, long e, long f) { shim_note("_GetWindowWidgetHilite"); return 0; }

long HideFloatingWindows(long a, long b, long c_, long d, long e, long f) __asm("_HideFloatingWindows");
long HideFloatingWindows(long a, long b, long c_, long d, long e, long f) { shim_note("_HideFloatingWindows"); return 0; }

long HiliteWindowFrameForDrag(long a, long b, long c_, long d, long e, long f) __asm("_HiliteWindowFrameForDrag");
long HiliteWindowFrameForDrag(long a, long b, long c_, long d, long e, long f) { shim_note("_HiliteWindowFrameForDrag"); return 0; }

long IsWindowCollapsable(long a, long b, long c_, long d, long e, long f) __asm("_IsWindowCollapsable");
long IsWindowCollapsable(long a, long b, long c_, long d, long e, long f) { shim_note("_IsWindowCollapsable"); return 0; }

long IsWindowModified(long a, long b, long c_, long d, long e, long f) __asm("_IsWindowModified");
long IsWindowModified(long a, long b, long c_, long d, long e, long f) { shim_note("_IsWindowModified"); return 0; }

long IsWindowPathSelectClick(long a, long b, long c_, long d, long e, long f) __asm("_IsWindowPathSelectClick");
long IsWindowPathSelectClick(long a, long b, long c_, long d, long e, long f) { shim_note("_IsWindowPathSelectClick"); return 0; }

long NavAskDiscardChanges(long a, long b, long c_, long d, long e, long f) __asm("_NavAskDiscardChanges");
long NavAskDiscardChanges(long a, long b, long c_, long d, long e, long f) { shim_note("_NavAskDiscardChanges"); return 0; }

long NavAskSaveChanges(long a, long b, long c_, long d, long e, long f) __asm("_NavAskSaveChanges");
long NavAskSaveChanges(long a, long b, long c_, long d, long e, long f) { shim_note("_NavAskSaveChanges"); return 0; }

long NavChooseVolume(long a, long b, long c_, long d, long e, long f) __asm("_NavChooseVolume");
long NavChooseVolume(long a, long b, long c_, long d, long e, long f) { shim_note("_NavChooseVolume"); return 0; }

long NavCustomAskSaveChanges(long a, long b, long c_, long d, long e, long f) __asm("_NavCustomAskSaveChanges");
long NavCustomAskSaveChanges(long a, long b, long c_, long d, long e, long f) { shim_note("_NavCustomAskSaveChanges"); return 0; }

long NavLibraryVersion(long a, long b, long c_, long d, long e, long f) __asm("_NavLibraryVersion");
long NavLibraryVersion(long a, long b, long c_, long d, long e, long f) { shim_note("_NavLibraryVersion"); return 0; }

long NavNewFolder(long a, long b, long c_, long d, long e, long f) __asm("_NavNewFolder");
long NavNewFolder(long a, long b, long c_, long d, long e, long f) { shim_note("_NavNewFolder"); return 0; }

long NavTranslateFile(long a, long b, long c_, long d, long e, long f) __asm("_NavTranslateFile");
long NavTranslateFile(long a, long b, long c_, long d, long e, long f) { shim_note("_NavTranslateFile"); return 0; }

long NewWindow(long a, long b, long c_, long d, long e, long f) __asm("_NewWindow");
long NewWindow(long a, long b, long c_, long d, long e, long f) { shim_note("_NewWindow"); return 0; }

long PaintBehind(long a, long b, long c_, long d, long e, long f) __asm("_PaintBehind");
long PaintBehind(long a, long b, long c_, long d, long e, long f) { shim_note("_PaintBehind"); return 0; }

long PaintOne(long a, long b, long c_, long d, long e, long f) __asm("_PaintOne");
long PaintOne(long a, long b, long c_, long d, long e, long f) { shim_note("_PaintOne"); return 0; }

long ReleaseWindow(long a, long b, long c_, long d, long e, long f) __asm("_ReleaseWindow");
long ReleaseWindow(long a, long b, long c_, long d, long e, long f) { shim_note("_ReleaseWindow"); return 0; }

long ReshapeCustomWindow(long a, long b, long c_, long d, long e, long f) __asm("_ReshapeCustomWindow");
long ReshapeCustomWindow(long a, long b, long c_, long d, long e, long f) { shim_note("_ReshapeCustomWindow"); return 0; }

long ResizeWindow(long a, long b, long c_, long d, long e, long f) __asm("_ResizeWindow");
long ResizeWindow(long a, long b, long c_, long d, long e, long f) { shim_note("_ResizeWindow"); return 0; }

long RetainWindow(long a, long b, long c_, long d, long e, long f) __asm("_RetainWindow");
long RetainWindow(long a, long b, long c_, long d, long e, long f) { shim_note("_RetainWindow"); return 0; }

long ScrollWindowRect(long a, long b, long c_, long d, long e, long f) __asm("_ScrollWindowRect");
long ScrollWindowRect(long a, long b, long c_, long d, long e, long f) { shim_note("_ScrollWindowRect"); return 0; }

long ScrollWindowRegion(long a, long b, long c_, long d, long e, long f) __asm("_ScrollWindowRegion");
long ScrollWindowRegion(long a, long b, long c_, long d, long e, long f) { shim_note("_ScrollWindowRegion"); return 0; }

long SetWindowClass(long a, long b, long c_, long d, long e, long f) __asm("_SetWindowClass");
long SetWindowClass(long a, long b, long c_, long d, long e, long f) { shim_note("_SetWindowClass"); return 0; }

long SetWindowContentPattern(long a, long b, long c_, long d, long e, long f) __asm("_SetWindowContentPattern");
long SetWindowContentPattern(long a, long b, long c_, long d, long e, long f) { shim_note("_SetWindowContentPattern"); return 0; }

long SetWindowPic(long a, long b, long c_, long d, long e, long f) __asm("_SetWindowPic");
long SetWindowPic(long a, long b, long c_, long d, long e, long f) { shim_note("_SetWindowPic"); return 0; }

long SetWindowProxyFSSpec(long a, long b, long c_, long d, long e, long f) __asm("_SetWindowProxyFSSpec");
long SetWindowProxyFSSpec(long a, long b, long c_, long d, long e, long f) { shim_note("_SetWindowProxyFSSpec"); return 0; }

long SetWindowStandardState(long a, long b, long c_, long d, long e, long f) __asm("_SetWindowStandardState");
long SetWindowStandardState(long a, long b, long c_, long d, long e, long f) { shim_note("_SetWindowStandardState"); return 0; }

long SetWindowUserState(long a, long b, long c_, long d, long e, long f) __asm("_SetWindowUserState");
long SetWindowUserState(long a, long b, long c_, long d, long e, long f) { shim_note("_SetWindowUserState"); return 0; }

long ShowFloatingWindows(long a, long b, long c_, long d, long e, long f) __asm("_ShowFloatingWindows");
long ShowFloatingWindows(long a, long b, long c_, long d, long e, long f) { shim_note("_ShowFloatingWindows"); return 0; }

long TrackBox(long a, long b, long c_, long d, long e, long f) __asm("_TrackBox");
long TrackBox(long a, long b, long c_, long d, long e, long f) { shim_note("_TrackBox"); return 0; }

long TrackGoAway(long a, long b, long c_, long d, long e, long f) __asm("_TrackGoAway");
long TrackGoAway(long a, long b, long c_, long d, long e, long f) { shim_note("_TrackGoAway"); return 0; }

long TrackWindowProxyDrag(long a, long b, long c_, long d, long e, long f) __asm("_TrackWindowProxyDrag");
long TrackWindowProxyDrag(long a, long b, long c_, long d, long e, long f) { shim_note("_TrackWindowProxyDrag"); return 0; }

long TransitionWindowAndParent(long a, long b, long c_, long d, long e, long f) __asm("_TransitionWindowAndParent");
long TransitionWindowAndParent(long a, long b, long c_, long d, long e, long f) { shim_note("_TransitionWindowAndParent"); return 0; }

long WindowPathSelect(long a, long b, long c_, long d, long e, long f) __asm("_WindowPathSelect");
long WindowPathSelect(long a, long b, long c_, long d, long e, long f) { shim_note("_WindowPathSelect"); return 0; }

long ZoomWindow(long a, long b, long c_, long d, long e, long f) __asm("_ZoomWindow");
long ZoomWindow(long a, long b, long c_, long d, long e, long f) { shim_note("_ZoomWindow"); return 0; }

long ZoomWindowIdeal(long a, long b, long c_, long d, long e, long f) __asm("_ZoomWindowIdeal");
long ZoomWindowIdeal(long a, long b, long c_, long d, long e, long f) { shim_note("_ZoomWindowIdeal"); return 0; }
