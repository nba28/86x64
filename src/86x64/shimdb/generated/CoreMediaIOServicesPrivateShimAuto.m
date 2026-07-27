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
  fprintf(stderr, "[shimauto:%s] %s: removed-OS symbol CALLED with no curated impl -- returning 0 (write one in shimdb/impl/ if its behavior matters)\n", "CoreMediaIOServicesPrivate", sym);
  if (getenv("SHIMGEN_STUB_ABORT")) abort();
  pthread_mutex_unlock(&mtx);
}

long TundraDeviceProcessAVCCommand(long a, long b, long c_, long d, long e, long f) __asm("_TundraDeviceProcessAVCCommand");
long TundraDeviceProcessAVCCommand(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraDeviceProcessAVCCommand"); return 0; }

long TundraFileWritingControlTokenAllVolumesAreOutOfDiskSpace(long a, long b, long c_, long d, long e, long f) __asm("_TundraFileWritingControlTokenAllVolumesAreOutOfDiskSpace");
long TundraFileWritingControlTokenAllVolumesAreOutOfDiskSpace(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraFileWritingControlTokenAllVolumesAreOutOfDiskSpace"); return 0; }

long TundraFileWritingControlTokenCurrentVolumeIsOutOfDiskSpace(long a, long b, long c_, long d, long e, long f) __asm("_TundraFileWritingControlTokenCurrentVolumeIsOutOfDiskSpace");
long TundraFileWritingControlTokenCurrentVolumeIsOutOfDiskSpace(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraFileWritingControlTokenCurrentVolumeIsOutOfDiskSpace"); return 0; }

long TundraFileWritingControlTokenGetDiscontinuityFlags(long a, long b, long c_, long d, long e, long f) __asm("_TundraFileWritingControlTokenGetDiscontinuityFlags");
long TundraFileWritingControlTokenGetDiscontinuityFlags(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraFileWritingControlTokenGetDiscontinuityFlags"); return 0; }

long TundraFileWritingControlTokenGetSMPTETime(long a, long b, long c_, long d, long e, long f) __asm("_TundraFileWritingControlTokenGetSMPTETime");
long TundraFileWritingControlTokenGetSMPTETime(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraFileWritingControlTokenGetSMPTETime"); return 0; }

long TundraFileWritingControlTokenGetSampleBuffer(long a, long b, long c_, long d, long e, long f) __asm("_TundraFileWritingControlTokenGetSampleBuffer");
long TundraFileWritingControlTokenGetSampleBuffer(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraFileWritingControlTokenGetSampleBuffer"); return 0; }

long TundraFileWritingControlTokenMaximumFileSizeHasBeenReached(long a, long b, long c_, long d, long e, long f) __asm("_TundraFileWritingControlTokenMaximumFileSizeHasBeenReached");
long TundraFileWritingControlTokenMaximumFileSizeHasBeenReached(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraFileWritingControlTokenMaximumFileSizeHasBeenReached"); return 0; }

long TundraFileWritingControlTokenMaximumRecordDurationHasBeenReached(long a, long b, long c_, long d, long e, long f) __asm("_TundraFileWritingControlTokenMaximumRecordDurationHasBeenReached");
long TundraFileWritingControlTokenMaximumRecordDurationHasBeenReached(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraFileWritingControlTokenMaximumRecordDurationHasBeenReached"); return 0; }

long TundraFileWritingControlTokenStartWriting(long a, long b, long c_, long d, long e, long f) __asm("_TundraFileWritingControlTokenStartWriting");
long TundraFileWritingControlTokenStartWriting(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraFileWritingControlTokenStartWriting"); return 0; }

long TundraFileWritingControlTokenStopWriting(long a, long b, long c_, long d, long e, long f) __asm("_TundraFileWritingControlTokenStopWriting");
long TundraFileWritingControlTokenStopWriting(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraFileWritingControlTokenStopWriting"); return 0; }

long TundraFileWritingControlTokenUnitCanStartWriting(long a, long b, long c_, long d, long e, long f) __asm("_TundraFileWritingControlTokenUnitCanStartWriting");
long TundraFileWritingControlTokenUnitCanStartWriting(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraFileWritingControlTokenUnitCanStartWriting"); return 0; }

long TundraFileWritingControlTokenUnitIsWriting(long a, long b, long c_, long d, long e, long f) __asm("_TundraFileWritingControlTokenUnitIsWriting");
long TundraFileWritingControlTokenUnitIsWriting(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraFileWritingControlTokenUnitIsWriting"); return 0; }

long TundraGraphAddPropertyListener(long a, long b, long c_, long d, long e, long f) __asm("_TundraGraphAddPropertyListener");
long TundraGraphAddPropertyListener(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraGraphAddPropertyListener"); return 0; }

long TundraGraphConnectNodeInput(long a, long b, long c_, long d, long e, long f) __asm("_TundraGraphConnectNodeInput");
long TundraGraphConnectNodeInput(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraGraphConnectNodeInput"); return 0; }

long TundraGraphCountNodeConnections(long a, long b, long c_, long d, long e, long f) __asm("_TundraGraphCountNodeConnections");
long TundraGraphCountNodeConnections(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraGraphCountNodeConnections"); return 0; }

long TundraGraphCreate(long a, long b, long c_, long d, long e, long f) __asm("_TundraGraphCreate");
long TundraGraphCreate(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraGraphCreate"); return 0; }

long TundraGraphCreateAndConfigureForUnits(long a, long b, long c_, long d, long e, long f) __asm("_TundraGraphCreateAndConfigureForUnits");
long TundraGraphCreateAndConfigureForUnits(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraGraphCreateAndConfigureForUnits"); return 0; }

long TundraGraphCreateNode(long a, long b, long c_, long d, long e, long f) __asm("_TundraGraphCreateNode");
long TundraGraphCreateNode(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraGraphCreateNode"); return 0; }

long TundraGraphDisconnectNodeInput(long a, long b, long c_, long d, long e, long f) __asm("_TundraGraphDisconnectNodeInput");
long TundraGraphDisconnectNodeInput(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraGraphDisconnectNodeInput"); return 0; }

long TundraGraphGetNodeByFunctionalDesignationAndIndex(long a, long b, long c_, long d, long e, long f) __asm("_TundraGraphGetNodeByFunctionalDesignationAndIndex");
long TundraGraphGetNodeByFunctionalDesignationAndIndex(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraGraphGetNodeByFunctionalDesignationAndIndex"); return 0; }

long TundraGraphGetNodeConnections(long a, long b, long c_, long d, long e, long f) __asm("_TundraGraphGetNodeConnections");
long TundraGraphGetNodeConnections(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraGraphGetNodeConnections"); return 0; }

long TundraGraphGetNodeInfo(long a, long b, long c_, long d, long e, long f) __asm("_TundraGraphGetNodeInfo");
long TundraGraphGetNodeInfo(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraGraphGetNodeInfo"); return 0; }

long TundraGraphGetProperty(long a, long b, long c_, long d, long e, long f) __asm("_TundraGraphGetProperty");
long TundraGraphGetProperty(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraGraphGetProperty"); return 0; }

long TundraGraphGetPropertyInfo(long a, long b, long c_, long d, long e, long f) __asm("_TundraGraphGetPropertyInfo");
long TundraGraphGetPropertyInfo(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraGraphGetPropertyInfo"); return 0; }

long TundraGraphInitialize(long a, long b, long c_, long d, long e, long f) __asm("_TundraGraphInitialize");
long TundraGraphInitialize(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraGraphInitialize"); return 0; }

long TundraGraphPause(long a, long b, long c_, long d, long e, long f) __asm("_TundraGraphPause");
long TundraGraphPause(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraGraphPause"); return 0; }

long TundraGraphRelease(long a, long b, long c_, long d, long e, long f) __asm("_TundraGraphRelease");
long TundraGraphRelease(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraGraphRelease"); return 0; }

long TundraGraphRemoveNode(long a, long b, long c_, long d, long e, long f) __asm("_TundraGraphRemoveNode");
long TundraGraphRemoveNode(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraGraphRemoveNode"); return 0; }

long TundraGraphRemovePropertyListener(long a, long b, long c_, long d, long e, long f) __asm("_TundraGraphRemovePropertyListener");
long TundraGraphRemovePropertyListener(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraGraphRemovePropertyListener"); return 0; }

long TundraGraphResume(long a, long b, long c_, long d, long e, long f) __asm("_TundraGraphResume");
long TundraGraphResume(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraGraphResume"); return 0; }

long TundraGraphSetProperty(long a, long b, long c_, long d, long e, long f) __asm("_TundraGraphSetProperty");
long TundraGraphSetProperty(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraGraphSetProperty"); return 0; }

long TundraGraphStart(long a, long b, long c_, long d, long e, long f) __asm("_TundraGraphStart");
long TundraGraphStart(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraGraphStart"); return 0; }

long TundraGraphStop(long a, long b, long c_, long d, long e, long f) __asm("_TundraGraphStop");
long TundraGraphStop(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraGraphStop"); return 0; }

long TundraGraphUninitialize(long a, long b, long c_, long d, long e, long f) __asm("_TundraGraphUninitialize");
long TundraGraphUninitialize(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraGraphUninitialize"); return 0; }

long TundraObjectAddPropertyListener(long a, long b, long c_, long d, long e, long f) __asm("_TundraObjectAddPropertyListener");
long TundraObjectAddPropertyListener(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraObjectAddPropertyListener"); return 0; }

long TundraObjectGetPropertyData(long a, long b, long c_, long d, long e, long f) __asm("_TundraObjectGetPropertyData");
long TundraObjectGetPropertyData(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraObjectGetPropertyData"); return 0; }

long TundraObjectGetPropertyDataSize(long a, long b, long c_, long d, long e, long f) __asm("_TundraObjectGetPropertyDataSize");
long TundraObjectGetPropertyDataSize(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraObjectGetPropertyDataSize"); return 0; }

long TundraObjectIsPropertySettable(long a, long b, long c_, long d, long e, long f) __asm("_TundraObjectIsPropertySettable");
long TundraObjectIsPropertySettable(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraObjectIsPropertySettable"); return 0; }

long TundraObjectRemovePropertyListener(long a, long b, long c_, long d, long e, long f) __asm("_TundraObjectRemovePropertyListener");
long TundraObjectRemovePropertyListener(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraObjectRemovePropertyListener"); return 0; }

long TundraObjectSetPropertyData(long a, long b, long c_, long d, long e, long f) __asm("_TundraObjectSetPropertyData");
long TundraObjectSetPropertyData(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraObjectSetPropertyData"); return 0; }

long TundraUnitAddRenderNotify(long a, long b, long c_, long d, long e, long f) __asm("_TundraUnitAddRenderNotify");
long TundraUnitAddRenderNotify(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraUnitAddRenderNotify"); return 0; }

long TundraUnitRelease(long a, long b, long c_, long d, long e, long f) __asm("_TundraUnitRelease");
long TundraUnitRelease(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraUnitRelease"); return 0; }

long TundraUnitRetain(long a, long b, long c_, long d, long e, long f) __asm("_TundraUnitRetain");
long TundraUnitRetain(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraUnitRetain"); return 0; }

long TundraUnitUtilityCreateAudioCompressionOptionsDictionary(long a, long b, long c_, long d, long e, long f) __asm("_TundraUnitUtilityCreateAudioCompressionOptionsDictionary");
long TundraUnitUtilityCreateAudioCompressionOptionsDictionary(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraUnitUtilityCreateAudioCompressionOptionsDictionary"); return 0; }

long TundraUnitUtilityGetAudioCompressionOptionsDictionaryFields(long a, long b, long c_, long d, long e, long f) __asm("_TundraUnitUtilityGetAudioCompressionOptionsDictionaryFields");
long TundraUnitUtilityGetAudioCompressionOptionsDictionaryFields(long a, long b, long c_, long d, long e, long f) { shim_note("_TundraUnitUtilityGetAudioCompressionOptionsDictionaryFields"); return 0; }
