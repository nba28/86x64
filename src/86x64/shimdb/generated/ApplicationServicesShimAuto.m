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

long AddIconToSuite(long a, long b, long c_, long d, long e, long f) __asm("_AddIconToSuite");
long AddIconToSuite(long a, long b, long c_, long d, long e, long f) { shim_note("_AddIconToSuite"); return 0; }

long AllocCursor(long a, long b, long c_, long d, long e, long f) __asm("_AllocCursor");
long AllocCursor(long a, long b, long c_, long d, long e, long f) { shim_note("_AllocCursor"); return 0; }

long AllowPurgePixels(long a, long b, long c_, long d, long e, long f) __asm("_AllowPurgePixels");
long AllowPurgePixels(long a, long b, long c_, long d, long e, long f) { shim_note("_AllowPurgePixels"); return 0; }

long BackPixPat(long a, long b, long c_, long d, long e, long f) __asm("_BackPixPat");
long BackPixPat(long a, long b, long c_, long d, long e, long f) { shim_note("_BackPixPat"); return 0; }

long CTabChanged(long a, long b, long c_, long d, long e, long f) __asm("_CTabChanged");
long CTabChanged(long a, long b, long c_, long d, long e, long f) { shim_note("_CTabChanged"); return 0; }

long CharExtra(long a, long b, long c_, long d, long e, long f) __asm("_CharExtra");
long CharExtra(long a, long b, long c_, long d, long e, long f) { shim_note("_CharExtra"); return 0; }

long CharWidth(long a, long b, long c_, long d, long e, long f) __asm("_CharWidth");
long CharWidth(long a, long b, long c_, long d, long e, long f) { shim_note("_CharWidth"); return 0; }

long ClosePoly(long a, long b, long c_, long d, long e, long f) __asm("_ClosePoly");
long ClosePoly(long a, long b, long c_, long d, long e, long f) { shim_note("_ClosePoly"); return 0; }

long CloseRgn(long a, long b, long c_, long d, long e, long f) __asm("_CloseRgn");
long CloseRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_CloseRgn"); return 0; }

long Color2Index(long a, long b, long c_, long d, long e, long f) __asm("_Color2Index");
long Color2Index(long a, long b, long c_, long d, long e, long f) { shim_note("_Color2Index"); return 0; }

long ColorBit(long a, long b, long c_, long d, long e, long f) __asm("_ColorBit");
long ColorBit(long a, long b, long c_, long d, long e, long f) { shim_note("_ColorBit"); return 0; }

long CopyPixMap(long a, long b, long c_, long d, long e, long f) __asm("_CopyPixMap");
long CopyPixMap(long a, long b, long c_, long d, long e, long f) { shim_note("_CopyPixMap"); return 0; }

long CopyPixPat(long a, long b, long c_, long d, long e, long f) __asm("_CopyPixPat");
long CopyPixPat(long a, long b, long c_, long d, long e, long f) { shim_note("_CopyPixPat"); return 0; }

long DeltaPoint(long a, long b, long c_, long d, long e, long f) __asm("_DeltaPoint");
long DeltaPoint(long a, long b, long c_, long d, long e, long f) { shim_note("_DeltaPoint"); return 0; }

long DisposeCCursor(long a, long b, long c_, long d, long e, long f) __asm("_DisposeCCursor");
long DisposeCCursor(long a, long b, long c_, long d, long e, long f) { shim_note("_DisposeCCursor"); return 0; }

long DisposeCIcon(long a, long b, long c_, long d, long e, long f) __asm("_DisposeCIcon");
long DisposeCIcon(long a, long b, long c_, long d, long e, long f) { shim_note("_DisposeCIcon"); return 0; }

long DisposeGDevice(long a, long b, long c_, long d, long e, long f) __asm("_DisposeGDevice");
long DisposeGDevice(long a, long b, long c_, long d, long e, long f) { shim_note("_DisposeGDevice"); return 0; }

long DisposeIconSuite(long a, long b, long c_, long d, long e, long f) __asm("_DisposeIconSuite");
long DisposeIconSuite(long a, long b, long c_, long d, long e, long f) { shim_note("_DisposeIconSuite"); return 0; }

long DisposeScreenBuffer(long a, long b, long c_, long d, long e, long f) __asm("_DisposeScreenBuffer");
long DisposeScreenBuffer(long a, long b, long c_, long d, long e, long f) { shim_note("_DisposeScreenBuffer"); return 0; }

long DrawChar(long a, long b, long c_, long d, long e, long f) __asm("_DrawChar");
long DrawChar(long a, long b, long c_, long d, long e, long f) { shim_note("_DrawChar"); return 0; }

long DrawString(long a, long b, long c_, long d, long e, long f) __asm("_DrawString");
long DrawString(long a, long b, long c_, long d, long e, long f) { shim_note("_DrawString"); return 0; }

long EraseArc(long a, long b, long c_, long d, long e, long f) __asm("_EraseArc");
long EraseArc(long a, long b, long c_, long d, long e, long f) { shim_note("_EraseArc"); return 0; }

long EraseOval(long a, long b, long c_, long d, long e, long f) __asm("_EraseOval");
long EraseOval(long a, long b, long c_, long d, long e, long f) { shim_note("_EraseOval"); return 0; }

long ErasePoly(long a, long b, long c_, long d, long e, long f) __asm("_ErasePoly");
long ErasePoly(long a, long b, long c_, long d, long e, long f) { shim_note("_ErasePoly"); return 0; }

long EraseRoundRect(long a, long b, long c_, long d, long e, long f) __asm("_EraseRoundRect");
long EraseRoundRect(long a, long b, long c_, long d, long e, long f) { shim_note("_EraseRoundRect"); return 0; }

long FillArc(long a, long b, long c_, long d, long e, long f) __asm("_FillArc");
long FillArc(long a, long b, long c_, long d, long e, long f) { shim_note("_FillArc"); return 0; }

long FillCArc(long a, long b, long c_, long d, long e, long f) __asm("_FillCArc");
long FillCArc(long a, long b, long c_, long d, long e, long f) { shim_note("_FillCArc"); return 0; }

long FillCOval(long a, long b, long c_, long d, long e, long f) __asm("_FillCOval");
long FillCOval(long a, long b, long c_, long d, long e, long f) { shim_note("_FillCOval"); return 0; }

long FillCPoly(long a, long b, long c_, long d, long e, long f) __asm("_FillCPoly");
long FillCPoly(long a, long b, long c_, long d, long e, long f) { shim_note("_FillCPoly"); return 0; }

