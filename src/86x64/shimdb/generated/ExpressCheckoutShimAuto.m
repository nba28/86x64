#import <Cocoa/Cocoa.h>
#include <stdio.h>

static void shim_note(const char *s) { fprintf(stderr, "[shimauto:%s] %s\n", "ExpressCheckout", s); }

@interface XPCOSession : NSObject
@end
@implementation XPCOSession

@end

@interface XPCOStoreFront : NSObject
@end
@implementation XPCOStoreFront

@end

long XPCOAssortmentAssortmentsKey(long a, long b, long c_, long d, long e, long f) { shim_note("XPCOAssortmentAssortmentsKey called (auto-stub)"); return 0; }

long XPCOAssortmentPartNumberKey(long a, long b, long c_, long d, long e, long f) { shim_note("XPCOAssortmentPartNumberKey called (auto-stub)"); return 0; }

long XPCOInternalErrorDomain(long a, long b, long c_, long d, long e, long f) { shim_note("XPCOInternalErrorDomain called (auto-stub)"); return 0; }

long XPCOOrderProcessingDomain(long a, long b, long c_, long d, long e, long f) { shim_note("XPCOOrderProcessingDomain called (auto-stub)"); return 0; }

long XPCOStoreReachabilityDomain(long a, long b, long c_, long d, long e, long f) { shim_note("XPCOStoreReachabilityDomain called (auto-stub)"); return 0; }

long kXPCOAssetAlbumNameKey(long a, long b, long c_, long d, long e, long f) { shim_note("kXPCOAssetAlbumNameKey called (auto-stub)"); return 0; }

long kXPCOAssetFileChecksumsKey(long a, long b, long c_, long d, long e, long f) { shim_note("kXPCOAssetFileChecksumsKey called (auto-stub)"); return 0; }

long kXPCOAssetFilePathKey(long a, long b, long c_, long d, long e, long f) { shim_note("kXPCOAssetFilePathKey called (auto-stub)"); return 0; }

long kXPCOAssetFileSizeKey(long a, long b, long c_, long d, long e, long f) { shim_note("kXPCOAssetFileSizeKey called (auto-stub)"); return 0; }

long kXPCOAssetHeightKey(long a, long b, long c_, long d, long e, long f) { shim_note("kXPCOAssetHeightKey called (auto-stub)"); return 0; }

long kXPCOAssetMediaType(long a, long b, long c_, long d, long e, long f) { shim_note("kXPCOAssetMediaType called (auto-stub)"); return 0; }

long kXPCOAssetMediaTypeKey(long a, long b, long c_, long d, long e, long f) { shim_note("kXPCOAssetMediaTypeKey called (auto-stub)"); return 0; }

long kXPCOAssetPDFPageCountKey(long a, long b, long c_, long d, long e, long f) { shim_note("kXPCOAssetPDFPageCountKey called (auto-stub)"); return 0; }

long kXPCOAssetPageCountKey(long a, long b, long c_, long d, long e, long f) { shim_note("kXPCOAssetPageCountKey called (auto-stub)"); return 0; }

long kXPCOAssetPartNumberKey(long a, long b, long c_, long d, long e, long f) { shim_note("kXPCOAssetPartNumberKey called (auto-stub)"); return 0; }

long kXPCOAssetScaleImageKey(long a, long b, long c_, long d, long e, long f) { shim_note("kXPCOAssetScaleImageKey called (auto-stub)"); return 0; }

long kXPCOAssetTokenKey(long a, long b, long c_, long d, long e, long f) { shim_note("kXPCOAssetTokenKey called (auto-stub)"); return 0; }

long kXPCOAssetVendorShouldEnhanceImageKey(long a, long b, long c_, long d, long e, long f) { shim_note("kXPCOAssetVendorShouldEnhanceImageKey called (auto-stub)"); return 0; }

long kXPCOAssetWidthKey(long a, long b, long c_, long d, long e, long f) { shim_note("kXPCOAssetWidthKey called (auto-stub)"); return 0; }

long kXPCOConfiguration(long a, long b, long c_, long d, long e, long f) { shim_note("kXPCOConfiguration called (auto-stub)"); return 0; }

long kXPCOFailedFinalization(long a, long b, long c_, long d, long e, long f) { shim_note("kXPCOFailedFinalization called (auto-stub)"); return 0; }

long kXPCOIncludeMadeOnMacLogoKey(long a, long b, long c_, long d, long e, long f) { shim_note("kXPCOIncludeMadeOnMacLogoKey called (auto-stub)"); return 0; }

long kXPCOInternetAvailable(long a, long b, long c_, long d, long e, long f) { shim_note("kXPCOInternetAvailable called (auto-stub)"); return 0; }

long kXPCONoURLInConfiguration(long a, long b, long c_, long d, long e, long f) { shim_note("kXPCONoURLInConfiguration called (auto-stub)"); return 0; }

long kXPCOOscar(long a, long b, long c_, long d, long e, long f) { shim_note("kXPCOOscar called (auto-stub)"); return 0; }

long kXPCOThumbnailMediaType(long a, long b, long c_, long d, long e, long f) { shim_note("kXPCOThumbnailMediaType called (auto-stub)"); return 0; }
