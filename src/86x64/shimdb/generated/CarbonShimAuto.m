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

long AreFloatingWindowsVisible(long a, long b, long c_, long d, long e, long f) { shim_note("AreFloatingWindowsVisible"); return 0; }

long BeginUpdate(long a, long b, long c_, long d, long e, long f) { shim_note("BeginUpdate"); return 0; }

long CalcVis(long a, long b, long c_, long d, long e, long f) { shim_note("CalcVis"); return 0; }

long CalcVisBehind(long a, long b, long c_, long d, long e, long f) { shim_note("CalcVisBehind"); return 0; }

long ChangeWindowPropertyAttributes(long a, long b, long c_, long d, long e, long f) { shim_note("ChangeWindowPropertyAttributes"); return 0; }

long CheckUpdate(long a, long b, long c_, long d, long e, long f) { shim_note("CheckUpdate"); return 0; }

long ClipAbove(long a, long b, long c_, long d, long e, long f) { shim_note("ClipAbove"); return 0; }

long CloneWindow(long a, long b, long c_, long d, long e, long f) { shim_note("CloneWindow"); return 0; }

long CopyWindowAlternateTitle(long a, long b, long c_, long d, long e, long f) { shim_note("CopyWindowAlternateTitle"); return 0; }

long CreateWindowFromResource(long a, long b, long c_, long d, long e, long f) { shim_note("CreateWindowFromResource"); return 0; }

long DrawGrowIcon(long a, long b, long c_, long d, long e, long f) { shim_note("DrawGrowIcon"); return 0; }

long EndUpdate(long a, long b, long c_, long d, long e, long f) { shim_note("EndUpdate"); return 0; }

long FindWindowOfClass(long a, long b, long c_, long d, long e, long f) { shim_note("FindWindowOfClass"); return 0; }

long GetAvailableWindowPositioningBounds(long a, long b, long c_, long d, long e, long f) { shim_note("GetAvailableWindowPositioningBounds"); return 0; }

long GetNewCWindow(long a, long b, long c_, long d, long e, long f) { shim_note("GetNewCWindow"); return 0; }

long GetNewWindow(long a, long b, long c_, long d, long e, long f) { shim_note("GetNewWindow"); return 0; }

long GetWTitle(long a, long b, long c_, long d, long e, long f) { shim_note("GetWTitle"); return 0; }

long GetWVariant(long a, long b, long c_, long d, long e, long f) { shim_note("GetWVariant"); return 0; }

long GetWindowContentColor(long a, long b, long c_, long d, long e, long f) { shim_note("GetWindowContentColor"); return 0; }

long GetWindowContentPattern(long a, long b, long c_, long d, long e, long f) { shim_note("GetWindowContentPattern"); return 0; }

long GetWindowFromPort(long a, long b, long c_, long d, long e, long f) { shim_note("GetWindowFromPort"); return 0; }

long GetWindowGreatestAreaDevice(long a, long b, long c_, long d, long e, long f) { shim_note("GetWindowGreatestAreaDevice"); return 0; }

long GetWindowOwnerCount(long a, long b, long c_, long d, long e, long f) { shim_note("GetWindowOwnerCount"); return 0; }

long GetWindowPic(long a, long b, long c_, long d, long e, long f) { shim_note("GetWindowPic"); return 0; }

long GetWindowPort(long a, long b, long c_, long d, long e, long f) { shim_note("GetWindowPort"); return 0; }

long GetWindowPropertyAttributes(long a, long b, long c_, long d, long e, long f) { shim_note("GetWindowPropertyAttributes"); return 0; }

long GetWindowProxyAlias(long a, long b, long c_, long d, long e, long f) { shim_note("GetWindowProxyAlias"); return 0; }

long GetWindowProxyFSSpec(long a, long b, long c_, long d, long e, long f) { shim_note("GetWindowProxyFSSpec"); return 0; }

long GetWindowProxyIcon(long a, long b, long c_, long d, long e, long f) { shim_note("GetWindowProxyIcon"); return 0; }