long FillCRect(long a, long b, long c_, long d, long e, long f) __asm("_FillCRect");
long FillCRect(long a, long b, long c_, long d, long e, long f) { shim_note("_FillCRect"); return 0; }

long FillCRgn(long a, long b, long c_, long d, long e, long f) __asm("_FillCRgn");
long FillCRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_FillCRgn"); return 0; }

long FillCRoundRect(long a, long b, long c_, long d, long e, long f) __asm("_FillCRoundRect");
long FillCRoundRect(long a, long b, long c_, long d, long e, long f) { shim_note("_FillCRoundRect"); return 0; }

long FillOval(long a, long b, long c_, long d, long e, long f) __asm("_FillOval");
long FillOval(long a, long b, long c_, long d, long e, long f) { shim_note("_FillOval"); return 0; }

long FillPoly(long a, long b, long c_, long d, long e, long f) __asm("_FillPoly");
long FillPoly(long a, long b, long c_, long d, long e, long f) { shim_note("_FillPoly"); return 0; }

long FillRoundRect(long a, long b, long c_, long d, long e, long f) __asm("_FillRoundRect");
long FillRoundRect(long a, long b, long c_, long d, long e, long f) { shim_note("_FillRoundRect"); return 0; }

long FontMetrics(long a, long b, long c_, long d, long e, long f) __asm("_FontMetrics");
long FontMetrics(long a, long b, long c_, long d, long e, long f) { shim_note("_FontMetrics"); return 0; }

long FrameArc(long a, long b, long c_, long d, long e, long f) __asm("_FrameArc");
long FrameArc(long a, long b, long c_, long d, long e, long f) { shim_note("_FrameArc"); return 0; }

long FrameOval(long a, long b, long c_, long d, long e, long f) __asm("_FrameOval");
long FrameOval(long a, long b, long c_, long d, long e, long f) { shim_note("_FrameOval"); return 0; }

long FramePoly(long a, long b, long c_, long d, long e, long f) __asm("_FramePoly");
long FramePoly(long a, long b, long c_, long d, long e, long f) { shim_note("_FramePoly"); return 0; }

long FrameRoundRect(long a, long b, long c_, long d, long e, long f) __asm("_FrameRoundRect");
long FrameRoundRect(long a, long b, long c_, long d, long e, long f) { shim_note("_FrameRoundRect"); return 0; }

long GDeviceChanged(long a, long b, long c_, long d, long e, long f) __asm("_GDeviceChanged");
long GDeviceChanged(long a, long b, long c_, long d, long e, long f) { shim_note("_GDeviceChanged"); return 0; }

long GetCCursor(long a, long b, long c_, long d, long e, long f) __asm("_GetCCursor");
long GetCCursor(long a, long b, long c_, long d, long e, long f) { shim_note("_GetCCursor"); return 0; }

long GetCIcon(long a, long b, long c_, long d, long e, long f) __asm("_GetCIcon");
long GetCIcon(long a, long b, long c_, long d, long e, long f) { shim_note("_GetCIcon"); return 0; }

long GetCPixel(long a, long b, long c_, long d, long e, long f) __asm("_GetCPixel");
long GetCPixel(long a, long b, long c_, long d, long e, long f) { shim_note("_GetCPixel"); return 0; }

long GetDefFontSize(long a, long b, long c_, long d, long e, long f) __asm("_GetDefFontSize");
long GetDefFontSize(long a, long b, long c_, long d, long e, long f) { shim_note("_GetDefFontSize"); return 0; }

long GetFNum(long a, long b, long c_, long d, long e, long f) __asm("_GetFNum");
long GetFNum(long a, long b, long c_, long d, long e, long f) { shim_note("_GetFNum"); return 0; }

long GetFontName(long a, long b, long c_, long d, long e, long f) __asm("_GetFontName");
long GetFontName(long a, long b, long c_, long d, long e, long f) { shim_note("_GetFontName"); return 0; }

long GetIcon(long a, long b, long c_, long d, long e, long f) __asm("_GetIcon");
long GetIcon(long a, long b, long c_, long d, long e, long f) { shim_note("_GetIcon"); return 0; }

long GetIconFromSuite(long a, long b, long c_, long d, long e, long f) __asm("_GetIconFromSuite");
long GetIconFromSuite(long a, long b, long c_, long d, long e, long f) { shim_note("_GetIconFromSuite"); return 0; }

long GetIconSizesFromIconRef(long a, long b, long c_, long d, long e, long f) __asm("_GetIconSizesFromIconRef");
long GetIconSizesFromIconRef(long a, long b, long c_, long d, long e, long f) { shim_note("_GetIconSizesFromIconRef"); return 0; }

long GetIconSuite(long a, long b, long c_, long d, long e, long f) __asm("_GetIconSuite");
long GetIconSuite(long a, long b, long c_, long d, long e, long f) { shim_note("_GetIconSuite"); return 0; }

long GetIndPattern(long a, long b, long c_, long d, long e, long f) __asm("_GetIndPattern");
long GetIndPattern(long a, long b, long c_, long d, long e, long f) { shim_note("_GetIndPattern"); return 0; }

long GetLabel(long a, long b, long c_, long d, long e, long f) __asm("_GetLabel");
long GetLabel(long a, long b, long c_, long d, long e, long f) { shim_note("_GetLabel"); return 0; }

