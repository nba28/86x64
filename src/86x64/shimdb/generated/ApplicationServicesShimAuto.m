#import <Cocoa/Cocoa.h>
#include <stdio.h>

static void shim_note(const char *s) { fprintf(stderr, "[shimauto:%s] %s\n", "ApplicationServices", s); }

long ATSFontActivateFromFileSpecification(long a, long b, long c_, long d, long e, long f) { shim_note("ATSFontActivateFromFileSpecification called (auto-stub)"); return 0; }

long ATSFontGetFileSpecification(long a, long b, long c_, long d, long e, long f) { shim_note("ATSFontGetFileSpecification called (auto-stub)"); return 0; }

long CGSTentBlur8(long a, long b, long c_, long d, long e, long f) { shim_note("CGSTentBlur8 called (auto-stub)"); return 0; }

long DCMShowDictionaryServiceWindow(long a, long b, long c_, long d, long e, long f) { shim_note("DCMShowDictionaryServiceWindow called (auto-stub)"); return 0; }

long FMCreateFontFamilyIterator(long a, long b, long c_, long d, long e, long f) { shim_note("FMCreateFontFamilyIterator called (auto-stub)"); return 0; }

long FMDisposeFontFamilyIterator(long a, long b, long c_, long d, long e, long f) { shim_note("FMDisposeFontFamilyIterator called (auto-stub)"); return 0; }

long FMGetFontFamilyFromName(long a, long b, long c_, long d, long e, long f) { shim_note("FMGetFontFamilyFromName called (auto-stub)"); return 0; }

long FMGetFontFamilyInstanceFromFont(long a, long b, long c_, long d, long e, long f) { shim_note("FMGetFontFamilyInstanceFromFont called (auto-stub)"); return 0; }

long FMGetFontFamilyName(long a, long b, long c_, long d, long e, long f) { shim_note("FMGetFontFamilyName called (auto-stub)"); return 0; }

long FMGetFontFamilyTextEncoding(long a, long b, long c_, long d, long e, long f) { shim_note("FMGetFontFamilyTextEncoding called (auto-stub)"); return 0; }

long FMGetFontFromFontFamilyInstance(long a, long b, long c_, long d, long e, long f) { shim_note("FMGetFontFromFontFamilyInstance called (auto-stub)"); return 0; }

long FMGetFontTable(long a, long b, long c_, long d, long e, long f) { shim_note("FMGetFontTable called (auto-stub)"); return 0; }

long FMGetNextFontFamily(long a, long b, long c_, long d, long e, long f) { shim_note("FMGetNextFontFamily called (auto-stub)"); return 0; }

long QDPictCreateWithProvider(long a, long b, long c_, long d, long e, long f) { shim_note("QDPictCreateWithProvider called (auto-stub)"); return 0; }

long QDPictDrawToCGContext(long a, long b, long c_, long d, long e, long f) { shim_note("QDPictDrawToCGContext called (auto-stub)"); return 0; }

long QDPictGetBounds(long a, long b, long c_, long d, long e, long f) { shim_note("QDPictGetBounds called (auto-stub)"); return 0; }

long QDPictRelease(long a, long b, long c_, long d, long e, long f) { shim_note("QDPictRelease called (auto-stub)"); return 0; }
