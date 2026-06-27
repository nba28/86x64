// fontmgr_shim.c — graceful shims for the dead classic Font Manager API.
//
// The classic Font Manager (FMActivateFonts / FMGetFontFamilyFromName / GetAppFont, the
// FMFontFamily-id model) was removed from 64-bit/modern macOS; text is now Core Text / ATS.
// Civ IV uses these to register/look up its bundled fonts. We report success and hand back
// the application-font family id (1) so name->family lookups resolve to a usable generic
// font rather than kInvalidFontFamily (-1), which could make a caller bail out of text setup.
//
// MTSHIM convention: rdi -> &i386 args[0]; result in eax.

#include <stdint.h>

#define PTR(n) ((void *)(uintptr_t)args[(n)])
#define FM_NO_ERR  (0)
#define FM_APP_FONT (1)   // applFont — the classic application font family id

uint32_t shim_FMActivateFonts(uint32_t *args)   { (void)args; return FM_NO_ERR; }
uint32_t shim_FMDeactivateFonts(uint32_t *args) { (void)args; return FM_NO_ERR; }
uint32_t shim_GetAppFont(uint32_t *args)        { (void)args; return FM_APP_FONT; }

// FMGetFontFamilyFromName(ConstStr255Param) -> FMFontFamily: hand back the app font family
// (a valid id) so the caller can proceed with text rendering.
uint32_t shim_FMGetFontFamilyFromName(uint32_t *args) { (void)args; return FM_APP_FONT; }

// FMGetFontFamilyName(FMFontFamily, Str255 oName) -> OSStatus: return an empty Pascal string.
uint32_t shim_FMGetFontFamilyName(uint32_t *args) {
    uint8_t *oName = (uint8_t *)PTR(1);
    if (oName) oName[0] = 0;   // length-prefixed: 0 chars
    return FM_NO_ERR;
}
