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
  fprintf(stderr, "[shimauto:%s] %s: removed-OS symbol CALLED with no curated impl -- returning 0 (write one in shimdb/impl/ if its behavior matters)\n", "libwx_macud_stc-2.8.0", sym);
  if (getenv("SHIMGEN_STUB_ABORT")) abort();
  pthread_mutex_unlock(&mtx);
}

long ZN16wxStyledTextCtrl10AddTextRawEPKc(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10AddTextRawEPKc");
long ZN16wxStyledTextCtrl10AddTextRawEPKc(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10AddTextRawEPKc"); return 0; }

long ZN16wxStyledTextCtrl10AppendTextERK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10AppendTextERK8wxString");
long ZN16wxStyledTextCtrl10AppendTextERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10AppendTextERK8wxString"); return 0; }

long ZN16wxStyledTextCtrl10BraceMatchEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10BraceMatchEi");
long ZN16wxStyledTextCtrl10BraceMatchEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10BraceMatchEi"); return 0; }

long ZN16wxStyledTextCtrl10DeleteBackEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10DeleteBackEv");
long ZN16wxStyledTextCtrl10DeleteBackEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10DeleteBackEv"); return 0; }

long ZN16wxStyledTextCtrl10DoDragOverEii12wxDragResult(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10DoDragOverEii12wxDragResult");
long ZN16wxStyledTextCtrl10DoDragOverEii12wxDragResult(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10DoDragOverEii12wxDragResult"); return 0; }

long ZN16wxStyledTextCtrl10DoDropTextEllRK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10DoDropTextEllRK8wxString");
long ZN16wxStyledTextCtrl10DoDropTextEllRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10DoDropTextEllRK8wxString"); return 0; }

long ZN16wxStyledTextCtrl10FindColumnEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10FindColumnEii");
long ZN16wxStyledTextCtrl10FindColumnEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10FindColumnEii"); return 0; }

long ZN16wxStyledTextCtrl10GetCurLineEPi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10GetCurLineEPi");
long ZN16wxStyledTextCtrl10GetCurLineEPi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10GetCurLineEPi"); return 0; }

long ZN16wxStyledTextCtrl10GetEOLModeEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10GetEOLModeEv");
long ZN16wxStyledTextCtrl10GetEOLModeEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10GetEOLModeEv"); return 0; }

long ZN16wxStyledTextCtrl10GetLineRawEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10GetLineRawEi");
long ZN16wxStyledTextCtrl10GetLineRawEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10GetLineRawEi"); return 0; }

long ZN16wxStyledTextCtrl10GetStyleAtEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10GetStyleAtEi");
long ZN16wxStyledTextCtrl10GetStyleAtEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10GetStyleAtEi"); return 0; }

long ZN16wxStyledTextCtrl10GetTextRawEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10GetTextRawEv");
long ZN16wxStyledTextCtrl10GetTextRawEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10GetTextRawEv"); return 0; }

long ZN16wxStyledTextCtrl10GetUseTabsEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10GetUseTabsEv");
long ZN16wxStyledTextCtrl10GetUseTabsEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10GetUseTabsEv"); return 0; }

long ZN16wxStyledTextCtrl10GetViewEOLEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10GetViewEOLEv");
long ZN16wxStyledTextCtrl10GetViewEOLEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10GetViewEOLEv"); return 0; }

long ZN16wxStyledTextCtrl10GetXOffsetEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10GetXOffsetEv");
long ZN16wxStyledTextCtrl10GetXOffsetEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10GetXOffsetEv"); return 0; }

long ZN16wxStyledTextCtrl10HomeExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10HomeExtendEv");
long ZN16wxStyledTextCtrl10HomeExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10HomeExtendEv"); return 0; }

long ZN16wxStyledTextCtrl10InsertTextEiRK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10InsertTextEiRK8wxString");
long ZN16wxStyledTextCtrl10InsertTextEiRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10InsertTextEiRK8wxString"); return 0; }

long ZN16wxStyledTextCtrl10LineDeleteEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10LineDeleteEv");
long ZN16wxStyledTextCtrl10LineDeleteEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10LineDeleteEv"); return 0; }

long ZN16wxStyledTextCtrl10LineLengthEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10LineLengthEi");
long ZN16wxStyledTextCtrl10LineLengthEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10LineLengthEi"); return 0; }

long ZN16wxStyledTextCtrl10LineScrollEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10LineScrollEii");
long ZN16wxStyledTextCtrl10LineScrollEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10LineScrollEii"); return 0; }

long ZN16wxStyledTextCtrl10LinesSplitEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10LinesSplitEi");
long ZN16wxStyledTextCtrl10LinesSplitEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10LinesSplitEi"); return 0; }

long ZN16wxStyledTextCtrl10MarkerNextEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10MarkerNextEii");
long ZN16wxStyledTextCtrl10MarkerNextEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10MarkerNextEii"); return 0; }

long ZN16wxStyledTextCtrl10SearchNextEiRK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10SearchNextEiRK8wxString");
long ZN16wxStyledTextCtrl10SearchNextEiRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10SearchNextEiRK8wxString"); return 0; }

long ZN16wxStyledTextCtrl10SearchPrevEiRK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10SearchPrevEiRK8wxString");
long ZN16wxStyledTextCtrl10SearchPrevEiRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10SearchPrevEiRK8wxString"); return 0; }

long ZN16wxStyledTextCtrl10SetEOLModeEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10SetEOLModeEi");
long ZN16wxStyledTextCtrl10SetEOLModeEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10SetEOLModeEi"); return 0; }

long ZN16wxStyledTextCtrl10SetMarginsEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10SetMarginsEii");
long ZN16wxStyledTextCtrl10SetMarginsEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10SetMarginsEii"); return 0; }

long ZN16wxStyledTextCtrl10SetStylingEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10SetStylingEii");
long ZN16wxStyledTextCtrl10SetStylingEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10SetStylingEii"); return 0; }

long ZN16wxStyledTextCtrl10SetTextRawEPKc(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10SetTextRawEPKc");
long ZN16wxStyledTextCtrl10SetTextRawEPKc(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10SetTextRawEPKc"); return 0; }

long ZN16wxStyledTextCtrl10SetUseTabsEb(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10SetUseTabsEb");
long ZN16wxStyledTextCtrl10SetUseTabsEb(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10SetUseTabsEb"); return 0; }

long ZN16wxStyledTextCtrl10SetViewEOLEb(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10SetViewEOLEb");
long ZN16wxStyledTextCtrl10SetViewEOLEb(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10SetViewEOLEb"); return 0; }

long ZN16wxStyledTextCtrl10SetXOffsetEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10SetXOffsetEi");
long ZN16wxStyledTextCtrl10SetXOffsetEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10SetXOffsetEi"); return 0; }

long ZN16wxStyledTextCtrl10StopRecordEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10StopRecordEv");
long ZN16wxStyledTextCtrl10StopRecordEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10StopRecordEv"); return 0; }

long ZN16wxStyledTextCtrl10TextHeightEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10TextHeightEi");
long ZN16wxStyledTextCtrl10TextHeightEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10TextHeightEi"); return 0; }

long ZN16wxStyledTextCtrl10ToggleFoldEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10ToggleFoldEi");
long ZN16wxStyledTextCtrl10ToggleFoldEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10ToggleFoldEi"); return 0; }

long ZN16wxStyledTextCtrl10VCHomeWrapEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl10VCHomeWrapEv");
long ZN16wxStyledTextCtrl10VCHomeWrapEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl10VCHomeWrapEv"); return 0; }

long ZN16wxStyledTextCtrl11CallTipShowEiRK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11CallTipShowEiRK8wxString");
long ZN16wxStyledTextCtrl11CallTipShowEiRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11CallTipShowEiRK8wxString"); return 0; }

long ZN16wxStyledTextCtrl11CmdKeyClearEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11CmdKeyClearEii");
long ZN16wxStyledTextCtrl11CmdKeyClearEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11CmdKeyClearEii"); return 0; }

long ZN16wxStyledTextCtrl11ConvertEOLsEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11ConvertEOLsEi");
long ZN16wxStyledTextCtrl11ConvertEOLsEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11ConvertEOLsEi"); return 0; }

long ZN16wxStyledTextCtrl11DelLineLeftEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11DelLineLeftEv");
long ZN16wxStyledTextCtrl11DelLineLeftEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11DelLineLeftEv"); return 0; }

long ZN16wxStyledTextCtrl11DelWordLeftEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11DelWordLeftEv");
long ZN16wxStyledTextCtrl11DelWordLeftEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11DelWordLeftEv"); return 0; }

long ZN16wxStyledTextCtrl11DocumentEndEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11DocumentEndEv");
long ZN16wxStyledTextCtrl11DocumentEndEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11DocumentEndEv"); return 0; }

long ZN16wxStyledTextCtrl11FormatRangeEbiiP4wxDCS1_6wxRectS2_(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11FormatRangeEbiiP4wxDCS1_6wxRectS2_");
long ZN16wxStyledTextCtrl11FormatRangeEbiiP4wxDCS1_6wxRectS2_(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11FormatRangeEbiiP4wxDCS1_6wxRectS2_"); return 0; }

long ZN16wxStyledTextCtrl11GetCodePageEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11GetCodePageEv");
long ZN16wxStyledTextCtrl11GetCodePageEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11GetCodePageEv"); return 0; }

long ZN16wxStyledTextCtrl11GetEdgeModeEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11GetEdgeModeEv");
long ZN16wxStyledTextCtrl11GetEdgeModeEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11GetEdgeModeEv"); return 0; }

long ZN16wxStyledTextCtrl11GetOvertypeEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11GetOvertypeEv");
long ZN16wxStyledTextCtrl11GetOvertypeEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11GetOvertypeEv"); return 0; }

long ZN16wxStyledTextCtrl11GetPropertyERK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11GetPropertyERK8wxString");
long ZN16wxStyledTextCtrl11GetPropertyERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11GetPropertyERK8wxString"); return 0; }

long ZN16wxStyledTextCtrl11GetReadOnlyEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11GetReadOnlyEv");
long ZN16wxStyledTextCtrl11GetReadOnlyEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11GetReadOnlyEv"); return 0; }

long ZN16wxStyledTextCtrl11GetSTCFocusEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11GetSTCFocusEv");
long ZN16wxStyledTextCtrl11GetSTCFocusEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11GetSTCFocusEv"); return 0; }

long ZN16wxStyledTextCtrl11GetSelAlphaEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11GetSelAlphaEv");
long ZN16wxStyledTextCtrl11GetSelAlphaEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11GetSelAlphaEv"); return 0; }

long ZN16wxStyledTextCtrl11GetTabWidthEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11GetTabWidthEv");
long ZN16wxStyledTextCtrl11GetTabWidthEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11GetTabWidthEv"); return 0; }

long ZN16wxStyledTextCtrl11GetWrapModeEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11GetWrapModeEv");
long ZN16wxStyledTextCtrl11GetWrapModeEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11GetWrapModeEv"); return 0; }

long ZN16wxStyledTextCtrl11HomeDisplayEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11HomeDisplayEv");
long ZN16wxStyledTextCtrl11HomeDisplayEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11HomeDisplayEv"); return 0; }

long ZN16wxStyledTextCtrl11LineEndWrapEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11LineEndWrapEv");
long ZN16wxStyledTextCtrl11LineEndWrapEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11LineEndWrapEv"); return 0; }

long ZN16wxStyledTextCtrl11SetCodePageEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11SetCodePageEi");
long ZN16wxStyledTextCtrl11SetCodePageEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11SetCodePageEi"); return 0; }

long ZN16wxStyledTextCtrl11SetEdgeModeEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11SetEdgeModeEi");
long ZN16wxStyledTextCtrl11SetEdgeModeEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11SetEdgeModeEi"); return 0; }

long ZN16wxStyledTextCtrl11SetKeyWordsEiRK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11SetKeyWordsEiRK8wxString");
long ZN16wxStyledTextCtrl11SetKeyWordsEiRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11SetKeyWordsEiRK8wxString"); return 0; }

long ZN16wxStyledTextCtrl11SetOvertypeEb(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11SetOvertypeEb");
long ZN16wxStyledTextCtrl11SetOvertypeEb(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11SetOvertypeEb"); return 0; }

long ZN16wxStyledTextCtrl11SetPropertyERK8wxStringS2_(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11SetPropertyERK8wxStringS2_");
long ZN16wxStyledTextCtrl11SetPropertyERK8wxStringS2_(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11SetPropertyERK8wxStringS2_"); return 0; }

long ZN16wxStyledTextCtrl11SetReadOnlyEb(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11SetReadOnlyEb");
long ZN16wxStyledTextCtrl11SetReadOnlyEb(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11SetReadOnlyEb"); return 0; }

long ZN16wxStyledTextCtrl11SetSTCFocusEb(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11SetSTCFocusEb");
long ZN16wxStyledTextCtrl11SetSTCFocusEb(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11SetSTCFocusEb"); return 0; }

long ZN16wxStyledTextCtrl11SetSelAlphaEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11SetSelAlphaEi");
long ZN16wxStyledTextCtrl11SetSelAlphaEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11SetSelAlphaEi"); return 0; }

long ZN16wxStyledTextCtrl11SetTabWidthEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11SetTabWidthEi");
long ZN16wxStyledTextCtrl11SetTabWidthEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11SetTabWidthEi"); return 0; }

long ZN16wxStyledTextCtrl11SetWrapModeEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11SetWrapModeEi");
long ZN16wxStyledTextCtrl11SetWrapModeEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11SetWrapModeEi"); return 0; }

long ZN16wxStyledTextCtrl11StartRecordEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11StartRecordEv");
long ZN16wxStyledTextCtrl11StartRecordEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11StartRecordEv"); return 0; }

long ZN16wxStyledTextCtrl11WordLeftEndEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl11WordLeftEndEv");
long ZN16wxStyledTextCtrl11WordLeftEndEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl11WordLeftEndEv"); return 0; }

long ZN16wxStyledTextCtrl12AutoCompShowEiRK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12AutoCompShowEiRK8wxString");
long ZN16wxStyledTextCtrl12AutoCompShowEiRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12AutoCompShowEiRK8wxString"); return 0; }

long ZN16wxStyledTextCtrl12ChooseCaretXEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12ChooseCaretXEv");
long ZN16wxStyledTextCtrl12ChooseCaretXEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12ChooseCaretXEv"); return 0; }

long ZN16wxStyledTextCtrl12CmdKeyAssignEiii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12CmdKeyAssignEiii");
long ZN16wxStyledTextCtrl12CmdKeyAssignEiii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12CmdKeyAssignEiii"); return 0; }

long ZN16wxStyledTextCtrl12DelLineRightEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12DelLineRightEv");
long ZN16wxStyledTextCtrl12DelLineRightEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12DelLineRightEv"); return 0; }

long ZN16wxStyledTextCtrl12DelWordRightEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12DelWordRightEv");
long ZN16wxStyledTextCtrl12DelWordRightEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12DelWordRightEv"); return 0; }

long ZN16wxStyledTextCtrl12GetEndStyledEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12GetEndStyledEv");
long ZN16wxStyledTextCtrl12GetEndStyledEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12GetEndStyledEv"); return 0; }

long ZN16wxStyledTextCtrl12GetFoldLevelEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12GetFoldLevelEi");
long ZN16wxStyledTextCtrl12GetFoldLevelEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12GetFoldLevelEi"); return 0; }

long ZN16wxStyledTextCtrl12GetLastChildEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12GetLastChildEii");
long ZN16wxStyledTextCtrl12GetLastChildEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12GetLastChildEii"); return 0; }

long ZN16wxStyledTextCtrl12GetLineCountEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12GetLineCountEv");
long ZN16wxStyledTextCtrl12GetLineCountEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12GetLineCountEv"); return 0; }

long ZN16wxStyledTextCtrl12GetLineStateEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12GetLineStateEi");
long ZN16wxStyledTextCtrl12GetLineStateEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12GetLineStateEi"); return 0; }

long ZN16wxStyledTextCtrl12GetSTCCursorEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12GetSTCCursorEv");
long ZN16wxStyledTextCtrl12GetSTCCursorEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12GetSTCCursorEv"); return 0; }

long ZN16wxStyledTextCtrl12GetSelectionEPiS0_(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12GetSelectionEPiS0_");
long ZN16wxStyledTextCtrl12GetSelectionEPiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12GetSelectionEPiS0_"); return 0; }

long ZN16wxStyledTextCtrl12GetStyleBitsEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12GetStyleBitsEv");
long ZN16wxStyledTextCtrl12GetStyleBitsEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12GetStyleBitsEv"); return 0; }

long ZN16wxStyledTextCtrl12GetTargetEndEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12GetTargetEndEv");
long ZN16wxStyledTextCtrl12GetTargetEndEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12GetTargetEndEv"); return 0; }

long ZN16wxStyledTextCtrl12GetTextRangeEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12GetTextRangeEii");
long ZN16wxStyledTextCtrl12GetTextRangeEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12GetTextRangeEii"); return 0; }

long ZN16wxStyledTextCtrl12LineScrollUpEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12LineScrollUpEv");
long ZN16wxStyledTextCtrl12LineScrollUpEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12LineScrollUpEv"); return 0; }

long ZN16wxStyledTextCtrl12LineUpExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12LineUpExtendEv");
long ZN16wxStyledTextCtrl12LineUpExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12LineUpExtendEv"); return 0; }

long ZN16wxStyledTextCtrl12MarkerAddSetEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12MarkerAddSetEii");
long ZN16wxStyledTextCtrl12MarkerAddSetEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12MarkerAddSetEii"); return 0; }

long ZN16wxStyledTextCtrl12MarkerDefineEiiRK8wxColourS2_(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12MarkerDefineEiiRK8wxColourS2_");
long ZN16wxStyledTextCtrl12MarkerDefineEiiRK8wxColourS2_(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12MarkerDefineEiiRK8wxColourS2_"); return 0; }

long ZN16wxStyledTextCtrl12MarkerDeleteEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12MarkerDeleteEii");
long ZN16wxStyledTextCtrl12MarkerDeleteEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12MarkerDeleteEii"); return 0; }

long ZN16wxStyledTextCtrl12PageUpExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12PageUpExtendEv");
long ZN16wxStyledTextCtrl12PageUpExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12PageUpExtendEv"); return 0; }

long ZN16wxStyledTextCtrl12ParaUpExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12ParaUpExtendEv");
long ZN16wxStyledTextCtrl12ParaUpExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12ParaUpExtendEv"); return 0; }

long ZN16wxStyledTextCtrl12ScrollToLineEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12ScrollToLineEi");
long ZN16wxStyledTextCtrl12ScrollToLineEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12ScrollToLineEi"); return 0; }

long ZN16wxStyledTextCtrl12SearchAnchorEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12SearchAnchorEv");
long ZN16wxStyledTextCtrl12SearchAnchorEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12SearchAnchorEv"); return 0; }

long ZN16wxStyledTextCtrl12SetFoldFlagsEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12SetFoldFlagsEi");
long ZN16wxStyledTextCtrl12SetFoldFlagsEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12SetFoldFlagsEi"); return 0; }

long ZN16wxStyledTextCtrl12SetFoldLevelEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12SetFoldLevelEii");
long ZN16wxStyledTextCtrl12SetFoldLevelEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12SetFoldLevelEii"); return 0; }

long ZN16wxStyledTextCtrl12SetLineStateEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12SetLineStateEii");
long ZN16wxStyledTextCtrl12SetLineStateEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12SetLineStateEii"); return 0; }

long ZN16wxStyledTextCtrl12SetSTCCursorEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12SetSTCCursorEi");
long ZN16wxStyledTextCtrl12SetSTCCursorEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12SetSTCCursorEi"); return 0; }

long ZN16wxStyledTextCtrl12SetSavePointEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12SetSavePointEv");
long ZN16wxStyledTextCtrl12SetSavePointEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12SetSavePointEv"); return 0; }

long ZN16wxStyledTextCtrl12SetSelectionEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12SetSelectionEii");
long ZN16wxStyledTextCtrl12SetSelectionEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12SetSelectionEii"); return 0; }

long ZN16wxStyledTextCtrl12SetStyleBitsEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12SetStyleBitsEi");
long ZN16wxStyledTextCtrl12SetStyleBitsEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12SetStyleBitsEi"); return 0; }

long ZN16wxStyledTextCtrl12SetTargetEndEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12SetTargetEndEi");
long ZN16wxStyledTextCtrl12SetTargetEndEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12SetTargetEndEi"); return 0; }

long ZN16wxStyledTextCtrl12SetWordCharsERK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12SetWordCharsERK8wxString");
long ZN16wxStyledTextCtrl12SetWordCharsERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12SetWordCharsERK8wxString"); return 0; }

long ZN16wxStyledTextCtrl12StartStylingEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12StartStylingEii");
long ZN16wxStyledTextCtrl12StartStylingEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12StartStylingEii"); return 0; }

long ZN16wxStyledTextCtrl12StyleSetBoldEib(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12StyleSetBoldEib");
long ZN16wxStyledTextCtrl12StyleSetBoldEib(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12StyleSetBoldEib"); return 0; }

long ZN16wxStyledTextCtrl12StyleSetCaseEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12StyleSetCaseEii");
long ZN16wxStyledTextCtrl12StyleSetCaseEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12StyleSetCaseEii"); return 0; }

long ZN16wxStyledTextCtrl12StyleSetFontEiR6wxFont(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12StyleSetFontEiR6wxFont");
long ZN16wxStyledTextCtrl12StyleSetFontEiR6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12StyleSetFontEiR6wxFont"); return 0; }

long ZN16wxStyledTextCtrl12StyleSetSizeEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12StyleSetSizeEii");
long ZN16wxStyledTextCtrl12StyleSetSizeEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12StyleSetSizeEii"); return 0; }

long ZN16wxStyledTextCtrl12StyleSetSpecEiRK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12StyleSetSpecEiRK8wxString");
long ZN16wxStyledTextCtrl12StyleSetSpecEiRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12StyleSetSpecEiRK8wxString"); return 0; }

long ZN16wxStyledTextCtrl12UserListShowEiRK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12UserListShowEiRK8wxString");
long ZN16wxStyledTextCtrl12UserListShowEiRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12UserListShowEiRK8wxString"); return 0; }

long ZN16wxStyledTextCtrl12VCHomeExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12VCHomeExtendEv");
long ZN16wxStyledTextCtrl12VCHomeExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12VCHomeExtendEv"); return 0; }

long ZN16wxStyledTextCtrl12WordPartLeftEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12WordPartLeftEv");
long ZN16wxStyledTextCtrl12WordPartLeftEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12WordPartLeftEv"); return 0; }

long ZN16wxStyledTextCtrl12WordRightEndEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl12WordRightEndEv");
long ZN16wxStyledTextCtrl12WordRightEndEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl12WordRightEndEv"); return 0; }

long ZN16wxStyledTextCtrl13AddStyledTextERK14wxMemoryBuffer(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13AddStyledTextERK14wxMemoryBuffer");
long ZN16wxStyledTextCtrl13AddStyledTextERK14wxMemoryBuffer(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13AddStyledTextERK14wxMemoryBuffer"); return 0; }

long ZN16wxStyledTextCtrl13AppendTextRawEPKc(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13AppendTextRawEPKc");
long ZN16wxStyledTextCtrl13AppendTextRawEPKc(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13AppendTextRawEPKc"); return 0; }

long ZN16wxStyledTextCtrl13AutoCompStopsERK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13AutoCompStopsERK8wxString");
long ZN16wxStyledTextCtrl13AutoCompStopsERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13AutoCompStopsERK8wxString"); return 0; }

long ZN16wxStyledTextCtrl13BraceBadLightEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13BraceBadLightEi");
long ZN16wxStyledTextCtrl13BraceBadLightEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13BraceBadLightEi"); return 0; }

long ZN16wxStyledTextCtrl13CallTipActiveEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13CallTipActiveEv");
long ZN16wxStyledTextCtrl13CallTipActiveEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13CallTipActiveEv"); return 0; }

long ZN16wxStyledTextCtrl13CallTipCancelEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13CallTipCancelEv");
long ZN16wxStyledTextCtrl13CallTipCancelEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13CallTipCancelEv"); return 0; }

long ZN16wxStyledTextCtrl13CmdKeyExecuteEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13CmdKeyExecuteEi");
long ZN16wxStyledTextCtrl13CmdKeyExecuteEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13CmdKeyExecuteEi"); return 0; }

long ZN16wxStyledTextCtrl13DocumentStartEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13DocumentStartEv");
long ZN16wxStyledTextCtrl13DocumentStartEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13DocumentStartEv"); return 0; }

long ZN16wxStyledTextCtrl13EndUndoActionEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13EndUndoActionEv");
long ZN16wxStyledTextCtrl13EndUndoActionEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13EndUndoActionEv"); return 0; }

long ZN16wxStyledTextCtrl13EnsureVisibleEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13EnsureVisibleEi");
long ZN16wxStyledTextCtrl13EnsureVisibleEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13EnsureVisibleEi"); return 0; }

long ZN16wxStyledTextCtrl13GetCaretWidthEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13GetCaretWidthEv");
long ZN16wxStyledTextCtrl13GetCaretWidthEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13GetCaretWidthEv"); return 0; }

long ZN16wxStyledTextCtrl13GetCurLineRawEPi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13GetCurLineRawEPi");
long ZN16wxStyledTextCtrl13GetCurLineRawEPi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13GetCurLineRawEPi"); return 0; }

long ZN16wxStyledTextCtrl13GetCurrentPosEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13GetCurrentPosEv");
long ZN16wxStyledTextCtrl13GetCurrentPosEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13GetCurrentPosEv"); return 0; }

long ZN16wxStyledTextCtrl13GetDocPointerEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13GetDocPointerEv");
long ZN16wxStyledTextCtrl13GetDocPointerEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13GetDocPointerEv"); return 0; }

long ZN16wxStyledTextCtrl13GetEdgeColourEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13GetEdgeColourEv");
long ZN16wxStyledTextCtrl13GetEdgeColourEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13GetEdgeColourEv"); return 0; }

long ZN16wxStyledTextCtrl13GetEdgeColumnEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13GetEdgeColumnEv");
long ZN16wxStyledTextCtrl13GetEdgeColumnEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13GetEdgeColumnEv"); return 0; }

long ZN16wxStyledTextCtrl13GetFoldParentEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13GetFoldParentEi");
long ZN16wxStyledTextCtrl13GetFoldParentEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13GetFoldParentEi"); return 0; }

long ZN16wxStyledTextCtrl13GetMarginLeftEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13GetMarginLeftEv");
long ZN16wxStyledTextCtrl13GetMarginLeftEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13GetMarginLeftEv"); return 0; }

long ZN16wxStyledTextCtrl13GetMarginMaskEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13GetMarginMaskEi");
long ZN16wxStyledTextCtrl13GetMarginMaskEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13GetMarginMaskEi"); return 0; }

long ZN16wxStyledTextCtrl13GetMarginTypeEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13GetMarginTypeEi");
long ZN16wxStyledTextCtrl13GetMarginTypeEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13GetMarginTypeEi"); return 0; }

long ZN16wxStyledTextCtrl13GetStyledTextEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13GetStyledTextEii");
long ZN16wxStyledTextCtrl13GetStyledTextEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13GetStyledTextEii"); return 0; }

long ZN16wxStyledTextCtrl13GetTabIndentsEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13GetTabIndentsEv");
long ZN16wxStyledTextCtrl13GetTabIndentsEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13GetTabIndentsEv"); return 0; }

long ZN16wxStyledTextCtrl13GetTextLengthEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13GetTextLengthEv");
long ZN16wxStyledTextCtrl13GetTextLengthEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13GetTextLengthEv"); return 0; }

long ZN16wxStyledTextCtrl13HideSelectionEb(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13HideSelectionEb");
long ZN16wxStyledTextCtrl13HideSelectionEb(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13HideSelectionEb"); return 0; }

long ZN16wxStyledTextCtrl13InsertTextRawEiPKc(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13InsertTextRawEiPKc");
long ZN16wxStyledTextCtrl13InsertTextRawEiPKc(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13InsertTextRawEiPKc"); return 0; }

long ZN16wxStyledTextCtrl13LineDuplicateEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13LineDuplicateEv");
long ZN16wxStyledTextCtrl13LineDuplicateEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13LineDuplicateEv"); return 0; }

long ZN16wxStyledTextCtrl13LineEndExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13LineEndExtendEv");
long ZN16wxStyledTextCtrl13LineEndExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13LineEndExtendEv"); return 0; }

long ZN16wxStyledTextCtrl13LineTransposeEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13LineTransposeEv");
long ZN16wxStyledTextCtrl13LineTransposeEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13LineTransposeEv"); return 0; }

long ZN16wxStyledTextCtrl13LinesOnScreenEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13LinesOnScreenEv");
long ZN16wxStyledTextCtrl13LinesOnScreenEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13LinesOnScreenEv"); return 0; }

long ZN16wxStyledTextCtrl13PositionAfterEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13PositionAfterEi");
long ZN16wxStyledTextCtrl13PositionAfterEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13PositionAfterEi"); return 0; }

long ZN16wxStyledTextCtrl13RegisterImageEiRK8wxBitmap(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13RegisterImageEiRK8wxBitmap");
long ZN16wxStyledTextCtrl13RegisterImageEiRK8wxBitmap(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13RegisterImageEiRK8wxBitmap"); return 0; }

long ZN16wxStyledTextCtrl13ReplaceTargetERK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13ReplaceTargetERK8wxString");
long ZN16wxStyledTextCtrl13ReplaceTargetERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13ReplaceTargetERK8wxString"); return 0; }

long ZN16wxStyledTextCtrl13SetCaretWidthEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13SetCaretWidthEi");
long ZN16wxStyledTextCtrl13SetCaretWidthEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13SetCaretWidthEi"); return 0; }

long ZN16wxStyledTextCtrl13SetCurrentPosEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13SetCurrentPosEi");
long ZN16wxStyledTextCtrl13SetCurrentPosEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13SetCurrentPosEi"); return 0; }

long ZN16wxStyledTextCtrl13SetDocPointerEPv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13SetDocPointerEPv");
long ZN16wxStyledTextCtrl13SetDocPointerEPv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13SetDocPointerEPv"); return 0; }

long ZN16wxStyledTextCtrl13SetEdgeColourERK8wxColour(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13SetEdgeColourERK8wxColour");
long ZN16wxStyledTextCtrl13SetEdgeColourERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13SetEdgeColourERK8wxColour"); return 0; }

long ZN16wxStyledTextCtrl13SetEdgeColumnEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13SetEdgeColumnEi");
long ZN16wxStyledTextCtrl13SetEdgeColumnEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13SetEdgeColumnEi"); return 0; }

long ZN16wxStyledTextCtrl13SetHScrollBarEP11wxScrollBar(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13SetHScrollBarEP11wxScrollBar");
long ZN16wxStyledTextCtrl13SetHScrollBarEP11wxScrollBar(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13SetHScrollBarEP11wxScrollBar"); return 0; }

long ZN16wxStyledTextCtrl13SetMarginLeftEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13SetMarginLeftEi");
long ZN16wxStyledTextCtrl13SetMarginLeftEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13SetMarginLeftEi"); return 0; }

long ZN16wxStyledTextCtrl13SetMarginMaskEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13SetMarginMaskEii");
long ZN16wxStyledTextCtrl13SetMarginMaskEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13SetMarginMaskEii"); return 0; }

long ZN16wxStyledTextCtrl13SetMarginTypeEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13SetMarginTypeEii");
long ZN16wxStyledTextCtrl13SetMarginTypeEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13SetMarginTypeEii"); return 0; }

long ZN16wxStyledTextCtrl13SetStyleBytesEiPc(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13SetStyleBytesEiPc");
long ZN16wxStyledTextCtrl13SetStyleBytesEiPc(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13SetStyleBytesEiPc"); return 0; }

long ZN16wxStyledTextCtrl13SetTabIndentsEb(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13SetTabIndentsEb");
long ZN16wxStyledTextCtrl13SetTabIndentsEb(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13SetTabIndentsEb"); return 0; }

long ZN16wxStyledTextCtrl13SetVScrollBarEP11wxScrollBar(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13SetVScrollBarEP11wxScrollBar");
long ZN16wxStyledTextCtrl13SetVScrollBarEP11wxScrollBar(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13SetVScrollBarEP11wxScrollBar"); return 0; }

long ZN16wxStyledTextCtrl13StyleClearAllEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13StyleClearAllEv");
long ZN16wxStyledTextCtrl13StyleClearAllEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13StyleClearAllEv"); return 0; }

long ZN16wxStyledTextCtrl13WordPartRightEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl13WordPartRightEv");
long ZN16wxStyledTextCtrl13WordPartRightEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl13WordPartRightEv"); return 0; }

long ZN16wxStyledTextCtrl14AddRefDocumentEPv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14AddRefDocumentEPv");
long ZN16wxStyledTextCtrl14AddRefDocumentEPv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14AddRefDocumentEPv"); return 0; }

long ZN16wxStyledTextCtrl14AutoCompActiveEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14AutoCompActiveEv");
long ZN16wxStyledTextCtrl14AutoCompActiveEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14AutoCompActiveEv"); return 0; }

long ZN16wxStyledTextCtrl14AutoCompCancelEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14AutoCompCancelEv");
long ZN16wxStyledTextCtrl14AutoCompCancelEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14AutoCompCancelEv"); return 0; }

long ZN16wxStyledTextCtrl14AutoCompSelectERK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14AutoCompSelectERK8wxString");
long ZN16wxStyledTextCtrl14AutoCompSelectERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14AutoCompSelectERK8wxString"); return 0; }

long ZN16wxStyledTextCtrl14BraceHighlightEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14BraceHighlightEii");
long ZN16wxStyledTextCtrl14BraceHighlightEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14BraceHighlightEii"); return 0; }

long ZN16wxStyledTextCtrl14CharLeftExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14CharLeftExtendEv");
long ZN16wxStyledTextCtrl14CharLeftExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14CharLeftExtendEv"); return 0; }

long ZN16wxStyledTextCtrl14CmdKeyClearAllEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14CmdKeyClearAllEv");
long ZN16wxStyledTextCtrl14CmdKeyClearAllEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14CmdKeyClearAllEv"); return 0; }

long ZN16wxStyledTextCtrl14CreateDocumentEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14CreateDocumentEv");
long ZN16wxStyledTextCtrl14CreateDocumentEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14CreateDocumentEv"); return 0; }

long ZN16wxStyledTextCtrl14GetCaretPeriodEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14GetCaretPeriodEv");
long ZN16wxStyledTextCtrl14GetCaretPeriodEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14GetCaretPeriodEv"); return 0; }

long ZN16wxStyledTextCtrl14GetCaretStickyEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14GetCaretStickyEv");
long ZN16wxStyledTextCtrl14GetCaretStickyEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14GetCaretStickyEv"); return 0; }

long ZN16wxStyledTextCtrl14GetCurrentLineEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14GetCurrentLineEv");
long ZN16wxStyledTextCtrl14GetCurrentLineEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14GetCurrentLineEv"); return 0; }

long ZN16wxStyledTextCtrl14GetLayoutCacheEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14GetLayoutCacheEv");
long ZN16wxStyledTextCtrl14GetLayoutCacheEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14GetLayoutCacheEv"); return 0; }

long ZN16wxStyledTextCtrl14GetLineVisibleEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14GetLineVisibleEi");
long ZN16wxStyledTextCtrl14GetLineVisibleEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14GetLineVisibleEi"); return 0; }

long ZN16wxStyledTextCtrl14GetMarginRightEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14GetMarginRightEv");
long ZN16wxStyledTextCtrl14GetMarginRightEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14GetMarginRightEv"); return 0; }

long ZN16wxStyledTextCtrl14GetMarginWidthEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14GetMarginWidthEi");
long ZN16wxStyledTextCtrl14GetMarginWidthEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14GetMarginWidthEi"); return 0; }

long ZN16wxStyledTextCtrl14GetPropertyIntERK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14GetPropertyIntERK8wxString");
long ZN16wxStyledTextCtrl14GetPropertyIntERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14GetPropertyIntERK8wxString"); return 0; }

long ZN16wxStyledTextCtrl14GetScrollWidthEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14GetScrollWidthEv");
long ZN16wxStyledTextCtrl14GetScrollWidthEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14GetScrollWidthEv"); return 0; }

long ZN16wxStyledTextCtrl14GetSearchFlagsEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14GetSearchFlagsEv");
long ZN16wxStyledTextCtrl14GetSearchFlagsEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14GetSearchFlagsEv"); return 0; }

long ZN16wxStyledTextCtrl14GetTargetStartEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14GetTargetStartEv");
long ZN16wxStyledTextCtrl14GetTargetStartEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14GetTargetStartEv"); return 0; }

long ZN16wxStyledTextCtrl14HomeRectExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14HomeRectExtendEv");
long ZN16wxStyledTextCtrl14HomeRectExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14HomeRectExtendEv"); return 0; }

long ZN16wxStyledTextCtrl14HomeWrapExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14HomeWrapExtendEv");
long ZN16wxStyledTextCtrl14HomeWrapExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14HomeWrapExtendEv"); return 0; }

long ZN16wxStyledTextCtrl14LineDownExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14LineDownExtendEv");
long ZN16wxStyledTextCtrl14LineDownExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14LineDownExtendEv"); return 0; }

long ZN16wxStyledTextCtrl14LineEndDisplayEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14LineEndDisplayEv");
long ZN16wxStyledTextCtrl14LineEndDisplayEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14LineEndDisplayEv"); return 0; }

long ZN16wxStyledTextCtrl14LineScrollDownEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14LineScrollDownEv");
long ZN16wxStyledTextCtrl14LineScrollDownEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14LineScrollDownEv"); return 0; }

long ZN16wxStyledTextCtrl14MarkerPreviousEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14MarkerPreviousEii");
long ZN16wxStyledTextCtrl14MarkerPreviousEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14MarkerPreviousEii"); return 0; }

long ZN16wxStyledTextCtrl14MarkerSetAlphaEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14MarkerSetAlphaEii");
long ZN16wxStyledTextCtrl14MarkerSetAlphaEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14MarkerSetAlphaEii"); return 0; }

long ZN16wxStyledTextCtrl14PageDownExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14PageDownExtendEv");
long ZN16wxStyledTextCtrl14PageDownExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14PageDownExtendEv"); return 0; }

long ZN16wxStyledTextCtrl14ParaDownExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14ParaDownExtendEv");
long ZN16wxStyledTextCtrl14ParaDownExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14ParaDownExtendEv"); return 0; }

long ZN16wxStyledTextCtrl14PositionBeforeEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14PositionBeforeEi");
long ZN16wxStyledTextCtrl14PositionBeforeEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14PositionBeforeEi"); return 0; }

long ZN16wxStyledTextCtrl14ScrollToColumnEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14ScrollToColumnEi");
long ZN16wxStyledTextCtrl14ScrollToColumnEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14ScrollToColumnEi"); return 0; }

long ZN16wxStyledTextCtrl14SearchInTargetERK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14SearchInTargetERK8wxString");
long ZN16wxStyledTextCtrl14SearchInTargetERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14SearchInTargetERK8wxString"); return 0; }

long ZN16wxStyledTextCtrl14SetCaretPeriodEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14SetCaretPeriodEi");
long ZN16wxStyledTextCtrl14SetCaretPeriodEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14SetCaretPeriodEi"); return 0; }

long ZN16wxStyledTextCtrl14SetCaretStickyEb(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14SetCaretStickyEb");
long ZN16wxStyledTextCtrl14SetCaretStickyEb(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14SetCaretStickyEb"); return 0; }

long ZN16wxStyledTextCtrl14SetLayoutCacheEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14SetLayoutCacheEi");
long ZN16wxStyledTextCtrl14SetLayoutCacheEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14SetLayoutCacheEi"); return 0; }

long ZN16wxStyledTextCtrl14SetMarginRightEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14SetMarginRightEi");
long ZN16wxStyledTextCtrl14SetMarginRightEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14SetMarginRightEi"); return 0; }

long ZN16wxStyledTextCtrl14SetMarginWidthEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14SetMarginWidthEii");
long ZN16wxStyledTextCtrl14SetMarginWidthEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14SetMarginWidthEii"); return 0; }

long ZN16wxStyledTextCtrl14SetScrollWidthEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14SetScrollWidthEi");
long ZN16wxStyledTextCtrl14SetScrollWidthEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14SetScrollWidthEi"); return 0; }

long ZN16wxStyledTextCtrl14SetSearchFlagsEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14SetSearchFlagsEi");
long ZN16wxStyledTextCtrl14SetSearchFlagsEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14SetSearchFlagsEi"); return 0; }

long ZN16wxStyledTextCtrl14SetTargetStartEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14SetTargetStartEi");
long ZN16wxStyledTextCtrl14SetTargetStartEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14SetTargetStartEi"); return 0; }

long ZN16wxStyledTextCtrl14StyleSetItalicEib(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14StyleSetItalicEib");
long ZN16wxStyledTextCtrl14StyleSetItalicEib(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14StyleSetItalicEib"); return 0; }

long ZN16wxStyledTextCtrl14WordLeftExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl14WordLeftExtendEv");
long ZN16wxStyledTextCtrl14WordLeftExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl14WordLeftExtendEv"); return 0; }

long ZN16wxStyledTextCtrl15BeginUndoActionEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl15BeginUndoActionEv");
long ZN16wxStyledTextCtrl15BeginUndoActionEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl15BeginUndoActionEv"); return 0; }

long ZN16wxStyledTextCtrl15CallTipUseStyleEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl15CallTipUseStyleEi");
long ZN16wxStyledTextCtrl15CallTipUseStyleEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl15CallTipUseStyleEi"); return 0; }

long ZN16wxStyledTextCtrl15CharRightExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl15CharRightExtendEv");
long ZN16wxStyledTextCtrl15CharRightExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl15CharRightExtendEv"); return 0; }

long ZN16wxStyledTextCtrl15EmptyUndoBufferEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl15EmptyUndoBufferEv");
long ZN16wxStyledTextCtrl15EmptyUndoBufferEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl15EmptyUndoBufferEv"); return 0; }

long ZN16wxStyledTextCtrl15GetBufferedDrawEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl15GetBufferedDrawEv");
long ZN16wxStyledTextCtrl15GetBufferedDrawEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl15GetBufferedDrawEv"); return 0; }

long ZN16wxStyledTextCtrl15GetFoldExpandedEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl15GetFoldExpandedEi");
long ZN16wxStyledTextCtrl15GetFoldExpandedEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl15GetFoldExpandedEi"); return 0; }

long ZN16wxStyledTextCtrl15GetMaxLineStateEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl15GetMaxLineStateEv");
long ZN16wxStyledTextCtrl15GetMaxLineStateEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl15GetMaxLineStateEv"); return 0; }

long ZN16wxStyledTextCtrl15GetModEventMaskEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl15GetModEventMaskEv");
long ZN16wxStyledTextCtrl15GetModEventMaskEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl15GetModEventMaskEv"); return 0; }

long ZN16wxStyledTextCtrl15GetSelectedTextEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl15GetSelectedTextEv");
long ZN16wxStyledTextCtrl15GetSelectedTextEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl15GetSelectedTextEv"); return 0; }

long ZN16wxStyledTextCtrl15GetSelectionEndEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl15GetSelectionEndEv");
long ZN16wxStyledTextCtrl15GetSelectionEndEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl15GetSelectionEndEv"); return 0; }

long ZN16wxStyledTextCtrl15GetTextRangeRawEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl15GetTextRangeRawEii");
long ZN16wxStyledTextCtrl15GetTextRangeRawEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl15GetTextRangeRawEii"); return 0; }

long ZN16wxStyledTextCtrl15GetTwoPhaseDrawEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl15GetTwoPhaseDrawEv");
long ZN16wxStyledTextCtrl15GetTwoPhaseDrawEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl15GetTwoPhaseDrawEv"); return 0; }

long ZN16wxStyledTextCtrl15MarkerDeleteAllEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl15MarkerDeleteAllEi");
long ZN16wxStyledTextCtrl15MarkerDeleteAllEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl15MarkerDeleteAllEi"); return 0; }

long ZN16wxStyledTextCtrl15ReleaseDocumentEPv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl15ReleaseDocumentEPv");
long ZN16wxStyledTextCtrl15ReleaseDocumentEPv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl15ReleaseDocumentEPv"); return 0; }

long ZN16wxStyledTextCtrl15ReplaceTargetREERK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl15ReplaceTargetREERK8wxString");
long ZN16wxStyledTextCtrl15ReplaceTargetREERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl15ReplaceTargetREERK8wxString"); return 0; }

long ZN16wxStyledTextCtrl15SetBufferedDrawEb(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl15SetBufferedDrawEb");
long ZN16wxStyledTextCtrl15SetBufferedDrawEb(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl15SetBufferedDrawEb"); return 0; }

long ZN16wxStyledTextCtrl15SetCharsDefaultEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl15SetCharsDefaultEv");
long ZN16wxStyledTextCtrl15SetCharsDefaultEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl15SetCharsDefaultEv"); return 0; }

long ZN16wxStyledTextCtrl15SetFoldExpandedEib(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl15SetFoldExpandedEib");
long ZN16wxStyledTextCtrl15SetFoldExpandedEib(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl15SetFoldExpandedEib"); return 0; }

long ZN16wxStyledTextCtrl15SetModEventMaskEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl15SetModEventMaskEi");
long ZN16wxStyledTextCtrl15SetModEventMaskEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl15SetModEventMaskEi"); return 0; }

long ZN16wxStyledTextCtrl15SetSelectionEndEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl15SetSelectionEndEi");
long ZN16wxStyledTextCtrl15SetSelectionEndEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl15SetSelectionEndEi"); return 0; }

long ZN16wxStyledTextCtrl15SetTwoPhaseDrawEb(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl15SetTwoPhaseDrawEb");
long ZN16wxStyledTextCtrl15SetTwoPhaseDrawEb(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl15SetTwoPhaseDrawEb"); return 0; }

long ZN16wxStyledTextCtrl15SetXCaretPolicyEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl15SetXCaretPolicyEii");
long ZN16wxStyledTextCtrl15SetXCaretPolicyEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl15SetXCaretPolicyEii"); return 0; }

long ZN16wxStyledTextCtrl15SetYCaretPolicyEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl15SetYCaretPolicyEii");
long ZN16wxStyledTextCtrl15SetYCaretPolicyEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl15SetYCaretPolicyEii"); return 0; }

long ZN16wxStyledTextCtrl15StutteredPageUpEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl15StutteredPageUpEv");
long ZN16wxStyledTextCtrl15StutteredPageUpEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl15StutteredPageUpEv"); return 0; }

long ZN16wxStyledTextCtrl15StyleSetHotSpotEib(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl15StyleSetHotSpotEib");
long ZN16wxStyledTextCtrl15StyleSetHotSpotEib(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl15StyleSetHotSpotEib"); return 0; }

long ZN16wxStyledTextCtrl15StyleSetVisibleEib(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl15StyleSetVisibleEib");
long ZN16wxStyledTextCtrl15StyleSetVisibleEib(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl15StyleSetVisibleEib"); return 0; }

long ZN16wxStyledTextCtrl15WordEndPositionEib(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl15WordEndPositionEib");
long ZN16wxStyledTextCtrl15WordEndPositionEib(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl15WordEndPositionEib"); return 0; }

long ZN16wxStyledTextCtrl15WordRightExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl15WordRightExtendEv");
long ZN16wxStyledTextCtrl15WordRightExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl15WordRightExtendEv"); return 0; }

long ZN16wxStyledTextCtrl16AutoCompCompleteEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl16AutoCompCompleteEv");
long ZN16wxStyledTextCtrl16AutoCompCompleteEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl16AutoCompCompleteEv"); return 0; }

long ZN16wxStyledTextCtrl16AutoCompPosStartEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl16AutoCompPosStartEv");
long ZN16wxStyledTextCtrl16AutoCompPosStartEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl16AutoCompPosStartEv"); return 0; }

long ZN16wxStyledTextCtrl16GetEndAtLastLineEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl16GetEndAtLastLineEv");
long ZN16wxStyledTextCtrl16GetEndAtLastLineEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl16GetEndAtLastLineEv"); return 0; }

long ZN16wxStyledTextCtrl16GetPrintWrapModeEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl16GetPrintWrapModeEv");
long ZN16wxStyledTextCtrl16GetPrintWrapModeEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl16GetPrintWrapModeEv"); return 0; }

long ZN16wxStyledTextCtrl16GetSelectionModeEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl16GetSelectionModeEv");
long ZN16wxStyledTextCtrl16GetSelectionModeEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl16GetSelectionModeEv"); return 0; }

long ZN16wxStyledTextCtrl16LineFromPositionEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl16LineFromPositionEi");
long ZN16wxStyledTextCtrl16LineFromPositionEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl16LineFromPositionEi"); return 0; }

long ZN16wxStyledTextCtrl16LineUpRectExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl16LineUpRectExtendEv");
long ZN16wxStyledTextCtrl16LineUpRectExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl16LineUpRectExtendEv"); return 0; }

long ZN16wxStyledTextCtrl16PageUpRectExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl16PageUpRectExtendEv");
long ZN16wxStyledTextCtrl16PageUpRectExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl16PageUpRectExtendEv"); return 0; }

long ZN16wxStyledTextCtrl16PositionFromLineEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl16PositionFromLineEi");
long ZN16wxStyledTextCtrl16PositionFromLineEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl16PositionFromLineEi"); return 0; }

long ZN16wxStyledTextCtrl16ReplaceSelectionERK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl16ReplaceSelectionERK8wxString");
long ZN16wxStyledTextCtrl16ReplaceSelectionERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl16ReplaceSelectionERK8wxString"); return 0; }

long ZN16wxStyledTextCtrl16SetEndAtLastLineEb(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl16SetEndAtLastLineEb");
long ZN16wxStyledTextCtrl16SetEndAtLastLineEb(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl16SetEndAtLastLineEb"); return 0; }

long ZN16wxStyledTextCtrl16SetLexerLanguageERK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl16SetLexerLanguageERK8wxString");
long ZN16wxStyledTextCtrl16SetLexerLanguageERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl16SetLexerLanguageERK8wxString"); return 0; }

long ZN16wxStyledTextCtrl16SetPrintWrapModeEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl16SetPrintWrapModeEi");
long ZN16wxStyledTextCtrl16SetPrintWrapModeEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl16SetPrintWrapModeEi"); return 0; }

long ZN16wxStyledTextCtrl16SetSelBackgroundEbRK8wxColour(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl16SetSelBackgroundEbRK8wxColour");
long ZN16wxStyledTextCtrl16SetSelBackgroundEbRK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl16SetSelBackgroundEbRK8wxColour"); return 0; }

long ZN16wxStyledTextCtrl16SetSelForegroundEbRK8wxColour(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl16SetSelForegroundEbRK8wxColour");
long ZN16wxStyledTextCtrl16SetSelForegroundEbRK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl16SetSelForegroundEbRK8wxColour"); return 0; }

long ZN16wxStyledTextCtrl16SetSelectionModeEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl16SetSelectionModeEi");
long ZN16wxStyledTextCtrl16SetSelectionModeEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl16SetSelectionModeEi"); return 0; }

long ZN16wxStyledTextCtrl16SetVisiblePolicyEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl16SetVisiblePolicyEii");
long ZN16wxStyledTextCtrl16SetVisiblePolicyEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl16SetVisiblePolicyEii"); return 0; }

long ZN16wxStyledTextCtrl16StyleSetFaceNameEiRK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl16StyleSetFaceNameEiRK8wxString");
long ZN16wxStyledTextCtrl16StyleSetFaceNameEiRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl16StyleSetFaceNameEiRK8wxString"); return 0; }

long ZN16wxStyledTextCtrl16StyleSetFontAttrEiiRK8wxStringbbb14wxFontEncoding(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl16StyleSetFontAttrEiiRK8wxStringbbb14wxFontEncoding");
long ZN16wxStyledTextCtrl16StyleSetFontAttrEiiRK8wxStringbbb14wxFontEncoding(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl16StyleSetFontAttrEiiRK8wxStringbbb14wxFontEncoding"); return 0; }

long ZN16wxStyledTextCtrl16VCHomeRectExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl16VCHomeRectExtendEv");
long ZN16wxStyledTextCtrl16VCHomeRectExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl16VCHomeRectExtendEv"); return 0; }

long ZN16wxStyledTextCtrl16VCHomeWrapExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl16VCHomeWrapExtendEv");
long ZN16wxStyledTextCtrl16VCHomeWrapExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl16VCHomeWrapExtendEv"); return 0; }

long ZN16wxStyledTextCtrl17CallTipPosAtStartEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl17CallTipPosAtStartEv");
long ZN16wxStyledTextCtrl17CallTipPosAtStartEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl17CallTipPosAtStartEv"); return 0; }

long ZN16wxStyledTextCtrl17DeleteBackNotLineEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl17DeleteBackNotLineEv");
long ZN16wxStyledTextCtrl17DeleteBackNotLineEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl17DeleteBackNotLineEv"); return 0; }

long ZN16wxStyledTextCtrl17DocumentEndExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl17DocumentEndExtendEv");
long ZN16wxStyledTextCtrl17DocumentEndExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl17DocumentEndExtendEv"); return 0; }

long ZN16wxStyledTextCtrl17GetHighlightGuideEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl17GetHighlightGuideEv");
long ZN16wxStyledTextCtrl17GetHighlightGuideEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl17GetHighlightGuideEv"); return 0; }

long ZN16wxStyledTextCtrl17GetMouseDwellTimeEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl17GetMouseDwellTimeEv");
long ZN16wxStyledTextCtrl17GetMouseDwellTimeEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl17GetMouseDwellTimeEv"); return 0; }

long ZN16wxStyledTextCtrl17GetSelectionStartEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl17GetSelectionStartEv");
long ZN16wxStyledTextCtrl17GetSelectionStartEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl17GetSelectionStartEv"); return 0; }

long ZN16wxStyledTextCtrl17GetUndoCollectionEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl17GetUndoCollectionEv");
long ZN16wxStyledTextCtrl17GetUndoCollectionEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl17GetUndoCollectionEv"); return 0; }

long ZN16wxStyledTextCtrl17GetViewWhiteSpaceEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl17GetViewWhiteSpaceEv");
long ZN16wxStyledTextCtrl17GetViewWhiteSpaceEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl17GetViewWhiteSpaceEv"); return 0; }

long ZN16wxStyledTextCtrl17HomeDisplayExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl17HomeDisplayExtendEv");
long ZN16wxStyledTextCtrl17HomeDisplayExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl17HomeDisplayExtendEv"); return 0; }

long ZN16wxStyledTextCtrl17IndicatorGetStyleEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl17IndicatorGetStyleEi");
long ZN16wxStyledTextCtrl17IndicatorGetStyleEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl17IndicatorGetStyleEi"); return 0; }

long ZN16wxStyledTextCtrl17IndicatorSetStyleEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl17IndicatorSetStyleEii");
long ZN16wxStyledTextCtrl17IndicatorSetStyleEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl17IndicatorSetStyleEii"); return 0; }

long ZN16wxStyledTextCtrl17LineEndRectExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl17LineEndRectExtendEv");
long ZN16wxStyledTextCtrl17LineEndRectExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl17LineEndRectExtendEv"); return 0; }

long ZN16wxStyledTextCtrl17LineEndWrapExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl17LineEndWrapExtendEv");
long ZN16wxStyledTextCtrl17LineEndWrapExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl17LineEndWrapExtendEv"); return 0; }

long ZN16wxStyledTextCtrl17PointFromPositionEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl17PointFromPositionEi");
long ZN16wxStyledTextCtrl17PointFromPositionEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl17PointFromPositionEi"); return 0; }

long ZN16wxStyledTextCtrl17PositionFromPointE7wxPoint(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl17PositionFromPointE7wxPoint");
long ZN16wxStyledTextCtrl17PositionFromPointE7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl17PositionFromPointE7wxPoint"); return 0; }

long ZN16wxStyledTextCtrl17SetHighlightGuideEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl17SetHighlightGuideEi");
long ZN16wxStyledTextCtrl17SetHighlightGuideEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl17SetHighlightGuideEi"); return 0; }

long ZN16wxStyledTextCtrl17SetMouseDwellTimeEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl17SetMouseDwellTimeEi");
long ZN16wxStyledTextCtrl17SetMouseDwellTimeEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl17SetMouseDwellTimeEi"); return 0; }

long ZN16wxStyledTextCtrl17SetSelectionStartEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl17SetSelectionStartEi");
long ZN16wxStyledTextCtrl17SetSelectionStartEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl17SetSelectionStartEi"); return 0; }

long ZN16wxStyledTextCtrl17SetUndoCollectionEb(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl17SetUndoCollectionEb");
long ZN16wxStyledTextCtrl17SetUndoCollectionEb(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl17SetUndoCollectionEb"); return 0; }

long ZN16wxStyledTextCtrl17SetViewWhiteSpaceEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl17SetViewWhiteSpaceEi");
long ZN16wxStyledTextCtrl17SetViewWhiteSpaceEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl17SetViewWhiteSpaceEi"); return 0; }

long ZN16wxStyledTextCtrl17StutteredPageDownEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl17StutteredPageDownEv");
long ZN16wxStyledTextCtrl17StutteredPageDownEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl17StutteredPageDownEv"); return 0; }

long ZN16wxStyledTextCtrl17StyleResetDefaultEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl17StyleResetDefaultEv");
long ZN16wxStyledTextCtrl17StyleResetDefaultEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl17StyleResetDefaultEv"); return 0; }

long ZN16wxStyledTextCtrl17StyleSetEOLFilledEib(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl17StyleSetEOLFilledEib");
long ZN16wxStyledTextCtrl17StyleSetEOLFilledEib(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl17StyleSetEOLFilledEib"); return 0; }

long ZN16wxStyledTextCtrl17StyleSetUnderlineEib(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl17StyleSetUnderlineEib");
long ZN16wxStyledTextCtrl17StyleSetUnderlineEib(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl17StyleSetUnderlineEib"); return 0; }

long ZN16wxStyledTextCtrl17ToggleCaretStickyEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl17ToggleCaretStickyEv");
long ZN16wxStyledTextCtrl17ToggleCaretStickyEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl17ToggleCaretStickyEv"); return 0; }

long ZN16wxStyledTextCtrl17WordLeftEndExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl17WordLeftEndExtendEv");
long ZN16wxStyledTextCtrl17WordLeftEndExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl17WordLeftEndExtendEv"); return 0; }

long ZN16wxStyledTextCtrl17WordStartPositionEib(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl17WordStartPositionEib");
long ZN16wxStyledTextCtrl17WordStartPositionEib(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl17WordStartPositionEib"); return 0; }

long ZN16wxStyledTextCtrl18AutoCompGetCurrentEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18AutoCompGetCurrentEv");
long ZN16wxStyledTextCtrl18AutoCompGetCurrentEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18AutoCompGetCurrentEv"); return 0; }

long ZN16wxStyledTextCtrl18AutoCompSetFillUpsERK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18AutoCompSetFillUpsERK8wxString");
long ZN16wxStyledTextCtrl18AutoCompSetFillUpsERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18AutoCompSetFillUpsERK8wxString"); return 0; }

long ZN16wxStyledTextCtrl18CharLeftRectExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18CharLeftRectExtendEv");
long ZN16wxStyledTextCtrl18CharLeftRectExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18CharLeftRectExtendEv"); return 0; }

long ZN16wxStyledTextCtrl18ClearDocumentStyleEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18ClearDocumentStyleEv");
long ZN16wxStyledTextCtrl18ClearDocumentStyleEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18ClearDocumentStyleEv"); return 0; }

long ZN16wxStyledTextCtrl18DocLineFromVisibleEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18DocLineFromVisibleEi");
long ZN16wxStyledTextCtrl18DocLineFromVisibleEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18DocLineFromVisibleEi"); return 0; }

long ZN16wxStyledTextCtrl18EditToggleOvertypeEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18EditToggleOvertypeEv");
long ZN16wxStyledTextCtrl18EditToggleOvertypeEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18EditToggleOvertypeEv"); return 0; }

long ZN16wxStyledTextCtrl18EnsureCaretVisibleEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18EnsureCaretVisibleEv");
long ZN16wxStyledTextCtrl18EnsureCaretVisibleEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18EnsureCaretVisibleEv"); return 0; }

long ZN16wxStyledTextCtrl18GetCaretForegroundEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18GetCaretForegroundEv");
long ZN16wxStyledTextCtrl18GetCaretForegroundEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18GetCaretForegroundEv"); return 0; }

long ZN16wxStyledTextCtrl18GetLineEndPositionEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18GetLineEndPositionEi");
long ZN16wxStyledTextCtrl18GetLineEndPositionEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18GetLineEndPositionEi"); return 0; }

long ZN16wxStyledTextCtrl18GetLineIndentationEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18GetLineIndentationEi");
long ZN16wxStyledTextCtrl18GetLineIndentationEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18GetLineIndentationEi"); return 0; }

long ZN16wxStyledTextCtrl18GetMarginSensitiveEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18GetMarginSensitiveEi");
long ZN16wxStyledTextCtrl18GetMarginSensitiveEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18GetMarginSensitiveEi"); return 0; }

long ZN16wxStyledTextCtrl18GetPrintColourModeEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18GetPrintColourModeEv");
long ZN16wxStyledTextCtrl18GetPrintColourModeEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18GetPrintColourModeEv"); return 0; }

long ZN16wxStyledTextCtrl18GetSelectedTextRawEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18GetSelectedTextRawEv");
long ZN16wxStyledTextCtrl18GetSelectedTextRawEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18GetSelectedTextRawEv"); return 0; }

long ZN16wxStyledTextCtrl18GetStyleBitsNeededEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18GetStyleBitsNeededEv");
long ZN16wxStyledTextCtrl18GetStyleBitsNeededEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18GetStyleBitsNeededEv"); return 0; }

long ZN16wxStyledTextCtrl18GetUseAntiAliasingEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18GetUseAntiAliasingEv");
long ZN16wxStyledTextCtrl18GetUseAntiAliasingEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18GetUseAntiAliasingEv"); return 0; }

long ZN16wxStyledTextCtrl18GetWrapStartIndentEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18GetWrapStartIndentEv");
long ZN16wxStyledTextCtrl18GetWrapStartIndentEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18GetWrapStartIndentEv"); return 0; }

long ZN16wxStyledTextCtrl18GetWrapVisualFlagsEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18GetWrapVisualFlagsEv");
long ZN16wxStyledTextCtrl18GetWrapVisualFlagsEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18GetWrapVisualFlagsEv"); return 0; }

long ZN16wxStyledTextCtrl18LineDownRectExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18LineDownRectExtendEv");
long ZN16wxStyledTextCtrl18LineDownRectExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18LineDownRectExtendEv"); return 0; }

long ZN16wxStyledTextCtrl18MarkerDefineBitmapEiRK8wxBitmap(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18MarkerDefineBitmapEiRK8wxBitmap");
long ZN16wxStyledTextCtrl18MarkerDefineBitmapEiRK8wxBitmap(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18MarkerDefineBitmapEiRK8wxBitmap"); return 0; }

long ZN16wxStyledTextCtrl18MarkerDeleteHandleEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18MarkerDeleteHandleEi");
long ZN16wxStyledTextCtrl18MarkerDeleteHandleEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18MarkerDeleteHandleEi"); return 0; }

long ZN16wxStyledTextCtrl18PageDownRectExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18PageDownRectExtendEv");
long ZN16wxStyledTextCtrl18PageDownRectExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18PageDownRectExtendEv"); return 0; }

long ZN16wxStyledTextCtrl18SelectionDuplicateEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18SelectionDuplicateEv");
long ZN16wxStyledTextCtrl18SelectionDuplicateEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18SelectionDuplicateEv"); return 0; }

long ZN16wxStyledTextCtrl18SetCaretForegroundERK8wxColour(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18SetCaretForegroundERK8wxColour");
long ZN16wxStyledTextCtrl18SetCaretForegroundERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18SetCaretForegroundERK8wxColour"); return 0; }

long ZN16wxStyledTextCtrl18SetLineIndentationEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18SetLineIndentationEii");
long ZN16wxStyledTextCtrl18SetLineIndentationEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18SetLineIndentationEii"); return 0; }

long ZN16wxStyledTextCtrl18SetMarginSensitiveEib(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18SetMarginSensitiveEib");
long ZN16wxStyledTextCtrl18SetMarginSensitiveEib(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18SetMarginSensitiveEib"); return 0; }

long ZN16wxStyledTextCtrl18SetPrintColourModeEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18SetPrintColourModeEi");
long ZN16wxStyledTextCtrl18SetPrintColourModeEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18SetPrintColourModeEi"); return 0; }

long ZN16wxStyledTextCtrl18SetUseAntiAliasingEb(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18SetUseAntiAliasingEb");
long ZN16wxStyledTextCtrl18SetUseAntiAliasingEb(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18SetUseAntiAliasingEb"); return 0; }

long ZN16wxStyledTextCtrl18SetWhitespaceCharsERK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18SetWhitespaceCharsERK8wxString");
long ZN16wxStyledTextCtrl18SetWhitespaceCharsERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18SetWhitespaceCharsERK8wxString"); return 0; }

long ZN16wxStyledTextCtrl18SetWrapStartIndentEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18SetWrapStartIndentEi");
long ZN16wxStyledTextCtrl18SetWrapStartIndentEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18SetWrapStartIndentEi"); return 0; }

long ZN16wxStyledTextCtrl18SetWrapVisualFlagsEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18SetWrapVisualFlagsEi");
long ZN16wxStyledTextCtrl18SetWrapVisualFlagsEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18SetWrapVisualFlagsEi"); return 0; }

long ZN16wxStyledTextCtrl18StyleSetBackgroundEiRK8wxColour(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18StyleSetBackgroundEiRK8wxColour");
long ZN16wxStyledTextCtrl18StyleSetBackgroundEiRK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18StyleSetBackgroundEiRK8wxColour"); return 0; }

long ZN16wxStyledTextCtrl18StyleSetChangeableEib(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18StyleSetChangeableEib");
long ZN16wxStyledTextCtrl18StyleSetChangeableEib(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18StyleSetChangeableEib"); return 0; }

long ZN16wxStyledTextCtrl18StyleSetForegroundEiRK8wxColour(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18StyleSetForegroundEiRK8wxColour");
long ZN16wxStyledTextCtrl18StyleSetForegroundEiRK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18StyleSetForegroundEiRK8wxColour"); return 0; }

long ZN16wxStyledTextCtrl18VisibleFromDocLineEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18VisibleFromDocLineEi");
long ZN16wxStyledTextCtrl18VisibleFromDocLineEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18VisibleFromDocLineEi"); return 0; }

long ZN16wxStyledTextCtrl18WordPartLeftExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18WordPartLeftExtendEv");
long ZN16wxStyledTextCtrl18WordPartLeftExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18WordPartLeftExtendEv"); return 0; }

long ZN16wxStyledTextCtrl18WordRightEndExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl18WordRightEndExtendEv");
long ZN16wxStyledTextCtrl18WordRightEndExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl18WordRightEndExtendEv"); return 0; }

long ZN16wxStyledTextCtrl19AutoCompGetAutoHideEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl19AutoCompGetAutoHideEv");
long ZN16wxStyledTextCtrl19AutoCompGetAutoHideEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl19AutoCompGetAutoHideEv"); return 0; }

long ZN16wxStyledTextCtrl19AutoCompGetMaxWidthEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl19AutoCompGetMaxWidthEv");
long ZN16wxStyledTextCtrl19AutoCompGetMaxWidthEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl19AutoCompGetMaxWidthEv"); return 0; }

long ZN16wxStyledTextCtrl19AutoCompSetAutoHideEb(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl19AutoCompSetAutoHideEb");
long ZN16wxStyledTextCtrl19AutoCompSetAutoHideEb(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl19AutoCompSetAutoHideEb"); return 0; }

long ZN16wxStyledTextCtrl19AutoCompSetMaxWidthEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl19AutoCompSetMaxWidthEi");
long ZN16wxStyledTextCtrl19AutoCompSetMaxWidthEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl19AutoCompSetMaxWidthEi"); return 0; }

long ZN16wxStyledTextCtrl19CallTipSetHighlightEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl19CallTipSetHighlightEii");
long ZN16wxStyledTextCtrl19CallTipSetHighlightEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl19CallTipSetHighlightEii"); return 0; }

long ZN16wxStyledTextCtrl19CharRightRectExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl19CharRightRectExtendEv");
long ZN16wxStyledTextCtrl19CharRightRectExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl19CharRightRectExtendEv"); return 0; }

long ZN16wxStyledTextCtrl19DocumentStartExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl19DocumentStartExtendEv");
long ZN16wxStyledTextCtrl19DocumentStartExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl19DocumentStartExtendEv"); return 0; }

long ZN16wxStyledTextCtrl19GetCaretLineVisibleEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl19GetCaretLineVisibleEv");
long ZN16wxStyledTextCtrl19GetCaretLineVisibleEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl19GetCaretLineVisibleEv"); return 0; }

long ZN16wxStyledTextCtrl19GetFirstVisibleLineEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl19GetFirstVisibleLineEv");
long ZN16wxStyledTextCtrl19GetFirstVisibleLineEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl19GetFirstVisibleLineEv"); return 0; }

long ZN16wxStyledTextCtrl19GetPropertyExpandedERK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl19GetPropertyExpandedERK8wxString");
long ZN16wxStyledTextCtrl19GetPropertyExpandedERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl19GetPropertyExpandedERK8wxString"); return 0; }

long ZN16wxStyledTextCtrl19MarkerSetBackgroundEiRK8wxColour(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl19MarkerSetBackgroundEiRK8wxColour");
long ZN16wxStyledTextCtrl19MarkerSetBackgroundEiRK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl19MarkerSetBackgroundEiRK8wxColour"); return 0; }

long ZN16wxStyledTextCtrl19MarkerSetForegroundEiRK8wxColour(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl19MarkerSetForegroundEiRK8wxColour");
long ZN16wxStyledTextCtrl19MarkerSetForegroundEiRK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl19MarkerSetForegroundEiRK8wxColour"); return 0; }

long ZN16wxStyledTextCtrl19MoveCaretInsideViewEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl19MoveCaretInsideViewEv");
long ZN16wxStyledTextCtrl19MoveCaretInsideViewEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl19MoveCaretInsideViewEv"); return 0; }

long ZN16wxStyledTextCtrl19SetCaretLineVisibleEb(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl19SetCaretLineVisibleEb");
long ZN16wxStyledTextCtrl19SetCaretLineVisibleEb(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl19SetCaretLineVisibleEb"); return 0; }

long ZN16wxStyledTextCtrl19SetFoldMarginColourEbRK8wxColour(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl19SetFoldMarginColourEbRK8wxColour");
long ZN16wxStyledTextCtrl19SetFoldMarginColourEbRK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl19SetFoldMarginColourEbRK8wxColour"); return 0; }

long ZN16wxStyledTextCtrl19TargetFromSelectionEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl19TargetFromSelectionEv");
long ZN16wxStyledTextCtrl19TargetFromSelectionEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl19TargetFromSelectionEv"); return 0; }

long ZN16wxStyledTextCtrl19WordPartRightExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl19WordPartRightExtendEv");
long ZN16wxStyledTextCtrl19WordPartRightExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl19WordPartRightExtendEv"); return 0; }

long ZN16wxStyledTextCtrl20AutoCompGetMaxHeightEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl20AutoCompGetMaxHeightEv");
long ZN16wxStyledTextCtrl20AutoCompGetMaxHeightEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl20AutoCompGetMaxHeightEv"); return 0; }

long ZN16wxStyledTextCtrl20AutoCompGetSeparatorEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl20AutoCompGetSeparatorEv");
long ZN16wxStyledTextCtrl20AutoCompGetSeparatorEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl20AutoCompGetSeparatorEv"); return 0; }

long ZN16wxStyledTextCtrl20AutoCompSetMaxHeightEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl20AutoCompSetMaxHeightEi");
long ZN16wxStyledTextCtrl20AutoCompSetMaxHeightEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl20AutoCompSetMaxHeightEi"); return 0; }

long ZN16wxStyledTextCtrl20AutoCompSetSeparatorEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl20AutoCompSetSeparatorEi");
long ZN16wxStyledTextCtrl20AutoCompSetSeparatorEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl20AutoCompSetSeparatorEi"); return 0; }

long ZN16wxStyledTextCtrl20CallTipSetBackgroundERK8wxColour(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl20CallTipSetBackgroundERK8wxColour");
long ZN16wxStyledTextCtrl20CallTipSetBackgroundERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl20CallTipSetBackgroundERK8wxColour"); return 0; }

long ZN16wxStyledTextCtrl20CallTipSetForegroundERK8wxColour(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl20CallTipSetForegroundERK8wxColour");
long ZN16wxStyledTextCtrl20CallTipSetForegroundERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl20CallTipSetForegroundERK8wxColour"); return 0; }

long ZN16wxStyledTextCtrl20GetControlCharSymbolEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl20GetControlCharSymbolEv");
long ZN16wxStyledTextCtrl20GetControlCharSymbolEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl20GetControlCharSymbolEv"); return 0; }

long ZN16wxStyledTextCtrl20GetIndentationGuidesEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl20GetIndentationGuidesEv");
long ZN16wxStyledTextCtrl20GetIndentationGuidesEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl20GetIndentationGuidesEv"); return 0; }

long ZN16wxStyledTextCtrl20GetMouseDownCapturesEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl20GetMouseDownCapturesEv");
long ZN16wxStyledTextCtrl20GetMouseDownCapturesEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl20GetMouseDownCapturesEv"); return 0; }

long ZN16wxStyledTextCtrl20LineEndDisplayExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl20LineEndDisplayExtendEv");
long ZN16wxStyledTextCtrl20LineEndDisplayExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl20LineEndDisplayExtendEv"); return 0; }

long ZN16wxStyledTextCtrl20MarkerLineFromHandleEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl20MarkerLineFromHandleEi");
long ZN16wxStyledTextCtrl20MarkerLineFromHandleEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl20MarkerLineFromHandleEi"); return 0; }

long ZN16wxStyledTextCtrl20SelectionIsRectangleEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl20SelectionIsRectangleEv");
long ZN16wxStyledTextCtrl20SelectionIsRectangleEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl20SelectionIsRectangleEv"); return 0; }

long ZN16wxStyledTextCtrl20SetControlCharSymbolEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl20SetControlCharSymbolEi");
long ZN16wxStyledTextCtrl20SetControlCharSymbolEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl20SetControlCharSymbolEi"); return 0; }

long ZN16wxStyledTextCtrl20SetHotspotSingleLineEb(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl20SetHotspotSingleLineEb");
long ZN16wxStyledTextCtrl20SetHotspotSingleLineEb(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl20SetHotspotSingleLineEb"); return 0; }

long ZN16wxStyledTextCtrl20SetIndentationGuidesEb(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl20SetIndentationGuidesEb");
long ZN16wxStyledTextCtrl20SetIndentationGuidesEb(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl20SetIndentationGuidesEb"); return 0; }

long ZN16wxStyledTextCtrl20SetMouseDownCapturesEb(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl20SetMouseDownCapturesEb");
long ZN16wxStyledTextCtrl20SetMouseDownCapturesEb(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl20SetMouseDownCapturesEb"); return 0; }

long ZN16wxStyledTextCtrl20StyleSetCharacterSetEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl20StyleSetCharacterSetEii");
long ZN16wxStyledTextCtrl20StyleSetCharacterSetEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl20StyleSetCharacterSetEii"); return 0; }

long ZN16wxStyledTextCtrl20StyleSetFontEncodingEi14wxFontEncoding(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl20StyleSetFontEncodingEi14wxFontEncoding");
long ZN16wxStyledTextCtrl20StyleSetFontEncodingEi14wxFontEncoding(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl20StyleSetFontEncodingEi14wxFontEncoding"); return 0; }

long ZN16wxStyledTextCtrl21AutoCompGetIgnoreCaseEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl21AutoCompGetIgnoreCaseEv");
long ZN16wxStyledTextCtrl21AutoCompGetIgnoreCaseEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl21AutoCompGetIgnoreCaseEv"); return 0; }

long ZN16wxStyledTextCtrl21AutoCompSetIgnoreCaseEb(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl21AutoCompSetIgnoreCaseEb");
long ZN16wxStyledTextCtrl21AutoCompSetIgnoreCaseEb(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl21AutoCompSetIgnoreCaseEb"); return 0; }

long ZN16wxStyledTextCtrl21ClearRegisteredImagesEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl21ClearRegisteredImagesEv");
long ZN16wxStyledTextCtrl21ClearRegisteredImagesEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl21ClearRegisteredImagesEv"); return 0; }

long ZN16wxStyledTextCtrl21GetBackSpaceUnIndentsEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl21GetBackSpaceUnIndentsEv");
long ZN16wxStyledTextCtrl21GetBackSpaceUnIndentsEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl21GetBackSpaceUnIndentsEv"); return 0; }

long ZN16wxStyledTextCtrl21GetCaretLineBackAlphaEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl21GetCaretLineBackAlphaEv");
long ZN16wxStyledTextCtrl21GetCaretLineBackAlphaEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl21GetCaretLineBackAlphaEv"); return 0; }

long ZN16wxStyledTextCtrl21GetLineIndentPositionEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl21GetLineIndentPositionEi");
long ZN16wxStyledTextCtrl21GetLineIndentPositionEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl21GetLineIndentPositionEi"); return 0; }

long ZN16wxStyledTextCtrl21GetLineSelEndPositionEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl21GetLineSelEndPositionEi");
long ZN16wxStyledTextCtrl21GetLineSelEndPositionEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl21GetLineSelEndPositionEi"); return 0; }

long ZN16wxStyledTextCtrl21GetPrintMagnificationEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl21GetPrintMagnificationEv");
long ZN16wxStyledTextCtrl21GetPrintMagnificationEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl21GetPrintMagnificationEv"); return 0; }

long ZN16wxStyledTextCtrl21SetBackSpaceUnIndentsEb(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl21SetBackSpaceUnIndentsEb");
long ZN16wxStyledTextCtrl21SetBackSpaceUnIndentsEb(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl21SetBackSpaceUnIndentsEb"); return 0; }

long ZN16wxStyledTextCtrl21SetCaretLineBackAlphaEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl21SetCaretLineBackAlphaEi");
long ZN16wxStyledTextCtrl21SetCaretLineBackAlphaEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl21SetCaretLineBackAlphaEi"); return 0; }

long ZN16wxStyledTextCtrl21SetFoldMarginHiColourEbRK8wxColour(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl21SetFoldMarginHiColourEbRK8wxColour");
long ZN16wxStyledTextCtrl21SetFoldMarginHiColourEbRK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl21SetFoldMarginHiColourEbRK8wxColour"); return 0; }

long ZN16wxStyledTextCtrl21SetPrintMagnificationEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl21SetPrintMagnificationEi");
long ZN16wxStyledTextCtrl21SetPrintMagnificationEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl21SetPrintMagnificationEi"); return 0; }

long ZN16wxStyledTextCtrl21StutteredPageUpExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl21StutteredPageUpExtendEv");
long ZN16wxStyledTextCtrl21StutteredPageUpExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl21StutteredPageUpExtendEv"); return 0; }

long ZN16wxStyledTextCtrl22GetCaretLineBackgroundEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl22GetCaretLineBackgroundEv");
long ZN16wxStyledTextCtrl22GetCaretLineBackgroundEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl22GetCaretLineBackgroundEv"); return 0; }

long ZN16wxStyledTextCtrl22GetPasteConvertEndingsEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl22GetPasteConvertEndingsEv");
long ZN16wxStyledTextCtrl22GetPasteConvertEndingsEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl22GetPasteConvertEndingsEv"); return 0; }

long ZN16wxStyledTextCtrl22IndicatorGetForegroundEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl22IndicatorGetForegroundEi");
long ZN16wxStyledTextCtrl22IndicatorGetForegroundEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl22IndicatorGetForegroundEi"); return 0; }

long ZN16wxStyledTextCtrl22IndicatorSetForegroundEiRK8wxColour(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl22IndicatorSetForegroundEiRK8wxColour");
long ZN16wxStyledTextCtrl22IndicatorSetForegroundEiRK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl22IndicatorSetForegroundEiRK8wxColour"); return 0; }

long ZN16wxStyledTextCtrl22PositionFromPointCloseEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl22PositionFromPointCloseEii");
long ZN16wxStyledTextCtrl22PositionFromPointCloseEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl22PositionFromPointCloseEii"); return 0; }

long ZN16wxStyledTextCtrl22SetCaretLineBackgroundERK8wxColour(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl22SetCaretLineBackgroundERK8wxColour");
long ZN16wxStyledTextCtrl22SetCaretLineBackgroundERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl22SetCaretLineBackgroundERK8wxColour"); return 0; }

long ZN16wxStyledTextCtrl22SetPasteConvertEndingsEb(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl22SetPasteConvertEndingsEb");
long ZN16wxStyledTextCtrl22SetPasteConvertEndingsEb(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl22SetPasteConvertEndingsEb"); return 0; }

long ZN16wxStyledTextCtrl23AutoCompGetChooseSingleEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl23AutoCompGetChooseSingleEv");
long ZN16wxStyledTextCtrl23AutoCompGetChooseSingleEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl23AutoCompGetChooseSingleEv"); return 0; }

long ZN16wxStyledTextCtrl23AutoCompSetChooseSingleEb(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl23AutoCompSetChooseSingleEb");
long ZN16wxStyledTextCtrl23AutoCompSetChooseSingleEb(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl23AutoCompSetChooseSingleEb"); return 0; }

long ZN16wxStyledTextCtrl23GetLineSelStartPositionEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl23GetLineSelStartPositionEi");
long ZN16wxStyledTextCtrl23GetLineSelStartPositionEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl23GetLineSelStartPositionEi"); return 0; }

long ZN16wxStyledTextCtrl23GetUseVerticalScrollBarEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl23GetUseVerticalScrollBarEv");
long ZN16wxStyledTextCtrl23GetUseVerticalScrollBarEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl23GetUseVerticalScrollBarEv"); return 0; }

long ZN16wxStyledTextCtrl23SetUseVerticalScrollBarEb(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl23SetUseVerticalScrollBarEb");
long ZN16wxStyledTextCtrl23SetUseVerticalScrollBarEb(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl23SetUseVerticalScrollBarEb"); return 0; }

long ZN16wxStyledTextCtrl23SetWhitespaceBackgroundEbRK8wxColour(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl23SetWhitespaceBackgroundEbRK8wxColour");
long ZN16wxStyledTextCtrl23SetWhitespaceBackgroundEbRK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl23SetWhitespaceBackgroundEbRK8wxColour"); return 0; }

long ZN16wxStyledTextCtrl23SetWhitespaceForegroundEbRK8wxColour(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl23SetWhitespaceForegroundEbRK8wxColour");
long ZN16wxStyledTextCtrl23SetWhitespaceForegroundEbRK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl23SetWhitespaceForegroundEbRK8wxColour"); return 0; }

long ZN16wxStyledTextCtrl23StutteredPageDownExtendEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl23StutteredPageDownExtendEv");
long ZN16wxStyledTextCtrl23StutteredPageDownExtendEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl23StutteredPageDownExtendEv"); return 0; }

long ZN16wxStyledTextCtrl24AutoCompGetCancelAtStartEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl24AutoCompGetCancelAtStartEv");
long ZN16wxStyledTextCtrl24AutoCompGetCancelAtStartEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl24AutoCompGetCancelAtStartEv"); return 0; }

long ZN16wxStyledTextCtrl24AutoCompGetTypeSeparatorEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl24AutoCompGetTypeSeparatorEv");
long ZN16wxStyledTextCtrl24AutoCompGetTypeSeparatorEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl24AutoCompGetTypeSeparatorEv"); return 0; }

long ZN16wxStyledTextCtrl24AutoCompSetCancelAtStartEb(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl24AutoCompSetCancelAtStartEb");
long ZN16wxStyledTextCtrl24AutoCompSetCancelAtStartEb(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl24AutoCompSetCancelAtStartEb"); return 0; }

long ZN16wxStyledTextCtrl24AutoCompSetTypeSeparatorEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl24AutoCompSetTypeSeparatorEi");
long ZN16wxStyledTextCtrl24AutoCompSetTypeSeparatorEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl24AutoCompSetTypeSeparatorEi"); return 0; }

long ZN16wxStyledTextCtrl25AutoCompGetDropRestOfWordEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl25AutoCompGetDropRestOfWordEv");
long ZN16wxStyledTextCtrl25AutoCompGetDropRestOfWordEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl25AutoCompGetDropRestOfWordEv"); return 0; }

long ZN16wxStyledTextCtrl25AutoCompSetDropRestOfWordEb(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl25AutoCompSetDropRestOfWordEb");
long ZN16wxStyledTextCtrl25AutoCompSetDropRestOfWordEb(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl25AutoCompSetDropRestOfWordEb"); return 0; }

long ZN16wxStyledTextCtrl25GetUseHorizontalScrollBarEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl25GetUseHorizontalScrollBarEv");
long ZN16wxStyledTextCtrl25GetUseHorizontalScrollBarEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl25GetUseHorizontalScrollBarEv"); return 0; }

long ZN16wxStyledTextCtrl25SetHotspotActiveUnderlineEb(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl25SetHotspotActiveUnderlineEb");
long ZN16wxStyledTextCtrl25SetHotspotActiveUnderlineEb(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl25SetHotspotActiveUnderlineEb"); return 0; }

long ZN16wxStyledTextCtrl25SetUseHorizontalScrollBarEb(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl25SetUseHorizontalScrollBarEb");
long ZN16wxStyledTextCtrl25SetUseHorizontalScrollBarEb(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl25SetUseHorizontalScrollBarEb"); return 0; }

long ZN16wxStyledTextCtrl26EnsureVisibleEnforcePolicyEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl26EnsureVisibleEnforcePolicyEi");
long ZN16wxStyledTextCtrl26EnsureVisibleEnforcePolicyEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl26EnsureVisibleEnforcePolicyEi"); return 0; }

long ZN16wxStyledTextCtrl26GetWrapVisualFlagsLocationEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl26GetWrapVisualFlagsLocationEv");
long ZN16wxStyledTextCtrl26GetWrapVisualFlagsLocationEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl26GetWrapVisualFlagsLocationEv"); return 0; }

long ZN16wxStyledTextCtrl26SetHotspotActiveBackgroundEbRK8wxColour(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl26SetHotspotActiveBackgroundEbRK8wxColour");
long ZN16wxStyledTextCtrl26SetHotspotActiveBackgroundEbRK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl26SetHotspotActiveBackgroundEbRK8wxColour"); return 0; }

long ZN16wxStyledTextCtrl26SetHotspotActiveForegroundEbRK8wxColour(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl26SetHotspotActiveForegroundEbRK8wxColour");
long ZN16wxStyledTextCtrl26SetHotspotActiveForegroundEbRK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl26SetHotspotActiveForegroundEbRK8wxColour"); return 0; }

long ZN16wxStyledTextCtrl26SetWrapVisualFlagsLocationEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl26SetWrapVisualFlagsLocationEi");
long ZN16wxStyledTextCtrl26SetWrapVisualFlagsLocationEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl26SetWrapVisualFlagsLocationEi"); return 0; }

long ZN16wxStyledTextCtrl29CallTipSetForegroundHighlightERK8wxColour(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl29CallTipSetForegroundHighlightERK8wxColour");
long ZN16wxStyledTextCtrl29CallTipSetForegroundHighlightERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl29CallTipSetForegroundHighlightERK8wxColour"); return 0; }

long ZN16wxStyledTextCtrl3CutEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl3CutEv");
long ZN16wxStyledTextCtrl3CutEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl3CutEv"); return 0; }

long ZN16wxStyledTextCtrl3TabEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl3TabEv");
long ZN16wxStyledTextCtrl3TabEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl3TabEv"); return 0; }

long ZN16wxStyledTextCtrl4CopyEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl4CopyEv");
long ZN16wxStyledTextCtrl4CopyEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl4CopyEv"); return 0; }

long ZN16wxStyledTextCtrl4HomeEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl4HomeEv");
long ZN16wxStyledTextCtrl4HomeEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl4HomeEv"); return 0; }

long ZN16wxStyledTextCtrl4RedoEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl4RedoEv");
long ZN16wxStyledTextCtrl4RedoEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl4RedoEv"); return 0; }

long ZN16wxStyledTextCtrl4UndoEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl4UndoEv");
long ZN16wxStyledTextCtrl4UndoEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl4UndoEv"); return 0; }

long ZN16wxStyledTextCtrl5ClearEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl5ClearEv");
long ZN16wxStyledTextCtrl5ClearEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl5ClearEv"); return 0; }

long ZN16wxStyledTextCtrl5PasteEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl5PasteEv");
long ZN16wxStyledTextCtrl5PasteEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl5PasteEv"); return 0; }

long ZN16wxStyledTextCtrl6CancelEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl6CancelEv");
long ZN16wxStyledTextCtrl6CancelEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl6CancelEv"); return 0; }

long ZN16wxStyledTextCtrl6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString");
long ZN16wxStyledTextCtrl6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN16wxStyledTextCtrl6LineUpEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl6LineUpEv");
long ZN16wxStyledTextCtrl6LineUpEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl6LineUpEv"); return 0; }

long ZN16wxStyledTextCtrl6PageUpEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl6PageUpEv");
long ZN16wxStyledTextCtrl6PageUpEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl6PageUpEv"); return 0; }

long ZN16wxStyledTextCtrl6ParaUpEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl6ParaUpEv");
long ZN16wxStyledTextCtrl6ParaUpEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl6ParaUpEv"); return 0; }

long ZN16wxStyledTextCtrl6VCHomeEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl6VCHomeEv");
long ZN16wxStyledTextCtrl6VCHomeEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl6VCHomeEv"); return 0; }

long ZN16wxStyledTextCtrl6ZoomInEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl6ZoomInEv");
long ZN16wxStyledTextCtrl6ZoomInEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl6ZoomInEv"); return 0; }

long ZN16wxStyledTextCtrl7AddTextERK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl7AddTextERK8wxString");
long ZN16wxStyledTextCtrl7AddTextERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl7AddTextERK8wxString"); return 0; }

long ZN16wxStyledTextCtrl7BackTabEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl7BackTabEv");
long ZN16wxStyledTextCtrl7BackTabEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl7BackTabEv"); return 0; }

long ZN16wxStyledTextCtrl7CanRedoEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl7CanRedoEv");
long ZN16wxStyledTextCtrl7CanRedoEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl7CanRedoEv"); return 0; }

long ZN16wxStyledTextCtrl7CanUndoEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl7CanUndoEv");
long ZN16wxStyledTextCtrl7CanUndoEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl7CanUndoEv"); return 0; }

long ZN16wxStyledTextCtrl7GetLineEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl7GetLineEi");
long ZN16wxStyledTextCtrl7GetLineEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl7GetLineEi"); return 0; }

long ZN16wxStyledTextCtrl7GetTextEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl7GetTextEv");
long ZN16wxStyledTextCtrl7GetTextEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl7GetTextEv"); return 0; }

long ZN16wxStyledTextCtrl7GetZoomEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl7GetZoomEv");
long ZN16wxStyledTextCtrl7GetZoomEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl7GetZoomEv"); return 0; }

long ZN16wxStyledTextCtrl7GotoPosEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl7GotoPosEi");
long ZN16wxStyledTextCtrl7GotoPosEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl7GotoPosEi"); return 0; }

long ZN16wxStyledTextCtrl7LineCutEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl7LineCutEv");
long ZN16wxStyledTextCtrl7LineCutEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl7LineCutEv"); return 0; }

long ZN16wxStyledTextCtrl7LineEndEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl7LineEndEv");
long ZN16wxStyledTextCtrl7LineEndEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl7LineEndEv"); return 0; }

long ZN16wxStyledTextCtrl7NewLineEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl7NewLineEv");
long ZN16wxStyledTextCtrl7NewLineEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl7NewLineEv"); return 0; }

long ZN16wxStyledTextCtrl7SendMsgEill(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl7SendMsgEill");
long ZN16wxStyledTextCtrl7SendMsgEill(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl7SendMsgEill"); return 0; }

long ZN16wxStyledTextCtrl7SetTextERK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl7SetTextERK8wxString");
long ZN16wxStyledTextCtrl7SetTextERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl7SetTextERK8wxString"); return 0; }

long ZN16wxStyledTextCtrl7SetZoomEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl7SetZoomEi");
long ZN16wxStyledTextCtrl7SetZoomEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl7SetZoomEi"); return 0; }

long ZN16wxStyledTextCtrl7ZoomOutEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl7ZoomOutEv");
long ZN16wxStyledTextCtrl7ZoomOutEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl7ZoomOutEv"); return 0; }

long ZN16wxStyledTextCtrl8AllocateEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl8AllocateEi");
long ZN16wxStyledTextCtrl8AllocateEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl8AllocateEi"); return 0; }

long ZN16wxStyledTextCtrl8CanPasteEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl8CanPasteEv");
long ZN16wxStyledTextCtrl8CanPasteEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl8CanPasteEv"); return 0; }

long ZN16wxStyledTextCtrl8CharLeftEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl8CharLeftEv");
long ZN16wxStyledTextCtrl8CharLeftEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl8CharLeftEv"); return 0; }

long ZN16wxStyledTextCtrl8ClearAllEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl8ClearAllEv");
long ZN16wxStyledTextCtrl8ClearAllEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl8ClearAllEv"); return 0; }

long ZN16wxStyledTextCtrl8CopyTextEiRK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl8CopyTextEiRK8wxString");
long ZN16wxStyledTextCtrl8CopyTextEiRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl8CopyTextEiRK8wxString"); return 0; }

long ZN16wxStyledTextCtrl8FindTextEiiRK8wxStringi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl8FindTextEiiRK8wxStringi");
long ZN16wxStyledTextCtrl8FindTextEiiRK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl8FindTextEiiRK8wxStringi"); return 0; }

long ZN16wxStyledTextCtrl8FormFeedEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl8FormFeedEv");
long ZN16wxStyledTextCtrl8FormFeedEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl8FormFeedEv"); return 0; }

long ZN16wxStyledTextCtrl8GetLexerEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl8GetLexerEv");
long ZN16wxStyledTextCtrl8GetLexerEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl8GetLexerEv"); return 0; }

long ZN16wxStyledTextCtrl8GotoLineEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl8GotoLineEi");
long ZN16wxStyledTextCtrl8GotoLineEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl8GotoLineEi"); return 0; }

long ZN16wxStyledTextCtrl8HomeWrapEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl8HomeWrapEv");
long ZN16wxStyledTextCtrl8HomeWrapEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl8HomeWrapEv"); return 0; }

long ZN16wxStyledTextCtrl8LineCopyEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl8LineCopyEv");
long ZN16wxStyledTextCtrl8LineCopyEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl8LineCopyEv"); return 0; }

long ZN16wxStyledTextCtrl8LineDownEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl8LineDownEv");
long ZN16wxStyledTextCtrl8LineDownEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl8LineDownEv"); return 0; }

long ZN16wxStyledTextCtrl8LoadFileERK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl8LoadFileERK8wxString");
long ZN16wxStyledTextCtrl8LoadFileERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl8LoadFileERK8wxString"); return 0; }

long ZN16wxStyledTextCtrl8PageDownEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl8PageDownEv");
long ZN16wxStyledTextCtrl8PageDownEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl8PageDownEv"); return 0; }

long ZN16wxStyledTextCtrl8ParaDownEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl8ParaDownEv");
long ZN16wxStyledTextCtrl8ParaDownEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl8ParaDownEv"); return 0; }

long ZN16wxStyledTextCtrl8SaveFileERK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl8SaveFileERK8wxString");
long ZN16wxStyledTextCtrl8SaveFileERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl8SaveFileERK8wxString"); return 0; }

long ZN16wxStyledTextCtrl8SetLexerEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl8SetLexerEi");
long ZN16wxStyledTextCtrl8SetLexerEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl8SetLexerEi"); return 0; }

long ZN16wxStyledTextCtrl8UsePopUpEb(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl8UsePopUpEb");
long ZN16wxStyledTextCtrl8UsePopUpEb(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl8UsePopUpEb"); return 0; }

long ZN16wxStyledTextCtrl8WordLeftEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl8WordLeftEv");
long ZN16wxStyledTextCtrl8WordLeftEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl8WordLeftEv"); return 0; }

long ZN16wxStyledTextCtrl9CharRightEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl9CharRightEv");
long ZN16wxStyledTextCtrl9CharRightEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl9CharRightEv"); return 0; }

long ZN16wxStyledTextCtrl9ColouriseEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl9ColouriseEii");
long ZN16wxStyledTextCtrl9ColouriseEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl9ColouriseEii"); return 0; }

long ZN16wxStyledTextCtrl9CopyRangeEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl9CopyRangeEii");
long ZN16wxStyledTextCtrl9CopyRangeEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl9CopyRangeEii"); return 0; }

long ZN16wxStyledTextCtrl9GetAnchorEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl9GetAnchorEv");
long ZN16wxStyledTextCtrl9GetAnchorEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl9GetAnchorEv"); return 0; }

long ZN16wxStyledTextCtrl9GetCharAtEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl9GetCharAtEi");
long ZN16wxStyledTextCtrl9GetCharAtEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl9GetCharAtEi"); return 0; }

long ZN16wxStyledTextCtrl9GetColumnEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl9GetColumnEi");
long ZN16wxStyledTextCtrl9GetColumnEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl9GetColumnEi"); return 0; }

long ZN16wxStyledTextCtrl9GetIndentEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl9GetIndentEv");
long ZN16wxStyledTextCtrl9GetIndentEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl9GetIndentEv"); return 0; }

long ZN16wxStyledTextCtrl9GetLengthEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl9GetLengthEv");
long ZN16wxStyledTextCtrl9GetLengthEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl9GetLengthEv"); return 0; }

long ZN16wxStyledTextCtrl9GetModifyEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl9GetModifyEv");
long ZN16wxStyledTextCtrl9GetModifyEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl9GetModifyEv"); return 0; }

long ZN16wxStyledTextCtrl9GetStatusEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl9GetStatusEv");
long ZN16wxStyledTextCtrl9GetStatusEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl9GetStatusEv"); return 0; }

long ZN16wxStyledTextCtrl9HideLinesEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl9HideLinesEii");
long ZN16wxStyledTextCtrl9HideLinesEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl9HideLinesEii"); return 0; }

long ZN16wxStyledTextCtrl9LinesJoinEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl9LinesJoinEv");
long ZN16wxStyledTextCtrl9LinesJoinEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl9LinesJoinEv"); return 0; }

long ZN16wxStyledTextCtrl9LowerCaseEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl9LowerCaseEv");
long ZN16wxStyledTextCtrl9LowerCaseEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl9LowerCaseEv"); return 0; }

long ZN16wxStyledTextCtrl9MarkerAddEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl9MarkerAddEii");
long ZN16wxStyledTextCtrl9MarkerAddEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl9MarkerAddEii"); return 0; }

long ZN16wxStyledTextCtrl9MarkerGetEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl9MarkerGetEi");
long ZN16wxStyledTextCtrl9MarkerGetEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl9MarkerGetEi"); return 0; }

long ZN16wxStyledTextCtrl9SelectAllEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl9SelectAllEv");
long ZN16wxStyledTextCtrl9SelectAllEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl9SelectAllEv"); return 0; }

long ZN16wxStyledTextCtrl9SetAnchorEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl9SetAnchorEi");
long ZN16wxStyledTextCtrl9SetAnchorEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl9SetAnchorEi"); return 0; }

long ZN16wxStyledTextCtrl9SetIndentEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl9SetIndentEi");
long ZN16wxStyledTextCtrl9SetIndentEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl9SetIndentEi"); return 0; }

long ZN16wxStyledTextCtrl9SetStatusEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl9SetStatusEi");
long ZN16wxStyledTextCtrl9SetStatusEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl9SetStatusEi"); return 0; }

long ZN16wxStyledTextCtrl9ShowLinesEii(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl9ShowLinesEii");
long ZN16wxStyledTextCtrl9ShowLinesEii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl9ShowLinesEii"); return 0; }

long ZN16wxStyledTextCtrl9TextWidthEiRK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl9TextWidthEiRK8wxString");
long ZN16wxStyledTextCtrl9TextWidthEiRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl9TextWidthEiRK8wxString"); return 0; }

long ZN16wxStyledTextCtrl9UpperCaseEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl9UpperCaseEv");
long ZN16wxStyledTextCtrl9UpperCaseEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl9UpperCaseEv"); return 0; }

long ZN16wxStyledTextCtrl9WordRightEv(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl9WordRightEv");
long ZN16wxStyledTextCtrl9WordRightEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl9WordRightEv"); return 0; }

long ZN16wxStyledTextCtrl9WrapCountEi(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrl9WrapCountEi");
long ZN16wxStyledTextCtrl9WrapCountEi(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrl9WrapCountEi"); return 0; }

long ZN16wxStyledTextCtrlC1EP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) __asm("__ZN16wxStyledTextCtrlC1EP8wxWindowiRK7wxPointRK6wxSizelRK8wxString");
long ZN16wxStyledTextCtrlC1EP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN16wxStyledTextCtrlC1EP8wxWindowiRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN17wxStyledTextEventC1Eii(long a, long b, long c_, long d, long e, long f) __asm("__ZN17wxStyledTextEventC1Eii");
long ZN17wxStyledTextEventC1Eii(long a, long b, long c_, long d, long e, long f) { shim_note("__ZN17wxStyledTextEventC1Eii"); return 0; }

long ZNK17wxStyledTextEvent10GetControlEv(long a, long b, long c_, long d, long e, long f) __asm("__ZNK17wxStyledTextEvent10GetControlEv");
long ZNK17wxStyledTextEvent10GetControlEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZNK17wxStyledTextEvent10GetControlEv"); return 0; }

long ZNK17wxStyledTextEvent6GetAltEv(long a, long b, long c_, long d, long e, long f) __asm("__ZNK17wxStyledTextEvent6GetAltEv");
long ZNK17wxStyledTextEvent6GetAltEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZNK17wxStyledTextEvent6GetAltEv"); return 0; }

long ZNK17wxStyledTextEvent8GetShiftEv(long a, long b, long c_, long d, long e, long f) __asm("__ZNK17wxStyledTextEvent8GetShiftEv");
long ZNK17wxStyledTextEvent8GetShiftEv(long a, long b, long c_, long d, long e, long f) { shim_note("__ZNK17wxStyledTextEvent8GetShiftEv"); return 0; }