long GetWindowRegion(long a, long b, long c_, long d, long e, long f) { shim_note("GetWindowRegion"); return 0; }

long GetWindowRetainCount(long a, long b, long c_, long d, long e, long f) { shim_note("GetWindowRetainCount"); return 0; }

long GetWindowStructurePort(long a, long b, long c_, long d, long e, long f) { shim_note("GetWindowStructurePort"); return 0; }

long GetWindowStructureWidths(long a, long b, long c_, long d, long e, long f) { shim_note("GetWindowStructureWidths"); return 0; }

long GetWindowUserState(long a, long b, long c_, long d, long e, long f) { shim_note("GetWindowUserState"); return 0; }

long GetWindowWidgetHilite(long a, long b, long c_, long d, long e, long f) { shim_note("GetWindowWidgetHilite"); return 0; }

long GrowWindow(long a, long b, long c_, long d, long e, long f) { shim_note("GrowWindow"); return 0; }

long HideFloatingWindows(long a, long b, long c_, long d, long e, long f) { shim_note("HideFloatingWindows"); return 0; }

long HiliteWindowFrameForDrag(long a, long b, long c_, long d, long e, long f) { shim_note("HiliteWindowFrameForDrag"); return 0; }

long InvalWindowRect(long a, long b, long c_, long d, long e, long f) { shim_note("InvalWindowRect"); return 0; }

long InvalWindowRgn(long a, long b, long c_, long d, long e, long f) { shim_note("InvalWindowRgn"); return 0; }

long IsWindowCollapsable(long a, long b, long c_, long d, long e, long f) { shim_note("IsWindowCollapsable"); return 0; }

long IsWindowModified(long a, long b, long c_, long d, long e, long f) { shim_note("IsWindowModified"); return 0; }

long IsWindowPathSelectClick(long a, long b, long c_, long d, long e, long f) { shim_note("IsWindowPathSelectClick"); return 0; }

long IsWindowUpdatePending(long a, long b, long c_, long d, long e, long f) { shim_note("IsWindowUpdatePending"); return 0; }

long NavAskDiscardChanges(long a, long b, long c_, long d, long e, long f) { shim_note("NavAskDiscardChanges"); return 0; }

long NavAskSaveChanges(long a, long b, long c_, long d, long e, long f) { shim_note("NavAskSaveChanges"); return 0; }

long NavChooseFile(long a, long b, long c_, long d, long e, long f) { shim_note("NavChooseFile"); return 0; }

long NavChooseFolder(long a, long b, long c_, long d, long e, long f) { shim_note("NavChooseFolder"); return 0; }

long NavChooseObject(long a, long b, long c_, long d, long e, long f) { shim_note("NavChooseObject"); return 0; }

long NavChooseVolume(long a, long b, long c_, long d, long e, long f) { shim_note("NavChooseVolume"); return 0; }

long NavCompleteSave(long a, long b, long c_, long d, long e, long f) { shim_note("NavCompleteSave"); return 0; }

long NavCustomAskSaveChanges(long a, long b, long c_, long d, long e, long f) { shim_note("NavCustomAskSaveChanges"); return 0; }

long NavDisposeReply(long a, long b, long c_, long d, long e, long f) { shim_note("NavDisposeReply"); return 0; }

long NavGetDefaultDialogOptions(long a, long b, long c_, long d, long e, long f) { shim_note("NavGetDefaultDialogOptions"); return 0; }

long NavGetFile(long a, long b, long c_, long d, long e, long f) { shim_note("NavGetFile"); return 0; }

long NavLibraryVersion(long a, long b, long c_, long d, long e, long f) { shim_note("NavLibraryVersion"); return 0; }

long NavNewFolder(long a, long b, long c_, long d, long e, long f) { shim_note("NavNewFolder"); return 0; }

long NavPutFile(long a, long b, long c_, long d, long e, long f) { shim_note("NavPutFile"); return 0; }

long NavTranslateFile(long a, long b, long c_, long d, long e, long f) { shim_note("NavTranslateFile"); return 0; }

