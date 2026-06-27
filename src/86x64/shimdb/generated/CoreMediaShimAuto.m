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
  fprintf(stderr, "[shimauto:%s] %s: removed-OS symbol CALLED with no curated impl -- returning 0 (write one in shimdb/impl/ if its behavior matters)\n", "CoreMedia", sym);
  if (getenv("SHIMGEN_STUB_ABORT")) abort();
  pthread_mutex_unlock(&mtx);
}

long FigAudioFormatDescriptionCreate(long a, long b, long c_, long d, long e, long f) { shim_note("FigAudioFormatDescriptionCreate"); return 0; }

long FigAudioFormatDescriptionGetChannelLayout(long a, long b, long c_, long d, long e, long f) { shim_note("FigAudioFormatDescriptionGetChannelLayout"); return 0; }

long FigAudioFormatDescriptionGetFormatList(long a, long b, long c_, long d, long e, long f) { shim_note("FigAudioFormatDescriptionGetFormatList"); return 0; }

long FigAudioFormatDescriptionGetMagicCookie(long a, long b, long c_, long d, long e, long f) { shim_note("FigAudioFormatDescriptionGetMagicCookie"); return 0; }

long FigAudioFormatDescriptionGetStreamBasicDescription(long a, long b, long c_, long d, long e, long f) { shim_note("FigAudioFormatDescriptionGetStreamBasicDescription"); return 0; }

long FigBlockBufferCopyDataBytes(long a, long b, long c_, long d, long e, long f) { shim_note("FigBlockBufferCopyDataBytes"); return 0; }

long FigBlockBufferCreateWithMemoryBlock(long a, long b, long c_, long d, long e, long f) { shim_note("FigBlockBufferCreateWithMemoryBlock"); return 0; }

long FigBlockBufferGetDataLength(long a, long b, long c_, long d, long e, long f) { shim_note("FigBlockBufferGetDataLength"); return 0; }

long FigBlockBufferGetDataPointer(long a, long b, long c_, long d, long e, long f) { shim_note("FigBlockBufferGetDataPointer"); return 0; }

long FigBlockBufferIsRangeContiguous(long a, long b, long c_, long d, long e, long f) { shim_note("FigBlockBufferIsRangeContiguous"); return 0; }

long FigBlockBufferToSimpleBBufCopy(long a, long b, long c_, long d, long e, long f) { shim_note("FigBlockBufferToSimpleBBufCopy"); return 0; }

long FigBufferQueueCreate(long a, long b, long c_, long d, long e, long f) { shim_note("FigBufferQueueCreate"); return 0; }

long FigBufferQueueDequeueAndRetain(long a, long b, long c_, long d, long e, long f) { shim_note("FigBufferQueueDequeueAndRetain"); return 0; }

long FigBufferQueueEnqueue(long a, long b, long c_, long d, long e, long f) { shim_note("FigBufferQueueEnqueue"); return 0; }

long FigBufferQueueGetBufferCount(long a, long b, long c_, long d, long e, long f) { shim_note("FigBufferQueueGetBufferCount"); return 0; }

long FigBufferQueueGetCallbacksForUnsortedSampleBuffers(long a, long b, long c_, long d, long e, long f) { shim_note("FigBufferQueueGetCallbacksForUnsortedSampleBuffers"); return 0; }

long FigBufferQueueGetHead(long a, long b, long c_, long d, long e, long f) { shim_note("FigBufferQueueGetHead"); return 0; }

long FigBufferQueueInstallTriggerWithIntegerThreshold(long a, long b, long c_, long d, long e, long f) { shim_note("FigBufferQueueInstallTriggerWithIntegerThreshold"); return 0; }

long FigBufferQueueIsAtEndOfData(long a, long b, long c_, long d, long e, long f) { shim_note("FigBufferQueueIsAtEndOfData"); return 0; }

long FigBufferQueueIsEmpty(long a, long b, long c_, long d, long e, long f) { shim_note("FigBufferQueueIsEmpty"); return 0; }

long FigBufferQueueMarkEndOfData(long a, long b, long c_, long d, long e, long f) { shim_note("FigBufferQueueMarkEndOfData"); return 0; }

long FigByteStreamBaseGetTypeID(long a, long b, long c_, long d, long e, long f) { shim_note("FigByteStreamBaseGetTypeID"); return 0; }

long FigByteStreamGetFigBaseObject(long a, long b, long c_, long d, long e, long f) { shim_note("FigByteStreamGetFigBaseObject"); return 0; }

