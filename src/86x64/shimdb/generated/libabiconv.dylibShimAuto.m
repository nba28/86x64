#import <Cocoa/Cocoa.h>
#include <stdio.h>

static void shim_note(const char *s) { fprintf(stderr, "[shimauto:%s] %s\n", "libabiconv.dylib", s); }

long AECoerceDesc(long a, long b, long c_, long d, long e, long f) { shim_note("AECoerceDesc called (auto-stub)"); return 0; }

long AECountItems(long a, long b, long c_, long d, long e, long f) { shim_note("AECountItems called (auto-stub)"); return 0; }

long AECreateAppleEvent(long a, long b, long c_, long d, long e, long f) { shim_note("AECreateAppleEvent called (auto-stub)"); return 0; }

long AECreateDesc(long a, long b, long c_, long d, long e, long f) { shim_note("AECreateDesc called (auto-stub)"); return 0; }

long AECreateList(long a, long b, long c_, long d, long e, long f) { shim_note("AECreateList called (auto-stub)"); return 0; }

long AEDisposeDesc(long a, long b, long c_, long d, long e, long f) { shim_note("AEDisposeDesc called (auto-stub)"); return 0; }

long AEFlattenDesc(long a, long b, long c_, long d, long e, long f) { shim_note("AEFlattenDesc called (auto-stub)"); return 0; }

long AEGetDescData(long a, long b, long c_, long d, long e, long f) { shim_note("AEGetDescData called (auto-stub)"); return 0; }

long AEGetDescDataSize(long a, long b, long c_, long d, long e, long f) { shim_note("AEGetDescDataSize called (auto-stub)"); return 0; }

long AEGetNthDesc(long a, long b, long c_, long d, long e, long f) { shim_note("AEGetNthDesc called (auto-stub)"); return 0; }

long AEPutDesc(long a, long b, long c_, long d, long e, long f) { shim_note("AEPutDesc called (auto-stub)"); return 0; }

long AEPutParamDesc(long a, long b, long c_, long d, long e, long f) { shim_note("AEPutParamDesc called (auto-stub)"); return 0; }

long AEPutParamPtr(long a, long b, long c_, long d, long e, long f) { shim_note("AEPutParamPtr called (auto-stub)"); return 0; }

long AESendMessage(long a, long b, long c_, long d, long e, long f) { shim_note("AESendMessage called (auto-stub)"); return 0; }

long AESizeOfFlattenedDesc(long a, long b, long c_, long d, long e, long f) { shim_note("AESizeOfFlattenedDesc called (auto-stub)"); return 0; }

long AddCollectionItem(long a, long b, long c_, long d, long e, long f) { shim_note("AddCollectionItem called (auto-stub)"); return 0; }

long AddResource(long a, long b, long c_, long d, long e, long f) { shim_note("AddResource called (auto-stub)"); return 0; }

long BatteryCount(long a, long b, long c_, long d, long e, long f) { shim_note("BatteryCount called (auto-stub)"); return 0; }

long CFAbsoluteTimeGetCurrent(long a, long b, long c_, long d, long e, long f) { shim_note("CFAbsoluteTimeGetCurrent called (auto-stub)"); return 0; }

long CFAbsoluteTimeGetDifferenceAsGregorianUnits(long a, long b, long c_, long d, long e, long f) { shim_note("CFAbsoluteTimeGetDifferenceAsGregorianUnits called (auto-stub)"); return 0; }

long CFAbsoluteTimeGetGregorianDate(long a, long b, long c_, long d, long e, long f) { shim_note("CFAbsoluteTimeGetGregorianDate called (auto-stub)"); return 0; }

long CFAllocatorAllocate(long a, long b, long c_, long d, long e, long f) { shim_note("CFAllocatorAllocate called (auto-stub)"); return 0; }

long CFAllocatorCreate(long a, long b, long c_, long d, long e, long f) { shim_note("CFAllocatorCreate called (auto-stub)"); return 0; }

long CFAllocatorDeallocate(long a, long b, long c_, long d, long e, long f) { shim_note("CFAllocatorDeallocate called (auto-stub)"); return 0; }

long CFAllocatorReallocate(long a, long b, long c_, long d, long e, long f) { shim_note("CFAllocatorReallocate called (auto-stub)"); return 0; }

long CFArrayAppendValue(long a, long b, long c_, long d, long e, long f) { shim_note("CFArrayAppendValue called (auto-stub)"); return 0; }

long CFArrayCreate(long a, long b, long c_, long d, long e, long f) { shim_note("CFArrayCreate called (auto-stub)"); return 0; }

long CFArrayCreateCopy(long a, long b, long c_, long d, long e, long f) { shim_note("CFArrayCreateCopy called (auto-stub)"); return 0; }

long CFArrayCreateMutable(long a, long b, long c_, long d, long e, long f) { shim_note("CFArrayCreateMutable called (auto-stub)"); return 0; }

long CFArrayCreateMutableCopy(long a, long b, long c_, long d, long e, long f) { shim_note("CFArrayCreateMutableCopy called (auto-stub)"); return 0; }

long CFArrayExchangeValuesAtIndices(long a, long b, long c_, long d, long e, long f) { shim_note("CFArrayExchangeValuesAtIndices called (auto-stub)"); return 0; }

long CFArrayGetCount(long a, long b, long c_, long d, long e, long f) { shim_note("CFArrayGetCount called (auto-stub)"); return 0; }

long CFArrayGetTypeID(long a, long b, long c_, long d, long e, long f) { shim_note("CFArrayGetTypeID called (auto-stub)"); return 0; }

long CFArrayGetValueAtIndex(long a, long b, long c_, long d, long e, long f) { shim_note("CFArrayGetValueAtIndex called (auto-stub)"); return 0; }

long CFArrayInsertValueAtIndex(long a, long b, long c_, long d, long e, long f) { shim_note("CFArrayInsertValueAtIndex called (auto-stub)"); return 0; }

long CFArrayRemoveAllValues(long a, long b, long c_, long d, long e, long f) { shim_note("CFArrayRemoveAllValues called (auto-stub)"); return 0; }

long CFArrayRemoveValueAtIndex(long a, long b, long c_, long d, long e, long f) { shim_note("CFArrayRemoveValueAtIndex called (auto-stub)"); return 0; }

long CFArraySetValueAtIndex(long a, long b, long c_, long d, long e, long f) { shim_note("CFArraySetValueAtIndex called (auto-stub)"); return 0; }

long CFBagGetCount(long a, long b, long c_, long d, long e, long f) { shim_note("CFBagGetCount called (auto-stub)"); return 0; }

long CFBagGetTypeID(long a, long b, long c_, long d, long e, long f) { shim_note("CFBagGetTypeID called (auto-stub)"); return 0; }

long CFBagGetValues(long a, long b, long c_, long d, long e, long f) { shim_note("CFBagGetValues called (auto-stub)"); return 0; }

long CFBooleanGetTypeID(long a, long b, long c_, long d, long e, long f) { shim_note("CFBooleanGetTypeID called (auto-stub)"); return 0; }

long CFBooleanGetValue(long a, long b, long c_, long d, long e, long f) { shim_note("CFBooleanGetValue called (auto-stub)"); return 0; }

long CFBundleCloseBundleResourceMap(long a, long b, long c_, long d, long e, long f) { shim_note("CFBundleCloseBundleResourceMap called (auto-stub)"); return 0; }

long CFBundleCopyBundleLocalizations(long a, long b, long c_, long d, long e, long f) { shim_note("CFBundleCopyBundleLocalizations called (auto-stub)"); return 0; }

long CFBundleCopyBundleURL(long a, long b, long c_, long d, long e, long f) { shim_note("CFBundleCopyBundleURL called (auto-stub)"); return 0; }

long CFBundleCopyExecutableURL(long a, long b, long c_, long d, long e, long f) { shim_note("CFBundleCopyExecutableURL called (auto-stub)"); return 0; }

long CFBundleCopyLocalizedString(long a, long b, long c_, long d, long e, long f) { shim_note("CFBundleCopyLocalizedString called (auto-stub)"); return 0; }

long CFBundleCopyPreferredLocalizationsFromArray(long a, long b, long c_, long d, long e, long f) { shim_note("CFBundleCopyPreferredLocalizationsFromArray called (auto-stub)"); return 0; }

long CFBundleCopyPrivateFrameworksURL(long a, long b, long c_, long d, long e, long f) { shim_note("CFBundleCopyPrivateFrameworksURL called (auto-stub)"); return 0; }

long CFBundleCopyResourceURL(long a, long b, long c_, long d, long e, long f) { shim_note("CFBundleCopyResourceURL called (auto-stub)"); return 0; }

long CFBundleCreate(long a, long b, long c_, long d, long e, long f) { shim_note("CFBundleCreate called (auto-stub)"); return 0; }

long CFBundleGetBundleWithIdentifier(long a, long b, long c_, long d, long e, long f) { shim_note("CFBundleGetBundleWithIdentifier called (auto-stub)"); return 0; }

long CFBundleGetDataPointerForName(long a, long b, long c_, long d, long e, long f) { shim_note("CFBundleGetDataPointerForName called (auto-stub)"); return 0; }

long CFBundleGetFunctionPointerForName(long a, long b, long c_, long d, long e, long f) { shim_note("CFBundleGetFunctionPointerForName called (auto-stub)"); return 0; }

long CFBundleGetMainBundle(long a, long b, long c_, long d, long e, long f) { shim_note("CFBundleGetMainBundle called (auto-stub)"); return 0; }

long CFBundleGetValueForInfoDictionaryKey(long a, long b, long c_, long d, long e, long f) { shim_note("CFBundleGetValueForInfoDictionaryKey called (auto-stub)"); return 0; }

long CFBundleGetVersionNumber(long a, long b, long c_, long d, long e, long f) { shim_note("CFBundleGetVersionNumber called (auto-stub)"); return 0; }

long CFBundleLoadExecutable(long a, long b, long c_, long d, long e, long f) { shim_note("CFBundleLoadExecutable called (auto-stub)"); return 0; }

long CFBundleOpenBundleResourceMap(long a, long b, long c_, long d, long e, long f) { shim_note("CFBundleOpenBundleResourceMap called (auto-stub)"); return 0; }

long CFCalendarCreateWithIdentifier(long a, long b, long c_, long d, long e, long f) { shim_note("CFCalendarCreateWithIdentifier called (auto-stub)"); return 0; }

long CFCopyDescription(long a, long b, long c_, long d, long e, long f) { shim_note("CFCopyDescription called (auto-stub)"); return 0; }

long CFCopyTypeIDDescription(long a, long b, long c_, long d, long e, long f) { shim_note("CFCopyTypeIDDescription called (auto-stub)"); return 0; }

long CFDataAppendBytes(long a, long b, long c_, long d, long e, long f) { shim_note("CFDataAppendBytes called (auto-stub)"); return 0; }

long CFDataCreate(long a, long b, long c_, long d, long e, long f) { shim_note("CFDataCreate called (auto-stub)"); return 0; }

long CFDataCreateCopy(long a, long b, long c_, long d, long e, long f) { shim_note("CFDataCreateCopy called (auto-stub)"); return 0; }

long CFDataCreateMutable(long a, long b, long c_, long d, long e, long f) { shim_note("CFDataCreateMutable called (auto-stub)"); return 0; }

long CFDataCreateMutableCopy(long a, long b, long c_, long d, long e, long f) { shim_note("CFDataCreateMutableCopy called (auto-stub)"); return 0; }

long CFDataCreateWithBytesNoCopy(long a, long b, long c_, long d, long e, long f) { shim_note("CFDataCreateWithBytesNoCopy called (auto-stub)"); return 0; }

long CFDataGetBytePtr(long a, long b, long c_, long d, long e, long f) { shim_note("CFDataGetBytePtr called (auto-stub)"); return 0; }

long CFDataGetLength(long a, long b, long c_, long d, long e, long f) { shim_note("CFDataGetLength called (auto-stub)"); return 0; }

long CFDataGetMutableBytePtr(long a, long b, long c_, long d, long e, long f) { shim_note("CFDataGetMutableBytePtr called (auto-stub)"); return 0; }

long CFDataGetTypeID(long a, long b, long c_, long d, long e, long f) { shim_note("CFDataGetTypeID called (auto-stub)"); return 0; }

long CFDataIncreaseLength(long a, long b, long c_, long d, long e, long f) { shim_note("CFDataIncreaseLength called (auto-stub)"); return 0; }

long CFDataSetLength(long a, long b, long c_, long d, long e, long f) { shim_note("CFDataSetLength called (auto-stub)"); return 0; }

long CFDateFormatterCreate(long a, long b, long c_, long d, long e, long f) { shim_note("CFDateFormatterCreate called (auto-stub)"); return 0; }

long CFDateFormatterCreateStringWithDate(long a, long b, long c_, long d, long e, long f) { shim_note("CFDateFormatterCreateStringWithDate called (auto-stub)"); return 0; }

long CFDateFormatterGetFormat(long a, long b, long c_, long d, long e, long f) { shim_note("CFDateFormatterGetFormat called (auto-stub)"); return 0; }

long CFDateFormatterSetFormat(long a, long b, long c_, long d, long e, long f) { shim_note("CFDateFormatterSetFormat called (auto-stub)"); return 0; }

long CFDictionaryAddValue(long a, long b, long c_, long d, long e, long f) { shim_note("CFDictionaryAddValue called (auto-stub)"); return 0; }

long CFDictionaryApplyFunction(long a, long b, long c_, long d, long e, long f) { shim_note("CFDictionaryApplyFunction called (auto-stub)"); return 0; }

long CFDictionaryContainsKey(long a, long b, long c_, long d, long e, long f) { shim_note("CFDictionaryContainsKey called (auto-stub)"); return 0; }

long CFDictionaryContainsValue(long a, long b, long c_, long d, long e, long f) { shim_note("CFDictionaryContainsValue called (auto-stub)"); return 0; }

long CFDictionaryCreate(long a, long b, long c_, long d, long e, long f) { shim_note("CFDictionaryCreate called (auto-stub)"); return 0; }

long CFDictionaryCreateCopy(long a, long b, long c_, long d, long e, long f) { shim_note("CFDictionaryCreateCopy called (auto-stub)"); return 0; }

long CFDictionaryCreateMutable(long a, long b, long c_, long d, long e, long f) { shim_note("CFDictionaryCreateMutable called (auto-stub)"); return 0; }

long CFDictionaryCreateMutableCopy(long a, long b, long c_, long d, long e, long f) { shim_note("CFDictionaryCreateMutableCopy called (auto-stub)"); return 0; }

long CFDictionaryGetCount(long a, long b, long c_, long d, long e, long f) { shim_note("CFDictionaryGetCount called (auto-stub)"); return 0; }

long CFDictionaryGetCountOfKey(long a, long b, long c_, long d, long e, long f) { shim_note("CFDictionaryGetCountOfKey called (auto-stub)"); return 0; }

long CFDictionaryGetCountOfValue(long a, long b, long c_, long d, long e, long f) { shim_note("CFDictionaryGetCountOfValue called (auto-stub)"); return 0; }

long CFDictionaryGetKeysAndValues(long a, long b, long c_, long d, long e, long f) { shim_note("CFDictionaryGetKeysAndValues called (auto-stub)"); return 0; }

long CFDictionaryGetTypeID(long a, long b, long c_, long d, long e, long f) { shim_note("CFDictionaryGetTypeID called (auto-stub)"); return 0; }

long CFDictionaryGetValue(long a, long b, long c_, long d, long e, long f) { shim_note("CFDictionaryGetValue called (auto-stub)"); return 0; }

long CFDictionaryGetValueIfPresent(long a, long b, long c_, long d, long e, long f) { shim_note("CFDictionaryGetValueIfPresent called (auto-stub)"); return 0; }

long CFDictionaryRemoveAllValues(long a, long b, long c_, long d, long e, long f) { shim_note("CFDictionaryRemoveAllValues called (auto-stub)"); return 0; }

long CFDictionaryRemoveValue(long a, long b, long c_, long d, long e, long f) { shim_note("CFDictionaryRemoveValue called (auto-stub)"); return 0; }

long CFDictionaryReplaceValue(long a, long b, long c_, long d, long e, long f) { shim_note("CFDictionaryReplaceValue called (auto-stub)"); return 0; }

long CFDictionarySetValue(long a, long b, long c_, long d, long e, long f) { shim_note("CFDictionarySetValue called (auto-stub)"); return 0; }

long CFEqual(long a, long b, long c_, long d, long e, long f) { shim_note("CFEqual called (auto-stub)"); return 0; }

