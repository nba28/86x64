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
  fprintf(stderr, "[shimauto:%s] %s: removed-OS symbol CALLED with no curated impl -- returning 0 (write one in shimdb/impl/ if its behavior matters)\n", "ApplicationServices", sym);
  if (getenv("SHIMGEN_STUB_ABORT")) abort();
  pthread_mutex_unlock(&mtx);
}

long BackPat(long a, long b, long c_, long d, long e, long f) __asm("_BackPat");
long BackPat(long a, long b, long c_, long d, long e, long f) { shim_note("_BackPat"); return 0; }

long CGSGetClientWindowCount(long a, long b, long c_, long d, long e, long f) __asm("_CGSGetClientWindowCount");
long CGSGetClientWindowCount(long a, long b, long c_, long d, long e, long f) { shim_note("_CGSGetClientWindowCount"); return 0; }

long CGSGetClientWindowList(long a, long b, long c_, long d, long e, long f) __asm("_CGSGetClientWindowList");
long CGSGetClientWindowList(long a, long b, long c_, long d, long e, long f) { shim_note("_CGSGetClientWindowList"); return 0; }

long CGSReleaseObj(long a, long b, long c_, long d, long e, long f) __asm("_CGSReleaseObj");
long CGSReleaseObj(long a, long b, long c_, long d, long e, long f) { shim_note("_CGSReleaseObj"); return 0; }

long CallDrawingNotifications(long a, long b, long c_, long d, long e, long f) __asm("_CallDrawingNotifications");
long CallDrawingNotifications(long a, long b, long c_, long d, long e, long f) { shim_note("_CallDrawingNotifications"); return 0; }

long CharWidth(long a, long b, long c_, long d, long e, long f) __asm("_CharWidth");
long CharWidth(long a, long b, long c_, long d, long e, long f) { shim_note("_CharWidth"); return 0; }

long CheckPictureRecording(long a, long b, long c_, long d, long e, long f) __asm("_CheckPictureRecording");
long CheckPictureRecording(long a, long b, long c_, long d, long e, long f) { shim_note("_CheckPictureRecording"); return 0; }

long ClosePoly(long a, long b, long c_, long d, long e, long f) __asm("_ClosePoly");
long ClosePoly(long a, long b, long c_, long d, long e, long f) { shim_note("_ClosePoly"); return 0; }

long CloseRgn(long a, long b, long c_, long d, long e, long f) __asm("_CloseRgn");
long CloseRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_CloseRgn"); return 0; }

long Color2Index(long a, long b, long c_, long d, long e, long f) __asm("_Color2Index");
long Color2Index(long a, long b, long c_, long d, long e, long f) { shim_note("_Color2Index"); return 0; }

long CopyMask(long a, long b, long c_, long d, long e, long f) __asm("_CopyMask");
long CopyMask(long a, long b, long c_, long d, long e, long f) { shim_note("_CopyMask"); return 0; }

long CopyPixMap(long a, long b, long c_, long d, long e, long f) __asm("_CopyPixMap");
long CopyPixMap(long a, long b, long c_, long d, long e, long f) { shim_note("_CopyPixMap"); return 0; }

long DrawString(long a, long b, long c_, long d, long e, long f) __asm("_DrawString");
long DrawString(long a, long b, long c_, long d, long e, long f) { shim_note("_DrawString"); return 0; }

long EraseRgn(long a, long b, long c_, long d, long e, long f) __asm("_EraseRgn");
long EraseRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_EraseRgn"); return 0; }

long FillRgn(long a, long b, long c_, long d, long e, long f) __asm("_FillRgn");
long FillRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_FillRgn"); return 0; }

long FrameArc(long a, long b, long c_, long d, long e, long f) __asm("_FrameArc");
long FrameArc(long a, long b, long c_, long d, long e, long f) { shim_note("_FrameArc"); return 0; }

long FrameOval(long a, long b, long c_, long d, long e, long f) __asm("_FrameOval");
long FrameOval(long a, long b, long c_, long d, long e, long f) { shim_note("_FrameOval"); return 0; }

long FramePoly(long a, long b, long c_, long d, long e, long f) __asm("_FramePoly");
long FramePoly(long a, long b, long c_, long d, long e, long f) { shim_note("_FramePoly"); return 0; }