long GetMaxDevice(long a, long b, long c_, long d, long e, long f) __asm("_GetMaxDevice");
long GetMaxDevice(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMaxDevice"); return 0; }

long GetOutlinePreferred(long a, long b, long c_, long d, long e, long f) __asm("_GetOutlinePreferred");
long GetOutlinePreferred(long a, long b, long c_, long d, long e, long f) { shim_note("_GetOutlinePreferred"); return 0; }

long GetPattern(long a, long b, long c_, long d, long e, long f) __asm("_GetPattern");
long GetPattern(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPattern"); return 0; }

long GetPen(long a, long b, long c_, long d, long e, long f) __asm("_GetPen");
long GetPen(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPen"); return 0; }

long GetPixDepth(long a, long b, long c_, long d, long e, long f) __asm("_GetPixDepth");
long GetPixDepth(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPixDepth"); return 0; }

long GetPixPat(long a, long b, long c_, long d, long e, long f) __asm("_GetPixPat");
long GetPixPat(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPixPat"); return 0; }

long GetPixel(long a, long b, long c_, long d, long e, long f) __asm("_GetPixel");
long GetPixel(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPixel"); return 0; }

long GetPixelsState(long a, long b, long c_, long d, long e, long f) __asm("_GetPixelsState");
long GetPixelsState(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPixelsState"); return 0; }

long GetPortBackPixPat(long a, long b, long c_, long d, long e, long f) __asm("_GetPortBackPixPat");
long GetPortBackPixPat(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPortBackPixPat"); return 0; }

long GetPortChExtra(long a, long b, long c_, long d, long e, long f) __asm("_GetPortChExtra");
long GetPortChExtra(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPortChExtra"); return 0; }

long GetPortFillPixPat(long a, long b, long c_, long d, long e, long f) __asm("_GetPortFillPixPat");
long GetPortFillPixPat(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPortFillPixPat"); return 0; }

long GetPortFracHPenLocation(long a, long b, long c_, long d, long e, long f) __asm("_GetPortFracHPenLocation");
long GetPortFracHPenLocation(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPortFracHPenLocation"); return 0; }

long GetPortHiliteColor(long a, long b, long c_, long d, long e, long f) __asm("_GetPortHiliteColor");
long GetPortHiliteColor(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPortHiliteColor"); return 0; }

long GetPortOpColor(long a, long b, long c_, long d, long e, long f) __asm("_GetPortOpColor");
long GetPortOpColor(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPortOpColor"); return 0; }

long GetPortPenLocation(long a, long b, long c_, long d, long e, long f) __asm("_GetPortPenLocation");
long GetPortPenLocation(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPortPenLocation"); return 0; }

long GetPortPenMode(long a, long b, long c_, long d, long e, long f) __asm("_GetPortPenMode");
long GetPortPenMode(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPortPenMode"); return 0; }

long GetPortPenPixPat(long a, long b, long c_, long d, long e, long f) __asm("_GetPortPenPixPat");
long GetPortPenPixPat(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPortPenPixPat"); return 0; }

long GetPortPenSize(long a, long b, long c_, long d, long e, long f) __asm("_GetPortPenSize");
long GetPortPenSize(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPortPenSize"); return 0; }

long GetPortPenVisibility(long a, long b, long c_, long d, long e, long f) __asm("_GetPortPenVisibility");
long GetPortPenVisibility(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPortPenVisibility"); return 0; }

long GetPortSpExtra(long a, long b, long c_, long d, long e, long f) __asm("_GetPortSpExtra");
long GetPortSpExtra(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPortSpExtra"); return 0; }

long GetPreserveGlyph(long a, long b, long c_, long d, long e, long f) __asm("_GetPreserveGlyph");
long GetPreserveGlyph(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPreserveGlyph"); return 0; }

long GetQDGlobalsArrow(long a, long b, long c_, long d, long e, long f) __asm("_GetQDGlobalsArrow");
long GetQDGlobalsArrow(long a, long b, long c_, long d, long e, long f) { shim_note("_GetQDGlobalsArrow"); return 0; }

long GetQDGlobalsRandomSeed(long a, long b, long c_, long d, long e, long f) __asm("_GetQDGlobalsRandomSeed");
long GetQDGlobalsRandomSeed(long a, long b, long c_, long d, long e, long f) { shim_note("_GetQDGlobalsRandomSeed"); return 0; }

long GetQDGlobalsScreenBits(long a, long b, long c_, long d, long e, long f) __asm("_GetQDGlobalsScreenBits");
long GetQDGlobalsScreenBits(long a, long b, long c_, long d, long e, long f) { shim_note("_GetQDGlobalsScreenBits"); return 0; }

long GetQDGlobalsThePort(long a, long b, long c_, long d, long e, long f) __asm("_GetQDGlobalsThePort");
long GetQDGlobalsThePort(long a, long b, long c_, long d, long e, long f) { shim_note("_GetQDGlobalsThePort"); return 0; }

long GetSubTable(long a, long b, long c_, long d, long e, long f) __asm("_GetSubTable");
long GetSubTable(long a, long b, long c_, long d, long e, long f) { shim_note("_GetSubTable"); return 0; }

long GetSuiteLabel(long a, long b, long c_, long d, long e, long f) __asm("_GetSuiteLabel");
long GetSuiteLabel(long a, long b, long c_, long d, long e, long f) { shim_note("_GetSuiteLabel"); return 0; }

long GetSysFont(long a, long b, long c_, long d, long e, long f) __asm("_GetSysFont");
long GetSysFont(long a, long b, long c_, long d, long e, long f) { shim_note("_GetSysFont"); return 0; }

long GrafDevice(long a, long b, long c_, long d, long e, long f) __asm("_GrafDevice");
long GrafDevice(long a, long b, long c_, long d, long e, long f) { shim_note("_GrafDevice"); return 0; }

long HiliteColor(long a, long b, long c_, long d, long e, long f) __asm("_HiliteColor");
long HiliteColor(long a, long b, long c_, long d, long e, long f) { shim_note("_HiliteColor"); return 0; }

long IconFamilyToIconSuite(long a, long b, long c_, long d, long e, long f) __asm("_IconFamilyToIconSuite");
long IconFamilyToIconSuite(long a, long b, long c_, long d, long e, long f) { shim_note("_IconFamilyToIconSuite"); return 0; }

long IconIDToRgn(long a, long b, long c_, long d, long e, long f) __asm("_IconIDToRgn");
long IconIDToRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_IconIDToRgn"); return 0; }

long IconRefToRgn(long a, long b, long c_, long d, long e, long f) __asm("_IconRefToRgn");
long IconRefToRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_IconRefToRgn"); return 0; }

long IconSuiteToIconFamily(long a, long b, long c_, long d, long e, long f) __asm("_IconSuiteToIconFamily");
long IconSuiteToIconFamily(long a, long b, long c_, long d, long e, long f) { shim_note("_IconSuiteToIconFamily"); return 0; }

long IconSuiteToRgn(long a, long b, long c_, long d, long e, long f) __asm("_IconSuiteToRgn");
long IconSuiteToRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_IconSuiteToRgn"); return 0; }

long Index2Color(long a, long b, long c_, long d, long e, long f) __asm("_Index2Color");
long Index2Color(long a, long b, long c_, long d, long e, long f) { shim_note("_Index2Color"); return 0; }

long InitGDevice(long a, long b, long c_, long d, long e, long f) __asm("_InitGDevice");
long InitGDevice(long a, long b, long c_, long d, long e, long f) { shim_note("_InitGDevice"); return 0; }

long InvertArc(long a, long b, long c_, long d, long e, long f) __asm("_InvertArc");
long InvertArc(long a, long b, long c_, long d, long e, long f) { shim_note("_InvertArc"); return 0; }

long InvertColor(long a, long b, long c_, long d, long e, long f) __asm("_InvertColor");
long InvertColor(long a, long b, long c_, long d, long e, long f) { shim_note("_InvertColor"); return 0; }

long InvertOval(long a, long b, long c_, long d, long e, long f) __asm("_InvertOval");
long InvertOval(long a, long b, long c_, long d, long e, long f) { shim_note("_InvertOval"); return 0; }

long InvertPoly(long a, long b, long c_, long d, long e, long f) __asm("_InvertPoly");
long InvertPoly(long a, long b, long c_, long d, long e, long f) { shim_note("_InvertPoly"); return 0; }

long InvertRoundRect(long a, long b, long c_, long d, long e, long f) __asm("_InvertRoundRect");
long InvertRoundRect(long a, long b, long c_, long d, long e, long f) { shim_note("_InvertRoundRect"); return 0; }

long IsOutline(long a, long b, long c_, long d, long e, long f) __asm("_IsOutline");
long IsOutline(long a, long b, long c_, long d, long e, long f) { shim_note("_IsOutline"); return 0; }

long IsPortClipRegionEmpty(long a, long b, long c_, long d, long e, long f) __asm("_IsPortClipRegionEmpty");
long IsPortClipRegionEmpty(long a, long b, long c_, long d, long e, long f) { shim_note("_IsPortClipRegionEmpty"); return 0; }

long IsPortColor(long a, long b, long c_, long d, long e, long f) __asm("_IsPortColor");
long IsPortColor(long a, long b, long c_, long d, long e, long f) { shim_note("_IsPortColor"); return 0; }

long IsPortOffscreen(long a, long b, long c_, long d, long e, long f) __asm("_IsPortOffscreen");
long IsPortOffscreen(long a, long b, long c_, long d, long e, long f) { shim_note("_IsPortOffscreen"); return 0; }

long IsPortPictureBeingDefined(long a, long b, long c_, long d, long e, long f) __asm("_IsPortPictureBeingDefined");
long IsPortPictureBeingDefined(long a, long b, long c_, long d, long e, long f) { shim_note("_IsPortPictureBeingDefined"); return 0; }

long IsPortPolyBeingDefined(long a, long b, long c_, long d, long e, long f) __asm("_IsPortPolyBeingDefined");
long IsPortPolyBeingDefined(long a, long b, long c_, long d, long e, long f) { shim_note("_IsPortPolyBeingDefined"); return 0; }

long IsPortRegionBeingDefined(long a, long b, long c_, long d, long e, long f) __asm("_IsPortRegionBeingDefined");
long IsPortRegionBeingDefined(long a, long b, long c_, long d, long e, long f) { shim_note("_IsPortRegionBeingDefined"); return 0; }

long IsPortVisibleRegionEmpty(long a, long b, long c_, long d, long e, long f) __asm("_IsPortVisibleRegionEmpty");
long IsPortVisibleRegionEmpty(long a, long b, long c_, long d, long e, long f) { shim_note("_IsPortVisibleRegionEmpty"); return 0; }

long IsValidPort(long a, long b, long c_, long d, long e, long f) __asm("_IsValidPort");
long IsValidPort(long a, long b, long c_, long d, long e, long f) { shim_note("_IsValidPort"); return 0; }

long KillPoly(long a, long b, long c_, long d, long e, long f) __asm("_KillPoly");
long KillPoly(long a, long b, long c_, long d, long e, long f) { shim_note("_KillPoly"); return 0; }

long LMGetCursorNew(long a, long b, long c_, long d, long e, long f) __asm("_LMGetCursorNew");
long LMGetCursorNew(long a, long b, long c_, long d, long e, long f) { shim_note("_LMGetCursorNew"); return 0; }

long LMGetDeviceList(long a, long b, long c_, long d, long e, long f) __asm("_LMGetDeviceList");
long LMGetDeviceList(long a, long b, long c_, long d, long e, long f) { shim_note("_LMGetDeviceList"); return 0; }

long LMGetFractEnable(long a, long b, long c_, long d, long e, long f) __asm("_LMGetFractEnable");
long LMGetFractEnable(long a, long b, long c_, long d, long e, long f) { shim_note("_LMGetFractEnable"); return 0; }

long LMGetHiliteMode(long a, long b, long c_, long d, long e, long f) __asm("_LMGetHiliteMode");
long LMGetHiliteMode(long a, long b, long c_, long d, long e, long f) { shim_note("_LMGetHiliteMode"); return 0; }

long LMGetHiliteRGB(long a, long b, long c_, long d, long e, long f) __asm("_LMGetHiliteRGB");
long LMGetHiliteRGB(long a, long b, long c_, long d, long e, long f) { shim_note("_LMGetHiliteRGB"); return 0; }

long LMGetLastFOND(long a, long b, long c_, long d, long e, long f) __asm("_LMGetLastFOND");
long LMGetLastFOND(long a, long b, long c_, long d, long e, long f) { shim_note("_LMGetLastFOND"); return 0; }

long LMGetLastSPExtra(long a, long b, long c_, long d, long e, long f) __asm("_LMGetLastSPExtra");
long LMGetLastSPExtra(long a, long b, long c_, long d, long e, long f) { shim_note("_LMGetLastSPExtra"); return 0; }

long LMGetMainDevice(long a, long b, long c_, long d, long e, long f) __asm("_LMGetMainDevice");
long LMGetMainDevice(long a, long b, long c_, long d, long e, long f) { shim_note("_LMGetMainDevice"); return 0; }

long LMGetQDColors(long a, long b, long c_, long d, long e, long f) __asm("_LMGetQDColors");
long LMGetQDColors(long a, long b, long c_, long d, long e, long f) { shim_note("_LMGetQDColors"); return 0; }

long LMGetScrHRes(long a, long b, long c_, long d, long e, long f) __asm("_LMGetScrHRes");
long LMGetScrHRes(long a, long b, long c_, long d, long e, long f) { shim_note("_LMGetScrHRes"); return 0; }

long LMGetScrVRes(long a, long b, long c_, long d, long e, long f) __asm("_LMGetScrVRes");
long LMGetScrVRes(long a, long b, long c_, long d, long e, long f) { shim_note("_LMGetScrVRes"); return 0; }

long LMGetTheGDevice(long a, long b, long c_, long d, long e, long f) __asm("_LMGetTheGDevice");
long LMGetTheGDevice(long a, long b, long c_, long d, long e, long f) { shim_note("_LMGetTheGDevice"); return 0; }

long LMGetWidthListHand(long a, long b, long c_, long d, long e, long f) __asm("_LMGetWidthListHand");
long LMGetWidthListHand(long a, long b, long c_, long d, long e, long f) { shim_note("_LMGetWidthListHand"); return 0; }

long LMGetWidthTabHandle(long a, long b, long c_, long d, long e, long f) __asm("_LMGetWidthTabHandle");
long LMGetWidthTabHandle(long a, long b, long c_, long d, long e, long f) { shim_note("_LMGetWidthTabHandle"); return 0; }

long LMSetCursorNew(long a, long b, long c_, long d, long e, long f) __asm("_LMSetCursorNew");
long LMSetCursorNew(long a, long b, long c_, long d, long e, long f) { shim_note("_LMSetCursorNew"); return 0; }

long LMSetDeviceList(long a, long b, long c_, long d, long e, long f) __asm("_LMSetDeviceList");
long LMSetDeviceList(long a, long b, long c_, long d, long e, long f) { shim_note("_LMSetDeviceList"); return 0; }

long LMSetFractEnable(long a, long b, long c_, long d, long e, long f) __asm("_LMSetFractEnable");
long LMSetFractEnable(long a, long b, long c_, long d, long e, long f) { shim_note("_LMSetFractEnable"); return 0; }

long LMSetHiliteMode(long a, long b, long c_, long d, long e, long f) __asm("_LMSetHiliteMode");
long LMSetHiliteMode(long a, long b, long c_, long d, long e, long f) { shim_note("_LMSetHiliteMode"); return 0; }

long LMSetHiliteRGB(long a, long b, long c_, long d, long e, long f) __asm("_LMSetHiliteRGB");
long LMSetHiliteRGB(long a, long b, long c_, long d, long e, long f) { shim_note("_LMSetHiliteRGB"); return 0; }

long LMSetLastFOND(long a, long b, long c_, long d, long e, long f) __asm("_LMSetLastFOND");
long LMSetLastFOND(long a, long b, long c_, long d, long e, long f) { shim_note("_LMSetLastFOND"); return 0; }

long LMSetLastSPExtra(long a, long b, long c_, long d, long e, long f) __asm("_LMSetLastSPExtra");
long LMSetLastSPExtra(long a, long b, long c_, long d, long e, long f) { shim_note("_LMSetLastSPExtra"); return 0; }

long LMSetMainDevice(long a, long b, long c_, long d, long e, long f) __asm("_LMSetMainDevice");
long LMSetMainDevice(long a, long b, long c_, long d, long e, long f) { shim_note("_LMSetMainDevice"); return 0; }

long LMSetQDColors(long a, long b, long c_, long d, long e, long f) __asm("_LMSetQDColors");
long LMSetQDColors(long a, long b, long c_, long d, long e, long f) { shim_note("_LMSetQDColors"); return 0; }

long LMSetScrHRes(long a, long b, long c_, long d, long e, long f) __asm("_LMSetScrHRes");
long LMSetScrHRes(long a, long b, long c_, long d, long e, long f) { shim_note("_LMSetScrHRes"); return 0; }

long LMSetScrVRes(long a, long b, long c_, long d, long e, long f) __asm("_LMSetScrVRes");
long LMSetScrVRes(long a, long b, long c_, long d, long e, long f) { shim_note("_LMSetScrVRes"); return 0; }

long LMSetTheGDevice(long a, long b, long c_, long d, long e, long f) __asm("_LMSetTheGDevice");
long LMSetTheGDevice(long a, long b, long c_, long d, long e, long f) { shim_note("_LMSetTheGDevice"); return 0; }

long LMSetWidthListHand(long a, long b, long c_, long d, long e, long f) __asm("_LMSetWidthListHand");
long LMSetWidthListHand(long a, long b, long c_, long d, long e, long f) { shim_note("_LMSetWidthListHand"); return 0; }

long LMSetWidthTabHandle(long a, long b, long c_, long d, long e, long f) __asm("_LMSetWidthTabHandle");
long LMSetWidthTabHandle(long a, long b, long c_, long d, long e, long f) { shim_note("_LMSetWidthTabHandle"); return 0; }

long LoadIconCache(long a, long b, long c_, long d, long e, long f) __asm("_LoadIconCache");
long LoadIconCache(long a, long b, long c_, long d, long e, long f) { shim_note("_LoadIconCache"); return 0; }

long MakeITable(long a, long b, long c_, long d, long e, long f) __asm("_MakeITable");
long MakeITable(long a, long b, long c_, long d, long e, long f) { shim_note("_MakeITable"); return 0; }

long MakeRGBPat(long a, long b, long c_, long d, long e, long f) __asm("_MakeRGBPat");
long MakeRGBPat(long a, long b, long c_, long d, long e, long f) { shim_note("_MakeRGBPat"); return 0; }

long MapPoly(long a, long b, long c_, long d, long e, long f) __asm("_MapPoly");
long MapPoly(long a, long b, long c_, long d, long e, long f) { shim_note("_MapPoly"); return 0; }

long MovePortTo(long a, long b, long c_, long d, long e, long f) __asm("_MovePortTo");
long MovePortTo(long a, long b, long c_, long d, long e, long f) { shim_note("_MovePortTo"); return 0; }

long NewGDevice(long a, long b, long c_, long d, long e, long f) __asm("_NewGDevice");
long NewGDevice(long a, long b, long c_, long d, long e, long f) { shim_note("_NewGDevice"); return 0; }

long NewIconSuite(long a, long b, long c_, long d, long e, long f) __asm("_NewIconSuite");
long NewIconSuite(long a, long b, long c_, long d, long e, long f) { shim_note("_NewIconSuite"); return 0; }

long NewPixMap(long a, long b, long c_, long d, long e, long f) __asm("_NewPixMap");
long NewPixMap(long a, long b, long c_, long d, long e, long f) { shim_note("_NewPixMap"); return 0; }

long NewPixPat(long a, long b, long c_, long d, long e, long f) __asm("_NewPixPat");
long NewPixPat(long a, long b, long c_, long d, long e, long f) { shim_note("_NewPixPat"); return 0; }

long NewScreenBuffer(long a, long b, long c_, long d, long e, long f) __asm("_NewScreenBuffer");
long NewScreenBuffer(long a, long b, long c_, long d, long e, long f) { shim_note("_NewScreenBuffer"); return 0; }

long NewTempScreenBuffer(long a, long b, long c_, long d, long e, long f) __asm("_NewTempScreenBuffer");
long NewTempScreenBuffer(long a, long b, long c_, long d, long e, long f) { shim_note("_NewTempScreenBuffer"); return 0; }

long NoPurgePixels(long a, long b, long c_, long d, long e, long f) __asm("_NoPurgePixels");
long NoPurgePixels(long a, long b, long c_, long d, long e, long f) { shim_note("_NoPurgePixels"); return 0; }

long OffscreenVersion(long a, long b, long c_, long d, long e, long f) __asm("_OffscreenVersion");
long OffscreenVersion(long a, long b, long c_, long d, long e, long f) { shim_note("_OffscreenVersion"); return 0; }

long OffsetPoly(long a, long b, long c_, long d, long e, long f) __asm("_OffsetPoly");
long OffsetPoly(long a, long b, long c_, long d, long e, long f) { shim_note("_OffsetPoly"); return 0; }

long OpColor(long a, long b, long c_, long d, long e, long f) __asm("_OpColor");
long OpColor(long a, long b, long c_, long d, long e, long f) { shim_note("_OpColor"); return 0; }

long OpenPicture(long a, long b, long c_, long d, long e, long f) __asm("_OpenPicture");
long OpenPicture(long a, long b, long c_, long d, long e, long f) { shim_note("_OpenPicture"); return 0; }

long OpenPoly(long a, long b, long c_, long d, long e, long f) __asm("_OpenPoly");
long OpenPoly(long a, long b, long c_, long d, long e, long f) { shim_note("_OpenPoly"); return 0; }

long OpenRgn(long a, long b, long c_, long d, long e, long f) __asm("_OpenRgn");
long OpenRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_OpenRgn"); return 0; }

long PaintArc(long a, long b, long c_, long d, long e, long f) __asm("_PaintArc");
long PaintArc(long a, long b, long c_, long d, long e, long f) { shim_note("_PaintArc"); return 0; }

long PaintOval(long a, long b, long c_, long d, long e, long f) __asm("_PaintOval");
long PaintOval(long a, long b, long c_, long d, long e, long f) { shim_note("_PaintOval"); return 0; }

long PaintPoly(long a, long b, long c_, long d, long e, long f) __asm("_PaintPoly");
long PaintPoly(long a, long b, long c_, long d, long e, long f) { shim_note("_PaintPoly"); return 0; }

long PaintRoundRect(long a, long b, long c_, long d, long e, long f) __asm("_PaintRoundRect");
long PaintRoundRect(long a, long b, long c_, long d, long e, long f) { shim_note("_PaintRoundRect"); return 0; }

long PenPixPat(long a, long b, long c_, long d, long e, long f) __asm("_PenPixPat");
long PenPixPat(long a, long b, long c_, long d, long e, long f) { shim_note("_PenPixPat"); return 0; }

long PicComment(long a, long b, long c_, long d, long e, long f) __asm("_PicComment");
long PicComment(long a, long b, long c_, long d, long e, long f) { shim_note("_PicComment"); return 0; }

long PixMap32Bit(long a, long b, long c_, long d, long e, long f) __asm("_PixMap32Bit");
long PixMap32Bit(long a, long b, long c_, long d, long e, long f) { shim_note("_PixMap32Bit"); return 0; }

long PixPatChanged(long a, long b, long c_, long d, long e, long f) __asm("_PixPatChanged");
long PixPatChanged(long a, long b, long c_, long d, long e, long f) { shim_note("_PixPatChanged"); return 0; }

long PlotCIcon(long a, long b, long c_, long d, long e, long f) __asm("_PlotCIcon");
long PlotCIcon(long a, long b, long c_, long d, long e, long f) { shim_note("_PlotCIcon"); return 0; }

long PlotCIconHandle(long a, long b, long c_, long d, long e, long f) __asm("_PlotCIconHandle");
long PlotCIconHandle(long a, long b, long c_, long d, long e, long f) { shim_note("_PlotCIconHandle"); return 0; }

long PlotIcon(long a, long b, long c_, long d, long e, long f) __asm("_PlotIcon");
long PlotIcon(long a, long b, long c_, long d, long e, long f) { shim_note("_PlotIcon"); return 0; }

long PlotIconHandle(long a, long b, long c_, long d, long e, long f) __asm("_PlotIconHandle");
long PlotIconHandle(long a, long b, long c_, long d, long e, long f) { shim_note("_PlotIconHandle"); return 0; }

long PlotIconID(long a, long b, long c_, long d, long e, long f) __asm("_PlotIconID");
long PlotIconID(long a, long b, long c_, long d, long e, long f) { shim_note("_PlotIconID"); return 0; }

long PlotIconRef(long a, long b, long c_, long d, long e, long f) __asm("_PlotIconRef");
long PlotIconRef(long a, long b, long c_, long d, long e, long f) { shim_note("_PlotIconRef"); return 0; }

long PlotIconSuite(long a, long b, long c_, long d, long e, long f) __asm("_PlotIconSuite");
long PlotIconSuite(long a, long b, long c_, long d, long e, long f) { shim_note("_PlotIconSuite"); return 0; }

long PlotSICNHandle(long a, long b, long c_, long d, long e, long f) __asm("_PlotSICNHandle");
long PlotSICNHandle(long a, long b, long c_, long d, long e, long f) { shim_note("_PlotSICNHandle"); return 0; }

long PortChanged(long a, long b, long c_, long d, long e, long f) __asm("_PortChanged");
long PortChanged(long a, long b, long c_, long d, long e, long f) { shim_note("_PortChanged"); return 0; }

long PortSize(long a, long b, long c_, long d, long e, long f) __asm("_PortSize");
long PortSize(long a, long b, long c_, long d, long e, long f) { shim_note("_PortSize"); return 0; }

long ProtectEntry(long a, long b, long c_, long d, long e, long f) __asm("_ProtectEntry");
long ProtectEntry(long a, long b, long c_, long d, long e, long f) { shim_note("_ProtectEntry"); return 0; }

long PtInIconID(long a, long b, long c_, long d, long e, long f) __asm("_PtInIconID");
long PtInIconID(long a, long b, long c_, long d, long e, long f) { shim_note("_PtInIconID"); return 0; }

long PtInIconRef(long a, long b, long c_, long d, long e, long f) __asm("_PtInIconRef");
long PtInIconRef(long a, long b, long c_, long d, long e, long f) { shim_note("_PtInIconRef"); return 0; }

long PtInIconSuite(long a, long b, long c_, long d, long e, long f) __asm("_PtInIconSuite");
long PtInIconSuite(long a, long b, long c_, long d, long e, long f) { shim_note("_PtInIconSuite"); return 0; }

long QDDone(long a, long b, long c_, long d, long e, long f) __asm("_QDDone");
long QDDone(long a, long b, long c_, long d, long e, long f) { shim_note("_QDDone"); return 0; }

long QDGetDirtyRegion(long a, long b, long c_, long d, long e, long f) __asm("_QDGetDirtyRegion");
long QDGetDirtyRegion(long a, long b, long c_, long d, long e, long f) { shim_note("_QDGetDirtyRegion"); return 0; }

long QDGlobalToLocalPoint(long a, long b, long c_, long d, long e, long f) __asm("_QDGlobalToLocalPoint");
long QDGlobalToLocalPoint(long a, long b, long c_, long d, long e, long f) { shim_note("_QDGlobalToLocalPoint"); return 0; }

long QDGlobalToLocalRect(long a, long b, long c_, long d, long e, long f) __asm("_QDGlobalToLocalRect");
long QDGlobalToLocalRect(long a, long b, long c_, long d, long e, long f) { shim_note("_QDGlobalToLocalRect"); return 0; }

long QDGlobalToLocalRegion(long a, long b, long c_, long d, long e, long f) __asm("_QDGlobalToLocalRegion");
long QDGlobalToLocalRegion(long a, long b, long c_, long d, long e, long f) { shim_note("_QDGlobalToLocalRegion"); return 0; }

long QDLocalToGlobalPoint(long a, long b, long c_, long d, long e, long f) __asm("_QDLocalToGlobalPoint");
long QDLocalToGlobalPoint(long a, long b, long c_, long d, long e, long f) { shim_note("_QDLocalToGlobalPoint"); return 0; }

long QDLocalToGlobalRect(long a, long b, long c_, long d, long e, long f) __asm("_QDLocalToGlobalRect");
long QDLocalToGlobalRect(long a, long b, long c_, long d, long e, long f) { shim_note("_QDLocalToGlobalRect"); return 0; }

long QDLocalToGlobalRegion(long a, long b, long c_, long d, long e, long f) __asm("_QDLocalToGlobalRegion");
long QDLocalToGlobalRegion(long a, long b, long c_, long d, long e, long f) { shim_note("_QDLocalToGlobalRegion"); return 0; }

long QDSetDirtyRegion(long a, long b, long c_, long d, long e, long f) __asm("_QDSetDirtyRegion");
long QDSetDirtyRegion(long a, long b, long c_, long d, long e, long f) { shim_note("_QDSetDirtyRegion"); return 0; }

long QDSwapPort(long a, long b, long c_, long d, long e, long f) __asm("_QDSwapPort");
long QDSwapPort(long a, long b, long c_, long d, long e, long f) { shim_note("_QDSwapPort"); return 0; }

long QDTextBounds(long a, long b, long c_, long d, long e, long f) __asm("_QDTextBounds");
long QDTextBounds(long a, long b, long c_, long d, long e, long f) { shim_note("_QDTextBounds"); return 0; }

long Random(long a, long b, long c_, long d, long e, long f) __asm("_Random");
long Random(long a, long b, long c_, long d, long e, long f) { shim_note("_Random"); return 0; }

long RealColor(long a, long b, long c_, long d, long e, long f) __asm("_RealColor");
long RealColor(long a, long b, long c_, long d, long e, long f) { shim_note("_RealColor"); return 0; }

long RealFont(long a, long b, long c_, long d, long e, long f) __asm("_RealFont");
long RealFont(long a, long b, long c_, long d, long e, long f) { shim_note("_RealFont"); return 0; }

long RectInIconID(long a, long b, long c_, long d, long e, long f) __asm("_RectInIconID");
long RectInIconID(long a, long b, long c_, long d, long e, long f) { shim_note("_RectInIconID"); return 0; }

long RectInIconRef(long a, long b, long c_, long d, long e, long f) __asm("_RectInIconRef");
long RectInIconRef(long a, long b, long c_, long d, long e, long f) { shim_note("_RectInIconRef"); return 0; }

long RectInIconSuite(long a, long b, long c_, long d, long e, long f) __asm("_RectInIconSuite");
long RectInIconSuite(long a, long b, long c_, long d, long e, long f) { shim_note("_RectInIconSuite"); return 0; }

long ReserveEntry(long a, long b, long c_, long d, long e, long f) __asm("_ReserveEntry");
long ReserveEntry(long a, long b, long c_, long d, long e, long f) { shim_note("_ReserveEntry"); return 0; }

long ScreenRes(long a, long b, long c_, long d, long e, long f) __asm("_ScreenRes");
long ScreenRes(long a, long b, long c_, long d, long e, long f) { shim_note("_ScreenRes"); return 0; }

long ScrollRect(long a, long b, long c_, long d, long e, long f) __asm("_ScrollRect");
long ScrollRect(long a, long b, long c_, long d, long e, long f) { shim_note("_ScrollRect"); return 0; }

long SectRegionWithPortClipRegion(long a, long b, long c_, long d, long e, long f) __asm("_SectRegionWithPortClipRegion");
long SectRegionWithPortClipRegion(long a, long b, long c_, long d, long e, long f) { shim_note("_SectRegionWithPortClipRegion"); return 0; }

long SectRegionWithPortVisibleRegion(long a, long b, long c_, long d, long e, long f) __asm("_SectRegionWithPortVisibleRegion");
long SectRegionWithPortVisibleRegion(long a, long b, long c_, long d, long e, long f) { shim_note("_SectRegionWithPortVisibleRegion"); return 0; }

long SetCPixel(long a, long b, long c_, long d, long e, long f) __asm("_SetCPixel");
long SetCPixel(long a, long b, long c_, long d, long e, long f) { shim_note("_SetCPixel"); return 0; }

long SetClientID(long a, long b, long c_, long d, long e, long f) __asm("_SetClientID");
long SetClientID(long a, long b, long c_, long d, long e, long f) { shim_note("_SetClientID"); return 0; }

long SetDeviceAttribute(long a, long b, long c_, long d, long e, long f) __asm("_SetDeviceAttribute");
long SetDeviceAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("_SetDeviceAttribute"); return 0; }

long SetFScaleDisable(long a, long b, long c_, long d, long e, long f) __asm("_SetFScaleDisable");
long SetFScaleDisable(long a, long b, long c_, long d, long e, long f) { shim_note("_SetFScaleDisable"); return 0; }

long SetFractEnable(long a, long b, long c_, long d, long e, long f) __asm("_SetFractEnable");
long SetFractEnable(long a, long b, long c_, long d, long e, long f) { shim_note("_SetFractEnable"); return 0; }

long SetOutlinePreferred(long a, long b, long c_, long d, long e, long f) __asm("_SetOutlinePreferred");
long SetOutlinePreferred(long a, long b, long c_, long d, long e, long f) { shim_note("_SetOutlinePreferred"); return 0; }

long SetPixelsState(long a, long b, long c_, long d, long e, long f) __asm("_SetPixelsState");
long SetPixelsState(long a, long b, long c_, long d, long e, long f) { shim_note("_SetPixelsState"); return 0; }

long SetPortBackPixPat(long a, long b, long c_, long d, long e, long f) __asm("_SetPortBackPixPat");
long SetPortBackPixPat(long a, long b, long c_, long d, long e, long f) { shim_note("_SetPortBackPixPat"); return 0; }

long SetPortBits(long a, long b, long c_, long d, long e, long f) __asm("_SetPortBits");
long SetPortBits(long a, long b, long c_, long d, long e, long f) { shim_note("_SetPortBits"); return 0; }

long SetPortClipRegion(long a, long b, long c_, long d, long e, long f) __asm("_SetPortClipRegion");
long SetPortClipRegion(long a, long b, long c_, long d, long e, long f) { shim_note("_SetPortClipRegion"); return 0; }

long SetPortFillPixPat(long a, long b, long c_, long d, long e, long f) __asm("_SetPortFillPixPat");
long SetPortFillPixPat(long a, long b, long c_, long d, long e, long f) { shim_note("_SetPortFillPixPat"); return 0; }

long SetPortFracHPenLocation(long a, long b, long c_, long d, long e, long f) __asm("_SetPortFracHPenLocation");
long SetPortFracHPenLocation(long a, long b, long c_, long d, long e, long f) { shim_note("_SetPortFracHPenLocation"); return 0; }

long SetPortOpColor(long a, long b, long c_, long d, long e, long f) __asm("_SetPortOpColor");
long SetPortOpColor(long a, long b, long c_, long d, long e, long f) { shim_note("_SetPortOpColor"); return 0; }

long SetPortPenMode(long a, long b, long c_, long d, long e, long f) __asm("_SetPortPenMode");
long SetPortPenMode(long a, long b, long c_, long d, long e, long f) { shim_note("_SetPortPenMode"); return 0; }

long SetPortPenPixPat(long a, long b, long c_, long d, long e, long f) __asm("_SetPortPenPixPat");
long SetPortPenPixPat(long a, long b, long c_, long d, long e, long f) { shim_note("_SetPortPenPixPat"); return 0; }

long SetPortPenSize(long a, long b, long c_, long d, long e, long f) __asm("_SetPortPenSize");
long SetPortPenSize(long a, long b, long c_, long d, long e, long f) { shim_note("_SetPortPenSize"); return 0; }

long SetPortPix(long a, long b, long c_, long d, long e, long f) __asm("_SetPortPix");
long SetPortPix(long a, long b, long c_, long d, long e, long f) { shim_note("_SetPortPix"); return 0; }

long SetPortTextFace(long a, long b, long c_, long d, long e, long f) __asm("_SetPortTextFace");
long SetPortTextFace(long a, long b, long c_, long d, long e, long f) { shim_note("_SetPortTextFace"); return 0; }

long SetPortTextFont(long a, long b, long c_, long d, long e, long f) __asm("_SetPortTextFont");
long SetPortTextFont(long a, long b, long c_, long d, long e, long f) { shim_note("_SetPortTextFont"); return 0; }

long SetPortTextMode(long a, long b, long c_, long d, long e, long f) __asm("_SetPortTextMode");
long SetPortTextMode(long a, long b, long c_, long d, long e, long f) { shim_note("_SetPortTextMode"); return 0; }

long SetPortTextSize(long a, long b, long c_, long d, long e, long f) __asm("_SetPortTextSize");
long SetPortTextSize(long a, long b, long c_, long d, long e, long f) { shim_note("_SetPortTextSize"); return 0; }

long SetPortVisibleRegion(long a, long b, long c_, long d, long e, long f) __asm("_SetPortVisibleRegion");
long SetPortVisibleRegion(long a, long b, long c_, long d, long e, long f) { shim_note("_SetPortVisibleRegion"); return 0; }

long SetPreserveGlyph(long a, long b, long c_, long d, long e, long f) __asm("_SetPreserveGlyph");
long SetPreserveGlyph(long a, long b, long c_, long d, long e, long f) { shim_note("_SetPreserveGlyph"); return 0; }

long SetQDError(long a, long b, long c_, long d, long e, long f) __asm("_SetQDError");
long SetQDError(long a, long b, long c_, long d, long e, long f) { shim_note("_SetQDError"); return 0; }

long SetQDGlobalsArrow(long a, long b, long c_, long d, long e, long f) __asm("_SetQDGlobalsArrow");
long SetQDGlobalsArrow(long a, long b, long c_, long d, long e, long f) { shim_note("_SetQDGlobalsArrow"); return 0; }

long SetSuiteLabel(long a, long b, long c_, long d, long e, long f) __asm("_SetSuiteLabel");
long SetSuiteLabel(long a, long b, long c_, long d, long e, long f) { shim_note("_SetSuiteLabel"); return 0; }

long SpaceExtra(long a, long b, long c_, long d, long e, long f) __asm("_SpaceExtra");
long SpaceExtra(long a, long b, long c_, long d, long e, long f) { shim_note("_SpaceExtra"); return 0; }

long StdBits(long a, long b, long c_, long d, long e, long f) __asm("_StdBits");
long StdBits(long a, long b, long c_, long d, long e, long f) { shim_note("_StdBits"); return 0; }

long StringWidth(long a, long b, long c_, long d, long e, long f) __asm("_StringWidth");
long StringWidth(long a, long b, long c_, long d, long e, long f) { shim_note("_StringWidth"); return 0; }

long SwapPortPicSaveHandle(long a, long b, long c_, long d, long e, long f) __asm("_SwapPortPicSaveHandle");
long SwapPortPicSaveHandle(long a, long b, long c_, long d, long e, long f) { shim_note("_SwapPortPicSaveHandle"); return 0; }

long SwapPortPolySaveHandle(long a, long b, long c_, long d, long e, long f) __asm("_SwapPortPolySaveHandle");
long SwapPortPolySaveHandle(long a, long b, long c_, long d, long e, long f) { shim_note("_SwapPortPolySaveHandle"); return 0; }

long SwapPortRegionSaveHandle(long a, long b, long c_, long d, long e, long f) __asm("_SwapPortRegionSaveHandle");
long SwapPortRegionSaveHandle(long a, long b, long c_, long d, long e, long f) { shim_note("_SwapPortRegionSaveHandle"); return 0; }

long SyncCGContextOriginWithPort(long a, long b, long c_, long d, long e, long f) __asm("_SyncCGContextOriginWithPort");
long SyncCGContextOriginWithPort(long a, long b, long c_, long d, long e, long f) { shim_note("_SyncCGContextOriginWithPort"); return 0; }

long TruncString(long a, long b, long c_, long d, long e, long f) __asm("_TruncString");
long TruncString(long a, long b, long c_, long d, long e, long f) { shim_note("_TruncString"); return 0; }
