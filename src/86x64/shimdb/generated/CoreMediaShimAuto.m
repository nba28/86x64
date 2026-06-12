#import <Cocoa/Cocoa.h>
#include <stdio.h>

static void shim_note(const char *s) { fprintf(stderr, "[shimauto:%s] %s\n", "CoreMedia", s); }

long FigAudioFormatDescriptionGetChannelLayout(long a, long b, long c_, long d, long e, long f) { shim_note("FigAudioFormatDescriptionGetChannelLayout called (auto-stub)"); return 0; }

long FigAudioFormatDescriptionGetFormatList(long a, long b, long c_, long d, long e, long f) { shim_note("FigAudioFormatDescriptionGetFormatList called (auto-stub)"); return 0; }

long FigAudioFormatDescriptionGetMagicCookie(long a, long b, long c_, long d, long e, long f) { shim_note("FigAudioFormatDescriptionGetMagicCookie called (auto-stub)"); return 0; }

long FigBaseClassGetCFTypeID(long a, long b, long c_, long d, long e, long f) { shim_note("FigBaseClassGetCFTypeID called (auto-stub)"); return 0; }

long FigBaseClassRegisterClass(long a, long b, long c_, long d, long e, long f) { shim_note("FigBaseClassRegisterClass called (auto-stub)"); return 0; }

long FigBaseGetClassID(long a, long b, long c_, long d, long e, long f) { shim_note("FigBaseGetClassID called (auto-stub)"); return 0; }

long FigBaseObjectGetDerivedStorage(long a, long b, long c_, long d, long e, long f) { shim_note("FigBaseObjectGetDerivedStorage called (auto-stub)"); return 0; }

long FigBaseObjectGetVTable(long a, long b, long c_, long d, long e, long f) { shim_note("FigBaseObjectGetVTable called (auto-stub)"); return 0; }

long FigBlockBufferCreateWithMemoryBlock(long a, long b, long c_, long d, long e, long f) { shim_note("FigBlockBufferCreateWithMemoryBlock called (auto-stub)"); return 0; }

long FigBlockBufferGetDataLength(long a, long b, long c_, long d, long e, long f) { shim_note("FigBlockBufferGetDataLength called (auto-stub)"); return 0; }

long FigBlockBufferGetDataPointer(long a, long b, long c_, long d, long e, long f) { shim_note("FigBlockBufferGetDataPointer called (auto-stub)"); return 0; }

long FigBlockBufferRelease(long a, long b, long c_, long d, long e, long f) { shim_note("FigBlockBufferRelease called (auto-stub)"); return 0; }

long FigBlockBufferRetain(long a, long b, long c_, long d, long e, long f) { shim_note("FigBlockBufferRetain called (auto-stub)"); return 0; }

long FigBufferQueueContainsEndOfData(long a, long b, long c_, long d, long e, long f) { shim_note("FigBufferQueueContainsEndOfData called (auto-stub)"); return 0; }

long FigBufferQueueCreate(long a, long b, long c_, long d, long e, long f) { shim_note("FigBufferQueueCreate called (auto-stub)"); return 0; }

long FigBufferQueueDequeueAndRetain(long a, long b, long c_, long d, long e, long f) { shim_note("FigBufferQueueDequeueAndRetain called (auto-stub)"); return 0; }

long FigBufferQueueDequeueIfDataReadyAndRetain(long a, long b, long c_, long d, long e, long f) { shim_note("FigBufferQueueDequeueIfDataReadyAndRetain called (auto-stub)"); return 0; }

long FigBufferQueueEnqueue(long a, long b, long c_, long d, long e, long f) { shim_note("FigBufferQueueEnqueue called (auto-stub)"); return 0; }

long FigBufferQueueGetBufferCount(long a, long b, long c_, long d, long e, long f) { shim_note("FigBufferQueueGetBufferCount called (auto-stub)"); return 0; }