long FrameRgn(long a, long b, long c_, long d, long e, long f) __asm("_FrameRgn");
long FrameRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_FrameRgn"); return 0; }

long GDeviceChanged(long a, long b, long c_, long d, long e, long f) __asm("_GDeviceChanged");
long GDeviceChanged(long a, long b, long c_, long d, long e, long f) { shim_note("_GDeviceChanged"); return 0; }

long GetFNum(long a, long b, long c_, long d, long e, long f) __asm("_GetFNum");
long GetFNum(long a, long b, long c_, long d, long e, long f) { shim_note("_GetFNum"); return 0; }

long GetGDevice(long a, long b, long c_, long d, long e, long f) __asm("_GetGDevice");
long GetGDevice(long a, long b, long c_, long d, long e, long f) { shim_note("_GetGDevice"); return 0; }

long GetPixMapData(long a, long b, long c_, long d, long e, long f) __asm("_GetPixMapData");
long GetPixMapData(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPixMapData"); return 0; }

long GetPixelsState(long a, long b, long c_, long d, long e, long f) __asm("_GetPixelsState");
long GetPixelsState(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPixelsState"); return 0; }

long GetPortBackPixPat(long a, long b, long c_, long d, long e, long f) __asm("_GetPortBackPixPat");
long GetPortBackPixPat(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPortBackPixPat"); return 0; }

long GetPortGrafProcs(long a, long b, long c_, long d, long e, long f) __asm("_GetPortGrafProcs");
long GetPortGrafProcs(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPortGrafProcs"); return 0; }

long GetPortHiliteColor(long a, long b, long c_, long d, long e, long f) __asm("_GetPortHiliteColor");
long GetPortHiliteColor(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPortHiliteColor"); return 0; }

long GetPortOpColor(long a, long b, long c_, long d, long e, long f) __asm("_GetPortOpColor");
long GetPortOpColor(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPortOpColor"); return 0; }

long GetPortPenLocation(long a, long b, long c_, long d, long e, long f) __asm("_GetPortPenLocation");
long GetPortPenLocation(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPortPenLocation"); return 0; }

long GetPortPenPixPat(long a, long b, long c_, long d, long e, long f) __asm("_GetPortPenPixPat");
long GetPortPenPixPat(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPortPenPixPat"); return 0; }

long GetPortPenSize(long a, long b, long c_, long d, long e, long f) __asm("_GetPortPenSize");
long GetPortPenSize(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPortPenSize"); return 0; }

long GetPortPenVisibility(long a, long b, long c_, long d, long e, long f) __asm("_GetPortPenVisibility");
long GetPortPenVisibility(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPortPenVisibility"); return 0; }

long GetPortTextMode(long a, long b, long c_, long d, long e, long f) __asm("_GetPortTextMode");
long GetPortTextMode(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPortTextMode"); return 0; }

long GetPortTextSize(long a, long b, long c_, long d, long e, long f) __asm("_GetPortTextSize");
long GetPortTextSize(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPortTextSize"); return 0; }

long GetQDGlobalsScreenBits(long a, long b, long c_, long d, long e, long f) __asm("_GetQDGlobalsScreenBits");
long GetQDGlobalsScreenBits(long a, long b, long c_, long d, long e, long f) { shim_note("_GetQDGlobalsScreenBits"); return 0; }

long GetQDPlayIndex(long a, long b, long c_, long d, long e, long f) __asm("_GetQDPlayIndex");
long GetQDPlayIndex(long a, long b, long c_, long d, long e, long f) { shim_note("_GetQDPlayIndex"); return 0; }

long HasDepth(long a, long b, long c_, long d, long e, long f) __asm("_HasDepth");
long HasDepth(long a, long b, long c_, long d, long e, long f) { shim_note("_HasDepth"); return 0; }

long Index2Color(long a, long b, long c_, long d, long e, long f) __asm("_Index2Color");
long Index2Color(long a, long b, long c_, long d, long e, long f) { shim_note("_Index2Color"); return 0; }

long IsPortOffscreen(long a, long b, long c_, long d, long e, long f) __asm("_IsPortOffscreen");
long IsPortOffscreen(long a, long b, long c_, long d, long e, long f) { shim_note("_IsPortOffscreen"); return 0; }

long IsPortPictureBeingDefined(long a, long b, long c_, long d, long e, long f) __asm("_IsPortPictureBeingDefined");
long IsPortPictureBeingDefined(long a, long b, long c_, long d, long e, long f) { shim_note("_IsPortPictureBeingDefined"); return 0; }

long KillPoly(long a, long b, long c_, long d, long e, long f) __asm("_KillPoly");
long KillPoly(long a, long b, long c_, long d, long e, long f) { shim_note("_KillPoly"); return 0; }

long LMGetDeviceList(long a, long b, long c_, long d, long e, long f) __asm("_LMGetDeviceList");
long LMGetDeviceList(long a, long b, long c_, long d, long e, long f) { shim_note("_LMGetDeviceList"); return 0; }

long LMGetMainDevice(long a, long b, long c_, long d, long e, long f) __asm("_LMGetMainDevice");
long LMGetMainDevice(long a, long b, long c_, long d, long e, long f) { shim_note("_LMGetMainDevice"); return 0; }

long LMGetQDErr(long a, long b, long c_, long d, long e, long f) __asm("_LMGetQDErr");
long LMGetQDErr(long a, long b, long c_, long d, long e, long f) { shim_note("_LMGetQDErr"); return 0; }

long LMGetTheGDevice(long a, long b, long c_, long d, long e, long f) __asm("_LMGetTheGDevice");
long LMGetTheGDevice(long a, long b, long c_, long d, long e, long f) { shim_note("_LMGetTheGDevice"); return 0; }

long LMSetQDErr(long a, long b, long c_, long d, long e, long f) __asm("_LMSetQDErr");
long LMSetQDErr(long a, long b, long c_, long d, long e, long f) { shim_note("_LMSetQDErr"); return 0; }

long LMSetTheGDevice(long a, long b, long c_, long d, long e, long f) __asm("_LMSetTheGDevice");
long LMSetTheGDevice(long a, long b, long c_, long d, long e, long f) { shim_note("_LMSetTheGDevice"); return 0; }

long Line(long a, long b, long c_, long d, long e, long f) __asm("_Line");
long Line(long a, long b, long c_, long d, long e, long f) { shim_note("_Line"); return 0; }

long LockPortBits(long a, long b, long c_, long d, long e, long f) __asm("_LockPortBits");
long LockPortBits(long a, long b, long c_, long d, long e, long f) { shim_note("_LockPortBits"); return 0; }

long MakeITable(long a, long b, long c_, long d, long e, long f) __asm("_MakeITable");
long MakeITable(long a, long b, long c_, long d, long e, long f) { shim_note("_MakeITable"); return 0; }

long MapQDRectToCGSRect(long a, long b, long c_, long d, long e, long f) __asm("_MapQDRectToCGSRect");
long MapQDRectToCGSRect(long a, long b, long c_, long d, long e, long f) { shim_note("_MapQDRectToCGSRect"); return 0; }

long Move(long a, long b, long c_, long d, long e, long f) __asm("_Move");
long Move(long a, long b, long c_, long d, long e, long f) { shim_note("_Move"); return 0; }

long NQDMisc(long a, long b, long c_, long d, long e, long f) __asm("_NQDMisc");
long NQDMisc(long a, long b, long c_, long d, long e, long f) { shim_note("_NQDMisc"); return 0; }

long NewPixMap(long a, long b, long c_, long d, long e, long f) __asm("_NewPixMap");
long NewPixMap(long a, long b, long c_, long d, long e, long f) { shim_note("_NewPixMap"); return 0; }

long NewPixPat(long a, long b, long c_, long d, long e, long f) __asm("_NewPixPat");
long NewPixPat(long a, long b, long c_, long d, long e, long f) { shim_note("_NewPixPat"); return 0; }

long OpColor(long a, long b, long c_, long d, long e, long f) __asm("_OpColor");
long OpColor(long a, long b, long c_, long d, long e, long f) { shim_note("_OpColor"); return 0; }

long OpenPicture(long a, long b, long c_, long d, long e, long f) __asm("_OpenPicture");
long OpenPicture(long a, long b, long c_, long d, long e, long f) { shim_note("_OpenPicture"); return 0; }

long OpenPoly(long a, long b, long c_, long d, long e, long f) __asm("_OpenPoly");
long OpenPoly(long a, long b, long c_, long d, long e, long f) { shim_note("_OpenPoly"); return 0; }

long OpenRgn(long a, long b, long c_, long d, long e, long f) __asm("_OpenRgn");
long OpenRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_OpenRgn"); return 0; }

long PaintOval(long a, long b, long c_, long d, long e, long f) __asm("_PaintOval");
long PaintOval(long a, long b, long c_, long d, long e, long f) { shim_note("_PaintOval"); return 0; }

long PaintPoly(long a, long b, long c_, long d, long e, long f) __asm("_PaintPoly");
long PaintPoly(long a, long b, long c_, long d, long e, long f) { shim_note("_PaintPoly"); return 0; }

long PaintRgn(long a, long b, long c_, long d, long e, long f) __asm("_PaintRgn");
long PaintRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_PaintRgn"); return 0; }

long PenMode(long a, long b, long c_, long d, long e, long f) __asm("_PenMode");
long PenMode(long a, long b, long c_, long d, long e, long f) { shim_note("_PenMode"); return 0; }

long PenPat(long a, long b, long c_, long d, long e, long f) __asm("_PenPat");
long PenPat(long a, long b, long c_, long d, long e, long f) { shim_note("_PenPat"); return 0; }

long PenPixPat(long a, long b, long c_, long d, long e, long f) __asm("_PenPixPat");
long PenPixPat(long a, long b, long c_, long d, long e, long f) { shim_note("_PenPixPat"); return 0; }

long PlotIconRef(long a, long b, long c_, long d, long e, long f) __asm("_PlotIconRef");
long PlotIconRef(long a, long b, long c_, long d, long e, long f) { shim_note("_PlotIconRef"); return 0; }

long PortChanged(long a, long b, long c_, long d, long e, long f) __asm("_PortChanged");
long PortChanged(long a, long b, long c_, long d, long e, long f) { shim_note("_PortChanged"); return 0; }

long PortSize(long a, long b, long c_, long d, long e, long f) __asm("_PortSize");
long PortSize(long a, long b, long c_, long d, long e, long f) { shim_note("_PortSize"); return 0; }

long ProtectCursor(long a, long b, long c_, long d, long e, long f) __asm("_ProtectCursor");
long ProtectCursor(long a, long b, long c_, long d, long e, long f) { shim_note("_ProtectCursor"); return 0; }

long PutPicOp(long a, long b, long c_, long d, long e, long f) __asm("_PutPicOp");
long PutPicOp(long a, long b, long c_, long d, long e, long f) { shim_note("_PutPicOp"); return 0; }

long QDDone(long a, long b, long c_, long d, long e, long f) __asm("_QDDone");
long QDDone(long a, long b, long c_, long d, long e, long f) { shim_note("_QDDone"); return 0; }

long QDError(long a, long b, long c_, long d, long e, long f) __asm("_QDError");
long QDError(long a, long b, long c_, long d, long e, long f) { shim_note("_QDError"); return 0; }

long QDGetDirtyRegion(long a, long b, long c_, long d, long e, long f) __asm("_QDGetDirtyRegion");
long QDGetDirtyRegion(long a, long b, long c_, long d, long e, long f) { shim_note("_QDGetDirtyRegion"); return 0; }

long QDGetNativeWindowFromPort(long a, long b, long c_, long d, long e, long f) __asm("_QDGetNativeWindowFromPort");
long QDGetNativeWindowFromPort(long a, long b, long c_, long d, long e, long f) { shim_note("_QDGetNativeWindowFromPort"); return 0; }

long QDIsPortBuffered(long a, long b, long c_, long d, long e, long f) __asm("_QDIsPortBuffered");
long QDIsPortBuffered(long a, long b, long c_, long d, long e, long f) { shim_note("_QDIsPortBuffered"); return 0; }

long QDSetDirtyRegion(long a, long b, long c_, long d, long e, long f) __asm("_QDSetDirtyRegion");
long QDSetDirtyRegion(long a, long b, long c_, long d, long e, long f) { shim_note("_QDSetDirtyRegion"); return 0; }

long Random(long a, long b, long c_, long d, long e, long f) __asm("_Random");
long Random(long a, long b, long c_, long d, long e, long f) { shim_note("_Random"); return 0; }

long RegisterDrawingNotification(long a, long b, long c_, long d, long e, long f) __asm("_RegisterDrawingNotification");
long RegisterDrawingNotification(long a, long b, long c_, long d, long e, long f) { shim_note("_RegisterDrawingNotification"); return 0; }

long SetDrawingNotificationRect(long a, long b, long c_, long d, long e, long f) __asm("_SetDrawingNotificationRect");
long SetDrawingNotificationRect(long a, long b, long c_, long d, long e, long f) { shim_note("_SetDrawingNotificationRect"); return 0; }

long SetPixelsState(long a, long b, long c_, long d, long e, long f) __asm("_SetPixelsState");
long SetPixelsState(long a, long b, long c_, long d, long e, long f) { shim_note("_SetPixelsState"); return 0; }

long SetPortBackPixPat(long a, long b, long c_, long d, long e, long f) __asm("_SetPortBackPixPat");
long SetPortBackPixPat(long a, long b, long c_, long d, long e, long f) { shim_note("_SetPortBackPixPat"); return 0; }

long SetPortClipRegion(long a, long b, long c_, long d, long e, long f) __asm("_SetPortClipRegion");
long SetPortClipRegion(long a, long b, long c_, long d, long e, long f) { shim_note("_SetPortClipRegion"); return 0; }

long SetPortGrafProcs(long a, long b, long c_, long d, long e, long f) __asm("_SetPortGrafProcs");
long SetPortGrafProcs(long a, long b, long c_, long d, long e, long f) { shim_note("_SetPortGrafProcs"); return 0; }

long SetPortPix(long a, long b, long c_, long d, long e, long f) __asm("_SetPortPix");
long SetPortPix(long a, long b, long c_, long d, long e, long f) { shim_note("_SetPortPix"); return 0; }

long SetPortVisibleRegion(long a, long b, long c_, long d, long e, long f) __asm("_SetPortVisibleRegion");
long SetPortVisibleRegion(long a, long b, long c_, long d, long e, long f) { shim_note("_SetPortVisibleRegion"); return 0; }

long StdBits(long a, long b, long c_, long d, long e, long f) __asm("_StdBits");
long StdBits(long a, long b, long c_, long d, long e, long f) { shim_note("_StdBits"); return 0; }

long StretchBits(long a, long b, long c_, long d, long e, long f) __asm("_StretchBits");
long StretchBits(long a, long b, long c_, long d, long e, long f) { shim_note("_StretchBits"); return 0; }

long StringWidth(long a, long b, long c_, long d, long e, long f) __asm("_StringWidth");
long StringWidth(long a, long b, long c_, long d, long e, long f) { shim_note("_StringWidth"); return 0; }

long TextMode(long a, long b, long c_, long d, long e, long f) __asm("_TextMode");
long TextMode(long a, long b, long c_, long d, long e, long f) { shim_note("_TextMode"); return 0; }

long TrimRect(long a, long b, long c_, long d, long e, long f) __asm("_TrimRect");
long TrimRect(long a, long b, long c_, long d, long e, long f) { shim_note("_TrimRect"); return 0; }

long TruncString(long a, long b, long c_, long d, long e, long f) __asm("_TruncString");
long TruncString(long a, long b, long c_, long d, long e, long f) { shim_note("_TruncString"); return 0; }

long UnlockPortBits(long a, long b, long c_, long d, long e, long f) __asm("_UnlockPortBits");
long UnlockPortBits(long a, long b, long c_, long d, long e, long f) { shim_note("_UnlockPortBits"); return 0; }

long UnprotectCursor(long a, long b, long c_, long d, long e, long f) __asm("_UnprotectCursor");
long UnprotectCursor(long a, long b, long c_, long d, long e, long f) { shim_note("_UnprotectCursor"); return 0; }

long UnregisterDrawingNotification(long a, long b, long c_, long d, long e, long f) __asm("_UnregisterDrawingNotification");
long UnregisterDrawingNotification(long a, long b, long c_, long d, long e, long f) { shim_note("_UnregisterDrawingNotification"); return 0; }

long UpdateGWorld(long a, long b, long c_, long d, long e, long f) __asm("_UpdateGWorld");
long UpdateGWorld(long a, long b, long c_, long d, long e, long f) { shim_note("_UpdateGWorld"); return 0; }
