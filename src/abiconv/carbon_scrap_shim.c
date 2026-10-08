/*
 * carbon_scrap_shim.c — the Carbon Scrap Manager on the Pasteboard.
 *
 * GetCurrentScrap / GetScrapFlavorSize / GetScrapFlavorData / PutScrapFlavor /
 * ClearCurrentScrap were removed from 64-bit HIToolbox; the Pasteboard Manager
 * that replaced them survives. The one flavor classic apps exchange is 'TEXT'
 * (MacRoman bytes), carried as public.utf8-plain-text on the clipboard. Call of
 * Duty 4 pastes into its console with exactly that flavor. The old stubs left
 * the ScrapRef and Size out-params undefined (stack garbage); a failing call
 * here always defines them.
 *
 * There is one scrap, the clipboard, so a ScrapRef is a fixed non-NULL token.
 */
#include <stdint.h>
#include <string.h>
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>

#define SCRAP_TOKEN        0x5343524Bu      /* 'SCRK' */
#define FLAVOR_TEXT        0x54455854u      /* 'TEXT' */
#define NO_TYPE_ERR        ((uint32_t)-102)  /* noTypeErr */
#define PARAM_ERR          ((uint32_t)-50)
#define PTR(i)             ((void *)(uintptr_t)a[(i)])

static PasteboardRef clipboard(void) {
    static PasteboardRef pb;
    if (!pb) PasteboardCreate(kPasteboardClipboard, &pb);
    if (pb) PasteboardSynchronize(pb);
    return pb;
}

/* The clipboard's plain text as MacRoman bytes (+1 CFData the caller releases),
 * or NULL if it holds no text. */
static CFDataRef clipboard_text(void) {
    PasteboardRef pb = clipboard();
    ItemCount n = 0;
    if (!pb || PasteboardGetItemCount(pb, &n) != noErr || n < 1) return NULL;
    PasteboardItemID item;
    CFDataRef utf8 = NULL;
    if (PasteboardGetItemIdentifier(pb, 1, &item) != noErr ||
        PasteboardCopyItemFlavorData(pb, item, CFSTR("public.utf8-plain-text"), &utf8) != noErr || !utf8)
        return NULL;
    CFStringRef s = CFStringCreateFromExternalRepresentation(NULL, utf8, kCFStringEncodingUTF8);
    CFRelease(utf8);
    if (!s) return NULL;
    CFDataRef roman = CFStringCreateExternalRepresentation(NULL, s, kCFStringEncodingMacRoman, '?');
    CFRelease(s);
    return roman;
}

/* OSStatus GetCurrentScrap(ScrapRef *scrap) */
uint32_t shim_GetCurrentScrap(uint32_t *a) {
    uint32_t *out = (uint32_t *)PTR(0);
    if (!out) return PARAM_ERR;
    *out = SCRAP_TOKEN;
    return noErr;
}

/* OSStatus GetScrapFlavorSize(ScrapRef, ScrapFlavorType, Size *byteCount) */
uint32_t shim_GetScrapFlavorSize(uint32_t *a) {
    int32_t *size = (int32_t *)PTR(2);
    if (size) *size = 0;
    if (a[1] != FLAVOR_TEXT) return NO_TYPE_ERR;
    CFDataRef d = clipboard_text();
    if (!d) return NO_TYPE_ERR;
    if (size) *size = (int32_t)CFDataGetLength(d);
    CFRelease(d);
    return noErr;
}

/* OSStatus GetScrapFlavorData(ScrapRef, ScrapFlavorType, Size *byteCount, void *dest)
 * *byteCount: in = room in dest, out = bytes copied. */
uint32_t shim_GetScrapFlavorData(uint32_t *a) {
    int32_t *size = (int32_t *)PTR(2);
    int32_t room = size ? *size : 0;
    if (size) *size = 0;
    if (a[1] != FLAVOR_TEXT) return NO_TYPE_ERR;
    CFDataRef d = clipboard_text();
    if (!d) return NO_TYPE_ERR;
    int32_t n = (int32_t)CFDataGetLength(d);
    if (n > room) n = room;
    if (n > 0 && PTR(3)) memcpy(PTR(3), CFDataGetBytePtr(d), (size_t)n);
    if (size) *size = n;
    CFRelease(d);
    return noErr;
}

/* OSStatus GetScrapFlavorCount(ScrapRef, UInt32 *infoCount) */
uint32_t shim_GetScrapFlavorCount(uint32_t *a) {
    uint32_t *count = (uint32_t *)PTR(1);
    CFDataRef d = clipboard_text();
    if (count) *count = d ? 1 : 0;
    if (d) CFRelease(d);
    return noErr;
}

/* OSStatus GetScrapFlavorFlags(ScrapRef, ScrapFlavorType, ScrapFlavorFlags *) */
uint32_t shim_GetScrapFlavorFlags(uint32_t *a) {
    uint32_t *flags = (uint32_t *)PTR(2);
    if (flags) *flags = 0;
    if (a[1] != FLAVOR_TEXT) return NO_TYPE_ERR;
    CFDataRef d = clipboard_text();
    if (!d) return NO_TYPE_ERR;
    CFRelease(d);
    return noErr;
}

/* OSStatus ClearCurrentScrap(void) */
uint32_t shim_ClearCurrentScrap(uint32_t *a) {
    (void)a;
    PasteboardRef pb = clipboard();
    return pb ? (uint32_t)PasteboardClear(pb) : PARAM_ERR;
}

/* OSStatus PutScrapFlavor(ScrapRef, ScrapFlavorType, ScrapFlavorFlags, Size, const void *) */
uint32_t shim_PutScrapFlavor(uint32_t *a) {
    if (a[1] != FLAVOR_TEXT) return NO_TYPE_ERR;
    PasteboardRef pb = clipboard();
    if (!pb) return PARAM_ERR;
    CFStringRef s = CFStringCreateWithBytes(NULL, (const UInt8 *)PTR(4), (CFIndex)(int32_t)a[3],
                                            kCFStringEncodingMacRoman, false);
    if (!s) return PARAM_ERR;
    CFDataRef utf8 = CFStringCreateExternalRepresentation(NULL, s, kCFStringEncodingUTF8, 0);
    CFRelease(s);
    if (!utf8) return PARAM_ERR;
    OSStatus st = PasteboardPutItemFlavor(pb, (PasteboardItemID)1, CFSTR("public.utf8-plain-text"), utf8, 0);
    CFRelease(utf8);
    return (uint32_t)st;
}