long CFGetAllocator(long a, long b, long c_, long d, long e, long f) { shim_note("CFGetAllocator called (auto-stub)"); return 0; }

long CFGetRetainCount(long a, long b, long c_, long d, long e, long f) { shim_note("CFGetRetainCount called (auto-stub)"); return 0; }

long CFGetTypeID(long a, long b, long c_, long d, long e, long f) { shim_note("CFGetTypeID called (auto-stub)"); return 0; }

long CFHash(long a, long b, long c_, long d, long e, long f) { shim_note("CFHash called (auto-stub)"); return 0; }

long CFLocaleCopyCurrent(long a, long b, long c_, long d, long e, long f) { shim_note("CFLocaleCopyCurrent called (auto-stub)"); return 0; }

long CFLocaleGetIdentifier(long a, long b, long c_, long d, long e, long f) { shim_note("CFLocaleGetIdentifier called (auto-stub)"); return 0; }

long CFLocaleGetValue(long a, long b, long c_, long d, long e, long f) { shim_note("CFLocaleGetValue called (auto-stub)"); return 0; }

long CFMachPortCreateRunLoopSource(long a, long b, long c_, long d, long e, long f) { shim_note("CFMachPortCreateRunLoopSource called (auto-stub)"); return 0; }

long CFMachPortCreateWithPort(long a, long b, long c_, long d, long e, long f) { shim_note("CFMachPortCreateWithPort called (auto-stub)"); return 0; }

long CFMakeCollectable(long a, long b, long c_, long d, long e, long f) { shim_note("CFMakeCollectable called (auto-stub)"); return 0; }

long CFNotificationCenterGetDistributedCenter(long a, long b, long c_, long d, long e, long f) { shim_note("CFNotificationCenterGetDistributedCenter called (auto-stub)"); return 0; }

long CFNotificationCenterPostNotification(long a, long b, long c_, long d, long e, long f) { shim_note("CFNotificationCenterPostNotification called (auto-stub)"); return 0; }

long CFNullGetTypeID(long a, long b, long c_, long d, long e, long f) { shim_note("CFNullGetTypeID called (auto-stub)"); return 0; }

long CFNumberCompare(long a, long b, long c_, long d, long e, long f) { shim_note("CFNumberCompare called (auto-stub)"); return 0; }

long CFNumberCreate(long a, long b, long c_, long d, long e, long f) { shim_note("CFNumberCreate called (auto-stub)"); return 0; }

long CFNumberGetByteSize(long a, long b, long c_, long d, long e, long f) { shim_note("CFNumberGetByteSize called (auto-stub)"); return 0; }

long CFNumberGetType(long a, long b, long c_, long d, long e, long f) { shim_note("CFNumberGetType called (auto-stub)"); return 0; }

long CFNumberGetTypeID(long a, long b, long c_, long d, long e, long f) { shim_note("CFNumberGetTypeID called (auto-stub)"); return 0; }

long CFNumberGetValue(long a, long b, long c_, long d, long e, long f) { shim_note("CFNumberGetValue called (auto-stub)"); return 0; }

long CFNumberIsFloatType(long a, long b, long c_, long d, long e, long f) { shim_note("CFNumberIsFloatType called (auto-stub)"); return 0; }

long CFPlugInRegisterFactoryFunction(long a, long b, long c_, long d, long e, long f) { shim_note("CFPlugInRegisterFactoryFunction called (auto-stub)"); return 0; }

long CFPlugInRegisterPlugInType(long a, long b, long c_, long d, long e, long f) { shim_note("CFPlugInRegisterPlugInType called (auto-stub)"); return 0; }

long CFPlugInUnregisterFactory(long a, long b, long c_, long d, long e, long f) { shim_note("CFPlugInUnregisterFactory called (auto-stub)"); return 0; }

long CFPlugInUnregisterPlugInType(long a, long b, long c_, long d, long e, long f) { shim_note("CFPlugInUnregisterPlugInType called (auto-stub)"); return 0; }

long CFPreferencesAddSuitePreferencesToApp(long a, long b, long c_, long d, long e, long f) { shim_note("CFPreferencesAddSuitePreferencesToApp called (auto-stub)"); return 0; }

long CFPreferencesAppSynchronize(long a, long b, long c_, long d, long e, long f) { shim_note("CFPreferencesAppSynchronize called (auto-stub)"); return 0; }

long CFPreferencesCopyAppValue(long a, long b, long c_, long d, long e, long f) { shim_note("CFPreferencesCopyAppValue called (auto-stub)"); return 0; }

long CFPreferencesCopyApplicationList(long a, long b, long c_, long d, long e, long f) { shim_note("CFPreferencesCopyApplicationList called (auto-stub)"); return 0; }

long CFPreferencesCopyKeyList(long a, long b, long c_, long d, long e, long f) { shim_note("CFPreferencesCopyKeyList called (auto-stub)"); return 0; }

long CFPreferencesCopyMultiple(long a, long b, long c_, long d, long e, long f) { shim_note("CFPreferencesCopyMultiple called (auto-stub)"); return 0; }

long CFPreferencesCopyValue(long a, long b, long c_, long d, long e, long f) { shim_note("CFPreferencesCopyValue called (auto-stub)"); return 0; }

long CFPreferencesGetAppBooleanValue(long a, long b, long c_, long d, long e, long f) { shim_note("CFPreferencesGetAppBooleanValue called (auto-stub)"); return 0; }

long CFPreferencesGetAppIntegerValue(long a, long b, long c_, long d, long e, long f) { shim_note("CFPreferencesGetAppIntegerValue called (auto-stub)"); return 0; }

long CFPreferencesRemoveSuitePreferencesFromApp(long a, long b, long c_, long d, long e, long f) { shim_note("CFPreferencesRemoveSuitePreferencesFromApp called (auto-stub)"); return 0; }

long CFPreferencesSetAppValue(long a, long b, long c_, long d, long e, long f) { shim_note("CFPreferencesSetAppValue called (auto-stub)"); return 0; }

long CFPreferencesSetMultiple(long a, long b, long c_, long d, long e, long f) { shim_note("CFPreferencesSetMultiple called (auto-stub)"); return 0; }

long CFPreferencesSetValue(long a, long b, long c_, long d, long e, long f) { shim_note("CFPreferencesSetValue called (auto-stub)"); return 0; }

long CFPreferencesSynchronize(long a, long b, long c_, long d, long e, long f) { shim_note("CFPreferencesSynchronize called (auto-stub)"); return 0; }

long CFPropertyListCreateDeepCopy(long a, long b, long c_, long d, long e, long f) { shim_note("CFPropertyListCreateDeepCopy called (auto-stub)"); return 0; }

long CFPropertyListCreateFromXMLData(long a, long b, long c_, long d, long e, long f) { shim_note("CFPropertyListCreateFromXMLData called (auto-stub)"); return 0; }

long CFPropertyListCreateXMLData(long a, long b, long c_, long d, long e, long f) { shim_note("CFPropertyListCreateXMLData called (auto-stub)"); return 0; }

long CFReadStreamClose(long a, long b, long c_, long d, long e, long f) { shim_note("CFReadStreamClose called (auto-stub)"); return 0; }

long CFReadStreamCopyProperty(long a, long b, long c_, long d, long e, long f) { shim_note("CFReadStreamCopyProperty called (auto-stub)"); return 0; }

long CFReadStreamGetError(long a, long b, long c_, long d, long e, long f) { shim_note("CFReadStreamGetError called (auto-stub)"); return 0; }

long CFReadStreamGetStatus(long a, long b, long c_, long d, long e, long f) { shim_note("CFReadStreamGetStatus called (auto-stub)"); return 0; }

long CFReadStreamHasBytesAvailable(long a, long b, long c_, long d, long e, long f) { shim_note("CFReadStreamHasBytesAvailable called (auto-stub)"); return 0; }

long CFReadStreamOpen(long a, long b, long c_, long d, long e, long f) { shim_note("CFReadStreamOpen called (auto-stub)"); return 0; }

long CFReadStreamRead(long a, long b, long c_, long d, long e, long f) { shim_note("CFReadStreamRead called (auto-stub)"); return 0; }

long CFReadStreamScheduleWithRunLoop(long a, long b, long c_, long d, long e, long f) { shim_note("CFReadStreamScheduleWithRunLoop called (auto-stub)"); return 0; }

long CFReadStreamSetClient(long a, long b, long c_, long d, long e, long f) { shim_note("CFReadStreamSetClient called (auto-stub)"); return 0; }

long CFReadStreamSetProperty(long a, long b, long c_, long d, long e, long f) { shim_note("CFReadStreamSetProperty called (auto-stub)"); return 0; }

long CFReadStreamUnscheduleFromRunLoop(long a, long b, long c_, long d, long e, long f) { shim_note("CFReadStreamUnscheduleFromRunLoop called (auto-stub)"); return 0; }

long CFRelease(long a, long b, long c_, long d, long e, long f) { shim_note("CFRelease called (auto-stub)"); return 0; }

long CFRetain(long a, long b, long c_, long d, long e, long f) { shim_note("CFRetain called (auto-stub)"); return 0; }

long CFRunLoopAddObserver(long a, long b, long c_, long d, long e, long f) { shim_note("CFRunLoopAddObserver called (auto-stub)"); return 0; }

long CFRunLoopAddSource(long a, long b, long c_, long d, long e, long f) { shim_note("CFRunLoopAddSource called (auto-stub)"); return 0; }

long CFRunLoopAddTimer(long a, long b, long c_, long d, long e, long f) { shim_note("CFRunLoopAddTimer called (auto-stub)"); return 0; }

long CFRunLoopGetCurrent(long a, long b, long c_, long d, long e, long f) { shim_note("CFRunLoopGetCurrent called (auto-stub)"); return 0; }

long CFRunLoopGetMain(long a, long b, long c_, long d, long e, long f) { shim_note("CFRunLoopGetMain called (auto-stub)"); return 0; }

long CFRunLoopObserverCreate(long a, long b, long c_, long d, long e, long f) { shim_note("CFRunLoopObserverCreate called (auto-stub)"); return 0; }

long CFRunLoopObserverInvalidate(long a, long b, long c_, long d, long e, long f) { shim_note("CFRunLoopObserverInvalidate called (auto-stub)"); return 0; }

long CFRunLoopRemoveObserver(long a, long b, long c_, long d, long e, long f) { shim_note("CFRunLoopRemoveObserver called (auto-stub)"); return 0; }

long CFRunLoopRemoveSource(long a, long b, long c_, long d, long e, long f) { shim_note("CFRunLoopRemoveSource called (auto-stub)"); return 0; }

long CFRunLoopRun(long a, long b, long c_, long d, long e, long f) { shim_note("CFRunLoopRun called (auto-stub)"); return 0; }

long CFRunLoopRunInMode(long a, long b, long c_, long d, long e, long f) { shim_note("CFRunLoopRunInMode called (auto-stub)"); return 0; }

long CFRunLoopSourceCreate(long a, long b, long c_, long d, long e, long f) { shim_note("CFRunLoopSourceCreate called (auto-stub)"); return 0; }

long CFRunLoopSourceInvalidate(long a, long b, long c_, long d, long e, long f) { shim_note("CFRunLoopSourceInvalidate called (auto-stub)"); return 0; }

long CFRunLoopSourceSignal(long a, long b, long c_, long d, long e, long f) { shim_note("CFRunLoopSourceSignal called (auto-stub)"); return 0; }

long CFRunLoopStop(long a, long b, long c_, long d, long e, long f) { shim_note("CFRunLoopStop called (auto-stub)"); return 0; }

long CFRunLoopTimerCreate(long a, long b, long c_, long d, long e, long f) { shim_note("CFRunLoopTimerCreate called (auto-stub)"); return 0; }

long CFRunLoopTimerInvalidate(long a, long b, long c_, long d, long e, long f) { shim_note("CFRunLoopTimerInvalidate called (auto-stub)"); return 0; }

long CFRunLoopTimerSetNextFireDate(long a, long b, long c_, long d, long e, long f) { shim_note("CFRunLoopTimerSetNextFireDate called (auto-stub)"); return 0; }

long CFRunLoopWakeUp(long a, long b, long c_, long d, long e, long f) { shim_note("CFRunLoopWakeUp called (auto-stub)"); return 0; }

long CFSetAddValue(long a, long b, long c_, long d, long e, long f) { shim_note("CFSetAddValue called (auto-stub)"); return 0; }

long CFSetApplyFunction(long a, long b, long c_, long d, long e, long f) { shim_note("CFSetApplyFunction called (auto-stub)"); return 0; }

long CFSetContainsValue(long a, long b, long c_, long d, long e, long f) { shim_note("CFSetContainsValue called (auto-stub)"); return 0; }

long CFSetCreate(long a, long b, long c_, long d, long e, long f) { shim_note("CFSetCreate called (auto-stub)"); return 0; }

long CFSetCreateCopy(long a, long b, long c_, long d, long e, long f) { shim_note("CFSetCreateCopy called (auto-stub)"); return 0; }

long CFSetCreateMutable(long a, long b, long c_, long d, long e, long f) { shim_note("CFSetCreateMutable called (auto-stub)"); return 0; }

long CFSetCreateMutableCopy(long a, long b, long c_, long d, long e, long f) { shim_note("CFSetCreateMutableCopy called (auto-stub)"); return 0; }

long CFSetGetCount(long a, long b, long c_, long d, long e, long f) { shim_note("CFSetGetCount called (auto-stub)"); return 0; }

long CFSetGetCountOfValue(long a, long b, long c_, long d, long e, long f) { shim_note("CFSetGetCountOfValue called (auto-stub)"); return 0; }

long CFSetGetTypeID(long a, long b, long c_, long d, long e, long f) { shim_note("CFSetGetTypeID called (auto-stub)"); return 0; }

long CFSetGetValue(long a, long b, long c_, long d, long e, long f) { shim_note("CFSetGetValue called (auto-stub)"); return 0; }

long CFSetGetValueIfPresent(long a, long b, long c_, long d, long e, long f) { shim_note("CFSetGetValueIfPresent called (auto-stub)"); return 0; }

long CFSetGetValues(long a, long b, long c_, long d, long e, long f) { shim_note("CFSetGetValues called (auto-stub)"); return 0; }

long CFSetRemoveAllValues(long a, long b, long c_, long d, long e, long f) { shim_note("CFSetRemoveAllValues called (auto-stub)"); return 0; }

long CFSetRemoveValue(long a, long b, long c_, long d, long e, long f) { shim_note("CFSetRemoveValue called (auto-stub)"); return 0; }

long CFSetReplaceValue(long a, long b, long c_, long d, long e, long f) { shim_note("CFSetReplaceValue called (auto-stub)"); return 0; }

long CFSetSetValue(long a, long b, long c_, long d, long e, long f) { shim_note("CFSetSetValue called (auto-stub)"); return 0; }

long CFSocketConnectToAddress(long a, long b, long c_, long d, long e, long f) { shim_note("CFSocketConnectToAddress called (auto-stub)"); return 0; }

long CFSocketCreateRunLoopSource(long a, long b, long c_, long d, long e, long f) { shim_note("CFSocketCreateRunLoopSource called (auto-stub)"); return 0; }

long CFSocketCreateWithNative(long a, long b, long c_, long d, long e, long f) { shim_note("CFSocketCreateWithNative called (auto-stub)"); return 0; }

long CFSocketGetNative(long a, long b, long c_, long d, long e, long f) { shim_note("CFSocketGetNative called (auto-stub)"); return 0; }

long CFSocketInvalidate(long a, long b, long c_, long d, long e, long f) { shim_note("CFSocketInvalidate called (auto-stub)"); return 0; }

long CFSocketSetSocketFlags(long a, long b, long c_, long d, long e, long f) { shim_note("CFSocketSetSocketFlags called (auto-stub)"); return 0; }

long CFStreamCreatePairWithSocketToHost(long a, long b, long c_, long d, long e, long f) { shim_note("CFStreamCreatePairWithSocketToHost called (auto-stub)"); return 0; }