long NewCWindow(long a, long b, long c_, long d, long e, long f) { shim_note("NewCWindow"); return 0; }

long NewWindow(long a, long b, long c_, long d, long e, long f) { shim_note("NewWindow"); return 0; }

long PaintBehind(long a, long b, long c_, long d, long e, long f) { shim_note("PaintBehind"); return 0; }

long PaintOne(long a, long b, long c_, long d, long e, long f) { shim_note("PaintOne"); return 0; }

long PinRect(long a, long b, long c_, long d, long e, long f) { shim_note("PinRect"); return 0; }

long ReleaseWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ReleaseWindow"); return 0; }

long ReshapeCustomWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ReshapeCustomWindow"); return 0; }

long ResizeWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ResizeWindow"); return 0; }

long RetainWindow(long a, long b, long c_, long d, long e, long f) { shim_note("RetainWindow"); return 0; }

long ScrollWindowRect(long a, long b, long c_, long d, long e, long f) { shim_note("ScrollWindowRect"); return 0; }

long ScrollWindowRegion(long a, long b, long c_, long d, long e, long f) { shim_note("ScrollWindowRegion"); return 0; }

long SetPortWindowPort(long a, long b, long c_, long d, long e, long f) { shim_note("SetPortWindowPort"); return 0; }

long SetWRefCon(long a, long b, long c_, long d, long e, long f) { shim_note("SetWRefCon"); return 0; }

long SetWTitle(long a, long b, long c_, long d, long e, long f) { shim_note("SetWTitle"); return 0; }

long SetWindowClass(long a, long b, long c_, long d, long e, long f) { shim_note("SetWindowClass"); return 0; }

long SetWindowContentColor(long a, long b, long c_, long d, long e, long f) { shim_note("SetWindowContentColor"); return 0; }

long SetWindowContentPattern(long a, long b, long c_, long d, long e, long f) { shim_note("SetWindowContentPattern"); return 0; }

long SetWindowKind(long a, long b, long c_, long d, long e, long f) { shim_note("SetWindowKind"); return 0; }

long SetWindowPic(long a, long b, long c_, long d, long e, long f) { shim_note("SetWindowPic"); return 0; }

long SetWindowProxyCreatorAndType(long a, long b, long c_, long d, long e, long f) { shim_note("SetWindowProxyCreatorAndType"); return 0; }

long SetWindowProxyFSSpec(long a, long b, long c_, long d, long e, long f) { shim_note("SetWindowProxyFSSpec"); return 0; }

long SetWindowStandardState(long a, long b, long c_, long d, long e, long f) { shim_note("SetWindowStandardState"); return 0; }

long SetWindowUserState(long a, long b, long c_, long d, long e, long f) { shim_note("SetWindowUserState"); return 0; }

long ShowFloatingWindows(long a, long b, long c_, long d, long e, long f) { shim_note("ShowFloatingWindows"); return 0; }

long TrackBox(long a, long b, long c_, long d, long e, long f) { shim_note("TrackBox"); return 0; }

long TrackGoAway(long a, long b, long c_, long d, long e, long f) { shim_note("TrackGoAway"); return 0; }

long TrackWindowProxyDrag(long a, long b, long c_, long d, long e, long f) { shim_note("TrackWindowProxyDrag"); return 0; }

long TransitionWindowAndParent(long a, long b, long c_, long d, long e, long f) { shim_note("TransitionWindowAndParent"); return 0; }

long ValidWindowRect(long a, long b, long c_, long d, long e, long f) { shim_note("ValidWindowRect"); return 0; }

long ValidWindowRgn(long a, long b, long c_, long d, long e, long f) { shim_note("ValidWindowRgn"); return 0; }

long WindowPathSelect(long a, long b, long c_, long d, long e, long f) { shim_note("WindowPathSelect"); return 0; }

long ZoomWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZoomWindow"); return 0; }

long ZoomWindowIdeal(long a, long b, long c_, long d, long e, long f) { shim_note("ZoomWindowIdeal"); return 0; }