long FigBufferQueueGetCallbacksForUnsortedSampleBuffers(long a, long b, long c_, long d, long e, long f) { shim_note("FigBufferQueueGetCallbacksForUnsortedSampleBuffers called (auto-stub)"); return 0; }

long FigBufferQueueGetDuration(long a, long b, long c_, long d, long e, long f) { shim_note("FigBufferQueueGetDuration called (auto-stub)"); return 0; }

long FigBufferQueueIsAtEndOfData(long a, long b, long c_, long d, long e, long f) { shim_note("FigBufferQueueIsAtEndOfData called (auto-stub)"); return 0; }

long FigBufferQueueIsEmpty(long a, long b, long c_, long d, long e, long f) { shim_note("FigBufferQueueIsEmpty called (auto-stub)"); return 0; }

long FigBufferQueueMarkEndOfData(long a, long b, long c_, long d, long e, long f) { shim_note("FigBufferQueueMarkEndOfData called (auto-stub)"); return 0; }

long FigBufferQueueRelease(long a, long b, long c_, long d, long e, long f) { shim_note("FigBufferQueueRelease called (auto-stub)"); return 0; }

long FigBufferQueueReset(long a, long b, long c_, long d, long e, long f) { shim_note("FigBufferQueueReset called (auto-stub)"); return 0; }

long FigDerivedObjectCreate(long a, long b, long c_, long d, long e, long f) { shim_note("FigDerivedObjectCreate called (auto-stub)"); return 0; }

long FigFormatDescriptionGetExtensions(long a, long b, long c_, long d, long e, long f) { shim_note("FigFormatDescriptionGetExtensions called (auto-stub)"); return 0; }

long FigFormatDescriptionGetMediaSubType(long a, long b, long c_, long d, long e, long f) { shim_note("FigFormatDescriptionGetMediaSubType called (auto-stub)"); return 0; }

long FigFormatDescriptionRelease(long a, long b, long c_, long d, long e, long f) { shim_note("FigFormatDescriptionRelease called (auto-stub)"); return 0; }

long FigFormatDescriptionRetain(long a, long b, long c_, long d, long e, long f) { shim_note("FigFormatDescriptionRetain called (auto-stub)"); return 0; }

long FigNotificationCenterCreate(long a, long b, long c_, long d, long e, long f) { shim_note("FigNotificationCenterCreate called (auto-stub)"); return 0; }

long FigRegistryAddSearchPath(long a, long b, long c_, long d, long e, long f) { shim_note("FigRegistryAddSearchPath called (auto-stub)"); return 0; }

long FigRegistryCopyItemList(long a, long b, long c_, long d, long e, long f) { shim_note("FigRegistryCopyItemList called (auto-stub)"); return 0; }

long FigRegistryCreate(long a, long b, long c_, long d, long e, long f) { shim_note("FigRegistryCreate called (auto-stub)"); return 0; }

long FigRegistryItemCopyBundle(long a, long b, long c_, long d, long e, long f) { shim_note("FigRegistryItemCopyBundle called (auto-stub)"); return 0; }

long FigRegistryItemCopyDescription(long a, long b, long c_, long d, long e, long f) { shim_note("FigRegistryItemCopyDescription called (auto-stub)"); return 0; }

long FigRegistryItemCopyMatchingInfo(long a, long b, long c_, long d, long e, long f) { shim_note("FigRegistryItemCopyMatchingInfo called (auto-stub)"); return 0; }

long FigRegistryItemGetFactory(long a, long b, long c_, long d, long e, long f) { shim_note("FigRegistryItemGetFactory called (auto-stub)"); return 0; }

