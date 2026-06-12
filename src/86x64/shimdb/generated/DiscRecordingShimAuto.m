#import <Cocoa/Cocoa.h>
#include <stdio.h>

static void shim_note(const char *s) { fprintf(stderr, "[shimauto:%s] %s\n", "DiscRecording", s); }

@interface DRFile : NSObject
@end
@implementation DRFile

@end

@interface DRFolder : NSObject
@end
@implementation DRFolder

@end

long DRAllFilesystems(long a, long b, long c_, long d, long e, long f) { shim_note("DRAllFilesystems called (auto-stub)"); return 0; }

long DRDeviceAppearedNotification(long a, long b, long c_, long d, long e, long f) { shim_note("DRDeviceAppearedNotification called (auto-stub)"); return 0; }

long DRDeviceDisappearedNotification(long a, long b, long c_, long d, long e, long f) { shim_note("DRDeviceDisappearedNotification called (auto-stub)"); return 0; }

long DRDeviceIsBusyKey(long a, long b, long c_, long d, long e, long f) { shim_note("DRDeviceIsBusyKey called (auto-stub)"); return 0; }

long DRDeviceIsTrayOpenKey(long a, long b, long c_, long d, long e, long f) { shim_note("DRDeviceIsTrayOpenKey called (auto-stub)"); return 0; }

long DRDeviceMediaBlocksFreeKey(long a, long b, long c_, long d, long e, long f) { shim_note("DRDeviceMediaBlocksFreeKey called (auto-stub)"); return 0; }

long DRDeviceMediaInfoKey(long a, long b, long c_, long d, long e, long f) { shim_note("DRDeviceMediaInfoKey called (auto-stub)"); return 0; }

long DRDeviceMediaIsAppendableKey(long a, long b, long c_, long d, long e, long f) { shim_note("DRDeviceMediaIsAppendableKey called (auto-stub)"); return 0; }

long DRDeviceMediaIsBlankKey(long a, long b, long c_, long d, long e, long f) { shim_note("DRDeviceMediaIsBlankKey called (auto-stub)"); return 0; }

long DRDeviceMediaIsOverwritableKey(long a, long b, long c_, long d, long e, long f) { shim_note("DRDeviceMediaIsOverwritableKey called (auto-stub)"); return 0; }

long DRDeviceStatusChangedNotification(long a, long b, long c_, long d, long e, long f) { shim_note("DRDeviceStatusChangedNotification called (auto-stub)"); return 0; }

long DRDeviceSupportLevelAppleShipping(long a, long b, long c_, long d, long e, long f) { shim_note("DRDeviceSupportLevelAppleShipping called (auto-stub)"); return 0; }

long DRDeviceSupportLevelAppleSupported(long a, long b, long c_, long d, long e, long f) { shim_note("DRDeviceSupportLevelAppleSupported called (auto-stub)"); return 0; }

long DRDeviceSupportLevelKey(long a, long b, long c_, long d, long e, long f) { shim_note("DRDeviceSupportLevelKey called (auto-stub)"); return 0; }

long DRDeviceSupportLevelNone(long a, long b, long c_, long d, long e, long f) { shim_note("DRDeviceSupportLevelNone called (auto-stub)"); return 0; }

long DRDeviceSupportLevelUnsupported(long a, long b, long c_, long d, long e, long f) { shim_note("DRDeviceSupportLevelUnsupported called (auto-stub)"); return 0; }

long DRDeviceSupportLevelVendorSupported(long a, long b, long c_, long d, long e, long f) { shim_note("DRDeviceSupportLevelVendorSupported called (auto-stub)"); return 0; }

long DRInvisible(long a, long b, long c_, long d, long e, long f) { shim_note("DRInvisible called (auto-stub)"); return 0; }