long FigCopyDictionaryOfAttachments(long a, long b, long c_, long d, long e, long f) { shim_note("FigCopyDictionaryOfAttachments"); return 0; }

long FigFormatDescriptionCreate(long a, long b, long c_, long d, long e, long f) { shim_note("FigFormatDescriptionCreate"); return 0; }

long FigFormatDescriptionGetExtension(long a, long b, long c_, long d, long e, long f) { shim_note("FigFormatDescriptionGetExtension"); return 0; }

long FigFormatDescriptionGetExtensions(long a, long b, long c_, long d, long e, long f) { shim_note("FigFormatDescriptionGetExtensions"); return 0; }

long FigFormatDescriptionGetMediaSubType(long a, long b, long c_, long d, long e, long f) { shim_note("FigFormatDescriptionGetMediaSubType"); return 0; }

long FigFormatDescriptionGetMediaType(long a, long b, long c_, long d, long e, long f) { shim_note("FigFormatDescriptionGetMediaType"); return 0; }

long FigGetAttachment(long a, long b, long c_, long d, long e, long f) { shim_note("FigGetAttachment"); return 0; }

long FigNotificationCenterAddListener(long a, long b, long c_, long d, long e, long f) { shim_note("FigNotificationCenterAddListener"); return 0; }

long FigNotificationCenterPostNotification(long a, long b, long c_, long d, long e, long f) { shim_note("FigNotificationCenterPostNotification"); return 0; }

long FigNotificationCenterRemoveListener(long a, long b, long c_, long d, long e, long f) { shim_note("FigNotificationCenterRemoveListener"); return 0; }

long FigRemoveAttachment(long a, long b, long c_, long d, long e, long f) { shim_note("FigRemoveAttachment"); return 0; }

