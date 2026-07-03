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
  fprintf(stderr, "[shimauto:%s] %s: removed-OS symbol CALLED with no curated impl -- returning 0 (write one in shimdb/impl/ if its behavior matters)\n", "libwx_macud-2.8.0", sym);
  if (getenv("SHIMGEN_STUB_ABORT")) abort();
  pthread_mutex_unlock(&mtx);
}

long Z10wxAboutBoxRK17wxAboutDialogInfo(long a, long b, long c_, long d, long e, long f) { shim_note("Z10wxAboutBoxRK17wxAboutDialogInfo"); return 0; }

long Z10wxLogDebugPKwz(long a, long b, long c_, long d, long e, long f) { shim_note("Z10wxLogDebugPKwz"); return 0; }

long Z10wxLogErrorPKwz(long a, long b, long c_, long d, long e, long f) { shim_note("Z10wxLogErrorPKwz"); return 0; }

long Z10wxLogTracePKwS0_z(long a, long b, long c_, long d, long e, long f) { shim_note("Z10wxLogTracePKwS0_z"); return 0; }

long Z10wxLogTracemPKwz(long a, long b, long c_, long d, long e, long f) { shim_note("Z10wxLogTracemPKwz"); return 0; }

long Z10wxOnAssertPKwiPKcS0_S0_(long a, long b, long c_, long d, long e, long f) { shim_note("Z10wxOnAssertPKwiPKcS0_S0_"); return 0; }

long Z10wxShutdown15wxShutdownFlags(long a, long b, long c_, long d, long e, long f) { shim_note("Z10wxShutdown15wxShutdownFlags"); return 0; }

long Z11wxGetLocalev(long a, long b, long c_, long d, long e, long f) { shim_note("Z11wxGetLocalev"); return 0; }

long Z11wxGetUserIdv(long a, long b, long c_, long d, long e, long f) { shim_note("Z11wxGetUserIdv"); return 0; }

long Z11wxIsStockIDi(long a, long b, long c_, long d, long e, long f) { shim_note("Z11wxIsStockIDi"); return 0; }

long Z11wxLogStatusP7wxFramePKwz(long a, long b, long c_, long d, long e, long f) { shim_note("Z11wxLogStatusP7wxFramePKwz"); return 0; }

long Z11wxLogStatusPKwz(long a, long b, long c_, long d, long e, long f) { shim_note("Z11wxLogStatusPKwz"); return 0; }

long Z11wxSafeYieldP8wxWindowb(long a, long b, long c_, long d, long e, long f) { shim_note("Z11wxSafeYieldP8wxWindowb"); return 0; }

long Z11wxSetCursorRK8wxCursor(long a, long b, long c_, long d, long e, long f) { shim_note("Z11wxSetCursorRK8wxCursor"); return 0; }

long Z12wxEntryStartRiPPc(long a, long b, long c_, long d, long e, long f) { shim_note("Z12wxEntryStartRiPPc"); return 0; }

long Z12wxGetHomeDirv(long a, long b, long c_, long d, long e, long f) { shim_note("Z12wxGetHomeDirv"); return 0; }

long Z12wxGetUTCTimev(long a, long b, long c_, long d, long e, long f) { shim_note("Z12wxGetUTCTimev"); return 0; }

long Z12wxLogGenericmPKwz(long a, long b, long c_, long d, long e, long f) { shim_note("Z12wxLogGenericmPKwz"); return 0; }

long Z12wxLogMessagePKwz(long a, long b, long c_, long d, long e, long f) { shim_note("Z12wxLogMessagePKwz"); return 0; }

long Z12wxLogVerbosePKwz(long a, long b, long c_, long d, long e, long f) { shim_note("Z12wxLogVerbosePKwz"); return 0; }

long Z12wxLogWarningPKwz(long a, long b, long c_, long d, long e, long f) { shim_note("Z12wxLogWarningPKwz"); return 0; }

long Z12wxMessageBoxRK8wxStringS1_lP8wxWindowii(long a, long b, long c_, long d, long e, long f) { shim_note("Z12wxMessageBoxRK8wxStringS1_lP8wxWindowii"); return 0; }

long Z12wxMicroSleepm(long a, long b, long c_, long d, long e, long f) { shim_note("Z12wxMicroSleepm"); return 0; }

long Z12wxMilliSleepm(long a, long b, long c_, long d, long e, long f) { shim_note("Z12wxMilliSleepm"); return 0; }

long Z12wxRegisterIdl(long a, long b, long c_, long d, long e, long f) { shim_note("Z12wxRegisterIdl"); return 0; }

long Z12wxStartTimerv(long a, long b, long c_, long d, long e, long f) { shim_note("Z12wxStartTimerv"); return 0; }

long Z12wxWakeUpIdlev(long a, long b, long c_, long d, long e, long f) { shim_note("Z12wxWakeUpIdlev"); return 0; }

long Z13wxDirSelectorRK8wxStringS1_lRK7wxPointP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("Z13wxDirSelectorRK8wxStringS1_lRK7wxPointP8wxWindow"); return 0; }

long Z13wxDisplaySizePiS_(long a, long b, long c_, long d, long e, long f) { shim_note("Z13wxDisplaySizePiS_"); return 0; }

long Z13wxGetHostNamev(long a, long b, long c_, long d, long e, long f) { shim_note("Z13wxGetHostNamev"); return 0; }

long Z13wxGetKeyState9wxKeyCode(long a, long b, long c_, long d, long e, long f) { shim_note("Z13wxGetKeyState9wxKeyCode"); return 0; }

long Z13wxGetUserHomeRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("Z13wxGetUserHomeRK8wxString"); return 0; }

long Z13wxGetUserNamev(long a, long b, long c_, long d, long e, long f) { shim_note("Z13wxGetUserNamev"); return 0; }

long Z13wxLogSysErrorPKwz(long a, long b, long c_, long d, long e, long f) { shim_note("Z13wxLogSysErrorPKwz"); return 0; }

long Z13wxSysErrorMsgm(long a, long b, long c_, long d, long e, long f) { shim_note("Z13wxSysErrorMsgm"); return 0; }

long Z14wxDisplayDepthv(long a, long b, long c_, long d, long e, long f) { shim_note("Z14wxDisplayDepthv"); return 0; }

long Z14wxEntryCleanupv(long a, long b, long c_, long d, long e, long f) { shim_note("Z14wxEntryCleanupv"); return 0; }

long Z14wxFileSelectorPKwS0_S0_S0_S0_iP8wxWindowii(long a, long b, long c_, long d, long e, long f) { shim_note("Z14wxFileSelectorPKwS0_S0_S0_S0_iP8wxWindowii"); return 0; }

long Z14wxGetCurrentIdv(long a, long b, long c_, long d, long e, long f) { shim_note("Z14wxGetCurrentIdv"); return 0; }

long Z14wxGetLocalTimev(long a, long b, long c_, long d, long e, long f) { shim_note("Z14wxGetLocalTimev"); return 0; }

long Z14wxGetOsVersionPiS_(long a, long b, long c_, long d, long e, long f) { shim_note("Z14wxGetOsVersionPiS_"); return 0; }

long Z14wxGetProcessIdv(long a, long b, long c_, long d, long e, long f) { shim_note("Z14wxGetProcessIdv"); return 0; }

long Z14wxIsStockLabeliRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("Z14wxIsStockLabeliRK8wxString"); return 0; }

long Z14wxNewEventTypev(long a, long b, long c_, long d, long e, long f) { shim_note("Z14wxNewEventTypev"); return 0; }

long Z14wxSysErrorCodev(long a, long b, long c_, long d, long e, long f) { shim_note("Z14wxSysErrorCodev"); return 0; }

long Z15wxColourDisplayv(long a, long b, long c_, long d, long e, long f) { shim_note("Z15wxColourDisplayv"); return 0; }

long Z15wxDisplaySizeMMPiS_(long a, long b, long c_, long d, long e, long f) { shim_note("Z15wxDisplaySizeMMPiS_"); return 0; }

long Z15wxEndBusyCursorv(long a, long b, long c_, long d, long e, long f) { shim_note("Z15wxEndBusyCursorv"); return 0; }

long Z15wxExpandEnvVarsRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("Z15wxExpandEnvVarsRK8wxString"); return 0; }

long Z15wxGetFreeMemoryv(long a, long b, long c_, long d, long e, long f) { shim_note("Z15wxGetFreeMemoryv"); return 0; }

long Z15wxGetMouseStatev(long a, long b, long c_, long d, long e, long f) { shim_note("Z15wxGetMouseStatev"); return 0; }

long Z15wxGetStockLabelil(long a, long b, long c_, long d, long e, long f) { shim_note("Z15wxGetStockLabelil"); return 0; }

long Z15wxLogFatalErrorPKwz(long a, long b, long c_, long d, long e, long f) { shim_note("Z15wxLogFatalErrorPKwz"); return 0; }

long Z15wxMutexGuiEnterv(long a, long b, long c_, long d, long e, long f) { shim_note("Z15wxMutexGuiEnterv"); return 0; }

long Z15wxMutexGuiLeavev(long a, long b, long c_, long d, long e, long f) { shim_note("Z15wxMutexGuiLeavev"); return 0; }

long Z15wxYieldIfNeededv(long a, long b, long c_, long d, long e, long f) { shim_note("Z15wxYieldIfNeededv"); return 0; }

long Z16wxGetDisplaySizev(long a, long b, long c_, long d, long e, long f) { shim_note("Z16wxGetDisplaySizev"); return 0; }

long Z16wxGetElapsedTimeb(long a, long b, long c_, long d, long e, long f) { shim_note("Z16wxGetElapsedTimeb"); return 0; }

long Z16wxIsDragResultOk12wxDragResult(long a, long b, long c_, long d, long e, long f) { shim_note("Z16wxIsDragResultOk12wxDragResult"); return 0; }

long Z16wxStripMenuCodesRK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("Z16wxStripMenuCodesRK8wxStringi"); return 0; }

long Z17wxBeginBusyCursorPK8wxCursor(long a, long b, long c_, long d, long e, long f) { shim_note("Z17wxBeginBusyCursorPK8wxCursor"); return 0; }

long Z17wxGetActiveWindowv(long a, long b, long c_, long d, long e, long f) { shim_note("Z17wxGetActiveWindowv"); return 0; }

long Z17wxGetEmailAddressv(long a, long b, long c_, long d, long e, long f) { shim_note("Z17wxGetEmailAddressv"); return 0; }

long Z17wxGetFontFromUserP8wxWindowRK6wxFontRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("Z17wxGetFontFromUserP8wxWindowRK6wxFontRK8wxString"); return 0; }

long Z17wxGetFullHostNamev(long a, long b, long c_, long d, long e, long f) { shim_note("Z17wxGetFullHostNamev"); return 0; }

long Z17wxGetSingleChoiceRK8wxStringS1_iPS0_P8wxWindowiibii(long a, long b, long c_, long d, long e, long f) { shim_note("Z17wxGetSingleChoiceRK8wxStringS1_iPS0_P8wxWindowiibii"); return 0; }

long Z17wxGetTextFromUserRK8wxStringS1_S1_P8wxWindowiib(long a, long b, long c_, long d, long e, long f) { shim_note("Z17wxGetTextFromUserRK8wxStringS1_S1_P8wxWindowiib"); return 0; }

long Z17wxIsPlatform64Bitv(long a, long b, long c_, long d, long e, long f) { shim_note("Z17wxIsPlatform64Bitv"); return 0; }

long Z17wxSafeShowMessageRK8wxStringS1_(long a, long b, long c_, long d, long e, long f) { shim_note("Z17wxSafeShowMessageRK8wxStringS1_"); return 0; }

long Z18wxGetDisplaySizeMMv(long a, long b, long c_, long d, long e, long f) { shim_note("Z18wxGetDisplaySizeMMv"); return 0; }

long Z18wxGetMousePositionv(long a, long b, long c_, long d, long e, long f) { shim_note("Z18wxGetMousePositionv"); return 0; }

long Z18wxGetOsDescriptionv(long a, long b, long c_, long d, long e, long f) { shim_note("Z18wxGetOsDescriptionv"); return 0; }

long Z18wxLoadFileSelectorPKwS0_S0_P8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("Z18wxLoadFileSelectorPKwS0_S0_P8wxWindow"); return 0; }

long Z18wxSaveFileSelectorPKwS0_S0_P8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("Z18wxSaveFileSelectorPKwS0_S0_P8wxWindow"); return 0; }

long Z18wxTestFontEncodingRK20wxNativeEncodingInfo(long a, long b, long c_, long d, long e, long f) { shim_note("Z18wxTestFontEncodingRK20wxNativeEncodingInfo"); return 0; }

long Z18wxWakeUpMainThreadv(long a, long b, long c_, long d, long e, long f) { shim_note("Z18wxWakeUpMainThreadv"); return 0; }

long Z19wxClientDisplayRectPiS_S_S_(long a, long b, long c_, long d, long e, long f) { shim_note("Z19wxClientDisplayRectPiS_S_S_"); return 0; }

long Z19wxFindWindowAtPointRK7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("Z19wxFindWindowAtPointRK7wxPoint"); return 0; }

long Z19wxGetColourFromUserP8wxWindowRK8wxColourRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("Z19wxGetColourFromUserP8wxWindowRK8wxColourRK8wxString"); return 0; }

long Z19wxGetNumberFromUserRK8wxStringS1_S1_lllP8wxWindowRK7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("Z19wxGetNumberFromUserRK8wxStringS1_S1_lllP8wxWindowRK7wxPoint"); return 0; }

long Z19wxGetTopLevelParentP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("Z19wxGetTopLevelParentP8wxWindow"); return 0; }

long Z20wxGetAccelFromStringRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("Z20wxGetAccelFromStringRK8wxString"); return 0; }

long Z20wxGetLocalTimeMillisv(long a, long b, long c_, long d, long e, long f) { shim_note("Z20wxGetLocalTimeMillisv"); return 0; }

long Z20wxGetStockHelpStringi23wxStockHelpStringClient(long a, long b, long c_, long d, long e, long f) { shim_note("Z20wxGetStockHelpStringi23wxStockHelpStringClient"); return 0; }

long Z20wxRichTextModuleInitv(long a, long b, long c_, long d, long e, long f) { shim_note("Z20wxRichTextModuleInitv"); return 0; }

long Z21wxFindWindowAtPointerR7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("Z21wxFindWindowAtPointerR7wxPoint"); return 0; }

long Z21wxGetPasswordFromUserRK8wxStringS1_S1_P8wxWindowiib(long a, long b, long c_, long d, long e, long f) { shim_note("Z21wxGetPasswordFromUserRK8wxStringS1_S1_P8wxWindowiib"); return 0; }

long Z21wxRenderer_DrawChoiceP8wxWindowR4wxDCRK6wxRecti(long a, long b, long c_, long d, long e, long f) { shim_note("Z21wxRenderer_DrawChoiceP8wxWindowR4wxDCRK6wxRecti"); return 0; }

long Z22wxGetClientDisplayRectv(long a, long b, long c_, long d, long e, long f) { shim_note("Z22wxGetClientDisplayRectv"); return 0; }

long Z22wxGetSingleChoiceIndexRK8wxStringS1_iPS0_P8wxWindowiibii(long a, long b, long c_, long d, long e, long f) { shim_note("Z22wxGetSingleChoiceIndexRK8wxStringS1_iPS0_P8wxWindowiibii"); return 0; }

long Z22wxInitAllImageHandlersv(long a, long b, long c_, long d, long e, long f) { shim_note("Z22wxInitAllImageHandlersv"); return 0; }

long Z22wxLaunchDefaultBrowserRK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("Z22wxLaunchDefaultBrowserRK8wxStringi"); return 0; }

long Z23wxCreateFileTipProviderRK8wxStringm(long a, long b, long c_, long d, long e, long f) { shim_note("Z23wxCreateFileTipProviderRK8wxStringm"); return 0; }

long Z23wxEnableTopLevelWindowsb(long a, long b, long c_, long d, long e, long f) { shim_note("Z23wxEnableTopLevelWindowsb"); return 0; }

long Z23wxGetNativeFontEncoding14wxFontEncodingP20wxNativeEncodingInfo(long a, long b, long c_, long d, long e, long f) { shim_note("Z23wxGetNativeFontEncoding14wxFontEncodingP20wxNativeEncodingInfo"); return 0; }

long Z23wxRenderer_DrawComboBoxP8wxWindowR4wxDCRK6wxRecti(long a, long b, long c_, long d, long e, long f) { shim_note("Z23wxRenderer_DrawComboBoxP8wxWindowR4wxDCRK6wxRecti"); return 0; }

long Z23wxRenderer_DrawTextCtrlP8wxWindowR4wxDCRK6wxRecti(long a, long b, long c_, long d, long e, long f) { shim_note("Z23wxRenderer_DrawTextCtrlP8wxWindowR4wxDCRK6wxRecti"); return 0; }

long Z23wxXmlInitResourceModulev(long a, long b, long c_, long d, long e, long f) { shim_note("Z23wxXmlInitResourceModulev"); return 0; }

long Z24wxIsPlatformLittleEndianv(long a, long b, long c_, long d, long e, long f) { shim_note("Z24wxIsPlatformLittleEndianv"); return 0; }

long Z26wxGenericFindWindowAtPointRK7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("Z26wxGenericFindWindowAtPointRK7wxPoint"); return 0; }

long Z26wxRenderer_DrawRadioButtonP8wxWindowR4wxDCRK6wxRecti(long a, long b, long c_, long d, long e, long f) { shim_note("Z26wxRenderer_DrawRadioButtonP8wxWindowR4wxDCRK6wxRecti"); return 0; }

long Z5wxNowv(long a, long b, long c_, long d, long e, long f) { shim_note("Z5wxNowv"); return 0; }

long Z6wxBellv(long a, long b, long c_, long d, long e, long f) { shim_note("Z6wxBellv"); return 0; }

long Z6wxExitv(long a, long b, long c_, long d, long e, long f) { shim_note("Z6wxExitv"); return 0; }

long Z6wxKilll8wxSignalP11wxKillErrori(long a, long b, long c_, long d, long e, long f) { shim_note("Z6wxKilll8wxSignalP11wxKillErrori"); return 0; }

long Z6wxTrapv(long a, long b, long c_, long d, long e, long f) { shim_note("Z6wxTrapv"); return 0; }

long Z7wxNewIdv(long a, long b, long c_, long d, long e, long f) { shim_note("Z7wxNewIdv"); return 0; }

long Z7wxShellRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("Z7wxShellRK8wxString"); return 0; }

long Z7wxSleepi(long a, long b, long c_, long d, long e, long f) { shim_note("Z7wxSleepi"); return 0; }

long Z7wxYieldv(long a, long b, long c_, long d, long e, long f) { shim_note("Z7wxYieldv"); return 0; }

long Z8wxIsBusyv(long a, long b, long c_, long d, long e, long f) { shim_note("Z8wxIsBusyv"); return 0; }

long Z9wxExecuteRK8wxStringiP9wxProcess(long a, long b, long c_, long d, long e, long f) { shim_note("Z9wxExecuteRK8wxStringiP9wxProcess"); return 0; }

long Z9wxLogInfoPKwz(long a, long b, long c_, long d, long e, long f) { shim_note("Z9wxLogInfoPKwz"); return 0; }

long Z9wxShowTipP8wxWindowP13wxTipProviderb(long a, long b, long c_, long d, long e, long f) { shim_note("Z9wxShowTipP8wxWindowP13wxTipProviderb"); return 0; }

long ZN10wxBoxSizerC1Ei(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxBoxSizerC1Ei"); return 0; }

long ZN10wxBusyInfoC1ERK8wxStringP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxBusyInfoC1ERK8wxStringP8wxWindow"); return 0; }

long ZN10wxCheckBox6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxCheckBox6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_"); return 0; }

long ZN10wxClientDCC1EP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxClientDCC1EP8wxWindow"); return 0; }

long ZN10wxClientDCD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxClientDCD1Ev"); return 0; }

long ZN10wxComboBox4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxComboBox4InitEv"); return 0; }

long ZN10wxComboBox6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizeRK13wxArrayStringlRK11wxValidatorS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxComboBox6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizeRK13wxArrayStringlRK11wxValidatorS4_"); return 0; }

long ZN10wxDateTime10GetCenturyEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime10GetCenturyEi"); return 0; }

long ZN10wxDateTime10GetCountryEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime10GetCountryEv"); return 0; }

long ZN10wxDateTime10IsLeapYearEiNS_8CalendarE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime10IsLeapYearEiNS_8CalendarE"); return 0; }

long ZN10wxDateTime10SetCountryENS_7CountryE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime10SetCountryENS_7CountryE"); return 0; }

long ZN10wxDateTime11GetBeginDSTEiNS_7CountryE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime11GetBeginDSTEiNS_7CountryE"); return 0; }

long ZN10wxDateTime11ParseFormatEPKwS1_RKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime11ParseFormatEPKwS1_RKS_"); return 0; }

long ZN10wxDateTime12GetMonthNameENS_5MonthENS_9NameFlagsE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime12GetMonthNameENS_5MonthENS_9NameFlagsE"); return 0; }

long ZN10wxDateTime12MakeTimezoneERKNS_8TimeZoneEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime12MakeTimezoneERKNS_8TimeZoneEb"); return 0; }

long ZN10wxDateTime12SetToTheWeekEtNS_7WeekDayENS_9WeekFlagsE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime12SetToTheWeekEtNS_7WeekDayENS_9WeekFlagsE"); return 0; }

long ZN10wxDateTime12SetToWeekDayENS_7WeekDayEiNS_5MonthEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime12SetToWeekDayENS_7WeekDayEiNS_5MonthEi"); return 0; }

long ZN10wxDateTime12SetToYearDayEt(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime12SetToYearDayEt"); return 0; }

long ZN10wxDateTime13ParseDateTimeEPKw(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime13ParseDateTimeEPKw"); return 0; }

long ZN10wxDateTime13TIME_T_FACTORE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime13TIME_T_FACTORE"); return 0; }

long ZN10wxDateTime14GetAmPmStringsEP8wxStringS1_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime14GetAmPmStringsEP8wxStringS1_"); return 0; }

long ZN10wxDateTime14GetCurrentYearENS_8CalendarE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime14GetCurrentYearENS_8CalendarE"); return 0; }

long ZN10wxDateTime14GetWeekDayNameENS_7WeekDayENS_9NameFlagsE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime14GetWeekDayNameENS_7WeekDayENS_9NameFlagsE"); return 0; }

long ZN10wxDateTime14SetMillisecondEt(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime14SetMillisecondEt"); return 0; }

long ZN10wxDateTime15ConvertYearToBCEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime15ConvertYearToBCEi"); return 0; }

long ZN10wxDateTime15GetCurrentMonthENS_8CalendarE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime15GetCurrentMonthENS_8CalendarE"); return 0; }

long ZN10wxDateTime15GetNumberOfDaysENS_5MonthEiNS_8CalendarE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime15GetNumberOfDaysENS_5MonthEiNS_8CalendarE"); return 0; }

long ZN10wxDateTime15GetNumberOfDaysEiNS_8CalendarE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime15GetNumberOfDaysEiNS_8CalendarE"); return 0; }

long ZN10wxDateTime15IsDSTApplicableEiNS_7CountryE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime15IsDSTApplicableEiNS_7CountryE"); return 0; }

long ZN10wxDateTime15ParseRfc822DateEPKw(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime15ParseRfc822DateEPKw"); return 0; }

long ZN10wxDateTime15SetToWeekOfYearEitNS_7WeekDayE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime15SetToWeekOfYearEitNS_7WeekDayE"); return 0; }

long ZN10wxDateTime16MakeFromTimezoneERKNS_8TimeZoneEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime16MakeFromTimezoneERKNS_8TimeZoneEb"); return 0; }

long ZN10wxDateTime16SetToNextWeekDayENS_7WeekDayE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime16SetToNextWeekDayENS_7WeekDayE"); return 0; }

long ZN10wxDateTime16SetToPrevWeekDayENS_7WeekDayE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime16SetToPrevWeekDayENS_7WeekDayE"); return 0; }

long ZN10wxDateTime17SetToLastMonthDayENS_5MonthEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime17SetToLastMonthDayENS_5MonthEi"); return 0; }

long ZN10wxDateTime21IsWestEuropeanCountryENS_7CountryE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime21IsWestEuropeanCountryENS_7CountryE"); return 0; }

long ZN10wxDateTime22SetToWeekDayInSameWeekENS_7WeekDayENS_9WeekFlagsE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime22SetToWeekDayInSameWeekENS_7WeekDayENS_9WeekFlagsE"); return 0; }

long ZN10wxDateTime2Tm14ComputeWeekDayEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime2Tm14ComputeWeekDayEv"); return 0; }

long ZN10wxDateTime3AddERK10wxDateSpan(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime3AddERK10wxDateSpan"); return 0; }

long ZN10wxDateTime3SetERK2tm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime3SetERK2tm"); return 0; }

long ZN10wxDateTime3SetEd(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime3SetEd"); return 0; }

long ZN10wxDateTime3SetEtNS_5MonthEitttt(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime3SetEtNS_5MonthEitttt"); return 0; }

long ZN10wxDateTime3SetEtttt(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime3SetEtttt"); return 0; }

long ZN10wxDateTime4UNowEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime4UNowEv"); return 0; }

long ZN10wxDateTime6SetDayEt(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime6SetDayEt"); return 0; }

long ZN10wxDateTime7SetHourEt(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime7SetHourEt"); return 0; }

long ZN10wxDateTime7SetYearEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime7SetYearEi"); return 0; }

long ZN10wxDateTime8GetTmNowEP2tm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime8GetTmNowEP2tm"); return 0; }

long ZN10wxDateTime8SetMonthENS_5MonthE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime8SetMonthENS_5MonthE"); return 0; }

long ZN10wxDateTime8TimeZoneC1ENS_2TZE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime8TimeZoneC1ENS_2TZE"); return 0; }

long ZN10wxDateTime9GetEndDSTEiNS_7CountryE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime9GetEndDSTEiNS_7CountryE"); return 0; }

long ZN10wxDateTime9ParseDateEPKw(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime9ParseDateEPKw"); return 0; }

long ZN10wxDateTime9ParseTimeEPKw(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime9ParseTimeEPKw"); return 0; }

long ZN10wxDateTime9ResetTimeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime9ResetTimeEv"); return 0; }

long ZN10wxDateTime9SetMinuteEt(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime9SetMinuteEt"); return 0; }

long ZN10wxDateTime9SetSecondEt(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxDateTime9SetSecondEt"); return 0; }

long ZN10wxFileName6AssignERK8wxString12wxPathFormat(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxFileName6AssignERK8wxString12wxPathFormat"); return 0; }

long ZN10wxFileType10SetCommandERK8wxStringS2_b(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxFileType10SetCommandERK8wxStringS2_b"); return 0; }

long ZN10wxFileType11UnassociateEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxFileType11UnassociateEv"); return 0; }

long ZN10wxFileType13ExpandCommandERK8wxStringRKNS_17MessageParametersE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxFileType13ExpandCommandERK8wxStringRKNS_17MessageParametersE"); return 0; }

long ZN10wxFileType13GetExtensionsER13wxArrayString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxFileType13GetExtensionsER13wxArrayString"); return 0; }

long ZN10wxFileType14SetDefaultIconERK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxFileType14SetDefaultIconERK8wxStringi"); return 0; }

long ZN10wxFileTypeC1ERK14wxFileTypeInfo(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxFileTypeC1ERK14wxFileTypeInfo"); return 0; }

long ZN10wxFileTypeD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxFileTypeD1Ev"); return 0; }

long ZN10wxFontBase17SetNativeFontInfoERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxFontBase17SetNativeFontInfoERK8wxString"); return 0; }

long ZN10wxFontBase18SetDefaultEncodingE14wxFontEncoding(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxFontBase18SetDefaultEncodingE14wxFontEncoding"); return 0; }

long ZN10wxFontBase18ms_encodingDefaultE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxFontBase18ms_encodingDefaultE"); return 0; }

long ZN10wxFontBase25SetNativeFontInfoUserDescERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxFontBase25SetNativeFontInfoUserDescERK8wxString"); return 0; }

long ZN10wxFontBase3NewERK6wxSize12wxFontFamilyiRK8wxString14wxFontEncoding(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxFontBase3NewERK6wxSize12wxFontFamilyiRK8wxString14wxFontEncoding"); return 0; }

long ZN10wxFontBase3NewERK6wxSizeiiibRK8wxString14wxFontEncoding(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxFontBase3NewERK6wxSizeiiibRK8wxString14wxFontEncoding"); return 0; }

long ZN10wxFontBase3NewEi12wxFontFamilyiRK8wxString14wxFontEncoding(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxFontBase3NewEi12wxFontFamilyiRK8wxString14wxFontEncoding"); return 0; }

long ZN10wxFontBaseD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxFontBaseD2Ev"); return 0; }

long ZN10wxFontDataC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxFontDataC1Ev"); return 0; }

long ZN10wxFontList10RemoveFontEP6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxFontList10RemoveFontEP6wxFont"); return 0; }

long ZN10wxFontList16FindOrCreateFontEiiiibRK8wxString14wxFontEncoding(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxFontList16FindOrCreateFontEiiiibRK8wxString14wxFontEncoding"); return 0; }

long ZN10wxFontList7AddFontEP6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxFontList7AddFontEP6wxFont"); return 0; }

long ZN10wxHtmlCell7SetLinkERK14wxHtmlLinkInfo(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxHtmlCell7SetLinkERK14wxHtmlLinkInfo"); return 0; }

long ZN10wxHtmlCellC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxHtmlCellC1Ev"); return 0; }

long ZN10wxHtmlCellC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxHtmlCellC2Ev"); return 0; }

long ZN10wxHtmlCellD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxHtmlCellD2Ev"); return 0; }

long ZN10wxJoystick10SetCaptureEP8wxWindowi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxJoystick10SetCaptureEP8wxWindowi"); return 0; }

long ZN10wxJoystick14ReleaseCaptureEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxJoystick14ReleaseCaptureEv"); return 0; }

long ZN10wxJoystick18GetNumberJoysticksEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxJoystick18GetNumberJoysticksEv"); return 0; }

long ZN10wxJoystick20SetMovementThresholdEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxJoystick20SetMovementThresholdEi"); return 0; }

long ZN10wxJoystickC1Ei(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxJoystickC1Ei"); return 0; }

long ZN10wxKeyEventC1Ei(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxKeyEventC1Ei"); return 0; }

long ZN10wxListBase12DeleteObjectEPv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListBase12DeleteObjectEPv"); return 0; }

long ZN10wxListBase4InitE9wxKeyType(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListBase4InitE9wxKeyType"); return 0; }

long ZN10wxListBase5ClearEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListBase5ClearEv"); return 0; }

long ZN10wxListBase6AppendEPv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListBase6AppendEPv"); return 0; }

long ZN10wxListBaseD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListBaseD2Ev"); return 0; }

long ZN10wxListCtrl10DeleteItemEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl10DeleteItemEl"); return 0; }

long ZN10wxListCtrl10InsertItemER10wxListItem(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl10InsertItemER10wxListItem"); return 0; }

long ZN10wxListCtrl10InsertItemElRK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl10InsertItemElRK8wxStringi"); return 0; }

long ZN10wxListCtrl10InsertItemEli(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl10InsertItemEli"); return 0; }

long ZN10wxListCtrl10ScrollListEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl10ScrollListEii"); return 0; }

long ZN10wxListCtrl11RefreshItemEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl11RefreshItemEl"); return 0; }

long ZN10wxListCtrl11SetItemDataEll(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl11SetItemDataEll"); return 0; }

long ZN10wxListCtrl11SetItemFontElRK6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl11SetItemFontElRK6wxFont"); return 0; }

long ZN10wxListCtrl11SetItemTextElRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl11SetItemTextElRK8wxString"); return 0; }

long ZN10wxListCtrl12DeleteColumnEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl12DeleteColumnEi"); return 0; }

long ZN10wxListCtrl12InsertColumnElR10wxListItem(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl12InsertColumnElR10wxListItem"); return 0; }

long ZN10wxListCtrl12InsertColumnElRK8wxStringii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl12InsertColumnElRK8wxStringii"); return 0; }

long ZN10wxListCtrl12RefreshItemsEll(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl12RefreshItemsEll"); return 0; }

long ZN10wxListCtrl12SetImageListEP11wxImageListi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl12SetImageListEP11wxImageListi"); return 0; }

long ZN10wxListCtrl12SetItemCountEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl12SetItemCountEl"); return 0; }

long ZN10wxListCtrl12SetItemImageElii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl12SetItemImageElii"); return 0; }

long ZN10wxListCtrl12SetItemStateElll(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl12SetItemStateElll"); return 0; }

long ZN10wxListCtrl12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl12ms_classInfoE"); return 0; }

long ZN10wxListCtrl13EnsureVisibleEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl13EnsureVisibleEl"); return 0; }

long ZN10wxListCtrl13SetDropTargetEP12wxDropTarget(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl13SetDropTargetEP12wxDropTarget"); return 0; }

long ZN10wxListCtrl13SetTextColourERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl13SetTextColourERK8wxColour"); return 0; }

long ZN10wxListCtrl14DeleteAllItemsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl14DeleteAllItemsEv"); return 0; }

long ZN10wxListCtrl14SetColumnWidthEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl14SetColumnWidthEii"); return 0; }

long ZN10wxListCtrl14SetItemSpacingEib(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl14SetItemSpacingEib"); return 0; }

long ZN10wxListCtrl14SetSingleStyleElb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl14SetSingleStyleElb"); return 0; }

long ZN10wxListCtrl15AssignImageListEP11wxImageListi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl15AssignImageListEP11wxImageListi"); return 0; }

long ZN10wxListCtrl15SetItemPositionElRK7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl15SetItemPositionElRK7wxPoint"); return 0; }

long ZN10wxListCtrl16DeleteAllColumnsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl16DeleteAllColumnsEv"); return 0; }

long ZN10wxListCtrl17SetItemTextColourElRK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl17SetItemTextColourElRK8wxColour"); return 0; }

long ZN10wxListCtrl18SetItemColumnImageElli(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl18SetItemColumnImageElli"); return 0; }

long ZN10wxListCtrl18SetWindowStyleFlagEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl18SetWindowStyleFlagEl"); return 0; }

long ZN10wxListCtrl19GetBackgroundColourEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl19GetBackgroundColourEv"); return 0; }

long ZN10wxListCtrl19SetBackgroundColourERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl19SetBackgroundColourERK8wxColour"); return 0; }

long ZN10wxListCtrl19SetForegroundColourERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl19SetForegroundColourERK8wxColour"); return 0; }

long ZN10wxListCtrl23SetItemBackgroundColourElRK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl23SetItemBackgroundColourElRK8wxColour"); return 0; }

long ZN10wxListCtrl25GetClassDefaultAttributesE15wxWindowVariant(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl25GetClassDefaultAttributesE15wxWindowVariant"); return 0; }

long ZN10wxListCtrl4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl4InitEv"); return 0; }

long ZN10wxListCtrl6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK11wxValidatorRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK11wxValidatorRK8wxString"); return 0; }

long ZN10wxListCtrl7ArrangeEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl7ArrangeEi"); return 0; }

long ZN10wxListCtrl7SetFontERK6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl7SetFontERK6wxFont"); return 0; }

long ZN10wxListCtrl7SetItemER10wxListItem(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl7SetItemER10wxListItem"); return 0; }

long ZN10wxListCtrl7SetItemEliRK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl7SetItemEliRK8wxStringi"); return 0; }

long ZN10wxListCtrl8ClearAllEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl8ClearAllEv"); return 0; }

long ZN10wxListCtrl8FindItemElRK7wxPointi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl8FindItemElRK7wxPointi"); return 0; }

long ZN10wxListCtrl8FindItemElRK8wxStringb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl8FindItemElRK8wxStringb"); return 0; }

long ZN10wxListCtrl8FindItemEll(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl8FindItemEll"); return 0; }

long ZN10wxListCtrl8SetFocusEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl8SetFocusEv"); return 0; }

long ZN10wxListCtrl9DoSetSizeEiiiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl9DoSetSizeEiiiii"); return 0; }

long ZN10wxListCtrl9EditLabelElP11wxClassInfo(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl9EditLabelElP11wxClassInfo"); return 0; }

long ZN10wxListCtrl9SetColumnEiR10wxListItem(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl9SetColumnEiR10wxListItem"); return 0; }

long ZN10wxListCtrl9SortItemsEPFilllEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrl9SortItemsEPFilllEl"); return 0; }

long ZN10wxListCtrlD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListCtrlD2Ev"); return 0; }

long ZN10wxListbook4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListbook4InitEv"); return 0; }

long ZN10wxListbook6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxListbook6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN10wxLogChain6SetLogEP5wxLog(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxLogChain6SetLogEP5wxLog"); return 0; }

long ZN10wxLogChainC1EP5wxLog(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxLogChainC1EP5wxLog"); return 0; }

long ZN10wxMemoryDC4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxMemoryDC4InitEv"); return 0; }

long ZN10wxMemoryDCC1EP4wxDC(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxMemoryDCC1EP4wxDC"); return 0; }

long ZN10wxMemoryDCD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxMemoryDCD1Ev"); return 0; }

long ZN10wxMemoryDCD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxMemoryDCD2Ev"); return 0; }

long ZN10wxMenuBase4InitEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxMenuBase4InitEl"); return 0; }

long ZN10wxMenuBase5CheckEib(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxMenuBase5CheckEib"); return 0; }

long ZN10wxMenuBase6DeleteEP10wxMenuItem(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxMenuBase6DeleteEP10wxMenuItem"); return 0; }

long ZN10wxMenuBase6EnableEib(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxMenuBase6EnableEib"); return 0; }

long ZN10wxMenuBase6InsertEmP10wxMenuItem(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxMenuBase6InsertEmP10wxMenuItem"); return 0; }

long ZN10wxMenuBase6RemoveEP10wxMenuItem(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxMenuBase6RemoveEP10wxMenuItem"); return 0; }

long ZN10wxMenuBase7DestroyEP10wxMenuItem(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxMenuBase7DestroyEP10wxMenuItem"); return 0; }

long ZN10wxMenuBase8SetLabelEiRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxMenuBase8SetLabelEiRK8wxString"); return 0; }

long ZN10wxMenuBase8UpdateUIEP12wxEvtHandler(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxMenuBase8UpdateUIEP12wxEvtHandler"); return 0; }

long ZN10wxMenuBaseD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxMenuBaseD2Ev"); return 0; }

long ZN10wxMenuItemC1EP6wxMenuiRK8wxStringS4_10wxItemKindS1_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxMenuItemC1EP6wxMenuiRK8wxStringS4_10wxItemKindS1_"); return 0; }

long ZN10wxMetafileC1ERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxMetafileC1ERK8wxString"); return 0; }

long ZN10wxMetafileD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxMetafileD1Ev"); return 0; }

long ZN10wxNodeBaseC2EP10wxListBasePS_S2_PvRK9wxListKey(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxNodeBaseC2EP10wxListBasePS_S2_PvRK9wxListKey"); return 0; }

long ZN10wxNodeBaseD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxNodeBaseD2Ev"); return 0; }

long ZN10wxNotebook6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxNotebook6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN10wxNotebookC1EP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxNotebookC1EP8wxWindowiRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN10wxNotebookC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxNotebookC1Ev"); return 0; }

long ZN10wxPrintout11GetPageInfoEPiS0_S0_S0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxPrintout11GetPageInfoEPiS0_S0_S0_"); return 0; }

long ZN10wxPrintout12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxPrintout12ms_classInfoE"); return 0; }

long ZN10wxPrintout13OnEndDocumentEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxPrintout13OnEndDocumentEv"); return 0; }

long ZN10wxPrintout13OnEndPrintingEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxPrintout13OnEndPrintingEv"); return 0; }

long ZN10wxPrintout15OnBeginDocumentEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxPrintout15OnBeginDocumentEii"); return 0; }

long ZN10wxPrintout15OnBeginPrintingEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxPrintout15OnBeginPrintingEv"); return 0; }

long ZN10wxPrintout16SetLogicalOriginEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxPrintout16SetLogicalOriginEii"); return 0; }

long ZN10wxPrintout17FitThisSizeToPageERK6wxSize(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxPrintout17FitThisSizeToPageERK6wxSize"); return 0; }

long ZN10wxPrintout18FitThisSizeToPaperERK6wxSize(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxPrintout18FitThisSizeToPaperERK6wxSize"); return 0; }

long ZN10wxPrintout19MapScreenSizeToPageEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxPrintout19MapScreenSizeToPageEv"); return 0; }

long ZN10wxPrintout19OffsetLogicalOriginEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxPrintout19OffsetLogicalOriginEii"); return 0; }

long ZN10wxPrintout20MapScreenSizeToPaperEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxPrintout20MapScreenSizeToPaperEv"); return 0; }

long ZN10wxPrintout21MapScreenSizeToDeviceEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxPrintout21MapScreenSizeToDeviceEv"); return 0; }

long ZN10wxPrintout24FitThisSizeToPageMarginsERK6wxSizeRK21wxPageSetupDialogData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxPrintout24FitThisSizeToPageMarginsERK6wxSizeRK21wxPageSetupDialogData"); return 0; }

long ZN10wxPrintout26MapScreenSizeToPageMarginsERK21wxPageSetupDialogData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxPrintout26MapScreenSizeToPageMarginsERK21wxPageSetupDialogData"); return 0; }

long ZN10wxPrintout7HasPageEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxPrintout7HasPageEi"); return 0; }

long ZN10wxPrintoutC2ERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxPrintoutC2ERK8wxString"); return 0; }

long ZN10wxPrintoutD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxPrintoutD2Ev"); return 0; }

long ZN10wxQuantize8QuantizeERK7wxImageRS0_iPPhi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxQuantize8QuantizeERK7wxImageRS0_iPPhi"); return 0; }

long ZN10wxRadioBox6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizeRK13wxArrayStringilRK11wxValidatorS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxRadioBox6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizeRK13wxArrayStringilRK11wxValidatorS4_"); return 0; }

long ZN10wxRadioBoxC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxRadioBoxC1Ev"); return 0; }

long ZN10wxScreenDCC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxScreenDCC1Ev"); return 0; }

long ZN10wxScreenDCD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxScreenDCD1Ev"); return 0; }

long ZN10wxSpinCtrl12SetSelectionEll(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxSpinCtrl12SetSelectionEll"); return 0; }

long ZN10wxSpinCtrl4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxSpinCtrl4InitEv"); return 0; }

long ZN10wxSpinCtrl6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizeliiiS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxSpinCtrl6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizeliiiS4_"); return 0; }

long ZN10wxSpinCtrl8SetRangeEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxSpinCtrl8SetRangeEii"); return 0; }

long ZN10wxSpinCtrl8SetValueERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxSpinCtrl8SetValueERK8wxString"); return 0; }

long ZN10wxSpinCtrl8SetValueEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxSpinCtrl8SetValueEi"); return 0; }

long ZN10wxStockGDI11ms_instanceE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxStockGDI11ms_instanceE"); return 0; }

long ZN10wxStockGDI6GetPenENS_4ItemE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxStockGDI6GetPenENS_4ItemE"); return 0; }

long ZN10wxStockGDI8GetBrushENS_4ItemE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxStockGDI8GetBrushENS_4ItemE"); return 0; }

long ZN10wxStockGDI9DeleteAllEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxStockGDI9DeleteAllEv"); return 0; }

long ZN10wxStockGDI9GetColourENS_4ItemE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxStockGDI9GetColourENS_4ItemE"); return 0; }

long ZN10wxStockGDI9GetCursorENS_4ItemE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxStockGDI9GetCursorENS_4ItemE"); return 0; }

long ZN10wxStockGDIC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxStockGDIC1Ev"); return 0; }

long ZN10wxTextAttr4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextAttr4InitEv"); return 0; }

long ZN10wxTextAttr7CombineERKS_S1_PK14wxTextCtrlBase(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextAttr7CombineERKS_S1_PK14wxTextCtrlBase"); return 0; }

long ZN10wxTextAttrC1ERK8wxColourS2_RK6wxFont19wxTextAttrAlignment(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextAttrC1ERK8wxColourS2_RK6wxFont19wxTextAttrAlignment"); return 0; }

long ZN10wxTextAttraSERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextAttraSERKS_"); return 0; }

long ZN10wxTextCtrl10AppendTextERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl10AppendTextERK8wxString"); return 0; }

long ZN10wxTextCtrl10CreatePeerERK8wxStringRK7wxPointRK6wxSizel(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl10CreatePeerERK8wxStringRK7wxPointRK6wxSizel"); return 0; }

long ZN10wxTextCtrl10DoSetValueERK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl10DoSetValueERK8wxStringi"); return 0; }

long ZN10wxTextCtrl11SetEditableEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl11SetEditableEb"); return 0; }

long ZN10wxTextCtrl12DiscardEditsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl12DiscardEditsEv"); return 0; }

long ZN10wxTextCtrl12SetMaxLengthEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl12SetMaxLengthEm"); return 0; }

long ZN10wxTextCtrl12SetSelectionEll(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl12SetSelectionEll"); return 0; }

long ZN10wxTextCtrl12ShowPositionEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl12ShowPositionEl"); return 0; }

long ZN10wxTextCtrl12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl12ms_classInfoE"); return 0; }

long ZN10wxTextCtrl13sm_eventTableE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl13sm_eventTableE"); return 0; }

long ZN10wxTextCtrl14MacSetupCursorERK7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl14MacSetupCursorERK7wxPoint"); return 0; }

long ZN10wxTextCtrl15SetDefaultStyleERK10wxTextAttr(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl15SetDefaultStyleERK10wxTextAttr"); return 0; }

long ZN10wxTextCtrl16MacCheckSpellingEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl16MacCheckSpellingEb"); return 0; }

long ZN10wxTextCtrl17SetInsertionPointEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl17SetInsertionPointEl"); return 0; }

long ZN10wxTextCtrl20MacVisibilityChangedEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl20MacVisibilityChangedEv"); return 0; }

long ZN10wxTextCtrl20SetInsertionPointEndEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl20SetInsertionPointEndEv"); return 0; }

long ZN10wxTextCtrl22MacEnabledStateChangedEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl22MacEnabledStateChangedEv"); return 0; }

long ZN10wxTextCtrl23MacSuperChangedPositionEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl23MacSuperChangedPositionEv"); return 0; }

long ZN10wxTextCtrl3CutEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl3CutEv"); return 0; }

long ZN10wxTextCtrl4CopyEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl4CopyEv"); return 0; }

long ZN10wxTextCtrl4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl4InitEv"); return 0; }

long ZN10wxTextCtrl4RedoEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl4RedoEv"); return 0; }

long ZN10wxTextCtrl4UndoEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl4UndoEv"); return 0; }

long ZN10wxTextCtrl5ClearEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl5ClearEv"); return 0; }

long ZN10wxTextCtrl5PasteEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl5PasteEv"); return 0; }

long ZN10wxTextCtrl6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_"); return 0; }

long ZN10wxTextCtrl6RemoveEll(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl6RemoveEll"); return 0; }

long ZN10wxTextCtrl7CommandER14wxCommandEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl7CommandER14wxCommandEvent"); return 0; }

long ZN10wxTextCtrl7ReplaceEllRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl7ReplaceEllRK8wxString"); return 0; }

long ZN10wxTextCtrl7SetFontERK6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl7SetFontERK6wxFont"); return 0; }

long ZN10wxTextCtrl8SetStyleEllRK10wxTextAttr(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl8SetStyleEllRK10wxTextAttr"); return 0; }

long ZN10wxTextCtrl9MarkDirtyEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl9MarkDirtyEv"); return 0; }

long ZN10wxTextCtrl9WriteTextERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrl9WriteTextERK8wxString"); return 0; }

long ZN10wxTextCtrlD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTextCtrlD2Ev"); return 0; }

long ZN10wxToolbook4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxToolbook4InitEv"); return 0; }

long ZN10wxToolbook6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxToolbook6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN10wxToolbook7RealizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxToolbook7RealizeEv"); return 0; }

long ZN10wxTreeCtrl12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTreeCtrl12ms_classInfoE"); return 0; }

long ZN10wxTreebook4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTreebook4InitEv"); return 0; }

long ZN10wxTreebook6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxTreebook6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN10wxVListBox10SetMarginsERK7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxVListBox10SetMarginsERK7wxPoint"); return 0; }

long ZN10wxVListBox11DoSelectAllEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxVListBox11DoSelectAllEb"); return 0; }

long ZN10wxVListBox11SelectRangeEmm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxVListBox11SelectRangeEmm"); return 0; }

long ZN10wxVListBox12SetItemCountEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxVListBox12SetItemCountEm"); return 0; }

long ZN10wxVListBox12SetSelectionEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxVListBox12SetSelectionEi"); return 0; }

long ZN10wxVListBox12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxVListBox12ms_classInfoE"); return 0; }

long ZN10wxVListBox22SetSelectionBackgroundERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxVListBox22SetSelectionBackgroundERK8wxColour"); return 0; }

long ZN10wxVListBox25GetClassDefaultAttributesE15wxWindowVariant(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxVListBox25GetClassDefaultAttributesE15wxWindowVariant"); return 0; }

long ZN10wxVListBox4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxVListBox4InitEv"); return 0; }

long ZN10wxVListBox6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxVListBox6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN10wxVListBox6SelectEmb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxVListBox6SelectEmb"); return 0; }

long ZN10wxVListBoxD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxVListBoxD2Ev"); return 0; }

long ZN10wxWindowDCC1EP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxWindowDCC1EP8wxWindow"); return 0; }

long ZN10wxWindowDCD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxWindowDCD1Ev"); return 0; }

long ZN11wxBrushList11RemoveBrushEP7wxBrush(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxBrushList11RemoveBrushEP7wxBrush"); return 0; }

long ZN11wxBrushList17FindOrCreateBrushERK8wxColouri(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxBrushList17FindOrCreateBrushERK8wxColouri"); return 0; }

long ZN11wxBrushList8AddBrushEP7wxBrush(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxBrushList8AddBrushEP7wxBrush"); return 0; }

long ZN11wxCaretBase12GetBlinkTimeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxCaretBase12GetBlinkTimeEv"); return 0; }

long ZN11wxCaretBase12SetBlinkTimeEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxCaretBase12SetBlinkTimeEi"); return 0; }

long ZN11wxClassInfo8RegisterEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxClassInfo8RegisterEv"); return 0; }

long ZN11wxClassInfo8sm_firstE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxClassInfo8sm_firstE"); return 0; }

long ZN11wxClassInfoD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxClassInfoD1Ev"); return 0; }

long ZN11wxClipboardC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxClipboardC1Ev"); return 0; }

long ZN11wxComboCtrl12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxComboCtrl12ms_classInfoE"); return 0; }

long ZN11wxDCOverlay5ClearEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxDCOverlay5ClearEv"); return 0; }

long ZN11wxDCOverlayC1ER9wxOverlayP10wxWindowDC(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxDCOverlayC1ER9wxOverlayP10wxWindowDC"); return 0; }

long ZN11wxDCOverlayC1ER9wxOverlayP10wxWindowDCiiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxDCOverlayC1ER9wxOverlayP10wxWindowDCiiii"); return 0; }

long ZN11wxDirDialogC1EP8wxWindowRK8wxStringS4_lRK7wxPointRK6wxSizeS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxDirDialogC1EP8wxWindowRK8wxStringS4_lRK7wxPointRK6wxSizeS4_"); return 0; }

long ZN11wxFrameBase10DoGiveHelpERK8wxStringb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxFrameBase10DoGiveHelpERK8wxStringb"); return 0; }

long ZN11wxFrameBase10SetMenuBarEP9wxMenuBar(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxFrameBase10SetMenuBarEP9wxMenuBar"); return 0; }

long ZN11wxFrameBase12SetStatusBarEP11wxStatusBar(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxFrameBase12SetStatusBarEP11wxStatusBar"); return 0; }

long ZN11wxFrameBase13DoMenuUpdatesEP6wxMenu(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxFrameBase13DoMenuUpdatesEP6wxMenu"); return 0; }

long ZN11wxFrameBase13PopStatusTextEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxFrameBase13PopStatusTextEi"); return 0; }

long ZN11wxFrameBase13SendSizeEventEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxFrameBase13SendSizeEventEv"); return 0; }

long ZN11wxFrameBase13SetStatusTextERK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxFrameBase13SetStatusTextERK8wxStringi"); return 0; }

long ZN11wxFrameBase14OnInternalIdleEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxFrameBase14OnInternalIdleEv"); return 0; }

long ZN11wxFrameBase14ProcessCommandEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxFrameBase14ProcessCommandEi"); return 0; }

long ZN11wxFrameBase14PushStatusTextERK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxFrameBase14PushStatusTextERK8wxStringi"); return 0; }

long ZN11wxFrameBase14UpdateWindowUIEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxFrameBase14UpdateWindowUIEl"); return 0; }

long ZN11wxFrameBase15CreateStatusBarEiliRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxFrameBase15CreateStatusBarEiliRK8wxString"); return 0; }

long ZN11wxFrameBase15OnCreateToolBarEliRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxFrameBase15OnCreateToolBarEliRK8wxString"); return 0; }

long ZN11wxFrameBase15SetStatusWidthsEiPKi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxFrameBase15SetStatusWidthsEiPKi"); return 0; }

long ZN11wxFrameBaseC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxFrameBaseC2Ev"); return 0; }

long ZN11wxFrameBaseD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxFrameBaseD2Ev"); return 0; }

long ZN11wxGaugeBaseD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxGaugeBaseD2Ev"); return 0; }

long ZN11wxGridEventC1EiiP8wxObjectiiiibbbbb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxGridEventC1EiiP8wxObjectiiiibbbbb"); return 0; }

long ZN11wxGridSizerC1Eiiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxGridSizerC1Eiiii"); return 0; }

long ZN11wxHelpEvent11GuessOriginENS_6OriginE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxHelpEvent11GuessOriginENS_6OriginE"); return 0; }

long ZN11wxIconArrayD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxIconArrayD1Ev"); return 0; }

long ZN11wxIdleEvent11sm_idleModeE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxIdleEvent11sm_idleModeE"); return 0; }

long ZN11wxIdleEvent7CanSendEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxIdleEvent7CanSendEP8wxWindow"); return 0; }

long ZN11wxImageList3AddERK6wxIcon(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxImageList3AddERK6wxIcon"); return 0; }

long ZN11wxImageList3AddERK8wxBitmapRK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxImageList3AddERK8wxBitmapRK8wxColour"); return 0; }

long ZN11wxImageList3AddERK8wxBitmapS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxImageList3AddERK8wxBitmapS2_"); return 0; }

long ZN11wxImageList6RemoveEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxImageList6RemoveEi"); return 0; }

long ZN11wxImageList7ReplaceEiRK8wxBitmapS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxImageList7ReplaceEiRK8wxBitmapS2_"); return 0; }

long ZN11wxImageList9RemoveAllEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxImageList9RemoveAllEv"); return 0; }

long ZN11wxImageListC1Eiibi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxImageListC1Eiibi"); return 0; }

long ZN11wxLogStderrC1EP7__sFILE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxLogStderrC1EP7__sFILE"); return 0; }

long ZN11wxLogWindow4ShowEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxLogWindow4ShowEb"); return 0; }

long ZN11wxLogWindowC1EP8wxWindowPKwbb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxLogWindowC1EP8wxWindowPKwbb"); return 0; }

long ZN11wxMediaCtrl15GetPlaybackRateEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxMediaCtrl15GetPlaybackRateEv"); return 0; }

long ZN11wxMediaCtrl15SetPlaybackRateEd(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxMediaCtrl15SetPlaybackRateEd"); return 0; }

long ZN11wxMediaCtrl16GetDownloadTotalEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxMediaCtrl16GetDownloadTotalEv"); return 0; }

long ZN11wxMediaCtrl18ShowPlayerControlsE25wxMediaCtrlPlayerControls(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxMediaCtrl18ShowPlayerControlsE25wxMediaCtrlPlayerControls"); return 0; }

long ZN11wxMediaCtrl19GetDownloadProgressEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxMediaCtrl19GetDownloadProgressEv"); return 0; }

long ZN11wxMediaCtrl4LoadERK5wxURI(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxMediaCtrl4LoadERK5wxURI"); return 0; }

long ZN11wxMediaCtrl4LoadERK5wxURIS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxMediaCtrl4LoadERK5wxURIS2_"); return 0; }

long ZN11wxMediaCtrl4LoadERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxMediaCtrl4LoadERK8wxString"); return 0; }

long ZN11wxMediaCtrl4PlayEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxMediaCtrl4PlayEv"); return 0; }

long ZN11wxMediaCtrl4SeekEx10wxSeekMode(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxMediaCtrl4SeekEx10wxSeekMode"); return 0; }

long ZN11wxMediaCtrl4StopEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxMediaCtrl4StopEv"); return 0; }

long ZN11wxMediaCtrl4TellEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxMediaCtrl4TellEv"); return 0; }

long ZN11wxMediaCtrl5PauseEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxMediaCtrl5PauseEv"); return 0; }

long ZN11wxMediaCtrl6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_RK11wxValidatorS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxMediaCtrl6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_RK11wxValidatorS4_"); return 0; }

long ZN11wxMediaCtrl6LengthEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxMediaCtrl6LengthEv"); return 0; }

long ZN11wxMediaCtrl8GetStateEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxMediaCtrl8GetStateEv"); return 0; }

long ZN11wxMediaCtrl9GetVolumeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxMediaCtrl9GetVolumeEv"); return 0; }

long ZN11wxMediaCtrl9SetVolumeEd(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxMediaCtrl9SetVolumeEd"); return 0; }

long ZN11wxPrintData11SetPrivDataEPci(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxPrintData11SetPrivDataEPci"); return 0; }

long ZN11wxPrintDataC1ERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxPrintDataC1ERKS_"); return 0; }

long ZN11wxPrintDataC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxPrintDataC1Ev"); return 0; }

long ZN11wxPrintDataaSERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxPrintDataaSERKS_"); return 0; }

long ZN11wxPrinterDCC1ERK11wxPrintData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxPrinterDCC1ERK11wxPrintData"); return 0; }

long ZN11wxRect2DIntaSERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxRect2DIntaSERKS_"); return 0; }

long ZN11wxScrollBar6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK11wxValidatorRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxScrollBar6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK11wxValidatorRK8wxString"); return 0; }

long ZN11wxSizerItem4InitERK12wxSizerFlags(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxSizerItem4InitERK12wxSizerFlags"); return 0; }

long ZN11wxSizerItem4ShowEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxSizerItem4ShowEb"); return 0; }

long ZN11wxSizerItem8SetSizerEP7wxSizer(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxSizerItem8SetSizerEP7wxSizer"); return 0; }

long ZN11wxSizerItem9SetSpacerERK6wxSize(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxSizerItem9SetSpacerERK6wxSize"); return 0; }

long ZN11wxSizerItem9SetWindowEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxSizerItem9SetWindowEP8wxWindow"); return 0; }

long ZN11wxSizerItemC1EP7wxSizeriiiP8wxObject(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxSizerItemC1EP7wxSizeriiiP8wxObject"); return 0; }

long ZN11wxSizerItemC1EP8wxWindowiiiP8wxObject(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxSizerItemC1EP8wxWindowiiiP8wxObject"); return 0; }

long ZN11wxSizerItemC1EiiiiiP8wxObject(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxSizerItemC1EiiiiiP8wxObject"); return 0; }

long ZN11wxSizerItemC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxSizerItemC1Ev"); return 0; }

long ZN11wxStaticBox6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxStaticBox6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_"); return 0; }

long ZN11wxStatusBar6CreateEP8wxWindowilRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxStatusBar6CreateEP8wxWindowilRK8wxString"); return 0; }

long ZN11wxStatusBarC1EP8wxWindowilRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxStatusBarC1EP8wxWindowilRK8wxString"); return 0; }

long ZN11wxStatusBarC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxStatusBarC1Ev"); return 0; }

long ZN11wxStopWatch5StartEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxStopWatch5StartEl"); return 0; }

long ZN11wxTimerBase6NotifyEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxTimerBase6NotifyEv"); return 0; }

long ZN11wxTimerBaseD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxTimerBaseD2Ev"); return 0; }

long ZN11wxTipWindow15SetBoundingRectERK6wxRect(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxTipWindow15SetBoundingRectERK6wxRect"); return 0; }

long ZN11wxTipWindow5CloseEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxTipWindow5CloseEv"); return 0; }

long ZN11wxTipWindowC1EP8wxWindowRK8wxStringiPPS_P6wxRect(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxTipWindowC1EP8wxWindowRK8wxStringiPPS_P6wxRect"); return 0; }

long ZN11wxTreeEventC1EiP14wxTreeCtrlBaseRK12wxTreeItemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxTreeEventC1EiP14wxTreeCtrlBaseRK12wxTreeItemId"); return 0; }

long ZN11wxTreeEventC1Eii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxTreeEventC1Eii"); return 0; }

long ZN11wxValidator11ms_isSilentE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxValidator11ms_isSilentE"); return 0; }

long ZN11wxValidator12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxValidator12ms_classInfoE"); return 0; }

long ZN11wxValidatorC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxValidatorC1Ev"); return 0; }

long ZN11wxValidatorC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxValidatorC2Ev"); return 0; }

long ZN11wxValidatorD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxValidatorD2Ev"); return 0; }

long ZN12wxAppConsole11FilterEventER7wxEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAppConsole11FilterEventER7wxEvent"); return 0; }

long ZN12wxAppConsole13OnCmdLineHelpER15wxCmdLineParser(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAppConsole13OnCmdLineHelpER15wxCmdLineParser"); return 0; }

long ZN12wxAppConsole14OnCmdLineErrorER15wxCmdLineParser(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAppConsole14OnCmdLineErrorER15wxCmdLineParser"); return 0; }

long ZN12wxAppConsole14ms_appInstanceE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAppConsole14ms_appInstanceE"); return 0; }

long ZN12wxAppConsole15OnAssertFailureEPKwiS1_S1_S1_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAppConsole15OnAssertFailureEPKwiS1_S1_S1_"); return 0; }

long ZN12wxAppConsole17CheckBuildOptionsEPKcS1_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAppConsole17CheckBuildOptionsEPKcS1_"); return 0; }

long ZN12wxAppConsole20ProcessPendingEventsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAppConsole20ProcessPendingEventsEv"); return 0; }

long ZN12wxAppConsole8OnAssertEPKwiS1_S1_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAppConsole8OnAssertEPKwiS1_S1_"); return 0; }

long ZN12wxAppConsole9GetTraitsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAppConsole9GetTraitsEv"); return 0; }

long ZN12wxAuiManager10DetachPaneEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiManager10DetachPaneEP8wxWindow"); return 0; }

long ZN12wxAuiManager10GetManagerEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiManager10GetManagerEP8wxWindow"); return 0; }

long ZN12wxAuiManager10InsertPaneEP8wxWindowRK13wxAuiPaneInfoi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiManager10InsertPaneEP8wxWindowRK13wxAuiPaneInfoi"); return 0; }

long ZN12wxAuiManager11GetAllPanesEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiManager11GetAllPanesEv"); return 0; }

long ZN12wxAuiManager11RestorePaneER13wxAuiPaneInfo(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiManager11RestorePaneER13wxAuiPaneInfo"); return 0; }

long ZN12wxAuiManager12DrawHintRectEP8wxWindowRK7wxPointS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiManager12DrawHintRectEP8wxWindowRK7wxPointS4_"); return 0; }

long ZN12wxAuiManager12LoadPaneInfoE8wxStringR13wxAuiPaneInfo(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiManager12LoadPaneInfoE8wxStringR13wxAuiPaneInfo"); return 0; }

long ZN12wxAuiManager12MaximizePaneER13wxAuiPaneInfo(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiManager12MaximizePaneER13wxAuiPaneInfo"); return 0; }

long ZN12wxAuiManager12OnPaneButtonER17wxAuiManagerEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiManager12OnPaneButtonER17wxAuiManagerEvent"); return 0; }

long ZN12wxAuiManager12SavePaneInfoER13wxAuiPaneInfo(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiManager12SavePaneInfoER13wxAuiPaneInfo"); return 0; }

long ZN12wxAuiManager13StartPaneDragEP8wxWindowRK7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiManager13StartPaneDragEP8wxWindowRK7wxPoint"); return 0; }

long ZN12wxAuiManager14SetArtProviderEP12wxAuiDockArt(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiManager14SetArtProviderEP12wxAuiDockArt"); return 0; }

long ZN12wxAuiManager15LoadPerspectiveERK8wxStringb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiManager15LoadPerspectiveERK8wxStringb"); return 0; }

long ZN12wxAuiManager15SavePerspectiveEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiManager15SavePerspectiveEv"); return 0; }

long ZN12wxAuiManager16SetManagedWindowEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiManager16SetManagedWindowEP8wxWindow"); return 0; }

long ZN12wxAuiManager17CalculateHintRectEP8wxWindowRK7wxPointS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiManager17CalculateHintRectEP8wxWindowRK7wxPointS4_"); return 0; }

long ZN12wxAuiManager20RestoreMaximizedPaneEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiManager20RestoreMaximizedPaneEv"); return 0; }

long ZN12wxAuiManager21SetDockSizeConstraintEdd(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiManager21SetDockSizeConstraintEdd"); return 0; }

long ZN12wxAuiManager6UnInitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiManager6UnInitEv"); return 0; }

long ZN12wxAuiManager6UpdateEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiManager6UpdateEv"); return 0; }

long ZN12wxAuiManager7AddPaneEP8wxWindowRK13wxAuiPaneInfo(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiManager7AddPaneEP8wxWindowRK13wxAuiPaneInfo"); return 0; }

long ZN12wxAuiManager7AddPaneEP8wxWindowRK13wxAuiPaneInfoRK7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiManager7AddPaneEP8wxWindowRK13wxAuiPaneInfoRK7wxPoint"); return 0; }

long ZN12wxAuiManager7AddPaneEP8wxWindowiRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiManager7AddPaneEP8wxWindowiRK8wxString"); return 0; }

long ZN12wxAuiManager7GetPaneEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiManager7GetPaneEP8wxWindow"); return 0; }

long ZN12wxAuiManager7GetPaneERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiManager7GetPaneERK8wxString"); return 0; }

long ZN12wxAuiManager8OnRenderER17wxAuiManagerEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiManager8OnRenderER17wxAuiManagerEvent"); return 0; }

long ZN12wxAuiManager8SetFlagsEj(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiManager8SetFlagsEj"); return 0; }

long ZN12wxAuiManager9ClosePaneER13wxAuiPaneInfo(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiManager9ClosePaneER13wxAuiPaneInfo"); return 0; }

long ZN12wxAuiManagerC1EP8wxWindowj(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiManagerC1EP8wxWindowj"); return 0; }

long ZN12wxAuiTabCtrlC1EP8wxWindowiRK7wxPointRK6wxSizel(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxAuiTabCtrlC1EP8wxWindowiRK7wxPointRK6wxSizel"); return 0; }

long ZN12wxBufferedDC9UseBufferEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxBufferedDC9UseBufferEii"); return 0; }

long ZN12wxCaretTimerC1EP7wxCaret(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxCaretTimerC1EP7wxCaret"); return 0; }

long ZN12wxChoiceBaseD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxChoiceBaseD2Ev"); return 0; }

long ZN12wxChoicebook4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxChoicebook4InitEv"); return 0; }

long ZN12wxChoicebook6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxChoicebook6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN12wxColourData15GetCustomColourEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxColourData15GetCustomColourEi"); return 0; }

long ZN12wxColourData15SetCustomColourEiRK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxColourData15SetCustomColourEiRK8wxColour"); return 0; }

long ZN12wxColourDataC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxColourDataC1Ev"); return 0; }

long ZN12wxComboPopup10LazyCreateEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxComboPopup10LazyCreateEv"); return 0; }

long ZN12wxComboPopup14SetStringValueERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxComboPopup14SetStringValueERK8wxString"); return 0; }

long ZN12wxComboPopup15GetAdjustedSizeEiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxComboPopup15GetAdjustedSizeEiii"); return 0; }

long ZN12wxComboPopup15OnComboKeyEventER10wxKeyEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxComboPopup15OnComboKeyEventER10wxKeyEvent"); return 0; }

long ZN12wxComboPopup17PaintComboControlER4wxDCRK6wxRect(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxComboPopup17PaintComboControlER4wxDCRK6wxRect"); return 0; }

long ZN12wxComboPopup18OnComboDoubleClickEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxComboPopup18OnComboDoubleClickEv"); return 0; }

long ZN12wxComboPopup24DefaultPaintComboControlEP15wxComboCtrlBaseR4wxDCRK6wxRect(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxComboPopup24DefaultPaintComboControlEP15wxComboCtrlBaseR4wxDCRK6wxRect"); return 0; }

long ZN12wxComboPopup7DismissEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxComboPopup7DismissEv"); return 0; }

long ZN12wxComboPopup7OnPopupEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxComboPopup7OnPopupEv"); return 0; }

long ZN12wxComboPopup9OnDismissEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxComboPopup9OnDismissEv"); return 0; }

long ZN12wxComboPopupD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxComboPopupD2Ev"); return 0; }

long ZN12wxConfigBase10ms_pConfigE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxConfigBase10ms_pConfigE"); return 0; }

long ZN12wxConfigBase14ms_bAutoCreateE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxConfigBase14ms_bAutoCreateE"); return 0; }

long ZN12wxConfigBase3SetEPS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxConfigBase3SetEPS_"); return 0; }

long ZN12wxConfigBase6CreateEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxConfigBase6CreateEv"); return 0; }

long ZN12wxDataFormat5SetIdEPKw(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxDataFormat5SetIdEPKw"); return 0; }

long ZN12wxDataFormat7SetTypeE14wxDataFormatId(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxDataFormat7SetTypeE14wxDataFormatId"); return 0; }

long ZN12wxDataFormatC1E14wxDataFormatId(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxDataFormatC1E14wxDataFormatId"); return 0; }

long ZN12wxDataFormatC1ERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxDataFormatC1ERK8wxString"); return 0; }

long ZN12wxDataFormatC1ERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxDataFormatC1ERKS_"); return 0; }

long ZN12wxDataFormatC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxDataFormatC1Ev"); return 0; }

long ZN12wxDataFormatD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxDataFormatD1Ev"); return 0; }

long ZN12wxDataFormataSERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxDataFormataSERKS_"); return 0; }

long ZN12wxDataObjectC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxDataObjectC2Ev"); return 0; }

long ZN12wxDialogBase11RemoveChildEP12wxWindowBase(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxDialogBase11RemoveChildEP12wxWindowBase"); return 0; }

long ZN12wxDialogBase11SetEscapeIdEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxDialogBase11SetEscapeIdEi"); return 0; }

long ZN12wxDialogBase12OnChildFocusER17wxChildFocusEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxDialogBase12OnChildFocusER17wxChildFocusEvent"); return 0; }

long ZN12wxDialogBase15CreateTextSizerERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxDialogBase15CreateTextSizerERK8wxString"); return 0; }

long ZN12wxDialogBase16SetAffirmativeIdEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxDialogBase16SetAffirmativeIdEi"); return 0; }

long ZN12wxDialogBase17CreateButtonSizerEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxDialogBase17CreateButtonSizerEl"); return 0; }

long ZN12wxDialogBase24SetFocusIgnoringChildrenEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxDialogBase24SetFocusIgnoringChildrenEv"); return 0; }

long ZN12wxDialogBase26CreateSeparatedButtonSizerEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxDialogBase26CreateSeparatedButtonSizerEl"); return 0; }

long ZN12wxDialogBase26CreateStdDialogButtonSizerEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxDialogBase26CreateStdDialogButtonSizerEl"); return 0; }

long ZN12wxDialogBase4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxDialogBase4InitEv"); return 0; }

long ZN12wxDialogBase8SetFocusEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxDialogBase8SetFocusEv"); return 0; }

long ZN12wxDropSource10DoDragDropEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxDropSource10DoDragDropEi"); return 0; }

long ZN12wxDropSourceC2EP8wxWindowRK8wxCursorS4_S4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxDropSourceC2EP8wxWindowRK8wxCursorS4_S4_"); return 0; }

long ZN12wxDropSourceD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxDropSourceD2Ev"); return 0; }

long ZN12wxDropTarget10OnDragOverEii12wxDragResult(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxDropTarget10OnDragOverEii12wxDragResult"); return 0; }

long ZN12wxDropTarget6OnDropEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxDropTarget6OnDropEii"); return 0; }

long ZN12wxDropTarget7GetDataEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxDropTarget7GetDataEv"); return 0; }

long ZN12wxDropTargetC2EP12wxDataObject(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxDropTargetC2EP12wxDataObject"); return 0; }

long ZN12wxEvtHandler10DisconnectEiiiM8wxObjectFvR7wxEventEPS0_PS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxEvtHandler10DisconnectEiiiM8wxObjectFvR7wxEventEPS0_PS_"); return 0; }

long ZN12wxEvtHandler12ProcessEventER7wxEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxEvtHandler12ProcessEventER7wxEvent"); return 0; }

long ZN12wxEvtHandler12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxEvtHandler12ms_classInfoE"); return 0; }

long ZN12wxEvtHandler15AddPendingEventER7wxEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxEvtHandler15AddPendingEventER7wxEvent"); return 0; }

long ZN12wxEvtHandler15DoSetClientDataEPv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxEvtHandler15DoSetClientDataEPv"); return 0; }

long ZN12wxEvtHandler16SearchEventTableER12wxEventTableR7wxEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxEvtHandler16SearchEventTableER12wxEventTableR7wxEvent"); return 0; }

long ZN12wxEvtHandler17DoSetClientObjectEP12wxClientData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxEvtHandler17DoSetClientObjectEP12wxClientData"); return 0; }

long ZN12wxEvtHandler20ProcessPendingEventsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxEvtHandler20ProcessPendingEventsEv"); return 0; }

long ZN12wxEvtHandler7ConnectEiiiM8wxObjectFvR7wxEventEPS0_PS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxEvtHandler7ConnectEiiiM8wxObjectFvR7wxEventEPS0_PS_"); return 0; }

long ZN12wxEvtHandler9TryParentER7wxEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxEvtHandler9TryParentER7wxEvent"); return 0; }

long ZN12wxEvtHandlerC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxEvtHandlerC1Ev"); return 0; }

long ZN12wxEvtHandlerC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxEvtHandlerC2Ev"); return 0; }

long ZN12wxEvtHandlerD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxEvtHandlerD2Ev"); return 0; }

long ZN12wxFileConfigC1ERK8wxStringS2_S2_S2_lRK8wxMBConv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxFileConfigC1ERK8wxStringS2_S2_S2_lRK8wxMBConv"); return 0; }

long ZN12wxFileDialogC1EP8wxWindowRK8wxStringS4_S4_S4_lRK7wxPointRK6wxSizeS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxFileDialogC1EP8wxWindowRK8wxStringS4_S4_S4_lRK7wxPointRK6wxSizeS4_"); return 0; }

long ZN12wxFileSystem10AddHandlerEP19wxFileSystemHandler(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxFileSystem10AddHandlerEP19wxFileSystemHandler"); return 0; }

long ZN12wxFileSystem12ChangePathToERK8wxStringb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxFileSystem12ChangePathToERK8wxStringb"); return 0; }

long ZN12wxFileSystem13FileNameToURLERK10wxFileName(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxFileSystem13FileNameToURLERK10wxFileName"); return 0; }

long ZN12wxFileSystem13RemoveHandlerEP19wxFileSystemHandler(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxFileSystem13RemoveHandlerEP19wxFileSystemHandler"); return 0; }

long ZN12wxFileSystem13URLToFileNameERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxFileSystem13URLToFileNameERK8wxString"); return 0; }

long ZN12wxFileSystem15CleanUpHandlersEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxFileSystem15CleanUpHandlersEv"); return 0; }

long ZN12wxFileSystem8FindNextEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxFileSystem8FindNextEv"); return 0; }

long ZN12wxFileSystem8OpenFileERK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxFileSystem8OpenFileERK8wxStringi"); return 0; }

long ZN12wxFileSystem9FindFirstERK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxFileSystem9FindFirstERK8wxStringi"); return 0; }

long ZN12wxFileSystemD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxFileSystemD1Ev"); return 0; }

long ZN12wxFontDialogC1EP8wxWindowRK10wxFontData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxFontDialogC1EP8wxWindowRK10wxFontData"); return 0; }

long ZN12wxFontMapper17GetAltForEncodingE14wxFontEncodingPS0_RK8wxStringb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxFontMapper17GetAltForEncodingE14wxFontEncodingPS0_RK8wxStringb"); return 0; }

long ZN12wxFontMapper3GetEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxFontMapper3GetEv"); return 0; }

long ZN12wxFontMapperC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxFontMapperC1Ev"); return 0; }

long ZN12wxHtmlFilter12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlFilter12ms_classInfoE"); return 0; }

long ZN12wxHtmlParser13PopTagHandlerEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlParser13PopTagHandlerEv"); return 0; }

long ZN12wxHtmlParser14GetInnerSourceERK9wxHtmlTag(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlParser14GetInnerSourceERK9wxHtmlTag"); return 0; }

long ZN12wxHtmlParser14PushTagHandlerEP16wxHtmlTagHandlerRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlParser14PushTagHandlerEP16wxHtmlTagHandlerRK8wxString"); return 0; }

long ZN12wxHtmlParser5ParseERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlParser5ParseERK8wxString"); return 0; }

long ZN12wxHtmlParser9DoParsingEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlParser9DoParsingEii"); return 0; }

long ZN12wxHtmlWindow10OnSetTitleERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow10OnSetTitleERK8wxString"); return 0; }

long ZN12wxHtmlWindow10SelectLineERK7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow10SelectLineERK7wxPoint"); return 0; }

long ZN12wxHtmlWindow10SelectWordERK7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow10SelectWordERK7wxPoint"); return 0; }

long ZN12wxHtmlWindow11HistoryBackEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow11HistoryBackEv"); return 0; }

long ZN12wxHtmlWindow12AppendToPageERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow12AppendToPageERK8wxString"); return 0; }

long ZN12wxHtmlWindow12HistoryClearEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow12HistoryClearEv"); return 0; }

long ZN12wxHtmlWindow12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow12ms_classInfoE"); return 0; }

long ZN12wxHtmlWindow13GetHTMLWindowEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow13GetHTMLWindowEv"); return 0; }

long ZN12wxHtmlWindow13OnLinkClickedERK14wxHtmlLinkInfo(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow13OnLinkClickedERK14wxHtmlLinkInfo"); return 0; }

long ZN12wxHtmlWindow14HistoryCanBackEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow14HistoryCanBackEv"); return 0; }

long ZN12wxHtmlWindow14HistoryForwardEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow14HistoryForwardEv"); return 0; }

long ZN12wxHtmlWindow14OnInternalIdleEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow14OnInternalIdleEv"); return 0; }

long ZN12wxHtmlWindow14ScrollToAnchorERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow14ScrollToAnchorERK8wxString"); return 0; }

long ZN12wxHtmlWindow15SetRelatedFrameEP7wxFrameRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow15SetRelatedFrameEP7wxFrameRK8wxString"); return 0; }

long ZN12wxHtmlWindow16SetStandardFontsEiRK8wxStringS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow16SetStandardFontsEiRK8wxStringS2_"); return 0; }

long ZN12wxHtmlWindow17DoSelectionToTextEP15wxHtmlSelection(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow17DoSelectionToTextEP15wxHtmlSelection"); return 0; }

long ZN12wxHtmlWindow17HistoryCanForwardEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow17HistoryCanForwardEv"); return 0; }

long ZN12wxHtmlWindow17OnHTMLLinkClickedERK14wxHtmlLinkInfo(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow17OnHTMLLinkClickedERK14wxHtmlLinkInfo"); return 0; }

long ZN12wxHtmlWindow17ReadCustomizationEP12wxConfigBase8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow17ReadCustomizationEP12wxConfigBase8wxString"); return 0; }

long ZN12wxHtmlWindow17SetHTMLStatusTextERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow17SetHTMLStatusTextERK8wxString"); return 0; }

long ZN12wxHtmlWindow18SetHTMLWindowTitleERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow18SetHTMLWindowTitleERK8wxString"); return 0; }

long ZN12wxHtmlWindow18WriteCustomizationEP12wxConfigBase8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow18WriteCustomizationEP12wxConfigBase8wxString"); return 0; }

long ZN12wxHtmlWindow19SetRelatedStatusBarEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow19SetRelatedStatusBarEi"); return 0; }

long ZN12wxHtmlWindow20GetDefaultHTMLCursorEN21wxHtmlWindowInterface10HTMLCursorE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow20GetDefaultHTMLCursorEN21wxHtmlWindowInterface10HTMLCursorE"); return 0; }

long ZN12wxHtmlWindow22SetHTMLBackgroundImageERK8wxBitmap(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow22SetHTMLBackgroundImageERK8wxBitmap"); return 0; }

long ZN12wxHtmlWindow23SetHTMLBackgroundColourERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow23SetHTMLBackgroundColourERK8wxColour"); return 0; }

long ZN12wxHtmlWindow4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow4InitEv"); return 0; }

long ZN12wxHtmlWindow6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN12wxHtmlWindow6ToTextEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow6ToTextEv"); return 0; }

long ZN12wxHtmlWindow7SetPageERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow7SetPageERK8wxString"); return 0; }

long ZN12wxHtmlWindow8LoadFileERK10wxFileName(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow8LoadFileERK10wxFileName"); return 0; }

long ZN12wxHtmlWindow8LoadPageERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow8LoadPageERK8wxString"); return 0; }

long ZN12wxHtmlWindow8SetFontsERK8wxStringS2_PKi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow8SetFontsERK8wxStringS2_PKi"); return 0; }

long ZN12wxHtmlWindow9AddFilterEP12wxHtmlFilter(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow9AddFilterEP12wxHtmlFilter"); return 0; }

long ZN12wxHtmlWindow9SelectAllEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindow9SelectAllEv"); return 0; }

long ZN12wxHtmlWindowD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxHtmlWindowD2Ev"); return 0; }

long ZN12wxIconBundle11DeleteIconsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxIconBundle11DeleteIconsEv"); return 0; }

long ZN12wxIconBundle7AddIconERK6wxIcon(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxIconBundle7AddIconERK6wxIcon"); return 0; }

long ZN12wxIconBundle7AddIconERK8wxStringl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxIconBundle7AddIconERK8wxStringl"); return 0; }

long ZN12wxMetafileDCC1ERK8wxStringiiS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxMetafileDCC1ERK8wxStringiiS2_"); return 0; }

long ZN12wxMouseEvent6AssignERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxMouseEvent6AssignERKS_"); return 0; }

long ZN12wxMouseEventC1Ei(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxMouseEventC1Ei"); return 0; }

long ZN12wxPickerBase10CreateBaseEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxPickerBase10CreateBaseEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_"); return 0; }

long ZN12wxRegionBase5UnionERK8wxBitmap(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxRegionBase5UnionERK8wxBitmap"); return 0; }

long ZN12wxRegionBase5UnionERK8wxBitmapRK8wxColouri(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxRegionBase5UnionERK8wxBitmapRK8wxColouri"); return 0; }

long ZN12wxSashWindow11SashHitTestEiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxSashWindow11SashHitTestEiii"); return 0; }

long ZN12wxSashWindow11SizeWindowsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxSashWindow11SizeWindowsEv"); return 0; }

long ZN12wxSashWindow14SetSashVisibleE18wxSashEdgePositionb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxSashWindow14SetSashVisibleE18wxSashEdgePositionb"); return 0; }

long ZN12wxSashWindow4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxSashWindow4InitEv"); return 0; }

long ZN12wxSashWindow6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxSashWindow6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN12wxSashWindowD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxSashWindowD2Ev"); return 0; }

long ZN12wxSearchCtrl18SetDescriptiveTextERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxSearchCtrl18SetDescriptiveTextERK8wxString"); return 0; }

long ZN12wxSearchCtrl6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxSearchCtrl6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_"); return 0; }

long ZN12wxSearchCtrlC1EP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxSearchCtrlC1EP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_"); return 0; }

long ZN12wxSearchCtrlC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxSearchCtrlC1Ev"); return 0; }

long ZN12wxSizerFlags24ReserveSpaceEvenIfHiddenEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxSizerFlags24ReserveSpaceEvenIfHiddenEv"); return 0; }

long ZN12wxSpinButton6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxSpinButton6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN12wxSpinButtonC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxSpinButtonC1Ev"); return 0; }

long ZN12wxStaticLine6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxStaticLine6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN12wxStaticText6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxStaticText6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_"); return 0; }

long ZN12wxStringBase10ConcatSelfEmPKwm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxStringBase10ConcatSelfEmPKwm"); return 0; }

long ZN12wxStringBase4nposE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxStringBase4nposE"); return 0; }

long ZN12wxStringBase4swapERS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxStringBase4swapERS_"); return 0; }

long ZN12wxStringBase5AllocEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxStringBase5AllocEm"); return 0; }

long ZN12wxStringBase5eraseEmm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxStringBase5eraseEmm"); return 0; }

long ZN12wxStringBase6appendEmw(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxStringBase6appendEmw"); return 0; }

long ZN12wxStringBase8InitWithEPKwmm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxStringBase8InitWithEPKwmm"); return 0; }

long ZN12wxStringBaseaSEPKw(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxStringBaseaSEPKw"); return 0; }

long ZN12wxStringBaseaSERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxStringBaseaSERKS_"); return 0; }

long ZN12wxStringHash16wxCharStringHashEPKw(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxStringHash16wxCharStringHashEPKw"); return 0; }

long ZN12wxStringListC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxStringListC1Ev"); return 0; }

long ZN12wxTextAttrEx4CopyERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxTextAttrEx4CopyERKS_"); return 0; }

long ZN12wxTextAttrEx4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxTextAttrEx4InitEv"); return 0; }

long ZN12wxTextAttrEx9CombineExERKS_S1_PK14wxTextCtrlBase(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxTextAttrEx9CombineExERKS_S1_PK14wxTextCtrlBase"); return 0; }

long ZN12wxTextAttrExC1ERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxTextAttrExC1ERKS_"); return 0; }

long ZN12wxTextAttrExaSERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxTextAttrExaSERKS_"); return 0; }

long ZN12wxWebKitCtrl10IsEditableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWebKitCtrl10IsEditableEv"); return 0; }

long ZN12wxWebKitCtrl12CanGoForwardEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWebKitCtrl12CanGoForwardEv"); return 0; }

long ZN12wxWebKitCtrl12GetScrollPosEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWebKitCtrl12GetScrollPosEv"); return 0; }

long ZN12wxWebKitCtrl12GetSelectionEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWebKitCtrl12GetSelectionEv"); return 0; }

long ZN12wxWebKitCtrl12MakeEditableEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWebKitCtrl12MakeEditableEb"); return 0; }

long ZN12wxWebKitCtrl12SetScrollPosEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWebKitCtrl12SetScrollPosEi"); return 0; }

long ZN12wxWebKitCtrl13GetPageSourceEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWebKitCtrl13GetPageSourceEv"); return 0; }

long ZN12wxWebKitCtrl13SetPageSourceERK8wxStringS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWebKitCtrl13SetPageSourceERK8wxStringS2_"); return 0; }

long ZN12wxWebKitCtrl16CanGetPageSourceEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWebKitCtrl16CanGetPageSourceEv"); return 0; }

long ZN12wxWebKitCtrl16DecreaseTextSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWebKitCtrl16DecreaseTextSizeEv"); return 0; }

long ZN12wxWebKitCtrl16IncreaseTextSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWebKitCtrl16IncreaseTextSizeEv"); return 0; }

long ZN12wxWebKitCtrl19CanDecreaseTextSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWebKitCtrl19CanDecreaseTextSizeEv"); return 0; }

long ZN12wxWebKitCtrl19CanIncreaseTextSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWebKitCtrl19CanIncreaseTextSizeEv"); return 0; }

long ZN12wxWebKitCtrl4StopEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWebKitCtrl4StopEv"); return 0; }

long ZN12wxWebKitCtrl5PrintEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWebKitCtrl5PrintEb"); return 0; }

long ZN12wxWebKitCtrl6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWebKitCtrl6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_"); return 0; }

long ZN12wxWebKitCtrl6GoBackEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWebKitCtrl6GoBackEv"); return 0; }

long ZN12wxWebKitCtrl6ReloadEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWebKitCtrl6ReloadEv"); return 0; }

long ZN12wxWebKitCtrl7LoadURLERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWebKitCtrl7LoadURLERK8wxString"); return 0; }

long ZN12wxWebKitCtrl9CanGoBackEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWebKitCtrl9CanGoBackEv"); return 0; }

long ZN12wxWebKitCtrl9GoForwardEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWebKitCtrl9GoForwardEv"); return 0; }

long ZN12wxWebKitCtrl9RunScriptERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWebKitCtrl9RunScriptERK8wxString"); return 0; }

long ZN12wxWindowBase10GetCaptureEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase10GetCaptureEv"); return 0; }

long ZN12wxWindowBase10InitDialogEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase10InitDialogEv"); return 0; }

long ZN12wxWindowBase10SetToolTipERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase10SetToolTipERK8wxString"); return 0; }

long ZN12wxWindowBase11SetHelpTextERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase11SetHelpTextERK8wxString"); return 0; }

long ZN12wxWindowBase12CaptureMouseEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase12CaptureMouseEv"); return 0; }

long ZN12wxWindowBase12LayoutPhase1EPi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase12LayoutPhase1EPi"); return 0; }

long ZN12wxWindowBase12LayoutPhase2EPi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase12LayoutPhase2EPi"); return 0; }

long ZN12wxWindowBase12ReleaseMouseEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase12ReleaseMouseEv"); return 0; }

long ZN12wxWindowBase12SetValidatorERK11wxValidator(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase12SetValidatorERK11wxValidator"); return 0; }

long ZN12wxWindowBase12TryValidatorER7wxEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase12TryValidatorER7wxEvent"); return 0; }

long ZN12wxWindowBase14DoSetSizeHintsEiiiiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase14DoSetSizeHintsEiiiiii"); return 0; }

long ZN12wxWindowBase14FindWindowByIdElPK8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase14FindWindowByIdElPK8wxWindow"); return 0; }

long ZN12wxWindowBase14MoveConstraintEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase14MoveConstraintEii"); return 0; }

long ZN12wxWindowBase14SetConstraintsEP19wxLayoutConstraints(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase14SetConstraintsEP19wxLayoutConstraints"); return 0; }

long ZN12wxWindowBase14SetInitialSizeERK6wxSize(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase14SetInitialSizeERK6wxSize"); return 0; }

long ZN12wxWindowBase14SetSizerAndFitEP7wxSizerb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase14SetSizerAndFitEP7wxSizerb"); return 0; }

long ZN12wxWindowBase14UpdateWindowUIEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase14UpdateWindowUIEl"); return 0; }

long ZN12wxWindowBase15DestroyChildrenEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase15DestroyChildrenEv"); return 0; }

long ZN12wxWindowBase15PopEventHandlerEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase15PopEventHandlerEb"); return 0; }

long ZN12wxWindowBase16DoMoveInTabOrderEP8wxWindowNS_8MoveKindE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase16DoMoveInTabOrderEP8wxWindowNS_8MoveKindE"); return 0; }

long ZN12wxWindowBase16DoSetVirtualSizeEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase16DoSetVirtualSizeEii"); return 0; }

long ZN12wxWindowBase16DoUpdateWindowUIER15wxUpdateUIEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase16DoUpdateWindowUIER15wxUpdateUIEvent"); return 0; }

long ZN12wxWindowBase16FindWindowByNameERK8wxStringPK8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase16FindWindowByNameERK8wxStringPK8wxWindow"); return 0; }

long ZN12wxWindowBase16PushEventHandlerEP12wxEvtHandler(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase16PushEventHandlerEP12wxEvtHandler"); return 0; }

long ZN12wxWindowBase16SetHelpTextForIdERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase16SetHelpTextForIdERK8wxString"); return 0; }

long ZN12wxWindowBase16SetWindowVariantE15wxWindowVariant(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase16SetWindowVariantE15wxWindowVariant"); return 0; }

long ZN12wxWindowBase16ms_lastControlIdE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase16ms_lastControlIdE"); return 0; }

long ZN12wxWindowBase17FindWindowByLabelERK8wxStringPK8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase17FindWindowByLabelERK8wxStringPK8wxWindow"); return 0; }

long ZN12wxWindowBase17InheritAttributesEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase17InheritAttributesEv"); return 0; }

long ZN12wxWindowBase17SetSizeConstraintEiiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase17SetSizeConstraintEiiii"); return 0; }

long ZN12wxWindowBase17ToggleWindowStyleEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase17ToggleWindowStyleEi"); return 0; }

long ZN12wxWindowBase18InvalidateBestSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase18InvalidateBestSizeEv"); return 0; }

long ZN12wxWindowBase18RemoveEventHandlerEP12wxEvtHandler(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase18RemoveEventHandlerEP12wxEvtHandler"); return 0; }

long ZN12wxWindowBase18SetConstraintSizesEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase18SetConstraintSizesEb"); return 0; }

long ZN12wxWindowBase18SetContainingSizerEP7wxSizer(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase18SetContainingSizerEP7wxSizer"); return 0; }

long ZN12wxWindowBase19SetVirtualSizeHintsEiiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase19SetVirtualSizeHintsEiiii"); return 0; }

long ZN12wxWindowBase20TransferDataToWindowEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase20TransferDataToWindowEv"); return 0; }

long ZN12wxWindowBase21ConvertDialogToPixelsERK7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase21ConvertDialogToPixelsERK7wxPoint"); return 0; }

long ZN12wxWindowBase21ConvertPixelsToDialogERK7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase21ConvertPixelsToDialogERK7wxPoint"); return 0; }

long ZN12wxWindowBase22TransferDataFromWindowEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase22TransferDataFromWindowEv"); return 0; }

long ZN12wxWindowBase25GetClassDefaultAttributesE15wxWindowVariant(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase25GetClassDefaultAttributesE15wxWindowVariant"); return 0; }

long ZN12wxWindowBase3FitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase3FitEv"); return 0; }

long ZN12wxWindowBase5CloseEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase5CloseEb"); return 0; }

long ZN12wxWindowBase6LayoutEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase6LayoutEv"); return 0; }

long ZN12wxWindowBase7DestroyEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase7DestroyEv"); return 0; }

long ZN12wxWindowBase7DoPhaseEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase7DoPhaseEi"); return 0; }

long ZN12wxWindowBase8AddChildEPS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase8AddChildEPS_"); return 0; }

long ZN12wxWindowBase8DoCentreEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase8DoCentreEi"); return 0; }

long ZN12wxWindowBase8NavigateEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase8NavigateEi"); return 0; }

long ZN12wxWindowBase8SetCaretEP7wxCaret(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase8SetCaretEP7wxCaret"); return 0; }

long ZN12wxWindowBase8SetSizerEP7wxSizerb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase8SetSizerEP7wxSizerb"); return 0; }

long ZN12wxWindowBase8ValidateEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase8ValidateEv"); return 0; }

long ZN12wxWindowBase9FindFocusEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase9FindFocusEv"); return 0; }

long ZN12wxWindowBase9FitInsideEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase9FitInsideEv"); return 0; }

long ZN12wxWindowBase9MakeModalEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase9MakeModalEb"); return 0; }

long ZN12wxWindowBase9TryParentER7wxEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWindowBase9TryParentER7wxEvent"); return 0; }

long ZN12wxWizardPage12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWizardPage12ms_classInfoE"); return 0; }

long ZN12wxWizardPage4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWizardPage4InitEv"); return 0; }

long ZN12wxWizardPage6CreateEP8wxWizardRK8wxBitmapPKw(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWizardPage6CreateEP8wxWizardRK8wxBitmapPKw"); return 0; }

long ZN12wxWizardPageC2EP8wxWizardRK8wxBitmapPKw(long a, long b, long c_, long d, long e, long f) { shim_note("ZN12wxWizardPageC2EP8wxWizardRK8wxBitmapPKw"); return 0; }

long ZN13wxArrayString3AddERK8wxStringm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxArrayString3AddERK8wxStringm"); return 0; }

long ZN13wxArrayString4InitEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxArrayString4InitEb"); return 0; }

long ZN13wxArrayString5AllocEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxArrayString5AllocEm"); return 0; }

long ZN13wxArrayString5ClearEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxArrayString5ClearEv"); return 0; }

long ZN13wxArrayStringC1ERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxArrayStringC1ERKS_"); return 0; }

long ZN13wxArrayStringD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxArrayStringD1Ev"); return 0; }

long ZN13wxArrayStringaSERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxArrayStringaSERKS_"); return 0; }

long ZN13wxArtProvider11GetSizeHintERK8wxStringb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxArtProvider11GetSizeHintERK8wxStringb"); return 0; }

long ZN13wxArtProvider3PopEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxArtProvider3PopEv"); return 0; }

long ZN13wxArtProvider4PushEPS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxArtProvider4PushEPS_"); return 0; }

long ZN13wxArtProvider6DeleteEPS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxArtProvider6DeleteEPS_"); return 0; }

long ZN13wxArtProvider6InsertEPS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxArtProvider6InsertEPS_"); return 0; }

long ZN13wxArtProvider7GetIconERK8wxStringS2_RK6wxSize(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxArtProvider7GetIconERK8wxStringS2_RK6wxSize"); return 0; }

long ZN13wxArtProvider9GetBitmapERK8wxStringS2_RK6wxSize(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxArtProvider9GetBitmapERK8wxStringS2_RK6wxSize"); return 0; }

long ZN13wxArtProviderD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxArtProviderD2Ev"); return 0; }

long ZN13wxAuiNotebook10DeletePageEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxAuiNotebook10DeletePageEm"); return 0; }

long ZN13wxAuiNotebook10InsertPageEmP8wxWindowRK8wxStringbRK8wxBitmap(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxAuiNotebook10InsertPageEmP8wxWindowRK8wxStringbRK8wxBitmap"); return 0; }

long ZN13wxAuiNotebook10RemovePageEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxAuiNotebook10RemovePageEm"); return 0; }

long ZN13wxAuiNotebook11SetPageTextEmRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxAuiNotebook11SetPageTextEmRK8wxString"); return 0; }

long ZN13wxAuiNotebook12SetSelectionEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxAuiNotebook12SetSelectionEm"); return 0; }

long ZN13wxAuiNotebook13SetNormalFontERK6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxAuiNotebook13SetNormalFontERK6wxFont"); return 0; }

long ZN13wxAuiNotebook13SetPageBitmapEmRK8wxBitmap(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxAuiNotebook13SetPageBitmapEmRK8wxBitmap"); return 0; }

long ZN13wxAuiNotebook14SetArtProviderEP11wxAuiTabArt(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxAuiNotebook14SetArtProviderEP11wxAuiTabArt"); return 0; }

long ZN13wxAuiNotebook14ShowWindowMenuEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxAuiNotebook14ShowWindowMenuEv"); return 0; }

long ZN13wxAuiNotebook15SetSelectedFontERK6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxAuiNotebook15SetSelectedFontERK6wxFont"); return 0; }

long ZN13wxAuiNotebook16AdvanceSelectionEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxAuiNotebook16AdvanceSelectionEb"); return 0; }

long ZN13wxAuiNotebook16SetMeasuringFontERK6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxAuiNotebook16SetMeasuringFontERK6wxFont"); return 0; }

long ZN13wxAuiNotebook22GetHeightForPageHeightEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxAuiNotebook22GetHeightForPageHeightEi"); return 0; }

long ZN13wxAuiNotebook6CreateEP8wxWindowiRK7wxPointRK6wxSizel(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxAuiNotebook6CreateEP8wxWindowiRK7wxPointRK6wxSizel"); return 0; }

long ZN13wxAuiNotebook7AddPageEP8wxWindowRK8wxStringbRK8wxBitmap(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxAuiNotebook7AddPageEP8wxWindowRK8wxStringbRK8wxBitmap"); return 0; }

long ZN13wxAuiNotebookC1EP8wxWindowiRK7wxPointRK6wxSizel(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxAuiNotebookC1EP8wxWindowiRK7wxPointRK6wxSizel"); return 0; }

long ZN13wxAuiNotebookC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxAuiNotebookC1Ev"); return 0; }

long ZN13wxContextHelp14EndContextHelpEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxContextHelp14EndContextHelpEv"); return 0; }

long ZN13wxContextHelp16BeginContextHelpEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxContextHelp16BeginContextHelpEP8wxWindow"); return 0; }

long ZN13wxContextHelpC1EP8wxWindowb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxContextHelpC1EP8wxWindowb"); return 0; }

long ZN13wxControlBase12GetLabelTextERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxControlBase12GetLabelTextERK8wxString"); return 0; }

long ZN13wxControlBase16DoUpdateWindowUIER15wxUpdateUIEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxControlBase16DoUpdateWindowUIER15wxUpdateUIEvent"); return 0; }

long ZN13wxControlBase7SetFontERK6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxControlBase7SetFontERK6wxFont"); return 0; }

long ZN13wxControlBase8SetLabelERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxControlBase8SetLabelERK8wxString"); return 0; }

long ZN13wxControlBaseD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxControlBaseD2Ev"); return 0; }

long ZN13wxDirItemData13SetNewDirNameERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxDirItemData13SetNewDirNameERK8wxString"); return 0; }

long ZN13wxFileHistoryC1Emi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxFileHistoryC1Emi"); return 0; }

long ZN13wxGBSizerItem10IntersectsERK12wxGBPositionRK8wxGBSpan(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxGBSizerItem10IntersectsERK12wxGBPositionRK8wxGBSpan"); return 0; }

long ZN13wxGBSizerItem10IntersectsERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxGBSizerItem10IntersectsERKS_"); return 0; }

long ZN13wxGBSizerItem6SetPosERK12wxGBPosition(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxGBSizerItem6SetPosERK12wxGBPosition"); return 0; }

long ZN13wxGBSizerItem7SetSpanERK8wxGBSpan(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxGBSizerItem7SetSpanERK8wxGBSpan"); return 0; }

long ZN13wxGBSizerItem9GetEndPosERiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxGBSizerItem9GetEndPosERiS0_"); return 0; }

long ZN13wxGBSizerItemC1EP7wxSizerRK12wxGBPositionRK8wxGBSpaniiP8wxObject(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxGBSizerItemC1EP7wxSizerRK12wxGBPositionRK8wxGBSpaniiP8wxObject"); return 0; }

long ZN13wxGBSizerItemC1EP8wxWindowRK12wxGBPositionRK8wxGBSpaniiP8wxObject(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxGBSizerItemC1EP8wxWindowRK12wxGBPositionRK8wxGBSpaniiP8wxObject"); return 0; }

long ZN13wxGBSizerItemC1EiiRK12wxGBPositionRK8wxGBSpaniiP8wxObject(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxGBSizerItemC1EiiRK12wxGBPositionRK8wxGBSpaniiP8wxObject"); return 0; }

long ZN13wxGBSizerItemC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxGBSizerItemC1Ev"); return 0; }

long ZN13wxHtmlListBox10RefreshAllEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxHtmlListBox10RefreshAllEv"); return 0; }

long ZN13wxHtmlListBox11RefreshLineEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxHtmlListBox11RefreshLineEm"); return 0; }

long ZN13wxHtmlListBox12RefreshLinesEmm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxHtmlListBox12RefreshLinesEmm"); return 0; }

long ZN13wxHtmlListBox12SetItemCountEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxHtmlListBox12SetItemCountEm"); return 0; }

long ZN13wxHtmlListBox12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxHtmlListBox12ms_classInfoE"); return 0; }

long ZN13wxHtmlListBox13GetHTMLWindowEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxHtmlListBox13GetHTMLWindowEv"); return 0; }

long ZN13wxHtmlListBox13OnLinkClickedEmRK14wxHtmlLinkInfo(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxHtmlListBox13OnLinkClickedEmRK14wxHtmlLinkInfo"); return 0; }

long ZN13wxHtmlListBox14OnInternalIdleEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxHtmlListBox14OnInternalIdleEv"); return 0; }

long ZN13wxHtmlListBox17OnHTMLLinkClickedERK14wxHtmlLinkInfo(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxHtmlListBox17OnHTMLLinkClickedERK14wxHtmlLinkInfo"); return 0; }

long ZN13wxHtmlListBox17SetHTMLStatusTextERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxHtmlListBox17SetHTMLStatusTextERK8wxString"); return 0; }

long ZN13wxHtmlListBox18SetHTMLWindowTitleERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxHtmlListBox18SetHTMLWindowTitleERK8wxString"); return 0; }

long ZN13wxHtmlListBox22SetHTMLBackgroundImageERK8wxBitmap(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxHtmlListBox22SetHTMLBackgroundImageERK8wxBitmap"); return 0; }

long ZN13wxHtmlListBox23SetHTMLBackgroundColourERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxHtmlListBox23SetHTMLBackgroundColourERK8wxColour"); return 0; }

long ZN13wxHtmlListBox6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxHtmlListBox6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN13wxHtmlListBoxC2EP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxHtmlListBoxC2EP8wxWindowiRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN13wxHtmlListBoxC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxHtmlListBoxC2Ev"); return 0; }

long ZN13wxHtmlListBoxD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxHtmlListBoxD2Ev"); return 0; }

long ZN13wxInputStream4GetCEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxInputStream4GetCEv"); return 0; }

long ZN13wxInputStream4PeekEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxInputStream4PeekEv"); return 0; }

long ZN13wxInputStream4ReadEPvm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxInputStream4ReadEPvm"); return 0; }

long ZN13wxInputStream5SeekIEx10wxSeekMode(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxInputStream5SeekIEx10wxSeekMode"); return 0; }

long ZN13wxInputStream7UngetchEc(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxInputStream7UngetchEc"); return 0; }

long ZN13wxInputStreamC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxInputStreamC2Ev"); return 0; }

long ZN13wxInputStreamD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxInputStreamD2Ev"); return 0; }

long ZN13wxListBoxBase11DeselectAllEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxListBoxBase11DeselectAllEi"); return 0; }

long ZN13wxListBoxBase11InsertItemsEjPK8wxStringj(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxListBoxBase11InsertItemsEjPK8wxStringj"); return 0; }

long ZN13wxListBoxBase12SetFirstItemERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxListBoxBase12SetFirstItemERK8wxString"); return 0; }

long ZN13wxListBoxBase18SetStringSelectionERK8wxStringb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxListBoxBase18SetStringSelectionERK8wxStringb"); return 0; }

long ZN13wxListBoxBase22AppendAndEnsureVisibleERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxListBoxBase22AppendAndEnsureVisibleERK8wxString"); return 0; }

long ZN13wxListBoxBase7CommandER14wxCommandEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxListBoxBase7CommandER14wxCommandEvent"); return 0; }

long ZN13wxListBoxBaseD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxListBoxBaseD2Ev"); return 0; }

long ZN13wxLogTextCtrlC1EP10wxTextCtrl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxLogTextCtrlC1EP10wxTextCtrl"); return 0; }

long ZN13wxMenuBarBase13SetHelpStringEiRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxMenuBarBase13SetHelpStringEiRK8wxString"); return 0; }

long ZN13wxMenuBarBase5CheckEib(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxMenuBarBase5CheckEib"); return 0; }

long ZN13wxMenuBarBase8SetLabelEiRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxMenuBarBase8SetLabelEiRK8wxString"); return 0; }

long ZN13wxPrintDialogC1EP8wxWindowP17wxPrintDialogData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxPrintDialogC1EP8wxWindowP17wxPrintDialogData"); return 0; }

long ZN13wxPrinterBase10sm_abortItE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxPrinterBase10sm_abortItE"); return 0; }

long ZN13wxPrinterBase12sm_lastErrorE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxPrinterBase12sm_lastErrorE"); return 0; }

long ZN13wxRadioButton6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxRadioButton6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_"); return 0; }

long ZN13wxRichTextBox4CopyERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxRichTextBox4CopyERKS_"); return 0; }

long ZN13wxRichTextBoxC1EP16wxRichTextObject(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxRichTextBoxC1EP16wxRichTextObject"); return 0; }

long ZN13wxScrollEventC1Eiiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxScrollEventC1Eiiii"); return 0; }

long ZN13wxTIFFHandlerC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxTIFFHandlerC1Ev"); return 0; }

long ZN13wxTaskBarIcon10RemoveIconEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxTaskBarIcon10RemoveIconEv"); return 0; }

long ZN13wxTaskBarIcon12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxTaskBarIcon12ms_classInfoE"); return 0; }

long ZN13wxTaskBarIcon7SetIconERK6wxIconRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxTaskBarIcon7SetIconERK6wxIconRK8wxString"); return 0; }

long ZN13wxTaskBarIcon9PopupMenuEP6wxMenu(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxTaskBarIcon9PopupMenuEP6wxMenu"); return 0; }

long ZN13wxTaskBarIconC2ENS_17wxTaskBarIconTypeE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxTaskBarIconC2ENS_17wxTaskBarIconTypeE"); return 0; }

long ZN13wxTaskBarIconD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxTaskBarIconD2Ev"); return 0; }

long ZN13wxToolBarBaseC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxToolBarBaseC2Ev"); return 0; }

long ZN13wxToolBarBaseD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxToolBarBaseD2Ev"); return 0; }

long ZN13wxWizardEventC1EiibP12wxWizardPage(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxWizardEventC1EiibP12wxWizardPage"); return 0; }

long ZN13wxXmlDocumentC1ER13wxInputStreamRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlDocumentC1ER13wxInputStreamRK8wxString"); return 0; }

long ZN13wxXmlDocumentC1ERK8wxStringS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlDocumentC1ERK8wxStringS2_"); return 0; }

long ZN13wxXmlDocumentC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlDocumentC1Ev"); return 0; }

long ZN13wxXmlResource10AddHandlerEP20wxXmlResourceHandler(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlResource10AddHandlerEP20wxXmlResourceHandler"); return 0; }

long ZN13wxXmlResource10LoadBitmapERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlResource10LoadBitmapERK8wxString"); return 0; }

long ZN13wxXmlResource10LoadDialogEP8wxDialogP8wxWindowRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlResource10LoadDialogEP8wxDialogP8wxWindowRK8wxString"); return 0; }

long ZN13wxXmlResource10LoadDialogEP8wxWindowRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlResource10LoadDialogEP8wxWindowRK8wxString"); return 0; }

long ZN13wxXmlResource10LoadObjectEP8wxObjectP8wxWindowRK8wxStringS6_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlResource10LoadObjectEP8wxObjectP8wxWindowRK8wxStringS6_"); return 0; }

long ZN13wxXmlResource10LoadObjectEP8wxWindowRK8wxStringS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlResource10LoadObjectEP8wxWindowRK8wxStringS4_"); return 0; }

long ZN13wxXmlResource11LoadMenuBarEP8wxWindowRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlResource11LoadMenuBarEP8wxWindowRK8wxString"); return 0; }

long ZN13wxXmlResource11LoadToolBarEP8wxWindowRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlResource11LoadToolBarEP8wxWindowRK8wxString"); return 0; }

long ZN13wxXmlResource13ClearHandlersEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlResource13ClearHandlersEv"); return 0; }

long ZN13wxXmlResource13InsertHandlerEP20wxXmlResourceHandler(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlResource13InsertHandlerEP20wxXmlResourceHandler"); return 0; }

long ZN13wxXmlResource15InitAllHandlersEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlResource15InitAllHandlersEv"); return 0; }

long ZN13wxXmlResource17CreateResFromNodeEP9wxXmlNodeP8wxObjectS3_P20wxXmlResourceHandler(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlResource17CreateResFromNodeEP9wxXmlNodeP8wxObjectS3_P20wxXmlResourceHandler"); return 0; }

long ZN13wxXmlResource18AddSubclassFactoryEP20wxXmlSubclassFactory(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlResource18AddSubclassFactoryEP20wxXmlSubclassFactory"); return 0; }

long ZN13wxXmlResource20AttachUnknownControlERK8wxStringP8wxWindowS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlResource20AttachUnknownControlERK8wxStringP8wxWindowS4_"); return 0; }

long ZN13wxXmlResource3GetEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlResource3GetEv"); return 0; }

long ZN13wxXmlResource3SetEPS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlResource3SetEPS_"); return 0; }

long ZN13wxXmlResource4LoadERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlResource4LoadERK8wxString"); return 0; }

long ZN13wxXmlResource6UnloadERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlResource6UnloadERK8wxString"); return 0; }

long ZN13wxXmlResource8GetXRCIDEPKwi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlResource8GetXRCIDEPKwi"); return 0; }

long ZN13wxXmlResource8LoadIconERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlResource8LoadIconERK8wxString"); return 0; }

long ZN13wxXmlResource8LoadMenuERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlResource8LoadMenuERK8wxString"); return 0; }

long ZN13wxXmlResource9LoadFrameEP7wxFrameP8wxWindowRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlResource9LoadFrameEP7wxFrameP8wxWindowRK8wxString"); return 0; }

long ZN13wxXmlResource9LoadFrameEP8wxWindowRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlResource9LoadFrameEP8wxWindowRK8wxString"); return 0; }

long ZN13wxXmlResource9LoadPanelEP7wxPanelP8wxWindowRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlResource9LoadPanelEP7wxPanelP8wxWindowRK8wxString"); return 0; }

long ZN13wxXmlResource9LoadPanelEP8wxWindowRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlResource9LoadPanelEP8wxWindowRK8wxString"); return 0; }

long ZN13wxXmlResource9SetDomainEPKw(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlResource9SetDomainEPKw"); return 0; }

long ZN13wxXmlResourceC1ERK8wxStringiS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlResourceC1ERK8wxStringiS2_"); return 0; }

long ZN13wxXmlResourceC1EiRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN13wxXmlResourceC1EiRK8wxString"); return 0; }

long ZN14wxBaseArrayInt3AddEim(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxBaseArrayInt3AddEim"); return 0; }

long ZN14wxBaseArrayIntC2ERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxBaseArrayIntC2ERKS_"); return 0; }

long ZN14wxBaseArrayIntC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxBaseArrayIntC2Ev"); return 0; }

long ZN14wxBaseArrayIntD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxBaseArrayIntD2Ev"); return 0; }

long ZN14wxBaseArrayIntaSERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxBaseArrayIntaSERKS_"); return 0; }

long ZN14wxBitmapButton6CreateEP8wxWindowiRK8wxBitmapRK7wxPointRK6wxSizelRK11wxValidatorRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxBitmapButton6CreateEP8wxWindowiRK8wxBitmapRK7wxPointRK6wxSizelRK11wxValidatorRK8wxString"); return 0; }

long ZN14wxBookCtrlBase15AssignImageListEP11wxImageList(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxBookCtrlBase15AssignImageListEP11wxImageList"); return 0; }

long ZN14wxBookCtrlBase4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxBookCtrlBase4InitEv"); return 0; }

long ZN14wxBookCtrlBaseD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxBookCtrlBaseD2Ev"); return 0; }

long ZN14wxCalendarCtrl10SetHolidayEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxCalendarCtrl10SetHolidayEm"); return 0; }

long ZN14wxCalendarCtrl12SetDateRangeERK10wxDateTimeS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxCalendarCtrl12SetDateRangeERK10wxDateTimeS2_"); return 0; }

long ZN14wxCalendarCtrl16EnableYearChangeEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxCalendarCtrl16EnableYearChangeEb"); return 0; }

long ZN14wxCalendarCtrl17EnableMonthChangeEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxCalendarCtrl17EnableMonthChangeEb"); return 0; }

long ZN14wxCalendarCtrl17SetLowerDateLimitERK10wxDateTime(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxCalendarCtrl17SetLowerDateLimitERK10wxDateTime"); return 0; }

long ZN14wxCalendarCtrl17SetUpperDateLimitERK10wxDateTime(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxCalendarCtrl17SetUpperDateLimitERK10wxDateTime"); return 0; }

long ZN14wxCalendarCtrl20EnableHolidayDisplayEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxCalendarCtrl20EnableHolidayDisplayEb"); return 0; }

long ZN14wxCalendarCtrl25GetClassDefaultAttributesE15wxWindowVariant(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxCalendarCtrl25GetClassDefaultAttributesE15wxWindowVariant"); return 0; }

long ZN14wxCalendarCtrl4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxCalendarCtrl4InitEv"); return 0; }

long ZN14wxCalendarCtrl6CreateEP8wxWindowiRK10wxDateTimeRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxCalendarCtrl6CreateEP8wxWindowiRK10wxDateTimeRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN14wxCalendarCtrl7HitTestERK7wxPointP10wxDateTimePNS3_7WeekDayE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxCalendarCtrl7HitTestERK7wxPointP10wxDateTimePNS3_7WeekDayE"); return 0; }

long ZN14wxCalendarCtrl7SetDateERK10wxDateTime(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxCalendarCtrl7SetDateERK10wxDateTime"); return 0; }

long ZN14wxCalendarCtrlC1EP8wxWindowiRK10wxDateTimeRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxCalendarCtrlC1EP8wxWindowiRK10wxDateTimeRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN14wxCheckListBox4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxCheckListBox4InitEv"); return 0; }

long ZN14wxCheckListBox6CreateEP8wxWindowiRK7wxPointRK6wxSizeRK13wxArrayStringlRK11wxValidatorRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxCheckListBox6CreateEP8wxWindowiRK7wxPointRK6wxSizeRK13wxArrayStringlRK11wxValidatorRK8wxString"); return 0; }

long ZN14wxColourDialogC1EP8wxWindowP12wxColourData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxColourDialogC1EP8wxWindowP12wxColourData"); return 0; }

long ZN14wxCommandEvent12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxCommandEvent12ms_classInfoE"); return 0; }

long ZN14wxCommandEventC1Eii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxCommandEventC1Eii"); return 0; }

long ZN14wxCommandEventC2Eii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxCommandEventC2Eii"); return 0; }

long ZN14wxFileTypeInfoC1EPKwS1_S1_S1_z(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxFileTypeInfoC1EPKwS1_S1_S1_z"); return 0; }

long ZN14wxFileTypeInfoC1ERK13wxArrayString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxFileTypeInfoC1ERK13wxArrayString"); return 0; }

long ZN14wxGraphicsPath11MoveToPointERK15wxPoint2DDouble(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGraphicsPath11MoveToPointERK15wxPoint2DDouble"); return 0; }

long ZN14wxGraphicsPath14AddLineToPointERK15wxPoint2DDouble(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGraphicsPath14AddLineToPointERK15wxPoint2DDouble"); return 0; }

long ZN14wxGraphicsPath15AddCurveToPointERK15wxPoint2DDoubleS2_S2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGraphicsPath15AddCurveToPointERK15wxPoint2DDoubleS2_S2_"); return 0; }

long ZN14wxGraphicsPath6AddArcERK15wxPoint2DDoubledddb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGraphicsPath6AddArcERK15wxPoint2DDoubledddb"); return 0; }

long ZN14wxGridBagSizer11GetItemSpanEP7wxSizer(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGridBagSizer11GetItemSpanEP7wxSizer"); return 0; }

long ZN14wxGridBagSizer11GetItemSpanEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGridBagSizer11GetItemSpanEP8wxWindow"); return 0; }

long ZN14wxGridBagSizer11GetItemSpanEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGridBagSizer11GetItemSpanEm"); return 0; }

long ZN14wxGridBagSizer11SetItemSpanEP7wxSizerRK8wxGBSpan(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGridBagSizer11SetItemSpanEP7wxSizerRK8wxGBSpan"); return 0; }

long ZN14wxGridBagSizer11SetItemSpanEP8wxWindowRK8wxGBSpan(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGridBagSizer11SetItemSpanEP8wxWindowRK8wxGBSpan"); return 0; }

long ZN14wxGridBagSizer11SetItemSpanEmRK8wxGBSpan(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGridBagSizer11SetItemSpanEmRK8wxGBSpan"); return 0; }

long ZN14wxGridBagSizer15FindItemAtPointERK7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGridBagSizer15FindItemAtPointERK7wxPoint"); return 0; }

long ZN14wxGridBagSizer15GetItemPositionEP7wxSizer(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGridBagSizer15GetItemPositionEP7wxSizer"); return 0; }

long ZN14wxGridBagSizer15GetItemPositionEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGridBagSizer15GetItemPositionEP8wxWindow"); return 0; }

long ZN14wxGridBagSizer15GetItemPositionEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGridBagSizer15GetItemPositionEm"); return 0; }

long ZN14wxGridBagSizer15SetItemPositionEP7wxSizerRK12wxGBPosition(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGridBagSizer15SetItemPositionEP7wxSizerRK12wxGBPosition"); return 0; }

long ZN14wxGridBagSizer15SetItemPositionEP8wxWindowRK12wxGBPosition(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGridBagSizer15SetItemPositionEP8wxWindowRK12wxGBPosition"); return 0; }

long ZN14wxGridBagSizer15SetItemPositionEmRK12wxGBPosition(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGridBagSizer15SetItemPositionEmRK12wxGBPosition"); return 0; }

long ZN14wxGridBagSizer18FindItemAtPositionERK12wxGBPosition(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGridBagSizer18FindItemAtPositionERK12wxGBPosition"); return 0; }

long ZN14wxGridBagSizer20CheckForIntersectionEP13wxGBSizerItemS1_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGridBagSizer20CheckForIntersectionEP13wxGBSizerItemS1_"); return 0; }

long ZN14wxGridBagSizer20CheckForIntersectionERK12wxGBPositionRK8wxGBSpanP13wxGBSizerItem(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGridBagSizer20CheckForIntersectionERK12wxGBPositionRK8wxGBSpanP13wxGBSizerItem"); return 0; }

long ZN14wxGridBagSizer3AddEP13wxGBSizerItem(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGridBagSizer3AddEP13wxGBSizerItem"); return 0; }

long ZN14wxGridBagSizer3AddEP7wxSizerRK12wxGBPositionRK8wxGBSpaniiP8wxObject(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGridBagSizer3AddEP7wxSizerRK12wxGBPositionRK8wxGBSpaniiP8wxObject"); return 0; }

long ZN14wxGridBagSizer3AddEP8wxWindowRK12wxGBPositionRK8wxGBSpaniiP8wxObject(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGridBagSizer3AddEP8wxWindowRK12wxGBPositionRK8wxGBSpaniiP8wxObject"); return 0; }

long ZN14wxGridBagSizer3AddEiiRK12wxGBPositionRK8wxGBSpaniiP8wxObject(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGridBagSizer3AddEiiRK12wxGBPositionRK8wxGBSpaniiP8wxObject"); return 0; }

long ZN14wxGridBagSizer8FindItemEP7wxSizer(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGridBagSizer8FindItemEP7wxSizer"); return 0; }

long ZN14wxGridBagSizer8FindItemEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGridBagSizer8FindItemEP8wxWindow"); return 0; }

long ZN14wxGridBagSizerC1Eii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGridBagSizerC1Eii"); return 0; }

long ZN14wxGridCellAttr4InitEPS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGridCellAttr4InitEPS_"); return 0; }

long ZN14wxGridCellAttr7SetSizeEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGridCellAttr7SetSizeEii"); return 0; }

long ZN14wxGridCellAttr9MergeWithEPS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxGridCellAttr9MergeWithEPS_"); return 0; }

long ZN14wxHelpProvider15ms_helpProviderE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxHelpProvider15ms_helpProviderE"); return 0; }

long ZN14wxHelpProviderD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxHelpProviderD2Ev"); return 0; }

long ZN14wxHtmlHelpData10SetTempDirERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxHtmlHelpData10SetTempDirERK8wxString"); return 0; }

long ZN14wxHtmlHelpData12FindPageByIdEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxHtmlHelpData12FindPageByIdEi"); return 0; }

long ZN14wxHtmlHelpData14FindPageByNameERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxHtmlHelpData14FindPageByNameERK8wxString"); return 0; }

long ZN14wxHtmlHelpData7AddBookERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxHtmlHelpData7AddBookERK8wxString"); return 0; }

long ZN14wxHtmlHelpDataC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxHtmlHelpDataC1Ev"); return 0; }

long ZN14wxHtmlPrintout10SetMarginsEfffff(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxHtmlPrintout10SetMarginsEfffff"); return 0; }

long ZN14wxHtmlPrintout11SetHtmlFileERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxHtmlPrintout11SetHtmlFileERK8wxString"); return 0; }

long ZN14wxHtmlPrintout11SetHtmlTextERK8wxStringS2_b(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxHtmlPrintout11SetHtmlTextERK8wxStringS2_b"); return 0; }

long ZN14wxHtmlPrintout14CleanUpStaticsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxHtmlPrintout14CleanUpStaticsEv"); return 0; }

long ZN14wxHtmlPrintout16SetStandardFontsEiRK8wxStringS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxHtmlPrintout16SetStandardFontsEiRK8wxStringS2_"); return 0; }

long ZN14wxHtmlPrintout8SetFontsERK8wxStringS2_PKi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxHtmlPrintout8SetFontsERK8wxStringS2_PKi"); return 0; }

long ZN14wxHtmlPrintout9AddFilterEP12wxHtmlFilter(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxHtmlPrintout9AddFilterEP12wxHtmlFilter"); return 0; }

long ZN14wxHtmlPrintout9SetFooterERK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxHtmlPrintout9SetFooterERK8wxStringi"); return 0; }

long ZN14wxHtmlPrintout9SetHeaderERK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxHtmlPrintout9SetHeaderERK8wxStringi"); return 0; }

long ZN14wxHtmlPrintoutC1ERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxHtmlPrintoutC1ERK8wxString"); return 0; }

long ZN14wxHtmlWordCell15SetPreviousWordEPS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxHtmlWordCell15SetPreviousWordEPS_"); return 0; }

long ZN14wxHtmlWordCellC1ERK8wxStringRK4wxDC(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxHtmlWordCellC1ERK8wxStringRK4wxDC"); return 0; }

long ZN14wxImageHandler13CallDoCanReadER13wxInputStream(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxImageHandler13CallDoCanReadER13wxInputStream"); return 0; }

long ZN14wxImageHandler7CanReadERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxImageHandler7CanReadERK8wxString"); return 0; }

long ZN14wxMenuItemBase12GetLabelTextERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxMenuItemBase12GetLabelTextERK8wxString"); return 0; }

long ZN14wxMenuItemBase16GetLabelFromTextERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxMenuItemBase16GetLabelFromTextERK8wxString"); return 0; }

long ZN14wxMenuItemBase3NewEP6wxMenuiRK8wxStringS4_10wxItemKindS1_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxMenuItemBase3NewEP6wxMenuiRK8wxStringS4_10wxItemKindS1_"); return 0; }

long ZN14wxMenuItemBase7SetHelpERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxMenuItemBase7SetHelpERK8wxString"); return 0; }

long ZN14wxNotebookBase20SendPageChangedEventEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxNotebookBase20SendPageChangedEventEii"); return 0; }

long ZN14wxNotebookBase21SendPageChangingEventEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxNotebookBase21SendPageChangingEventEi"); return 0; }

long ZN14wxOutputStream4PutCEc(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxOutputStream4PutCEc"); return 0; }

long ZN14wxOutputStream4SyncEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxOutputStream4SyncEv"); return 0; }

long ZN14wxOutputStream5SeekOEx10wxSeekMode(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxOutputStream5SeekOEx10wxSeekMode"); return 0; }

long ZN14wxOutputStream5WriteEPKvm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxOutputStream5WriteEPKvm"); return 0; }

long ZN14wxOutputStreamC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxOutputStreamC2Ev"); return 0; }

long ZN14wxOutputStreamD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxOutputStreamD2Ev"); return 0; }

long ZN14wxPlatformInfo11GetArchNameE14wxArchitecture(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPlatformInfo11GetArchNameE14wxArchitecture"); return 0; }

long ZN14wxPlatformInfo13GetPortIdNameE8wxPortIdb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPlatformInfo13GetPortIdNameE8wxPortIdb"); return 0; }

long ZN14wxPlatformInfo17GetEndiannessNameE12wxEndianness(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPlatformInfo17GetEndiannessNameE12wxEndianness"); return 0; }

long ZN14wxPlatformInfo18GetPortIdShortNameE8wxPortIdb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPlatformInfo18GetPortIdShortNameE8wxPortIdb"); return 0; }

long ZN14wxPlatformInfo24GetOperatingSystemIdNameE19wxOperatingSystemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPlatformInfo24GetOperatingSystemIdNameE19wxOperatingSystemId"); return 0; }

long ZN14wxPlatformInfo28GetOperatingSystemFamilyNameE19wxOperatingSystemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPlatformInfo28GetOperatingSystemFamilyNameE19wxOperatingSystemId"); return 0; }

long ZN14wxPlatformInfoC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPlatformInfoC1Ev"); return 0; }

long ZN14wxPostScriptDC13GetResolutionEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPostScriptDC13GetResolutionEv"); return 0; }

long ZN14wxPostScriptDC13SetResolutionEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPostScriptDC13SetResolutionEi"); return 0; }

long ZN14wxPostScriptDCC1ERK11wxPrintData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPostScriptDCC1ERK11wxPrintData"); return 0; }

long ZN14wxPreviewFrame10InitializeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPreviewFrame10InitializeEv"); return 0; }

long ZN14wxPreviewFrame12CreateCanvasEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPreviewFrame12CreateCanvasEv"); return 0; }

long ZN14wxPreviewFrame12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPreviewFrame12ms_classInfoE"); return 0; }

long ZN14wxPreviewFrame16CreateControlBarEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPreviewFrame16CreateControlBarEv"); return 0; }

long ZN14wxPreviewFrameC1EP18wxPrintPreviewBaseP8wxWindowRK8wxStringRK7wxPointRK6wxSizelS6_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPreviewFrameC1EP18wxPrintPreviewBaseP8wxWindowRK8wxStringRK7wxPointRK6wxSizelS6_"); return 0; }

long ZN14wxPreviewFrameC2EP18wxPrintPreviewBaseP8wxWindowRK8wxStringRK7wxPointRK6wxSizelS6_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPreviewFrameC2EP18wxPrintPreviewBaseP8wxWindowRK8wxStringRK7wxPointRK6wxSizelS6_"); return 0; }

long ZN14wxPreviewFrameD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPreviewFrameD2Ev"); return 0; }

long ZN14wxPrintPreview10RenderPageEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPrintPreview10RenderPageEi"); return 0; }

long ZN14wxPrintPreview11SetPrintoutEP10wxPrintout(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPrintPreview11SetPrintoutEP10wxPrintout"); return 0; }

long ZN14wxPrintPreview13DrawBlankPageEP15wxPreviewCanvasR4wxDC(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPrintPreview13DrawBlankPageEP15wxPreviewCanvasR4wxDC"); return 0; }

long ZN14wxPrintPreview14SetCurrentPageEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPrintPreview14SetCurrentPageEi"); return 0; }

long ZN14wxPrintPreview16AdjustScrollbarsEP15wxPreviewCanvas(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPrintPreview16AdjustScrollbarsEP15wxPreviewCanvas"); return 0; }

long ZN14wxPrintPreview16DetermineScalingEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPrintPreview16DetermineScalingEv"); return 0; }

long ZN14wxPrintPreview18GetPrintDialogDataEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPrintPreview18GetPrintDialogDataEv"); return 0; }

long ZN14wxPrintPreview5PrintEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPrintPreview5PrintEb"); return 0; }

long ZN14wxPrintPreview5SetOkEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPrintPreview5SetOkEb"); return 0; }

long ZN14wxPrintPreview7SetZoomEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPrintPreview7SetZoomEi"); return 0; }

long ZN14wxPrintPreview8SetFrameEP7wxFrame(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPrintPreview8SetFrameEP7wxFrame"); return 0; }

long ZN14wxPrintPreview9PaintPageEP15wxPreviewCanvasR4wxDC(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPrintPreview9PaintPageEP15wxPreviewCanvasR4wxDC"); return 0; }

long ZN14wxPrintPreview9SetCanvasEP15wxPreviewCanvas(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPrintPreview9SetCanvasEP15wxPreviewCanvas"); return 0; }

long ZN14wxPrintPreviewC1EP10wxPrintoutS1_P11wxPrintData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPrintPreviewC1EP10wxPrintoutS1_P11wxPrintData"); return 0; }

long ZN14wxPrintPreviewC1EP10wxPrintoutS1_P17wxPrintDialogData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPrintPreviewC1EP10wxPrintoutS1_P17wxPrintDialogData"); return 0; }

long ZN14wxPrintPreviewC2EP10wxPrintoutS1_P11wxPrintData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPrintPreviewC2EP10wxPrintoutS1_P11wxPrintData"); return 0; }

long ZN14wxPrintPreviewC2EP10wxPrintoutS1_P17wxPrintDialogData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPrintPreviewC2EP10wxPrintoutS1_P17wxPrintDialogData"); return 0; }

long ZN14wxPrintPreviewD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxPrintPreviewD2Ev"); return 0; }

long ZN14wxRadioBoxBase14SetItemToolTipEjRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxRadioBoxBase14SetItemToolTipEjRK8wxString"); return 0; }

long ZN14wxRadioBoxBase15SetItemHelpTextEjRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxRadioBoxBase15SetItemHelpTextEjRK8wxString"); return 0; }

long ZN14wxRadioBoxBaseD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxRadioBoxBaseD2Ev"); return 0; }

long ZN14wxRect2DDouble11ConstrainToERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxRect2DDouble11ConstrainToERKS_"); return 0; }

long ZN14wxRect2DDouble5UnionERKS_S1_PS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxRect2DDouble5UnionERKS_S1_PS_"); return 0; }

long ZN14wxRect2DDouble9IntersectERKS_S1_PS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxRect2DDouble9IntersectERKS_S1_PS_"); return 0; }

long ZN14wxRect2DDoubleaSERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxRect2DDoubleaSERKS_"); return 0; }

long ZN14wxRichTextAttr4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxRichTextAttr4InitEv"); return 0; }

long ZN14wxRichTextAttrC1ERK12wxTextAttrEx(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxRichTextAttrC1ERK12wxTextAttrEx"); return 0; }

long ZN14wxRichTextCtrl15ApplyStyleSheetEP20wxRichTextStyleSheet(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxRichTextCtrl15ApplyStyleSheetEP20wxRichTextStyleSheet"); return 0; }

long ZN14wxRichTextCtrl17SetSelectionRangeERK15wxRichTextRange(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxRichTextCtrl17SetSelectionRangeERK15wxRichTextRange"); return 0; }

long ZN14wxRichTextCtrl28SetDefaultStyleToCursorStyleEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxRichTextCtrl28SetDefaultStyleToCursorStyleEv"); return 0; }

long ZN14wxRichTextCtrl6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxRichTextCtrl6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_"); return 0; }

long ZN14wxRichTextCtrlC1EP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxRichTextCtrlC1EP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_"); return 0; }

long ZN14wxRichTextCtrlC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxRichTextCtrlC1Ev"); return 0; }

long ZN14wxRichTextLine4CopyERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxRichTextLine4CopyERKS_"); return 0; }

long ZN14wxRichTextLine4InitEP19wxRichTextParagraph(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxRichTextLine4InitEP19wxRichTextParagraph"); return 0; }

long ZN14wxRichTextLineC1EP19wxRichTextParagraph(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxRichTextLineC1EP19wxRichTextParagraph"); return 0; }

long ZN14wxScrollHelper11DoPrepareDCER4wxDC(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxScrollHelper11DoPrepareDCER4wxDC"); return 0; }

long ZN14wxScrollHelper12ScrollLayoutEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxScrollHelper12ScrollLayoutEv"); return 0; }

long ZN14wxScrollHelper13CalcScrollIncER16wxScrollWinEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxScrollHelper13CalcScrollIncER16wxScrollWinEvent"); return 0; }

long ZN14wxScrollHelper13SetScrollRateEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxScrollHelper13SetScrollRateEii"); return 0; }

long ZN14wxScrollHelper13SetScrollbarsEiiiiiib(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxScrollHelper13SetScrollbarsEiiiiiib"); return 0; }

long ZN14wxScrollHelper14HandleOnScrollER16wxScrollWinEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxScrollHelper14HandleOnScrollER16wxScrollWinEvent"); return 0; }

long ZN14wxScrollHelper15EnableScrollingEbb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxScrollHelper15EnableScrollingEbb"); return 0; }

long ZN14wxScrollHelper15SetTargetWindowEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxScrollHelper15SetTargetWindowEP8wxWindow"); return 0; }

long ZN14wxScrollHelper16AdjustScrollbarsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxScrollHelper16AdjustScrollbarsEv"); return 0; }

long ZN14wxScrollHelper17SetScrollPageSizeEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxScrollHelper17SetScrollPageSizeEii"); return 0; }

long ZN14wxScrollHelper22ScrollDoSetVirtualSizeEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxScrollHelper22ScrollDoSetVirtualSizeEii"); return 0; }

long ZN14wxScrollHelper6ScrollEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxScrollHelper6ScrollEii"); return 0; }

long ZN14wxScrollHelperC2EP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxScrollHelperC2EP8wxWindow"); return 0; }

long ZN14wxScrollHelperD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxScrollHelperD2Ev"); return 0; }

long ZN14wxSplashScreenC1ERK8wxBitmapliP8wxWindowiRK7wxPointRK6wxSizel(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxSplashScreenC1ERK8wxBitmapliP8wxWindowiRK7wxPointRK6wxSizel"); return 0; }

long ZN14wxStaticBitmap6CreateEP8wxWindowiRK8wxBitmapRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxStaticBitmap6CreateEP8wxWindowiRK8wxBitmapRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN14wxTextCtrlBase10DoLoadFileERK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxTextCtrlBase10DoLoadFileERK8wxStringi"); return 0; }

long ZN14wxTextCtrlBase10DoSaveFileERK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxTextCtrlBase10DoSaveFileERK8wxStringi"); return 0; }

long ZN14wxTextCtrlBase15EmulateKeyPressERK10wxKeyEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxTextCtrlBase15EmulateKeyPressERK10wxKeyEvent"); return 0; }

long ZN14wxTextCtrlBase16DoUpdateWindowUIER15wxUpdateUIEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxTextCtrlBase16DoUpdateWindowUIER15wxUpdateUIEvent"); return 0; }

long ZN14wxTextCtrlBase20SendTextUpdatedEventEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxTextCtrlBase20SendTextUpdatedEventEv"); return 0; }

long ZN14wxTextCtrlBase8GetStyleElR10wxTextAttr(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxTextCtrlBase8GetStyleElR10wxTextAttr"); return 0; }

long ZN14wxTextCtrlBase8SaveFileERK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxTextCtrlBase8SaveFileERK8wxStringi"); return 0; }

long ZN14wxTextCtrlBase9SelectAllEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxTextCtrlBase9SelectAllEv"); return 0; }

long ZN14wxToggleButton6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxToggleButton6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_"); return 0; }

long ZN14wxTreeCtrlBase11CollapseAllEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxTreeCtrlBase11CollapseAllEv"); return 0; }

long ZN14wxTreeCtrlBase17ExpandAllChildrenERK12wxTreeItemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxTreeCtrlBase17ExpandAllChildrenERK12wxTreeItemId"); return 0; }

long ZN14wxTreeCtrlBase19CollapseAllChildrenERK12wxTreeItemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxTreeCtrlBase19CollapseAllChildrenERK12wxTreeItemId"); return 0; }

long ZN14wxTreeCtrlBase9ExpandAllEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxTreeCtrlBase9ExpandAllEv"); return 0; }

long ZN14wxTreeCtrlBaseD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN14wxTreeCtrlBaseD2Ev"); return 0; }

long ZN15wxAnimationCtrl16DrawCurrentFrameER4wxDC(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxAnimationCtrl16DrawCurrentFrameER4wxDC"); return 0; }

long ZN15wxAnimationCtrl4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxAnimationCtrl4InitEv"); return 0; }

long ZN15wxAnimationCtrl6CreateEP8wxWindowiRK11wxAnimationRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxAnimationCtrl6CreateEP8wxWindowiRK11wxAnimationRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN15wxClipboardBase3GetEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxClipboardBase3GetEv"); return 0; }

long ZN15wxComboCtrlBase11AnimateShowERK6wxRecti(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase11AnimateShowERK6wxRecti"); return 0; }

long ZN15wxComboCtrlBase11DoShowPopupERK6wxRecti(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase11DoShowPopupERK6wxRecti"); return 0; }

long ZN15wxComboCtrlBase12DoSetToolTipEP9wxToolTip(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase12DoSetToolTipEP9wxToolTip"); return 0; }

long ZN15wxComboCtrlBase12GetValidatorEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase12GetValidatorEv"); return 0; }

long ZN15wxComboCtrlBase12SetSelectionEll(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase12SetSelectionEll"); return 0; }

long ZN15wxComboCtrlBase12SetValidatorERK11wxValidator(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase12SetValidatorERK11wxValidator"); return 0; }

long ZN15wxComboCtrlBase13GetButtonSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase13GetButtonSizeEv"); return 0; }

long ZN15wxComboCtrlBase13OnButtonClickEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase13OnButtonClickEv"); return 0; }

long ZN15wxComboCtrlBase13OnThemeChangeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase13OnThemeChangeEv"); return 0; }

long ZN15wxComboCtrlBase13SetTextIndentEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase13SetTextIndentEi"); return 0; }

long ZN15wxComboCtrlBase14OnPopupDismissEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase14OnPopupDismissEv"); return 0; }

long ZN15wxComboCtrlBase16SetButtonBitmapsERK8wxBitmapbS2_S2_S2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase16SetButtonBitmapsERK8wxBitmapbS2_S2_S2_"); return 0; }

long ZN15wxComboCtrlBase17DoSetPopupControlEP12wxComboPopup(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase17DoSetPopupControlEP12wxComboPopup"); return 0; }

long ZN15wxComboCtrlBase17SetButtonPositionEiiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase17SetButtonPositionEiiii"); return 0; }

long ZN15wxComboCtrlBase17SetInsertionPointEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase17SetInsertionPointEl"); return 0; }

long ZN15wxComboCtrlBase17SetValueWithEventERK8wxStringb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase17SetValueWithEventERK8wxStringb"); return 0; }

long ZN15wxComboCtrlBase18EnsurePopupControlEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase18EnsurePopupControlEv"); return 0; }

long ZN15wxComboCtrlBase20SetInsertionPointEndEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase20SetInsertionPointEndEv"); return 0; }

long ZN15wxComboCtrlBase3CutEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase3CutEv"); return 0; }

long ZN15wxComboCtrlBase4CopyEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase4CopyEv"); return 0; }

long ZN15wxComboCtrlBase4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase4InitEv"); return 0; }

long ZN15wxComboCtrlBase4ShowEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase4ShowEb"); return 0; }

long ZN15wxComboCtrlBase4UndoEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase4UndoEv"); return 0; }

long ZN15wxComboCtrlBase5PasteEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase5PasteEv"); return 0; }

long ZN15wxComboCtrlBase6EnableEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase6EnableEb"); return 0; }

long ZN15wxComboCtrlBase6RemoveEll(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase6RemoveEll"); return 0; }

long ZN15wxComboCtrlBase7ReplaceEllRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase7ReplaceEllRK8wxString"); return 0; }

long ZN15wxComboCtrlBase7SetFontERK6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase7SetFontERK6wxFont"); return 0; }

long ZN15wxComboCtrlBase7SetTextERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase7SetTextERK8wxString"); return 0; }

long ZN15wxComboCtrlBase8SetValueERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase8SetValueERK8wxString"); return 0; }

long ZN15wxComboCtrlBase9HidePopupEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase9HidePopupEv"); return 0; }

long ZN15wxComboCtrlBase9ShowPopupEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBase9ShowPopupEv"); return 0; }

long ZN15wxComboCtrlBaseD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxComboCtrlBaseD2Ev"); return 0; }

long ZN15wxEventLoopBase13ms_activeLoopE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxEventLoopBase13ms_activeLoopE"); return 0; }

long ZN15wxFlexGridSizer14AddGrowableColEmi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxFlexGridSizer14AddGrowableColEmi"); return 0; }

long ZN15wxFlexGridSizer14AddGrowableRowEmi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxFlexGridSizer14AddGrowableRowEmi"); return 0; }

long ZN15wxFlexGridSizer17RemoveGrowableColEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxFlexGridSizer17RemoveGrowableColEm"); return 0; }

long ZN15wxFlexGridSizer17RemoveGrowableRowEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxFlexGridSizer17RemoveGrowableRowEm"); return 0; }

long ZN15wxFlexGridSizerC1Eiiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxFlexGridSizerC1Eiiii"); return 0; }

long ZN15wxGridSizeEventC1EiiP8wxObjectiiibbbb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxGridSizeEventC1EiiP8wxObjectiiibbbb"); return 0; }

long ZN15wxGridTableBase10AppendColsEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxGridTableBase10AppendColsEm"); return 0; }

long ZN15wxGridTableBase10AppendRowsEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxGridTableBase10AppendRowsEm"); return 0; }

long ZN15wxGridTableBase10DeleteColsEmm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxGridTableBase10DeleteColsEmm"); return 0; }

long ZN15wxGridTableBase10DeleteRowsEmm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxGridTableBase10DeleteRowsEmm"); return 0; }

long ZN15wxGridTableBase10InsertColsEmm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxGridTableBase10InsertColsEmm"); return 0; }

long ZN15wxGridTableBase10InsertRowsEmm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxGridTableBase10InsertRowsEmm"); return 0; }

long ZN15wxGridTableBase10SetColAttrEP14wxGridCellAttri(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxGridTableBase10SetColAttrEP14wxGridCellAttri"); return 0; }

long ZN15wxGridTableBase10SetRowAttrEP14wxGridCellAttri(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxGridTableBase10SetRowAttrEP14wxGridCellAttri"); return 0; }

long ZN15wxGridTableBase11GetTypeNameEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxGridTableBase11GetTypeNameEii"); return 0; }

long ZN15wxGridTableBase13CanGetValueAsEiiRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxGridTableBase13CanGetValueAsEiiRK8wxString"); return 0; }

long ZN15wxGridTableBase13CanSetValueAsEiiRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxGridTableBase13CanSetValueAsEiiRK8wxString"); return 0; }

long ZN15wxGridTableBase15SetAttrProviderEP22wxGridCellAttrProvider(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxGridTableBase15SetAttrProviderEP22wxGridCellAttrProvider"); return 0; }

long ZN15wxGridTableBase16GetColLabelValueEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxGridTableBase16GetColLabelValueEi"); return 0; }

long ZN15wxGridTableBase16GetRowLabelValueEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxGridTableBase16GetRowLabelValueEi"); return 0; }

long ZN15wxGridTableBase16GetValueAsCustomEiiRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxGridTableBase16GetValueAsCustomEiiRK8wxString"); return 0; }

long ZN15wxGridTableBase16SetValueAsCustomEiiRK8wxStringPv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxGridTableBase16SetValueAsCustomEiiRK8wxStringPv"); return 0; }

long ZN15wxGridTableBase17CanHaveAttributesEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxGridTableBase17CanHaveAttributesEv"); return 0; }

long ZN15wxGridTableBase7GetAttrEiiN14wxGridCellAttr10wxAttrKindE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxGridTableBase7GetAttrEiiN14wxGridCellAttr10wxAttrKindE"); return 0; }

long ZN15wxGridTableBase7SetAttrEP14wxGridCellAttrii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxGridTableBase7SetAttrEP14wxGridCellAttrii"); return 0; }

long ZN15wxGridTableBaseC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxGridTableBaseC2Ev"); return 0; }

long ZN15wxGridTableBaseD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxGridTableBaseD2Ev"); return 0; }

long ZN15wxHtmlHelpFrame14SetTitleFormatERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxHtmlHelpFrame14SetTitleFormatERK8wxString"); return 0; }

long ZN15wxHtmlHelpFrame15AddGrabIfNeededEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxHtmlHelpFrame15AddGrabIfNeededEv"); return 0; }

long ZN15wxHtmlHelpFrame4InitEP14wxHtmlHelpData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxHtmlHelpFrame4InitEP14wxHtmlHelpData"); return 0; }

long ZN15wxHtmlHelpFrame6CreateEP8wxWindowiRK8wxStringiP12wxConfigBaseS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxHtmlHelpFrame6CreateEP8wxWindowiRK8wxStringiP12wxConfigBaseS4_"); return 0; }

long ZN15wxHtmlHelpFrameC1EP8wxWindowiRK8wxStringiP14wxHtmlHelpDataP12wxConfigBaseS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxHtmlHelpFrameC1EP8wxWindowiRK8wxStringiP14wxHtmlHelpDataP12wxConfigBaseS4_"); return 0; }

long ZN15wxHtmlModalHelpC1EP8wxWindowRK8wxStringS4_i(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxHtmlModalHelpC1EP8wxWindowRK8wxStringS4_i"); return 0; }

long ZN15wxHtmlSelection3SetEPK10wxHtmlCellS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxHtmlSelection3SetEPK10wxHtmlCellS2_"); return 0; }

long ZN15wxHtmlSelection3SetERK7wxPointPK10wxHtmlCellS2_S5_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxHtmlSelection3SetERK7wxPointPK10wxHtmlCellS2_S5_"); return 0; }

long ZN15wxHtmlWinParser11SetFontSizeEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxHtmlWinParser11SetFontSizeEi"); return 0; }

long ZN15wxHtmlWinParser12SetContainerEP19wxHtmlContainerCell(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxHtmlWinParser12SetContainerEP19wxHtmlContainerCell"); return 0; }

long ZN15wxHtmlWinParser13OpenContainerEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxHtmlWinParser13OpenContainerEv"); return 0; }

long ZN15wxHtmlWinParser14CloseContainerEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxHtmlWinParser14CloseContainerEv"); return 0; }

long ZN15wxHtmlWinParser16SetStandardFontsEiRK8wxStringS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxHtmlWinParser16SetStandardFontsEiRK8wxStringS2_"); return 0; }

long ZN15wxHtmlWinParser7SetLinkERK14wxHtmlLinkInfo(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxHtmlWinParser7SetLinkERK14wxHtmlLinkInfo"); return 0; }

long ZN15wxHtmlWinParser8SetFontsERK8wxStringS2_PKi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxHtmlWinParser8SetFontsERK8wxStringS2_PKi"); return 0; }

long ZN15wxHtmlWinParser9AddModuleEP16wxHtmlTagsModule(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxHtmlWinParser9AddModuleEP16wxHtmlTagsModule"); return 0; }

long ZN15wxHtmlWinParser9GetWindowEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxHtmlWinParser9GetWindowEv"); return 0; }

long ZN15wxHtmlWinParserC1EP21wxHtmlWindowInterface(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxHtmlWinParserC1EP21wxHtmlWindowInterface"); return 0; }

long ZN15wxHyperlinkCtrl15SetNormalColourERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxHyperlinkCtrl15SetNormalColourERK8wxColour"); return 0; }

long ZN15wxHyperlinkCtrl16SetVisitedColourERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxHyperlinkCtrl16SetVisitedColourERK8wxColour"); return 0; }

long ZN15wxHyperlinkCtrl6CreateEP8wxWindowiRK8wxStringS4_RK7wxPointRK6wxSizelS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxHyperlinkCtrl6CreateEP8wxWindowiRK8wxStringS4_RK7wxPointRK6wxSizelS4_"); return 0; }

long ZN15wxItemContainer15SetClientObjectEjP12wxClientData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxItemContainer15SetClientObjectEjP12wxClientData"); return 0; }

long ZN15wxItemContainer6AppendERK13wxArrayString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxItemContainer6AppendERK13wxArrayString"); return 0; }

long ZN15wxItemContainer6InsertERK8wxStringjP12wxClientData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxItemContainer6InsertERK8wxStringjP12wxClientData"); return 0; }

long ZN15wxItemContainerD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxItemContainerD2Ev"); return 0; }

long ZN15wxMDIChildFrame4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxMDIChildFrame4InitEv"); return 0; }

long ZN15wxMDIChildFrame6CreateEP16wxMDIParentFrameiRK8wxStringRK7wxPointRK6wxSizelS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxMDIChildFrame6CreateEP16wxMDIParentFrameiRK8wxStringRK7wxPointRK6wxSizelS4_"); return 0; }

long ZN15wxMDIChildFrameC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxMDIChildFrameC1Ev"); return 0; }

long ZN15wxMessageDialogC1EP8wxWindowRK8wxStringS4_lRK7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxMessageDialogC1EP8wxWindowRK8wxStringS4_lRK7wxPoint"); return 0; }

long ZN15wxPoint2DDouble14SetVectorAngleEd(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxPoint2DDouble14SetVectorAngleEd"); return 0; }

long ZN15wxPreviewCanvasC1EP18wxPrintPreviewBaseP8wxWindowRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxPreviewCanvasC1EP18wxPrintPreviewBaseP8wxWindowRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN15wxRichTextImage4CopyERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxRichTextImage4CopyERKS_"); return 0; }

long ZN15wxRichTextRange7LimitToERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxRichTextRange7LimitToERKS_"); return 0; }

long ZN15wxStatusBarBase13PopStatusTextEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxStatusBarBase13PopStatusTextEi"); return 0; }

long ZN15wxStatusBarBase14PushStatusTextERK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxStatusBarBase14PushStatusTextERK8wxStringi"); return 0; }

long ZN15wxSystemOptions12GetOptionIntERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxSystemOptions12GetOptionIntERK8wxString"); return 0; }

long ZN15wxSystemOptions9GetOptionERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxSystemOptions9GetOptionERK8wxString"); return 0; }

long ZN15wxSystemOptions9HasOptionERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxSystemOptions9HasOptionERK8wxString"); return 0; }

long ZN15wxSystemOptions9SetOptionERK8wxStringS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxSystemOptions9SetOptionERK8wxStringS2_"); return 0; }

long ZN15wxSystemOptions9SetOptionERK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxSystemOptions9SetOptionERK8wxStringi"); return 0; }

long ZN15wxUpdateUIEvent13sm_updateModeE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxUpdateUIEvent13sm_updateModeE"); return 0; }

long ZN15wxUpdateUIEvent15ResetUpdateTimeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxUpdateUIEvent15ResetUpdateTimeEv"); return 0; }

long ZN15wxUpdateUIEvent17sm_updateIntervalE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxUpdateUIEvent17sm_updateIntervalE"); return 0; }

long ZN15wxUpdateUIEvent9CanUpdateEP12wxWindowBase(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxUpdateUIEvent9CanUpdateEP12wxWindowBase"); return 0; }

long ZN16wxBaseArrayShort3AddEsm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxBaseArrayShort3AddEsm"); return 0; }

long ZN16wxBaseArrayShortC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxBaseArrayShortC2Ev"); return 0; }

long ZN16wxBaseArrayShortD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxBaseArrayShortD2Ev"); return 0; }

long ZN16wxBitmapComboBox4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxBitmapComboBox4InitEv"); return 0; }

long ZN16wxBitmapComboBox6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizeRK13wxArrayStringlRK11wxValidatorS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxBitmapComboBox6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizeRK13wxArrayStringlRK11wxValidatorS4_"); return 0; }

long ZN16wxBitmapComboBox6InsertERK8wxStringRK8wxBitmapjP12wxClientData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxBitmapComboBox6InsertERK8wxStringRK8wxBitmapjP12wxClientData"); return 0; }

long ZN16wxBitmapComboBoxC1EP8wxWindowiRK8wxStringRK7wxPointRK6wxSizeRK13wxArrayStringlRK11wxValidatorS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxBitmapComboBoxC1EP8wxWindowiRK8wxStringRK7wxPointRK6wxSizeRK13wxArrayStringlRK11wxValidatorS4_"); return 0; }

long ZN16wxColourDatabase9AddColourERK8wxStringRK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxColourDatabase9AddColourERK8wxStringRK8wxColour"); return 0; }

long ZN16wxColourDatabaseC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxColourDatabaseC1Ev"); return 0; }

long ZN16wxColourDatabaseD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxColourDatabaseD1Ev"); return 0; }

long ZN16wxDataObjectBaseD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxDataObjectBaseD2Ev"); return 0; }

long ZN16wxEventHashTableC1ERK12wxEventTable(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxEventHashTableC1ERK12wxEventTable"); return 0; }

long ZN16wxEventHashTableD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxEventHashTableD1Ev"); return 0; }

long ZN16wxFileDataObject7AddFileERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxFileDataObject7AddFileERK8wxString"); return 0; }

long ZN16wxFileDropTarget6OnDataEii12wxDragResult(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxFileDropTarget6OnDataEii12wxDragResult"); return 0; }

long ZN16wxFileDropTargetC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxFileDropTargetC2Ev"); return 0; }

long ZN16wxFontEnumerator12GetEncodingsERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxFontEnumerator12GetEncodingsERK8wxString"); return 0; }

long ZN16wxFontEnumerator12GetFacenamesE14wxFontEncodingb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxFontEnumerator12GetFacenamesE14wxFontEncodingb"); return 0; }

long ZN16wxFontEnumerator15IsValidFacenameERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxFontEnumerator15IsValidFacenameERK8wxString"); return 0; }

long ZN16wxFontEnumerator18EnumerateEncodingsERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxFontEnumerator18EnumerateEncodingsERK8wxString"); return 0; }

long ZN16wxFontEnumerator18EnumerateFacenamesE14wxFontEncodingb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxFontEnumerator18EnumerateFacenamesE14wxFontEncodingb"); return 0; }

long ZN16wxFontMapperBase11GetEncodingEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxFontMapperBase11GetEncodingEm"); return 0; }

long ZN16wxFontMapperBase13SetConfigPathERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxFontMapperBase13SetConfigPathERK8wxString"); return 0; }

long ZN16wxFontMapperBase15GetEncodingNameE14wxFontEncoding(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxFontMapperBase15GetEncodingNameE14wxFontEncoding"); return 0; }

long ZN16wxFontMapperBase19GetEncodingFromNameERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxFontMapperBase19GetEncodingFromNameERK8wxString"); return 0; }

long ZN16wxFontMapperBase20GetDefaultConfigPathEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxFontMapperBase20GetDefaultConfigPathEv"); return 0; }

long ZN16wxFontMapperBase22GetEncodingDescriptionE14wxFontEncoding(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxFontMapperBase22GetEncodingDescriptionE14wxFontEncoding"); return 0; }

long ZN16wxFontMapperBase26GetSupportedEncodingsCountEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxFontMapperBase26GetSupportedEncodingsCountEv"); return 0; }

long ZN16wxFontMapperBase3SetEP12wxFontMapper(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxFontMapperBase3SetEP12wxFontMapper"); return 0; }

long ZN16wxFontPickerCtrl15SetSelectedFontERK6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxFontPickerCtrl15SetSelectedFontERK6wxFont"); return 0; }

long ZN16wxFontPickerCtrl6CreateEP8wxWindowiRK6wxFontRK7wxPointRK6wxSizelRK11wxValidatorRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxFontPickerCtrl6CreateEP8wxWindowiRK6wxFontRK7wxPointRK6wxSizelRK11wxValidatorRK8wxString"); return 0; }

long ZN16wxGDIObjListBaseC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxGDIObjListBaseC1Ev"); return 0; }

long ZN16wxGDIObjListBaseD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxGDIObjListBaseD1Ev"); return 0; }

long ZN16wxGenericDirCtrl4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxGenericDirCtrl4InitEv"); return 0; }

long ZN16wxGenericDirCtrl6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_iS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxGenericDirCtrl6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_iS4_"); return 0; }

long ZN16wxGenericDirCtrlC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxGenericDirCtrlC1Ev"); return 0; }

long ZN16wxGraphicsObjectC1EP18wxGraphicsRenderer(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxGraphicsObjectC1EP18wxGraphicsRenderer"); return 0; }

long ZN16wxGraphicsObjectC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxGraphicsObjectC2Ev"); return 0; }

long ZN16wxGraphicsObjectD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxGraphicsObjectD2Ev"); return 0; }

long ZN16wxGridCellEditor11StartingKeyER10wxKeyEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxGridCellEditor11StartingKeyER10wxKeyEvent"); return 0; }

long ZN16wxGridCellEditor12HandleReturnER10wxKeyEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxGridCellEditor12HandleReturnER10wxKeyEvent"); return 0; }

long ZN16wxGridCellEditor13IsAcceptedKeyER10wxKeyEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxGridCellEditor13IsAcceptedKeyER10wxKeyEvent"); return 0; }

long ZN16wxGridCellEditor13StartingClickEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxGridCellEditor13StartingClickEv"); return 0; }

long ZN16wxGridCellEditor15PaintBackgroundERK6wxRectP14wxGridCellAttr(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxGridCellEditor15PaintBackgroundERK6wxRectP14wxGridCellAttr"); return 0; }

long ZN16wxGridCellEditor4ShowEbP14wxGridCellAttr(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxGridCellEditor4ShowEbP14wxGridCellAttr"); return 0; }

long ZN16wxGridCellEditor7DestroyEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxGridCellEditor7DestroyEv"); return 0; }

long ZN16wxGridCellEditor7SetSizeERK6wxRect(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxGridCellEditor7SetSizeERK6wxRect"); return 0; }

long ZN16wxGridCellEditorC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxGridCellEditorC2Ev"); return 0; }

long ZN16wxGridCellEditorD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxGridCellEditorD2Ev"); return 0; }

long ZN16wxGridCellWorker13SetParametersERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxGridCellWorker13SetParametersERK8wxString"); return 0; }

long ZN16wxGridCellWorkerD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxGridCellWorkerD2Ev"); return 0; }

long ZN16wxHtmlDCRenderer11SetHtmlTextERK8wxStringS2_b(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxHtmlDCRenderer11SetHtmlTextERK8wxStringS2_b"); return 0; }

long ZN16wxHtmlDCRenderer14GetTotalHeightEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxHtmlDCRenderer14GetTotalHeightEv"); return 0; }

long ZN16wxHtmlDCRenderer16SetStandardFontsEiRK8wxStringS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxHtmlDCRenderer16SetStandardFontsEiRK8wxStringS2_"); return 0; }

long ZN16wxHtmlDCRenderer5SetDCEP4wxDCd(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxHtmlDCRenderer5SetDCEP4wxDCd"); return 0; }

long ZN16wxHtmlDCRenderer6RenderEiiR10wxArrayIntiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxHtmlDCRenderer6RenderEiiR10wxArrayIntiii"); return 0; }

long ZN16wxHtmlDCRenderer7SetSizeEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxHtmlDCRenderer7SetSizeEii"); return 0; }

long ZN16wxHtmlDCRenderer8SetFontsERK8wxStringS2_PKi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxHtmlDCRenderer8SetFontsERK8wxStringS2_PKi"); return 0; }

long ZN16wxHtmlDCRendererC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxHtmlDCRendererC1Ev"); return 0; }

long ZN16wxHtmlHelpDialog14SetTitleFormatERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxHtmlHelpDialog14SetTitleFormatERK8wxString"); return 0; }

long ZN16wxHtmlHelpDialog4InitEP14wxHtmlHelpData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxHtmlHelpDialog4InitEP14wxHtmlHelpData"); return 0; }

long ZN16wxHtmlHelpDialog6CreateEP8wxWindowiRK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxHtmlHelpDialog6CreateEP8wxWindowiRK8wxStringi"); return 0; }

long ZN16wxHtmlHelpDialogC1EP8wxWindowiRK8wxStringiP14wxHtmlHelpData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxHtmlHelpDialogC1EP8wxWindowiRK8wxStringiP14wxHtmlHelpData"); return 0; }

long ZN16wxHtmlHelpWindow12DisplayIndexEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxHtmlHelpWindow12DisplayIndexEv"); return 0; }

long ZN16wxHtmlHelpWindow12RefreshListsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxHtmlHelpWindow12RefreshListsEv"); return 0; }

long ZN16wxHtmlHelpWindow13KeywordSearchERK8wxString16wxHelpSearchMode(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxHtmlHelpWindow13KeywordSearchERK8wxString16wxHelpSearchMode"); return 0; }

long ZN16wxHtmlHelpWindow13SetControllerEP20wxHtmlHelpController(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxHtmlHelpWindow13SetControllerEP20wxHtmlHelpController"); return 0; }

long ZN16wxHtmlHelpWindow15DisplayContentsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxHtmlHelpWindow15DisplayContentsEv"); return 0; }

long ZN16wxHtmlHelpWindow17NotifyPageChangedEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxHtmlHelpWindow17NotifyPageChangedEv"); return 0; }

long ZN16wxHtmlHelpWindow17ReadCustomizationEP12wxConfigBaseRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxHtmlHelpWindow17ReadCustomizationEP12wxConfigBaseRK8wxString"); return 0; }

long ZN16wxHtmlHelpWindow18WriteCustomizationEP12wxConfigBaseRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxHtmlHelpWindow18WriteCustomizationEP12wxConfigBaseRK8wxString"); return 0; }

long ZN16wxHtmlHelpWindow4InitEP14wxHtmlHelpData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxHtmlHelpWindow4InitEP14wxHtmlHelpData"); return 0; }

long ZN16wxHtmlHelpWindow6CreateEP8wxWindowiRK7wxPointRK6wxSizeii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxHtmlHelpWindow6CreateEP8wxWindowiRK7wxPointRK6wxSizeii"); return 0; }

long ZN16wxHtmlHelpWindow7DisplayERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxHtmlHelpWindow7DisplayERK8wxString"); return 0; }

long ZN16wxHtmlHelpWindow7DisplayEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxHtmlHelpWindow7DisplayEi"); return 0; }

long ZN16wxHtmlHelpWindowC1EP8wxWindowiRK7wxPointRK6wxSizeiiP14wxHtmlHelpData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxHtmlHelpWindowC1EP8wxWindowiRK7wxPointRK6wxSizeiiP14wxHtmlHelpData"); return 0; }

long ZN16wxHtmlTagHandler12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxHtmlTagHandler12ms_classInfoE"); return 0; }

long ZN16wxHtmlTagsModule6OnInitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxHtmlTagsModule6OnInitEv"); return 0; }

long ZN16wxHtmlWidgetCellC1EP8wxWindowi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxHtmlWidgetCellC1EP8wxWindowi"); return 0; }

long ZN16wxMDIParentFrame4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxMDIParentFrame4InitEv"); return 0; }

long ZN16wxMDIParentFrame6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxMDIParentFrame6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_"); return 0; }

long ZN16wxNativeFontInfo10FromStringERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxNativeFontInfo10FromStringERK8wxString"); return 0; }

long ZN16wxNativeFontInfo11SetEncodingE14wxFontEncoding(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxNativeFontInfo11SetEncodingE14wxFontEncoding"); return 0; }

long ZN16wxNativeFontInfo11SetFaceNameERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxNativeFontInfo11SetFaceNameERK8wxString"); return 0; }

long ZN16wxNativeFontInfo12SetPointSizeEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxNativeFontInfo12SetPointSizeEi"); return 0; }

long ZN16wxNativeFontInfo13SetUnderlinedEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxNativeFontInfo13SetUnderlinedEb"); return 0; }

long ZN16wxNativeFontInfo14FromUserStringERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxNativeFontInfo14FromUserStringERK8wxString"); return 0; }

long ZN16wxNativeFontInfo4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxNativeFontInfo4InitEv"); return 0; }

long ZN16wxNativeFontInfo8SetStyleE11wxFontStyle(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxNativeFontInfo8SetStyleE11wxFontStyle"); return 0; }

long ZN16wxNativeFontInfo9SetFamilyE12wxFontFamily(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxNativeFontInfo9SetFamilyE12wxFontFamily"); return 0; }

long ZN16wxNativeFontInfo9SetWeightE12wxFontWeight(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxNativeFontInfo9SetWeightE12wxFontWeight"); return 0; }

long ZN16wxProgressDialog6ResumeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxProgressDialog6ResumeEv"); return 0; }

long ZN16wxProgressDialogC1ERK8wxStringS2_iP8wxWindowi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxProgressDialogC1ERK8wxStringS2_iP8wxWindowi"); return 0; }

long ZN16wxRegionIteratorC1ERK8wxRegion(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRegionIteratorC1ERK8wxRegion"); return 0; }

long ZN16wxRegionIteratorD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRegionIteratorD1Ev"); return 0; }

long ZN16wxRegionIteratorppEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRegionIteratorppEi"); return 0; }

long ZN16wxRendererNative10GetDefaultEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRendererNative10GetDefaultEv"); return 0; }

long ZN16wxRendererNative10GetGenericEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRendererNative10GetGenericEv"); return 0; }

long ZN16wxRendererNative3GetEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRendererNative3GetEv"); return 0; }

long ZN16wxRendererNative3SetEPS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRendererNative3SetEPS_"); return 0; }

long ZN16wxRichTextBuffer10AddHandlerEP21wxRichTextFileHandler(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer10AddHandlerEP21wxRichTextFileHandler"); return 0; }

long ZN16wxRichTextBuffer11BeginItalicEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer11BeginItalicEv"); return 0; }

long ZN16wxRichTextBuffer11FindHandlerERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer11FindHandlerERK8wxString"); return 0; }

long ZN16wxRichTextBuffer11FindHandlerERK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer11FindHandlerERK8wxStringi"); return 0; }

long ZN16wxRichTextBuffer11FindHandlerEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer11FindHandlerEi"); return 0; }

long ZN16wxRichTextBuffer11SetRendererEP18wxRichTextRenderer(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer11SetRendererEP18wxRichTextRenderer"); return 0; }

long ZN16wxRichTextBuffer11sm_handlersE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer11sm_handlersE"); return 0; }

long ZN16wxRichTextBuffer11sm_rendererE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer11sm_rendererE"); return 0; }

long ZN16wxRichTextBuffer13BeginFontSizeEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer13BeginFontSizeEi"); return 0; }

long ZN16wxRichTextBuffer13InsertHandlerEP21wxRichTextFileHandler(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer13InsertHandlerEP21wxRichTextFileHandler"); return 0; }

long ZN16wxRichTextBuffer13PopStyleSheetEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer13PopStyleSheetEv"); return 0; }

long ZN16wxRichTextBuffer13RemoveHandlerERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer13RemoveHandlerERK8wxString"); return 0; }

long ZN16wxRichTextBuffer14BeginAlignmentE19wxTextAttrAlignment(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer14BeginAlignmentE19wxTextAttrAlignment"); return 0; }

long ZN16wxRichTextBuffer14BeginListStyleERK8wxStringii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer14BeginListStyleERK8wxStringii"); return 0; }

long ZN16wxRichTextBuffer14BeginUnderlineEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer14BeginUnderlineEv"); return 0; }

long ZN16wxRichTextBuffer14GetExtWildcardEbbP10wxArrayInt(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer14GetExtWildcardEbbP10wxArrayInt"); return 0; }

long ZN16wxRichTextBuffer14PushStyleSheetEP20wxRichTextStyleSheet(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer14PushStyleSheetEP20wxRichTextStyleSheet"); return 0; }

long ZN16wxRichTextBuffer15AddEventHandlerEP12wxEvtHandler(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer15AddEventHandlerEP12wxEvtHandler"); return 0; }

long ZN16wxRichTextBuffer15BeginLeftIndentEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer15BeginLeftIndentEii"); return 0; }

long ZN16wxRichTextBuffer15BeginTextColourERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer15BeginTextColourERK8wxColour"); return 0; }

long ZN16wxRichTextBuffer15CleanUpHandlersEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer15CleanUpHandlersEv"); return 0; }

long ZN16wxRichTextBuffer16BeginLineSpacingEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer16BeginLineSpacingEi"); return 0; }

long ZN16wxRichTextBuffer16BeginRightIndentEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer16BeginRightIndentEi"); return 0; }

long ZN16wxRichTextBuffer17BeginSymbolBulletERK8wxStringiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer17BeginSymbolBulletERK8wxStringiii"); return 0; }

long ZN16wxRichTextBuffer18ClearEventHandlersEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer18ClearEventHandlersEv"); return 0; }

long ZN16wxRichTextBuffer18InsertTextWithUndoElRK8wxStringP14wxRichTextCtrli(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer18InsertTextWithUndoElRK8wxStringP14wxRichTextCtrli"); return 0; }

long ZN16wxRichTextBuffer18RemoveEventHandlerEP12wxEvtHandlerb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer18RemoveEventHandlerEP12wxEvtHandlerb"); return 0; }

long ZN16wxRichTextBuffer19BeginCharacterStyleERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer19BeginCharacterStyleERK8wxString"); return 0; }

long ZN16wxRichTextBuffer19BeginNumberedBulletEiiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer19BeginNumberedBulletEiiii"); return 0; }

long ZN16wxRichTextBuffer19BeginParagraphStyleERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer19BeginParagraphStyleERK8wxString"); return 0; }

long ZN16wxRichTextBuffer19BeginStandardBulletERK8wxStringiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer19BeginStandardBulletERK8wxStringiii"); return 0; }

long ZN16wxRichTextBuffer19DeleteRangeWithUndoERK15wxRichTextRangeP14wxRichTextCtrl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer19DeleteRangeWithUndoERK15wxRichTextRangeP14wxRichTextCtrl"); return 0; }

long ZN16wxRichTextBuffer19InsertImageWithUndoElRK20wxRichTextImageBlockP14wxRichTextCtrli(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer19InsertImageWithUndoElRK20wxRichTextImageBlockP14wxRichTextCtrli"); return 0; }

long ZN16wxRichTextBuffer19sm_bulletProportionE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer19sm_bulletProportionE"); return 0; }

long ZN16wxRichTextBuffer20InitStandardHandlersEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer20InitStandardHandlersEv"); return 0; }

long ZN16wxRichTextBuffer20sm_bulletRightMarginE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer20sm_bulletRightMarginE"); return 0; }

long ZN16wxRichTextBuffer21BeginParagraphSpacingEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer21BeginParagraphSpacingEii"); return 0; }

long ZN16wxRichTextBuffer21InsertNewlineWithUndoElP14wxRichTextCtrli(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer21InsertNewlineWithUndoElP14wxRichTextCtrli"); return 0; }

long ZN16wxRichTextBuffer22SetStyleSheetAndNotifyEP20wxRichTextStyleSheet(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer22SetStyleSheetAndNotifyEP20wxRichTextStyleSheet"); return 0; }

long ZN16wxRichTextBuffer24InsertParagraphsWithUndoElRK28wxRichTextParagraphLayoutBoxP14wxRichTextCtrli(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer24InsertParagraphsWithUndoElRK28wxRichTextParagraphLayoutBoxP14wxRichTextCtrli"); return 0; }

long ZN16wxRichTextBuffer25FindHandlerFilenameOrTypeERK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer25FindHandlerFilenameOrTypeERK8wxStringi"); return 0; }

long ZN16wxRichTextBuffer4CopyERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer4CopyERKS_"); return 0; }

long ZN16wxRichTextBuffer4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer4InitEv"); return 0; }

long ZN16wxRichTextBuffer8BeginURLERK8wxStringS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer8BeginURLERK8wxStringS2_"); return 0; }

long ZN16wxRichTextBuffer9BeginBoldEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer9BeginBoldEv"); return 0; }

long ZN16wxRichTextBuffer9BeginFontERK6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer9BeginFontERK6wxFont"); return 0; }

long ZN16wxRichTextBuffer9SendEventER7wxEventb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextBuffer9SendEventER7wxEventb"); return 0; }

long ZN16wxRichTextObject11DereferenceEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextObject11DereferenceEv"); return 0; }

long ZN16wxRichTextObject23ConvertTenthsMMToPixelsER4wxDCi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextObject23ConvertTenthsMMToPixelsER4wxDCi"); return 0; }

long ZN16wxRichTextObject23ConvertTenthsMMToPixelsEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextObject23ConvertTenthsMMToPixelsEii"); return 0; }

long ZN16wxRichTextObject4CopyERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextObject4CopyERKS_"); return 0; }

long ZN16wxRichTextObjectC2EPS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextObjectC2EPS_"); return 0; }

long ZN16wxRichTextObjectD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxRichTextObjectD2Ev"); return 0; }

long ZN16wxScrollWinEventC1Eiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxScrollWinEventC1Eiii"); return 0; }

long ZN16wxScrolledWindow12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxScrolledWindow12ms_classInfoE"); return 0; }

long ZN16wxScrolledWindow13sm_eventTableE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxScrolledWindow13sm_eventTableE"); return 0; }

long ZN16wxScrolledWindow6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxScrolledWindow6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN16wxScrolledWindowD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxScrolledWindowD2Ev"); return 0; }

long ZN16wxSplitterWindow10InitializeEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxSplitterWindow10InitializeEP8wxWindow"); return 0; }

long ZN16wxSplitterWindow10UpdateSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxSplitterWindow10UpdateSizeEv"); return 0; }

long ZN16wxSplitterWindow13ReplaceWindowEP8wxWindowS1_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxSplitterWindow13ReplaceWindowEP8wxWindowS1_"); return 0; }

long ZN16wxSplitterWindow14SetSashGravityEd(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxSplitterWindow14SetSashGravityEd"); return 0; }

long ZN16wxSplitterWindow15SetSashPositionEib(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxSplitterWindow15SetSashPositionEib"); return 0; }

long ZN16wxSplitterWindow18SetMinimumPaneSizeEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxSplitterWindow18SetMinimumPaneSizeEi"); return 0; }

long ZN16wxSplitterWindow4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxSplitterWindow4InitEv"); return 0; }

long ZN16wxSplitterWindow6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxSplitterWindow6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN16wxSplitterWindow7UnsplitEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxSplitterWindow7UnsplitEP8wxWindow"); return 0; }

long ZN16wxStaticBoxSizerC1EP11wxStaticBoxi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxStaticBoxSizerC1EP11wxStaticBoxi"); return 0; }

long ZN16wxStaticTextBase4WrapEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxStaticTextBase4WrapEi"); return 0; }

long ZN16wxSystemSettings13GetScreenTypeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxSystemSettings13GetScreenTypeEv"); return 0; }

long ZN16wxSystemSettings13SetScreenTypeE18wxSystemScreenType(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxSystemSettings13SetScreenTypeE18wxSystemScreenType"); return 0; }

long ZN16wxTextDataObject7SetDataERK12wxDataFormatmPKv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxTextDataObject7SetDataERK12wxDataFormatmPKv"); return 0; }

long ZN16wxTextDropTarget6OnDataEii12wxDragResult(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxTextDropTarget6OnDataEii12wxDragResult"); return 0; }

long ZN16wxTextDropTargetC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxTextDropTargetC2Ev"); return 0; }

long ZN16wxWindowDisablerC1EP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxWindowDisablerC1EP8wxWindow"); return 0; }

long ZN16wxWindowDisablerD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN16wxWindowDisablerD1Ev"); return 0; }

long ZN17_wxHashTableBase211DeleteNodesEmPP21_wxHashTable_NodeBasePFvS1_E(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17_wxHashTableBase211DeleteNodesEmPP21_wxHashTable_NodeBasePFvS1_E"); return 0; }

long ZN17_wxHashTableBase212GetNextPrimeEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17_wxHashTableBase212GetNextPrimeEm"); return 0; }

long ZN17_wxHashTableBase213CopyHashTableEPP21_wxHashTable_NodeBasemPS_S2_PFmS3_S1_EPFS1_S1_E(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17_wxHashTableBase213CopyHashTableEPP21_wxHashTable_NodeBasemPS_S2_PFmS3_S1_EPFS1_S1_E"); return 0; }

long ZN17_wxHashTableBase216DummyProcessNodeEP21_wxHashTable_NodeBase(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17_wxHashTableBase216DummyProcessNodeEP21_wxHashTable_NodeBase"); return 0; }

long ZN17wxArrayVideoModesD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxArrayVideoModesD1Ev"); return 0; }

long ZN17wxAuiSimpleTabArtC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxAuiSimpleTabArtC1Ev"); return 0; }

long ZN17wxAuiTabContainer10DoShowHideEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxAuiTabContainer10DoShowHideEv"); return 0; }

long ZN17wxAuiTabContainer10InsertPageEP8wxWindowRK17wxAuiNotebookPagem(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxAuiTabContainer10InsertPageEP8wxWindowRK17wxAuiNotebookPagem"); return 0; }

long ZN17wxAuiTabContainer10RemovePageEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxAuiTabContainer10RemovePageEP8wxWindow"); return 0; }

long ZN17wxAuiTabContainer12IsTabVisibleEiiP4wxDCP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxAuiTabContainer12IsTabVisibleEiiP4wxDCP8wxWindow"); return 0; }

long ZN17wxAuiTabContainer12RemoveButtonEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxAuiTabContainer12RemoveButtonEi"); return 0; }

long ZN17wxAuiTabContainer12SetTabOffsetEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxAuiTabContainer12SetTabOffsetEm"); return 0; }

long ZN17wxAuiTabContainer13SetActivePageEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxAuiTabContainer13SetActivePageEP8wxWindow"); return 0; }

long ZN17wxAuiTabContainer13SetActivePageEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxAuiTabContainer13SetActivePageEm"); return 0; }

long ZN17wxAuiTabContainer13SetNoneActiveEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxAuiTabContainer13SetNoneActiveEv"); return 0; }

long ZN17wxAuiTabContainer13SetNormalFontERK6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxAuiTabContainer13SetNormalFontERK6wxFont"); return 0; }

long ZN17wxAuiTabContainer14MakeTabVisibleEiP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxAuiTabContainer14MakeTabVisibleEiP8wxWindow"); return 0; }

long ZN17wxAuiTabContainer14SetArtProviderEP11wxAuiTabArt(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxAuiTabContainer14SetArtProviderEP11wxAuiTabArt"); return 0; }

long ZN17wxAuiTabContainer15SetSelectedFontERK6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxAuiTabContainer15SetSelectedFontERK6wxFont"); return 0; }

long ZN17wxAuiTabContainer16SetMeasuringFontERK6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxAuiTabContainer16SetMeasuringFontERK6wxFont"); return 0; }

long ZN17wxAuiTabContainer7AddPageEP8wxWindowRK17wxAuiNotebookPage(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxAuiTabContainer7AddPageEP8wxWindowRK17wxAuiNotebookPage"); return 0; }

long ZN17wxAuiTabContainer7GetPageEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxAuiTabContainer7GetPageEm"); return 0; }

long ZN17wxAuiTabContainer7SetRectERK6wxRect(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxAuiTabContainer7SetRectERK6wxRect"); return 0; }

long ZN17wxAuiTabContainer8GetPagesEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxAuiTabContainer8GetPagesEv"); return 0; }

long ZN17wxAuiTabContainer8MovePageEP8wxWindowm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxAuiTabContainer8MovePageEP8wxWindowm"); return 0; }

long ZN17wxAuiTabContainer8SetFlagsEj(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxAuiTabContainer8SetFlagsEj"); return 0; }

long ZN17wxAuiTabContainer9AddButtonEiiRK8wxBitmapS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxAuiTabContainer9AddButtonEiiRK8wxBitmapS2_"); return 0; }

long ZN17wxAuiTabContainerC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxAuiTabContainerC1Ev"); return 0; }

long ZN17wxBaseArrayDoubleC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxBaseArrayDoubleC2Ev"); return 0; }

long ZN17wxBaseArrayDoubleD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxBaseArrayDoubleD2Ev"); return 0; }

long ZN17wxBaseArrayDoubleaSERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxBaseArrayDoubleaSERKS_"); return 0; }

long ZN17wxChildFocusEventC1EP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxChildFocusEventC1EP8wxWindow"); return 0; }

long ZN17wxEventLoopManualC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxEventLoopManualC2Ev"); return 0; }

long ZN17wxFindReplaceData4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxFindReplaceData4InitEv"); return 0; }

long ZN17wxGenericTreeCtrl10SelectItemERK12wxTreeItemIdb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl10SelectItemERK12wxTreeItemIdb"); return 0; }

long ZN17wxGenericTreeCtrl11SetItemBoldERK12wxTreeItemIdb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl11SetItemBoldERK12wxTreeItemIdb"); return 0; }

long ZN17wxGenericTreeCtrl11SetItemDataERK12wxTreeItemIdP14wxTreeItemData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl11SetItemDataERK12wxTreeItemIdP14wxTreeItemData"); return 0; }

long ZN17wxGenericTreeCtrl11SetItemFontERK12wxTreeItemIdRK6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl11SetItemFontERK12wxTreeItemIdRK6wxFont"); return 0; }

long ZN17wxGenericTreeCtrl11SetItemTextERK12wxTreeItemIdRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl11SetItemTextERK12wxTreeItemIdRK8wxString"); return 0; }

long ZN17wxGenericTreeCtrl11UnselectAllEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl11UnselectAllEv"); return 0; }

long ZN17wxGenericTreeCtrl12DoInsertItemERK12wxTreeItemIdmRK8wxStringiiP14wxTreeItemData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl12DoInsertItemERK12wxTreeItemIdmRK8wxStringiiP14wxTreeItemData"); return 0; }

long ZN17wxGenericTreeCtrl12EndEditLabelERK12wxTreeItemIdb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl12EndEditLabelERK12wxTreeItemIdb"); return 0; }

long ZN17wxGenericTreeCtrl12SetImageListEP11wxImageList(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl12SetImageListEP11wxImageList"); return 0; }

long ZN17wxGenericTreeCtrl12SetItemImageERK12wxTreeItemIdi14wxTreeItemIcon(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl12SetItemImageERK12wxTreeItemIdi14wxTreeItemIcon"); return 0; }

long ZN17wxGenericTreeCtrl12SortChildrenERK12wxTreeItemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl12SortChildrenERK12wxTreeItemId"); return 0; }

long ZN17wxGenericTreeCtrl13DoInsertAfterERK12wxTreeItemIdS2_RK8wxStringiiP14wxTreeItemData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl13DoInsertAfterERK12wxTreeItemIdS2_RK8wxStringiiP14wxTreeItemData"); return 0; }

long ZN17wxGenericTreeCtrl13EnsureVisibleERK12wxTreeItemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl13EnsureVisibleERK12wxTreeItemId"); return 0; }

long ZN17wxGenericTreeCtrl14DeleteAllItemsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl14DeleteAllItemsEv"); return 0; }

long ZN17wxGenericTreeCtrl14DeleteChildrenERK12wxTreeItemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl14DeleteChildrenERK12wxTreeItemId"); return 0; }

long ZN17wxGenericTreeCtrl14OnInternalIdleEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl14OnInternalIdleEv"); return 0; }

long ZN17wxGenericTreeCtrl14SetWindowStyleEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl14SetWindowStyleEl"); return 0; }

long ZN17wxGenericTreeCtrl16CollapseAndResetERK12wxTreeItemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl16CollapseAndResetERK12wxTreeItemId"); return 0; }

long ZN17wxGenericTreeCtrl17SetItemTextColourERK12wxTreeItemIdRK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl17SetItemTextColourERK12wxTreeItemIdRK8wxColour"); return 0; }

long ZN17wxGenericTreeCtrl17SetStateImageListEP11wxImageList(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl17SetStateImageListEP11wxImageList"); return 0; }

long ZN17wxGenericTreeCtrl18SetItemHasChildrenERK12wxTreeItemIdb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl18SetItemHasChildrenERK12wxTreeItemIdb"); return 0; }

long ZN17wxGenericTreeCtrl19SetBackgroundColourERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl19SetBackgroundColourERK8wxColour"); return 0; }

long ZN17wxGenericTreeCtrl19SetForegroundColourERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl19SetForegroundColourERK8wxColour"); return 0; }

long ZN17wxGenericTreeCtrl20SetItemDropHighlightERK12wxTreeItemIdb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl20SetItemDropHighlightERK12wxTreeItemIdb"); return 0; }

long ZN17wxGenericTreeCtrl23SetItemBackgroundColourERK12wxTreeItemIdRK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl23SetItemBackgroundColourERK12wxTreeItemIdRK8wxColour"); return 0; }

long ZN17wxGenericTreeCtrl25GetClassDefaultAttributesE15wxWindowVariant(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl25GetClassDefaultAttributesE15wxWindowVariant"); return 0; }

long ZN17wxGenericTreeCtrl4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl4InitEv"); return 0; }

long ZN17wxGenericTreeCtrl4ThawEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl4ThawEv"); return 0; }

long ZN17wxGenericTreeCtrl6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK11wxValidatorRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK11wxValidatorRK8wxString"); return 0; }

long ZN17wxGenericTreeCtrl6DeleteERK12wxTreeItemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl6DeleteERK12wxTreeItemId"); return 0; }

long ZN17wxGenericTreeCtrl6ExpandERK12wxTreeItemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl6ExpandERK12wxTreeItemId"); return 0; }

long ZN17wxGenericTreeCtrl6FreezeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl6FreezeEv"); return 0; }

long ZN17wxGenericTreeCtrl6ToggleERK12wxTreeItemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl6ToggleERK12wxTreeItemId"); return 0; }

long ZN17wxGenericTreeCtrl7AddRootERK8wxStringiiP14wxTreeItemData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl7AddRootERK8wxStringiiP14wxTreeItemData"); return 0; }

long ZN17wxGenericTreeCtrl7RefreshEbPK6wxRect(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl7RefreshEbPK6wxRect"); return 0; }

long ZN17wxGenericTreeCtrl7SetFontERK6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl7SetFontERK6wxFont"); return 0; }

long ZN17wxGenericTreeCtrl8CollapseERK12wxTreeItemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl8CollapseERK12wxTreeItemId"); return 0; }

long ZN17wxGenericTreeCtrl8ScrollToERK12wxTreeItemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl8ScrollToERK12wxTreeItemId"); return 0; }

long ZN17wxGenericTreeCtrl8UnselectEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl8UnselectEv"); return 0; }

long ZN17wxGenericTreeCtrl9EditLabelERK12wxTreeItemIdP11wxClassInfo(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl9EditLabelERK12wxTreeItemIdP11wxClassInfo"); return 0; }

long ZN17wxGenericTreeCtrl9SetIndentEj(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrl9SetIndentEj"); return 0; }

long ZN17wxGenericTreeCtrlD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGenericTreeCtrlD2Ev"); return 0; }

long ZN17wxGraphicsContext16CreateFromNativeEPv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGraphicsContext16CreateFromNativeEPv"); return 0; }

long ZN17wxGraphicsContext22CreateFromNativeWindowEPv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGraphicsContext22CreateFromNativeWindowEPv"); return 0; }

long ZN17wxGraphicsContext6CreateEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGraphicsContext6CreateEP8wxWindow"); return 0; }

long ZN17wxGraphicsContext6CreateERK10wxWindowDC(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGraphicsContext6CreateERK10wxWindowDC"); return 0; }

long ZN17wxGraphicsContext6CreateEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGraphicsContext6CreateEv"); return 0; }

long ZN17wxGraphicsContext6SetPenERK5wxPen(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGraphicsContext6SetPenERK5wxPen"); return 0; }

long ZN17wxGraphicsContext7SetFontERK6wxFontRK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGraphicsContext7SetFontERK6wxFontRK8wxColour"); return 0; }

long ZN17wxGraphicsContext8SetBrushERK7wxBrush(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGraphicsContext8SetBrushERK7wxBrush"); return 0; }

long ZN17wxGridStringTableC1Eii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxGridStringTableC1Eii"); return 0; }

long ZN17wxLayoutAlgorithm11LayoutFrameEP7wxFrameP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxLayoutAlgorithm11LayoutFrameEP7wxFrameP8wxWindow"); return 0; }

long ZN17wxLayoutAlgorithm12LayoutWindowEP8wxWindowS1_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxLayoutAlgorithm12LayoutWindowEP8wxWindowS1_"); return 0; }

long ZN17wxLayoutAlgorithm14LayoutMDIFrameEP16wxMDIParentFrameP6wxRect(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxLayoutAlgorithm14LayoutMDIFrameEP16wxMDIParentFrameP6wxRect"); return 0; }

long ZN17wxMDIClientWindow12CreateClientEP16wxMDIParentFramel(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxMDIClientWindow12CreateClientEP16wxMDIParentFramel"); return 0; }

long ZN17wxMDIClientWindowC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxMDIClientWindowC1Ev"); return 0; }

long ZN17wxMacPrintPreview12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxMacPrintPreview12ms_classInfoE"); return 0; }

long ZN17wxMemoryFSHandler7AddFileERK8wxStringRK7wxImagel(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxMemoryFSHandler7AddFileERK8wxStringRK7wxImagel"); return 0; }

long ZN17wxMemoryFSHandler7AddFileERK8wxStringRK8wxBitmapl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxMemoryFSHandler7AddFileERK8wxStringRK8wxBitmapl"); return 0; }

long ZN17wxPageSetupDialog16GetPageSetupDataEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxPageSetupDialog16GetPageSetupDataEv"); return 0; }

long ZN17wxPageSetupDialog22GetPageSetupDialogDataEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxPageSetupDialog22GetPageSetupDialogDataEv"); return 0; }

long ZN17wxPageSetupDialog9ShowModalEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxPageSetupDialog9ShowModalEv"); return 0; }

long ZN17wxPageSetupDialogC1EP8wxWindowP21wxPageSetupDialogData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxPageSetupDialogC1EP8wxWindowP21wxPageSetupDialogData"); return 0; }

long ZN17wxPrintDialogDataC1ERK11wxPrintData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxPrintDialogDataC1ERK11wxPrintData"); return 0; }

long ZN17wxPrintDialogDataC1ERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxPrintDialogDataC1ERKS_"); return 0; }

long ZN17wxPrintDialogDataC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxPrintDialogDataC1Ev"); return 0; }

long ZN17wxTextEntryDialog8SetValueERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxTextEntryDialog8SetValueERK8wxString"); return 0; }

long ZN17wxTextEntryDialogC1EP8wxWindowRK8wxStringS4_S4_lRK7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxTextEntryDialogC1EP8wxWindowRK8wxStringS4_S4_lRK7wxPoint"); return 0; }

long ZN17wxToolBarToolBase11SetLongHelpERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxToolBarToolBase11SetLongHelpERK8wxString"); return 0; }

long ZN17wxToolBarToolBase12SetShortHelpERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxToolBarToolBase12SetShortHelpERK8wxString"); return 0; }

long ZN17wxToolBarToolBase6EnableEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxToolBarToolBase6EnableEb"); return 0; }

long ZN17wxToolBarToolBase6ToggleEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxToolBarToolBase6ToggleEb"); return 0; }

long ZN17wxToolBarToolBase9SetToggleEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxToolBarToolBase9SetToggleEb"); return 0; }

long ZN17wxVScrolledWindow10RefreshAllEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxVScrolledWindow10RefreshAllEv"); return 0; }

long ZN17wxVScrolledWindow11RefreshLineEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxVScrolledWindow11RefreshLineEm"); return 0; }

long ZN17wxVScrolledWindow11ScrollLinesEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxVScrolledWindow11ScrollLinesEi"); return 0; }

long ZN17wxVScrolledWindow11ScrollPagesEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxVScrolledWindow11ScrollPagesEi"); return 0; }

long ZN17wxVScrolledWindow12RefreshLinesEmm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxVScrolledWindow12RefreshLinesEmm"); return 0; }

long ZN17wxVScrolledWindow12ScrollToLineEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxVScrolledWindow12ScrollToLineEm"); return 0; }

long ZN17wxVScrolledWindow12SetLineCountEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxVScrolledWindow12SetLineCountEm"); return 0; }

long ZN17wxVScrolledWindow12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxVScrolledWindow12ms_classInfoE"); return 0; }

long ZN17wxVScrolledWindow19FindFirstFromBottomEmb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxVScrolledWindow19FindFirstFromBottomEmb"); return 0; }

long ZN17wxVScrolledWindow4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxVScrolledWindow4InitEv"); return 0; }

long ZN17wxVScrolledWindow6LayoutEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxVScrolledWindow6LayoutEv"); return 0; }

long ZN18wxAcceleratorEntry10FromStringERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAcceleratorEntry10FromStringERK8wxString"); return 0; }

long ZN18wxAcceleratorEntry6CreateERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAcceleratorEntry6CreateERK8wxString"); return 0; }

long ZN18wxAcceleratorTableC1EiPK18wxAcceleratorEntry(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAcceleratorTableC1EiPK18wxAcceleratorEntry"); return 0; }

long ZN18wxArchiveFSHandlerC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxArchiveFSHandlerC1Ev"); return 0; }

long ZN18wxAuiDefaultTabArt10DrawButtonER4wxDCP8wxWindowRK6wxRectiiiPS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAuiDefaultTabArt10DrawButtonER4wxDCP8wxWindowRK6wxRectiiiPS4_"); return 0; }

long ZN18wxAuiDefaultTabArt10GetTabSizeER4wxDCP8wxWindowRK8wxStringRK8wxBitmapbiPi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAuiDefaultTabArt10GetTabSizeER4wxDCP8wxWindowRK8wxStringRK8wxBitmapbiPi"); return 0; }

long ZN18wxAuiDefaultTabArt12ShowDropDownEP8wxWindowRK22wxAuiNotebookPageArrayi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAuiDefaultTabArt12ShowDropDownEP8wxWindowRK22wxAuiNotebookPageArrayi"); return 0; }

long ZN18wxAuiDefaultTabArt13GetIndentSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAuiDefaultTabArt13GetIndentSizeEv"); return 0; }

long ZN18wxAuiDefaultTabArt13SetNormalFontERK6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAuiDefaultTabArt13SetNormalFontERK6wxFont"); return 0; }

long ZN18wxAuiDefaultTabArt13SetSizingInfoERK6wxSizem(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAuiDefaultTabArt13SetSizingInfoERK6wxSizem"); return 0; }

long ZN18wxAuiDefaultTabArt14DrawBackgroundER4wxDCP8wxWindowRK6wxRect(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAuiDefaultTabArt14DrawBackgroundER4wxDCP8wxWindowRK6wxRect"); return 0; }

long ZN18wxAuiDefaultTabArt15SetSelectedFontERK6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAuiDefaultTabArt15SetSelectedFontERK6wxFont"); return 0; }

long ZN18wxAuiDefaultTabArt16SetMeasuringFontERK6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAuiDefaultTabArt16SetMeasuringFontERK6wxFont"); return 0; }

long ZN18wxAuiDefaultTabArt18GetBestTabCtrlSizeEP8wxWindowRK22wxAuiNotebookPageArrayRK6wxSize(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAuiDefaultTabArt18GetBestTabCtrlSizeEP8wxWindowRK22wxAuiNotebookPageArrayRK6wxSize"); return 0; }

long ZN18wxAuiDefaultTabArt5CloneEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAuiDefaultTabArt5CloneEv"); return 0; }

long ZN18wxAuiDefaultTabArt7DrawTabER4wxDCP8wxWindowRK17wxAuiNotebookPageRK6wxRectiPS7_SA_Pi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAuiDefaultTabArt7DrawTabER4wxDCP8wxWindowRK17wxAuiNotebookPageRK6wxRectiPS7_SA_Pi"); return 0; }

long ZN18wxAuiDefaultTabArt8SetFlagsEj(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAuiDefaultTabArt8SetFlagsEj"); return 0; }

long ZN18wxAuiDefaultTabArtC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAuiDefaultTabArtC1Ev"); return 0; }

long ZN18wxAuiDefaultTabArtC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAuiDefaultTabArtC2Ev"); return 0; }

long ZN18wxAuiDefaultTabArtD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAuiDefaultTabArtD2Ev"); return 0; }

long ZN18wxAuiFloatingFrame13SetPaneWindowERK13wxAuiPaneInfo(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAuiFloatingFrame13SetPaneWindowERK13wxAuiPaneInfo"); return 0; }

long ZN18wxAuiFloatingFrameC1EP8wxWindowP12wxAuiManagerRK13wxAuiPaneInfoil(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAuiFloatingFrameC1EP8wxWindowP12wxAuiManagerRK13wxAuiPaneInfoil"); return 0; }

long ZN18wxAuiMDIChildFrame10OnActivateER15wxActivateEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAuiMDIChildFrame10OnActivateER15wxActivateEvent"); return 0; }

long ZN18wxAuiMDIChildFrame13OnCloseWindowER12wxCloseEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAuiMDIChildFrame13OnCloseWindowER12wxCloseEvent"); return 0; }

long ZN18wxAuiMDIChildFrame15OnMenuHighlightER11wxMenuEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAuiMDIChildFrame15OnMenuHighlightER11wxMenuEvent"); return 0; }

long ZN18wxAuiMDIChildFrame17SetMDIParentFrameEP19wxAuiMDIParentFrame(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAuiMDIChildFrame17SetMDIParentFrameEP19wxAuiMDIParentFrame"); return 0; }

long ZN18wxAuiMDIChildFrame22ApplyMDIChildFrameRectEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAuiMDIChildFrame22ApplyMDIChildFrameRectEv"); return 0; }

long ZN18wxAuiMDIChildFrame6CreateEP19wxAuiMDIParentFrameiRK8wxStringRK7wxPointRK6wxSizelS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAuiMDIChildFrame6CreateEP19wxAuiMDIParentFrameiRK8wxStringRK7wxPointRK6wxSizelS4_"); return 0; }

long ZN18wxAuiMDIChildFrame6DoShowEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAuiMDIChildFrame6DoShowEb"); return 0; }

long ZN18wxAuiMDIChildFrameC1EP19wxAuiMDIParentFrameiRK8wxStringRK7wxPointRK6wxSizelS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAuiMDIChildFrameC1EP19wxAuiMDIParentFrameiRK8wxStringRK7wxPointRK6wxSizelS4_"); return 0; }

long ZN18wxAuiMDIChildFrameC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxAuiMDIChildFrameC1Ev"); return 0; }

long ZN18wxBaseArrayPtrVoid3AddEPKvm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxBaseArrayPtrVoid3AddEPKvm"); return 0; }

long ZN18wxBaseArrayPtrVoid4SortEPFiPKvS1_E(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxBaseArrayPtrVoid4SortEPFiPKvS1_E"); return 0; }

long ZN18wxBaseArrayPtrVoid5ClearEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxBaseArrayPtrVoid5ClearEv"); return 0; }

long ZN18wxBaseArrayPtrVoid6InsertEPKvmm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxBaseArrayPtrVoid6InsertEPKvmm"); return 0; }

long ZN18wxBaseArrayPtrVoid8RemoveAtEmm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxBaseArrayPtrVoid8RemoveAtEmm"); return 0; }

long ZN18wxBaseArrayPtrVoidC2ERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxBaseArrayPtrVoidC2ERKS_"); return 0; }

long ZN18wxBaseArrayPtrVoidC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxBaseArrayPtrVoidC2Ev"); return 0; }

long ZN18wxBaseArrayPtrVoidD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxBaseArrayPtrVoidD2Ev"); return 0; }

long ZN18wxBaseArrayPtrVoidaSERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxBaseArrayPtrVoidaSERKS_"); return 0; }

long ZN18wxBitmapDataObject7SetDataERK12wxDataFormatmPKv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxBitmapDataObject7SetDataERK12wxDataFormatmPKv"); return 0; }

long ZN18wxBitmapDataObject7SetDataEmPKv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxBitmapDataObject7SetDataEmPKv"); return 0; }

long ZN18wxBitmapDataObjectC1ERK8wxBitmap(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxBitmapDataObjectC1ERK8wxBitmap"); return 0; }

long ZN18wxBitmapDataObjectC2ERK8wxBitmap(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxBitmapDataObjectC2ERK8wxBitmap"); return 0; }

long ZN18wxBitmapDataObjectD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxBitmapDataObjectD2Ev"); return 0; }

long ZN18wxColourPickerCtrl6CreateEP8wxWindowiRK8wxColourRK7wxPointRK6wxSizelRK11wxValidatorRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxColourPickerCtrl6CreateEP8wxWindowiRK8wxColourRK7wxPointRK6wxSizelRK11wxValidatorRK8wxString"); return 0; }

long ZN18wxColourPickerCtrl9SetColourERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxColourPickerCtrl9SetColourERK8wxColour"); return 0; }

long ZN18wxControlContainerC1EP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxControlContainerC1EP8wxWindow"); return 0; }

long ZN18wxCustomDataObjectC1ERK12wxDataFormat(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxCustomDataObjectC1ERK12wxDataFormat"); return 0; }

long ZN18wxGenericComboCtrl19SetCustomPaintWidthEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxGenericComboCtrl19SetCustomPaintWidthEi"); return 0; }

long ZN18wxGenericComboCtrl4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxGenericComboCtrl4InitEv"); return 0; }

long ZN18wxGenericComboCtrl6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxGenericComboCtrl6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_"); return 0; }

long ZN18wxGenericComboCtrl8OnResizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxGenericComboCtrl8OnResizeEv"); return 0; }

long ZN18wxGenericComboCtrlD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxGenericComboCtrlD2Ev"); return 0; }

long ZN18wxGenericDragImage4HideEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxGenericDragImage4HideEv"); return 0; }

long ZN18wxGenericDragImage4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxGenericDragImage4InitEv"); return 0; }

long ZN18wxGenericDragImage4MoveERK7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxGenericDragImage4MoveERK7wxPoint"); return 0; }

long ZN18wxGenericDragImage4ShowEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxGenericDragImage4ShowEv"); return 0; }

long ZN18wxGenericDragImage6CreateERK10wxListCtrll(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxGenericDragImage6CreateERK10wxListCtrll"); return 0; }

long ZN18wxGenericDragImage6CreateERK10wxTreeCtrlR12wxTreeItemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxGenericDragImage6CreateERK10wxTreeCtrlR12wxTreeItemId"); return 0; }

long ZN18wxGenericDragImage6CreateERK6wxIconRK8wxCursor(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxGenericDragImage6CreateERK6wxIconRK8wxCursor"); return 0; }

long ZN18wxGenericDragImage6CreateERK8wxBitmapRK8wxCursor(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxGenericDragImage6CreateERK8wxBitmapRK8wxCursor"); return 0; }

long ZN18wxGenericDragImage6CreateERK8wxStringRK8wxCursor(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxGenericDragImage6CreateERK8wxStringRK8wxCursor"); return 0; }

long ZN18wxGenericDragImage7EndDragEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxGenericDragImage7EndDragEv"); return 0; }

long ZN18wxGenericDragImage9BeginDragERK7wxPointP8wxWindowS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxGenericDragImage9BeginDragERK7wxPointP8wxWindowS4_"); return 0; }

long ZN18wxGenericDragImage9BeginDragERK7wxPointP8wxWindowbP6wxRect(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxGenericDragImage9BeginDragERK7wxPointP8wxWindowbP6wxRect"); return 0; }

long ZN18wxGraphicsRenderer18GetDefaultRendererEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxGraphicsRenderer18GetDefaultRendererEv"); return 0; }

long ZN18wxGridTableMessageC1EP15wxGridTableBaseiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxGridTableMessageC1EP15wxGridTableBaseiii"); return 0; }

long ZN18wxHtmlEasyPrinting11PreviewFileERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxHtmlEasyPrinting11PreviewFileERK8wxString"); return 0; }

long ZN18wxHtmlEasyPrinting11PreviewTextERK8wxStringS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxHtmlEasyPrinting11PreviewTextERK8wxStringS2_"); return 0; }

long ZN18wxHtmlEasyPrinting12GetPrintDataEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxHtmlEasyPrinting12GetPrintDataEv"); return 0; }

long ZN18wxHtmlEasyPrinting16SetStandardFontsEiRK8wxStringS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxHtmlEasyPrinting16SetStandardFontsEiRK8wxStringS2_"); return 0; }

long ZN18wxHtmlEasyPrinting8SetFontsERK8wxStringS2_PKi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxHtmlEasyPrinting8SetFontsERK8wxStringS2_PKi"); return 0; }

long ZN18wxHtmlEasyPrinting9PageSetupEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxHtmlEasyPrinting9PageSetupEv"); return 0; }

long ZN18wxHtmlEasyPrinting9PrintFileERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxHtmlEasyPrinting9PrintFileERK8wxString"); return 0; }

long ZN18wxHtmlEasyPrinting9PrintTextERK8wxStringS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxHtmlEasyPrinting9PrintTextERK8wxStringS2_"); return 0; }

long ZN18wxHtmlEasyPrinting9SetFooterERK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxHtmlEasyPrinting9SetFooterERK8wxStringi"); return 0; }

long ZN18wxHtmlEasyPrinting9SetHeaderERK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxHtmlEasyPrinting9SetHeaderERK8wxStringi"); return 0; }

long ZN18wxHtmlEasyPrintingC1ERK8wxStringP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxHtmlEasyPrintingC1ERK8wxStringP8wxWindow"); return 0; }

long ZN18wxHtmlSearchStatus6SearchEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxHtmlSearchStatus6SearchEv"); return 0; }

long ZN18wxMimeTypesManager10InitializeEiRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxMimeTypesManager10InitializeEiRK8wxString"); return 0; }

long ZN18wxMimeTypesManager11ReadMailcapERK8wxStringb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxMimeTypesManager11ReadMailcapERK8wxStringb"); return 0; }

long ZN18wxMimeTypesManager11UnassociateEP10wxFileType(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxMimeTypesManager11UnassociateEP10wxFileType"); return 0; }

long ZN18wxMimeTypesManager13ReadMimeTypesERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxMimeTypesManager13ReadMimeTypesERK8wxString"); return 0; }

long ZN18wxMimeTypesManager16EnumAllFileTypesER13wxArrayString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxMimeTypesManager16EnumAllFileTypesER13wxArrayString"); return 0; }

long ZN18wxMimeTypesManager23GetFileTypeFromMimeTypeERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxMimeTypesManager23GetFileTypeFromMimeTypeERK8wxString"); return 0; }

long ZN18wxMimeTypesManager24GetFileTypeFromExtensionERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxMimeTypesManager24GetFileTypeFromExtensionERK8wxString"); return 0; }

long ZN18wxMimeTypesManager8IsOfTypeERK8wxStringS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxMimeTypesManager8IsOfTypeERK8wxStringS2_"); return 0; }

long ZN18wxMimeTypesManager9AssociateERK14wxFileTypeInfo(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxMimeTypesManager9AssociateERK14wxFileTypeInfo"); return 0; }

long ZN18wxMimeTypesManager9ClearDataEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxMimeTypesManager9ClearDataEv"); return 0; }

long ZN18wxMimeTypesManagerC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxMimeTypesManagerC1Ev"); return 0; }

long ZN18wxMimeTypesManagerD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxMimeTypesManagerD1Ev"); return 0; }

long ZN18wxPrintPreviewBase9CalcRectsEP15wxPreviewCanvasR6wxRectS3_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxPrintPreviewBase9CalcRectsEP15wxPreviewCanvasR6wxRectS3_"); return 0; }

long ZN18wxRichTextPrinting11PreviewFileERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxRichTextPrinting11PreviewFileERK8wxString"); return 0; }

long ZN18wxRichTextPrinting11PrintBufferERK16wxRichTextBuffer(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxRichTextPrinting11PrintBufferERK16wxRichTextBuffer"); return 0; }

long ZN18wxRichTextPrinting12GetPrintDataEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxRichTextPrinting12GetPrintDataEv"); return 0; }

long ZN18wxRichTextPrinting12SetPrintDataERK11wxPrintData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxRichTextPrinting12SetPrintDataERK11wxPrintData"); return 0; }

long ZN18wxRichTextPrinting13PreviewBufferERK16wxRichTextBuffer(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxRichTextPrinting13PreviewBufferERK16wxRichTextBuffer"); return 0; }

long ZN18wxRichTextPrinting13SetFooterTextERK8wxString21wxRichTextOddEvenPage22wxRichTextPageLocation(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxRichTextPrinting13SetFooterTextERK8wxString21wxRichTextOddEvenPage22wxRichTextPageLocation"); return 0; }

long ZN18wxRichTextPrinting13SetHeaderTextERK8wxString21wxRichTextOddEvenPage22wxRichTextPageLocation(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxRichTextPrinting13SetHeaderTextERK8wxString21wxRichTextOddEvenPage22wxRichTextPageLocation"); return 0; }

long ZN18wxRichTextPrinting16SetPageSetupDataERK21wxPageSetupDialogData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxRichTextPrinting16SetPageSetupDataERK21wxPageSetupDialogData"); return 0; }

long ZN18wxRichTextPrinting24SetRichTextBufferPreviewEP16wxRichTextBuffer(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxRichTextPrinting24SetRichTextBufferPreviewEP16wxRichTextBuffer"); return 0; }

long ZN18wxRichTextPrinting25SetRichTextBufferPrintingEP16wxRichTextBuffer(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxRichTextPrinting25SetRichTextBufferPrintingEP16wxRichTextBuffer"); return 0; }

long ZN18wxRichTextPrinting9PageSetupEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxRichTextPrinting9PageSetupEv"); return 0; }

long ZN18wxRichTextPrinting9PrintFileERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxRichTextPrinting9PrintFileERK8wxString"); return 0; }

long ZN18wxRichTextPrintingC1ERK8wxStringP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxRichTextPrintingC1ERK8wxStringP8wxWindow"); return 0; }

long ZN18wxRichTextPrintout10SetMarginsEiiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxRichTextPrintout10SetMarginsEiiii"); return 0; }

long ZN18wxRichTextPrintout16CalculateScalingEP4wxDCR6wxRectS3_S3_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxRichTextPrintout16CalculateScalingEP4wxDCR6wxRectS3_S3_"); return 0; }

long ZN18wxRichTextPrintoutC1ERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxRichTextPrintoutC1ERK8wxString"); return 0; }

long ZN18wxSashLayoutWindow4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxSashLayoutWindow4InitEv"); return 0; }

long ZN18wxSashLayoutWindow6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxSashLayoutWindow6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN18wxStaticBitmapBaseD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxStaticBitmapBaseD2Ev"); return 0; }

long ZN18wxTextOutputStreamC1ER14wxOutputStream5wxEOLRK8wxMBConv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxTextOutputStreamC1ER14wxOutputStream5wxEOLRK8wxMBConv"); return 0; }

long ZN18wxTextOutputStreamD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN18wxTextOutputStreamD1Ev"); return 0; }

long ZN19wxArrayFileTypeInfo3AddERK14wxFileTypeInfom(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxArrayFileTypeInfo3AddERK14wxFileTypeInfom"); return 0; }

long ZN19wxAuiDefaultDockArt10DrawBorderER4wxDCP8wxWindowRK6wxRectR13wxAuiPaneInfo(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxAuiDefaultDockArt10DrawBorderER4wxDCP8wxWindowRK6wxRectR13wxAuiPaneInfo"); return 0; }

long ZN19wxAuiDefaultDockArt11DrawCaptionER4wxDCP8wxWindowRK8wxStringRK6wxRectR13wxAuiPaneInfo(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxAuiDefaultDockArt11DrawCaptionER4wxDCP8wxWindowRK8wxStringRK6wxRectR13wxAuiPaneInfo"); return 0; }

long ZN19wxAuiDefaultDockArt11DrawGripperER4wxDCP8wxWindowRK6wxRectR13wxAuiPaneInfo(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxAuiDefaultDockArt11DrawGripperER4wxDCP8wxWindowRK6wxRectR13wxAuiPaneInfo"); return 0; }

long ZN19wxAuiDefaultDockArt14DrawBackgroundER4wxDCP8wxWindowiRK6wxRect(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxAuiDefaultDockArt14DrawBackgroundER4wxDCP8wxWindowiRK6wxRect"); return 0; }

long ZN19wxAuiDefaultDockArt14DrawPaneButtonER4wxDCP8wxWindowiiRK6wxRectR13wxAuiPaneInfo(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxAuiDefaultDockArt14DrawPaneButtonER4wxDCP8wxWindowiiRK6wxRectR13wxAuiPaneInfo"); return 0; }

long ZN19wxAuiDefaultDockArt7GetFontEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxAuiDefaultDockArt7GetFontEi"); return 0; }

long ZN19wxAuiDefaultDockArt7SetFontEiRK6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxAuiDefaultDockArt7SetFontEiRK6wxFont"); return 0; }

long ZN19wxAuiDefaultDockArt8DrawSashER4wxDCP8wxWindowiRK6wxRect(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxAuiDefaultDockArt8DrawSashER4wxDCP8wxWindowiRK6wxRect"); return 0; }

long ZN19wxAuiDefaultDockArt9GetColourEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxAuiDefaultDockArt9GetColourEi"); return 0; }

long ZN19wxAuiDefaultDockArt9GetMetricEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxAuiDefaultDockArt9GetMetricEi"); return 0; }

long ZN19wxAuiDefaultDockArt9SetColourEiRK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxAuiDefaultDockArt9SetColourEiRK8wxColour"); return 0; }

long ZN19wxAuiDefaultDockArt9SetMetricEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxAuiDefaultDockArt9SetMetricEii"); return 0; }

long ZN19wxAuiDefaultDockArtC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxAuiDefaultDockArtC1Ev"); return 0; }

long ZN19wxAuiDefaultDockArtC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxAuiDefaultDockArtC2Ev"); return 0; }

long ZN19wxAuiMDIParentFrame13SetWindowMenuEP6wxMenu(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxAuiMDIParentFrame13SetWindowMenuEP6wxMenu"); return 0; }

long ZN19wxAuiMDIParentFrame14GetArtProviderEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxAuiMDIParentFrame14GetArtProviderEv"); return 0; }

long ZN19wxAuiMDIParentFrame14SetActiveChildEP18wxAuiMDIChildFrame(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxAuiMDIParentFrame14SetActiveChildEP18wxAuiMDIChildFrame"); return 0; }

long ZN19wxAuiMDIParentFrame14SetArtProviderEP11wxAuiTabArt(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxAuiMDIParentFrame14SetArtProviderEP11wxAuiTabArt"); return 0; }

long ZN19wxAuiMDIParentFrame15SetChildMenuBarEP18wxAuiMDIChildFrame(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxAuiMDIParentFrame15SetChildMenuBarEP18wxAuiMDIChildFrame"); return 0; }

long ZN19wxAuiMDIParentFrame6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxAuiMDIParentFrame6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_"); return 0; }

long ZN19wxAuiMDIParentFrameC1EP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxAuiMDIParentFrameC1EP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_"); return 0; }

long ZN19wxAuiMDIParentFrameC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxAuiMDIParentFrameC1Ev"); return 0; }

long ZN19wxConfigPathChangerC1EPK12wxConfigBaseRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxConfigPathChangerC1EPK12wxConfigBaseRK8wxString"); return 0; }

long ZN19wxConfigPathChangerD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxConfigPathChangerD1Ev"); return 0; }

long ZN19wxContextHelpButtonC1EP8wxWindowiRK7wxPointRK6wxSizel(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxContextHelpButtonC1EP8wxWindowiRK7wxPointRK6wxSizel"); return 0; }

long ZN19wxDirFilterListCtrl14FillFilterListERK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxDirFilterListCtrl14FillFilterListERK8wxStringi"); return 0; }

long ZN19wxDirFilterListCtrl4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxDirFilterListCtrl4InitEv"); return 0; }

long ZN19wxDirFilterListCtrl6CreateEP16wxGenericDirCtrliRK7wxPointRK6wxSizel(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxDirFilterListCtrl6CreateEP16wxGenericDirCtrliRK7wxPointRK6wxSizel"); return 0; }

long ZN19wxEncodingConverter17GetAllEquivalentsE14wxFontEncoding(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxEncodingConverter17GetAllEquivalentsE14wxFontEncoding"); return 0; }

long ZN19wxEncodingConverter22GetPlatformEquivalentsE14wxFontEncodingi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxEncodingConverter22GetPlatformEquivalentsE14wxFontEncodingi"); return 0; }

long ZN19wxEncodingConverter4InitE14wxFontEncodingS0_i(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxEncodingConverter4InitE14wxFontEncodingS0_i"); return 0; }

long ZN19wxEncodingConverterC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxEncodingConverterC1Ev"); return 0; }

long ZN19wxFileSystemHandler18GetMimeTypeFromExtERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxFileSystemHandler18GetMimeTypeFromExtERK8wxString"); return 0; }

long ZN19wxFindReplaceDialog12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxFindReplaceDialog12ms_classInfoE"); return 0; }

long ZN19wxFindReplaceDialog4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxFindReplaceDialog4InitEv"); return 0; }

long ZN19wxFindReplaceDialog6CreateEP8wxWindowP17wxFindReplaceDataRK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxFindReplaceDialog6CreateEP8wxWindowP17wxFindReplaceDataRK8wxStringi"); return 0; }

long ZN19wxHtmlContainerCell10InsertCellEP10wxHtmlCell(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxHtmlContainerCell10InsertCellEP10wxHtmlCell"); return 0; }

long ZN19wxHtmlContainerCell13SetWidthFloatERK9wxHtmlTagd(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxHtmlContainerCell13SetWidthFloatERK9wxHtmlTagd"); return 0; }

long ZN19wxHtmlContainerCell19GetBackgroundColourEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxHtmlContainerCell19GetBackgroundColourEv"); return 0; }

long ZN19wxHtmlContainerCell8SetAlignERK9wxHtmlTag(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxHtmlContainerCell8SetAlignERK9wxHtmlTag"); return 0; }

long ZN19wxHtmlContainerCell9SetIndentEiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxHtmlContainerCell9SetIndentEiii"); return 0; }

long ZN19wxHtmlContainerCellC1EPS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxHtmlContainerCellC1EPS_"); return 0; }

long ZN19wxHtmlWinTagHandler12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxHtmlWinTagHandler12ms_classInfoE"); return 0; }

long ZN19wxLayoutConstraints18SatisfyConstraintsEP12wxWindowBasePi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxLayoutConstraints18SatisfyConstraintsEP12wxWindowBasePi"); return 0; }

long ZN19wxLayoutConstraintsC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxLayoutConstraintsC1Ev"); return 0; }

long ZN19wxMultiChoiceDialog13SetSelectionsERK10wxArrayInt(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxMultiChoiceDialog13SetSelectionsERK10wxArrayInt"); return 0; }

long ZN19wxMultiChoiceDialog6CreateEP8wxWindowRK8wxStringS4_iPS3_lRK7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxMultiChoiceDialog6CreateEP8wxWindowRK8wxStringS4_iPS3_lRK7wxPoint"); return 0; }

long ZN19wxNumberEntryDialogC1EP8wxWindowRK8wxStringS4_S4_lllRK7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxNumberEntryDialogC1EP8wxWindowRK8wxStringS4_S4_lllRK7wxPoint"); return 0; }

long ZN19wxPreviewControlBar10OnPreviousEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxPreviewControlBar10OnPreviousEv"); return 0; }

long ZN19wxPreviewControlBar12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxPreviewControlBar12ms_classInfoE"); return 0; }

long ZN19wxPreviewControlBar13CreateButtonsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxPreviewControlBar13CreateButtonsEv"); return 0; }

long ZN19wxPreviewControlBar14GetZoomControlEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxPreviewControlBar14GetZoomControlEv"); return 0; }

long ZN19wxPreviewControlBar14SetZoomControlEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxPreviewControlBar14SetZoomControlEi"); return 0; }

long ZN19wxPreviewControlBar6OnGotoEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxPreviewControlBar6OnGotoEv"); return 0; }

long ZN19wxPreviewControlBar6OnLastEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxPreviewControlBar6OnLastEv"); return 0; }

long ZN19wxPreviewControlBar6OnNextEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxPreviewControlBar6OnNextEv"); return 0; }

long ZN19wxPreviewControlBar7OnFirstEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxPreviewControlBar7OnFirstEv"); return 0; }

long ZN19wxPreviewControlBarC1EP18wxPrintPreviewBaselP8wxWindowRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxPreviewControlBarC1EP18wxPrintPreviewBaselP8wxWindowRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN19wxPreviewControlBarC2EP18wxPrintPreviewBaselP8wxWindowRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxPreviewControlBarC2EP18wxPrintPreviewBaselP8wxWindowRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN19wxPreviewControlBarD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxPreviewControlBarD2Ev"); return 0; }

long ZN19wxRichTextParagraph10ClearLinesEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxRichTextParagraph10ClearLinesEv"); return 0; }

long ZN19wxRichTextParagraph12AllocateLineEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxRichTextParagraph12AllocateLineEi"); return 0; }

long ZN19wxRichTextParagraph13GetBulletTextEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxRichTextParagraph13GetBulletTextEv"); return 0; }

long ZN19wxRichTextParagraph14sm_defaultTabsE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxRichTextParagraph14sm_defaultTabsE"); return 0; }

long ZN19wxRichTextParagraph15InitDefaultTabsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxRichTextParagraph15InitDefaultTabsEv"); return 0; }

long ZN19wxRichTextParagraph16ClearDefaultTabsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxRichTextParagraph16ClearDefaultTabsEv"); return 0; }

long ZN19wxRichTextParagraph16ClearUnusedLinesEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxRichTextParagraph16ClearUnusedLinesEi"); return 0; }

long ZN19wxRichTextParagraph16FindWrapPositionERK15wxRichTextRangeR4wxDCiRl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxRichTextParagraph16FindWrapPositionERK15wxRichTextRangeR4wxDCiRl"); return 0; }

long ZN19wxRichTextParagraph20FindObjectAtPositionEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxRichTextParagraph20FindObjectAtPositionEl"); return 0; }

long ZN19wxRichTextParagraph22GetContiguousPlainTextER8wxStringRK15wxRichTextRangeb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxRichTextParagraph22GetContiguousPlainTextER8wxStringRK15wxRichTextRangeb"); return 0; }

long ZN19wxRichTextParagraph25GetFirstLineBreakPositionEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxRichTextParagraph25GetFirstLineBreakPositionEl"); return 0; }

long ZN19wxRichTextParagraph4CopyERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxRichTextParagraph4CopyERKS_"); return 0; }

long ZN19wxRichTextParagraphC1ERK8wxStringP16wxRichTextObjectP12wxTextAttrExS6_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxRichTextParagraphC1ERK8wxStringP16wxRichTextObjectP12wxTextAttrExS6_"); return 0; }

long ZN19wxRichTextPlainText25GetFirstLineBreakPositionEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxRichTextPlainText25GetFirstLineBreakPositionEl"); return 0; }

long ZN19wxRichTextPlainText4CopyERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxRichTextPlainText4CopyERKS_"); return 0; }

long ZN19wxRichTextPlainTextC1ERK8wxStringP16wxRichTextObjectP12wxTextAttrEx(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxRichTextPlainTextC1ERK8wxStringP16wxRichTextObjectP12wxTextAttrEx"); return 0; }

long ZN19wxSimpleHtmlListBox6CreateEP8wxWindowiRK7wxPointRK6wxSizeRK13wxArrayStringlRK11wxValidatorRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxSimpleHtmlListBox6CreateEP8wxWindowiRK7wxPointRK6wxSizeRK13wxArrayStringlRK11wxValidatorRK8wxString"); return 0; }

long ZN19wxStandardPathsBase3GetEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxStandardPathsBase3GetEv"); return 0; }

long ZN19wxTopLevelWindowMac11MacActivateElb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxTopLevelWindowMac11MacActivateElb"); return 0; }

long ZN19wxTopLevelWindowMac12DoMoveWindowEiiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxTopLevelWindowMac12DoMoveWindowEiiii"); return 0; }

long ZN19wxTopLevelWindowMac13SetExtraStyleEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxTopLevelWindowMac13SetExtraStyleEl"); return 0; }

long ZN19wxTopLevelWindowMac14SetTransparentEh(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxTopLevelWindowMac14SetTransparentEh"); return 0; }

long ZN19wxTopLevelWindowMac14ShowFullScreenEbl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxTopLevelWindowMac14ShowFullScreenEbl"); return 0; }

long ZN19wxTopLevelWindowMac15ClearBackgroundEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxTopLevelWindowMac15ClearBackgroundEv"); return 0; }

long ZN19wxTopLevelWindowMac17CanSetTransparentEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxTopLevelWindowMac17CanSetTransparentEv"); return 0; }

long ZN19wxTopLevelWindowMac17MacPerformUpdatesEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxTopLevelWindowMac17MacPerformUpdatesEv"); return 0; }

long ZN19wxTopLevelWindowMac19MacCreateRealWindowERK8wxStringRK7wxPointRK6wxSizelS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxTopLevelWindowMac19MacCreateRealWindowERK8wxStringRK7wxPointRK6wxSizelS2_"); return 0; }

long ZN19wxTopLevelWindowMac20RequestUserAttentionEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxTopLevelWindowMac20RequestUserAttentionEi"); return 0; }

long ZN19wxTopLevelWindowMac21MacSetBackgroundBrushERK7wxBrush(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxTopLevelWindowMac21MacSetBackgroundBrushERK7wxBrush"); return 0; }

long ZN19wxTopLevelWindowMac22MacGetContentAreaInsetERiS0_S0_S0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxTopLevelWindowMac22MacGetContentAreaInsetERiS0_S0_S0_"); return 0; }

long ZN19wxTopLevelWindowMac36MacInstallTopLevelWindowEventHandlerEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxTopLevelWindowMac36MacInstallTopLevelWindowEventHandlerEv"); return 0; }

long ZN19wxTopLevelWindowMac4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxTopLevelWindowMac4InitEv"); return 0; }

long ZN19wxTopLevelWindowMac4ShowEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxTopLevelWindowMac4ShowEb"); return 0; }

long ZN19wxTopLevelWindowMac5LowerEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxTopLevelWindowMac5LowerEv"); return 0; }

long ZN19wxTopLevelWindowMac5RaiseEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxTopLevelWindowMac5RaiseEv"); return 0; }

long ZN19wxTopLevelWindowMac7IconizeEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxTopLevelWindowMac7IconizeEb"); return 0; }

long ZN19wxTopLevelWindowMac7RestoreEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxTopLevelWindowMac7RestoreEv"); return 0; }

long ZN19wxTopLevelWindowMac7SetIconERK6wxIcon(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxTopLevelWindowMac7SetIconERK6wxIcon"); return 0; }

long ZN19wxTopLevelWindowMac8DoCentreEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxTopLevelWindowMac8DoCentreEi"); return 0; }

long ZN19wxTopLevelWindowMac8MaximizeEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxTopLevelWindowMac8MaximizeEb"); return 0; }

long ZN19wxTopLevelWindowMac8SetShapeERK8wxRegion(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxTopLevelWindowMac8SetShapeERK8wxRegion"); return 0; }

long ZN19wxTopLevelWindowMac8SetTitleERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxTopLevelWindowMac8SetTitleERK8wxString"); return 0; }

long ZN19wxTopLevelWindowMacD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxTopLevelWindowMacD2Ev"); return 0; }

long ZN19wxWindowCreateEventC1EP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxWindowCreateEventC1EP8wxWindow"); return 0; }

long ZN20wxAuiMDIClientWindowC1EP19wxAuiMDIParentFramel(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxAuiMDIClientWindowC1EP19wxAuiMDIParentFramel"); return 0; }

long ZN20wxAuiMDIClientWindowC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxAuiMDIClientWindowC1Ev"); return 0; }

long ZN20wxAuiPaneButtonArrayC1ERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxAuiPaneButtonArrayC1ERKS_"); return 0; }

long ZN20wxAuiPaneButtonArrayD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxAuiPaneButtonArrayD1Ev"); return 0; }

long ZN20wxAuiPaneButtonArrayaSERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxAuiPaneButtonArrayaSERKS_"); return 0; }

long ZN20wxGridCellBoolEditor11IsTrueValueERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxGridCellBoolEditor11IsTrueValueERK8wxString"); return 0; }

long ZN20wxGridCellBoolEditor15UseStringValuesERK8wxStringS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxGridCellBoolEditor15UseStringValuesERK8wxStringS2_"); return 0; }

long ZN20wxGridCellEnumEditorC1ERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxGridCellEnumEditorC1ERK8wxString"); return 0; }

long ZN20wxGridCellTextEditorC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxGridCellTextEditorC1Ev"); return 0; }

long ZN20wxGridCellTextEditorC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxGridCellTextEditorC2Ev"); return 0; }

long ZN20wxHtmlHelpController12DisplayIndexEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxHtmlHelpController12DisplayIndexEv"); return 0; }

long ZN20wxHtmlHelpController13SetHelpWindowEP16wxHtmlHelpWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxHtmlHelpController13SetHelpWindowEP16wxHtmlHelpWindow"); return 0; }

long ZN20wxHtmlHelpController14SetTitleFormatERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxHtmlHelpController14SetTitleFormatERK8wxString"); return 0; }

long ZN20wxHtmlHelpController17MakeModalIfNeededEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxHtmlHelpController17MakeModalIfNeededEv"); return 0; }

long ZN20wxHtmlHelpController18FindTopLevelWindowEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxHtmlHelpController18FindTopLevelWindowEv"); return 0; }

long ZN20wxHtmlHelpController7AddBookERK8wxStringb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxHtmlHelpController7AddBookERK8wxStringb"); return 0; }

long ZN20wxHtmlHelpController7DisplayERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxHtmlHelpController7DisplayERK8wxString"); return 0; }

long ZN20wxHtmlHelpController7DisplayEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxHtmlHelpController7DisplayEi"); return 0; }

long ZN20wxHtmlHelpController9UseConfigEP12wxConfigBaseRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxHtmlHelpController9UseConfigEP12wxConfigBaseRK8wxString"); return 0; }

long ZN20wxHtmlHelpControllerC1EiP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxHtmlHelpControllerC1EiP8wxWindow"); return 0; }

long ZN20wxNativeEncodingInfo10FromStringERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxNativeEncodingInfo10FromStringERK8wxString"); return 0; }

long ZN20wxOwnerDrawnComboBox17DoSetPopupControlEP12wxComboPopup(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxOwnerDrawnComboBox17DoSetPopupControlEP12wxComboPopup"); return 0; }

long ZN20wxOwnerDrawnComboBox19DoSetItemClientDataEjPv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxOwnerDrawnComboBox19DoSetItemClientDataEjPv"); return 0; }

long ZN20wxOwnerDrawnComboBox21DoSetItemClientObjectEjP12wxClientData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxOwnerDrawnComboBox21DoSetItemClientObjectEjP12wxClientData"); return 0; }

long ZN20wxOwnerDrawnComboBox4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxOwnerDrawnComboBox4InitEv"); return 0; }

long ZN20wxOwnerDrawnComboBox5ClearEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxOwnerDrawnComboBox5ClearEv"); return 0; }

long ZN20wxOwnerDrawnComboBox6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizeRK13wxArrayStringlRK11wxValidatorS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxOwnerDrawnComboBox6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizeRK13wxArrayStringlRK11wxValidatorS4_"); return 0; }

long ZN20wxOwnerDrawnComboBox6DeleteEj(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxOwnerDrawnComboBox6DeleteEj"); return 0; }

long ZN20wxOwnerDrawnComboBox6SelectEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxOwnerDrawnComboBox6SelectEi"); return 0; }

long ZN20wxOwnerDrawnComboBox8DoAppendERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxOwnerDrawnComboBox8DoAppendERK8wxString"); return 0; }

long ZN20wxOwnerDrawnComboBox8DoInsertERK8wxStringj(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxOwnerDrawnComboBox8DoInsertERK8wxStringj"); return 0; }

long ZN20wxOwnerDrawnComboBox9SetStringEjRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxOwnerDrawnComboBox9SetStringEjRK8wxString"); return 0; }

long ZN20wxOwnerDrawnComboBoxC2EP8wxWindowiRK8wxStringRK7wxPointRK6wxSizeRK13wxArrayStringlRK11wxValidatorS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxOwnerDrawnComboBoxC2EP8wxWindowiRK8wxStringRK7wxPointRK6wxSizeRK13wxArrayStringlRK11wxValidatorS4_"); return 0; }

long ZN20wxOwnerDrawnComboBoxD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxOwnerDrawnComboBoxD2Ev"); return 0; }

long ZN20wxRichTextImageBlockC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxRichTextImageBlockC1Ev"); return 0; }

long ZN20wxSingleChoiceDialog12SetSelectionEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxSingleChoiceDialog12SetSelectionEi"); return 0; }

long ZN20wxSingleChoiceDialogC1EP8wxWindowRK8wxStringS4_iPS3_PPclRK7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxSingleChoiceDialogC1EP8wxWindowRK8wxStringS4_iPS3_PPclRK7wxPoint"); return 0; }

long ZN20wxSplashScreenWindowC1ERK8wxBitmapP8wxWindowiRK7wxPointRK6wxSizel(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxSplashScreenWindowC1ERK8wxBitmapP8wxWindowiRK7wxPointRK6wxSizel"); return 0; }

long ZN20wxStringOutputStreamD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxStringOutputStreamD1Ev"); return 0; }

long ZN20wxTopLevelWindowBase10SetMaxSizeERK6wxSize(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxTopLevelWindowBase10SetMaxSizeERK6wxSize"); return 0; }

long ZN20wxTopLevelWindowBase10SetMinSizeERK6wxSize(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxTopLevelWindowBase10SetMinSizeERK6wxSize"); return 0; }

long ZN20wxTopLevelWindowBase11RemoveChildEP12wxWindowBase(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxTopLevelWindowBase11RemoveChildEP12wxWindowBase"); return 0; }

long ZN20wxTopLevelWindowBase14DoSetSizeHintsEiiiiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxTopLevelWindowBase14DoSetSizeHintsEiiiiii"); return 0; }

long ZN20wxTopLevelWindowBase16DoUpdateWindowUIER15wxUpdateUIEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxTopLevelWindowBase16DoUpdateWindowUIER15wxUpdateUIEvent"); return 0; }

long ZN20wxTopLevelWindowBase26GetRectForTopLevelChildrenEPiS0_S0_S0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxTopLevelWindowBase26GetRectForTopLevelChildrenEPiS0_S0_S0_"); return 0; }

long ZN20wxTopLevelWindowBase7DestroyEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxTopLevelWindowBase7DestroyEv"); return 0; }

long ZN20wxTopLevelWindowBaseC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxTopLevelWindowBaseC2Ev"); return 0; }

long ZN20wxTopLevelWindowBaseD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxTopLevelWindowBaseD2Ev"); return 0; }

long ZN20wxVListBoxComboPopup10CalcWidthsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxVListBoxComboPopup10CalcWidthsEv"); return 0; }

long ZN20wxWindowDestroyEventC1EP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxWindowDestroyEventC1EP8wxWindow"); return 0; }

long ZN20wxXmlResourceHandler11GetPositionERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxXmlResourceHandler11GetPositionERK8wxString"); return 0; }

long ZN20wxXmlResourceHandler11SetupWindowEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxXmlResourceHandler11SetupWindowEP8wxWindow"); return 0; }

long ZN20wxXmlResourceHandler12GetAnimationERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxXmlResourceHandler12GetAnimationERK8wxString"); return 0; }

long ZN20wxXmlResourceHandler12GetDimensionERK8wxStringiP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxXmlResourceHandler12GetDimensionERK8wxStringiP8wxWindow"); return 0; }

long ZN20wxXmlResourceHandler12GetParamNodeERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxXmlResourceHandler12GetParamNodeERK8wxString"); return 0; }

long ZN20wxXmlResourceHandler13GetParamValueERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxXmlResourceHandler13GetParamValueERK8wxString"); return 0; }

long ZN20wxXmlResourceHandler14CreateChildrenEP8wxObjectb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxXmlResourceHandler14CreateChildrenEP8wxObjectb"); return 0; }

long ZN20wxXmlResourceHandler14CreateResourceEP9wxXmlNodeP8wxObjectS3_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxXmlResourceHandler14CreateResourceEP9wxXmlNodeP8wxObjectS3_"); return 0; }

long ZN20wxXmlResourceHandler14GetNodeContentEP9wxXmlNode(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxXmlResourceHandler14GetNodeContentEP9wxXmlNode"); return 0; }

long ZN20wxXmlResourceHandler15AddWindowStylesEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxXmlResourceHandler15AddWindowStylesEv"); return 0; }

long ZN20wxXmlResourceHandler23CreateChildrenPrivatelyEP8wxObjectP9wxXmlNode(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxXmlResourceHandler23CreateChildrenPrivatelyEP8wxObjectP9wxXmlNode"); return 0; }

long ZN20wxXmlResourceHandler5GetIDEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxXmlResourceHandler5GetIDEv"); return 0; }

long ZN20wxXmlResourceHandler7GetBoolERK8wxStringb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxXmlResourceHandler7GetBoolERK8wxStringb"); return 0; }

long ZN20wxXmlResourceHandler7GetFontERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxXmlResourceHandler7GetFontERK8wxString"); return 0; }

long ZN20wxXmlResourceHandler7GetIconERK8wxStringS2_6wxSize(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxXmlResourceHandler7GetIconERK8wxStringS2_6wxSize"); return 0; }

long ZN20wxXmlResourceHandler7GetLongERK8wxStringl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxXmlResourceHandler7GetLongERK8wxStringl"); return 0; }

long ZN20wxXmlResourceHandler7GetNameEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxXmlResourceHandler7GetNameEv"); return 0; }

long ZN20wxXmlResourceHandler7GetSizeERK8wxStringP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxXmlResourceHandler7GetSizeERK8wxStringP8wxWindow"); return 0; }

long ZN20wxXmlResourceHandler7GetTextERK8wxStringb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxXmlResourceHandler7GetTextERK8wxStringb"); return 0; }

long ZN20wxXmlResourceHandler8AddStyleERK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxXmlResourceHandler8AddStyleERK8wxStringi"); return 0; }

long ZN20wxXmlResourceHandler8GetStyleERK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxXmlResourceHandler8GetStyleERK8wxStringi"); return 0; }

long ZN20wxXmlResourceHandler8HasParamERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxXmlResourceHandler8HasParamERK8wxString"); return 0; }

long ZN20wxXmlResourceHandler9GetBitmapERK8wxStringS2_6wxSize(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxXmlResourceHandler9GetBitmapERK8wxStringS2_6wxSize"); return 0; }

long ZN20wxXmlResourceHandler9GetColourERK8wxStringRK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxXmlResourceHandler9GetColourERK8wxStringRK8wxColour"); return 0; }

long ZN20wxXmlResourceHandler9IsOfClassEP9wxXmlNodeRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxXmlResourceHandler9IsOfClassEP9wxXmlNodeRK8wxString"); return 0; }

long ZN20wxXmlResourceHandlerC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxXmlResourceHandlerC2Ev"); return 0; }

long ZN21wxClientDataContainer15DoSetClientDataEPv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxClientDataContainer15DoSetClientDataEPv"); return 0; }

long ZN21wxClientDataContainer17DoSetClientObjectEP12wxClientData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxClientDataContainer17DoSetClientObjectEP12wxClientData"); return 0; }

long ZN21wxClientDataContainerC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxClientDataContainerC2Ev"); return 0; }

long ZN21wxClientDataContainerD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxClientDataContainerD2Ev"); return 0; }

long ZN21wxDataObjectComposite3AddEP18wxDataObjectSimpleb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxDataObjectComposite3AddEP18wxDataObjectSimpleb"); return 0; }

long ZN21wxDataObjectCompositeC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxDataObjectCompositeC1Ev"); return 0; }

long ZN21wxGridCellCoordsArrayD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxGridCellCoordsArrayD1Ev"); return 0; }

long ZN21wxGridCellCoordsArrayaSERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxGridCellCoordsArrayaSERKS_"); return 0; }

long ZN21wxGridCellFloatEditorC1Eii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxGridCellFloatEditorC1Eii"); return 0; }

long ZN21wxMemoryFSHandlerBase10RemoveFileERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxMemoryFSHandlerBase10RemoveFileERK8wxString"); return 0; }

long ZN21wxMemoryFSHandlerBase19AddFileWithMimeTypeERK8wxStringPKvmS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxMemoryFSHandlerBase19AddFileWithMimeTypeERK8wxStringPKvmS2_"); return 0; }

long ZN21wxMemoryFSHandlerBase7AddFileERK8wxStringPKvm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxMemoryFSHandlerBase7AddFileERK8wxStringPKvm"); return 0; }

long ZN21wxMemoryFSHandlerBase7AddFileERK8wxStringS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxMemoryFSHandlerBase7AddFileERK8wxStringS2_"); return 0; }

long ZN21wxMemoryFSHandlerBase7CanOpenERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxMemoryFSHandlerBase7CanOpenERK8wxString"); return 0; }

long ZN21wxMemoryFSHandlerBase8FindNextEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxMemoryFSHandlerBase8FindNextEv"); return 0; }

long ZN21wxMemoryFSHandlerBase8OpenFileER12wxFileSystemRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxMemoryFSHandlerBase8OpenFileER12wxFileSystemRK8wxString"); return 0; }

long ZN21wxMemoryFSHandlerBase9FindFirstERK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxMemoryFSHandlerBase9FindFirstERK8wxStringi"); return 0; }

long ZN21wxMemoryFSHandlerBaseC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxMemoryFSHandlerBaseC2Ev"); return 0; }

long ZN21wxMemoryFSHandlerBaseD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxMemoryFSHandlerBaseD2Ev"); return 0; }

long ZN21wxPageSetupDialogData12SetPaperSizeERK6wxSize(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxPageSetupDialogData12SetPaperSizeERK6wxSize"); return 0; }

long ZN21wxPageSetupDialogData12SetPrintDataERK11wxPrintData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxPageSetupDialogData12SetPrintDataERK11wxPrintData"); return 0; }

long ZN21wxPageSetupDialogData24CalculateIdFromPaperSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxPageSetupDialogData24CalculateIdFromPaperSizeEv"); return 0; }

long ZN21wxPageSetupDialogData24CalculatePaperSizeFromIdEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxPageSetupDialogData24CalculatePaperSizeFromIdEv"); return 0; }

long ZN21wxPageSetupDialogDataC1ERK11wxPrintData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxPageSetupDialogDataC1ERK11wxPrintData"); return 0; }

long ZN21wxPageSetupDialogDataC1ERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxPageSetupDialogDataC1ERKS_"); return 0; }

long ZN21wxPageSetupDialogDataC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxPageSetupDialogDataC1Ev"); return 0; }

long ZN21wxPasswordEntryDialogC1EP8wxWindowRK8wxStringS4_S4_lRK7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxPasswordEntryDialogC1EP8wxWindowRK8wxStringS4_S4_lRK7wxPoint"); return 0; }

long ZN21wxRichTextFileHandler8LoadFileEP16wxRichTextBufferRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxRichTextFileHandler8LoadFileEP16wxRichTextBufferRK8wxString"); return 0; }

long ZN21wxRichTextFileHandler8SaveFileEP16wxRichTextBufferRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxRichTextFileHandler8SaveFileEP16wxRichTextBufferRK8wxString"); return 0; }

long ZN21wxRichTextHTMLHandler14sm_fileCounterE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxRichTextHTMLHandler14sm_fileCounterE"); return 0; }

long ZN21wxRichTextHTMLHandler21DeleteTemporaryImagesEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxRichTextHTMLHandler21DeleteTemporaryImagesEv"); return 0; }

long ZN21wxRichTextHTMLHandlerC1ERK8wxStringS2_i(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxRichTextHTMLHandlerC1ERK8wxStringS2_i"); return 0; }

long ZN22wxGridCellAttrProvider10SetColAttrEP14wxGridCellAttri(long a, long b, long c_, long d, long e, long f) { shim_note("ZN22wxGridCellAttrProvider10SetColAttrEP14wxGridCellAttri"); return 0; }

long ZN22wxGridCellAttrProvider10SetRowAttrEP14wxGridCellAttri(long a, long b, long c_, long d, long e, long f) { shim_note("ZN22wxGridCellAttrProvider10SetRowAttrEP14wxGridCellAttri"); return 0; }

long ZN22wxGridCellAttrProvider14UpdateAttrColsEmi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN22wxGridCellAttrProvider14UpdateAttrColsEmi"); return 0; }

long ZN22wxGridCellAttrProvider14UpdateAttrRowsEmi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN22wxGridCellAttrProvider14UpdateAttrRowsEmi"); return 0; }

long ZN22wxGridCellAttrProvider7SetAttrEP14wxGridCellAttrii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN22wxGridCellAttrProvider7SetAttrEP14wxGridCellAttrii"); return 0; }

long ZN22wxGridCellAttrProviderC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN22wxGridCellAttrProviderC1Ev"); return 0; }

long ZN22wxGridCellAttrProviderC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN22wxGridCellAttrProviderC2Ev"); return 0; }

long ZN22wxGridCellAttrProviderD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN22wxGridCellAttrProviderD2Ev"); return 0; }

long ZN22wxGridCellChoiceEditorC1EmPK8wxStringb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN22wxGridCellChoiceEditorC1EmPK8wxStringb"); return 0; }

long ZN22wxGridCellEnumRendererC1ERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN22wxGridCellEnumRendererC1ERK8wxString"); return 0; }

long ZN22wxGridCellNumberEditorC1Eii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN22wxGridCellNumberEditorC1Eii"); return 0; }

long ZN22wxGridRangeSelectEventC1EiiP8wxObjectRK16wxGridCellCoordsS4_bbbbb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN22wxGridRangeSelectEventC1EiiP8wxObjectRK16wxGridCellCoordsS4_bbbbb"); return 0; }

long ZN22wxStdDialogButtonSizer15SetCancelButtonEP8wxButton(long a, long b, long c_, long d, long e, long f) { shim_note("ZN22wxStdDialogButtonSizer15SetCancelButtonEP8wxButton"); return 0; }

long ZN22wxStdDialogButtonSizer17SetNegativeButtonEP8wxButton(long a, long b, long c_, long d, long e, long f) { shim_note("ZN22wxStdDialogButtonSizer17SetNegativeButtonEP8wxButton"); return 0; }

long ZN22wxStdDialogButtonSizer20SetAffirmativeButtonEP8wxButton(long a, long b, long c_, long d, long e, long f) { shim_note("ZN22wxStdDialogButtonSizer20SetAffirmativeButtonEP8wxButton"); return 0; }

long ZN22wxStdDialogButtonSizer7RealizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN22wxStdDialogButtonSizer7RealizeEv"); return 0; }

long ZN22wxStdDialogButtonSizer9AddButtonEP8wxButton(long a, long b, long c_, long d, long e, long f) { shim_note("ZN22wxStdDialogButtonSizer9AddButtonEP8wxButton"); return 0; }

long ZN22wxStdDialogButtonSizerC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN22wxStdDialogButtonSizerC1Ev"); return 0; }

long ZN22wxSystemSettingsNative10HasFeatureE15wxSystemFeature(long a, long b, long c_, long d, long e, long f) { shim_note("ZN22wxSystemSettingsNative10HasFeatureE15wxSystemFeature"); return 0; }

long ZN22wxSystemSettingsNative7GetFontE12wxSystemFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZN22wxSystemSettingsNative7GetFontE12wxSystemFont"); return 0; }

long ZN22wxSystemSettingsNative9GetColourE14wxSystemColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN22wxSystemSettingsNative9GetColourE14wxSystemColour"); return 0; }

long ZN22wxSystemSettingsNative9GetMetricE14wxSystemMetricP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN22wxSystemSettingsNative9GetMetricE14wxSystemMetricP8wxWindow"); return 0; }

long ZN22wxWebKitNewWindowEventC1EP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN22wxWebKitNewWindowEventC1EP8wxWindow"); return 0; }

long ZN23wxDatePickerCtrlGeneric4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN23wxDatePickerCtrlGeneric4InitEv"); return 0; }

long ZN23wxDatePickerCtrlGeneric6CreateEP8wxWindowiRK10wxDateTimeRK7wxPointRK6wxSizelRK11wxValidatorRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN23wxDatePickerCtrlGeneric6CreateEP8wxWindowiRK10wxDateTimeRK7wxPointRK6wxSizelRK11wxValidatorRK8wxString"); return 0; }

long ZN23wxFileDirPickerCtrlBase10CreateBaseEP8wxWindowiRK8wxStringS4_S4_RK7wxPointRK6wxSizelRK11wxValidatorS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN23wxFileDirPickerCtrlBase10CreateBaseEP8wxWindowiRK8wxStringS4_S4_RK7wxPointRK6wxSizelRK11wxValidatorS4_"); return 0; }

long ZN23wxFileDirPickerCtrlBase7SetPathERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN23wxFileDirPickerCtrlBase7SetPathERK8wxString"); return 0; }

long ZN23wxFindReplaceDialogBaseD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN23wxFindReplaceDialogBaseD2Ev"); return 0; }

long ZN23wxGridCellFloatRendererC1Eii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN23wxGridCellFloatRendererC1Eii"); return 0; }

long ZN23wxHtmlWindowMouseHelper13OnCellClickedEP10wxHtmlCelliiRK12wxMouseEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN23wxHtmlWindowMouseHelper13OnCellClickedEP10wxHtmlCelliiRK12wxMouseEvent"); return 0; }

long ZN23wxHtmlWindowMouseHelper16OnCellMouseHoverEP10wxHtmlCellii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN23wxHtmlWindowMouseHelper16OnCellMouseHoverEP10wxHtmlCellii"); return 0; }

long ZN23wxHtmlWindowMouseHelperC2EP21wxHtmlWindowInterface(long a, long b, long c_, long d, long e, long f) { shim_note("ZN23wxHtmlWindowMouseHelperC2EP21wxHtmlWindowInterface"); return 0; }

long ZN23wxSingleInstanceChecker6CreateERK8wxStringS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN23wxSingleInstanceChecker6CreateERK8wxStringS2_"); return 0; }

long ZN23wxSingleInstanceCheckerD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN23wxSingleInstanceCheckerD1Ev"); return 0; }

long ZN23wxWebKitBeforeLoadEventC1EP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN23wxWebKitBeforeLoadEventC1EP8wxWindow"); return 0; }

long ZN24wxGenericCollapsiblePane6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN24wxGenericCollapsiblePane6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_"); return 0; }

long ZN24wxGridEditorCreatedEventC1EiiP8wxObjectiiP9wxControl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN24wxGridEditorCreatedEventC1EiiP8wxObjectiiP9wxControl"); return 0; }

long ZN24wxItemContainerImmutable18SetStringSelectionERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN24wxItemContainerImmutable18SetStringSelectionERK8wxString"); return 0; }

long ZN24wxItemContainerImmutableD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN24wxItemContainerImmutableD2Ev"); return 0; }

long ZN25wxRichTextCompositeObject10DefragmentEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN25wxRichTextCompositeObject10DefragmentEv"); return 0; }

long ZN25wxRichTextCompositeObject11AppendChildEP16wxRichTextObject(long a, long b, long c_, long d, long e, long f) { shim_note("ZN25wxRichTextCompositeObject11AppendChildEP16wxRichTextObject"); return 0; }

long ZN25wxRichTextCompositeObject11InsertChildEP16wxRichTextObjectS1_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN25wxRichTextCompositeObject11InsertChildEP16wxRichTextObjectS1_"); return 0; }

long ZN25wxRichTextCompositeObject11RemoveChildEP16wxRichTextObjectb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN25wxRichTextCompositeObject11RemoveChildEP16wxRichTextObjectb"); return 0; }

long ZN25wxRichTextCompositeObject14DeleteChildrenEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN25wxRichTextCompositeObject14DeleteChildrenEv"); return 0; }

long ZN25wxRichTextCompositeObject4CopyERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN25wxRichTextCompositeObject4CopyERKS_"); return 0; }

long ZN25wxRichTextCompositeObjectD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN25wxRichTextCompositeObjectD2Ev"); return 0; }

long ZN25wxWebKitStateChangedEventC1EP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN25wxWebKitStateChangedEventC1EP8wxWindow"); return 0; }

long ZN26wxGridCellDateTimeRendererC1ERK8wxStringS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN26wxGridCellDateTimeRendererC1ERK8wxStringS2_"); return 0; }

long ZN26wxRichTextHeaderFooterData4CopyERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN26wxRichTextHeaderFooterData4CopyERKS_"); return 0; }

long ZN28wxIndividualLayoutConstraint10ResetIfWinEP12wxWindowBase(long a, long b, long c_, long d, long e, long f) { shim_note("ZN28wxIndividualLayoutConstraint10ResetIfWinEP12wxWindowBase"); return 0; }

long ZN28wxIndividualLayoutConstraint17SatisfyConstraintEP19wxLayoutConstraintsP12wxWindowBase(long a, long b, long c_, long d, long e, long f) { shim_note("ZN28wxIndividualLayoutConstraint17SatisfyConstraintEP19wxLayoutConstraintsP12wxWindowBase"); return 0; }

long ZN28wxIndividualLayoutConstraint3SetE14wxRelationshipP12wxWindowBase6wxEdgeii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN28wxIndividualLayoutConstraint3SetE14wxRelationshipP12wxWindowBase6wxEdgeii"); return 0; }

long ZN28wxIndividualLayoutConstraint5AboveEP12wxWindowBasei(long a, long b, long c_, long d, long e, long f) { shim_note("ZN28wxIndividualLayoutConstraint5AboveEP12wxWindowBasei"); return 0; }

long ZN28wxIndividualLayoutConstraint5BelowEP12wxWindowBasei(long a, long b, long c_, long d, long e, long f) { shim_note("ZN28wxIndividualLayoutConstraint5BelowEP12wxWindowBasei"); return 0; }

long ZN28wxIndividualLayoutConstraint6LeftOfEP12wxWindowBasei(long a, long b, long c_, long d, long e, long f) { shim_note("ZN28wxIndividualLayoutConstraint6LeftOfEP12wxWindowBasei"); return 0; }

long ZN28wxIndividualLayoutConstraint6SameAsEP12wxWindowBase6wxEdgei(long a, long b, long c_, long d, long e, long f) { shim_note("ZN28wxIndividualLayoutConstraint6SameAsEP12wxWindowBase6wxEdgei"); return 0; }

long ZN28wxIndividualLayoutConstraint7RightOfEP12wxWindowBasei(long a, long b, long c_, long d, long e, long f) { shim_note("ZN28wxIndividualLayoutConstraint7RightOfEP12wxWindowBasei"); return 0; }

long ZN28wxIndividualLayoutConstraint8AbsoluteEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN28wxIndividualLayoutConstraint8AbsoluteEi"); return 0; }

long ZN28wxIndividualLayoutConstraint9PercentOfEP12wxWindowBase6wxEdgei(long a, long b, long c_, long d, long e, long f) { shim_note("ZN28wxIndividualLayoutConstraint9PercentOfEP12wxWindowBase6wxEdgei"); return 0; }

long ZN28wxRichTextParagraphLayoutBox10InvalidateERK15wxRichTextRange(long a, long b, long c_, long d, long e, long f) { shim_note("ZN28wxRichTextParagraphLayoutBox10InvalidateERK15wxRichTextRange"); return 0; }

long ZN28wxRichTextParagraphLayoutBox12CollectStyleER12wxTextAttrExRKS0_RlRi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN28wxRichTextParagraphLayoutBox12CollectStyleER12wxTextAttrExRKS0_RlRi"); return 0; }

long ZN28wxRichTextParagraphLayoutBox4CopyERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN28wxRichTextParagraphLayoutBox4CopyERKS_"); return 0; }

long ZN28wxRichTextParagraphLayoutBox4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN28wxRichTextParagraphLayoutBox4InitEv"); return 0; }

long ZN28wxRichTextParagraphLayoutBoxC1EP16wxRichTextObject(long a, long b, long c_, long d, long e, long f) { shim_note("ZN28wxRichTextParagraphLayoutBoxC1EP16wxRichTextObject"); return 0; }

long ZN28wxRichTextParagraphLayoutBoxC2EP16wxRichTextObject(long a, long b, long c_, long d, long e, long f) { shim_note("ZN28wxRichTextParagraphLayoutBoxC2EP16wxRichTextObject"); return 0; }

long ZN4wxDC10DoDrawIconERK6wxIconii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC10DoDrawIconERK6wxIconii"); return 0; }

long ZN4wxDC10DoDrawLineEiiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC10DoDrawLineEiiii"); return 0; }

long ZN4wxDC10DoDrawTextERK8wxStringii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC10DoDrawTextERK8wxStringii"); return 0; }

long ZN4wxDC10SetMapModeEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC10SetMapModeEi"); return 0; }

long ZN4wxDC10SetPaletteERK9wxPalette(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC10SetPaletteERK9wxPalette"); return 0; }

long ZN4wxDC11DoCrossHairEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC11DoCrossHairEii"); return 0; }

long ZN4wxDC11DoDrawLinesEiP7wxPointii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC11DoDrawLinesEiP7wxPointii"); return 0; }

long ZN4wxDC11DoDrawPointEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC11DoDrawPointEii"); return 0; }

long ZN4wxDC11DoFloodFillEiiRK8wxColouri(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC11DoFloodFillEiiRK8wxColouri"); return 0; }

long ZN4wxDC12DoDrawBitmapERK8wxBitmapiib(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC12DoDrawBitmapERK8wxBitmapiib"); return 0; }

long ZN4wxDC12DoDrawSplineEP6wxList(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC12DoDrawSplineEP6wxList"); return 0; }

long ZN4wxDC12SetUserScaleEdd(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC12SetUserScaleEdd"); return 0; }

long ZN4wxDC13DoDrawEllipseEiiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC13DoDrawEllipseEiiii"); return 0; }

long ZN4wxDC13DoDrawPolygonEiP7wxPointiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC13DoDrawPolygonEiP7wxPointiii"); return 0; }

long ZN4wxDC13SetBackgroundERK7wxBrush(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC13SetBackgroundERK7wxBrush"); return 0; }

long ZN4wxDC15DoDrawCheckMarkEiiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC15DoDrawCheckMarkEiiii"); return 0; }

long ZN4wxDC15DoDrawRectangleEiiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC15DoDrawRectangleEiiii"); return 0; }

long ZN4wxDC15SetDeviceOriginEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC15SetDeviceOriginEii"); return 0; }

long ZN4wxDC15SetLogicalScaleEdd(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC15SetLogicalScaleEdd"); return 0; }

long ZN4wxDC16SetLogicalOriginEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC16SetLogicalOriginEii"); return 0; }

long ZN4wxDC17DoDrawEllipticArcEiiiidd(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC17DoDrawEllipticArcEiiiidd"); return 0; }

long ZN4wxDC17DoDrawPolyPolygonEiPiP7wxPointiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC17DoDrawPolyPolygonEiPiP7wxPointiii"); return 0; }

long ZN4wxDC17DoDrawRotatedTextERK8wxStringiid(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC17DoDrawRotatedTextERK8wxStringiid"); return 0; }

long ZN4wxDC17SetBackgroundModeEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC17SetBackgroundModeEi"); return 0; }

long ZN4wxDC17SetTextBackgroundERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC17SetTextBackgroundERK8wxColour"); return 0; }

long ZN4wxDC17SetTextForegroundERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC17SetTextForegroundERK8wxColour"); return 0; }

long ZN4wxDC18SetAxisOrientationEbb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC18SetAxisOrientationEbb"); return 0; }

long ZN4wxDC18SetGraphicsContextEP17wxGraphicsContext(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC18SetGraphicsContextEP17wxGraphicsContext"); return 0; }

long ZN4wxDC18SetLogicalFunctionEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC18SetLogicalFunctionEi"); return 0; }

long ZN4wxDC19DoSetClippingRegionEiiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC19DoSetClippingRegionEiiii"); return 0; }

long ZN4wxDC20DoGradientFillLinearERK6wxRectRK8wxColourS5_11wxDirection(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC20DoGradientFillLinearERK6wxRectRK8wxColourS5_11wxDirection"); return 0; }

long ZN4wxDC21ComputeScaleAndOriginEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC21ComputeScaleAndOriginEv"); return 0; }

long ZN4wxDC21DestroyClippingRegionEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC21DestroyClippingRegionEv"); return 0; }

long ZN4wxDC22DoDrawRoundedRectangleEiiiid(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC22DoDrawRoundedRectangleEiiiid"); return 0; }

long ZN4wxDC24DoGradientFillConcentricERK6wxRectRK8wxColourS5_RK7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC24DoGradientFillConcentricERK6wxRectRK8wxColourS5_RK7wxPoint"); return 0; }

long ZN4wxDC27DoSetClippingRegionAsRegionERK8wxRegion(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC27DoSetClippingRegionAsRegionERK8wxRegion"); return 0; }

long ZN4wxDC5ClearEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC5ClearEv"); return 0; }

long ZN4wxDC5FlushEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC5FlushEv"); return 0; }

long ZN4wxDC6DoBlitEiiiiPS_iiibii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC6DoBlitEiiiiPS_iiibii"); return 0; }

long ZN4wxDC6EndDocEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC6EndDocEv"); return 0; }

long ZN4wxDC6SetPenERK5wxPen(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC6SetPenERK5wxPen"); return 0; }

long ZN4wxDC7EndPageEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC7EndPageEv"); return 0; }

long ZN4wxDC7SetFontERK6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC7SetFontERK6wxFont"); return 0; }

long ZN4wxDC8SetBrushERK7wxBrush(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC8SetBrushERK7wxBrush"); return 0; }

long ZN4wxDC8StartDocERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC8StartDocERK8wxString"); return 0; }

long ZN4wxDC9DoDrawArcEiiiiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC9DoDrawArcEiiiiii"); return 0; }

long ZN4wxDC9StartPageEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDC9StartPageEv"); return 0; }

long ZN4wxDCC1ERK10wxWindowDC(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDCC1ERK10wxWindowDC"); return 0; }

long ZN4wxDCC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDCC2Ev"); return 0; }

long ZN4wxDCD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN4wxDCD2Ev"); return 0; }

long ZN5wxApp10InitializeERiPPw(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxApp10InitializeERiPPw"); return 0; }

long ZN5wxApp10MacHideAppEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxApp10MacHideAppEv"); return 0; }

long ZN5wxApp10WakeUpIdleEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxApp10WakeUpIdleEv"); return 0; }

long ZN5wxApp12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxApp12ms_classInfoE"); return 0; }

long ZN5wxApp15MacHandleAEOAppEPvS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxApp15MacHandleAEOAppEPvS0_"); return 0; }

long ZN5wxApp15MacHandleAEODocEPvS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxApp15MacHandleAEODocEPvS0_"); return 0; }

long ZN5wxApp15MacHandleAEPDocEPvS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxApp15MacHandleAEPDocEPvS0_"); return 0; }

long ZN5wxApp15MacHandleAEQuitEPvS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxApp15MacHandleAEQuitEPvS0_"); return 0; }

long ZN5wxApp15MacHandleAERAppEPvS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxApp15MacHandleAERAppEPvS0_"); return 0; }

long ZN5wxApp19s_macExitMenuItemIdE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxApp19s_macExitMenuItemIdE"); return 0; }

long ZN5wxApp20s_macAboutMenuItemIdE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxApp20s_macAboutMenuItemIdE"); return 0; }

long ZN5wxApp22s_macHelpMenuTitleNameE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxApp22s_macHelpMenuTitleNameE"); return 0; }

long ZN5wxApp23MacHandleUnhandledEventEPv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxApp23MacHandleUnhandledEventEPv"); return 0; }

long ZN5wxApp23MacRequestUserAttentionE21wxNotificationOptions(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxApp23MacRequestUserAttentionE21wxNotificationOptions"); return 0; }

long ZN5wxApp26s_macPreferencesMenuItemIdE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxApp26s_macPreferencesMenuItemIdE"); return 0; }

long ZN5wxApp5YieldEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxApp5YieldEb"); return 0; }

long ZN5wxApp7CleanUpEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxApp7CleanUpEv"); return 0; }

long ZN5wxApp9OnInitGuiEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxApp9OnInitGuiEv"); return 0; }

long ZN5wxAppC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxAppC2Ev"); return 0; }

long ZN5wxLog11DoLogStringEPKwl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxLog11DoLogStringEPKwl"); return 0; }

long ZN5wxLog11ms_bVerboseE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxLog11ms_bVerboseE"); return 0; }

long ZN5wxLog11ms_logLevelE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxLog11ms_logLevelE"); return 0; }

long ZN5wxLog12ms_timestampE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxLog12ms_timestampE"); return 0; }

long ZN5wxLog14ms_aTraceMasksE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxLog14ms_aTraceMasksE"); return 0; }

long ZN5wxLog14ms_ulTraceMaskE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxLog14ms_ulTraceMaskE"); return 0; }

long ZN5wxLog15ClearTraceMasksEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxLog15ClearTraceMasksEv"); return 0; }

long ZN5wxLog15GetActiveTargetEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxLog15GetActiveTargetEv"); return 0; }

long ZN5wxLog15RemoveTraceMaskERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxLog15RemoveTraceMaskERK8wxString"); return 0; }

long ZN5wxLog15SetActiveTargetEPS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxLog15SetActiveTargetEPS_"); return 0; }

long ZN5wxLog15ms_suspendCountE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxLog15ms_suspendCountE"); return 0; }

long ZN5wxLog17ms_bRepetCountingE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxLog17ms_bRepetCountingE"); return 0; }

long ZN5wxLog18DontCreateOnDemandEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxLog18DontCreateOnDemandEv"); return 0; }

long ZN5wxLog18IsAllowedTraceMaskEPKw(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxLog18IsAllowedTraceMaskEPKw"); return 0; }

long ZN5wxLog5DoLogEmPKwl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxLog5DoLogEmPKwl"); return 0; }

long ZN5wxLog5FlushEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxLog5FlushEv"); return 0; }

long ZN5wxLog5OnLogEmPKwl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxLog5OnLogEmPKwl"); return 0; }

long ZN5wxLog8ms_doLogE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxLog8ms_doLogE"); return 0; }

long ZN5wxLog9TimeStampEP8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxLog9TimeStampEP8wxString"); return 0; }

long ZN5wxLogD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxLogD2Ev"); return 0; }

long ZN5wxPen6SetCapEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxPen6SetCapEi"); return 0; }

long ZN5wxPen7SetJoinEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxPen7SetJoinEi"); return 0; }

long ZN5wxPen8SetStyleEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxPen8SetStyleEi"); return 0; }

long ZN5wxPen8SetWidthEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxPen8SetWidthEi"); return 0; }

long ZN5wxPen9SetColourERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxPen9SetColourERK8wxColour"); return 0; }

long ZN5wxPen9SetDashesEiPKa(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxPen9SetDashesEiPKa"); return 0; }

long ZN5wxPenC1ERK8wxColourii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxPenC1ERK8wxColourii"); return 0; }

long ZN5wxPenC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxPenC1Ev"); return 0; }

long ZN5wxPenD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxPenD1Ev"); return 0; }

long ZN5wxURIC1ERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxURIC1ERK8wxString"); return 0; }

long ZN5wxURID1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN5wxURID1Ev"); return 0; }

long ZN6wxFont11SetFaceNameERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxFont11SetFaceNameERK8wxString"); return 0; }

long ZN6wxFont12SetPointSizeEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxFont12SetPointSizeEi"); return 0; }

long ZN6wxFont13SetUnderlinedEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxFont13SetUnderlinedEb"); return 0; }

long ZN6wxFont18MacCreateThemeFontEt(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxFont18MacCreateThemeFontEt"); return 0; }

long ZN6wxFont6CreateERK16wxNativeFontInfo(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxFont6CreateERK16wxNativeFontInfo"); return 0; }

long ZN6wxFont6CreateEiiiibRK8wxString14wxFontEncoding(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxFont6CreateEiiiibRK8wxString14wxFontEncoding"); return 0; }

long ZN6wxFont8SetStyleEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxFont8SetStyleEi"); return 0; }

long ZN6wxFont9SetWeightEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxFont9SetWeightEi"); return 0; }

long ZN6wxFontD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxFontD1Ev"); return 0; }

long ZN6wxGrid10AppendColsEib(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid10AppendColsEib"); return 0; }

long ZN6wxGrid10AppendRowsEib(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid10AppendRowsEib"); return 0; }

long ZN6wxGrid10CellToRectEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid10CellToRectEii"); return 0; }

long ZN6wxGrid10CreateGridEiiNS_20wxGridSelectionModesE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid10CreateGridEiiNS_20wxGridSelectionModesE"); return 0; }

long ZN6wxGrid10DeleteColsEiib(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid10DeleteColsEiib"); return 0; }

long ZN6wxGrid10DeleteRowsEiib(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid10DeleteRowsEiib"); return 0; }

long ZN6wxGrid10GetColSizeEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid10GetColSizeEi"); return 0; }

long ZN6wxGrid10GetRowSizeEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid10GetRowSizeEi"); return 0; }

long ZN6wxGrid10InsertColsEiib(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid10InsertColsEiib"); return 0; }

long ZN6wxGrid10InsertRowsEiib(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid10InsertRowsEiib"); return 0; }

long ZN6wxGrid10MovePageUpEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid10MovePageUpEv"); return 0; }

long ZN6wxGrid10SetColAttrEiP14wxGridCellAttr(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid10SetColAttrEiP14wxGridCellAttr"); return 0; }

long ZN6wxGrid10SetColSizeEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid10SetColSizeEii"); return 0; }

long ZN6wxGrid10SetRowAttrEiP14wxGridCellAttr(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid10SetRowAttrEiP14wxGridCellAttr"); return 0; }

long ZN6wxGrid10SetRowSizeEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid10SetRowSizeEii"); return 0; }

long ZN6wxGrid11DeselectColEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid11DeselectColEi"); return 0; }

long ZN6wxGrid11DeselectRowEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid11DeselectRowEi"); return 0; }

long ZN6wxGrid11GetCellFontEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid11GetCellFontEii"); return 0; }

long ZN6wxGrid11GetCellSizeEiiPiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid11GetCellSizeEiiPiS0_"); return 0; }

long ZN6wxGrid11IsSelectionEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid11IsSelectionEv"); return 0; }

long ZN6wxGrid11SelectBlockEiiiib(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid11SelectBlockEiiiib"); return 0; }

long ZN6wxGrid11SetCellFontEiiRK6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid11SetCellFontEiiRK6wxFont"); return 0; }

long ZN6wxGrid11SetCellSizeEiiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid11SetCellSizeEiiii"); return 0; }

long ZN6wxGrid11SetReadOnlyEiib(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid11SetReadOnlyEiib"); return 0; }

long ZN6wxGrid12DeselectCellEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid12DeselectCellEii"); return 0; }

long ZN6wxGrid12ForceRefreshEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid12ForceRefreshEv"); return 0; }

long ZN6wxGrid12MoveCursorUpEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid12MoveCursorUpEb"); return 0; }

long ZN6wxGrid12MovePageDownEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid12MovePageDownEv"); return 0; }

long ZN6wxGrid12SetCellValueEiiRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid12SetCellValueEiiRK8wxString"); return 0; }

long ZN6wxGrid12SetLabelFontERK6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid12SetLabelFontERK6wxFont"); return 0; }

long ZN6wxGrid12XToEdgeOfColEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid12XToEdgeOfColEi"); return 0; }

long ZN6wxGrid12YToEdgeOfRowEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid12YToEdgeOfRowEi"); return 0; }

long ZN6wxGrid13EnableEditingEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid13EnableEditingEb"); return 0; }

long ZN6wxGrid13GetCellEditorEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid13GetCellEditorEii"); return 0; }

long ZN6wxGrid13SetCellEditorEiiP16wxGridCellEditor(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid13SetCellEditorEiiP16wxGridCellEditor"); return 0; }

long ZN6wxGrid14CalcDimensionsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid14CalcDimensionsEv"); return 0; }

long ZN6wxGrid14ClearSelectionEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid14ClearSelectionEv"); return 0; }

long ZN6wxGrid14EnableDragCellEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid14EnableDragCellEb"); return 0; }

long ZN6wxGrid14GetTextBoxSizeERK4wxDCRK13wxArrayStringPlS6_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid14GetTextBoxSizeERK4wxDCRK13wxArrayStringPlS6_"); return 0; }

long ZN6wxGrid14MoveCursorDownEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid14MoveCursorDownEb"); return 0; }

long ZN6wxGrid14MoveCursorLeftEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid14MoveCursorLeftEb"); return 0; }

long ZN6wxGrid14SetCurrentCellERK16wxGridCellCoords(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid14SetCurrentCellERK16wxGridCellCoords"); return 0; }

long ZN6wxGrid15EnableGridLinesEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid15EnableGridLinesEb"); return 0; }

long ZN6wxGrid15GetCellOverflowEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid15GetCellOverflowEii"); return 0; }

long ZN6wxGrid15GetCellRendererEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid15GetCellRendererEii"); return 0; }

long ZN6wxGrid15MakeCellVisibleEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid15MakeCellVisibleEii"); return 0; }

long ZN6wxGrid15MoveCursorRightEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid15MoveCursorRightEb"); return 0; }

long ZN6wxGrid15SetCellOverflowEiib(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid15SetCellOverflowEiib"); return 0; }

long ZN6wxGrid15SetCellRendererEiiP18wxGridCellRenderer(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid15SetCellRendererEiiP18wxGridCellRenderer"); return 0; }

long ZN6wxGrid15SetColLabelSizeEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid15SetColLabelSizeEi"); return 0; }

long ZN6wxGrid15SetRowLabelSizeEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid15SetRowLabelSizeEi"); return 0; }

long ZN6wxGrid16AutoSizeColOrRowEibb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid16AutoSizeColOrRowEibb"); return 0; }

long ZN6wxGrid16GetCellAlignmentEiiPiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid16GetCellAlignmentEiiPiS0_"); return 0; }

long ZN6wxGrid16GetColLabelValueEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid16GetColLabelValueEi"); return 0; }

long ZN6wxGrid16GetRowLabelValueEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid16GetRowLabelValueEi"); return 0; }

long ZN6wxGrid16RegisterDataTypeERK8wxStringP18wxGridCellRendererP16wxGridCellEditor(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid16RegisterDataTypeERK8wxStringP18wxGridCellRendererP16wxGridCellEditor"); return 0; }

long ZN6wxGrid16SetCellAlignmentEiiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid16SetCellAlignmentEiiii"); return 0; }

long ZN6wxGrid16SetColFormatBoolEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid16SetColFormatBoolEi"); return 0; }

long ZN6wxGrid16SetColLabelValueEiRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid16SetColLabelValueEiRK8wxString"); return 0; }

long ZN6wxGrid16SetDefaultEditorEP16wxGridCellEditor(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid16SetDefaultEditorEP16wxGridCellEditor"); return 0; }

long ZN6wxGrid16SetRowLabelValueEiRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid16SetRowLabelValueEiRK8wxString"); return 0; }

long ZN6wxGrid16SetSelectionModeENS_20wxGridSelectionModesE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid16SetSelectionModeENS_20wxGridSelectionModesE"); return 0; }

long ZN6wxGrid17BlockToDeviceRectERK16wxGridCellCoordsS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid17BlockToDeviceRectERK16wxGridCellCoordsS2_"); return 0; }

long ZN6wxGrid17DrawTextRectangleER4wxDCRK8wxStringRK6wxRectiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid17DrawTextRectangleER4wxDCRK8wxStringRK6wxRectiii"); return 0; }

long ZN6wxGrid17EnableDragColMoveEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid17EnableDragColMoveEb"); return 0; }

long ZN6wxGrid17EnableDragColSizeEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid17EnableDragColSizeEb"); return 0; }

long ZN6wxGrid17EnableDragRowSizeEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid17EnableDragRowSizeEb"); return 0; }

long ZN6wxGrid17GetCellTextColourEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid17GetCellTextColourEii"); return 0; }

long ZN6wxGrid17GetDefaultColSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid17GetDefaultColSizeEv"); return 0; }

long ZN6wxGrid17GetDefaultRowSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid17GetDefaultRowSizeEv"); return 0; }

long ZN6wxGrid17MoveCursorUpBlockEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid17MoveCursorUpBlockEb"); return 0; }

long ZN6wxGrid17SetCellTextColourEiiRK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid17SetCellTextColourEiiRK8wxColour"); return 0; }

long ZN6wxGrid17SetColFormatFloatEiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid17SetColFormatFloatEiii"); return 0; }

long ZN6wxGrid17SetDefaultColSizeEib(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid17SetDefaultColSizeEib"); return 0; }

long ZN6wxGrid17SetDefaultRowSizeEib(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid17SetDefaultRowSizeEib"); return 0; }

long ZN6wxGrid17SetGridLineColourERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid17SetGridLineColourERK8wxColour"); return 0; }

long ZN6wxGrid17SetOrCalcRowSizesEbb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid17SetOrCalcRowSizesEbb"); return 0; }

long ZN6wxGrid18EnableDragGridSizeEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid18EnableDragGridSizeEb"); return 0; }

long ZN6wxGrid18GetDefaultCellFontEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid18GetDefaultCellFontEv"); return 0; }

long ZN6wxGrid18SetColFormatCustomEiRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid18SetColFormatCustomEiRK8wxString"); return 0; }

long ZN6wxGrid18SetColFormatNumberEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid18SetColFormatNumberEi"); return 0; }

long ZN6wxGrid18SetColMinimalWidthEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid18SetColMinimalWidthEii"); return 0; }

long ZN6wxGrid18SetDefaultCellFontERK6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid18SetDefaultCellFontERK6wxFont"); return 0; }

long ZN6wxGrid18SetDefaultRendererEP18wxGridCellRenderer(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid18SetDefaultRendererEP18wxGridCellRenderer"); return 0; }

long ZN6wxGrid18SetLabelTextColourERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid18SetLabelTextColourERK8wxColour"); return 0; }

long ZN6wxGrid19HideCellEditControlEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid19HideCellEditControlEv"); return 0; }

long ZN6wxGrid19MoveCursorDownBlockEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid19MoveCursorDownBlockEb"); return 0; }

long ZN6wxGrid19MoveCursorLeftBlockEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid19MoveCursorLeftBlockEb"); return 0; }

long ZN6wxGrid19ProcessTableMessageER18wxGridTableMessage(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid19ProcessTableMessageER18wxGridTableMessage"); return 0; }

long ZN6wxGrid19SetRowMinimalHeightEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid19SetRowMinimalHeightEii"); return 0; }

long ZN6wxGrid19ShowCellEditControlEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid19ShowCellEditControlEv"); return 0; }

long ZN6wxGrid20AutoSizeColLabelSizeEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid20AutoSizeColLabelSizeEi"); return 0; }

long ZN6wxGrid20AutoSizeRowLabelSizeEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid20AutoSizeRowLabelSizeEi"); return 0; }

long ZN6wxGrid20GetColLabelAlignmentEPiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid20GetColLabelAlignmentEPiS0_"); return 0; }

long ZN6wxGrid20GetRowLabelAlignmentEPiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid20GetRowLabelAlignmentEPiS0_"); return 0; }

long ZN6wxGrid20MoveCursorRightBlockEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid20MoveCursorRightBlockEb"); return 0; }

long ZN6wxGrid20SaveEditControlValueEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid20SaveEditControlValueEv"); return 0; }

long ZN6wxGrid20SetColLabelAlignmentEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid20SetColLabelAlignmentEii"); return 0; }

long ZN6wxGrid20SetOrCalcColumnSizesEbb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid20SetOrCalcColumnSizesEbb"); return 0; }

long ZN6wxGrid20SetRowLabelAlignmentEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid20SetRowLabelAlignmentEii"); return 0; }

long ZN6wxGrid21EnableCellEditControlEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid21EnableCellEditControlEb"); return 0; }

long ZN6wxGrid22GetDefaultCellOverflowEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid22GetDefaultCellOverflowEv"); return 0; }

long ZN6wxGrid22SetCellHighlightColourERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid22SetCellHighlightColourERK8wxColour"); return 0; }

long ZN6wxGrid22SetDefaultCellOverflowEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid22SetDefaultCellOverflowEb"); return 0; }

long ZN6wxGrid23GetCellBackgroundColourEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid23GetCellBackgroundColourEii"); return 0; }

long ZN6wxGrid23GetDefaultCellAlignmentEPiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid23GetDefaultCellAlignmentEPiS0_"); return 0; }

long ZN6wxGrid23SetCellBackgroundColourEiiRK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid23SetCellBackgroundColourEiiRK8wxColour"); return 0; }

long ZN6wxGrid23SetDefaultCellAlignmentEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid23SetDefaultCellAlignmentEii"); return 0; }

long ZN6wxGrid24GetDefaultCellTextColourEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid24GetDefaultCellTextColourEv"); return 0; }

long ZN6wxGrid24SetCellHighlightPenWidthEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid24SetCellHighlightPenWidthEi"); return 0; }

long ZN6wxGrid24SetDefaultCellTextColourERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid24SetDefaultCellTextColourERK8wxColour"); return 0; }

long ZN6wxGrid24SetLabelBackgroundColourERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid24SetLabelBackgroundColourERK8wxColour"); return 0; }

long ZN6wxGrid26GetColLabelTextOrientationEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid26GetColLabelTextOrientationEv"); return 0; }

long ZN6wxGrid26SetCellHighlightROPenWidthEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid26SetCellHighlightROPenWidthEi"); return 0; }

long ZN6wxGrid26SetColLabelTextOrientationEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid26SetColLabelTextOrientationEi"); return 0; }

long ZN6wxGrid28SetColMinimalAcceptableWidthEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid28SetColMinimalAcceptableWidthEi"); return 0; }

long ZN6wxGrid29SetRowMinimalAcceptableHeightEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid29SetRowMinimalAcceptableHeightEi"); return 0; }

long ZN6wxGrid30GetDefaultCellBackgroundColourEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid30GetDefaultCellBackgroundColourEv"); return 0; }

long ZN6wxGrid30SetDefaultCellBackgroundColourERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid30SetDefaultCellBackgroundColourERK8wxColour"); return 0; }

long ZN6wxGrid6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN6wxGrid6XToColEib(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid6XToColEib"); return 0; }

long ZN6wxGrid6YToRowEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid6YToRowEi"); return 0; }

long ZN6wxGrid7SetAttrEiiP14wxGridCellAttr(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid7SetAttrEiiP14wxGridCellAttr"); return 0; }

long ZN6wxGrid8AutoSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid8AutoSizeEv"); return 0; }

long ZN6wxGrid8EndBatchEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid8EndBatchEv"); return 0; }

long ZN6wxGrid8SetTableEP15wxGridTableBasebNS_20wxGridSelectionModesE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid8SetTableEP15wxGridTableBasebNS_20wxGridSelectionModesE"); return 0; }

long ZN6wxGrid8XYToCellEiiR16wxGridCellCoords(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid8XYToCellEiiR16wxGridCellCoords"); return 0; }

long ZN6wxGrid9ClearGridEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid9ClearGridEv"); return 0; }

long ZN6wxGrid9IsVisibleEiib(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid9IsVisibleEiib"); return 0; }

long ZN6wxGrid9SelectAllEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid9SelectAllEv"); return 0; }

long ZN6wxGrid9SelectColEib(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid9SelectColEib"); return 0; }

long ZN6wxGrid9SelectRowEib(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid9SelectRowEib"); return 0; }

long ZN6wxGrid9SetColPosEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGrid9SetColPosEii"); return 0; }

long ZN6wxGridC1EP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGridC1EP8wxWindowiRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN6wxGridC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxGridC1Ev"); return 0; }

long ZN6wxIcon14CopyFromBitmapERK8wxBitmap(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxIcon14CopyFromBitmapERK8wxBitmap"); return 0; }

long ZN6wxIcon8LoadFileERK8wxString12wxBitmapTypeii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxIcon8LoadFileERK8wxString12wxBitmapTypeii"); return 0; }

long ZN6wxIcon8SetDepthEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxIcon8SetDepthEi"); return 0; }

long ZN6wxIcon8SetWidthEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxIcon8SetWidthEi"); return 0; }

long ZN6wxIcon9SetHeightEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxIcon9SetHeightEi"); return 0; }

long ZN6wxIconC1EPPc(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxIconC1EPPc"); return 0; }

long ZN6wxIconC1ERK8wxStringiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxIconC1ERK8wxStringiii"); return 0; }

long ZN6wxIconC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxIconC1Ev"); return 0; }

long ZN6wxIconD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxIconD1Ev"); return 0; }

long ZN6wxListC1Ei(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxListC1Ei"); return 0; }

long ZN6wxMaskC1ERK8wxBitmapRK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxMaskC1ERK8wxBitmapRK8wxColour"); return 0; }

long ZN6wxMenu4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxMenu4InitEv"); return 0; }

long ZN6wxRect5UnionERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxRect5UnionERKS_"); return 0; }

long ZN6wxRect7InflateEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxRect7InflateEii"); return 0; }

long ZN6wxRect9IntersectERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxRect9IntersectERKS_"); return 0; }

long ZN6wxRectC1ERK7wxPointS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN6wxRectC1ERK7wxPointS2_"); return 0; }

long ZN7wxBrush9SetColourERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxBrush9SetColourERK8wxColour"); return 0; }

long ZN7wxBrushC1ERK8wxBitmap(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxBrushC1ERK8wxBitmap"); return 0; }

long ZN7wxBrushC1ERK8wxColouri(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxBrushC1ERK8wxColouri"); return 0; }

long ZN7wxBrushC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxBrushC1Ev"); return 0; }

long ZN7wxBrushD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxBrushD1Ev"); return 0; }

long ZN7wxCaret11InitGenericEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxCaret11InitGenericEv"); return 0; }

long ZN7wxEvent12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxEvent12ms_classInfoE"); return 0; }

long ZN7wxEventC2ERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxEventC2ERKS_"); return 0; }

long ZN7wxEventC2Eii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxEventC2Eii"); return 0; }

long ZN7wxFrame10SetToolBarEP9wxToolBar(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxFrame10SetToolBarEP9wxToolBar"); return 0; }

long ZN7wxFrame13AttachMenuBarEP9wxMenuBar(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxFrame13AttachMenuBarEP9wxMenuBar"); return 0; }

long ZN7wxFrame13CreateToolBarEliRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxFrame13CreateToolBarEliRK8wxString"); return 0; }

long ZN7wxFrame13DetachMenuBarEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxFrame13DetachMenuBarEv"); return 0; }

long ZN7wxFrame15DoSetClientSizeEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxFrame15DoSetClientSizeEii"); return 0; }

long ZN7wxFrame15PositionToolBarEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxFrame15PositionToolBarEv"); return 0; }

long ZN7wxFrame17OnCreateStatusBarEiliRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxFrame17OnCreateStatusBarEiliRK8wxString"); return 0; }

long ZN7wxFrame17PositionStatusBarEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxFrame17PositionStatusBarEv"); return 0; }

long ZN7wxFrame4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxFrame4InitEv"); return 0; }

long ZN7wxFrame6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxFrame6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_"); return 0; }

long ZN7wxFrame6EnableEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxFrame6EnableEb"); return 0; }

long ZN7wxFrameD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxFrameD2Ev"); return 0; }

long ZN7wxGauge6CreateEP8wxWindowiiRK7wxPointRK6wxSizelRK11wxValidatorRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxGauge6CreateEP8wxWindowiiRK7wxPointRK6wxSizelRK11wxValidatorRK8wxString"); return 0; }

long ZN7wxImage10AddHandlerEP14wxImageHandler(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage10AddHandlerEP14wxImageHandler"); return 0; }

long ZN7wxImage11sm_handlersE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage11sm_handlersE"); return 0; }

long ZN7wxImage12BlurVerticalEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage12BlurVerticalEi"); return 0; }

long ZN7wxImage13GetImageCountERK8wxStringl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage13GetImageCountERK8wxStringl"); return 0; }

long ZN7wxImage13InsertHandlerEP14wxImageHandler(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage13InsertHandlerEP14wxImageHandler"); return 0; }

long ZN7wxImage13RemoveHandlerERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage13RemoveHandlerERK8wxString"); return 0; }

long ZN7wxImage13SetMaskColourEhhh(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage13SetMaskColourEhhh"); return 0; }

long ZN7wxImage14BlurHorizontalEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage14BlurHorizontalEi"); return 0; }

long ZN7wxImage16SetMaskFromImageERKS_hhh(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage16SetMaskFromImageERKS_hhh"); return 0; }

long ZN7wxImage18ConvertAlphaToMaskEh(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage18ConvertAlphaToMaskEh"); return 0; }

long ZN7wxImage19GetImageExtWildcardEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage19GetImageExtWildcardEv"); return 0; }

long ZN7wxImage20ConvertColourToAlphaEhhh(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage20ConvertColourToAlphaEhhh"); return 0; }

long ZN7wxImage4BlurEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage4BlurEi"); return 0; }

long ZN7wxImage5PasteERKS_ii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage5PasteERKS_ii"); return 0; }

long ZN7wxImage6CreateEiib(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage6CreateEiib"); return 0; }

long ZN7wxImage6SetRGBERK6wxRecthhh(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage6SetRGBERK6wxRecthhh"); return 0; }

long ZN7wxImage6SetRGBEiihhh(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage6SetRGBEiihhh"); return 0; }

long ZN7wxImage7CanReadER13wxInputStream(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage7CanReadER13wxInputStream"); return 0; }

long ZN7wxImage7CanReadERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage7CanReadERK8wxString"); return 0; }

long ZN7wxImage7DestroyEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage7DestroyEv"); return 0; }

long ZN7wxImage7ReplaceEhhhhhh(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage7ReplaceEhhhhhh"); return 0; }

long ZN7wxImage7SetDataEPhb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage7SetDataEPhb"); return 0; }

long ZN7wxImage7SetMaskEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage7SetMaskEb"); return 0; }

long ZN7wxImage8HSVtoRGBERKNS_8HSVValueE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage8HSVtoRGBERKNS_8HSVValueE"); return 0; }

long ZN7wxImage8RGBtoHSVERKNS_8RGBValueE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage8RGBtoHSVERKNS_8RGBValueE"); return 0; }

long ZN7wxImage8SetAlphaEPhb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage8SetAlphaEPhb"); return 0; }

long ZN7wxImage8SetAlphaEiih(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage8SetAlphaEiih"); return 0; }

long ZN7wxImage9InitAlphaEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage9InitAlphaEv"); return 0; }

long ZN7wxImage9RotateHueEd(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage9RotateHueEd"); return 0; }

long ZN7wxImage9SetOptionERK8wxStringS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage9SetOptionERK8wxStringS2_"); return 0; }

long ZN7wxImage9SetOptionERK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImage9SetOptionERK8wxStringi"); return 0; }

long ZN7wxImageC1ER13wxInputStreamRK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImageC1ER13wxInputStreamRK8wxStringi"); return 0; }

long ZN7wxImageC1ER13wxInputStreamli(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImageC1ER13wxInputStreamli"); return 0; }

long ZN7wxImageC1ERK8wxStringS2_i(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImageC1ERK8wxStringS2_i"); return 0; }

long ZN7wxImageC1ERK8wxStringli(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImageC1ERK8wxStringli"); return 0; }

long ZN7wxImageC1EiiPhS0_b(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImageC1EiiPhS0_b"); return 0; }

long ZN7wxImageC1EiiPhb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImageC1EiiPhb"); return 0; }

long ZN7wxImageC1Eiib(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxImageC1Eiib"); return 0; }

long ZN7wxPanel10InitDialogEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxPanel10InitDialogEv"); return 0; }

long ZN7wxPanel11RemoveChildEP12wxWindowBase(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxPanel11RemoveChildEP12wxWindowBase"); return 0; }

long ZN7wxPanel12OnChildFocusER17wxChildFocusEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxPanel12OnChildFocusER17wxChildFocusEvent"); return 0; }

long ZN7wxPanel12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxPanel12ms_classInfoE"); return 0; }

long ZN7wxPanel24SetFocusIgnoringChildrenEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxPanel24SetFocusIgnoringChildrenEv"); return 0; }

long ZN7wxPanel4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxPanel4InitEv"); return 0; }

long ZN7wxPanel6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxPanel6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN7wxPanel8SetFocusEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxPanel8SetFocusEv"); return 0; }

long ZN7wxPanelD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxPanelD2Ev"); return 0; }

long ZN7wxSizer10GetMinSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer10GetMinSizeEv"); return 0; }

long ZN7wxSizer12DoSetMinSizeEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer12DoSetMinSizeEii"); return 0; }

long ZN7wxSizer12SetDimensionEiiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer12SetDimensionEiiii"); return 0; }

long ZN7wxSizer12SetSizeHintsEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer12SetSizeHintsEP8wxWindow"); return 0; }

long ZN7wxSizer12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer12ms_classInfoE"); return 0; }

long ZN7wxSizer13DeleteWindowsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer13DeleteWindowsEv"); return 0; }

long ZN7wxSizer16DoSetItemMinSizeEP8wxWindowii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer16DoSetItemMinSizeEP8wxWindowii"); return 0; }

long ZN7wxSizer16DoSetItemMinSizeEPS_ii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer16DoSetItemMinSizeEPS_ii"); return 0; }

long ZN7wxSizer16DoSetItemMinSizeEmii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer16DoSetItemMinSizeEmii"); return 0; }

long ZN7wxSizer19SetContainingWindowEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer19SetContainingWindowEP8wxWindow"); return 0; }

long ZN7wxSizer19SetVirtualSizeHintsEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer19SetVirtualSizeHintsEP8wxWindow"); return 0; }

long ZN7wxSizer24ComputeFittingClientSizeEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer24ComputeFittingClientSizeEP8wxWindow"); return 0; }

long ZN7wxSizer24ComputeFittingWindowSizeEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer24ComputeFittingWindowSizeEP8wxWindow"); return 0; }

long ZN7wxSizer3FitEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer3FitEP8wxWindow"); return 0; }

long ZN7wxSizer4ShowEP8wxWindowbb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer4ShowEP8wxWindowbb"); return 0; }

long ZN7wxSizer4ShowEPS_bb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer4ShowEPS_bb"); return 0; }

long ZN7wxSizer4ShowEmb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer4ShowEmb"); return 0; }

long ZN7wxSizer5ClearEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer5ClearEb"); return 0; }

long ZN7wxSizer6DetachEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer6DetachEP8wxWindow"); return 0; }

long ZN7wxSizer6DetachEPS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer6DetachEPS_"); return 0; }

long ZN7wxSizer6DetachEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer6DetachEi"); return 0; }

long ZN7wxSizer6InsertEmP11wxSizerItem(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer6InsertEmP11wxSizerItem"); return 0; }

long ZN7wxSizer6LayoutEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer6LayoutEv"); return 0; }

long ZN7wxSizer6RemoveEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer6RemoveEP8wxWindow"); return 0; }

long ZN7wxSizer6RemoveEPS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer6RemoveEPS_"); return 0; }

long ZN7wxSizer6RemoveEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer6RemoveEi"); return 0; }

long ZN7wxSizer7GetItemEP8wxWindowb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer7GetItemEP8wxWindowb"); return 0; }

long ZN7wxSizer7GetItemEPS_b(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer7GetItemEPS_b"); return 0; }

long ZN7wxSizer7GetItemEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer7GetItemEm"); return 0; }

long ZN7wxSizer7ReplaceEP8wxWindowS1_b(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer7ReplaceEP8wxWindowS1_b"); return 0; }

long ZN7wxSizer7ReplaceEPS_S0_b(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer7ReplaceEPS_S0_b"); return 0; }

long ZN7wxSizer7ReplaceEmP11wxSizerItem(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer7ReplaceEmP11wxSizerItem"); return 0; }

long ZN7wxSizer9FitInsideEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer9FitInsideEP8wxWindow"); return 0; }

long ZN7wxSizer9ShowItemsEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizer9ShowItemsEb"); return 0; }

long ZN7wxSizerD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSizerD2Ev"); return 0; }

long ZN7wxSound4StopEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSound4StopEv"); return 0; }

long ZN7wxSound6CreateERK8wxStringb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSound6CreateERK8wxStringb"); return 0; }

long ZN7wxSoundC1ERK8wxStringb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSoundC1ERK8wxStringb"); return 0; }

long ZN7wxSoundC1EiPKh(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSoundC1EiPKh"); return 0; }

long ZN7wxSoundC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSoundC1Ev"); return 0; }

long ZN7wxSoundD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxSoundD1Ev"); return 0; }

long ZN7wxTimer12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxTimer12ms_classInfoE"); return 0; }

long ZN7wxTimer4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxTimer4InitEv"); return 0; }

long ZN7wxTimer4StopEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxTimer4StopEv"); return 0; }

long ZN7wxTimer5StartEib(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxTimer5StartEib"); return 0; }

long ZN7wxTimerD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxTimerD1Ev"); return 0; }

long ZN7wxTimerD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN7wxTimerD2Ev"); return 0; }

long ZN8wxBitmap10GetRawDataER15wxPixelDataBasei(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxBitmap10GetRawDataER15wxPixelDataBasei"); return 0; }

long ZN8wxBitmap12CopyFromIconERK6wxIcon(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxBitmap12CopyFromIconERK6wxIcon"); return 0; }

long ZN8wxBitmap12UngetRawDataER15wxPixelDataBase(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxBitmap12UngetRawDataER15wxPixelDataBase"); return 0; }

long ZN8wxBitmap8UseAlphaEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxBitmap8UseAlphaEv"); return 0; }

long ZN8wxBitmapC1EPKPKc(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxBitmapC1EPKPKc"); return 0; }

long ZN8wxBitmapC1EPKciii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxBitmapC1EPKciii"); return 0; }

long ZN8wxBitmapC1ERK7wxImagei(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxBitmapC1ERK7wxImagei"); return 0; }

long ZN8wxBitmapC1ERK8wxString12wxBitmapType(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxBitmapC1ERK8wxString12wxBitmapType"); return 0; }

long ZN8wxBitmapC1Eiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxBitmapC1Eiii"); return 0; }

long ZN8wxBitmapC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxBitmapC1Ev"); return 0; }

long ZN8wxBitmapD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxBitmapD1Ev"); return 0; }

long ZN8wxButton10SetDefaultEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxButton10SetDefaultEv"); return 0; }

long ZN8wxButton13MacControlHitEPvS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxButton13MacControlHitEPvS0_"); return 0; }

long ZN8wxButton14GetDefaultSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxButton14GetDefaultSizeEv"); return 0; }

long ZN8wxButton6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxButton6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_"); return 0; }

long ZN8wxButton7CommandER14wxCommandEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxButton7CommandER14wxCommandEvent"); return 0; }

long ZN8wxChoice6CreateEP8wxWindowiRK7wxPointRK6wxSizeRK13wxArrayStringlRK11wxValidatorRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxChoice6CreateEP8wxWindowiRK7wxPointRK6wxSizeRK13wxArrayStringlRK11wxValidatorRK8wxString"); return 0; }

long ZN8wxChoiceD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxChoiceD2Ev"); return 0; }

long ZN8wxColour4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxColour4InitEv"); return 0; }

long ZN8wxColourD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxColourD1Ev"); return 0; }

long ZN8wxCursorC1ERK7wxImage(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxCursorC1ERK7wxImage"); return 0; }

long ZN8wxCursorC1ERK8wxStringlii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxCursorC1ERK8wxStringlii"); return 0; }

long ZN8wxCursorC1Ei(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxCursorC1Ei"); return 0; }

long ZN8wxCursorC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxCursorC1Ev"); return 0; }

long ZN8wxCursorD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxCursorD1Ev"); return 0; }

long ZN8wxDCBase10DrawSplineEiP7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxDCBase10DrawSplineEiP7wxPoint"); return 0; }

long ZN8wxDCBase9DrawLabelERK8wxStringRK8wxBitmapRK6wxRectiiPS6_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxDCBase9DrawLabelERK8wxStringRK8wxBitmapRK6wxRectiiPS6_"); return 0; }

long ZN8wxDialog11IsEscapeKeyERK10wxKeyEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxDialog11IsEscapeKeyERK10wxKeyEvent"); return 0; }

long ZN8wxDialog4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxDialog4InitEv"); return 0; }

long ZN8wxDialog4ShowEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxDialog4ShowEb"); return 0; }

long ZN8wxDialog6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxDialog6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_"); return 0; }

long ZN8wxDialog8EndModalEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxDialog8EndModalEi"); return 0; }

long ZN8wxDialog9ShowModalEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxDialog9ShowModalEv"); return 0; }

long ZN8wxDialogD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxDialogD2Ev"); return 0; }

long ZN8wxLocale10AddCatalogEPKw(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxLocale10AddCatalogEPKw"); return 0; }

long ZN8wxLocale11AddLanguageERK14wxLanguageInfo(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxLocale11AddLanguageERK14wxLanguageInfo"); return 0; }

long ZN8wxLocale11IsAvailableEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxLocale11IsAvailableEi"); return 0; }

long ZN8wxLocale12DoCommonInitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxLocale12DoCommonInitEv"); return 0; }

long ZN8wxLocale15GetLanguageInfoEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxLocale15GetLanguageInfoEi"); return 0; }

long ZN8wxLocale15GetLanguageNameEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxLocale15GetLanguageNameEi"); return 0; }

long ZN8wxLocale16FindLanguageInfoERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxLocale16FindLanguageInfoERK8wxString"); return 0; }

long ZN8wxLocale17GetSystemEncodingEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxLocale17GetSystemEncodingEv"); return 0; }

long ZN8wxLocale17GetSystemLanguageEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxLocale17GetSystemLanguageEv"); return 0; }

long ZN8wxLocale21GetSystemEncodingNameEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxLocale21GetSystemEncodingNameEv"); return 0; }

long ZN8wxLocale26AddCatalogLookupPathPrefixERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxLocale26AddCatalogLookupPathPrefixERK8wxString"); return 0; }

long ZN8wxLocale4InitEPKwS1_S1_bb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxLocale4InitEPKwS1_S1_bb"); return 0; }

long ZN8wxLocale4InitEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxLocale4InitEii"); return 0; }

long ZN8wxLocaleD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxLocaleD2Ev"); return 0; }

long ZN8wxLogGuiC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxLogGuiC1Ev"); return 0; }

long ZN8wxMBConvD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxMBConvD2Ev"); return 0; }

long ZN8wxModule14RegisterModuleEPS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxModule14RegisterModuleEPS_"); return 0; }

long ZN8wxObject12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxObject12ms_classInfoE"); return 0; }

long ZN8wxObject14AllocExclusiveEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxObject14AllocExclusiveEv"); return 0; }

long ZN8wxObject3RefERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxObject3RefERKS_"); return 0; }

long ZN8wxObject5UnRefEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxObject5UnRefEv"); return 0; }

long ZN8wxRegion5ClearEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxRegion5ClearEv"); return 0; }

long ZN8wxRegionC1ERK6wxRect(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxRegionC1ERK6wxRect"); return 0; }

long ZN8wxRegionC1Ellll(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxRegionC1Ellll"); return 0; }

long ZN8wxRegionC1EmPK7wxPointi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxRegionC1EmPK7wxPointi"); return 0; }

long ZN8wxRegionC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxRegionC1Ev"); return 0; }

long ZN8wxRegionD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxRegionD1Ev"); return 0; }

long ZN8wxSlider6CreateEP8wxWindowiiiiRK7wxPointRK6wxSizelRK11wxValidatorRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxSlider6CreateEP8wxWindowiiiiRK7wxPointRK6wxSizelRK11wxValidatorRK8wxString"); return 0; }

long ZN8wxSliderC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxSliderC1Ev"); return 0; }

long ZN8wxString11GetWriteBufEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxString11GetWriteBufEm"); return 0; }

long ZN8wxString13UngetWriteBufEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxString13UngetWriteBufEv"); return 0; }

long ZN8wxString6FormatEPKwz(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxString6FormatEPKwz"); return 0; }

long ZN8wxString6PrintfEPKwz(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxString6PrintfEPKwz"); return 0; }

long ZN8wxString7ReplaceEPKwS1_b(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxString7ReplaceEPKwS1_b"); return 0; }

long ZN8wxString8TruncateEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxString8TruncateEm"); return 0; }

long ZN8wxString9MakeLowerEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxString9MakeLowerEv"); return 0; }

long ZN8wxStringC1EPKcRK8wxMBConvm(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxStringC1EPKcRK8wxMBConvm"); return 0; }

long ZN8wxThread11TestDestroyEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxThread11TestDestroyEv"); return 0; }

long ZN8wxThread6IsMainEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxThread6IsMainEv"); return 0; }

long ZN8wxThreadD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxThreadD2Ev"); return 0; }

long ZN8wxWindow11DoPopupMenuEP6wxMenuii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow11DoPopupMenuEP6wxMenuii"); return 0; }

long ZN8wxWindow11MacDoRedrawEPvl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow11MacDoRedrawEPvl"); return 0; }

long ZN8wxWindow11RemoveChildEP12wxWindowBase(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow11RemoveChildEP12wxWindowBase"); return 0; }

long ZN8wxWindow11WarpPointerEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow11WarpPointerEii"); return 0; }

long ZN8wxWindow12DoMoveWindowEiiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow12DoMoveWindowEiiii"); return 0; }

long ZN8wxWindow12DoSetToolTipEP9wxToolTip(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow12DoSetToolTipEP9wxToolTip"); return 0; }

long ZN8wxWindow12ScrollWindowEiiPK6wxRect(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow12ScrollWindowEiiPK6wxRect"); return 0; }

long ZN8wxWindow12SetScrollPosEiib(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow12SetScrollPosEiib"); return 0; }

long ZN8wxWindow12SetScrollbarEiiiib(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow12SetScrollbarEiiiib"); return 0; }

long ZN8wxWindow12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow12ms_classInfoE"); return 0; }

long ZN8wxWindow13MacChildAddedEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow13MacChildAddedEv"); return 0; }

long ZN8wxWindow13MacControlHitEPvS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow13MacControlHitEPvS0_"); return 0; }

long ZN8wxWindow13SetDropTargetEP12wxDropTarget(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow13SetDropTargetEP12wxDropTarget"); return 0; }

long ZN8wxWindow13sm_eventTableE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow13sm_eventTableE"); return 0; }

long ZN8wxWindow14DoCaptureMouseEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow14DoCaptureMouseEv"); return 0; }

long ZN8wxWindow14DoReleaseMouseEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow14DoReleaseMouseEv"); return 0; }

long ZN8wxWindow14MacSetupCursorERK7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow14MacSetupCursorERK7wxPoint"); return 0; }

long ZN8wxWindow14OnInternalIdleEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow14OnInternalIdleEv"); return 0; }

long ZN8wxWindow14SetTransparentEh(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow14SetTransparentEh"); return 0; }

long ZN8wxWindow15ClearBackgroundEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow15ClearBackgroundEv"); return 0; }

long ZN8wxWindow15DoSetClientSizeEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow15DoSetClientSizeEii"); return 0; }

long ZN8wxWindow15DragAcceptFilesEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow15DragAcceptFilesEb"); return 0; }

long ZN8wxWindow15MacPaintBordersEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow15MacPaintBordersEii"); return 0; }

long ZN8wxWindow16MacHiliteChangedEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow16MacHiliteChangedEv"); return 0; }

long ZN8wxWindow17CanSetTransparentEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow17CanSetTransparentEv"); return 0; }

long ZN8wxWindow18DoSetWindowVariantE15wxWindowVariant(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow18DoSetWindowVariantE15wxWindowVariant"); return 0; }

long ZN8wxWindow19MacGetToolTipStringER7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow19MacGetToolTipStringER7wxPoint"); return 0; }

long ZN8wxWindow19SetBackgroundColourERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow19SetBackgroundColourERK8wxColour"); return 0; }

long ZN8wxWindow19SetForegroundColourERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow19SetForegroundColourERK8wxColour"); return 0; }

long ZN8wxWindow20MacVisibilityChangedEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow20MacVisibilityChangedEv"); return 0; }

long ZN8wxWindow21MacHandleControlClickEP18wxOpaqueControlRefsb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow21MacHandleControlClickEP18wxOpaqueControlRefsb"); return 0; }

long ZN8wxWindow21MacSetBackgroundBrushERK7wxBrush(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow21MacSetBackgroundBrushERK7wxBrush"); return 0; }

long ZN8wxWindow22MacEnabledStateChangedEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow22MacEnabledStateChangedEv"); return 0; }

long ZN8wxWindow22MacGetContentAreaInsetERiS0_S0_S0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow22MacGetContentAreaInsetERiS0_S0_S0_"); return 0; }

long ZN8wxWindow22MacInstallEventHandlerEP18wxOpaqueControlRef(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow22MacInstallEventHandlerEP18wxOpaqueControlRef"); return 0; }

long ZN8wxWindow23MacSuperChangedPositionEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow23MacSuperChangedPositionEv"); return 0; }

long ZN8wxWindow32MacTopLevelWindowChangedPositionEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow32MacTopLevelWindowChangedPositionEv"); return 0; }

long ZN8wxWindow4ShowEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow4ShowEb"); return 0; }

long ZN8wxWindow4ThawEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow4ThawEv"); return 0; }

long ZN8wxWindow5LowerEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow5LowerEv"); return 0; }

long ZN8wxWindow5RaiseEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow5RaiseEv"); return 0; }

long ZN8wxWindow6CreateEPS_iRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow6CreateEPS_iRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN8wxWindow6EnableEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow6EnableEb"); return 0; }

long ZN8wxWindow6FreezeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow6FreezeEv"); return 0; }

long ZN8wxWindow6UpdateEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow6UpdateEv"); return 0; }

long ZN8wxWindow7RefreshEbPK6wxRect(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow7RefreshEbPK6wxRect"); return 0; }

long ZN8wxWindow7SetFontERK6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow7SetFontERK6wxFont"); return 0; }

long ZN8wxWindow8ReparentEP12wxWindowBase(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow8ReparentEP12wxWindowBase"); return 0; }

long ZN8wxWindow8SetFocusEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow8SetFocusEv"); return 0; }

long ZN8wxWindow8SetLabelERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow8SetLabelERK8wxString"); return 0; }

long ZN8wxWindow9DoSetSizeEiiiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow9DoSetSizeEiiiii"); return 0; }

long ZN8wxWindow9SetCursorERK8wxCursor(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindow9SetCursorERK8wxCursor"); return 0; }

long ZN8wxWindowC1EPS_iRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindowC1EPS_iRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN8wxWindowC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindowC1Ev"); return 0; }

long ZN8wxWindowC2EPS_iRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindowC2EPS_iRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN8wxWindowC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindowC2Ev"); return 0; }

long ZN8wxWindowD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWindowD2Ev"); return 0; }

long ZN8wxWizard4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWizard4InitEv"); return 0; }

long ZN8wxWizard6CreateEP8wxWindowiRK8wxStringRK8wxBitmapRK7wxPointl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWizard6CreateEP8wxWindowiRK8wxStringRK8wxBitmapRK7wxPointl"); return 0; }

long ZN8wxWizard8ShowPageEP12wxWizardPageb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWizard8ShowPageEP12wxWizardPageb"); return 0; }

long ZN8wxWizard9SetBitmapERK8wxBitmap(long a, long b, long c_, long d, long e, long f) { shim_note("ZN8wxWizard9SetBitmapERK8wxBitmap"); return 0; }

long ZN9wxAppBase11ProcessIdleEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxAppBase11ProcessIdleEv"); return 0; }

long ZN9wxAppBase12CreateTraitsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxAppBase12CreateTraitsEv"); return 0; }

long ZN9wxAppBase12ExitMainLoopEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxAppBase12ExitMainLoopEv"); return 0; }

long ZN9wxAppBase13OnInitCmdLineER15wxCmdLineParser(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxAppBase13OnInitCmdLineER15wxCmdLineParser"); return 0; }

long ZN9wxAppBase14SendIdleEventsEP8wxWindowR11wxIdleEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxAppBase14SendIdleEventsEP8wxWindowR11wxIdleEvent"); return 0; }

long ZN9wxAppBase15OnCmdLineParsedER15wxCmdLineParser(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxAppBase15OnCmdLineParsedER15wxCmdLineParser"); return 0; }

long ZN9wxAppBase20DeletePendingObjectsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxAppBase20DeletePendingObjectsEv"); return 0; }

long ZN9wxAppBase21OnExceptionInMainLoopEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxAppBase21OnExceptionInMainLoopEv"); return 0; }

long ZN9wxAppBase4ExitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxAppBase4ExitEv"); return 0; }

long ZN9wxAppBase5OnRunEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxAppBase5OnRunEv"); return 0; }

long ZN9wxAppBase6OnExitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxAppBase6OnExitEv"); return 0; }

long ZN9wxAppBase7PendingEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxAppBase7PendingEv"); return 0; }

long ZN9wxAppBase8DispatchEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxAppBase8DispatchEv"); return 0; }

long ZN9wxAppBase8MainLoopEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxAppBase8MainLoopEv"); return 0; }

long ZN9wxAppBase9SetActiveEbP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxAppBase9SetActiveEbP8wxWindow"); return 0; }

long ZN9wxAppBaseD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxAppBaseD2Ev"); return 0; }

long ZN9wxControl12ms_classInfoE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxControl12ms_classInfoE"); return 0; }

long ZN9wxControl14ProcessCommandER14wxCommandEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxControl14ProcessCommandER14wxCommandEvent"); return 0; }

long ZN9wxControl6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK11wxValidatorRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxControl6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK11wxValidatorRK8wxString"); return 0; }

long ZN9wxControlC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxControlC1Ev"); return 0; }

long ZN9wxControlC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxControlC2Ev"); return 0; }

long ZN9wxControlD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxControlD2Ev"); return 0; }

long ZN9wxDisplay10ChangeModeERK11wxVideoMode(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxDisplay10ChangeModeERK11wxVideoMode"); return 0; }

long ZN9wxDisplay12GetFromPointERK7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxDisplay12GetFromPointERK7wxPoint"); return 0; }

long ZN9wxDisplay13GetFromWindowEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxDisplay13GetFromWindowEP8wxWindow"); return 0; }

long ZN9wxDisplay8GetCountEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxDisplay8GetCountEv"); return 0; }

long ZN9wxDisplayC1Ej(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxDisplayC1Ej"); return 0; }

long ZN9wxDisplayD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxDisplayD1Ev"); return 0; }

long ZN9wxEffects10TileBitmapERK6wxRectR4wxDCRK8wxBitmap(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxEffects10TileBitmapERK6wxRectR4wxDCRK8wxBitmap"); return 0; }

long ZN9wxEffects14DrawSunkenEdgeER4wxDCRK6wxRecti(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxEffects14DrawSunkenEdgeER4wxDCRK6wxRecti"); return 0; }

long ZN9wxEffectsC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxEffectsC1Ev"); return 0; }

long ZN9wxListBox10DoSetItemsERK13wxArrayStringPPv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxListBox10DoSetItemsERK13wxArrayStringPPv"); return 0; }

long ZN9wxListBox13DoInsertItemsERK13wxArrayStringj(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxListBox13DoInsertItemsERK13wxArrayStringj"); return 0; }

long ZN9wxListBox13EnsureVisibleEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxListBox13EnsureVisibleEi"); return 0; }

long ZN9wxListBox14DoSetFirstItemEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxListBox14DoSetFirstItemEi"); return 0; }

long ZN9wxListBox14DoSetSelectionEib(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxListBox14DoSetSelectionEib"); return 0; }

long ZN9wxListBox19DoSetItemClientDataEjPv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxListBox19DoSetItemClientDataEjPv"); return 0; }

long ZN9wxListBox21DoSetItemClientObjectEjP12wxClientData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxListBox21DoSetItemClientObjectEjP12wxClientData"); return 0; }

long ZN9wxListBox25GetClassDefaultAttributesE15wxWindowVariant(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxListBox25GetClassDefaultAttributesE15wxWindowVariant"); return 0; }

long ZN9wxListBox5ClearEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxListBox5ClearEv"); return 0; }

long ZN9wxListBox6CreateEP8wxWindowiRK7wxPointRK6wxSizeRK13wxArrayStringlRK11wxValidatorRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxListBox6CreateEP8wxWindowiRK7wxPointRK6wxSizeRK13wxArrayStringlRK11wxValidatorRK8wxString"); return 0; }

long ZN9wxListBox6DeleteEj(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxListBox6DeleteEj"); return 0; }

long ZN9wxListBox7RefreshEbPK6wxRect(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxListBox7RefreshEbPK6wxRect"); return 0; }

long ZN9wxListBox8DoAppendERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxListBox8DoAppendERK8wxString"); return 0; }

long ZN9wxListBox9SetStringEjRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxListBox9SetStringEjRK8wxString"); return 0; }

long ZN9wxListBoxC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxListBoxC1Ev"); return 0; }

long ZN9wxListBoxC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxListBoxC2Ev"); return 0; }

long ZN9wxListBoxD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxListBoxD2Ev"); return 0; }

long ZN9wxMenuBar18s_macCommonMenuBarE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxMenuBar18s_macCommonMenuBarE"); return 0; }

long ZN9wxMenuBar19s_macAutoWindowMenuE(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxMenuBar19s_macAutoWindowMenuE"); return 0; }

long ZN9wxMenuBar8FindMenuERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxMenuBar8FindMenuERK8wxString"); return 0; }

long ZN9wxMenuBarC1El(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxMenuBarC1El"); return 0; }

long ZN9wxOverlay5ResetEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxOverlay5ResetEv"); return 0; }

long ZN9wxOverlayC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxOverlayC1Ev"); return 0; }

long ZN9wxOverlayD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxOverlayD1Ev"); return 0; }

long ZN9wxPaintDCC1EP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxPaintDCC1EP8wxWindow"); return 0; }

long ZN9wxPaintDCC2EP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxPaintDCC2EP8wxWindow"); return 0; }

long ZN9wxPaintDCC2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxPaintDCC2Ev"); return 0; }

long ZN9wxPaintDCD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxPaintDCD1Ev"); return 0; }

long ZN9wxPaintDCD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxPaintDCD2Ev"); return 0; }

long ZN9wxPalette6CreateEiPKhS1_S1_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxPalette6CreateEiPKhS1_S1_"); return 0; }

long ZN9wxPaletteC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxPaletteC1Ev"); return 0; }

long ZN9wxPaletteD1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxPaletteD1Ev"); return 0; }

long ZN9wxPenList15FindOrCreatePenERK8wxColourii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxPenList15FindOrCreatePenERK8wxColourii"); return 0; }

long ZN9wxPenList6AddPenEP5wxPen(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxPenList6AddPenEP5wxPen"); return 0; }

long ZN9wxPenList9RemovePenEP5wxPen(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxPenList9RemovePenEP5wxPen"); return 0; }

long ZN9wxPrinterC1EP17wxPrintDialogData(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxPrinterC1EP17wxPrintDialogData"); return 0; }

long ZN9wxProcess11OnTerminateEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxProcess11OnTerminateEii"); return 0; }

long ZN9wxProcess4InitEP12wxEvtHandlerii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxProcess4InitEP12wxEvtHandlerii"); return 0; }

long ZN9wxProcess4KillEi8wxSignali(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxProcess4KillEi8wxSignali"); return 0; }

long ZN9wxProcess4OpenERK8wxStringi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxProcess4OpenERK8wxStringi"); return 0; }

long ZN9wxProcess6DetachEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxProcess6DetachEv"); return 0; }

long ZN9wxProcess6ExistsEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxProcess6ExistsEi"); return 0; }

long ZN9wxProcessD2Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxProcessD2Ev"); return 0; }

long ZN9wxToolBar19SetToolNormalBitmapEiRK8wxBitmap(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxToolBar19SetToolNormalBitmapEiRK8wxBitmap"); return 0; }

long ZN9wxToolBar21SetToolDisabledBitmapEiRK8wxBitmap(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxToolBar21SetToolDisabledBitmapEiRK8wxBitmap"); return 0; }

long ZN9wxToolBar4InitEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxToolBar4InitEv"); return 0; }

long ZN9wxToolBar6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxToolBar6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN9wxToolTip6EnableEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxToolTip6EnableEb"); return 0; }

long ZN9wxToolTip6SetTipERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxToolTip6SetTipERK8wxString"); return 0; }

long ZN9wxToolTip8SetDelayEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxToolTip8SetDelayEl"); return 0; }

long ZN9wxToolTipC1ERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxToolTipC1ERK8wxString"); return 0; }

long ZN9wxXmlNode16InsertChildAfterEPS_S0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxXmlNode16InsertChildAfterEPS_S0_"); return 0; }

long ZN9wxXmlNodeC1E13wxXmlNodeTypeRK8wxStringS3_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxXmlNodeC1E13wxXmlNodeTypeRK8wxStringS3_"); return 0; }

long ZN9wxXmlNodeC1EPS_13wxXmlNodeTypeRK8wxStringS4_P13wxXmlPropertyS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN9wxXmlNodeC1EPS_13wxXmlNodeTypeRK8wxStringS4_P13wxXmlPropertyS0_"); return 0; }

long ZNK10wxDateTime10GetRataDieEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxDateTime10GetRataDieEv"); return 0; }

long ZNK10wxDateTime11GetDateOnlyEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxDateTime11GetDateOnlyEv"); return 0; }

long ZNK10wxDateTime12GetDayOfYearERKNS_8TimeZoneE(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxDateTime12GetDayOfYearERKNS_8TimeZoneE"); return 0; }

long ZNK10wxDateTime13GetWeekOfYearENS_9WeekFlagsERKNS_8TimeZoneE(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxDateTime13GetWeekOfYearENS_9WeekFlagsERKNS_8TimeZoneE"); return 0; }

long ZNK10wxDateTime14GetWeekOfMonthENS_9WeekFlagsERKNS_8TimeZoneE(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxDateTime14GetWeekOfMonthENS_9WeekFlagsERKNS_8TimeZoneE"); return 0; }

long ZNK10wxDateTime18GetJulianDayNumberEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxDateTime18GetJulianDayNumberEv"); return 0; }

long ZNK10wxDateTime5GetTmERKNS_8TimeZoneE(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxDateTime5GetTmERKNS_8TimeZoneE"); return 0; }

long ZNK10wxDateTime5IsDSTENS_7CountryE(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxDateTime5IsDSTENS_7CountryE"); return 0; }

long ZNK10wxDateTime6FormatEPKwRKNS_8TimeZoneE(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxDateTime6FormatEPKwRKNS_8TimeZoneE"); return 0; }

long ZNK10wxDateTime7GetWeekEtNS_7WeekDayENS_9WeekFlagsE(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxDateTime7GetWeekEtNS_7WeekDayENS_9WeekFlagsE"); return 0; }

long ZNK10wxDateTime9IsWorkDayENS_7CountryE(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxDateTime9IsWorkDayENS_7CountryE"); return 0; }

long ZNK10wxFileName11GetFullPathE12wxPathFormat(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxFileName11GetFullPathE12wxPathFormat"); return 0; }

long ZNK10wxFileType11GetMimeTypeEP8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxFileType11GetMimeTypeEP8wxString"); return 0; }

long ZNK10wxFileType12GetMimeTypesER13wxArrayString(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxFileType12GetMimeTypesER13wxArrayString"); return 0; }

long ZNK10wxFileType14GetAllCommandsEP13wxArrayStringS1_RKNS_17MessageParametersE(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxFileType14GetAllCommandsEP13wxArrayStringS1_RKNS_17MessageParametersE"); return 0; }

long ZNK10wxFileType14GetDescriptionEP8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxFileType14GetDescriptionEP8wxString"); return 0; }

long ZNK10wxFileType14GetOpenCommandEP8wxStringRKNS_17MessageParametersE(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxFileType14GetOpenCommandEP8wxStringRKNS_17MessageParametersE"); return 0; }

long ZNK10wxFileType15GetPrintCommandEP8wxStringRKNS_17MessageParametersE(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxFileType15GetPrintCommandEP8wxStringRKNS_17MessageParametersE"); return 0; }

long ZNK10wxFileType7GetIconEP14wxIconLocation(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxFileType7GetIconEP14wxIconLocation"); return 0; }

long ZNK10wxFontBase14GetStyleStringEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxFontBase14GetStyleStringEv"); return 0; }

long ZNK10wxFontBase15GetFamilyStringEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxFontBase15GetFamilyStringEv"); return 0; }

long ZNK10wxFontBase15GetWeightStringEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxFontBase15GetWeightStringEv"); return 0; }

long ZNK10wxFontBase21GetNativeFontInfoDescEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxFontBase21GetNativeFontInfoDescEv"); return 0; }

long ZNK10wxFontBase25GetNativeFontInfoUserDescEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxFontBase25GetNativeFontInfoUserDescEv"); return 0; }

long ZNK10wxFontBaseeqERK6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxFontBaseeqERK6wxFont"); return 0; }

long ZNK10wxFontBaseneERK6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxFontBaseneERK6wxFont"); return 0; }

long ZNK10wxHtmlCell11GetRootCellEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxHtmlCell11GetRootCellEv"); return 0; }

long ZNK10wxHtmlCell8GetDepthEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxHtmlCell8GetDepthEv"); return 0; }

long ZNK10wxHtmlCell8IsBeforeEPS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxHtmlCell8IsBeforeEPS_"); return 0; }

long ZNK10wxHtmlCell9GetAbsPosEPS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxHtmlCell9GetAbsPosEPS_"); return 0; }

long ZNK10wxJoystick10GetMaxAxesEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick10GetMaxAxesEv"); return 0; }

long ZNK10wxJoystick10HasPOV4DirEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick10HasPOV4DirEv"); return 0; }

long ZNK10wxJoystick11GetPositionEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick11GetPositionEv"); return 0; }

long ZNK10wxJoystick12GetProductIdEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick12GetProductIdEv"); return 0; }

long ZNK10wxJoystick12GetRudderMaxEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick12GetRudderMaxEv"); return 0; }

long ZNK10wxJoystick12GetRudderMinEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick12GetRudderMinEv"); return 0; }

long ZNK10wxJoystick12GetUPositionEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick12GetUPositionEv"); return 0; }

long ZNK10wxJoystick12GetVPositionEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick12GetVPositionEv"); return 0; }

long ZNK10wxJoystick12GetZPositionEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick12GetZPositionEv"); return 0; }

long ZNK10wxJoystick13GetMaxButtonsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick13GetMaxButtonsEv"); return 0; }

long ZNK10wxJoystick13GetNumberAxesEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick13GetNumberAxesEv"); return 0; }

long ZNK10wxJoystick13GetPollingMaxEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick13GetPollingMaxEv"); return 0; }

long ZNK10wxJoystick13GetPollingMinEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick13GetPollingMinEv"); return 0; }

long ZNK10wxJoystick14GetButtonStateEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick14GetButtonStateEv"); return 0; }

long ZNK10wxJoystick14GetPOVPositionEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick14GetPOVPositionEv"); return 0; }

long ZNK10wxJoystick14GetProductNameEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick14GetProductNameEv"); return 0; }

long ZNK10wxJoystick16GetNumberButtonsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick16GetNumberButtonsEv"); return 0; }

long ZNK10wxJoystick17GetManufacturerIdEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick17GetManufacturerIdEv"); return 0; }

long ZNK10wxJoystick17GetPOVCTSPositionEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick17GetPOVCTSPositionEv"); return 0; }

long ZNK10wxJoystick17GetRudderPositionEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick17GetRudderPositionEv"); return 0; }

long ZNK10wxJoystick20GetMovementThresholdEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick20GetMovementThresholdEv"); return 0; }

long ZNK10wxJoystick4HasUEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick4HasUEv"); return 0; }

long ZNK10wxJoystick4HasVEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick4HasVEv"); return 0; }

long ZNK10wxJoystick4HasZEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick4HasZEv"); return 0; }

long ZNK10wxJoystick4IsOkEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick4IsOkEv"); return 0; }

long ZNK10wxJoystick6HasPOVEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick6HasPOVEv"); return 0; }

long ZNK10wxJoystick7GetUMaxEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick7GetUMaxEv"); return 0; }

long ZNK10wxJoystick7GetUMinEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick7GetUMinEv"); return 0; }

long ZNK10wxJoystick7GetVMaxEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick7GetVMaxEv"); return 0; }

long ZNK10wxJoystick7GetVMinEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick7GetVMinEv"); return 0; }

long ZNK10wxJoystick7GetXMaxEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick7GetXMaxEv"); return 0; }

long ZNK10wxJoystick7GetXMinEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick7GetXMinEv"); return 0; }

long ZNK10wxJoystick7GetYMaxEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick7GetYMaxEv"); return 0; }

long ZNK10wxJoystick7GetYMinEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick7GetYMinEv"); return 0; }

long ZNK10wxJoystick7GetZMaxEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick7GetZMaxEv"); return 0; }

long ZNK10wxJoystick7GetZMinEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick7GetZMinEv"); return 0; }

long ZNK10wxJoystick9HasPOVCTSEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick9HasPOVCTSEv"); return 0; }

long ZNK10wxJoystick9HasRudderEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxJoystick9HasRudderEv"); return 0; }

long ZNK10wxListBase4FindEPKv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListBase4FindEPKv"); return 0; }

long ZNK10wxListBase4FindERK9wxListKey(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListBase4FindERK9wxListKey"); return 0; }

long ZNK10wxListBase4ItemEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListBase4ItemEm"); return 0; }

long ZNK10wxListBase7IndexOfEPv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListBase7IndexOfEPv"); return 0; }

long ZNK10wxListCtrl10GetTopItemEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl10GetTopItemEv"); return 0; }

long ZNK10wxListCtrl11GetItemDataEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl11GetItemDataEl"); return 0; }

long ZNK10wxListCtrl11GetItemFontEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl11GetItemFontEl"); return 0; }

long ZNK10wxListCtrl11GetItemRectElR6wxRecti(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl11GetItemRectElR6wxRecti"); return 0; }

long ZNK10wxListCtrl11GetItemTextEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl11GetItemTextEl"); return 0; }

long ZNK10wxListCtrl11GetNextItemElii(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl11GetNextItemElii"); return 0; }

long ZNK10wxListCtrl11GetViewRectEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl11GetViewRectEv"); return 0; }

long ZNK10wxListCtrl12GetImageListEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl12GetImageListEi"); return 0; }

long ZNK10wxListCtrl12GetItemCountEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl12GetItemCountEv"); return 0; }

long ZNK10wxListCtrl12GetItemStateEll(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl12GetItemStateEll"); return 0; }

long ZNK10wxListCtrl12GetScrollPosEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl12GetScrollPosEi"); return 0; }

long ZNK10wxListCtrl13DoGetBestSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl13DoGetBestSizeEv"); return 0; }

long ZNK10wxListCtrl13GetDropTargetEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl13GetDropTargetEv"); return 0; }

long ZNK10wxListCtrl13GetEventTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl13GetEventTableEv"); return 0; }

long ZNK10wxListCtrl13GetTextColourEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl13GetTextColourEv"); return 0; }

long ZNK10wxListCtrl13OnGetItemAttrEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl13OnGetItemAttrEl"); return 0; }

long ZNK10wxListCtrl13OnGetItemTextEll(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl13OnGetItemTextEll"); return 0; }

long ZNK10wxListCtrl14GetColumnCountEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl14GetColumnCountEv"); return 0; }

long ZNK10wxListCtrl14GetColumnWidthEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl14GetColumnWidthEi"); return 0; }

long ZNK10wxListCtrl14GetEditControlEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl14GetEditControlEv"); return 0; }

long ZNK10wxListCtrl14GetItemSpacingEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl14GetItemSpacingEv"); return 0; }

long ZNK10wxListCtrl15GetCountPerPageEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl15GetCountPerPageEv"); return 0; }

long ZNK10wxListCtrl15GetItemPositionElR7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl15GetItemPositionElR7wxPoint"); return 0; }

long ZNK10wxListCtrl17GetEventHashTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl17GetEventHashTableEv"); return 0; }

long ZNK10wxListCtrl17GetItemTextColourEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl17GetItemTextColourEl"); return 0; }

long ZNK10wxListCtrl20GetSelectedItemCountEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl20GetSelectedItemCountEv"); return 0; }

long ZNK10wxListCtrl20OnGetItemColumnImageEll(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl20OnGetItemColumnImageEll"); return 0; }

long ZNK10wxListCtrl23GetItemBackgroundColourEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl23GetItemBackgroundColourEl"); return 0; }

long ZNK10wxListCtrl7GetItemER10wxListItem(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl7GetItemER10wxListItem"); return 0; }

long ZNK10wxListCtrl7HitTestERK7wxPointRiPl(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl7HitTestERK7wxPointRiPl"); return 0; }

long ZNK10wxListCtrl9GetColumnEiR10wxListItem(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxListCtrl9GetColumnEiR10wxListItem"); return 0; }

long ZNK10wxMenuBase10GetMenuBarEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxMenuBase10GetMenuBarEv"); return 0; }

long ZNK10wxMenuBase13FindChildItemEiPm(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxMenuBase13FindChildItemEiPm"); return 0; }

long ZNK10wxMenuBase18FindItemByPositionEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxMenuBase18FindItemByPositionEm"); return 0; }

long ZNK10wxMenuBase8FindItemEiPP6wxMenu(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxMenuBase8FindItemEiPP6wxMenu"); return 0; }

long ZNK10wxMenuBase8GetLabelEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxMenuBase8GetLabelEi"); return 0; }

long ZNK10wxMenuBase9IsCheckedEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxMenuBase9IsCheckedEi"); return 0; }

long ZNK10wxMenuBase9IsEnabledEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxMenuBase9IsEnabledEi"); return 0; }

long ZNK10wxMetafile4IsOkEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxMetafile4IsOkEv"); return 0; }

long ZNK10wxMetafile7GetSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxMetafile7GetSizeEv"); return 0; }

long ZNK10wxPrintout18GetLogicalPageRectEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxPrintout18GetLogicalPageRectEv"); return 0; }

long ZNK10wxPrintout19GetLogicalPaperRectEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxPrintout19GetLogicalPaperRectEv"); return 0; }

long ZNK10wxPrintout25GetLogicalPageMarginsRectERK21wxPageSetupDialogData(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxPrintout25GetLogicalPageMarginsRectERK21wxPageSetupDialogData"); return 0; }

long ZNK10wxSpinCtrl6GetMaxEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxSpinCtrl6GetMaxEv"); return 0; }

long ZNK10wxSpinCtrl6GetMinEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxSpinCtrl6GetMinEv"); return 0; }

long ZNK10wxSpinCtrl8GetValueEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxSpinCtrl8GetValueEv"); return 0; }

long ZNK10wxTextCtrl10IsEditableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxTextCtrl10IsEditableEv"); return 0; }

long ZNK10wxTextCtrl10IsModifiedEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxTextCtrl10IsModifiedEv"); return 0; }

long ZNK10wxTextCtrl11GetLineTextEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxTextCtrl11GetLineTextEl"); return 0; }

long ZNK10wxTextCtrl12AcceptsFocusEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxTextCtrl12AcceptsFocusEv"); return 0; }

long ZNK10wxTextCtrl12GetClassInfoEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxTextCtrl12GetClassInfoEv"); return 0; }

long ZNK10wxTextCtrl12GetSelectionEPlS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxTextCtrl12GetSelectionEPlS0_"); return 0; }

long ZNK10wxTextCtrl12PositionToXYElPlS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxTextCtrl12PositionToXYElPlS0_"); return 0; }

long ZNK10wxTextCtrl12XYToPositionEll(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxTextCtrl12XYToPositionEll"); return 0; }

long ZNK10wxTextCtrl13DoGetBestSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxTextCtrl13DoGetBestSizeEv"); return 0; }

long ZNK10wxTextCtrl13GetLineLengthEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxTextCtrl13GetLineLengthEl"); return 0; }

long ZNK10wxTextCtrl15GetLastPositionEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxTextCtrl15GetLastPositionEv"); return 0; }

long ZNK10wxTextCtrl16GetNumberOfLinesEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxTextCtrl16GetNumberOfLinesEv"); return 0; }

long ZNK10wxTextCtrl17GetInsertionPointEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxTextCtrl17GetInsertionPointEv"); return 0; }

long ZNK10wxTextCtrl6CanCutEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxTextCtrl6CanCutEv"); return 0; }

long ZNK10wxTextCtrl7CanCopyEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxTextCtrl7CanCopyEv"); return 0; }

long ZNK10wxTextCtrl7CanRedoEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxTextCtrl7CanRedoEv"); return 0; }

long ZNK10wxTextCtrl7CanUndoEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxTextCtrl7CanUndoEv"); return 0; }

long ZNK10wxTextCtrl8CanPasteEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxTextCtrl8CanPasteEv"); return 0; }

long ZNK10wxTextCtrl8GetValueEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxTextCtrl8GetValueEv"); return 0; }

long ZNK10wxTimeSpan6FormatEPKw(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxTimeSpan6FormatEPKw"); return 0; }

long ZNK10wxTreebook13GetPageParentEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxTreebook13GetPageParentEm"); return 0; }

long ZNK10wxVListBox10IsSelectedEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxVListBox10IsSelectedEm"); return 0; }

long ZNK10wxVListBox13GetEventTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxVListBox13GetEventTableEv"); return 0; }

long ZNK10wxVListBox15GetNextSelectedERm(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxVListBox15GetNextSelectedERm"); return 0; }

long ZNK10wxVListBox15OnDrawSeparatorER4wxDCR6wxRectm(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxVListBox15OnDrawSeparatorER4wxDCR6wxRectm"); return 0; }

long ZNK10wxVListBox15OnGetLineHeightEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxVListBox15OnGetLineHeightEm"); return 0; }

long ZNK10wxVListBox16GetFirstSelectedERm(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxVListBox16GetFirstSelectedERm"); return 0; }

long ZNK10wxVListBox16GetSelectedCountEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxVListBox16GetSelectedCountEv"); return 0; }

long ZNK10wxVListBox16OnDrawBackgroundER4wxDCRK6wxRectm(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxVListBox16OnDrawBackgroundER4wxDCRK6wxRectm"); return 0; }

long ZNK10wxVListBox17GetEventHashTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxVListBox17GetEventHashTableEv"); return 0; }

long ZNK10wxWindowDC13DoGetAsBitmapEPK6wxRect(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxWindowDC13DoGetAsBitmapEPK6wxRect"); return 0; }

long ZNK10wxWindowDC9DoGetSizeEPiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK10wxWindowDC9DoGetSizeEPiS0_"); return 0; }

long ZNK11wxAnimation12GetFrameSizeEj(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK11wxAnimation12GetFrameSizeEj"); return 0; }

long ZNK11wxAnimation16GetFramePositionEj(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK11wxAnimation16GetFramePositionEj"); return 0; }

long ZNK11wxAnimation17GetDisposalMethodEj(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK11wxAnimation17GetDisposalMethodEj"); return 0; }

long ZNK11wxAnimation19GetBackgroundColourEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK11wxAnimation19GetBackgroundColourEv"); return 0; }

long ZNK11wxAnimation20GetTransparentColourEj(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK11wxAnimation20GetTransparentColourEj"); return 0; }

long ZNK11wxFrameBase11IsOneOfBarsEPK8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK11wxFrameBase11IsOneOfBarsEPK8wxWindow"); return 0; }

long ZNK11wxGDIObject12GetClassInfoEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK11wxGDIObject12GetClassInfoEv"); return 0; }

long ZNK11wxImageList7GetIconEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK11wxImageList7GetIconEi"); return 0; }

long ZNK11wxImageList9GetBitmapEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK11wxImageList9GetBitmapEi"); return 0; }

long ZNK11wxLogWindow8GetFrameEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK11wxLogWindow8GetFrameEv"); return 0; }

long ZNK11wxPrintData4IsOkEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK11wxPrintData4IsOkEv"); return 0; }

long ZNK11wxScrollBar16GetThumbPositionEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK11wxScrollBar16GetThumbPositionEv"); return 0; }

long ZNK11wxSizerItem20GetMinSizeWithBorderEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK11wxSizerItem20GetMinSizeWithBorderEv"); return 0; }

long ZNK11wxSizerItem7IsShownEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK11wxSizerItem7IsShownEv"); return 0; }

long ZNK11wxSizerItem9GetSpacerEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK11wxSizerItem9GetSpacerEv"); return 0; }

long ZNK11wxStopWatch14GetElapsedTimeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK11wxStopWatch14GetElapsedTimeEv"); return 0; }

long ZNK11wxStopWatch4TimeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK11wxStopWatch4TimeEv"); return 0; }

long ZNK12wxAppConsole11HandleEventEP12wxEvtHandlerMS0_FvR7wxEventES3_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxAppConsole11HandleEventEP12wxEvtHandlerMS0_FvR7wxEventES3_"); return 0; }

long ZNK12wxAuiManager14GetArtProviderEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxAuiManager14GetArtProviderEv"); return 0; }

long ZNK12wxAuiManager16GetManagedWindowEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxAuiManager16GetManagedWindowEv"); return 0; }

long ZNK12wxAuiManager21GetDockSizeConstraintEPdS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxAuiManager21GetDockSizeConstraintEPdS0_"); return 0; }

long ZNK12wxAuiManager8GetFlagsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxAuiManager8GetFlagsEv"); return 0; }

long ZNK12wxConfigBase13ExpandEnvVarsERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxConfigBase13ExpandEnvVarsERK8wxString"); return 0; }

long ZNK12wxConfigBase4ReadERK8wxStringPS0_S2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxConfigBase4ReadERK8wxStringPS0_S2_"); return 0; }

long ZNK12wxConfigBase4ReadERK8wxStringPbb(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxConfigBase4ReadERK8wxStringPbb"); return 0; }

long ZNK12wxConfigBase4ReadERK8wxStringPdd(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxConfigBase4ReadERK8wxStringPdd"); return 0; }

long ZNK12wxConfigBase4ReadERK8wxStringPll(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxConfigBase4ReadERK8wxStringPll"); return 0; }

long ZNK12wxDataFormat5GetIdEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxDataFormat5GetIdEv"); return 0; }

long ZNK12wxDataFormateqERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxDataFormateqERKS_"); return 0; }

long ZNK12wxDataObject17IsSupportedFormatERK12wxDataFormatN16wxDataObjectBase9DirectionE(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxDataObject17IsSupportedFormatERK12wxDataFormatN16wxDataObjectBase9DirectionE"); return 0; }

long ZNK12wxDialogBase12AcceptsFocusEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxDialogBase12AcceptsFocusEv"); return 0; }

long ZNK12wxDialogBase13GetEventTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxDialogBase13GetEventTableEv"); return 0; }

long ZNK12wxDialogBase17GetEventHashTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxDialogBase17GetEventHashTableEv"); return 0; }

long ZNK12wxEvtHandler13GetEventTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxEvtHandler13GetEventTableEv"); return 0; }

long ZNK12wxEvtHandler15DoGetClientDataEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxEvtHandler15DoGetClientDataEv"); return 0; }

long ZNK12wxEvtHandler17DoGetClientObjectEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxEvtHandler17DoGetClientObjectEv"); return 0; }

long ZNK12wxEvtHandler17GetEventHashTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxEvtHandler17GetEventHashTableEv"); return 0; }

long ZNK12wxHtmlWindow13GetEventTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxHtmlWindow13GetEventTableEv"); return 0; }

long ZNK12wxHtmlWindow13GetHTMLCursorEN21wxHtmlWindowInterface10HTMLCursorE(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxHtmlWindow13GetHTMLCursorEN21wxHtmlWindowInterface10HTMLCursorE"); return 0; }

long ZNK12wxHtmlWindow16OnHTMLOpeningURLE13wxHtmlURLTypeRK8wxStringPS1_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxHtmlWindow16OnHTMLOpeningURLE13wxHtmlURLTypeRK8wxStringPS1_"); return 0; }

long ZNK12wxHtmlWindow17GetEventHashTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxHtmlWindow17GetEventHashTableEv"); return 0; }

long ZNK12wxHtmlWindow18HTMLCoordsToWindowEP10wxHtmlCellRK7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxHtmlWindow18HTMLCoordsToWindowEP10wxHtmlCellRK7wxPoint"); return 0; }

long ZNK12wxHtmlWindow23GetHTMLBackgroundColourEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxHtmlWindow23GetHTMLBackgroundColourEv"); return 0; }

long ZNK12wxIconBundle7GetIconERK6wxSize(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxIconBundle7GetIconERK6wxSize"); return 0; }

long ZNK12wxMouseEvent10ButtonDownEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxMouseEvent10ButtonDownEi"); return 0; }

long ZNK12wxMouseEvent12ButtonDClickEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxMouseEvent12ButtonDClickEi"); return 0; }

long ZNK12wxMouseEvent12ButtonIsDownEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxMouseEvent12ButtonIsDownEi"); return 0; }

long ZNK12wxMouseEvent18GetLogicalPositionERK4wxDC(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxMouseEvent18GetLogicalPositionERK4wxDC"); return 0; }

long ZNK12wxMouseEvent6ButtonEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxMouseEvent6ButtonEi"); return 0; }

long ZNK12wxMouseEvent8ButtonUpEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxMouseEvent8ButtonUpEi"); return 0; }

long ZNK12wxMouseEvent9GetButtonEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxMouseEvent9GetButtonEv"); return 0; }

long ZNK12wxRegionBase15ConvertToBitmapEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxRegionBase15ConvertToBitmapEv"); return 0; }

long ZNK12wxRegionBase7IsEqualERK8wxRegion(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxRegionBase7IsEqualERK8wxRegion"); return 0; }

long ZNK12wxSearchCtrl18GetDescriptiveTextEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxSearchCtrl18GetDescriptiveTextEv"); return 0; }

long ZNK12wxStreamBase7GetSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxStreamBase7GetSizeEv"); return 0; }

long ZNK12wxWindowBase10FindWindowERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxWindowBase10FindWindowERK8wxString"); return 0; }

long ZNK12wxWindowBase10FindWindowEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxWindowBase10FindWindowEl"); return 0; }

long ZNK12wxWindowBase10IsTopLevelEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxWindowBase10IsTopLevelEv"); return 0; }

long ZNK12wxWindowBase11DoIsExposedEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxWindowBase11DoIsExposedEii"); return 0; }

long ZNK12wxWindowBase11DoIsExposedEiiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxWindowBase11DoIsExposedEiiii"); return 0; }

long ZNK12wxWindowBase15IsShownOnScreenEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxWindowBase15IsShownOnScreenEv"); return 0; }

long ZNK12wxWindowBase16DoGetVirtualSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxWindowBase16DoGetVirtualSizeEv"); return 0; }

long ZNK12wxWindowBase16GetDefaultBorderEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxWindowBase16GetDefaultBorderEv"); return 0; }

long ZNK12wxWindowBase17GetSizeConstraintEPiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxWindowBase17GetSizeConstraintEPiS0_"); return 0; }

long ZNK12wxWindowBase18ClientToWindowSizeERK6wxSize(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxWindowBase18ClientToWindowSizeERK6wxSize"); return 0; }

long ZNK12wxWindowBase18GetHelpTextAtPointERK7wxPointN11wxHelpEvent6OriginE(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxWindowBase18GetHelpTextAtPointERK7wxPointN11wxHelpEvent6OriginE"); return 0; }

long ZNK12wxWindowBase18WindowToClientSizeERK6wxSize(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxWindowBase18WindowToClientSizeERK6wxSize"); return 0; }

long ZNK12wxWindowBase19DoGetScreenPositionEPiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxWindowBase19DoGetScreenPositionEPiS0_"); return 0; }

long ZNK12wxWindowBase19GetBackgroundColourEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxWindowBase19GetBackgroundColourEv"); return 0; }

long ZNK12wxWindowBase19GetEffectiveMinSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxWindowBase19GetEffectiveMinSizeEv"); return 0; }

long ZNK12wxWindowBase19GetForegroundColourEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxWindowBase19GetForegroundColourEv"); return 0; }

long ZNK12wxWindowBase19GetUpdateClientRectEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxWindowBase19GetUpdateClientRectEv"); return 0; }

long ZNK12wxWindowBase19GetWindowBorderSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxWindowBase19GetWindowBorderSizeEv"); return 0; }

long ZNK12wxWindowBase21GetPositionConstraintEPiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxWindowBase21GetPositionConstraintEPiS0_"); return 0; }

long ZNK12wxWindowBase23GetClientSizeConstraintEPiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxWindowBase23GetClientSizeConstraintEPiS0_"); return 0; }

long ZNK12wxWindowBase24AdjustForLayoutDirectionEiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxWindowBase24AdjustForLayoutDirectionEiii"); return 0; }

long ZNK12wxWindowBase27AdjustForParentClientOriginERiS0_i(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxWindowBase27AdjustForParentClientOriginERiS0_i"); return 0; }

long ZNK12wxWindowBase7GetFontEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxWindowBase7GetFontEv"); return 0; }

long ZNK12wxWindowBase9DoHitTestEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxWindowBase9DoHitTestEii"); return 0; }

long ZNK12wxWindowBase9GetBorderEl(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK12wxWindowBase9GetBorderEl"); return 0; }

long ZNK13wxArtProvider12GetClassInfoEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxArtProvider12GetClassInfoEv"); return 0; }

long ZNK13wxAuiNotebook11GetPageTextEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxAuiNotebook11GetPageTextEm"); return 0; }

long ZNK13wxAuiNotebook12GetPageCountEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxAuiNotebook12GetPageCountEv"); return 0; }

long ZNK13wxAuiNotebook12GetPageIndexEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxAuiNotebook12GetPageIndexEP8wxWindow"); return 0; }

long ZNK13wxAuiNotebook12GetSelectionEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxAuiNotebook12GetSelectionEv"); return 0; }

long ZNK13wxAuiNotebook13GetPageBitmapEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxAuiNotebook13GetPageBitmapEm"); return 0; }

long ZNK13wxAuiNotebook14GetArtProviderEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxAuiNotebook14GetArtProviderEv"); return 0; }

long ZNK13wxAuiNotebook16GetTabCtrlHeightEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxAuiNotebook16GetTabCtrlHeightEv"); return 0; }

long ZNK13wxAuiNotebook7GetPageEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxAuiNotebook7GetPageEm"); return 0; }

long ZNK13wxHtmlListBox10OnDrawItemER4wxDCRK6wxRectm(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxHtmlListBox10OnDrawItemER4wxDCRK6wxRectm"); return 0; }

long ZNK13wxHtmlListBox13GetEventTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxHtmlListBox13GetEventTableEv"); return 0; }

long ZNK13wxHtmlListBox13GetHTMLCursorEN21wxHtmlWindowInterface10HTMLCursorE(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxHtmlListBox13GetHTMLCursorEN21wxHtmlWindowInterface10HTMLCursorE"); return 0; }

long ZNK13wxHtmlListBox13OnMeasureItemEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxHtmlListBox13OnMeasureItemEm"); return 0; }

long ZNK13wxHtmlListBox15OnGetItemMarkupEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxHtmlListBox15OnGetItemMarkupEm"); return 0; }

long ZNK13wxHtmlListBox16OnHTMLOpeningURLE13wxHtmlURLTypeRK8wxStringPS1_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxHtmlListBox16OnHTMLOpeningURLE13wxHtmlURLTypeRK8wxStringPS1_"); return 0; }

long ZNK13wxHtmlListBox17GetEventHashTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxHtmlListBox17GetEventHashTableEv"); return 0; }

long ZNK13wxHtmlListBox18HTMLCoordsToWindowEP10wxHtmlCellRK7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxHtmlListBox18HTMLCoordsToWindowEP10wxHtmlCellRK7wxPoint"); return 0; }

long ZNK13wxHtmlListBox21GetSelectedTextColourERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxHtmlListBox21GetSelectedTextColourERK8wxColour"); return 0; }

long ZNK13wxHtmlListBox23GetHTMLBackgroundColourEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxHtmlListBox23GetHTMLBackgroundColourEv"); return 0; }

long ZNK13wxHtmlListBox23GetSelectedTextBgColourERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxHtmlListBox23GetSelectedTextBgColourERK8wxColour"); return 0; }

long ZNK13wxInputStream3EofEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxInputStream3EofEv"); return 0; }

long ZNK13wxInputStream5TellIEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxInputStream5TellIEv"); return 0; }

long ZNK13wxInputStream7CanReadEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxInputStream7CanReadEv"); return 0; }

long ZNK13wxMenuBarBase13GetHelpStringEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxMenuBarBase13GetHelpStringEi"); return 0; }

long ZNK13wxMenuBarBase16GetMenuLabelTextEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxMenuBarBase16GetMenuLabelTextEm"); return 0; }

long ZNK13wxMenuBarBase7GetMenuEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxMenuBarBase7GetMenuEm"); return 0; }

long ZNK13wxMenuBarBase8GetLabelEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxMenuBarBase8GetLabelEi"); return 0; }

long ZNK13wxMenuBarBase9IsCheckedEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxMenuBarBase9IsCheckedEi"); return 0; }

long ZNK13wxMenuBarBase9IsEnabledEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxMenuBarBase9IsEnabledEi"); return 0; }

long ZNK13wxNotifyEvent12GetClassInfoEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxNotifyEvent12GetClassInfoEv"); return 0; }

long ZNK13wxTaskBarIcon15IsIconInstalledEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxTaskBarIcon15IsIconInstalledEv"); return 0; }

long ZNK13wxToolBarBase8FindByIdEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK13wxToolBarBase8FindByIdEi"); return 0; }

long ZNK14wxBaseArrayInt5IndexEib(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxBaseArrayInt5IndexEib"); return 0; }

long ZNK14wxBookCtrlBase11GetNextPageEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxBookCtrlBase11GetNextPageEb"); return 0; }

long ZNK14wxCalendarCtrl14GetYearControlEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxCalendarCtrl14GetYearControlEv"); return 0; }

long ZNK14wxCalendarCtrl15GetMonthControlEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxCalendarCtrl15GetMonthControlEv"); return 0; }

long ZNK14wxCommandEvent9GetStringEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxCommandEvent9GetStringEv"); return 0; }

long ZNK14wxGraphicsPath15GetCurrentPointEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxGraphicsPath15GetCurrentPointEv"); return 0; }

long ZNK14wxGraphicsPath6GetBoxEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxGraphicsPath6GetBoxEv"); return 0; }

long ZNK14wxGraphicsPath8ContainsERK15wxPoint2DDoublei(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxGraphicsPath8ContainsERK15wxPoint2DDoublei"); return 0; }

long ZNK14wxGridBagSizer11GetCellSizeEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxGridBagSizer11GetCellSizeEii"); return 0; }

long ZNK14wxGridCellAttr11GetRendererEP6wxGridii(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxGridCellAttr11GetRendererEP6wxGridii"); return 0; }

long ZNK14wxGridCellAttr12GetAlignmentEPiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxGridCellAttr12GetAlignmentEPiS0_"); return 0; }

long ZNK14wxGridCellAttr13GetTextColourEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxGridCellAttr13GetTextColourEv"); return 0; }

long ZNK14wxGridCellAttr19GetBackgroundColourEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxGridCellAttr19GetBackgroundColourEv"); return 0; }

long ZNK14wxGridCellAttr5CloneEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxGridCellAttr5CloneEv"); return 0; }

long ZNK14wxGridCellAttr7GetFontEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxGridCellAttr7GetFontEv"); return 0; }

long ZNK14wxGridCellAttr7GetSizeEPiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxGridCellAttr7GetSizeEPiS0_"); return 0; }

long ZNK14wxGridCellAttr9GetEditorEP6wxGridii(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxGridCellAttr9GetEditorEP6wxGridii"); return 0; }

long ZNK14wxImageHandler12GetClassInfoEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxImageHandler12GetClassInfoEv"); return 0; }

long ZNK14wxOutputStream5TellOEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxOutputStream5TellOEv"); return 0; }

long ZNK14wxPlatformInfoeqERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxPlatformInfoeqERKS_"); return 0; }

long ZNK14wxPreviewFrame13GetEventTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxPreviewFrame13GetEventTableEv"); return 0; }

long ZNK14wxPreviewFrame17GetEventHashTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxPreviewFrame17GetEventHashTableEv"); return 0; }

long ZNK14wxPrintPreview10GetMaxPageEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxPrintPreview10GetMaxPageEv"); return 0; }

long ZNK14wxPrintPreview10GetMinPageEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxPrintPreview10GetMinPageEv"); return 0; }

long ZNK14wxPrintPreview11GetPrintoutEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxPrintPreview11GetPrintoutEv"); return 0; }

long ZNK14wxPrintPreview14GetCurrentPageEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxPrintPreview14GetCurrentPageEv"); return 0; }

long ZNK14wxPrintPreview22GetPrintoutForPrintingEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxPrintPreview22GetPrintoutForPrintingEv"); return 0; }

long ZNK14wxPrintPreview4IsOkEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxPrintPreview4IsOkEv"); return 0; }

long ZNK14wxPrintPreview7GetZoomEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxPrintPreview7GetZoomEv"); return 0; }

long ZNK14wxPrintPreview8GetFrameEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxPrintPreview8GetFrameEv"); return 0; }

long ZNK14wxPrintPreview9GetCanvasEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxPrintPreview9GetCanvasEv"); return 0; }

long ZNK14wxRadioBoxBase11GetNextItemEi11wxDirectionl(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxRadioBoxBase11GetNextItemEi11wxDirectionl"); return 0; }

long ZNK14wxRadioBoxBase15GetItemHelpTextEj(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxRadioBoxBase15GetItemHelpTextEj"); return 0; }

long ZNK14wxRect2DDouble10IntersectsERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxRect2DDouble10IntersectsERKS_"); return 0; }

long ZNK14wxRichTextAttrcv12wxTextAttrExEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxRichTextAttrcv12wxTextAttrExEv"); return 0; }

long ZNK14wxRichTextCtrl17GetSelectionRangeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxRichTextCtrl17GetSelectionRangeEv"); return 0; }

long ZNK14wxRichTextLine16GetAbsoluteRangeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxRichTextLine16GetAbsoluteRangeEv"); return 0; }

long ZNK14wxScrollHelper12GetViewStartEPiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxScrollHelper12GetViewStartEPiS0_"); return 0; }

long ZNK14wxScrollHelper15GetTargetWindowEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxScrollHelper15GetTargetWindowEv"); return 0; }

long ZNK14wxScrollHelper17GetScrollPageSizeEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxScrollHelper17GetScrollPageSizeEi"); return 0; }

long ZNK14wxScrollHelper20SendAutoScrollEventsER16wxScrollWinEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxScrollHelper20SendAutoScrollEventsER16wxScrollWinEvent"); return 0; }

long ZNK14wxScrollHelper22DoCalcScrolledPositionEiiPiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxScrollHelper22DoCalcScrolledPositionEiiPiS0_"); return 0; }

long ZNK14wxScrollHelper22GetScrollPixelsPerUnitEPiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxScrollHelper22GetScrollPixelsPerUnitEPiS0_"); return 0; }

long ZNK14wxScrollHelper24DoCalcUnscrolledPositionEiiPiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxScrollHelper24DoCalcUnscrolledPositionEiiPiS0_"); return 0; }

long ZNK14wxScrollHelper24ScrollGetBestVirtualSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxScrollHelper24ScrollGetBestVirtualSizeEv"); return 0; }

long ZNK14wxScrollHelper33ScrollGetWindowSizeForVirtualSizeERK6wxSize(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxScrollHelper33ScrollGetWindowSizeForVirtualSizeERK6wxSize"); return 0; }

long ZNK14wxTextCtrlBase15GetDefaultStyleEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxTextCtrlBase15GetDefaultStyleEv"); return 0; }

long ZNK14wxTextCtrlBase18GetStringSelectionEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxTextCtrlBase18GetStringSelectionEv"); return 0; }

long ZNK14wxTextCtrlBase7HitTestERK7wxPointPl(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxTextCtrlBase7HitTestERK7wxPointPl"); return 0; }

long ZNK14wxTextCtrlBase7HitTestERK7wxPointPlS3_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxTextCtrlBase7HitTestERK7wxPointPlS3_"); return 0; }

long ZNK14wxTextCtrlBase8GetRangeEll(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxTextCtrlBase8GetRangeEll"); return 0; }

long ZNK14wxTreeCtrlBase7IsEmptyEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK14wxTreeCtrlBase7IsEmptyEv"); return 0; }

long ZNK15wxComboCtrlBase13DoGetBestSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK15wxComboCtrlBase13DoGetBestSizeEv"); return 0; }

long ZNK15wxComboCtrlBase15GetLastPositionEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK15wxComboCtrlBase15GetLastPositionEv"); return 0; }

long ZNK15wxComboCtrlBase17GetInsertionPointEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK15wxComboCtrlBase17GetInsertionPointEv"); return 0; }

long ZNK15wxComboCtrlBase17PrepareBackgroundER4wxDCRK6wxRecti(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK15wxComboCtrlBase17PrepareBackgroundER4wxDCRK6wxRecti"); return 0; }

long ZNK15wxComboCtrlBase19GetNativeTextIndentEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK15wxComboCtrlBase19GetNativeTextIndentEv"); return 0; }

long ZNK15wxComboCtrlBase8GetValueEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK15wxComboCtrlBase8GetValueEv"); return 0; }

long ZNK15wxGridTableBase12GetClassInfoEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK15wxGridTableBase12GetClassInfoEv"); return 0; }

long ZNK15wxItemContainer15GetClientObjectEj(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK15wxItemContainer15GetClientObjectEj"); return 0; }

long ZNK15wxPoint2DDouble14GetVectorAngleEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK15wxPoint2DDouble14GetVectorAngleEv"); return 0; }

long ZNK16wxColourDatabase4FindERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxColourDatabase4FindERK8wxString"); return 0; }

long ZNK16wxColourDatabase8FindNameERK8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxColourDatabase8FindNameERK8wxColour"); return 0; }

long ZNK16wxDataObjectBase11IsSupportedERK12wxDataFormatNS_9DirectionE(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxDataObjectBase11IsSupportedERK12wxDataFormatNS_9DirectionE"); return 0; }

long ZNK16wxGraphicsObject11GetRendererEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxGraphicsObject11GetRendererEv"); return 0; }

long ZNK16wxGraphicsObject6IsNullEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxGraphicsObject6IsNullEv"); return 0; }

long ZNK16wxHtmlBookRecord11GetFullPathERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxHtmlBookRecord11GetFullPathERK8wxString"); return 0; }

long ZNK16wxHtmlTagsModule12GetClassInfoEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxHtmlTagsModule12GetClassInfoEv"); return 0; }

long ZNK16wxImageHistogram21FindFirstUnusedColourEPhS0_S0_hhh(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxImageHistogram21FindFirstUnusedColourEPhS0_S0_hhh"); return 0; }

long ZNK16wxMDIParentFrame14GetActiveChildEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxMDIParentFrame14GetActiveChildEv"); return 0; }

long ZNK16wxNativeFontInfo11GetEncodingEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxNativeFontInfo11GetEncodingEv"); return 0; }

long ZNK16wxNativeFontInfo11GetFaceNameEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxNativeFontInfo11GetFaceNameEv"); return 0; }

long ZNK16wxNativeFontInfo12GetPointSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxNativeFontInfo12GetPointSizeEv"); return 0; }

long ZNK16wxNativeFontInfo12ToUserStringEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxNativeFontInfo12ToUserStringEv"); return 0; }

long ZNK16wxNativeFontInfo13GetUnderlinedEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxNativeFontInfo13GetUnderlinedEv"); return 0; }

long ZNK16wxNativeFontInfo8GetStyleEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxNativeFontInfo8GetStyleEv"); return 0; }

long ZNK16wxNativeFontInfo8ToStringEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxNativeFontInfo8ToStringEv"); return 0; }

long ZNK16wxNativeFontInfo9GetFamilyEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxNativeFontInfo9GetFamilyEv"); return 0; }

long ZNK16wxNativeFontInfo9GetWeightEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxNativeFontInfo9GetWeightEv"); return 0; }

long ZNK16wxRegionIterator4GetHEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxRegionIterator4GetHEv"); return 0; }

long ZNK16wxRegionIterator4GetWEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxRegionIterator4GetWEv"); return 0; }

long ZNK16wxRegionIterator4GetXEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxRegionIterator4GetXEv"); return 0; }

long ZNK16wxRegionIterator4GetYEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxRegionIterator4GetYEv"); return 0; }

long ZNK16wxRichTextBuffer23GetStyleForNewParagraphElbb(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxRichTextBuffer23GetStyleForNewParagraphElbb"); return 0; }

long ZNK16wxRichTextObject9GetBufferEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxRichTextObject9GetBufferEv"); return 0; }

long ZNK16wxScrolledWindow13GetEventTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxScrolledWindow13GetEventTableEv"); return 0; }

long ZNK16wxScrolledWindow17GetEventHashTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxScrolledWindow17GetEventHashTableEv"); return 0; }

long ZNK16wxSplitterWindow11GetSashSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxSplitterWindow11GetSashSizeEv"); return 0; }

long ZNK16wxSplitterWindow13GetBorderSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxSplitterWindow13GetBorderSizeEv"); return 0; }

long ZNK16wxTextDataObject11GetDataHereERK12wxDataFormatPv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxTextDataObject11GetDataHereERK12wxDataFormatPv"); return 0; }

long ZNK16wxTextDataObject11GetDataSizeERK12wxDataFormat(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxTextDataObject11GetDataSizeERK12wxDataFormat"); return 0; }

long ZNK16wxTextDataObject13GetAllFormatsEP12wxDataFormatN16wxDataObjectBase9DirectionE(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK16wxTextDataObject13GetAllFormatsEP12wxDataFormatN16wxDataObjectBase9DirectionE"); return 0; }

long ZNK17wxAboutDialogInfo24GetDescriptionAndCreditsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxAboutDialogInfo24GetDescriptionAndCreditsEv"); return 0; }

long ZNK17wxAboutDialogInfo7GetIconEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxAboutDialogInfo7GetIconEv"); return 0; }

long ZNK17wxAuiTabContainer10TabHitTestEiiPP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxAuiTabContainer10TabHitTestEiiPP8wxWindow"); return 0; }

long ZNK17wxAuiTabContainer12GetPageCountEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxAuiTabContainer12GetPageCountEv"); return 0; }

long ZNK17wxAuiTabContainer12GetTabOffsetEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxAuiTabContainer12GetTabOffsetEv"); return 0; }

long ZNK17wxAuiTabContainer13ButtonHitTestEiiPP23wxAuiTabContainerButton(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxAuiTabContainer13ButtonHitTestEiiPP23wxAuiTabContainerButton"); return 0; }

long ZNK17wxAuiTabContainer13GetActivePageEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxAuiTabContainer13GetActivePageEv"); return 0; }

long ZNK17wxAuiTabContainer14GetArtProviderEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxAuiTabContainer14GetArtProviderEv"); return 0; }

long ZNK17wxAuiTabContainer16GetIdxFromWindowEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxAuiTabContainer16GetIdxFromWindowEP8wxWindow"); return 0; }

long ZNK17wxAuiTabContainer16GetWindowFromIdxEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxAuiTabContainer16GetWindowFromIdxEm"); return 0; }

long ZNK17wxAuiTabContainer8GetFlagsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxAuiTabContainer8GetFlagsEv"); return 0; }

long ZNK17wxGenericTreeCtrl10IsExpandedERK12wxTreeItemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGenericTreeCtrl10IsExpandedERK12wxTreeItemId"); return 0; }

long ZNK17wxGenericTreeCtrl10IsSelectedERK12wxTreeItemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGenericTreeCtrl10IsSelectedERK12wxTreeItemId"); return 0; }

long ZNK17wxGenericTreeCtrl11GetItemDataERK12wxTreeItemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGenericTreeCtrl11GetItemDataERK12wxTreeItemId"); return 0; }

long ZNK17wxGenericTreeCtrl11GetItemFontERK12wxTreeItemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGenericTreeCtrl11GetItemFontERK12wxTreeItemId"); return 0; }

long ZNK17wxGenericTreeCtrl11GetItemTextERK12wxTreeItemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGenericTreeCtrl11GetItemTextERK12wxTreeItemId"); return 0; }

long ZNK17wxGenericTreeCtrl12GetItemImageERK12wxTreeItemId14wxTreeItemIcon(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGenericTreeCtrl12GetItemImageERK12wxTreeItemId14wxTreeItemIcon"); return 0; }

long ZNK17wxGenericTreeCtrl12GetLastChildERK12wxTreeItemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGenericTreeCtrl12GetLastChildERK12wxTreeItemId"); return 0; }

long ZNK17wxGenericTreeCtrl12GetNextChildERK12wxTreeItemIdRPv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGenericTreeCtrl12GetNextChildERK12wxTreeItemIdRPv"); return 0; }

long ZNK17wxGenericTreeCtrl13DoGetBestSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGenericTreeCtrl13DoGetBestSizeEv"); return 0; }

long ZNK17wxGenericTreeCtrl13DoTreeHitTestERK7wxPointRi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGenericTreeCtrl13DoTreeHitTestERK7wxPointRi"); return 0; }

long ZNK17wxGenericTreeCtrl13GetEventTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGenericTreeCtrl13GetEventTableEv"); return 0; }

long ZNK17wxGenericTreeCtrl13GetFirstChildERK12wxTreeItemIdRPv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGenericTreeCtrl13GetFirstChildERK12wxTreeItemIdRPv"); return 0; }

long ZNK17wxGenericTreeCtrl13GetItemParentERK12wxTreeItemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGenericTreeCtrl13GetItemParentERK12wxTreeItemId"); return 0; }

long ZNK17wxGenericTreeCtrl13GetSelectionsER18wxArrayTreeItemIds(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGenericTreeCtrl13GetSelectionsER18wxArrayTreeItemIds"); return 0; }

long ZNK17wxGenericTreeCtrl14GetEditControlEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGenericTreeCtrl14GetEditControlEv"); return 0; }

long ZNK17wxGenericTreeCtrl14GetNextSiblingERK12wxTreeItemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGenericTreeCtrl14GetNextSiblingERK12wxTreeItemId"); return 0; }

long ZNK17wxGenericTreeCtrl14GetNextVisibleERK12wxTreeItemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGenericTreeCtrl14GetNextVisibleERK12wxTreeItemId"); return 0; }

long ZNK17wxGenericTreeCtrl14GetPrevSiblingERK12wxTreeItemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGenericTreeCtrl14GetPrevSiblingERK12wxTreeItemId"); return 0; }

long ZNK17wxGenericTreeCtrl14GetPrevVisibleERK12wxTreeItemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGenericTreeCtrl14GetPrevVisibleERK12wxTreeItemId"); return 0; }

long ZNK17wxGenericTreeCtrl15GetBoundingRectERK12wxTreeItemIdR6wxRectb(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGenericTreeCtrl15GetBoundingRectERK12wxTreeItemIdR6wxRectb"); return 0; }

long ZNK17wxGenericTreeCtrl15ItemHasChildrenERK12wxTreeItemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGenericTreeCtrl15ItemHasChildrenERK12wxTreeItemId"); return 0; }

long ZNK17wxGenericTreeCtrl16GetChildrenCountERK12wxTreeItemIdb(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGenericTreeCtrl16GetChildrenCountERK12wxTreeItemIdb"); return 0; }

long ZNK17wxGenericTreeCtrl17GetEventHashTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGenericTreeCtrl17GetEventHashTableEv"); return 0; }

long ZNK17wxGenericTreeCtrl17GetItemTextColourERK12wxTreeItemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGenericTreeCtrl17GetItemTextColourERK12wxTreeItemId"); return 0; }

long ZNK17wxGenericTreeCtrl19GetFirstVisibleItemEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGenericTreeCtrl19GetFirstVisibleItemEv"); return 0; }

long ZNK17wxGenericTreeCtrl23GetItemBackgroundColourERK12wxTreeItemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGenericTreeCtrl23GetItemBackgroundColourERK12wxTreeItemId"); return 0; }

long ZNK17wxGenericTreeCtrl6IsBoldERK12wxTreeItemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGenericTreeCtrl6IsBoldERK12wxTreeItemId"); return 0; }

long ZNK17wxGenericTreeCtrl8GetCountEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGenericTreeCtrl8GetCountEv"); return 0; }

long ZNK17wxGenericTreeCtrl9IsVisibleERK12wxTreeItemId(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGenericTreeCtrl9IsVisibleERK12wxTreeItemId"); return 0; }

long ZNK17wxGraphicsContext10CreatePathEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxGraphicsContext10CreatePathEv"); return 0; }

long ZNK17wxTaskBarIconBase13GetEventTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxTaskBarIconBase13GetEventTableEv"); return 0; }

long ZNK17wxTaskBarIconBase17GetEventHashTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxTaskBarIconBase17GetEventHashTableEv"); return 0; }

long ZNK17wxVScrolledWindow13GetEventTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxVScrolledWindow13GetEventTableEv"); return 0; }

long ZNK17wxVScrolledWindow14GetLinesHeightEmm(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxVScrolledWindow14GetLinesHeightEmm"); return 0; }

long ZNK17wxVScrolledWindow17GetEventHashTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxVScrolledWindow17GetEventHashTableEv"); return 0; }

long ZNK17wxVScrolledWindow19EstimateTotalHeightEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxVScrolledWindow19EstimateTotalHeightEv"); return 0; }

long ZNK17wxVScrolledWindow7HitTestEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK17wxVScrolledWindow7HitTestEii"); return 0; }

long ZNK18wxAcceleratorEntry8ToStringEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK18wxAcceleratorEntry8ToStringEv"); return 0; }

long ZNK18wxAcceleratorTable4IsOkEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK18wxAcceleratorTable4IsOkEv"); return 0; }

long ZNK18wxAuiFloatingFrame15GetOwnerManagerEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK18wxAuiFloatingFrame15GetOwnerManagerEv"); return 0; }

long ZNK18wxAuiMDIChildFrame17GetMDIParentFrameEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK18wxAuiMDIChildFrame17GetMDIParentFrameEv"); return 0; }

long ZNK18wxBaseArrayPtrVoid5IndexEPKvb(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK18wxBaseArrayPtrVoid5IndexEPKvb"); return 0; }

long ZNK18wxBitmapDataObject11GetDataHereEPv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK18wxBitmapDataObject11GetDataHereEPv"); return 0; }

long ZNK18wxBitmapDataObject11GetDataHereERK12wxDataFormatPv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK18wxBitmapDataObject11GetDataHereERK12wxDataFormatPv"); return 0; }

long ZNK18wxBitmapDataObject11GetDataSizeERK12wxDataFormat(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK18wxBitmapDataObject11GetDataSizeERK12wxDataFormat"); return 0; }

long ZNK18wxBitmapDataObject11GetDataSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK18wxBitmapDataObject11GetDataSizeEv"); return 0; }

long ZNK18wxBitmapDataObject13GetAllFormatsEP12wxDataFormatN16wxDataObjectBase9DirectionE(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK18wxBitmapDataObject13GetAllFormatsEP12wxDataFormatN16wxDataObjectBase9DirectionE"); return 0; }

long ZNK18wxBitmapDataObject14GetFormatCountEN16wxDataObjectBase9DirectionE(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK18wxBitmapDataObject14GetFormatCountEN16wxDataObjectBase9DirectionE"); return 0; }

long ZNK18wxGenericComboCtrl13GetEventTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK18wxGenericComboCtrl13GetEventTableEv"); return 0; }

long ZNK18wxGenericComboCtrl16IsKeyPopupToggleERK10wxKeyEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK18wxGenericComboCtrl16IsKeyPopupToggleERK10wxKeyEvent"); return 0; }

long ZNK18wxGenericComboCtrl17GetEventHashTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK18wxGenericComboCtrl17GetEventHashTableEv"); return 0; }

long ZNK18wxRichTextPrinting13GetFooterTextE21wxRichTextOddEvenPage22wxRichTextPageLocation(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK18wxRichTextPrinting13GetFooterTextE21wxRichTextOddEvenPage22wxRichTextPageLocation"); return 0; }

long ZNK18wxRichTextPrinting13GetHeaderTextE21wxRichTextOddEvenPage22wxRichTextPageLocation(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK18wxRichTextPrinting13GetHeaderTextE21wxRichTextOddEvenPage22wxRichTextPageLocation"); return 0; }

long ZNK19wxAuiMDIParentFrame11GetNotebookEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK19wxAuiMDIParentFrame11GetNotebookEv"); return 0; }

long ZNK19wxAuiMDIParentFrame14GetActiveChildEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK19wxAuiMDIParentFrame14GetActiveChildEv"); return 0; }

long ZNK19wxAuiMDIParentFrame15GetClientWindowEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK19wxAuiMDIParentFrame15GetClientWindowEv"); return 0; }

long ZNK19wxEncodingConverter7ConvertERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK19wxEncodingConverter7ConvertERK8wxString"); return 0; }

long ZNK19wxFileSystemHandler11GetProtocolERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK19wxFileSystemHandler11GetProtocolERK8wxString"); return 0; }

long ZNK19wxFileSystemHandler12GetClassInfoEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK19wxFileSystemHandler12GetClassInfoEv"); return 0; }

long ZNK19wxFileSystemHandler15GetLeftLocationERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK19wxFileSystemHandler15GetLeftLocationERK8wxString"); return 0; }

long ZNK19wxFileSystemHandler16GetRightLocationERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK19wxFileSystemHandler16GetRightLocationERK8wxString"); return 0; }

long ZNK19wxFileSystemHandler9GetAnchorERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK19wxFileSystemHandler9GetAnchorERK8wxString"); return 0; }

long ZNK19wxHtmlContainerCell14GetIndentUnitsEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK19wxHtmlContainerCell14GetIndentUnitsEi"); return 0; }

long ZNK19wxHtmlContainerCell9GetIndentEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK19wxHtmlContainerCell9GetIndentEi"); return 0; }

long ZNK19wxPreviewControlBar13GetEventTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK19wxPreviewControlBar13GetEventTableEv"); return 0; }

long ZNK19wxPreviewControlBar17GetEventHashTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK19wxPreviewControlBar17GetEventHashTableEv"); return 0; }

long ZNK19wxRichTextParagraph21GetCombinedAttributesERK12wxTextAttrEx(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK19wxRichTextParagraph21GetCombinedAttributesERK12wxTextAttrEx"); return 0; }

long ZNK19wxRichTextParagraph21GetCombinedAttributesEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK19wxRichTextParagraph21GetCombinedAttributesEv"); return 0; }

long ZNK19wxTopLevelWindowMac10IsIconizedEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK19wxTopLevelWindowMac10IsIconizedEv"); return 0; }

long ZNK19wxTopLevelWindowMac11IsMaximizedEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK19wxTopLevelWindowMac11IsMaximizedEv"); return 0; }

long ZNK19wxTopLevelWindowMac12IsFullScreenEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK19wxTopLevelWindowMac12IsFullScreenEv"); return 0; }

long ZNK19wxTopLevelWindowMac13DoGetPositionEPiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK19wxTopLevelWindowMac13DoGetPositionEPiS0_"); return 0; }

long ZNK19wxTopLevelWindowMac15DoGetClientSizeEPiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK19wxTopLevelWindowMac15DoGetClientSizeEPiS0_"); return 0; }

long ZNK19wxTopLevelWindowMac19GetClientAreaOriginEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK19wxTopLevelWindowMac19GetClientAreaOriginEv"); return 0; }

long ZNK19wxTopLevelWindowMac21MacGetMetalAppearanceEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK19wxTopLevelWindowMac21MacGetMetalAppearanceEv"); return 0; }

long ZNK19wxTopLevelWindowMac23MacGetUnifiedAppearanceEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK19wxTopLevelWindowMac23MacGetUnifiedAppearanceEv"); return 0; }

long ZNK19wxTopLevelWindowMac8GetTitleEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK19wxTopLevelWindowMac8GetTitleEv"); return 0; }

long ZNK19wxTopLevelWindowMac9DoGetSizeEPiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK19wxTopLevelWindowMac9DoGetSizeEPiS0_"); return 0; }

long ZNK20wxNativeEncodingInfo8ToStringEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK20wxNativeEncodingInfo8ToStringEv"); return 0; }

long ZNK20wxOwnerDrawnComboBox10FindStringERK8wxStringb(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK20wxOwnerDrawnComboBox10FindStringERK8wxStringb"); return 0; }

long ZNK20wxOwnerDrawnComboBox10OnDrawItemER4wxDCRK6wxRectii(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK20wxOwnerDrawnComboBox10OnDrawItemER4wxDCRK6wxRectii"); return 0; }

long ZNK20wxOwnerDrawnComboBox12GetClassInfoEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK20wxOwnerDrawnComboBox12GetClassInfoEv"); return 0; }

long ZNK20wxOwnerDrawnComboBox12GetSelectionEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK20wxOwnerDrawnComboBox12GetSelectionEv"); return 0; }

long ZNK20wxOwnerDrawnComboBox13GetEventTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK20wxOwnerDrawnComboBox13GetEventTableEv"); return 0; }

long ZNK20wxOwnerDrawnComboBox13OnMeasureItemEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK20wxOwnerDrawnComboBox13OnMeasureItemEm"); return 0; }

long ZNK20wxOwnerDrawnComboBox16OnDrawBackgroundER4wxDCRK6wxRectii(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK20wxOwnerDrawnComboBox16OnDrawBackgroundER4wxDCRK6wxRectii"); return 0; }

long ZNK20wxOwnerDrawnComboBox17GetEventHashTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK20wxOwnerDrawnComboBox17GetEventHashTableEv"); return 0; }

long ZNK20wxOwnerDrawnComboBox18OnMeasureItemWidthEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK20wxOwnerDrawnComboBox18OnMeasureItemWidthEm"); return 0; }

long ZNK20wxOwnerDrawnComboBox19DoGetItemClientDataEj(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK20wxOwnerDrawnComboBox19DoGetItemClientDataEj"); return 0; }

long ZNK20wxOwnerDrawnComboBox21DoGetItemClientObjectEj(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK20wxOwnerDrawnComboBox21DoGetItemClientObjectEj"); return 0; }

long ZNK20wxOwnerDrawnComboBox8GetCountEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK20wxOwnerDrawnComboBox8GetCountEv"); return 0; }

long ZNK20wxOwnerDrawnComboBox9GetStringEj(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK20wxOwnerDrawnComboBox9GetStringEj"); return 0; }

long ZNK20wxTopLevelWindowBase16DoClientToScreenEPiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK20wxTopLevelWindowBase16DoClientToScreenEPiS0_"); return 0; }

long ZNK20wxTopLevelWindowBase16DoScreenToClientEPiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK20wxTopLevelWindowBase16DoScreenToClientEPiS0_"); return 0; }

long ZNK20wxTopLevelWindowBase17IsAlwaysMaximizedEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK20wxTopLevelWindowBase17IsAlwaysMaximizedEv"); return 0; }

long ZNK20wxXmlResourceHandler12GetClassInfoEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK20wxXmlResourceHandler12GetClassInfoEv"); return 0; }

long ZNK21wxClientDataContainer15DoGetClientDataEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK21wxClientDataContainer15DoGetClientDataEv"); return 0; }

long ZNK21wxClientDataContainer17DoGetClientObjectEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK21wxClientDataContainer17DoGetClientObjectEv"); return 0; }

long ZNK21wxDataObjectComposite17GetReceivedFormatEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK21wxDataObjectComposite17GetReceivedFormatEv"); return 0; }

long ZNK22wxGridCellAttrProvider7GetAttrEiiN14wxGridCellAttr10wxAttrKindE(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK22wxGridCellAttrProvider7GetAttrEiiN14wxGridCellAttr10wxAttrKindE"); return 0; }

long ZNK23wxFileDirPickerCtrlBase7GetPathEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK23wxFileDirPickerCtrlBase7GetPathEv"); return 0; }

long ZNK23wxSingleInstanceChecker16IsAnotherRunningEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK23wxSingleInstanceChecker16IsAnotherRunningEv"); return 0; }

long ZNK24wxItemContainerImmutable10GetStringsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK24wxItemContainerImmutable10GetStringsEv"); return 0; }

long ZNK24wxItemContainerImmutable18GetStringSelectionEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK24wxItemContainerImmutable18GetStringSelectionEv"); return 0; }

long ZNK25wxRichTextCompositeObject13GetChildCountEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK25wxRichTextCompositeObject13GetChildCountEv"); return 0; }

long ZNK25wxRichTextCompositeObject8GetChildEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK25wxRichTextCompositeObject8GetChildEm"); return 0; }

long ZNK28wxIndividualLayoutConstraint7GetEdgeE6wxEdgeP12wxWindowBaseS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK28wxIndividualLayoutConstraint7GetEdgeE6wxEdgeP12wxWindowBaseS2_"); return 0; }

long ZNK28wxRichTextParagraphLayoutBox15GetInvalidRangeEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK28wxRichTextParagraphLayoutBox15GetInvalidRangeEb"); return 0; }

long ZNK4wxDC10DoGetPixelEiiP8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK4wxDC10DoGetPixelEiiP8wxColour"); return 0; }

long ZNK4wxDC11DoGetSizeMMEPiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK4wxDC11DoGetSizeMMEPiS0_"); return 0; }

long ZNK4wxDC12GetCharWidthEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK4wxDC12GetCharWidthEv"); return 0; }

long ZNK4wxDC12GetClassInfoEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK4wxDC12GetClassInfoEv"); return 0; }

long ZNK4wxDC13CanDrawBitmapEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK4wxDC13CanDrawBitmapEv"); return 0; }

long ZNK4wxDC13GetCharHeightEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK4wxDC13GetCharHeightEv"); return 0; }

long ZNK4wxDC15DoGetTextExtentERK8wxStringPiS3_S3_S3_P6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK4wxDC15DoGetTextExtentERK8wxStringPiS3_S3_S3_P6wxFont"); return 0; }

long ZNK4wxDC16CanGetTextExtentEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK4wxDC16CanGetTextExtentEv"); return 0; }

long ZNK4wxDC23DoGetPartialTextExtentsERK8wxStringR10wxArrayInt(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK4wxDC23DoGetPartialTextExtentsERK8wxStringR10wxArrayInt"); return 0; }

long ZNK4wxDC6GetPPIEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK4wxDC6GetPPIEv"); return 0; }

long ZNK4wxDC8GetDepthEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK4wxDC8GetDepthEv"); return 0; }

long ZNK5wxApp13GetEventTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK5wxApp13GetEventTableEv"); return 0; }

long ZNK5wxApp17GetEventHashTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK5wxApp17GetEventHashTableEv"); return 0; }

long ZNK6wxFont11GetEncodingEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxFont11GetEncodingEv"); return 0; }

long ZNK6wxFont11GetFaceNameEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxFont11GetFaceNameEv"); return 0; }

long ZNK6wxFont12GetPointSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxFont12GetPointSizeEv"); return 0; }

long ZNK6wxFont13GetUnderlinedEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxFont13GetUnderlinedEv"); return 0; }

long ZNK6wxFont8GetStyleEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxFont8GetStyleEv"); return 0; }

long ZNK6wxFont9GetFamilyEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxFont9GetFamilyEv"); return 0; }

long ZNK6wxGrid10IsReadOnlyEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxGrid10IsReadOnlyEii"); return 0; }

long ZNK6wxGrid13IsInSelectionEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxGrid13IsInSelectionEii"); return 0; }

long ZNK6wxGrid15GetSelectedColsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxGrid15GetSelectedColsEv"); return 0; }

long ZNK6wxGrid15GetSelectedRowsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxGrid15GetSelectedRowsEv"); return 0; }

long ZNK6wxGrid16GetDefaultEditorEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxGrid16GetDefaultEditorEv"); return 0; }

long ZNK6wxGrid16GetSelectedCellsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxGrid16GetSelectedCellsEv"); return 0; }

long ZNK6wxGrid16GetSelectionModeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxGrid16GetSelectionModeEv"); return 0; }

long ZNK6wxGrid18GetDefaultRendererEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxGrid18GetDefaultRendererEv"); return 0; }

long ZNK6wxGrid19GetOrCreateCellAttrEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxGrid19GetOrCreateCellAttrEii"); return 0; }

long ZNK6wxGrid20CanEnableCellControlEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxGrid20CanEnableCellControlEv"); return 0; }

long ZNK6wxGrid21IsCurrentCellReadOnlyEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxGrid21IsCurrentCellReadOnlyEv"); return 0; }

long ZNK6wxGrid22IsCellEditControlShownEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxGrid22IsCellEditControlShownEv"); return 0; }

long ZNK6wxGrid24GetSelectionBlockTopLeftEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxGrid24GetSelectionBlockTopLeftEv"); return 0; }

long ZNK6wxGrid24IsCellEditControlEnabledEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxGrid24IsCellEditControlEnabledEv"); return 0; }

long ZNK6wxGrid28GetColMinimalAcceptableWidthEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxGrid28GetColMinimalAcceptableWidthEv"); return 0; }

long ZNK6wxGrid28GetSelectionBlockBottomRightEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxGrid28GetSelectionBlockBottomRightEv"); return 0; }

long ZNK6wxGrid29GetRowMinimalAcceptableHeightEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxGrid29GetRowMinimalAcceptableHeightEv"); return 0; }

long ZNK6wxIcon4IsOkEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxIcon4IsOkEv"); return 0; }

long ZNK6wxIcon8GetDepthEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxIcon8GetDepthEv"); return 0; }

long ZNK6wxIcon8GetWidthEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxIcon8GetWidthEv"); return 0; }

long ZNK6wxIcon9GetHeightEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxIcon9GetHeightEv"); return 0; }

long ZNK6wxRect10IntersectsERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxRect10IntersectsERKS_"); return 0; }

long ZNK6wxRect8ContainsERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxRect8ContainsERKS_"); return 0; }

long ZNK6wxRect8ContainsEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxRect8ContainsEii"); return 0; }

long ZNK6wxRecteqERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxRecteqERKS_"); return 0; }

long ZNK6wxRectplERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK6wxRectplERKS_"); return 0; }

long ZNK7wxBrush10GetStippleEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxBrush10GetStippleEv"); return 0; }

long ZNK7wxBrush11MacGetThemeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxBrush11MacGetThemeEv"); return 0; }

long ZNK7wxBrush9GetColourEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxBrush9GetColourEv"); return 0; }

long ZNK7wxEvent12GetClassInfoEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxEvent12GetClassInfoEv"); return 0; }

long ZNK7wxFrame15DoGetClientSizeEPiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxFrame15DoGetClientSizeEPiS0_"); return 0; }

long ZNK7wxFrame19GetClientAreaOriginEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxFrame19GetClientAreaOriginEv"); return 0; }

long ZNK7wxFrame22MacIsChildOfClientAreaEPK8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxFrame22MacIsChildOfClientAreaEPK8wxWindow"); return 0; }

long ZNK7wxImage10GetMaskRedEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage10GetMaskRedEv"); return 0; }

long ZNK7wxImage11GetMaskBlueEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage11GetMaskBlueEv"); return 0; }

long ZNK7wxImage11GetSubImageERK6wxRect(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage11GetSubImageERK6wxRect"); return 0; }

long ZNK7wxImage11ResampleBoxEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage11ResampleBoxEii"); return 0; }

long ZNK7wxImage12CountColoursEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage12CountColoursEm"); return 0; }

long ZNK7wxImage12GetMaskGreenEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage12GetMaskGreenEv"); return 0; }

long ZNK7wxImage12GetOptionIntERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage12GetOptionIntERK8wxString"); return 0; }

long ZNK7wxImage13ConvertToMonoEhhh(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage13ConvertToMonoEhhh"); return 0; }

long ZNK7wxImage13IsTransparentEiih(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage13IsTransparentEiih"); return 0; }

long ZNK7wxImage15ResampleBicubicEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage15ResampleBicubicEii"); return 0; }

long ZNK7wxImage16ComputeHistogramER16wxImageHistogram(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage16ComputeHistogramER16wxImageHistogram"); return 0; }

long ZNK7wxImage18ConvertToGreyscaleEddd(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage18ConvertToGreyscaleEddd"); return 0; }

long ZNK7wxImage19GetOrFindMaskColourEPhS0_S0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage19GetOrFindMaskColourEPhS0_S0_"); return 0; }

long ZNK7wxImage21FindFirstUnusedColourEPhS0_S0_hhh(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage21FindFirstUnusedColourEPhS0_S0_hhh"); return 0; }

long ZNK7wxImage4CopyEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage4CopyEv"); return 0; }

long ZNK7wxImage4IsOkEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage4IsOkEv"); return 0; }

long ZNK7wxImage4SizeERK6wxSizeRK7wxPointiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage4SizeERK6wxSizeRK7wxPointiii"); return 0; }

long ZNK7wxImage5ScaleEiii(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage5ScaleEiii"); return 0; }

long ZNK7wxImage6GetRedEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage6GetRedEii"); return 0; }

long ZNK7wxImage6MirrorEb(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage6MirrorEb"); return 0; }

long ZNK7wxImage6RotateEdRK7wxPointbPS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage6RotateEdRK7wxPointbPS0_"); return 0; }

long ZNK7wxImage7GetBlueEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage7GetBlueEii"); return 0; }

long ZNK7wxImage7GetDataEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage7GetDataEv"); return 0; }

long ZNK7wxImage7HasMaskEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage7HasMaskEv"); return 0; }

long ZNK7wxImage8GetAlphaEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage8GetAlphaEii"); return 0; }

long ZNK7wxImage8GetAlphaEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage8GetAlphaEv"); return 0; }

long ZNK7wxImage8GetGreenEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage8GetGreenEii"); return 0; }

long ZNK7wxImage8GetWidthEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage8GetWidthEv"); return 0; }

long ZNK7wxImage8Rotate90Eb(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage8Rotate90Eb"); return 0; }

long ZNK7wxImage8ShrinkByEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage8ShrinkByEii"); return 0; }

long ZNK7wxImage9GetHeightEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage9GetHeightEv"); return 0; }

long ZNK7wxImage9GetOptionERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage9GetOptionERK8wxString"); return 0; }

long ZNK7wxImage9HasOptionERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxImage9HasOptionERK8wxString"); return 0; }

long ZNK7wxPanel12AcceptsFocusEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxPanel12AcceptsFocusEv"); return 0; }

long ZNK7wxPanel13GetEventTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxPanel13GetEventTableEv"); return 0; }

long ZNK7wxPanel17GetEventHashTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxPanel17GetEventHashTableEv"); return 0; }

long ZNK7wxSizer7IsShownEP8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxSizer7IsShownEP8wxWindow"); return 0; }

long ZNK7wxSizer7IsShownEPS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxSizer7IsShownEPS_"); return 0; }

long ZNK7wxSizer7IsShownEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxSizer7IsShownEm"); return 0; }

long ZNK7wxTimer12GetClassInfoEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxTimer12GetClassInfoEv"); return 0; }

long ZNK7wxTimer9IsRunningEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK7wxTimer9IsRunningEv"); return 0; }

long ZNK8wxBitmap14ConvertToImageEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxBitmap14ConvertToImageEv"); return 0; }

long ZNK8wxBitmap8HasAlphaEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxBitmap8HasAlphaEv"); return 0; }

long ZNK8wxButton12GetClassInfoEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxButton12GetClassInfoEv"); return 0; }

long ZNK8wxButton13DoGetBestSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxButton13DoGetBestSizeEv"); return 0; }

long ZNK8wxDCBase16DeviceToLogicalXEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxDCBase16DeviceToLogicalXEi"); return 0; }

long ZNK8wxDCBase16DeviceToLogicalYEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxDCBase16DeviceToLogicalYEi"); return 0; }

long ZNK8wxDCBase16LogicalToDeviceXEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxDCBase16LogicalToDeviceXEi"); return 0; }

long ZNK8wxDCBase16LogicalToDeviceYEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxDCBase16LogicalToDeviceYEi"); return 0; }

long ZNK8wxDCBase19DeviceToLogicalXRelEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxDCBase19DeviceToLogicalXRelEi"); return 0; }

long ZNK8wxDCBase19DeviceToLogicalYRelEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxDCBase19DeviceToLogicalYRelEi"); return 0; }

long ZNK8wxDCBase19LogicalToDeviceXRelEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxDCBase19LogicalToDeviceXRelEi"); return 0; }

long ZNK8wxDCBase19LogicalToDeviceYRelEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxDCBase19LogicalToDeviceYRelEi"); return 0; }

long ZNK8wxDCBase22GetMultiLineTextExtentERK8wxStringPiS3_S3_P6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxDCBase22GetMultiLineTextExtentERK8wxStringPiS3_S3_P6wxFont"); return 0; }

long ZNK8wxDialog12GetClassInfoEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxDialog12GetClassInfoEv"); return 0; }

long ZNK8wxDialog7IsModalEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxDialog7IsModalEv"); return 0; }

long ZNK8wxLocale10GetSysNameEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxLocale10GetSysNameEv"); return 0; }

long ZNK8wxLocale8IsLoadedEPKw(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxLocale8IsLoadedEPKw"); return 0; }

long ZNK8wxLocale9GetStringEPKwS1_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxLocale9GetStringEPKwS1_"); return 0; }

long ZNK8wxLocale9GetStringEPKwS1_mS1_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxLocale9GetStringEPKwS1_mS1_"); return 0; }

long ZNK8wxObject12CloneRefDataEPK15wxObjectRefData(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxObject12CloneRefDataEPK15wxObjectRefData"); return 0; }

long ZNK8wxObject12GetClassInfoEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxObject12GetClassInfoEv"); return 0; }

long ZNK8wxObject13CreateRefDataEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxObject13CreateRefDataEv"); return 0; }

long ZNK8wxObject8IsKindOfEP11wxClassInfo(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxObject8IsKindOfEP11wxClassInfo"); return 0; }

long ZNK8wxRegion7IsEmptyEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxRegion7IsEmptyEv"); return 0; }

long ZNK8wxString3CmpEPKw(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxString3CmpEPKw"); return 0; }

long ZNK8wxString3CmpERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxString3CmpERKS_"); return 0; }

long ZNK8wxString3MidEmm(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxString3MidEmm"); return 0; }

long ZNK8wxString6ToLongEPli(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxString6ToLongEPli"); return 0; }

long ZNK8wxString6mb_strERK8wxMBConv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxString6mb_strERK8wxMBConv"); return 0; }

long ZNK8wxString9CmpNoCaseEPKw(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxString9CmpNoCaseEPKw"); return 0; }

long ZNK8wxString9CmpNoCaseERKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxString9CmpNoCaseERKS_"); return 0; }

long ZNK8wxWindow11MacCanFocusEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxWindow11MacCanFocusEv"); return 0; }

long ZNK8wxWindow12AcceptsFocusEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxWindow12AcceptsFocusEv"); return 0; }

long ZNK8wxWindow12GetCharWidthEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxWindow12GetCharWidthEv"); return 0; }

long ZNK8wxWindow12GetClassInfoEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxWindow12GetClassInfoEv"); return 0; }

long ZNK8wxWindow12GetScrollPosEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxWindow12GetScrollPosEi"); return 0; }

long ZNK8wxWindow13DoGetBestSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxWindow13DoGetBestSizeEv"); return 0; }

long ZNK8wxWindow13DoGetPositionEPiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxWindow13DoGetPositionEPiS0_"); return 0; }

long ZNK8wxWindow13GetCharHeightEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxWindow13GetCharHeightEv"); return 0; }

long ZNK8wxWindow13GetEventTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxWindow13GetEventTableEv"); return 0; }

long ZNK8wxWindow13GetTextExtentERK8wxStringPiS3_S3_S3_PK6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxWindow13GetTextExtentERK8wxStringPiS3_S3_S3_PK6wxFont"); return 0; }

long ZNK8wxWindow14GetScrollRangeEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxWindow14GetScrollRangeEi"); return 0; }

long ZNK8wxWindow14GetScrollThumbEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxWindow14GetScrollThumbEi"); return 0; }

long ZNK8wxWindow14GetTransparentEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxWindow14GetTransparentEv"); return 0; }

long ZNK8wxWindow15DoGetClientSizeEPiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxWindow15DoGetClientSizeEPiS0_"); return 0; }

long ZNK8wxWindow16DoClientToScreenEPiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxWindow16DoClientToScreenEPiS0_"); return 0; }

long ZNK8wxWindow16DoScreenToClientEPiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxWindow16DoScreenToClientEPiS0_"); return 0; }

long ZNK8wxWindow17GetEventHashTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxWindow17GetEventHashTableEv"); return 0; }

long ZNK8wxWindow19GetClientAreaOriginEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxWindow19GetClientAreaOriginEv"); return 0; }

long ZNK8wxWindow19MacGetTopBorderSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxWindow19MacGetTopBorderSizeEv"); return 0; }

long ZNK8wxWindow20MacGetLeftBorderSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxWindow20MacGetLeftBorderSizeEv"); return 0; }

long ZNK8wxWindow21MacGetRightBorderSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxWindow21MacGetRightBorderSizeEv"); return 0; }

long ZNK8wxWindow22MacGetBottomBorderSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxWindow22MacGetBottomBorderSizeEv"); return 0; }

long ZNK8wxWindow22MacIsChildOfClientAreaEPKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxWindow22MacIsChildOfClientAreaEPKS_"); return 0; }

long ZNK8wxWindow23DoGetSizeFromClientSizeERK6wxSize(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxWindow23DoGetSizeFromClientSizeERK6wxSize"); return 0; }

long ZNK8wxWindow23MacGetTopLevelWindowRefEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxWindow23MacGetTopLevelWindowRefEv"); return 0; }

long ZNK8wxWindow8GetLabelEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxWindow8GetLabelEv"); return 0; }

long ZNK8wxWindow8IsFrozenEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxWindow8IsFrozenEv"); return 0; }

long ZNK8wxWindow9DoGetSizeEPiS0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxWindow9DoGetSizeEPiS0_"); return 0; }

long ZNK8wxWindow9GetHandleEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK8wxWindow9GetHandleEv"); return 0; }

long ZNK9wxAppBase12GetTopWindowEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxAppBase12GetTopWindowEv"); return 0; }

long ZNK9wxAppBase14GetDisplayModeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxAppBase14GetDisplayModeEv"); return 0; }

long ZNK9wxAppBase18GetLayoutDirectionEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxAppBase18GetLayoutDirectionEv"); return 0; }

long ZNK9wxControl12GetClassInfoEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxControl12GetClassInfoEv"); return 0; }

long ZNK9wxDisplay11GetGeometryEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxDisplay11GetGeometryEv"); return 0; }

long ZNK9wxDisplay13GetClientAreaEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxDisplay13GetClientAreaEv"); return 0; }

long ZNK9wxDisplay14GetCurrentModeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxDisplay14GetCurrentModeEv"); return 0; }

long ZNK9wxDisplay7GetNameEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxDisplay7GetNameEv"); return 0; }

long ZNK9wxDisplay8GetModesERK11wxVideoMode(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxDisplay8GetModesERK11wxVideoMode"); return 0; }

long ZNK9wxDisplay9IsPrimaryEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxDisplay9IsPrimaryEv"); return 0; }

long ZNK9wxHtmlTag12GetAllParamsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxHtmlTag12GetAllParamsEv"); return 0; }

long ZNK9wxHtmlTag8GetParamERK8wxStringb(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxHtmlTag8GetParamERK8wxStringb"); return 0; }

long ZNK9wxHtmlTag8HasParamERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxHtmlTag8HasParamERK8wxString"); return 0; }

long ZNK9wxListBox10FindStringERK8wxStringb(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxListBox10FindStringERK8wxStringb"); return 0; }

long ZNK9wxListBox10IsSelectedEi(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxListBox10IsSelectedEi"); return 0; }

long ZNK9wxListBox12GetClassInfoEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxListBox12GetClassInfoEv"); return 0; }

long ZNK9wxListBox12GetSelectionEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxListBox12GetSelectionEv"); return 0; }

long ZNK9wxListBox13DoGetBestSizeEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxListBox13DoGetBestSizeEv"); return 0; }

long ZNK9wxListBox13DoListHitTestERK7wxPoint(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxListBox13DoListHitTestERK7wxPoint"); return 0; }

long ZNK9wxListBox13GetEventTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxListBox13GetEventTableEv"); return 0; }

long ZNK9wxListBox13GetSelectionsER10wxArrayInt(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxListBox13GetSelectionsER10wxArrayInt"); return 0; }

long ZNK9wxListBox17GetEventHashTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxListBox17GetEventHashTableEv"); return 0; }

long ZNK9wxListBox19DoGetItemClientDataEj(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxListBox19DoGetItemClientDataEj"); return 0; }

long ZNK9wxListBox21DoGetItemClientObjectEj(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxListBox21DoGetItemClientObjectEj"); return 0; }

long ZNK9wxListBox8GetCountEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxListBox8GetCountEv"); return 0; }

long ZNK9wxListBox9GetStringEj(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxListBox9GetStringEj"); return 0; }

long ZNK9wxMenuBar12GetMenuLabelEm(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxMenuBar12GetMenuLabelEm"); return 0; }

long ZNK9wxPaintDC12GetClassInfoEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxPaintDC12GetClassInfoEv"); return 0; }

long ZNK9wxPalette6GetRGBEiPhS0_S0_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxPalette6GetRGBEiPhS0_S0_"); return 0; }

long ZNK9wxPalette8GetPixelEhhh(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxPalette8GetPixelEhhh"); return 0; }

long ZNK9wxProcess12GetClassInfoEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxProcess12GetClassInfoEv"); return 0; }

long ZNK9wxProcess13IsInputOpenedEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxProcess13IsInputOpenedEv"); return 0; }

long ZNK9wxProcess16IsErrorAvailableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxProcess16IsErrorAvailableEv"); return 0; }

long ZNK9wxProcess16IsInputAvailableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxProcess16IsInputAvailableEv"); return 0; }

long ZNK9wxXmlNode10GetPropValERK8wxStringS2_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxXmlNode10GetPropValERK8wxStringS2_"); return 0; }

long ZNK9wxXmlNode14GetNodeContentEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxXmlNode14GetNodeContentEv"); return 0; }

long ZNK9wxXmlNode16IsWhitespaceOnlyEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxXmlNode16IsWhitespaceOnlyEv"); return 0; }

long ZNK9wxXmlNode7HasPropERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxXmlNode7HasPropERK8wxString"); return 0; }

long ZNK9wxXmlNode8GetDepthEPS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK9wxXmlNode8GetDepthEPS_"); return 0; }

long ZTI10wxListBase(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI10wxListBase"); return 0; }

long ZTI10wxListCtrl(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI10wxListCtrl"); return 0; }

long ZTI10wxNodeBase(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI10wxNodeBase"); return 0; }

long ZTI10wxPrintout(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI10wxPrintout"); return 0; }

long ZTI10wxTextCtrl(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI10wxTextCtrl"); return 0; }

long ZTI10wxTreeCtrl(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI10wxTreeCtrl"); return 0; }

long ZTI10wxVListBox(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI10wxVListBox"); return 0; }

long ZTI11wxComboCtrl(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI11wxComboCtrl"); return 0; }

long ZTI11wxGDIObject(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI11wxGDIObject"); return 0; }

long ZTI11wxValidator(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI11wxValidator"); return 0; }

long ZTI12wxComboPopup(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI12wxComboPopup"); return 0; }

long ZTI12wxDataObject(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI12wxDataObject"); return 0; }

long ZTI12wxDropSource(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI12wxDropSource"); return 0; }

long ZTI12wxDropTarget(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI12wxDropTarget"); return 0; }

long ZTI12wxEvtHandler(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI12wxEvtHandler"); return 0; }

long ZTI12wxHtmlFilter(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI12wxHtmlFilter"); return 0; }

long ZTI12wxHtmlWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI12wxHtmlWindow"); return 0; }

long ZTI12wxWizardPage(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI12wxWizardPage"); return 0; }

long ZTI13wxArtProvider(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI13wxArtProvider"); return 0; }

long ZTI13wxHtmlListBox(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI13wxHtmlListBox"); return 0; }

long ZTI13wxInputStream(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI13wxInputStream"); return 0; }

long ZTI13wxNotifyEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI13wxNotifyEvent"); return 0; }

long ZTI13wxTaskBarIcon(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI13wxTaskBarIcon"); return 0; }

long ZTI14wxCommandEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI14wxCommandEvent"); return 0; }

long ZTI14wxImageHandler(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI14wxImageHandler"); return 0; }

long ZTI14wxOutputStream(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI14wxOutputStream"); return 0; }

long ZTI14wxPreviewFrame(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI14wxPreviewFrame"); return 0; }

long ZTI14wxPrintPreview(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI14wxPrintPreview"); return 0; }

long ZTI15wxGridTableBase(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI15wxGridTableBase"); return 0; }

long ZTI15wxItemContainer(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI15wxItemContainer"); return 0; }

long ZTI16wxFileDropTarget(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI16wxFileDropTarget"); return 0; }

long ZTI16wxGridCellEditor(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI16wxGridCellEditor"); return 0; }

long ZTI16wxGridCellWorker(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI16wxGridCellWorker"); return 0; }

long ZTI16wxHtmlTagHandler(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI16wxHtmlTagHandler"); return 0; }

long ZTI16wxScrolledWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI16wxScrolledWindow"); return 0; }

long ZTI16wxTextDataObject(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI16wxTextDataObject"); return 0; }

long ZTI16wxTextDropTarget(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI16wxTextDropTarget"); return 0; }

long ZTI17wxVScrolledWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI17wxVScrolledWindow"); return 0; }

long ZTI18wxAuiDefaultTabArt(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI18wxAuiDefaultTabArt"); return 0; }

long ZTI18wxBitmapDataObject(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI18wxBitmapDataObject"); return 0; }

long ZTI19wxAuiDefaultDockArt(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI19wxAuiDefaultDockArt"); return 0; }

long ZTI19wxFileSystemHandler(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI19wxFileSystemHandler"); return 0; }

long ZTI19wxPreviewControlBar(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI19wxPreviewControlBar"); return 0; }

long ZTI20wxOwnerDrawnComboBox(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI20wxOwnerDrawnComboBox"); return 0; }

long ZTI20wxXmlResourceHandler(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI20wxXmlResourceHandler"); return 0; }

long ZTI5wxLog(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI5wxLog"); return 0; }

long ZTI7wxEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI7wxEvent"); return 0; }

long ZTI7wxPanel(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI7wxPanel"); return 0; }

long ZTI7wxTimer(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI7wxTimer"); return 0; }

long ZTI8wxButton(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI8wxButton"); return 0; }

long ZTI8wxObject(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI8wxObject"); return 0; }

long ZTI8wxThread(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI8wxThread"); return 0; }

long ZTI8wxWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI8wxWindow"); return 0; }

long ZTI9wxControl(long a, long b, long c_, long d, long e, long f) { shim_note("ZTI9wxControl"); return 0; }

long ZTV10wxConvAuto(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV10wxConvAuto"); return 0; }

long ZTV10wxKeyEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV10wxKeyEvent"); return 0; }

long ZTV10wxListItem(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV10wxListItem"); return 0; }

long ZTV10wxTextCtrl(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV10wxTextCtrl"); return 0; }

long ZTV11wxAnimation(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV11wxAnimation"); return 0; }

long ZTV11wxComboCtrl(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV11wxComboCtrl"); return 0; }

long ZTV11wxDateEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV11wxDateEvent"); return 0; }

long ZTV11wxGDIObject(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV11wxGDIObject"); return 0; }

long ZTV11wxListEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV11wxListEvent"); return 0; }

long ZTV11wxMediaCtrl(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV11wxMediaCtrl"); return 0; }

long ZTV11wxTimerBase(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV11wxTimerBase"); return 0; }

long ZTV11wxTreeEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV11wxTreeEvent"); return 0; }

long ZTV12wxColourBase(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV12wxColourBase"); return 0; }

long ZTV12wxMBConvUTF8(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV12wxMBConvUTF8"); return 0; }

long ZTV12wxMediaEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV12wxMediaEvent"); return 0; }

long ZTV12wxWebKitCtrl(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV12wxWebKitCtrl"); return 0; }

long ZTV13wxNotifyEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV13wxNotifyEvent"); return 0; }

long ZTV13wxRichTextBox(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV13wxRichTextBox"); return 0; }

long ZTV14wxCalendarCtrl(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV14wxCalendarCtrl"); return 0; }

long ZTV14wxCommandEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV14wxCommandEvent"); return 0; }

long ZTV14wxTextCtrlBase(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV14wxTextCtrlBase"); return 0; }

long ZTV15wxAnimationBase(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV15wxAnimationBase"); return 0; }

long ZTV15wxAnimationCtrl(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV15wxAnimationCtrl"); return 0; }

long ZTV15wxCalendarEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV15wxCalendarEvent"); return 0; }

long ZTV15wxComboCtrlBase(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV15wxComboCtrlBase"); return 0; }

long ZTV15wxItemContainer(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV15wxItemContainer"); return 0; }

long ZTV15wxRichTextEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV15wxRichTextEvent"); return 0; }

long ZTV15wxRichTextImage(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV15wxRichTextImage"); return 0; }

long ZTV16wxBitmapComboBox(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV16wxBitmapComboBox"); return 0; }

long ZTV16wxObjectListNode(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV16wxObjectListNode"); return 0; }

long ZTV16wxRichTextBuffer(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV16wxRichTextBuffer"); return 0; }

long ZTV16wxScrolledWindow(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV16wxScrolledWindow"); return 0; }

long ZTV18wxGenericComboCtrl(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV18wxGenericComboCtrl"); return 0; }

long ZTV19wxAnimationCtrlBase(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV19wxAnimationCtrlBase"); return 0; }

long ZTV20wxOwnerDrawnComboBox(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV20wxOwnerDrawnComboBox"); return 0; }

long ZTV20wxRichTextXMLHandler(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV20wxRichTextXMLHandler"); return 0; }

long ZTV20wxStringOutputStream(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV20wxStringOutputStream"); return 0; }

long ZTV21wxRichTextFileHandler(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV21wxRichTextFileHandler"); return 0; }

long ZTV21wxRichTextStdRenderer(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV21wxRichTextStdRenderer"); return 0; }

long ZTV26wxRichTextPlainTextHandler(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV26wxRichTextPlainTextHandler"); return 0; }

long ZTV28wxRichTextParagraphLayoutBox(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV28wxRichTextParagraphLayoutBox"); return 0; }

long ZTV6wxFont(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV6wxFont"); return 0; }

long ZTV6wxIcon(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV6wxIcon"); return 0; }

long ZTV6wxList(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV6wxList"); return 0; }

long ZTV7wxEvent(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV7wxEvent"); return 0; }

long ZTV7wxImage(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV7wxImage"); return 0; }

long ZTV7wxPanel(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV7wxPanel"); return 0; }

long ZTV7wxTimer(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV7wxTimer"); return 0; }

long ZTV8wxBitmap(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV8wxBitmap"); return 0; }

long ZTV8wxColour(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV8wxColour"); return 0; }

long ZTV8wxCursor(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV8wxCursor"); return 0; }

long ZTV8wxObject(long a, long b, long c_, long d, long e, long f) { shim_note("ZTV8wxObject"); return 0; }

long ZThn636_N20wxOwnerDrawnComboBox19DoSetItemClientDataEjPv(long a, long b, long c_, long d, long e, long f) { shim_note("ZThn636_N20wxOwnerDrawnComboBox19DoSetItemClientDataEjPv"); return 0; }

long ZThn636_N20wxOwnerDrawnComboBox21DoSetItemClientObjectEjP12wxClientData(long a, long b, long c_, long d, long e, long f) { shim_note("ZThn636_N20wxOwnerDrawnComboBox21DoSetItemClientObjectEjP12wxClientData"); return 0; }

long ZThn636_N20wxOwnerDrawnComboBox5ClearEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZThn636_N20wxOwnerDrawnComboBox5ClearEv"); return 0; }

long ZThn636_N20wxOwnerDrawnComboBox6DeleteEj(long a, long b, long c_, long d, long e, long f) { shim_note("ZThn636_N20wxOwnerDrawnComboBox6DeleteEj"); return 0; }

long ZThn636_N20wxOwnerDrawnComboBox8DoAppendERK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZThn636_N20wxOwnerDrawnComboBox8DoAppendERK8wxString"); return 0; }

long ZThn636_N20wxOwnerDrawnComboBox8DoInsertERK8wxStringj(long a, long b, long c_, long d, long e, long f) { shim_note("ZThn636_N20wxOwnerDrawnComboBox8DoInsertERK8wxStringj"); return 0; }

long ZThn636_N20wxOwnerDrawnComboBox9SetStringEjRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZThn636_N20wxOwnerDrawnComboBox9SetStringEjRK8wxString"); return 0; }

long ZThn636_NK20wxOwnerDrawnComboBox10FindStringERK8wxStringb(long a, long b, long c_, long d, long e, long f) { shim_note("ZThn636_NK20wxOwnerDrawnComboBox10FindStringERK8wxStringb"); return 0; }

long ZThn636_NK20wxOwnerDrawnComboBox12GetSelectionEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZThn636_NK20wxOwnerDrawnComboBox12GetSelectionEv"); return 0; }

long ZThn636_NK20wxOwnerDrawnComboBox19DoGetItemClientDataEj(long a, long b, long c_, long d, long e, long f) { shim_note("ZThn636_NK20wxOwnerDrawnComboBox19DoGetItemClientDataEj"); return 0; }

long ZThn636_NK20wxOwnerDrawnComboBox21DoGetItemClientObjectEj(long a, long b, long c_, long d, long e, long f) { shim_note("ZThn636_NK20wxOwnerDrawnComboBox21DoGetItemClientObjectEj"); return 0; }

long ZThn636_NK20wxOwnerDrawnComboBox8GetCountEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZThn636_NK20wxOwnerDrawnComboBox8GetCountEv"); return 0; }

long ZThn636_NK20wxOwnerDrawnComboBox9GetStringEj(long a, long b, long c_, long d, long e, long f) { shim_note("ZThn636_NK20wxOwnerDrawnComboBox9GetStringEj"); return 0; }

long ZmliRK10wxDateSpan(long a, long b, long c_, long d, long e, long f) { shim_note("ZmliRK10wxDateSpan"); return 0; }

long ZmliRK10wxTimeSpan(long a, long b, long c_, long d, long e, long f) { shim_note("ZmliRK10wxTimeSpan"); return 0; }

long ZplPKwRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZplPKwRK8wxString"); return 0; }

long ZplRK8wxStringPKw(long a, long b, long c_, long d, long e, long f) { shim_note("ZplRK8wxStringPKw"); return 0; }

long wxAnimationCtrlNameStr(long a, long b, long c_, long d, long e, long f) { shim_note("wxAnimationCtrlNameStr"); return 0; }

long wxBitmapComboBoxNameStr(long a, long b, long c_, long d, long e, long f) { shim_note("wxBitmapComboBoxNameStr"); return 0; }

long wxComboBoxNameStr(long a, long b, long c_, long d, long e, long f) { shim_note("wxComboBoxNameStr"); return 0; }

long wxConvLibc(long a, long b, long c_, long d, long e, long f) { shim_note("wxConvLibc"); return 0; }

long wxDefaultDateTime(long a, long b, long c_, long d, long e, long f) { shim_note("wxDefaultDateTime"); return 0; }

long wxDefaultPosition(long a, long b, long c_, long d, long e, long f) { shim_note("wxDefaultPosition"); return 0; }

long wxDefaultSize(long a, long b, long c_, long d, long e, long f) { shim_note("wxDefaultSize"); return 0; }

long wxDefaultValidator(long a, long b, long c_, long d, long e, long f) { shim_note("wxDefaultValidator"); return 0; }

long wxEVT_CALENDAR_DAY_CHANGED(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_CALENDAR_DAY_CHANGED"); return 0; }

long wxEVT_CALENDAR_DOUBLECLICKED(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_CALENDAR_DOUBLECLICKED"); return 0; }

long wxEVT_CALENDAR_MONTH_CHANGED(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_CALENDAR_MONTH_CHANGED"); return 0; }

long wxEVT_CALENDAR_SEL_CHANGED(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_CALENDAR_SEL_CHANGED"); return 0; }

long wxEVT_CALENDAR_WEEKDAY_CLICKED(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_CALENDAR_WEEKDAY_CLICKED"); return 0; }

long wxEVT_CALENDAR_YEAR_CHANGED(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_CALENDAR_YEAR_CHANGED"); return 0; }

long wxEVT_CHAR(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_CHAR"); return 0; }

long wxEVT_COMMAND_LIST_COL_BEGIN_DRAG(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_LIST_COL_BEGIN_DRAG"); return 0; }

long wxEVT_COMMAND_LIST_COL_CLICK(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_LIST_COL_CLICK"); return 0; }

long wxEVT_COMMAND_LIST_COL_DRAGGING(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_LIST_COL_DRAGGING"); return 0; }

long wxEVT_COMMAND_LIST_COL_END_DRAG(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_LIST_COL_END_DRAG"); return 0; }

long wxEVT_COMMAND_LIST_COL_RIGHT_CLICK(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_LIST_COL_RIGHT_CLICK"); return 0; }

long wxEVT_COMMAND_RICHTEXT_CHARACTER(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_RICHTEXT_CHARACTER"); return 0; }

long wxEVT_COMMAND_RICHTEXT_CONTENT_DELETED(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_RICHTEXT_CONTENT_DELETED"); return 0; }

long wxEVT_COMMAND_RICHTEXT_CONTENT_INSERTED(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_RICHTEXT_CONTENT_INSERTED"); return 0; }

long wxEVT_COMMAND_RICHTEXT_DELETE(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_RICHTEXT_DELETE"); return 0; }

long wxEVT_COMMAND_RICHTEXT_LEFT_CLICK(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_RICHTEXT_LEFT_CLICK"); return 0; }

long wxEVT_COMMAND_RICHTEXT_LEFT_DCLICK(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_RICHTEXT_LEFT_DCLICK"); return 0; }

long wxEVT_COMMAND_RICHTEXT_MIDDLE_CLICK(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_RICHTEXT_MIDDLE_CLICK"); return 0; }

long wxEVT_COMMAND_RICHTEXT_RETURN(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_RICHTEXT_RETURN"); return 0; }

long wxEVT_COMMAND_RICHTEXT_RIGHT_CLICK(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_RICHTEXT_RIGHT_CLICK"); return 0; }

long wxEVT_COMMAND_RICHTEXT_SELECTION_CHANGED(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_RICHTEXT_SELECTION_CHANGED"); return 0; }

long wxEVT_COMMAND_RICHTEXT_STYLESHEET_CHANGED(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_RICHTEXT_STYLESHEET_CHANGED"); return 0; }

long wxEVT_COMMAND_RICHTEXT_STYLESHEET_CHANGING(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_RICHTEXT_STYLESHEET_CHANGING"); return 0; }

long wxEVT_COMMAND_RICHTEXT_STYLESHEET_REPLACED(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_RICHTEXT_STYLESHEET_REPLACED"); return 0; }

long wxEVT_COMMAND_RICHTEXT_STYLESHEET_REPLACING(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_RICHTEXT_STYLESHEET_REPLACING"); return 0; }

long wxEVT_COMMAND_RICHTEXT_STYLE_CHANGED(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_RICHTEXT_STYLE_CHANGED"); return 0; }

long wxEVT_COMMAND_TREE_BEGIN_DRAG(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_TREE_BEGIN_DRAG"); return 0; }

long wxEVT_COMMAND_TREE_BEGIN_LABEL_EDIT(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_TREE_BEGIN_LABEL_EDIT"); return 0; }

long wxEVT_COMMAND_TREE_BEGIN_RDRAG(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_TREE_BEGIN_RDRAG"); return 0; }

long wxEVT_COMMAND_TREE_DELETE_ITEM(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_TREE_DELETE_ITEM"); return 0; }

long wxEVT_COMMAND_TREE_END_DRAG(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_TREE_END_DRAG"); return 0; }

long wxEVT_COMMAND_TREE_END_LABEL_EDIT(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_TREE_END_LABEL_EDIT"); return 0; }

long wxEVT_COMMAND_TREE_ITEM_ACTIVATED(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_TREE_ITEM_ACTIVATED"); return 0; }

long wxEVT_COMMAND_TREE_ITEM_COLLAPSED(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_TREE_ITEM_COLLAPSED"); return 0; }

long wxEVT_COMMAND_TREE_ITEM_COLLAPSING(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_TREE_ITEM_COLLAPSING"); return 0; }

long wxEVT_COMMAND_TREE_ITEM_EXPANDED(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_TREE_ITEM_EXPANDED"); return 0; }

long wxEVT_COMMAND_TREE_ITEM_EXPANDING(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_TREE_ITEM_EXPANDING"); return 0; }

long wxEVT_COMMAND_TREE_ITEM_RIGHT_CLICK(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_TREE_ITEM_RIGHT_CLICK"); return 0; }

long wxEVT_COMMAND_TREE_KEY_DOWN(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_TREE_KEY_DOWN"); return 0; }

long wxEVT_COMMAND_TREE_SEL_CHANGED(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_TREE_SEL_CHANGED"); return 0; }

long wxEVT_COMMAND_TREE_SEL_CHANGING(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_COMMAND_TREE_SEL_CHANGING"); return 0; }

long wxEVT_ENTER_WINDOW(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_ENTER_WINDOW"); return 0; }

long wxEVT_IDLE(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_IDLE"); return 0; }

long wxEVT_KEY_UP(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_KEY_UP"); return 0; }

long wxEVT_KILL_FOCUS(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_KILL_FOCUS"); return 0; }

long wxEVT_LEAVE_WINDOW(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_LEAVE_WINDOW"); return 0; }

long wxEVT_LEFT_DCLICK(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_LEFT_DCLICK"); return 0; }

long wxEVT_LEFT_DOWN(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_LEFT_DOWN"); return 0; }

long wxEVT_LEFT_UP(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_LEFT_UP"); return 0; }

long wxEVT_MEDIA_FINISHED(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_MEDIA_FINISHED"); return 0; }

long wxEVT_MEDIA_LOADED(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_MEDIA_LOADED"); return 0; }

long wxEVT_MEDIA_PAUSE(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_MEDIA_PAUSE"); return 0; }

long wxEVT_MEDIA_PLAY(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_MEDIA_PLAY"); return 0; }

long wxEVT_MEDIA_STATECHANGED(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_MEDIA_STATECHANGED"); return 0; }

long wxEVT_MEDIA_STOP(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_MEDIA_STOP"); return 0; }

long wxEVT_MIDDLE_DCLICK(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_MIDDLE_DCLICK"); return 0; }

long wxEVT_MIDDLE_DOWN(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_MIDDLE_DOWN"); return 0; }

long wxEVT_MIDDLE_UP(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_MIDDLE_UP"); return 0; }

long wxEVT_MOTION(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_MOTION"); return 0; }

long wxEVT_MOUSEWHEEL(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_MOUSEWHEEL"); return 0; }

long wxEVT_NULL(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_NULL"); return 0; }

long wxEVT_PAINT(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_PAINT"); return 0; }

long wxEVT_RIGHT_DCLICK(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_RIGHT_DCLICK"); return 0; }

long wxEVT_RIGHT_DOWN(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_RIGHT_DOWN"); return 0; }

long wxEVT_RIGHT_UP(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_RIGHT_UP"); return 0; }

long wxEVT_SCROLLWIN_BOTTOM(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_SCROLLWIN_BOTTOM"); return 0; }

long wxEVT_SCROLLWIN_LINEDOWN(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_SCROLLWIN_LINEDOWN"); return 0; }

long wxEVT_SCROLLWIN_LINEUP(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_SCROLLWIN_LINEUP"); return 0; }

long wxEVT_SCROLLWIN_PAGEDOWN(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_SCROLLWIN_PAGEDOWN"); return 0; }

long wxEVT_SCROLLWIN_PAGEUP(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_SCROLLWIN_PAGEUP"); return 0; }

long wxEVT_SCROLLWIN_THUMBRELEASE(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_SCROLLWIN_THUMBRELEASE"); return 0; }

long wxEVT_SCROLLWIN_THUMBTRACK(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_SCROLLWIN_THUMBTRACK"); return 0; }

long wxEVT_SCROLLWIN_TOP(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_SCROLLWIN_TOP"); return 0; }

long wxEVT_SET_FOCUS(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_SET_FOCUS"); return 0; }

long wxEVT_SIZE(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_SIZE"); return 0; }

long wxEVT_WEBKIT_BEFORE_LOAD(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_WEBKIT_BEFORE_LOAD"); return 0; }

long wxEVT_WEBKIT_NEW_WINDOW(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_WEBKIT_NEW_WINDOW"); return 0; }

long wxEVT_WEBKIT_STATE_CHANGED(long a, long b, long c_, long d, long e, long f) { shim_note("wxEVT_WEBKIT_STATE_CHANGED"); return 0; }

long wxEmptyString(long a, long b, long c_, long d, long e, long f) { shim_note("wxEmptyString"); return 0; }

long wxNullAnimation(long a, long b, long c_, long d, long e, long f) { shim_note("wxNullAnimation"); return 0; }

long wxNullBitmap(long a, long b, long c_, long d, long e, long f) { shim_note("wxNullBitmap"); return 0; }

long wxNullBrush(long a, long b, long c_, long d, long e, long f) { shim_note("wxNullBrush"); return 0; }

long wxNullColour(long a, long b, long c_, long d, long e, long f) { shim_note("wxNullColour"); return 0; }

long wxNullFont(long a, long b, long c_, long d, long e, long f) { shim_note("wxNullFont"); return 0; }

long wxNullPalette(long a, long b, long c_, long d, long e, long f) { shim_note("wxNullPalette"); return 0; }

long wxNullPen(long a, long b, long c_, long d, long e, long f) { shim_note("wxNullPen"); return 0; }

long wxPendingDelete(long a, long b, long c_, long d, long e, long f) { shim_note("wxPendingDelete"); return 0; }

long wxTextCtrlNameStr(long a, long b, long c_, long d, long e, long f) { shim_note("wxTextCtrlNameStr"); return 0; }