long FigSampleBufferCreate(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferCreate called (auto-stub)"); return 0; }

long FigSampleBufferCreateForImageBuffer(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferCreateForImageBuffer called (auto-stub)"); return 0; }

long FigSampleBufferDataIsReady(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferDataIsReady called (auto-stub)"); return 0; }

long FigSampleBufferGetAudioStreamPacketDescriptions(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferGetAudioStreamPacketDescriptions called (auto-stub)"); return 0; }

long FigSampleBufferGetDataBuffer(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferGetDataBuffer called (auto-stub)"); return 0; }

long FigSampleBufferGetFormatDescription(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferGetFormatDescription called (auto-stub)"); return 0; }

long FigSampleBufferGetImageBuffer(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferGetImageBuffer called (auto-stub)"); return 0; }

long FigSampleBufferGetOutputDecodeTimeStamp(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferGetOutputDecodeTimeStamp called (auto-stub)"); return 0; }

long FigSampleBufferGetOutputDuration(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferGetOutputDuration called (auto-stub)"); return 0; }

long FigSampleBufferGetOutputPresentationTimeStamp(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferGetOutputPresentationTimeStamp called (auto-stub)"); return 0; }

long FigSampleBufferGetPresentationTimeStamp(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferGetPresentationTimeStamp called (auto-stub)"); return 0; }

long FigSampleBufferGetSampleAttachmentsArray(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferGetSampleAttachmentsArray called (auto-stub)"); return 0; }

long FigSampleBufferRelease(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferRelease called (auto-stub)"); return 0; }

long FigSampleBufferRetain(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferRetain called (auto-stub)"); return 0; }

long FigSampleBufferSetDataBufferFromAudioBufferList(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferSetDataBufferFromAudioBufferList called (auto-stub)"); return 0; }

long FigThreadRunOnce(long a, long b, long c_, long d, long e, long f) { shim_note("FigThreadRunOnce called (auto-stub)"); return 0; }

long FigTimeAdd(long a, long b, long c_, long d, long e, long f) { shim_note("FigTimeAdd called (auto-stub)"); return 0; }

long FigTimeCompare(long a, long b, long c_, long d, long e, long f) { shim_note("FigTimeCompare called (auto-stub)"); return 0; }

long FigTimeConvertScale(long a, long b, long c_, long d, long e, long f) { shim_note("FigTimeConvertScale called (auto-stub)"); return 0; }

long FigTimeCopyAsDictionary(long a, long b, long c_, long d, long e, long f) { shim_note("FigTimeCopyAsDictionary called (auto-stub)"); return 0; }

long FigTimeGetSeconds(long a, long b, long c_, long d, long e, long f) { shim_note("FigTimeGetSeconds called (auto-stub)"); return 0; }

long FigTimeMake(long a, long b, long c_, long d, long e, long f) { shim_note("FigTimeMake called (auto-stub)"); return 0; }

long FigTimeMakeFromDictionary(long a, long b, long c_, long d, long e, long f) { shim_note("FigTimeMakeFromDictionary called (auto-stub)"); return 0; }

long FigTimeMultiplyByFloat64(long a, long b, long c_, long d, long e, long f) { shim_note("FigTimeMultiplyByFloat64 called (auto-stub)"); return 0; }

long FigTimeSubtract(long a, long b, long c_, long d, long e, long f) { shim_note("FigTimeSubtract called (auto-stub)"); return 0; }

long FigVideoFormatDescriptionCreateForImageBuffer(long a, long b, long c_, long d, long e, long f) { shim_note("FigVideoFormatDescriptionCreateForImageBuffer called (auto-stub)"); return 0; }

long FigVideoFormatDescriptionGetDimensions(long a, long b, long c_, long d, long e, long f) { shim_note("FigVideoFormatDescriptionGetDimensions called (auto-stub)"); return 0; }

long FigVideoFormatDescriptionGetExtensionKeysCommonWithImageBuffers(long a, long b, long c_, long d, long e, long f) { shim_note("FigVideoFormatDescriptionGetExtensionKeysCommonWithImageBuffers called (auto-stub)"); return 0; }

long fig_logP(long a, long b, long c_, long d, long e, long f) { shim_note("fig_logP called (auto-stub)"); return 0; }

long fig_log_CF1P(long a, long b, long c_, long d, long e, long f) { shim_note("fig_log_CF1P called (auto-stub)"); return 0; }