long CFStringAppend(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringAppend called (auto-stub)"); return 0; }

long CFStringAppendCharacters(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringAppendCharacters called (auto-stub)"); return 0; }

long CFStringAppendFormatAndArguments(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringAppendFormatAndArguments called (auto-stub)"); return 0; }

long CFStringCapitalize(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringCapitalize called (auto-stub)"); return 0; }

long CFStringCompare(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringCompare called (auto-stub)"); return 0; }

long CFStringConvertEncodingToNSStringEncoding(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringConvertEncodingToNSStringEncoding called (auto-stub)"); return 0; }

long CFStringConvertNSStringEncodingToEncoding(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringConvertNSStringEncodingToEncoding called (auto-stub)"); return 0; }

long CFStringCreateArrayBySeparatingStrings(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringCreateArrayBySeparatingStrings called (auto-stub)"); return 0; }

long CFStringCreateByCombiningStrings(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringCreateByCombiningStrings called (auto-stub)"); return 0; }

long CFStringCreateCopy(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringCreateCopy called (auto-stub)"); return 0; }

long CFStringCreateExternalRepresentation(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringCreateExternalRepresentation called (auto-stub)"); return 0; }

long CFStringCreateFromExternalRepresentation(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringCreateFromExternalRepresentation called (auto-stub)"); return 0; }

long CFStringCreateMutable(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringCreateMutable called (auto-stub)"); return 0; }

long CFStringCreateMutableCopy(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringCreateMutableCopy called (auto-stub)"); return 0; }

long CFStringCreateMutableWithExternalCharactersNoCopy(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringCreateMutableWithExternalCharactersNoCopy called (auto-stub)"); return 0; }

long CFStringCreateWithBytes(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringCreateWithBytes called (auto-stub)"); return 0; }

long CFStringCreateWithCString(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringCreateWithCString called (auto-stub)"); return 0; }

long CFStringCreateWithCStringNoCopy(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringCreateWithCStringNoCopy called (auto-stub)"); return 0; }

long CFStringCreateWithCharacters(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringCreateWithCharacters called (auto-stub)"); return 0; }

long CFStringCreateWithCharactersNoCopy(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringCreateWithCharactersNoCopy called (auto-stub)"); return 0; }

long CFStringCreateWithFormatAndArguments(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringCreateWithFormatAndArguments called (auto-stub)"); return 0; }

long CFStringCreateWithPascalString(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringCreateWithPascalString called (auto-stub)"); return 0; }

long CFStringCreateWithPascalStringNoCopy(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringCreateWithPascalStringNoCopy called (auto-stub)"); return 0; }

long CFStringFind(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringFind called (auto-stub)"); return 0; }

long CFStringGetCString(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringGetCString called (auto-stub)"); return 0; }

long CFStringGetCStringPtr(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringGetCStringPtr called (auto-stub)"); return 0; }

long CFStringGetCharacterAtIndex(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringGetCharacterAtIndex called (auto-stub)"); return 0; }

long CFStringGetCharactersPtr(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringGetCharactersPtr called (auto-stub)"); return 0; }

long CFStringGetDoubleValue(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringGetDoubleValue called (auto-stub)"); return 0; }

long CFStringGetFastestEncoding(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringGetFastestEncoding called (auto-stub)"); return 0; }

long CFStringGetIntValue(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringGetIntValue called (auto-stub)"); return 0; }

long CFStringGetLength(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringGetLength called (auto-stub)"); return 0; }

long CFStringGetMaximumSizeForEncoding(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringGetMaximumSizeForEncoding called (auto-stub)"); return 0; }

long CFStringGetPascalString(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringGetPascalString called (auto-stub)"); return 0; }

long CFStringGetPascalStringPtr(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringGetPascalStringPtr called (auto-stub)"); return 0; }

long CFStringGetSmallestEncoding(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringGetSmallestEncoding called (auto-stub)"); return 0; }

long CFStringGetSystemEncoding(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringGetSystemEncoding called (auto-stub)"); return 0; }

long CFStringGetTypeID(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringGetTypeID called (auto-stub)"); return 0; }

long CFStringHasPrefix(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringHasPrefix called (auto-stub)"); return 0; }

long CFStringHasSuffix(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringHasSuffix called (auto-stub)"); return 0; }

long CFStringLowercase(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringLowercase called (auto-stub)"); return 0; }

long CFStringNormalize(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringNormalize called (auto-stub)"); return 0; }

long CFStringTrimWhitespace(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringTrimWhitespace called (auto-stub)"); return 0; }

long CFStringUppercase(long a, long b, long c_, long d, long e, long f) { shim_note("CFStringUppercase called (auto-stub)"); return 0; }

long CFTimeZoneGetSecondsFromGMT(long a, long b, long c_, long d, long e, long f) { shim_note("CFTimeZoneGetSecondsFromGMT called (auto-stub)"); return 0; }

long CFURLCopyAbsoluteURL(long a, long b, long c_, long d, long e, long f) { shim_note("CFURLCopyAbsoluteURL called (auto-stub)"); return 0; }

long CFURLCopyFileSystemPath(long a, long b, long c_, long d, long e, long f) { shim_note("CFURLCopyFileSystemPath called (auto-stub)"); return 0; }

long CFURLCopyHostName(long a, long b, long c_, long d, long e, long f) { shim_note("CFURLCopyHostName called (auto-stub)"); return 0; }

long CFURLCopyLastPathComponent(long a, long b, long c_, long d, long e, long f) { shim_note("CFURLCopyLastPathComponent called (auto-stub)"); return 0; }

long CFURLCopyPassword(long a, long b, long c_, long d, long e, long f) { shim_note("CFURLCopyPassword called (auto-stub)"); return 0; }

long CFURLCopyPath(long a, long b, long c_, long d, long e, long f) { shim_note("CFURLCopyPath called (auto-stub)"); return 0; }

long CFURLCopyQueryString(long a, long b, long c_, long d, long e, long f) { shim_note("CFURLCopyQueryString called (auto-stub)"); return 0; }

long CFURLCopyResourceSpecifier(long a, long b, long c_, long d, long e, long f) { shim_note("CFURLCopyResourceSpecifier called (auto-stub)"); return 0; }

long CFURLCopyScheme(long a, long b, long c_, long d, long e, long f) { shim_note("CFURLCopyScheme called (auto-stub)"); return 0; }

long CFURLCopyUserName(long a, long b, long c_, long d, long e, long f) { shim_note("CFURLCopyUserName called (auto-stub)"); return 0; }

long CFURLCreateCopyAppendingPathComponent(long a, long b, long c_, long d, long e, long f) { shim_note("CFURLCreateCopyAppendingPathComponent called (auto-stub)"); return 0; }

long CFURLCreateCopyDeletingLastPathComponent(long a, long b, long c_, long d, long e, long f) { shim_note("CFURLCreateCopyDeletingLastPathComponent called (auto-stub)"); return 0; }

long CFURLCreateDataAndPropertiesFromResource(long a, long b, long c_, long d, long e, long f) { shim_note("CFURLCreateDataAndPropertiesFromResource called (auto-stub)"); return 0; }

long CFURLCreateFromFSRef(long a, long b, long c_, long d, long e, long f) { shim_note("CFURLCreateFromFSRef called (auto-stub)"); return 0; }

long CFURLCreateFromFileSystemRepresentation(long a, long b, long c_, long d, long e, long f) { shim_note("CFURLCreateFromFileSystemRepresentation called (auto-stub)"); return 0; }

long CFURLCreateStringByAddingPercentEscapes(long a, long b, long c_, long d, long e, long f) { shim_note("CFURLCreateStringByAddingPercentEscapes called (auto-stub)"); return 0; }

long CFURLCreateStringByReplacingPercentEscapes(long a, long b, long c_, long d, long e, long f) { shim_note("CFURLCreateStringByReplacingPercentEscapes called (auto-stub)"); return 0; }

long CFURLCreateStringByReplacingPercentEscapesUsingEncoding(long a, long b, long c_, long d, long e, long f) { shim_note("CFURLCreateStringByReplacingPercentEscapesUsingEncoding called (auto-stub)"); return 0; }

long CFURLCreateWithBytes(long a, long b, long c_, long d, long e, long f) { shim_note("CFURLCreateWithBytes called (auto-stub)"); return 0; }

long CFURLCreateWithFileSystemPath(long a, long b, long c_, long d, long e, long f) { shim_note("CFURLCreateWithFileSystemPath called (auto-stub)"); return 0; }

long CFURLCreateWithString(long a, long b, long c_, long d, long e, long f) { shim_note("CFURLCreateWithString called (auto-stub)"); return 0; }

long CFURLGetBytes(long a, long b, long c_, long d, long e, long f) { shim_note("CFURLGetBytes called (auto-stub)"); return 0; }

long CFURLGetFSRef(long a, long b, long c_, long d, long e, long f) { shim_note("CFURLGetFSRef called (auto-stub)"); return 0; }

long CFURLGetPortNumber(long a, long b, long c_, long d, long e, long f) { shim_note("CFURLGetPortNumber called (auto-stub)"); return 0; }

long CFURLGetString(long a, long b, long c_, long d, long e, long f) { shim_note("CFURLGetString called (auto-stub)"); return 0; }

long CFURLGetTypeID(long a, long b, long c_, long d, long e, long f) { shim_note("CFURLGetTypeID called (auto-stub)"); return 0; }

long CFURLHasDirectoryPath(long a, long b, long c_, long d, long e, long f) { shim_note("CFURLHasDirectoryPath called (auto-stub)"); return 0; }

long CFUUIDCreate(long a, long b, long c_, long d, long e, long f) { shim_note("CFUUIDCreate called (auto-stub)"); return 0; }

long CFUUIDCreateFromString(long a, long b, long c_, long d, long e, long f) { shim_note("CFUUIDCreateFromString called (auto-stub)"); return 0; }

long CFUUIDCreateString(long a, long b, long c_, long d, long e, long f) { shim_note("CFUUIDCreateString called (auto-stub)"); return 0; }

long CFUUIDGetConstantUUIDWithBytes(long a, long b, long c_, long d, long e, long f) { shim_note("CFUUIDGetConstantUUIDWithBytes called (auto-stub)"); return 0; }

long CFUUIDGetUUIDBytes(long a, long b, long c_, long d, long e, long f) { shim_note("CFUUIDGetUUIDBytes called (auto-stub)"); return 0; }

long CFWriteStreamCanAcceptBytes(long a, long b, long c_, long d, long e, long f) { shim_note("CFWriteStreamCanAcceptBytes called (auto-stub)"); return 0; }

long CFWriteStreamClose(long a, long b, long c_, long d, long e, long f) { shim_note("CFWriteStreamClose called (auto-stub)"); return 0; }

long CFWriteStreamCopyProperty(long a, long b, long c_, long d, long e, long f) { shim_note("CFWriteStreamCopyProperty called (auto-stub)"); return 0; }

long CFWriteStreamGetError(long a, long b, long c_, long d, long e, long f) { shim_note("CFWriteStreamGetError called (auto-stub)"); return 0; }

long CFWriteStreamGetStatus(long a, long b, long c_, long d, long e, long f) { shim_note("CFWriteStreamGetStatus called (auto-stub)"); return 0; }

long CFWriteStreamOpen(long a, long b, long c_, long d, long e, long f) { shim_note("CFWriteStreamOpen called (auto-stub)"); return 0; }

long CFWriteStreamScheduleWithRunLoop(long a, long b, long c_, long d, long e, long f) { shim_note("CFWriteStreamScheduleWithRunLoop called (auto-stub)"); return 0; }

long CFWriteStreamSetClient(long a, long b, long c_, long d, long e, long f) { shim_note("CFWriteStreamSetClient called (auto-stub)"); return 0; }

long CFWriteStreamSetProperty(long a, long b, long c_, long d, long e, long f) { shim_note("CFWriteStreamSetProperty called (auto-stub)"); return 0; }

long CFWriteStreamUnscheduleFromRunLoop(long a, long b, long c_, long d, long e, long f) { shim_note("CFWriteStreamUnscheduleFromRunLoop called (auto-stub)"); return 0; }

long CFWriteStreamWrite(long a, long b, long c_, long d, long e, long f) { shim_note("CFWriteStreamWrite called (auto-stub)"); return 0; }

long CFXMLCreateStringByEscapingEntities(long a, long b, long c_, long d, long e, long f) { shim_note("CFXMLCreateStringByEscapingEntities called (auto-stub)"); return 0; }

long CFXMLCreateStringByUnescapingEntities(long a, long b, long c_, long d, long e, long f) { shim_note("CFXMLCreateStringByUnescapingEntities called (auto-stub)"); return 0; }

long CSBackupSetItemExcluded(long a, long b, long c_, long d, long e, long f) { shim_note("CSBackupSetItemExcluded called (auto-stub)"); return 0; }

long CSCopyMachineName(long a, long b, long c_, long d, long e, long f) { shim_note("CSCopyMachineName called (auto-stub)"); return 0; }

long CSCopyUserName(long a, long b, long c_, long d, long e, long f) { shim_note("CSCopyUserName called (auto-stub)"); return 0; }

long ChangedResource(long a, long b, long c_, long d, long e, long f) { shim_note("ChangedResource called (auto-stub)"); return 0; }

long CloneCollection(long a, long b, long c_, long d, long e, long f) { shim_note("CloneCollection called (auto-stub)"); return 0; }

long CloseComponent(long a, long b, long c_, long d, long e, long f) { shim_note("CloseComponent called (auto-stub)"); return 0; }

long CloseResFile(long a, long b, long c_, long d, long e, long f) { shim_note("CloseResFile called (auto-stub)"); return 0; }

long CompareAndSwap(long a, long b, long c_, long d, long e, long f) { shim_note("CompareAndSwap called (auto-stub)"); return 0; }

long CoreEndianInstallFlipper(long a, long b, long c_, long d, long e, long f) { shim_note("CoreEndianInstallFlipper called (auto-stub)"); return 0; }

long Count1Resources(long a, long b, long c_, long d, long e, long f) { shim_note("Count1Resources called (auto-stub)"); return 0; }

long CountCollectionOwners(long a, long b, long c_, long d, long e, long f) { shim_note("CountCollectionOwners called (auto-stub)"); return 0; }

long CreateTextEncoding(long a, long b, long c_, long d, long e, long f) { shim_note("CreateTextEncoding called (auto-stub)"); return 0; }

long CreateTextToUnicodeInfoByEncoding(long a, long b, long c_, long d, long e, long f) { shim_note("CreateTextToUnicodeInfoByEncoding called (auto-stub)"); return 0; }

long CreateUnicodeToTextInfoByEncoding(long a, long b, long c_, long d, long e, long f) { shim_note("CreateUnicodeToTextInfoByEncoding called (auto-stub)"); return 0; }

long CurResFile(long a, long b, long c_, long d, long e, long f) { shim_note("CurResFile called (auto-stub)"); return 0; }

long DebugStr(long a, long b, long c_, long d, long e, long f) { shim_note("DebugStr called (auto-stub)"); return 0; }

long Debugger(long a, long b, long c_, long d, long e, long f) { shim_note("Debugger called (auto-stub)"); return 0; }

long DecrementAtomic(long a, long b, long c_, long d, long e, long f) { shim_note("DecrementAtomic called (auto-stub)"); return 0; }

long Delay(long a, long b, long c_, long d, long e, long f) { shim_note("Delay called (auto-stub)"); return 0; }

long Dequeue(long a, long b, long c_, long d, long e, long f) { shim_note("Dequeue called (auto-stub)"); return 0; }

long DetachResource(long a, long b, long c_, long d, long e, long f) { shim_note("DetachResource called (auto-stub)"); return 0; }

long DisposeCollection(long a, long b, long c_, long d, long e, long f) { shim_note("DisposeCollection called (auto-stub)"); return 0; }

long DisposeHandle(long a, long b, long c_, long d, long e, long f) { shim_note("DisposeHandle called (auto-stub)"); return 0; }

long DisposePtr(long a, long b, long c_, long d, long e, long f) { shim_note("DisposePtr called (auto-stub)"); return 0; }

long DisposeTextToUnicodeInfo(long a, long b, long c_, long d, long e, long f) { shim_note("DisposeTextToUnicodeInfo called (auto-stub)"); return 0; }

long DisposeUnicodeToTextInfo(long a, long b, long c_, long d, long e, long f) { shim_note("DisposeUnicodeToTextInfo called (auto-stub)"); return 0; }

long Enqueue(long a, long b, long c_, long d, long e, long f) { shim_note("Enqueue called (auto-stub)"); return 0; }

long FNNotify(long a, long b, long c_, long d, long e, long f) { shim_note("FNNotify called (auto-stub)"); return 0; }

long FNNotifyByPath(long a, long b, long c_, long d, long e, long f) { shim_note("FNNotifyByPath called (auto-stub)"); return 0; }

long FSAllocateFork(long a, long b, long c_, long d, long e, long f) { shim_note("FSAllocateFork called (auto-stub)"); return 0; }

long FSCloseFork(long a, long b, long c_, long d, long e, long f) { shim_note("FSCloseFork called (auto-stub)"); return 0; }

long FSCloseIterator(long a, long b, long c_, long d, long e, long f) { shim_note("FSCloseIterator called (auto-stub)"); return 0; }

long FSCompareFSRefs(long a, long b, long c_, long d, long e, long f) { shim_note("FSCompareFSRefs called (auto-stub)"); return 0; }

long FSCreateDirectoryUnicode(long a, long b, long c_, long d, long e, long f) { shim_note("FSCreateDirectoryUnicode called (auto-stub)"); return 0; }

long FSCreateFileUnicode(long a, long b, long c_, long d, long e, long f) { shim_note("FSCreateFileUnicode called (auto-stub)"); return 0; }

long FSCreateResFile(long a, long b, long c_, long d, long e, long f) { shim_note("FSCreateResFile called (auto-stub)"); return 0; }

long FSDeleteObject(long a, long b, long c_, long d, long e, long f) { shim_note("FSDeleteObject called (auto-stub)"); return 0; }

long FSExchangeObjects(long a, long b, long c_, long d, long e, long f) { shim_note("FSExchangeObjects called (auto-stub)"); return 0; }

long FSFindFolder(long a, long b, long c_, long d, long e, long f) { shim_note("FSFindFolder called (auto-stub)"); return 0; }

long FSFlushFork(long a, long b, long c_, long d, long e, long f) { shim_note("FSFlushFork called (auto-stub)"); return 0; }

long FSGetForkPosition(long a, long b, long c_, long d, long e, long f) { shim_note("FSGetForkPosition called (auto-stub)"); return 0; }

long FSGetForkSize(long a, long b, long c_, long d, long e, long f) { shim_note("FSGetForkSize called (auto-stub)"); return 0; }

long FSIsAliasFile(long a, long b, long c_, long d, long e, long f) { shim_note("FSIsAliasFile called (auto-stub)"); return 0; }

long FSMakeFSRefUnicode(long a, long b, long c_, long d, long e, long f) { shim_note("FSMakeFSRefUnicode called (auto-stub)"); return 0; }

long FSMoveObject(long a, long b, long c_, long d, long e, long f) { shim_note("FSMoveObject called (auto-stub)"); return 0; }

long FSNewAlias(long a, long b, long c_, long d, long e, long f) { shim_note("FSNewAlias called (auto-stub)"); return 0; }

long FSOpenFork(long a, long b, long c_, long d, long e, long f) { shim_note("FSOpenFork called (auto-stub)"); return 0; }

long FSOpenIterator(long a, long b, long c_, long d, long e, long f) { shim_note("FSOpenIterator called (auto-stub)"); return 0; }

long FSOpenResFile(long a, long b, long c_, long d, long e, long f) { shim_note("FSOpenResFile called (auto-stub)"); return 0; }

long FSPathMakeRef(long a, long b, long c_, long d, long e, long f) { shim_note("FSPathMakeRef called (auto-stub)"); return 0; }

long FSReadFork(long a, long b, long c_, long d, long e, long f) { shim_note("FSReadFork called (auto-stub)"); return 0; }

long FSRenameUnicode(long a, long b, long c_, long d, long e, long f) { shim_note("FSRenameUnicode called (auto-stub)"); return 0; }

long FSResolveAliasFile(long a, long b, long c_, long d, long e, long f) { shim_note("FSResolveAliasFile called (auto-stub)"); return 0; }

long FSResolveAliasFileWithMountFlags(long a, long b, long c_, long d, long e, long f) { shim_note("FSResolveAliasFileWithMountFlags called (auto-stub)"); return 0; }

long FSResolveAliasWithMountFlags(long a, long b, long c_, long d, long e, long f) { shim_note("FSResolveAliasWithMountFlags called (auto-stub)"); return 0; }

long FSSetCatalogInfo(long a, long b, long c_, long d, long e, long f) { shim_note("FSSetCatalogInfo called (auto-stub)"); return 0; }

long FSSetForkPosition(long a, long b, long c_, long d, long e, long f) { shim_note("FSSetForkPosition called (auto-stub)"); return 0; }

long FSSetForkSize(long a, long b, long c_, long d, long e, long f) { shim_note("FSSetForkSize called (auto-stub)"); return 0; }

long FSWriteFork(long a, long b, long c_, long d, long e, long f) { shim_note("FSWriteFork called (auto-stub)"); return 0; }

long FindFolder(long a, long b, long c_, long d, long e, long f) { shim_note("FindFolder called (auto-stub)"); return 0; }

long FindNextComponent(long a, long b, long c_, long d, long e, long f) { shim_note("FindNextComponent called (auto-stub)"); return 0; }

long FixRatio(long a, long b, long c_, long d, long e, long f) { shim_note("FixRatio called (auto-stub)"); return 0; }

long Gestalt(long a, long b, long c_, long d, long e, long f) { shim_note("Gestalt called (auto-stub)"); return 0; }

long Get1IndResource(long a, long b, long c_, long d, long e, long f) { shim_note("Get1IndResource called (auto-stub)"); return 0; }

long Get1NamedResource(long a, long b, long c_, long d, long e, long f) { shim_note("Get1NamedResource called (auto-stub)"); return 0; }

long Get1Resource(long a, long b, long c_, long d, long e, long f) { shim_note("Get1Resource called (auto-stub)"); return 0; }

long GetComponentInfo(long a, long b, long c_, long d, long e, long f) { shim_note("GetComponentInfo called (auto-stub)"); return 0; }

long GetCurrentProcess(long a, long b, long c_, long d, long e, long f) { shim_note("GetCurrentProcess called (auto-stub)"); return 0; }

long GetCurrentThread(long a, long b, long c_, long d, long e, long f) { shim_note("GetCurrentThread called (auto-stub)"); return 0; }

long GetFrontProcess(long a, long b, long c_, long d, long e, long f) { shim_note("GetFrontProcess called (auto-stub)"); return 0; }

long GetHandleSize(long a, long b, long c_, long d, long e, long f) { shim_note("GetHandleSize called (auto-stub)"); return 0; }

long GetIconFamilyData(long a, long b, long c_, long d, long e, long f) { shim_note("GetIconFamilyData called (auto-stub)"); return 0; }

long GetIconRef(long a, long b, long c_, long d, long e, long f) { shim_note("GetIconRef called (auto-stub)"); return 0; }

long GetNextProcess(long a, long b, long c_, long d, long e, long f) { shim_note("GetNextProcess called (auto-stub)"); return 0; }

long GetNextResourceFile(long a, long b, long c_, long d, long e, long f) { shim_note("GetNextResourceFile called (auto-stub)"); return 0; }

long GetProcessInformation(long a, long b, long c_, long d, long e, long f) { shim_note("GetProcessInformation called (auto-stub)"); return 0; }

long GetResAttrs(long a, long b, long c_, long d, long e, long f) { shim_note("GetResAttrs called (auto-stub)"); return 0; }

long GetResInfo(long a, long b, long c_, long d, long e, long f) { shim_note("GetResInfo called (auto-stub)"); return 0; }

long GetResource(long a, long b, long c_, long d, long e, long f) { shim_note("GetResource called (auto-stub)"); return 0; }

long GetScriptManagerVariable(long a, long b, long c_, long d, long e, long f) { shim_note("GetScriptManagerVariable called (auto-stub)"); return 0; }

long GetThreadCurrentTaskRef(long a, long b, long c_, long d, long e, long f) { shim_note("GetThreadCurrentTaskRef called (auto-stub)"); return 0; }

long GetTopResourceFile(long a, long b, long c_, long d, long e, long f) { shim_note("GetTopResourceFile called (auto-stub)"); return 0; }

long HandToHand(long a, long b, long c_, long d, long e, long f) { shim_note("HandToHand called (auto-stub)"); return 0; }

long ICAddMapEntry(long a, long b, long c_, long d, long e, long f) { shim_note("ICAddMapEntry called (auto-stub)"); return 0; }

long ICBegin(long a, long b, long c_, long d, long e, long f) { shim_note("ICBegin called (auto-stub)"); return 0; }

long ICCountMapEntries(long a, long b, long c_, long d, long e, long f) { shim_note("ICCountMapEntries called (auto-stub)"); return 0; }

long ICDeleteMapEntry(long a, long b, long c_, long d, long e, long f) { shim_note("ICDeleteMapEntry called (auto-stub)"); return 0; }

long ICEnd(long a, long b, long c_, long d, long e, long f) { shim_note("ICEnd called (auto-stub)"); return 0; }

long ICFindPrefHandle(long a, long b, long c_, long d, long e, long f) { shim_note("ICFindPrefHandle called (auto-stub)"); return 0; }

long ICGetMapEntry(long a, long b, long c_, long d, long e, long f) { shim_note("ICGetMapEntry called (auto-stub)"); return 0; }

long ICGetPref(long a, long b, long c_, long d, long e, long f) { shim_note("ICGetPref called (auto-stub)"); return 0; }

long ICLaunchURL(long a, long b, long c_, long d, long e, long f) { shim_note("ICLaunchURL called (auto-stub)"); return 0; }

long ICSetMapEntry(long a, long b, long c_, long d, long e, long f) { shim_note("ICSetMapEntry called (auto-stub)"); return 0; }

long ICSetPrefHandle(long a, long b, long c_, long d, long e, long f) { shim_note("ICSetPrefHandle called (auto-stub)"); return 0; }

long ICStart(long a, long b, long c_, long d, long e, long f) { shim_note("ICStart called (auto-stub)"); return 0; }

long ICStop(long a, long b, long c_, long d, long e, long f) { shim_note("ICStop called (auto-stub)"); return 0; }

long IOAllowPowerChange(long a, long b, long c_, long d, long e, long f) { shim_note("IOAllowPowerChange called (auto-stub)"); return 0; }

long IODeregisterForSystemPower(long a, long b, long c_, long d, long e, long f) { shim_note("IODeregisterForSystemPower called (auto-stub)"); return 0; }

long IONotificationPortDestroy(long a, long b, long c_, long d, long e, long f) { shim_note("IONotificationPortDestroy called (auto-stub)"); return 0; }

long IONotificationPortGetRunLoopSource(long a, long b, long c_, long d, long e, long f) { shim_note("IONotificationPortGetRunLoopSource called (auto-stub)"); return 0; }

long IORegisterForSystemPower(long a, long b, long c_, long d, long e, long f) { shim_note("IORegisterForSystemPower called (auto-stub)"); return 0; }

long IOServiceClose(long a, long b, long c_, long d, long e, long f) { shim_note("IOServiceClose called (auto-stub)"); return 0; }

long IncrementAtomic(long a, long b, long c_, long d, long e, long f) { shim_note("IncrementAtomic called (auto-stub)"); return 0; }

long InsertResourceFile(long a, long b, long c_, long d, long e, long f) { shim_note("InsertResourceFile called (auto-stub)"); return 0; }

long KCDeleteItem(long a, long b, long c_, long d, long e, long f) { shim_note("KCDeleteItem called (auto-stub)"); return 0; }

long KCFindFirstItem(long a, long b, long c_, long d, long e, long f) { shim_note("KCFindFirstItem called (auto-stub)"); return 0; }

long KCFindNextItem(long a, long b, long c_, long d, long e, long f) { shim_note("KCFindNextItem called (auto-stub)"); return 0; }

long KCGetAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("KCGetAttribute called (auto-stub)"); return 0; }

long KCGetData(long a, long b, long c_, long d, long e, long f) { shim_note("KCGetData called (auto-stub)"); return 0; }

long KCNewItem(long a, long b, long c_, long d, long e, long f) { shim_note("KCNewItem called (auto-stub)"); return 0; }

long KCReleaseItem(long a, long b, long c_, long d, long e, long f) { shim_note("KCReleaseItem called (auto-stub)"); return 0; }

long KCReleaseSearch(long a, long b, long c_, long d, long e, long f) { shim_note("KCReleaseSearch called (auto-stub)"); return 0; }

long KCSetAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("KCSetAttribute called (auto-stub)"); return 0; }

long LMGetResLoad(long a, long b, long c_, long d, long e, long f) { shim_note("LMGetResLoad called (auto-stub)"); return 0; }

long LMSetResLoad(long a, long b, long c_, long d, long e, long f) { shim_note("LMSetResLoad called (auto-stub)"); return 0; }

long LSCopyApplicationForMIMEType(long a, long b, long c_, long d, long e, long f) { shim_note("LSCopyApplicationForMIMEType called (auto-stub)"); return 0; }

long LSCopyDisplayNameForRef(long a, long b, long c_, long d, long e, long f) { shim_note("LSCopyDisplayNameForRef called (auto-stub)"); return 0; }

long LSCopyDisplayNameForURL(long a, long b, long c_, long d, long e, long f) { shim_note("LSCopyDisplayNameForURL called (auto-stub)"); return 0; }

long LSCopyItemInfoForRef(long a, long b, long c_, long d, long e, long f) { shim_note("LSCopyItemInfoForRef called (auto-stub)"); return 0; }

long LSFindApplicationForInfo(long a, long b, long c_, long d, long e, long f) { shim_note("LSFindApplicationForInfo called (auto-stub)"); return 0; }

long LSGetApplicationForInfo(long a, long b, long c_, long d, long e, long f) { shim_note("LSGetApplicationForInfo called (auto-stub)"); return 0; }

long LSGetApplicationForURL(long a, long b, long c_, long d, long e, long f) { shim_note("LSGetApplicationForURL called (auto-stub)"); return 0; }

long LSOpenFromURLSpec(long a, long b, long c_, long d, long e, long f) { shim_note("LSOpenFromURLSpec called (auto-stub)"); return 0; }

long LaunchApplication(long a, long b, long c_, long d, long e, long f) { shim_note("LaunchApplication called (auto-stub)"); return 0; }

long LoadResource(long a, long b, long c_, long d, long e, long f) { shim_note("LoadResource called (auto-stub)"); return 0; }

long Long2Fix(long a, long b, long c_, long d, long e, long f) { shim_note("Long2Fix called (auto-stub)"); return 0; }

long MDItemCopyAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("MDItemCopyAttribute called (auto-stub)"); return 0; }

long MDItemCreate(long a, long b, long c_, long d, long e, long f) { shim_note("MDItemCreate called (auto-stub)"); return 0; }

long MPCreateQueue(long a, long b, long c_, long d, long e, long f) { shim_note("MPCreateQueue called (auto-stub)"); return 0; }

long MPCreateTask(long a, long b, long c_, long d, long e, long f) { shim_note("MPCreateTask called (auto-stub)"); return 0; }

long MPDeleteQueue(long a, long b, long c_, long d, long e, long f) { shim_note("MPDeleteQueue called (auto-stub)"); return 0; }

long MPNotifyQueue(long a, long b, long c_, long d, long e, long f) { shim_note("MPNotifyQueue called (auto-stub)"); return 0; }

long MPProcessorsScheduled(long a, long b, long c_, long d, long e, long f) { shim_note("MPProcessorsScheduled called (auto-stub)"); return 0; }

long MPTaskIsPreemptive(long a, long b, long c_, long d, long e, long f) { shim_note("MPTaskIsPreemptive called (auto-stub)"); return 0; }

long MPWaitOnQueue(long a, long b, long c_, long d, long e, long f) { shim_note("MPWaitOnQueue called (auto-stub)"); return 0; }

long MemError(long a, long b, long c_, long d, long e, long f) { shim_note("MemError called (auto-stub)"); return 0; }

long Microseconds(long a, long b, long c_, long d, long e, long f) { shim_note("Microseconds called (auto-stub)"); return 0; }

long Munger(long a, long b, long c_, long d, long e, long f) { shim_note("Munger called (auto-stub)"); return 0; }

long NSAccessibilityActionDescription(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityActionDescription called (auto-stub)"); return 0; }

long NSAccessibilityButtonRole(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityButtonRole called (auto-stub)"); return 0; }

long NSAccessibilityCheckBoxRole(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityCheckBoxRole called (auto-stub)"); return 0; }

long NSAccessibilityChildrenAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityChildrenAttribute called (auto-stub)"); return 0; }

long NSAccessibilityColumnCountAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityColumnCountAttribute called (auto-stub)"); return 0; }

long NSAccessibilityDecrementAction(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityDecrementAction called (auto-stub)"); return 0; }

long NSAccessibilityDecrementButtonAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityDecrementButtonAttribute called (auto-stub)"); return 0; }

long NSAccessibilityDescriptionAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityDescriptionAttribute called (auto-stub)"); return 0; }

long NSAccessibilityDisclosureTriangleRole(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityDisclosureTriangleRole called (auto-stub)"); return 0; }

long NSAccessibilityEnabledAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityEnabledAttribute called (auto-stub)"); return 0; }

long NSAccessibilityFocusedAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityFocusedAttribute called (auto-stub)"); return 0; }

long NSAccessibilityFocusedUIElementAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityFocusedUIElementAttribute called (auto-stub)"); return 0; }

long NSAccessibilityFocusedUIElementChangedNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityFocusedUIElementChangedNotification called (auto-stub)"); return 0; }

long NSAccessibilityGridRole(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityGridRole called (auto-stub)"); return 0; }

long NSAccessibilityGroupRole(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityGroupRole called (auto-stub)"); return 0; }

long NSAccessibilityGrowAreaAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityGrowAreaAttribute called (auto-stub)"); return 0; }

long NSAccessibilityGrowAreaRole(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityGrowAreaRole called (auto-stub)"); return 0; }

long NSAccessibilityHelpAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityHelpAttribute called (auto-stub)"); return 0; }

long NSAccessibilityHorizontalOrientationValue(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityHorizontalOrientationValue called (auto-stub)"); return 0; }

long NSAccessibilityImageRole(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityImageRole called (auto-stub)"); return 0; }

long NSAccessibilityIncrementAction(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityIncrementAction called (auto-stub)"); return 0; }

long NSAccessibilityIncrementButtonAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityIncrementButtonAttribute called (auto-stub)"); return 0; }

long NSAccessibilityIncrementorRole(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityIncrementorRole called (auto-stub)"); return 0; }

long NSAccessibilityLinkedUIElementsAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityLinkedUIElementsAttribute called (auto-stub)"); return 0; }

long NSAccessibilityListRole(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityListRole called (auto-stub)"); return 0; }

long NSAccessibilityMaxValueAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityMaxValueAttribute called (auto-stub)"); return 0; }

long NSAccessibilityMinValueAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityMinValueAttribute called (auto-stub)"); return 0; }

long NSAccessibilityOrderedByRowAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityOrderedByRowAttribute called (auto-stub)"); return 0; }

long NSAccessibilityOrientationAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityOrientationAttribute called (auto-stub)"); return 0; }

long NSAccessibilityParentAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityParentAttribute called (auto-stub)"); return 0; }

long NSAccessibilityPositionAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityPositionAttribute called (auto-stub)"); return 0; }

long NSAccessibilityPostNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityPostNotification called (auto-stub)"); return 0; }

long NSAccessibilityPressAction(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityPressAction called (auto-stub)"); return 0; }

long NSAccessibilityRoleAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityRoleAttribute called (auto-stub)"); return 0; }

long NSAccessibilityRoleDescription(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityRoleDescription called (auto-stub)"); return 0; }

long NSAccessibilityRoleDescriptionAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityRoleDescriptionAttribute called (auto-stub)"); return 0; }

long NSAccessibilityRowCountAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityRowCountAttribute called (auto-stub)"); return 0; }

long NSAccessibilitySelectedChildrenAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilitySelectedChildrenAttribute called (auto-stub)"); return 0; }

long NSAccessibilitySelectedChildrenChangedNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilitySelectedChildrenChangedNotification called (auto-stub)"); return 0; }

long NSAccessibilityShowMenuAction(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityShowMenuAction called (auto-stub)"); return 0; }

long NSAccessibilitySizeAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilitySizeAttribute called (auto-stub)"); return 0; }

long NSAccessibilitySliderRole(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilitySliderRole called (auto-stub)"); return 0; }

long NSAccessibilityStaticTextRole(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityStaticTextRole called (auto-stub)"); return 0; }

long NSAccessibilitySubroleAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilitySubroleAttribute called (auto-stub)"); return 0; }

long NSAccessibilityTimelineSubrole(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityTimelineSubrole called (auto-stub)"); return 0; }

long NSAccessibilityTitleAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityTitleAttribute called (auto-stub)"); return 0; }

long NSAccessibilityTitleUIElementAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityTitleUIElementAttribute called (auto-stub)"); return 0; }

long NSAccessibilityTopLevelUIElementAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityTopLevelUIElementAttribute called (auto-stub)"); return 0; }

long NSAccessibilityUnignoredAncestor(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityUnignoredAncestor called (auto-stub)"); return 0; }

long NSAccessibilityUnignoredChildren(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityUnignoredChildren called (auto-stub)"); return 0; }

long NSAccessibilityUnignoredChildrenForOnlyChild(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityUnignoredChildrenForOnlyChild called (auto-stub)"); return 0; }

long NSAccessibilityUnignoredDescendant(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityUnignoredDescendant called (auto-stub)"); return 0; }

long NSAccessibilityValueAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityValueAttribute called (auto-stub)"); return 0; }

long NSAccessibilityValueChangedNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityValueChangedNotification called (auto-stub)"); return 0; }

long NSAccessibilityValueDescriptionAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityValueDescriptionAttribute called (auto-stub)"); return 0; }

long NSAccessibilityValueIndicatorRole(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityValueIndicatorRole called (auto-stub)"); return 0; }

long NSAccessibilityVerticalOrientationValue(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityVerticalOrientationValue called (auto-stub)"); return 0; }

long NSAccessibilityVisibleChildrenAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityVisibleChildrenAttribute called (auto-stub)"); return 0; }

long NSAccessibilityWindowAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityWindowAttribute called (auto-stub)"); return 0; }

long NSAccessibilityWindowRole(long a, long b, long c_, long d, long e, long f) { shim_note("NSAccessibilityWindowRole called (auto-stub)"); return 0; }

long NSAllMapTableKeys(long a, long b, long c_, long d, long e, long f) { shim_note("NSAllMapTableKeys called (auto-stub)"); return 0; }

long NSAllocateMemoryPages(long a, long b, long c_, long d, long e, long f) { shim_note("NSAllocateMemoryPages called (auto-stub)"); return 0; }

long NSAnimationTriggerOrderIn(long a, long b, long c_, long d, long e, long f) { shim_note("NSAnimationTriggerOrderIn called (auto-stub)"); return 0; }

long NSApp(long a, long b, long c_, long d, long e, long f) { shim_note("NSApp called (auto-stub)"); return 0; }

long NSApplicationDidBecomeActiveNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSApplicationDidBecomeActiveNotification called (auto-stub)"); return 0; }

long NSApplicationDidChangeScreenParametersNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSApplicationDidChangeScreenParametersNotification called (auto-stub)"); return 0; }

long NSApplicationDidResignActiveNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSApplicationDidResignActiveNotification called (auto-stub)"); return 0; }

long NSApplicationMain(long a, long b, long c_, long d, long e, long f) { shim_note("NSApplicationMain called (auto-stub)"); return 0; }

long NSApplicationWillBecomeActiveNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSApplicationWillBecomeActiveNotification called (auto-stub)"); return 0; }

long NSApplicationWillHideNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSApplicationWillHideNotification called (auto-stub)"); return 0; }

long NSApplicationWillResignActiveNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSApplicationWillResignActiveNotification called (auto-stub)"); return 0; }

long NSApplicationWillTerminateNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSApplicationWillTerminateNotification called (auto-stub)"); return 0; }

long NSArgumentDomain(long a, long b, long c_, long d, long e, long f) { shim_note("NSArgumentDomain called (auto-stub)"); return 0; }

long NSBackgroundColorAttributeName(long a, long b, long c_, long d, long e, long f) { shim_note("NSBackgroundColorAttributeName called (auto-stub)"); return 0; }

long NSBeep(long a, long b, long c_, long d, long e, long f) { shim_note("NSBeep called (auto-stub)"); return 0; }

long NSCalibratedRGBColorSpace(long a, long b, long c_, long d, long e, long f) { shim_note("NSCalibratedRGBColorSpace called (auto-stub)"); return 0; }

long NSClassFromString(long a, long b, long c_, long d, long e, long f) { shim_note("NSClassFromString called (auto-stub)"); return 0; }

long NSCocoaErrorDomain(long a, long b, long c_, long d, long e, long f) { shim_note("NSCocoaErrorDomain called (auto-stub)"); return 0; }

long NSConnectionReplyMode(long a, long b, long c_, long d, long e, long f) { shim_note("NSConnectionReplyMode called (auto-stub)"); return 0; }

long NSControlTextDidChangeNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSControlTextDidChangeNotification called (auto-stub)"); return 0; }

long NSControlTextDidEndEditingNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSControlTextDidEndEditingNotification called (auto-stub)"); return 0; }

long NSCopyObject(long a, long b, long c_, long d, long e, long f) { shim_note("NSCopyObject called (auto-stub)"); return 0; }

long NSCountHashTable(long a, long b, long c_, long d, long e, long f) { shim_note("NSCountHashTable called (auto-stub)"); return 0; }

long NSCountMapTable(long a, long b, long c_, long d, long e, long f) { shim_note("NSCountMapTable called (auto-stub)"); return 0; }

long NSCreateHashTable(long a, long b, long c_, long d, long e, long f) { shim_note("NSCreateHashTable called (auto-stub)"); return 0; }

long NSCreateMapTable(long a, long b, long c_, long d, long e, long f) { shim_note("NSCreateMapTable called (auto-stub)"); return 0; }

long NSCursorAttributeName(long a, long b, long c_, long d, long e, long f) { shim_note("NSCursorAttributeName called (auto-stub)"); return 0; }

long NSDateFormatString(long a, long b, long c_, long d, long e, long f) { shim_note("NSDateFormatString called (auto-stub)"); return 0; }

long NSDateTimeOrdering(long a, long b, long c_, long d, long e, long f) { shim_note("NSDateTimeOrdering called (auto-stub)"); return 0; }

long NSDeallocateMemoryPages(long a, long b, long c_, long d, long e, long f) { shim_note("NSDeallocateMemoryPages called (auto-stub)"); return 0; }

long NSDefaultRunLoopMode(long a, long b, long c_, long d, long e, long f) { shim_note("NSDefaultRunLoopMode called (auto-stub)"); return 0; }

long NSDeviceRGBColorSpace(long a, long b, long c_, long d, long e, long f) { shim_note("NSDeviceRGBColorSpace called (auto-stub)"); return 0; }

long NSDisableScreenUpdates(long a, long b, long c_, long d, long e, long f) { shim_note("NSDisableScreenUpdates called (auto-stub)"); return 0; }

long NSDragPboard(long a, long b, long c_, long d, long e, long f) { shim_note("NSDragPboard called (auto-stub)"); return 0; }

long NSEnableScreenUpdates(long a, long b, long c_, long d, long e, long f) { shim_note("NSEnableScreenUpdates called (auto-stub)"); return 0; }

long NSEndHashTableEnumeration(long a, long b, long c_, long d, long e, long f) { shim_note("NSEndHashTableEnumeration called (auto-stub)"); return 0; }

long NSEndMapTableEnumeration(long a, long b, long c_, long d, long e, long f) { shim_note("NSEndMapTableEnumeration called (auto-stub)"); return 0; }

long NSEnumerateHashTable(long a, long b, long c_, long d, long e, long f) { shim_note("NSEnumerateHashTable called (auto-stub)"); return 0; }

long NSEnumerateMapTable(long a, long b, long c_, long d, long e, long f) { shim_note("NSEnumerateMapTable called (auto-stub)"); return 0; }

long NSEventTrackingRunLoopMode(long a, long b, long c_, long d, long e, long f) { shim_note("NSEventTrackingRunLoopMode called (auto-stub)"); return 0; }

long NSFileCreationDate(long a, long b, long c_, long d, long e, long f) { shim_note("NSFileCreationDate called (auto-stub)"); return 0; }

long NSFileGroupOwnerAccountName(long a, long b, long c_, long d, long e, long f) { shim_note("NSFileGroupOwnerAccountName called (auto-stub)"); return 0; }

long NSFileHFSTypeCode(long a, long b, long c_, long d, long e, long f) { shim_note("NSFileHFSTypeCode called (auto-stub)"); return 0; }

long NSFileModificationDate(long a, long b, long c_, long d, long e, long f) { shim_note("NSFileModificationDate called (auto-stub)"); return 0; }

long NSFilePosixPermissions(long a, long b, long c_, long d, long e, long f) { shim_note("NSFilePosixPermissions called (auto-stub)"); return 0; }

long NSFileSize(long a, long b, long c_, long d, long e, long f) { shim_note("NSFileSize called (auto-stub)"); return 0; }

long NSFileSystemFreeSize(long a, long b, long c_, long d, long e, long f) { shim_note("NSFileSystemFreeSize called (auto-stub)"); return 0; }

long NSFileType(long a, long b, long c_, long d, long e, long f) { shim_note("NSFileType called (auto-stub)"); return 0; }

long NSFileTypeDirectory(long a, long b, long c_, long d, long e, long f) { shim_note("NSFileTypeDirectory called (auto-stub)"); return 0; }

long NSFileTypeForHFSTypeCode(long a, long b, long c_, long d, long e, long f) { shim_note("NSFileTypeForHFSTypeCode called (auto-stub)"); return 0; }

long NSFileTypeRegular(long a, long b, long c_, long d, long e, long f) { shim_note("NSFileTypeRegular called (auto-stub)"); return 0; }

long NSFileTypeSymbolicLink(long a, long b, long c_, long d, long e, long f) { shim_note("NSFileTypeSymbolicLink called (auto-stub)"); return 0; }

long NSFilenamesPboardType(long a, long b, long c_, long d, long e, long f) { shim_note("NSFilenamesPboardType called (auto-stub)"); return 0; }

long NSFilesPromisePboardType(long a, long b, long c_, long d, long e, long f) { shim_note("NSFilesPromisePboardType called (auto-stub)"); return 0; }

long NSFontAttributeName(long a, long b, long c_, long d, long e, long f) { shim_note("NSFontAttributeName called (auto-stub)"); return 0; }

long NSForegroundColorAttributeName(long a, long b, long c_, long d, long e, long f) { shim_note("NSForegroundColorAttributeName called (auto-stub)"); return 0; }

long NSFreeHashTable(long a, long b, long c_, long d, long e, long f) { shim_note("NSFreeHashTable called (auto-stub)"); return 0; }

long NSFreeMapTable(long a, long b, long c_, long d, long e, long f) { shim_note("NSFreeMapTable called (auto-stub)"); return 0; }

long NSFullUserName(long a, long b, long c_, long d, long e, long f) { shim_note("NSFullUserName called (auto-stub)"); return 0; }

long NSGeneralPboard(long a, long b, long c_, long d, long e, long f) { shim_note("NSGeneralPboard called (auto-stub)"); return 0; }

long NSGenericException(long a, long b, long c_, long d, long e, long f) { shim_note("NSGenericException called (auto-stub)"); return 0; }

long NSGregorianCalendar(long a, long b, long c_, long d, long e, long f) { shim_note("NSGregorianCalendar called (auto-stub)"); return 0; }

long NSHFSTypeOfFile(long a, long b, long c_, long d, long e, long f) { shim_note("NSHFSTypeOfFile called (auto-stub)"); return 0; }

long NSHashGet(long a, long b, long c_, long d, long e, long f) { shim_note("NSHashGet called (auto-stub)"); return 0; }

long NSHashInsert(long a, long b, long c_, long d, long e, long f) { shim_note("NSHashInsert called (auto-stub)"); return 0; }

long NSHashRemove(long a, long b, long c_, long d, long e, long f) { shim_note("NSHashRemove called (auto-stub)"); return 0; }

long NSHomeDirectory(long a, long b, long c_, long d, long e, long f) { shim_note("NSHomeDirectory called (auto-stub)"); return 0; }

long NSImageCompressionFactor(long a, long b, long c_, long d, long e, long f) { shim_note("NSImageCompressionFactor called (auto-stub)"); return 0; }

long NSIntMapValueCallBacks(long a, long b, long c_, long d, long e, long f) { shim_note("NSIntMapValueCallBacks called (auto-stub)"); return 0; }

long NSIntegerMapKeyCallBacks(long a, long b, long c_, long d, long e, long f) { shim_note("NSIntegerMapKeyCallBacks called (auto-stub)"); return 0; }

long NSIntegerMapValueCallBacks(long a, long b, long c_, long d, long e, long f) { shim_note("NSIntegerMapValueCallBacks called (auto-stub)"); return 0; }

long NSInternalInconsistencyException(long a, long b, long c_, long d, long e, long f) { shim_note("NSInternalInconsistencyException called (auto-stub)"); return 0; }

long NSInvalidArchiveOperationException(long a, long b, long c_, long d, long e, long f) { shim_note("NSInvalidArchiveOperationException called (auto-stub)"); return 0; }

long NSInvalidArgumentException(long a, long b, long c_, long d, long e, long f) { shim_note("NSInvalidArgumentException called (auto-stub)"); return 0; }

long NSKeyValueChangeNewKey(long a, long b, long c_, long d, long e, long f) { shim_note("NSKeyValueChangeNewKey called (auto-stub)"); return 0; }

long NSKeyValueChangeNotificationIsPriorKey(long a, long b, long c_, long d, long e, long f) { shim_note("NSKeyValueChangeNotificationIsPriorKey called (auto-stub)"); return 0; }

long NSKeyValueChangeOldKey(long a, long b, long c_, long d, long e, long f) { shim_note("NSKeyValueChangeOldKey called (auto-stub)"); return 0; }

long NSLinkAttributeName(long a, long b, long c_, long d, long e, long f) { shim_note("NSLinkAttributeName called (auto-stub)"); return 0; }

long NSLocaleCountryCode(long a, long b, long c_, long d, long e, long f) { shim_note("NSLocaleCountryCode called (auto-stub)"); return 0; }

long NSLocalizedDescriptionKey(long a, long b, long c_, long d, long e, long f) { shim_note("NSLocalizedDescriptionKey called (auto-stub)"); return 0; }

long NSLocalizedFailureReasonErrorKey(long a, long b, long c_, long d, long e, long f) { shim_note("NSLocalizedFailureReasonErrorKey called (auto-stub)"); return 0; }

long NSLocalizedRecoveryOptionsErrorKey(long a, long b, long c_, long d, long e, long f) { shim_note("NSLocalizedRecoveryOptionsErrorKey called (auto-stub)"); return 0; }

long NSLocalizedRecoverySuggestionErrorKey(long a, long b, long c_, long d, long e, long f) { shim_note("NSLocalizedRecoverySuggestionErrorKey called (auto-stub)"); return 0; }

long NSLogv(long a, long b, long c_, long d, long e, long f) { shim_note("NSLogv called (auto-stub)"); return 0; }

long NSMapGet(long a, long b, long c_, long d, long e, long f) { shim_note("NSMapGet called (auto-stub)"); return 0; }

long NSMapInsert(long a, long b, long c_, long d, long e, long f) { shim_note("NSMapInsert called (auto-stub)"); return 0; }

long NSMapInsertIfAbsent(long a, long b, long c_, long d, long e, long f) { shim_note("NSMapInsertIfAbsent called (auto-stub)"); return 0; }

long NSMapRemove(long a, long b, long c_, long d, long e, long f) { shim_note("NSMapRemove called (auto-stub)"); return 0; }

long NSMetadataQueryDidFinishGatheringNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSMetadataQueryDidFinishGatheringNotification called (auto-stub)"); return 0; }

long NSMetadataQueryDidStartGatheringNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSMetadataQueryDidStartGatheringNotification called (auto-stub)"); return 0; }

long NSModalPanelRunLoopMode(long a, long b, long c_, long d, long e, long f) { shim_note("NSModalPanelRunLoopMode called (auto-stub)"); return 0; }

long NSMonthNameArray(long a, long b, long c_, long d, long e, long f) { shim_note("NSMonthNameArray called (auto-stub)"); return 0; }

long NSMultipleValuesMarker(long a, long b, long c_, long d, long e, long f) { shim_note("NSMultipleValuesMarker called (auto-stub)"); return 0; }

long NSNextHashEnumeratorItem(long a, long b, long c_, long d, long e, long f) { shim_note("NSNextHashEnumeratorItem called (auto-stub)"); return 0; }

long NSNextMapEnumeratorPair(long a, long b, long c_, long d, long e, long f) { shim_note("NSNextMapEnumeratorPair called (auto-stub)"); return 0; }

long NSNoSelectionMarker(long a, long b, long c_, long d, long e, long f) { shim_note("NSNoSelectionMarker called (auto-stub)"); return 0; }

long NSNonOwnedPointerHashCallBacks(long a, long b, long c_, long d, long e, long f) { shim_note("NSNonOwnedPointerHashCallBacks called (auto-stub)"); return 0; }

long NSNonOwnedPointerMapKeyCallBacks(long a, long b, long c_, long d, long e, long f) { shim_note("NSNonOwnedPointerMapKeyCallBacks called (auto-stub)"); return 0; }

long NSNonOwnedPointerMapValueCallBacks(long a, long b, long c_, long d, long e, long f) { shim_note("NSNonOwnedPointerMapValueCallBacks called (auto-stub)"); return 0; }

long NSNonRetainedObjectMapValueCallBacks(long a, long b, long c_, long d, long e, long f) { shim_note("NSNonRetainedObjectMapValueCallBacks called (auto-stub)"); return 0; }

long NSNotApplicableMarker(long a, long b, long c_, long d, long e, long f) { shim_note("NSNotApplicableMarker called (auto-stub)"); return 0; }

long NSOSStatusErrorDomain(long a, long b, long c_, long d, long e, long f) { shim_note("NSOSStatusErrorDomain called (auto-stub)"); return 0; }

long NSObjectMapKeyCallBacks(long a, long b, long c_, long d, long e, long f) { shim_note("NSObjectMapKeyCallBacks called (auto-stub)"); return 0; }

long NSObjectMapValueCallBacks(long a, long b, long c_, long d, long e, long f) { shim_note("NSObjectMapValueCallBacks called (auto-stub)"); return 0; }

long NSObjectNotAvailableException(long a, long b, long c_, long d, long e, long f) { shim_note("NSObjectNotAvailableException called (auto-stub)"); return 0; }

long NSObservedKeyPathKey(long a, long b, long c_, long d, long e, long f) { shim_note("NSObservedKeyPathKey called (auto-stub)"); return 0; }

long NSObservedObjectKey(long a, long b, long c_, long d, long e, long f) { shim_note("NSObservedObjectKey called (auto-stub)"); return 0; }

long NSOutlineViewItemDidCollapseNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSOutlineViewItemDidCollapseNotification called (auto-stub)"); return 0; }

long NSOutlineViewItemDidExpandNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSOutlineViewItemDidExpandNotification called (auto-stub)"); return 0; }

long NSPOSIXErrorDomain(long a, long b, long c_, long d, long e, long f) { shim_note("NSPOSIXErrorDomain called (auto-stub)"); return 0; }

long NSParagraphStyleAttributeName(long a, long b, long c_, long d, long e, long f) { shim_note("NSParagraphStyleAttributeName called (auto-stub)"); return 0; }

long NSPointFromString(long a, long b, long c_, long d, long e, long f) { shim_note("NSPointFromString called (auto-stub)"); return 0; }

long NSPopUpButtonWillPopUpNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSPopUpButtonWillPopUpNotification called (auto-stub)"); return 0; }

long NSPrintSaveJob(long a, long b, long c_, long d, long e, long f) { shim_note("NSPrintSaveJob called (auto-stub)"); return 0; }

long NSPrintSavePath(long a, long b, long c_, long d, long e, long f) { shim_note("NSPrintSavePath called (auto-stub)"); return 0; }

long NSRangeException(long a, long b, long c_, long d, long e, long f) { shim_note("NSRangeException called (auto-stub)"); return 0; }

long NSRecoveryAttempterErrorKey(long a, long b, long c_, long d, long e, long f) { shim_note("NSRecoveryAttempterErrorKey called (auto-stub)"); return 0; }

long NSRectFromString(long a, long b, long c_, long d, long e, long f) { shim_note("NSRectFromString called (auto-stub)"); return 0; }

long NSReleaseAlertPanel(long a, long b, long c_, long d, long e, long f) { shim_note("NSReleaseAlertPanel called (auto-stub)"); return 0; }

long NSResetMapTable(long a, long b, long c_, long d, long e, long f) { shim_note("NSResetMapTable called (auto-stub)"); return 0; }

long NSRunLoopCommonModes(long a, long b, long c_, long d, long e, long f) { shim_note("NSRunLoopCommonModes called (auto-stub)"); return 0; }

long NSSearchPathForDirectoriesInDomains(long a, long b, long c_, long d, long e, long f) { shim_note("NSSearchPathForDirectoriesInDomains called (auto-stub)"); return 0; }

long NSSelectorFromString(long a, long b, long c_, long d, long e, long f) { shim_note("NSSelectorFromString called (auto-stub)"); return 0; }

long NSSetFocusRingStyle(long a, long b, long c_, long d, long e, long f) { shim_note("NSSetFocusRingStyle called (auto-stub)"); return 0; }

long NSShadowAttributeName(long a, long b, long c_, long d, long e, long f) { shim_note("NSShadowAttributeName called (auto-stub)"); return 0; }

long NSShortDateFormatString(long a, long b, long c_, long d, long e, long f) { shim_note("NSShortDateFormatString called (auto-stub)"); return 0; }

long NSShortMonthNameArray(long a, long b, long c_, long d, long e, long f) { shim_note("NSShortMonthNameArray called (auto-stub)"); return 0; }

long NSShortTimeDateFormatString(long a, long b, long c_, long d, long e, long f) { shim_note("NSShortTimeDateFormatString called (auto-stub)"); return 0; }

long NSShortWeekDayNameArray(long a, long b, long c_, long d, long e, long f) { shim_note("NSShortWeekDayNameArray called (auto-stub)"); return 0; }

long NSSizeFromString(long a, long b, long c_, long d, long e, long f) { shim_note("NSSizeFromString called (auto-stub)"); return 0; }

long NSStreamSocketSSLErrorDomain(long a, long b, long c_, long d, long e, long f) { shim_note("NSStreamSocketSSLErrorDomain called (auto-stub)"); return 0; }

long NSStringFromClass(long a, long b, long c_, long d, long e, long f) { shim_note("NSStringFromClass called (auto-stub)"); return 0; }

long NSStringFromSelector(long a, long b, long c_, long d, long e, long f) { shim_note("NSStringFromSelector called (auto-stub)"); return 0; }

long NSStringPboardType(long a, long b, long c_, long d, long e, long f) { shim_note("NSStringPboardType called (auto-stub)"); return 0; }

long NSTIFFPboardType(long a, long b, long c_, long d, long e, long f) { shim_note("NSTIFFPboardType called (auto-stub)"); return 0; }

long NSTaskDidTerminateNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSTaskDidTerminateNotification called (auto-stub)"); return 0; }

long NSTemporaryDirectory(long a, long b, long c_, long d, long e, long f) { shim_note("NSTemporaryDirectory called (auto-stub)"); return 0; }

long NSTextDidEndEditingNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSTextDidEndEditingNotification called (auto-stub)"); return 0; }

long NSTextViewDidChangeSelectionNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSTextViewDidChangeSelectionNotification called (auto-stub)"); return 0; }

long NSTimeFormatString(long a, long b, long c_, long d, long e, long f) { shim_note("NSTimeFormatString called (auto-stub)"); return 0; }

long NSToolbarFlexibleSpaceItemIdentifier(long a, long b, long c_, long d, long e, long f) { shim_note("NSToolbarFlexibleSpaceItemIdentifier called (auto-stub)"); return 0; }

long NSURLAuthenticationMethodHTTPDigest(long a, long b, long c_, long d, long e, long f) { shim_note("NSURLAuthenticationMethodHTTPDigest called (auto-stub)"); return 0; }

long NSURLErrorDomain(long a, long b, long c_, long d, long e, long f) { shim_note("NSURLErrorDomain called (auto-stub)"); return 0; }

long NSUnderlineStyleAttributeName(long a, long b, long c_, long d, long e, long f) { shim_note("NSUnderlineStyleAttributeName called (auto-stub)"); return 0; }

long NSUndoManagerDidRedoChangeNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSUndoManagerDidRedoChangeNotification called (auto-stub)"); return 0; }

long NSUndoManagerDidUndoChangeNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSUndoManagerDidUndoChangeNotification called (auto-stub)"); return 0; }

long NSUndoManagerWillCloseUndoGroupNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSUndoManagerWillCloseUndoGroupNotification called (auto-stub)"); return 0; }

long NSUpdateDynamicServices(long a, long b, long c_, long d, long e, long f) { shim_note("NSUpdateDynamicServices called (auto-stub)"); return 0; }

long NSUserDefaultsDidChangeNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSUserDefaultsDidChangeNotification called (auto-stub)"); return 0; }

long NSUserName(long a, long b, long c_, long d, long e, long f) { shim_note("NSUserName called (auto-stub)"); return 0; }

long NSValueTransformerBindingOption(long a, long b, long c_, long d, long e, long f) { shim_note("NSValueTransformerBindingOption called (auto-stub)"); return 0; }

long NSValueTransformerNameBindingOption(long a, long b, long c_, long d, long e, long f) { shim_note("NSValueTransformerNameBindingOption called (auto-stub)"); return 0; }

long NSViewAnimationEndFrameKey(long a, long b, long c_, long d, long e, long f) { shim_note("NSViewAnimationEndFrameKey called (auto-stub)"); return 0; }

long NSViewAnimationTargetKey(long a, long b, long c_, long d, long e, long f) { shim_note("NSViewAnimationTargetKey called (auto-stub)"); return 0; }

long NSViewBoundsDidChangeNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSViewBoundsDidChangeNotification called (auto-stub)"); return 0; }

long NSViewFrameDidChangeNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSViewFrameDidChangeNotification called (auto-stub)"); return 0; }

long NSWeekDayNameArray(long a, long b, long c_, long d, long e, long f) { shim_note("NSWeekDayNameArray called (auto-stub)"); return 0; }

long NSWindowDidBecomeKeyNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSWindowDidBecomeKeyNotification called (auto-stub)"); return 0; }

long NSWindowDidBecomeMainNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSWindowDidBecomeMainNotification called (auto-stub)"); return 0; }

long NSWindowDidChangeScreenNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSWindowDidChangeScreenNotification called (auto-stub)"); return 0; }

long NSWindowDidEndSheetNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSWindowDidEndSheetNotification called (auto-stub)"); return 0; }

long NSWindowDidMiniaturizeNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSWindowDidMiniaturizeNotification called (auto-stub)"); return 0; }

long NSWindowDidMoveNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSWindowDidMoveNotification called (auto-stub)"); return 0; }

long NSWindowDidResignKeyNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSWindowDidResignKeyNotification called (auto-stub)"); return 0; }

long NSWindowDidResignMainNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSWindowDidResignMainNotification called (auto-stub)"); return 0; }

long NSWindowDidResizeNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSWindowDidResizeNotification called (auto-stub)"); return 0; }

long NSWindowWillBeginSheetNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSWindowWillBeginSheetNotification called (auto-stub)"); return 0; }

long NSWindowWillCloseNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSWindowWillCloseNotification called (auto-stub)"); return 0; }

long NSWindowWillMiniaturizeNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSWindowWillMiniaturizeNotification called (auto-stub)"); return 0; }

long NSWindowWillMoveNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSWindowWillMoveNotification called (auto-stub)"); return 0; }

long NSWorkspaceRecycleOperation(long a, long b, long c_, long d, long e, long f) { shim_note("NSWorkspaceRecycleOperation called (auto-stub)"); return 0; }

long NSWorkspaceWillPowerOffNotification(long a, long b, long c_, long d, long e, long f) { shim_note("NSWorkspaceWillPowerOffNotification called (auto-stub)"); return 0; }

long NSZoneFromPointer(long a, long b, long c_, long d, long e, long f) { shim_note("NSZoneFromPointer called (auto-stub)"); return 0; }

long NewCollection(long a, long b, long c_, long d, long e, long f) { shim_note("NewCollection called (auto-stub)"); return 0; }

long NewHandle(long a, long b, long c_, long d, long e, long f) { shim_note("NewHandle called (auto-stub)"); return 0; }

long NewHandleClear(long a, long b, long c_, long d, long e, long f) { shim_note("NewHandleClear called (auto-stub)"); return 0; }

long NewPtr(long a, long b, long c_, long d, long e, long f) { shim_note("NewPtr called (auto-stub)"); return 0; }

long NewPtrClear(long a, long b, long c_, long d, long e, long f) { shim_note("NewPtrClear called (auto-stub)"); return 0; }

long NewThread(long a, long b, long c_, long d, long e, long f) { shim_note("NewThread called (auto-stub)"); return 0; }

long OpenAComponent(long a, long b, long c_, long d, long e, long f) { shim_note("OpenAComponent called (auto-stub)"); return 0; }

long OpenADefaultComponent(long a, long b, long c_, long d, long e, long f) { shim_note("OpenADefaultComponent called (auto-stub)"); return 0; }

long OpenComponent(long a, long b, long c_, long d, long e, long f) { shim_note("OpenComponent called (auto-stub)"); return 0; }

long OpenDefaultComponent(long a, long b, long c_, long d, long e, long f) { shim_note("OpenDefaultComponent called (auto-stub)"); return 0; }

long PMCopyPageFormat(long a, long b, long c_, long d, long e, long f) { shim_note("PMCopyPageFormat called (auto-stub)"); return 0; }

long PMCopyPrintSettings(long a, long b, long c_, long d, long e, long f) { shim_note("PMCopyPrintSettings called (auto-stub)"); return 0; }

long PMCreateGenericPrinter(long a, long b, long c_, long d, long e, long f) { shim_note("PMCreateGenericPrinter called (auto-stub)"); return 0; }

long PMCreatePageFormatWithPMPaper(long a, long b, long c_, long d, long e, long f) { shim_note("PMCreatePageFormatWithPMPaper called (auto-stub)"); return 0; }

long PMPaperGetHeight(long a, long b, long c_, long d, long e, long f) { shim_note("PMPaperGetHeight called (auto-stub)"); return 0; }

long PMPaperGetID(long a, long b, long c_, long d, long e, long f) { shim_note("PMPaperGetID called (auto-stub)"); return 0; }

long PMPaperGetMargins(long a, long b, long c_, long d, long e, long f) { shim_note("PMPaperGetMargins called (auto-stub)"); return 0; }

long PMPaperGetWidth(long a, long b, long c_, long d, long e, long f) { shim_note("PMPaperGetWidth called (auto-stub)"); return 0; }

long PMPresetCopyName(long a, long b, long c_, long d, long e, long f) { shim_note("PMPresetCopyName called (auto-stub)"); return 0; }

long PMPresetCreatePrintSettings(long a, long b, long c_, long d, long e, long f) { shim_note("PMPresetCreatePrintSettings called (auto-stub)"); return 0; }

long PMPresetGetAttributes(long a, long b, long c_, long d, long e, long f) { shim_note("PMPresetGetAttributes called (auto-stub)"); return 0; }

long PMPrinterCopyDescriptionURL(long a, long b, long c_, long d, long e, long f) { shim_note("PMPrinterCopyDescriptionURL called (auto-stub)"); return 0; }

long PMPrinterCopyPresets(long a, long b, long c_, long d, long e, long f) { shim_note("PMPrinterCopyPresets called (auto-stub)"); return 0; }

long PMPrinterGetName(long a, long b, long c_, long d, long e, long f) { shim_note("PMPrinterGetName called (auto-stub)"); return 0; }

long PMPrinterGetPaperList(long a, long b, long c_, long d, long e, long f) { shim_note("PMPrinterGetPaperList called (auto-stub)"); return 0; }

long PMPrinterIsDefault(long a, long b, long c_, long d, long e, long f) { shim_note("PMPrinterIsDefault called (auto-stub)"); return 0; }

long PMPrinterIsFavorite(long a, long b, long c_, long d, long e, long f) { shim_note("PMPrinterIsFavorite called (auto-stub)"); return 0; }

long PMRelease(long a, long b, long c_, long d, long e, long f) { shim_note("PMRelease called (auto-stub)"); return 0; }

long PMRetain(long a, long b, long c_, long d, long e, long f) { shim_note("PMRetain called (auto-stub)"); return 0; }

long PMServerCreatePrinterList(long a, long b, long c_, long d, long e, long f) { shim_note("PMServerCreatePrinterList called (auto-stub)"); return 0; }

long PMSessionGetCurrentPrinter(long a, long b, long c_, long d, long e, long f) { shim_note("PMSessionGetCurrentPrinter called (auto-stub)"); return 0; }

long PMSessionValidatePageFormat(long a, long b, long c_, long d, long e, long f) { shim_note("PMSessionValidatePageFormat called (auto-stub)"); return 0; }

long PMSessionValidatePrintSettings(long a, long b, long c_, long d, long e, long f) { shim_note("PMSessionValidatePrintSettings called (auto-stub)"); return 0; }

long PtrToHand(long a, long b, long c_, long d, long e, long f) { shim_note("PtrToHand called (auto-stub)"); return 0; }

long ReleaseIconRef(long a, long b, long c_, long d, long e, long f) { shim_note("ReleaseIconRef called (auto-stub)"); return 0; }

long ReleaseResource(long a, long b, long c_, long d, long e, long f) { shim_note("ReleaseResource called (auto-stub)"); return 0; }

long RemoveResource(long a, long b, long c_, long d, long e, long f) { shim_note("RemoveResource called (auto-stub)"); return 0; }

long ResError(long a, long b, long c_, long d, long e, long f) { shim_note("ResError called (auto-stub)"); return 0; }

long ResolveComponentAlias(long a, long b, long c_, long d, long e, long f) { shim_note("ResolveComponentAlias called (auto-stub)"); return 0; }

long SameProcess(long a, long b, long c_, long d, long e, long f) { shim_note("SameProcess called (auto-stub)"); return 0; }

long SetFrontProcess(long a, long b, long c_, long d, long e, long f) { shim_note("SetFrontProcess called (auto-stub)"); return 0; }

long SetHandleSize(long a, long b, long c_, long d, long e, long f) { shim_note("SetHandleSize called (auto-stub)"); return 0; }

long SetResAttrs(long a, long b, long c_, long d, long e, long f) { shim_note("SetResAttrs called (auto-stub)"); return 0; }

long SetResLoad(long a, long b, long c_, long d, long e, long f) { shim_note("SetResLoad called (auto-stub)"); return 0; }

long SetThreadReadyGivenTaskRef(long a, long b, long c_, long d, long e, long f) { shim_note("SetThreadReadyGivenTaskRef called (auto-stub)"); return 0; }

long SetThreadScheduler(long a, long b, long c_, long d, long e, long f) { shim_note("SetThreadScheduler called (auto-stub)"); return 0; }

long SetThreadState(long a, long b, long c_, long d, long e, long f) { shim_note("SetThreadState called (auto-stub)"); return 0; }

long TECConvertText(long a, long b, long c_, long d, long e, long f) { shim_note("TECConvertText called (auto-stub)"); return 0; }

long TECCreateConverter(long a, long b, long c_, long d, long e, long f) { shim_note("TECCreateConverter called (auto-stub)"); return 0; }

long TECDisposeConverter(long a, long b, long c_, long d, long e, long f) { shim_note("TECDisposeConverter called (auto-stub)"); return 0; }

long TempNewHandle(long a, long b, long c_, long d, long e, long f) { shim_note("TempNewHandle called (auto-stub)"); return 0; }

long TickCount(long a, long b, long c_, long d, long e, long f) { shim_note("TickCount called (auto-stub)"); return 0; }

long UCCompareTextDefault(long a, long b, long c_, long d, long e, long f) { shim_note("UCCompareTextDefault called (auto-stub)"); return 0; }

long UCGetCharProperty(long a, long b, long c_, long d, long e, long f) { shim_note("UCGetCharProperty called (auto-stub)"); return 0; }

long UTGetOSTypeFromString(long a, long b, long c_, long d, long e, long f) { shim_note("UTGetOSTypeFromString called (auto-stub)"); return 0; }

long Unique1ID(long a, long b, long c_, long d, long e, long f) { shim_note("Unique1ID called (auto-stub)"); return 0; }

long UnregisterComponent(long a, long b, long c_, long d, long e, long f) { shim_note("UnregisterComponent called (auto-stub)"); return 0; }

long UpdateResFile(long a, long b, long c_, long d, long e, long f) { shim_note("UpdateResFile called (auto-stub)"); return 0; }

long UpdateSystemActivity(long a, long b, long c_, long d, long e, long f) { shim_note("UpdateSystemActivity called (auto-stub)"); return 0; }

long UseResFile(long a, long b, long c_, long d, long e, long f) { shim_note("UseResFile called (auto-stub)"); return 0; }

long WriteResource(long a, long b, long c_, long d, long e, long f) { shim_note("WriteResource called (auto-stub)"); return 0; }

long YieldToAnyThread(long a, long b, long c_, long d, long e, long f) { shim_note("YieldToAnyThread called (auto-stub)"); return 0; }

long YieldToThread(long a, long b, long c_, long d, long e, long f) { shim_note("YieldToThread called (auto-stub)"); return 0; }

long assert_rtn(long a, long b, long c_, long d, long e, long f) { shim_note("assert_rtn called (auto-stub)"); return 0; }

long error(long a, long b, long c_, long d, long e, long f) { shim_note("error called (auto-stub)"); return 0; }

long maskrune(long a, long b, long c_, long d, long e, long f) { shim_note("maskrune called (auto-stub)"); return 0; }

long sprintf_chk(long a, long b, long c_, long d, long e, long f) { shim_note("sprintf_chk called (auto-stub)"); return 0; }

long accept(long a, long b, long c_, long d, long e, long f) { shim_note("accept called (auto-stub)"); return 0; }

long access(long a, long b, long c_, long d, long e, long f) { shim_note("access called (auto-stub)"); return 0; }

long arc4random(long a, long b, long c_, long d, long e, long f) { shim_note("arc4random called (auto-stub)"); return 0; }

long atexit(long a, long b, long c_, long d, long e, long f) { shim_note("atexit called (auto-stub)"); return 0; }

long atof(long a, long b, long c_, long d, long e, long f) { shim_note("atof called (auto-stub)"); return 0; }

long atoi(long a, long b, long c_, long d, long e, long f) { shim_note("atoi called (auto-stub)"); return 0; }

long bcopy(long a, long b, long c_, long d, long e, long f) { shim_note("bcopy called (auto-stub)"); return 0; }

long bind(long a, long b, long c_, long d, long e, long f) { shim_note("bind called (auto-stub)"); return 0; }

long calloc(long a, long b, long c_, long d, long e, long f) { shim_note("calloc called (auto-stub)"); return 0; }

long close(long a, long b, long c_, long d, long e, long f) { shim_note("close called (auto-stub)"); return 0; }

long closedir(long a, long b, long c_, long d, long e, long f) { shim_note("closedir called (auto-stub)"); return 0; }

long connect(long a, long b, long c_, long d, long e, long f) { shim_note("connect called (auto-stub)"); return 0; }

long creat(long a, long b, long c_, long d, long e, long f) { shim_note("creat called (auto-stub)"); return 0; }

long ctime(long a, long b, long c_, long d, long e, long f) { shim_note("ctime called (auto-stub)"); return 0; }

long exit(long a, long b, long c_, long d, long e, long f) { shim_note("exit called (auto-stub)"); return 0; }

long fclose(long a, long b, long c_, long d, long e, long f) { shim_note("fclose called (auto-stub)"); return 0; }

long feof(long a, long b, long c_, long d, long e, long f) { shim_note("feof called (auto-stub)"); return 0; }

long ferror(long a, long b, long c_, long d, long e, long f) { shim_note("ferror called (auto-stub)"); return 0; }

long fflush(long a, long b, long c_, long d, long e, long f) { shim_note("fflush called (auto-stub)"); return 0; }

long fileno(long a, long b, long c_, long d, long e, long f) { shim_note("fileno called (auto-stub)"); return 0; }

long fopen(long a, long b, long c_, long d, long e, long f) { shim_note("fopen called (auto-stub)"); return 0; }

long fprintf(long a, long b, long c_, long d, long e, long f) { shim_note("fprintf called (auto-stub)"); return 0; }

long fputc(long a, long b, long c_, long d, long e, long f) { shim_note("fputc called (auto-stub)"); return 0; }

long fputs(long a, long b, long c_, long d, long e, long f) { shim_note("fputs called (auto-stub)"); return 0; }

long fread(long a, long b, long c_, long d, long e, long f) { shim_note("fread called (auto-stub)"); return 0; }

long free(long a, long b, long c_, long d, long e, long f) { shim_note("free called (auto-stub)"); return 0; }

long fscanf(long a, long b, long c_, long d, long e, long f) { shim_note("fscanf called (auto-stub)"); return 0; }

long fseek(long a, long b, long c_, long d, long e, long f) { shim_note("fseek called (auto-stub)"); return 0; }

long ftell(long a, long b, long c_, long d, long e, long f) { shim_note("ftell called (auto-stub)"); return 0; }

long funopen(long a, long b, long c_, long d, long e, long f) { shim_note("funopen called (auto-stub)"); return 0; }

long fwrite(long a, long b, long c_, long d, long e, long f) { shim_note("fwrite called (auto-stub)"); return 0; }

long getaddrinfo(long a, long b, long c_, long d, long e, long f) { shim_note("getaddrinfo called (auto-stub)"); return 0; }

long geteuid(long a, long b, long c_, long d, long e, long f) { shim_note("geteuid called (auto-stub)"); return 0; }

long gethostbyname(long a, long b, long c_, long d, long e, long f) { shim_note("gethostbyname called (auto-stub)"); return 0; }

long gethostname(long a, long b, long c_, long d, long e, long f) { shim_note("gethostname called (auto-stub)"); return 0; }

long getpeername(long a, long b, long c_, long d, long e, long f) { shim_note("getpeername called (auto-stub)"); return 0; }

long getpid(long a, long b, long c_, long d, long e, long f) { shim_note("getpid called (auto-stub)"); return 0; }

long getppid(long a, long b, long c_, long d, long e, long f) { shim_note("getppid called (auto-stub)"); return 0; }

long getrusage(long a, long b, long c_, long d, long e, long f) { shim_note("getrusage called (auto-stub)"); return 0; }

long getsockname(long a, long b, long c_, long d, long e, long f) { shim_note("getsockname called (auto-stub)"); return 0; }

long gettimeofday(long a, long b, long c_, long d, long e, long f) { shim_note("gettimeofday called (auto-stub)"); return 0; }

long getuid(long a, long b, long c_, long d, long e, long f) { shim_note("getuid called (auto-stub)"); return 0; }

long gmtime_r(long a, long b, long c_, long d, long e, long f) { shim_note("gmtime_r called (auto-stub)"); return 0; }

long host_info(long a, long b, long c_, long d, long e, long f) { shim_note("host_info called (auto-stub)"); return 0; }

long inet_addr(long a, long b, long c_, long d, long e, long f) { shim_note("inet_addr called (auto-stub)"); return 0; }

long inet_aton(long a, long b, long c_, long d, long e, long f) { shim_note("inet_aton called (auto-stub)"); return 0; }

long inet_makeaddr(long a, long b, long c_, long d, long e, long f) { shim_note("inet_makeaddr called (auto-stub)"); return 0; }

long kCFAllocatorDefault(long a, long b, long c_, long d, long e, long f) { shim_note("kCFAllocatorDefault called (auto-stub)"); return 0; }

long kCFAllocatorMalloc(long a, long b, long c_, long d, long e, long f) { shim_note("kCFAllocatorMalloc called (auto-stub)"); return 0; }

long kCFAllocatorNull(long a, long b, long c_, long d, long e, long f) { shim_note("kCFAllocatorNull called (auto-stub)"); return 0; }

long kCFAllocatorSystemDefault(long a, long b, long c_, long d, long e, long f) { shim_note("kCFAllocatorSystemDefault called (auto-stub)"); return 0; }

long kCFBooleanFalse(long a, long b, long c_, long d, long e, long f) { shim_note("kCFBooleanFalse called (auto-stub)"); return 0; }

long kCFBooleanTrue(long a, long b, long c_, long d, long e, long f) { shim_note("kCFBooleanTrue called (auto-stub)"); return 0; }

long kCFBundleExecutableKey(long a, long b, long c_, long d, long e, long f) { shim_note("kCFBundleExecutableKey called (auto-stub)"); return 0; }

long kCFBundleIdentifierKey(long a, long b, long c_, long d, long e, long f) { shim_note("kCFBundleIdentifierKey called (auto-stub)"); return 0; }

long kCFBundleNameKey(long a, long b, long c_, long d, long e, long f) { shim_note("kCFBundleNameKey called (auto-stub)"); return 0; }

long kCFBundleVersionKey(long a, long b, long c_, long d, long e, long f) { shim_note("kCFBundleVersionKey called (auto-stub)"); return 0; }

long kCFGregorianCalendar(long a, long b, long c_, long d, long e, long f) { shim_note("kCFGregorianCalendar called (auto-stub)"); return 0; }

long kCFPreferencesCurrentApplication(long a, long b, long c_, long d, long e, long f) { shim_note("kCFPreferencesCurrentApplication called (auto-stub)"); return 0; }

long kCFPreferencesCurrentHost(long a, long b, long c_, long d, long e, long f) { shim_note("kCFPreferencesCurrentHost called (auto-stub)"); return 0; }

long kCFPreferencesCurrentUser(long a, long b, long c_, long d, long e, long f) { shim_note("kCFPreferencesCurrentUser called (auto-stub)"); return 0; }

long kCFRunLoopCommonModes(long a, long b, long c_, long d, long e, long f) { shim_note("kCFRunLoopCommonModes called (auto-stub)"); return 0; }

long kCFRunLoopDefaultMode(long a, long b, long c_, long d, long e, long f) { shim_note("kCFRunLoopDefaultMode called (auto-stub)"); return 0; }

long kCFStreamPropertySOCKSProxyHost(long a, long b, long c_, long d, long e, long f) { shim_note("kCFStreamPropertySOCKSProxyHost called (auto-stub)"); return 0; }

long kCFStreamPropertySOCKSProxyPort(long a, long b, long c_, long d, long e, long f) { shim_note("kCFStreamPropertySOCKSProxyPort called (auto-stub)"); return 0; }

long kCFStreamPropertySocketNativeHandle(long a, long b, long c_, long d, long e, long f) { shim_note("kCFStreamPropertySocketNativeHandle called (auto-stub)"); return 0; }

long kMDItemCodecs(long a, long b, long c_, long d, long e, long f) { shim_note("kMDItemCodecs called (auto-stub)"); return 0; }

long kMDItemContentTypeTree(long a, long b, long c_, long d, long e, long f) { shim_note("kMDItemContentTypeTree called (auto-stub)"); return 0; }

long kMDItemDurationSeconds(long a, long b, long c_, long d, long e, long f) { shim_note("kMDItemDurationSeconds called (auto-stub)"); return 0; }

long kMDItemPath(long a, long b, long c_, long d, long e, long f) { shim_note("kMDItemPath called (auto-stub)"); return 0; }

long kMDItemPixelHeight(long a, long b, long c_, long d, long e, long f) { shim_note("kMDItemPixelHeight called (auto-stub)"); return 0; }

long kMDItemPixelWidth(long a, long b, long c_, long d, long e, long f) { shim_note("kMDItemPixelWidth called (auto-stub)"); return 0; }

long kMDItemTitle(long a, long b, long c_, long d, long e, long f) { shim_note("kMDItemTitle called (auto-stub)"); return 0; }

long kUTTypeJPEG(long a, long b, long c_, long d, long e, long f) { shim_note("kUTTypeJPEG called (auto-stub)"); return 0; }

long kUTTypePNG(long a, long b, long c_, long d, long e, long f) { shim_note("kUTTypePNG called (auto-stub)"); return 0; }

long kUTTypeTIFF(long a, long b, long c_, long d, long e, long f) { shim_note("kUTTypeTIFF called (auto-stub)"); return 0; }

long lchmod(long a, long b, long c_, long d, long e, long f) { shim_note("lchmod called (auto-stub)"); return 0; }

long lchown(long a, long b, long c_, long d, long e, long f) { shim_note("lchown called (auto-stub)"); return 0; }

long listen(long a, long b, long c_, long d, long e, long f) { shim_note("listen called (auto-stub)"); return 0; }

long localtime(long a, long b, long c_, long d, long e, long f) { shim_note("localtime called (auto-stub)"); return 0; }

long mach_host_self(long a, long b, long c_, long d, long e, long f) { shim_note("mach_host_self called (auto-stub)"); return 0; }

long mach_port_deallocate(long a, long b, long c_, long d, long e, long f) { shim_note("mach_port_deallocate called (auto-stub)"); return 0; }

long mach_thread_self(long a, long b, long c_, long d, long e, long f) { shim_note("mach_thread_self called (auto-stub)"); return 0; }

long malloc(long a, long b, long c_, long d, long e, long f) { shim_note("malloc called (auto-stub)"); return 0; }

long memchr(long a, long b, long c_, long d, long e, long f) { shim_note("memchr called (auto-stub)"); return 0; }

long memcmp(long a, long b, long c_, long d, long e, long f) { shim_note("memcmp called (auto-stub)"); return 0; }

long memcpy(long a, long b, long c_, long d, long e, long f) { shim_note("memcpy called (auto-stub)"); return 0; }

long memmove(long a, long b, long c_, long d, long e, long f) { shim_note("memmove called (auto-stub)"); return 0; }

long memset(long a, long b, long c_, long d, long e, long f) { shim_note("memset called (auto-stub)"); return 0; }

long mkdir(long a, long b, long c_, long d, long e, long f) { shim_note("mkdir called (auto-stub)"); return 0; }

long mkstemp(long a, long b, long c_, long d, long e, long f) { shim_note("mkstemp called (auto-stub)"); return 0; }

long mktime(long a, long b, long c_, long d, long e, long f) { shim_note("mktime called (auto-stub)"); return 0; }

long objc_msgSend(long a, long b, long c_, long d, long e, long f) { shim_note("objc_msgSend called (auto-stub)"); return 0; }

long objc_msgSendSuper(long a, long b, long c_, long d, long e, long f) { shim_note("objc_msgSendSuper called (auto-stub)"); return 0; }

long objc_msgSendSuper_stret(long a, long b, long c_, long d, long e, long f) { shim_note("objc_msgSendSuper_stret called (auto-stub)"); return 0; }

long objc_msgSend_fpret(long a, long b, long c_, long d, long e, long f) { shim_note("objc_msgSend_fpret called (auto-stub)"); return 0; }

long objc_msgSend_stret(long a, long b, long c_, long d, long e, long f) { shim_note("objc_msgSend_stret called (auto-stub)"); return 0; }

long opendir$INODE64(long a, long b, long c_, long d, long e, long f) { shim_note("opendir$INODE64 called (auto-stub)"); return 0; }

long pclose(long a, long b, long c_, long d, long e, long f) { shim_note("pclose called (auto-stub)"); return 0; }

long pipe(long a, long b, long c_, long d, long e, long f) { shim_note("pipe called (auto-stub)"); return 0; }

long popen(long a, long b, long c_, long d, long e, long f) { shim_note("popen called (auto-stub)"); return 0; }

long printf(long a, long b, long c_, long d, long e, long f) { shim_note("printf called (auto-stub)"); return 0; }

long pthread_attr_destroy(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_attr_destroy called (auto-stub)"); return 0; }

long pthread_attr_init(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_attr_init called (auto-stub)"); return 0; }

long pthread_attr_setdetachstate(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_attr_setdetachstate called (auto-stub)"); return 0; }

long pthread_cancel(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_cancel called (auto-stub)"); return 0; }

long pthread_cond_broadcast(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_cond_broadcast called (auto-stub)"); return 0; }

long pthread_cond_destroy(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_cond_destroy called (auto-stub)"); return 0; }

long pthread_cond_init(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_cond_init called (auto-stub)"); return 0; }

long pthread_cond_signal(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_cond_signal called (auto-stub)"); return 0; }

long pthread_cond_timedwait(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_cond_timedwait called (auto-stub)"); return 0; }

long pthread_cond_timedwait_relative_np(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_cond_timedwait_relative_np called (auto-stub)"); return 0; }

long pthread_cond_wait(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_cond_wait called (auto-stub)"); return 0; }

long pthread_create(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_create called (auto-stub)"); return 0; }

long pthread_equal(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_equal called (auto-stub)"); return 0; }

long pthread_exit(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_exit called (auto-stub)"); return 0; }

long pthread_join(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_join called (auto-stub)"); return 0; }

long pthread_mach_thread_np(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_mach_thread_np called (auto-stub)"); return 0; }

long pthread_main_np(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_main_np called (auto-stub)"); return 0; }

long pthread_mutex_destroy(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_mutex_destroy called (auto-stub)"); return 0; }

long pthread_mutex_init(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_mutex_init called (auto-stub)"); return 0; }

long pthread_mutex_lock(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_mutex_lock called (auto-stub)"); return 0; }

long pthread_mutex_trylock(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_mutex_trylock called (auto-stub)"); return 0; }

long pthread_mutex_unlock(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_mutex_unlock called (auto-stub)"); return 0; }

long pthread_mutexattr_init(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_mutexattr_init called (auto-stub)"); return 0; }

long pthread_mutexattr_settype(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_mutexattr_settype called (auto-stub)"); return 0; }

long pthread_once(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_once called (auto-stub)"); return 0; }

long pthread_self(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_self called (auto-stub)"); return 0; }

long pthread_setname_np(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_setname_np called (auto-stub)"); return 0; }

long pthread_yield_np(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_yield_np called (auto-stub)"); return 0; }

long putchar(long a, long b, long c_, long d, long e, long f) { shim_note("putchar called (auto-stub)"); return 0; }

long puts(long a, long b, long c_, long d, long e, long f) { shim_note("puts called (auto-stub)"); return 0; }

long pwrite(long a, long b, long c_, long d, long e, long f) { shim_note("pwrite called (auto-stub)"); return 0; }

long qsort(long a, long b, long c_, long d, long e, long f) { shim_note("qsort called (auto-stub)"); return 0; }

long rand(long a, long b, long c_, long d, long e, long f) { shim_note("rand called (auto-stub)"); return 0; }

long random(long a, long b, long c_, long d, long e, long f) { shim_note("random called (auto-stub)"); return 0; }

long read(long a, long b, long c_, long d, long e, long f) { shim_note("read called (auto-stub)"); return 0; }

long readdir$INODE64(long a, long b, long c_, long d, long e, long f) { shim_note("readdir$INODE64 called (auto-stub)"); return 0; }

long realloc(long a, long b, long c_, long d, long e, long f) { shim_note("realloc called (auto-stub)"); return 0; }

long realpath$DARWIN_EXTSN(long a, long b, long c_, long d, long e, long f) { shim_note("realpath$DARWIN_EXTSN called (auto-stub)"); return 0; }

long recv(long a, long b, long c_, long d, long e, long f) { shim_note("recv called (auto-stub)"); return 0; }

long recvfrom(long a, long b, long c_, long d, long e, long f) { shim_note("recvfrom called (auto-stub)"); return 0; }

long remove(long a, long b, long c_, long d, long e, long f) { shim_note("remove called (auto-stub)"); return 0; }

long rewind(long a, long b, long c_, long d, long e, long f) { shim_note("rewind called (auto-stub)"); return 0; }

long rmdir(long a, long b, long c_, long d, long e, long f) { shim_note("rmdir called (auto-stub)"); return 0; }

long sched_yield(long a, long b, long c_, long d, long e, long f) { shim_note("sched_yield called (auto-stub)"); return 0; }

long select(long a, long b, long c_, long d, long e, long f) { shim_note("select called (auto-stub)"); return 0; }

long send(long a, long b, long c_, long d, long e, long f) { shim_note("send called (auto-stub)"); return 0; }

long sendto(long a, long b, long c_, long d, long e, long f) { shim_note("sendto called (auto-stub)"); return 0; }

long setbuf(long a, long b, long c_, long d, long e, long f) { shim_note("setbuf called (auto-stub)"); return 0; }

long setsockopt(long a, long b, long c_, long d, long e, long f) { shim_note("setsockopt called (auto-stub)"); return 0; }

long setvbuf(long a, long b, long c_, long d, long e, long f) { shim_note("setvbuf called (auto-stub)"); return 0; }

long shutdown(long a, long b, long c_, long d, long e, long f) { shim_note("shutdown called (auto-stub)"); return 0; }

long sigaction(long a, long b, long c_, long d, long e, long f) { shim_note("sigaction called (auto-stub)"); return 0; }

long sleep(long a, long b, long c_, long d, long e, long f) { shim_note("sleep called (auto-stub)"); return 0; }

long snprintf(long a, long b, long c_, long d, long e, long f) { shim_note("snprintf called (auto-stub)"); return 0; }

long socket(long a, long b, long c_, long d, long e, long f) { shim_note("socket called (auto-stub)"); return 0; }

long sprintf(long a, long b, long c_, long d, long e, long f) { shim_note("sprintf called (auto-stub)"); return 0; }

long srand(long a, long b, long c_, long d, long e, long f) { shim_note("srand called (auto-stub)"); return 0; }

long srandom(long a, long b, long c_, long d, long e, long f) { shim_note("srandom called (auto-stub)"); return 0; }

long sscanf(long a, long b, long c_, long d, long e, long f) { shim_note("sscanf called (auto-stub)"); return 0; }

long strcasecmp(long a, long b, long c_, long d, long e, long f) { shim_note("strcasecmp called (auto-stub)"); return 0; }

long strcat(long a, long b, long c_, long d, long e, long f) { shim_note("strcat called (auto-stub)"); return 0; }

long strchr(long a, long b, long c_, long d, long e, long f) { shim_note("strchr called (auto-stub)"); return 0; }

long strcmp(long a, long b, long c_, long d, long e, long f) { shim_note("strcmp called (auto-stub)"); return 0; }

long strcpy(long a, long b, long c_, long d, long e, long f) { shim_note("strcpy called (auto-stub)"); return 0; }

long strdup(long a, long b, long c_, long d, long e, long f) { shim_note("strdup called (auto-stub)"); return 0; }

long strerror(long a, long b, long c_, long d, long e, long f) { shim_note("strerror called (auto-stub)"); return 0; }

long strlen(long a, long b, long c_, long d, long e, long f) { shim_note("strlen called (auto-stub)"); return 0; }

long strncasecmp(long a, long b, long c_, long d, long e, long f) { shim_note("strncasecmp called (auto-stub)"); return 0; }

long strncat(long a, long b, long c_, long d, long e, long f) { shim_note("strncat called (auto-stub)"); return 0; }

long strncmp(long a, long b, long c_, long d, long e, long f) { shim_note("strncmp called (auto-stub)"); return 0; }

long strncpy(long a, long b, long c_, long d, long e, long f) { shim_note("strncpy called (auto-stub)"); return 0; }

long strnstr(long a, long b, long c_, long d, long e, long f) { shim_note("strnstr called (auto-stub)"); return 0; }

long strrchr(long a, long b, long c_, long d, long e, long f) { shim_note("strrchr called (auto-stub)"); return 0; }

long strstr(long a, long b, long c_, long d, long e, long f) { shim_note("strstr called (auto-stub)"); return 0; }

long strtol(long a, long b, long c_, long d, long e, long f) { shim_note("strtol called (auto-stub)"); return 0; }

long strtoul(long a, long b, long c_, long d, long e, long f) { shim_note("strtoul called (auto-stub)"); return 0; }

long strtouq(long a, long b, long c_, long d, long e, long f) { shim_note("strtouq called (auto-stub)"); return 0; }

long sysctl(long a, long b, long c_, long d, long e, long f) { shim_note("sysctl called (auto-stub)"); return 0; }

long sysctlbyname(long a, long b, long c_, long d, long e, long f) { shim_note("sysctlbyname called (auto-stub)"); return 0; }

long task_info(long a, long b, long c_, long d, long e, long f) { shim_note("task_info called (auto-stub)"); return 0; }

long task_threads(long a, long b, long c_, long d, long e, long f) { shim_note("task_threads called (auto-stub)"); return 0; }

long thread_info(long a, long b, long c_, long d, long e, long f) { shim_note("thread_info called (auto-stub)"); return 0; }

long thread_policy(long a, long b, long c_, long d, long e, long f) { shim_note("thread_policy called (auto-stub)"); return 0; }

long thread_policy_set(long a, long b, long c_, long d, long e, long f) { shim_note("thread_policy_set called (auto-stub)"); return 0; }

long thread_switch(long a, long b, long c_, long d, long e, long f) { shim_note("thread_switch called (auto-stub)"); return 0; }

long time(long a, long b, long c_, long d, long e, long f) { shim_note("time called (auto-stub)"); return 0; }

long unlink(long a, long b, long c_, long d, long e, long f) { shim_note("unlink called (auto-stub)"); return 0; }

long usleep(long a, long b, long c_, long d, long e, long f) { shim_note("usleep called (auto-stub)"); return 0; }

long utimes(long a, long b, long c_, long d, long e, long f) { shim_note("utimes called (auto-stub)"); return 0; }

long vfprintf(long a, long b, long c_, long d, long e, long f) { shim_note("vfprintf called (auto-stub)"); return 0; }

long vm_deallocate(long a, long b, long c_, long d, long e, long f) { shim_note("vm_deallocate called (auto-stub)"); return 0; }

long vm_region_64(long a, long b, long c_, long d, long e, long f) { shim_note("vm_region_64 called (auto-stub)"); return 0; }

long vprintf(long a, long b, long c_, long d, long e, long f) { shim_note("vprintf called (auto-stub)"); return 0; }

long waitpid(long a, long b, long c_, long d, long e, long f) { shim_note("waitpid called (auto-stub)"); return 0; }

long write(long a, long b, long c_, long d, long e, long f) { shim_note("write called (auto-stub)"); return 0; }

long dyld_stub_binder(long a, long b, long c_, long d, long e, long f) { shim_note("dyld_stub_binder called (auto-stub)"); return 0; }