long FigSampleBufferCreate(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferCreate"); return 0; }

long FigSampleBufferCreateCopyWithNewTiming(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferCreateCopyWithNewTiming"); return 0; }

long FigSampleBufferCreateForImageBuffer(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferCreateForImageBuffer"); return 0; }

long FigSampleBufferGetAudioBufferListWithRetainedBlockBuffer(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferGetAudioBufferListWithRetainedBlockBuffer"); return 0; }

long FigSampleBufferGetAudioStreamPacketDescriptionsPtr(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferGetAudioStreamPacketDescriptionsPtr"); return 0; }

long FigSampleBufferGetDataBuffer(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferGetDataBuffer"); return 0; }

long FigSampleBufferGetDuration(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferGetDuration"); return 0; }

long FigSampleBufferGetFormatDescription(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferGetFormatDescription"); return 0; }

long FigSampleBufferGetImageBuffer(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferGetImageBuffer"); return 0; }

long FigSampleBufferGetNumSamples(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferGetNumSamples"); return 0; }

long FigSampleBufferGetOutputDecodeTimeStamp(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferGetOutputDecodeTimeStamp"); return 0; }

long FigSampleBufferGetOutputDuration(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferGetOutputDuration"); return 0; }

long FigSampleBufferGetOutputPresentationTimeStamp(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferGetOutputPresentationTimeStamp"); return 0; }

long FigSampleBufferGetSampleAttachmentsArray(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferGetSampleAttachmentsArray"); return 0; }

long FigSampleBufferGetSampleSizeArray(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferGetSampleSizeArray"); return 0; }

long FigSampleBufferGetSampleTimingInfoArray(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferGetSampleTimingInfoArray"); return 0; }

long FigSampleBufferSetOutputPresentationTimeStamp(long a, long b, long c_, long d, long e, long f) { shim_note("FigSampleBufferSetOutputPresentationTimeStamp"); return 0; }

long FigSetAttachment(long a, long b, long c_, long d, long e, long f) { shim_note("FigSetAttachment"); return 0; }

long FigSetAttachments(long a, long b, long c_, long d, long e, long f) { shim_note("FigSetAttachments"); return 0; }

long FigTimeAdd(long a, long b, long c_, long d, long e, long f) { shim_note("FigTimeAdd"); return 0; }

long FigTimeCompare(long a, long b, long c_, long d, long e, long f) { shim_note("FigTimeCompare"); return 0; }

long FigTimeConvertScale(long a, long b, long c_, long d, long e, long f) { shim_note("FigTimeConvertScale"); return 0; }

long FigTimeCopyAsDictionary(long a, long b, long c_, long d, long e, long f) { shim_note("FigTimeCopyAsDictionary"); return 0; }

long FigTimeGetSeconds(long a, long b, long c_, long d, long e, long f) { shim_note("FigTimeGetSeconds"); return 0; }

long FigTimeMake(long a, long b, long c_, long d, long e, long f) { shim_note("FigTimeMake"); return 0; }

long FigTimeMakeFromDictionary(long a, long b, long c_, long d, long e, long f) { shim_note("FigTimeMakeFromDictionary"); return 0; }

long FigTimeMultiplyByFloat64(long a, long b, long c_, long d, long e, long f) { shim_note("FigTimeMultiplyByFloat64"); return 0; }

long FigTimeSubtract(long a, long b, long c_, long d, long e, long f) { shim_note("FigTimeSubtract"); return 0; }

long FigVideoFormatDescriptionCreate(long a, long b, long c_, long d, long e, long f) { shim_note("FigVideoFormatDescriptionCreate"); return 0; }

long FigVideoFormatDescriptionGetExtensionKeysCommonWithImageBuffers(long a, long b, long c_, long d, long e, long f) { shim_note("FigVideoFormatDescriptionGetExtensionKeysCommonWithImageBuffers"); return 0; }

long FigVideoFormatDescriptionGetPresentationDimensions(long a, long b, long c_, long d, long e, long f) { shim_note("FigVideoFormatDescriptionGetPresentationDimensions"); return 0; }

long kFigByteStreamProperty_URL(long a, long b, long c_, long d, long e, long f) { shim_note("kFigByteStreamProperty_URL"); return 0; }

long kFigFormatDescriptionExtension_BytesPerRow(long a, long b, long c_, long d, long e, long f) { shim_note("kFigFormatDescriptionExtension_BytesPerRow"); return 0; }

long kFigFormatDescriptionExtension_SampleDescriptionExtensionAtoms(long a, long b, long c_, long d, long e, long f) { shim_note("kFigFormatDescriptionExtension_SampleDescriptionExtensionAtoms"); return 0; }

long kFigFormatDescriptionExtension_VerbatimSampleDescription(long a, long b, long c_, long d, long e, long f) { shim_note("kFigFormatDescriptionExtension_VerbatimSampleDescription"); return 0; }

long kFigFormatDescriptionTransferFunction_SMPTE_240M_1995(long a, long b, long c_, long d, long e, long f) { shim_note("kFigFormatDescriptionTransferFunction_SMPTE_240M_1995"); return 0; }

long kFigFormatDescriptionTransferFunction_UseGamma(long a, long b, long c_, long d, long e, long f) { shim_note("kFigFormatDescriptionTransferFunction_UseGamma"); return 0; }

long kFigSampleAttachmentKey_DoNotDisplay(long a, long b, long c_, long d, long e, long f) { shim_note("kFigSampleAttachmentKey_DoNotDisplay"); return 0; }

long kFigSampleAttachmentKey_IsDependedOnByOthers(long a, long b, long c_, long d, long e, long f) { shim_note("kFigSampleAttachmentKey_IsDependedOnByOthers"); return 0; }

long kFigSampleAttachmentKey_NotSync(long a, long b, long c_, long d, long e, long f) { shim_note("kFigSampleAttachmentKey_NotSync"); return 0; }

long kFigSampleAttachmentKey_PartialSync(long a, long b, long c_, long d, long e, long f) { shim_note("kFigSampleAttachmentKey_PartialSync"); return 0; }

long kFigSampleBufferAttachmentKey_SpeedMultiplier(long a, long b, long c_, long d, long e, long f) { shim_note("kFigSampleBufferAttachmentKey_SpeedMultiplier"); return 0; }

long kFigSampleBufferAttachmentKey_TrimDurationAtEnd(long a, long b, long c_, long d, long e, long f) { shim_note("kFigSampleBufferAttachmentKey_TrimDurationAtEnd"); return 0; }

long kFigSampleBufferAttachmentKey_TrimDurationAtStart(long a, long b, long c_, long d, long e, long f) { shim_note("kFigSampleBufferAttachmentKey_TrimDurationAtStart"); return 0; }

long kFigTimeInvalid(long a, long b, long c_, long d, long e, long f) { shim_note("kFigTimeInvalid"); return 0; }

long kFigTimePositiveInfinity(long a, long b, long c_, long d, long e, long f) { shim_note("kFigTimePositiveInfinity"); return 0; }

long kFigTimeZero(long a, long b, long c_, long d, long e, long f) { shim_note("kFigTimeZero"); return 0; }
