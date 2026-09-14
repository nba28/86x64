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
  fprintf(stderr, "[shimauto:%s] %s: removed-OS symbol CALLED with no curated impl -- returning 0 (write one in shimdb/impl/ if its behavior matters)\n", "QuickTime", sym);
  if (getenv("SHIMGEN_STUB_ABORT")) abort();
  pthread_mutex_unlock(&mtx);
}

long AbortPrePrerollMovie(long a, long b, long c_, long d, long e, long f) __asm("_AbortPrePrerollMovie");
long AbortPrePrerollMovie(long a, long b, long c_, long d, long e, long f) { shim_note("_AbortPrePrerollMovie"); return 0; }

long AddClonedTrackToMovie(long a, long b, long c_, long d, long e, long f) __asm("_AddClonedTrackToMovie");
long AddClonedTrackToMovie(long a, long b, long c_, long d, long e, long f) { shim_note("_AddClonedTrackToMovie"); return 0; }

long AddEmptyTrackToMovie(long a, long b, long c_, long d, long e, long f) __asm("_AddEmptyTrackToMovie");
long AddEmptyTrackToMovie(long a, long b, long c_, long d, long e, long f) { shim_note("_AddEmptyTrackToMovie"); return 0; }

long AddFilePreview(long a, long b, long c_, long d, long e, long f) __asm("_AddFilePreview");
long AddFilePreview(long a, long b, long c_, long d, long e, long f) { shim_note("_AddFilePreview"); return 0; }

long AddImageDescriptionExtension(long a, long b, long c_, long d, long e, long f) __asm("_AddImageDescriptionExtension");
long AddImageDescriptionExtension(long a, long b, long c_, long d, long e, long f) { shim_note("_AddImageDescriptionExtension"); return 0; }

long AddMediaDataRef(long a, long b, long c_, long d, long e, long f) __asm("_AddMediaDataRef");
long AddMediaDataRef(long a, long b, long c_, long d, long e, long f) { shim_note("_AddMediaDataRef"); return 0; }

long AddMediaSample(long a, long b, long c_, long d, long e, long f) __asm("_AddMediaSample");
long AddMediaSample(long a, long b, long c_, long d, long e, long f) { shim_note("_AddMediaSample"); return 0; }

long AddMediaSampleReference(long a, long b, long c_, long d, long e, long f) __asm("_AddMediaSampleReference");
long AddMediaSampleReference(long a, long b, long c_, long d, long e, long f) { shim_note("_AddMediaSampleReference"); return 0; }

long AddMovieResource(long a, long b, long c_, long d, long e, long f) __asm("_AddMovieResource");
long AddMovieResource(long a, long b, long c_, long d, long e, long f) { shim_note("_AddMovieResource"); return 0; }

long AddMovieSelection(long a, long b, long c_, long d, long e, long f) __asm("_AddMovieSelection");
long AddMovieSelection(long a, long b, long c_, long d, long e, long f) { shim_note("_AddMovieSelection"); return 0; }

long AddMovieToStorage(long a, long b, long c_, long d, long e, long f) __asm("_AddMovieToStorage");
long AddMovieToStorage(long a, long b, long c_, long d, long e, long f) { shim_note("_AddMovieToStorage"); return 0; }

long AddSoundDescriptionExtension(long a, long b, long c_, long d, long e, long f) __asm("_AddSoundDescriptionExtension");
long AddSoundDescriptionExtension(long a, long b, long c_, long d, long e, long f) { shim_note("_AddSoundDescriptionExtension"); return 0; }

long AddTime(long a, long b, long c_, long d, long e, long f) __asm("_AddTime");
long AddTime(long a, long b, long c_, long d, long e, long f) { shim_note("_AddTime"); return 0; }

long AddTrackReference(long a, long b, long c_, long d, long e, long f) __asm("_AddTrackReference");
long AddTrackReference(long a, long b, long c_, long d, long e, long f) { shim_note("_AddTrackReference"); return 0; }

long AddUserData(long a, long b, long c_, long d, long e, long f) __asm("_AddUserData");
long AddUserData(long a, long b, long c_, long d, long e, long f) { shim_note("_AddUserData"); return 0; }

long AddUserDataText(long a, long b, long c_, long d, long e, long f) __asm("_AddUserDataText");
long AddUserDataText(long a, long b, long c_, long d, long e, long f) { shim_note("_AddUserDataText"); return 0; }

long AlignWindow(long a, long b, long c_, long d, long e, long f) __asm("_AlignWindow");
long AlignWindow(long a, long b, long c_, long d, long e, long f) { shim_note("_AlignWindow"); return 0; }

long BeginMediaEdits(long a, long b, long c_, long d, long e, long f) __asm("_BeginMediaEdits");
long BeginMediaEdits(long a, long b, long c_, long d, long e, long f) { shim_note("_BeginMediaEdits"); return 0; }

long CanQuickTimeOpenDataRef(long a, long b, long c_, long d, long e, long f) __asm("_CanQuickTimeOpenDataRef");
long CanQuickTimeOpenDataRef(long a, long b, long c_, long d, long e, long f) { shim_note("_CanQuickTimeOpenDataRef"); return 0; }

long CanQuickTimeOpenFile(long a, long b, long c_, long d, long e, long f) __asm("_CanQuickTimeOpenFile");
long CanQuickTimeOpenFile(long a, long b, long c_, long d, long e, long f) { shim_note("_CanQuickTimeOpenFile"); return 0; }

long ChooseMovieClock(long a, long b, long c_, long d, long e, long f) __asm("_ChooseMovieClock");
long ChooseMovieClock(long a, long b, long c_, long d, long e, long f) { shim_note("_ChooseMovieClock"); return 0; }

long ClearMovieChanged(long a, long b, long c_, long d, long e, long f) __asm("_ClearMovieChanged");
long ClearMovieChanged(long a, long b, long c_, long d, long e, long f) { shim_note("_ClearMovieChanged"); return 0; }

long ClearMovieSelection(long a, long b, long c_, long d, long e, long f) __asm("_ClearMovieSelection");
long ClearMovieSelection(long a, long b, long c_, long d, long e, long f) { shim_note("_ClearMovieSelection"); return 0; }

long ClearMoviesStickyError(long a, long b, long c_, long d, long e, long f) __asm("_ClearMoviesStickyError");
long ClearMoviesStickyError(long a, long b, long c_, long d, long e, long f) { shim_note("_ClearMoviesStickyError"); return 0; }

long ClockGetRate(long a, long b, long c_, long d, long e, long f) __asm("_ClockGetRate");
long ClockGetRate(long a, long b, long c_, long d, long e, long f) { shim_note("_ClockGetRate"); return 0; }

long ClockGetTime(long a, long b, long c_, long d, long e, long f) __asm("_ClockGetTime");
long ClockGetTime(long a, long b, long c_, long d, long e, long f) { shim_note("_ClockGetTime"); return 0; }

long ClockSetTimeBase(long a, long b, long c_, long d, long e, long f) __asm("_ClockSetTimeBase");
long ClockSetTimeBase(long a, long b, long c_, long d, long e, long f) { shim_note("_ClockSetTimeBase"); return 0; }

long CloseMovieFile(long a, long b, long c_, long d, long e, long f) __asm("_CloseMovieFile");
long CloseMovieFile(long a, long b, long c_, long d, long e, long f) { shim_note("_CloseMovieFile"); return 0; }

long CloseMovieStorage(long a, long b, long c_, long d, long e, long f) __asm("_CloseMovieStorage");
long CloseMovieStorage(long a, long b, long c_, long d, long e, long f) { shim_note("_CloseMovieStorage"); return 0; }

long CodecManagerVersion(long a, long b, long c_, long d, long e, long f) __asm("_CodecManagerVersion");
long CodecManagerVersion(long a, long b, long c_, long d, long e, long f) { shim_note("_CodecManagerVersion"); return 0; }

long CompAdd(long a, long b, long c_, long d, long e, long f) __asm("_CompAdd");
long CompAdd(long a, long b, long c_, long d, long e, long f) { shim_note("_CompAdd"); return 0; }

long CompCompare(long a, long b, long c_, long d, long e, long f) __asm("_CompCompare");
long CompCompare(long a, long b, long c_, long d, long e, long f) { shim_note("_CompCompare"); return 0; }

long CompDiv(long a, long b, long c_, long d, long e, long f) __asm("_CompDiv");
long CompDiv(long a, long b, long c_, long d, long e, long f) { shim_note("_CompDiv"); return 0; }

long CompFixMul(long a, long b, long c_, long d, long e, long f) __asm("_CompFixMul");
long CompFixMul(long a, long b, long c_, long d, long e, long f) { shim_note("_CompFixMul"); return 0; }

long CompMul(long a, long b, long c_, long d, long e, long f) __asm("_CompMul");
long CompMul(long a, long b, long c_, long d, long e, long f) { shim_note("_CompMul"); return 0; }

long CompMulDiv(long a, long b, long c_, long d, long e, long f) __asm("_CompMulDiv");
long CompMulDiv(long a, long b, long c_, long d, long e, long f) { shim_note("_CompMulDiv"); return 0; }

long CompMulDivTrunc(long a, long b, long c_, long d, long e, long f) __asm("_CompMulDivTrunc");
long CompMulDivTrunc(long a, long b, long c_, long d, long e, long f) { shim_note("_CompMulDivTrunc"); return 0; }

long CompNeg(long a, long b, long c_, long d, long e, long f) __asm("_CompNeg");
long CompNeg(long a, long b, long c_, long d, long e, long f) { shim_note("_CompNeg"); return 0; }

long CompShift(long a, long b, long c_, long d, long e, long f) __asm("_CompShift");
long CompShift(long a, long b, long c_, long d, long e, long f) { shim_note("_CompShift"); return 0; }

long CompSquareRoot(long a, long b, long c_, long d, long e, long f) __asm("_CompSquareRoot");
long CompSquareRoot(long a, long b, long c_, long d, long e, long f) { shim_note("_CompSquareRoot"); return 0; }

long CompSub(long a, long b, long c_, long d, long e, long f) __asm("_CompSub");
long CompSub(long a, long b, long c_, long d, long e, long f) { shim_note("_CompSub"); return 0; }

long CompressImage(long a, long b, long c_, long d, long e, long f) __asm("_CompressImage");
long CompressImage(long a, long b, long c_, long d, long e, long f) { shim_note("_CompressImage"); return 0; }

long CompressPicture(long a, long b, long c_, long d, long e, long f) __asm("_CompressPicture");
long CompressPicture(long a, long b, long c_, long d, long e, long f) { shim_note("_CompressPicture"); return 0; }

long CompressPictureFile(long a, long b, long c_, long d, long e, long f) __asm("_CompressPictureFile");
long CompressPictureFile(long a, long b, long c_, long d, long e, long f) { shim_note("_CompressPictureFile"); return 0; }

long ConvertImage(long a, long b, long c_, long d, long e, long f) __asm("_ConvertImage");
long ConvertImage(long a, long b, long c_, long d, long e, long f) { shim_note("_ConvertImage"); return 0; }

long ConvertMovieToFile(long a, long b, long c_, long d, long e, long f) __asm("_ConvertMovieToFile");
long ConvertMovieToFile(long a, long b, long c_, long d, long e, long f) { shim_note("_ConvertMovieToFile"); return 0; }

long ConvertTime(long a, long b, long c_, long d, long e, long f) __asm("_ConvertTime");
long ConvertTime(long a, long b, long c_, long d, long e, long f) { shim_note("_ConvertTime"); return 0; }

long ConvertTimeScale(long a, long b, long c_, long d, long e, long f) __asm("_ConvertTimeScale");
long ConvertTimeScale(long a, long b, long c_, long d, long e, long f) { shim_note("_ConvertTimeScale"); return 0; }

long CopyMediaUserData(long a, long b, long c_, long d, long e, long f) __asm("_CopyMediaUserData");
long CopyMediaUserData(long a, long b, long c_, long d, long e, long f) { shim_note("_CopyMediaUserData"); return 0; }

long CopyMovieSelection(long a, long b, long c_, long d, long e, long f) __asm("_CopyMovieSelection");
long CopyMovieSelection(long a, long b, long c_, long d, long e, long f) { shim_note("_CopyMovieSelection"); return 0; }

long CopyMovieSettings(long a, long b, long c_, long d, long e, long f) __asm("_CopyMovieSettings");
long CopyMovieSettings(long a, long b, long c_, long d, long e, long f) { shim_note("_CopyMovieSettings"); return 0; }

long CopyMovieUserData(long a, long b, long c_, long d, long e, long f) __asm("_CopyMovieUserData");
long CopyMovieUserData(long a, long b, long c_, long d, long e, long f) { shim_note("_CopyMovieUserData"); return 0; }

long CopyTrackSettings(long a, long b, long c_, long d, long e, long f) __asm("_CopyTrackSettings");
long CopyTrackSettings(long a, long b, long c_, long d, long e, long f) { shim_note("_CopyTrackSettings"); return 0; }

long CopyTrackUserData(long a, long b, long c_, long d, long e, long f) __asm("_CopyTrackUserData");
long CopyTrackUserData(long a, long b, long c_, long d, long e, long f) { shim_note("_CopyTrackUserData"); return 0; }

long CopyUserData(long a, long b, long c_, long d, long e, long f) __asm("_CopyUserData");
long CopyUserData(long a, long b, long c_, long d, long e, long f) { shim_note("_CopyUserData"); return 0; }

long CountImageDescriptionExtensionType(long a, long b, long c_, long d, long e, long f) __asm("_CountImageDescriptionExtensionType");
long CountImageDescriptionExtensionType(long a, long b, long c_, long d, long e, long f) { shim_note("_CountImageDescriptionExtensionType"); return 0; }

long CountUserDataType(long a, long b, long c_, long d, long e, long f) __asm("_CountUserDataType");
long CountUserDataType(long a, long b, long c_, long d, long e, long f) { shim_note("_CountUserDataType"); return 0; }

long CreateMovieControl(long a, long b, long c_, long d, long e, long f) __asm("_CreateMovieControl");
long CreateMovieControl(long a, long b, long c_, long d, long e, long f) { shim_note("_CreateMovieControl"); return 0; }

long CreateMovieFile(long a, long b, long c_, long d, long e, long f) __asm("_CreateMovieFile");
long CreateMovieFile(long a, long b, long c_, long d, long e, long f) { shim_note("_CreateMovieFile"); return 0; }

long CreateMovieStorage(long a, long b, long c_, long d, long e, long f) __asm("_CreateMovieStorage");
long CreateMovieStorage(long a, long b, long c_, long d, long e, long f) { shim_note("_CreateMovieStorage"); return 0; }

long CreateShortcutMovieFile(long a, long b, long c_, long d, long e, long f) __asm("_CreateShortcutMovieFile");
long CreateShortcutMovieFile(long a, long b, long c_, long d, long e, long f) { shim_note("_CreateShortcutMovieFile"); return 0; }

long CutMovieSelection(long a, long b, long c_, long d, long e, long f) __asm("_CutMovieSelection");
long CutMovieSelection(long a, long b, long c_, long d, long e, long f) { shim_note("_CutMovieSelection"); return 0; }

long DataCodecBeginInterruptSafe(long a, long b, long c_, long d, long e, long f) __asm("_DataCodecBeginInterruptSafe");
long DataCodecBeginInterruptSafe(long a, long b, long c_, long d, long e, long f) { shim_note("_DataCodecBeginInterruptSafe"); return 0; }

long DataCodecCompress(long a, long b, long c_, long d, long e, long f) __asm("_DataCodecCompress");
long DataCodecCompress(long a, long b, long c_, long d, long e, long f) { shim_note("_DataCodecCompress"); return 0; }

long DataCodecDecompress(long a, long b, long c_, long d, long e, long f) __asm("_DataCodecDecompress");
long DataCodecDecompress(long a, long b, long c_, long d, long e, long f) { shim_note("_DataCodecDecompress"); return 0; }

long DataCodecEndInterruptSafe(long a, long b, long c_, long d, long e, long f) __asm("_DataCodecEndInterruptSafe");
long DataCodecEndInterruptSafe(long a, long b, long c_, long d, long e, long f) { shim_note("_DataCodecEndInterruptSafe"); return 0; }

long DataCodecGetCompressBufferSize(long a, long b, long c_, long d, long e, long f) __asm("_DataCodecGetCompressBufferSize");
long DataCodecGetCompressBufferSize(long a, long b, long c_, long d, long e, long f) { shim_note("_DataCodecGetCompressBufferSize"); return 0; }

long DataHAddMovie(long a, long b, long c_, long d, long e, long f) __asm("_DataHAddMovie");
long DataHAddMovie(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHAddMovie"); return 0; }

long DataHAppend64(long a, long b, long c_, long d, long e, long f) __asm("_DataHAppend64");
long DataHAppend64(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHAppend64"); return 0; }

long DataHCanUseDataRef(long a, long b, long c_, long d, long e, long f) __asm("_DataHCanUseDataRef");
long DataHCanUseDataRef(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHCanUseDataRef"); return 0; }

long DataHCloseForRead(long a, long b, long c_, long d, long e, long f) __asm("_DataHCloseForRead");
long DataHCloseForRead(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHCloseForRead"); return 0; }

long DataHCloseForWrite(long a, long b, long c_, long d, long e, long f) __asm("_DataHCloseForWrite");
long DataHCloseForWrite(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHCloseForWrite"); return 0; }

long DataHCompareDataRef(long a, long b, long c_, long d, long e, long f) __asm("_DataHCompareDataRef");
long DataHCompareDataRef(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHCompareDataRef"); return 0; }

long DataHCreateFile(long a, long b, long c_, long d, long e, long f) __asm("_DataHCreateFile");
long DataHCreateFile(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHCreateFile"); return 0; }

long DataHCreateFileWithFlags(long a, long b, long c_, long d, long e, long f) __asm("_DataHCreateFileWithFlags");
long DataHCreateFileWithFlags(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHCreateFileWithFlags"); return 0; }

long DataHDeleteFile(long a, long b, long c_, long d, long e, long f) __asm("_DataHDeleteFile");
long DataHDeleteFile(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHDeleteFile"); return 0; }

long DataHDoesBuffer(long a, long b, long c_, long d, long e, long f) __asm("_DataHDoesBuffer");
long DataHDoesBuffer(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHDoesBuffer"); return 0; }

long DataHFinishData(long a, long b, long c_, long d, long e, long f) __asm("_DataHFinishData");
long DataHFinishData(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHFinishData"); return 0; }

long DataHFlushCache(long a, long b, long c_, long d, long e, long f) __asm("_DataHFlushCache");
long DataHFlushCache(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHFlushCache"); return 0; }

long DataHFlushData(long a, long b, long c_, long d, long e, long f) __asm("_DataHFlushData");
long DataHFlushData(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHFlushData"); return 0; }

long DataHGetAvailableFileSize(long a, long b, long c_, long d, long e, long f) __asm("_DataHGetAvailableFileSize");
long DataHGetAvailableFileSize(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHGetAvailableFileSize"); return 0; }

long DataHGetCacheSizeLimit(long a, long b, long c_, long d, long e, long f) __asm("_DataHGetCacheSizeLimit");
long DataHGetCacheSizeLimit(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHGetCacheSizeLimit"); return 0; }

long DataHGetData(long a, long b, long c_, long d, long e, long f) __asm("_DataHGetData");
long DataHGetData(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHGetData"); return 0; }

long DataHGetDataAvailability(long a, long b, long c_, long d, long e, long f) __asm("_DataHGetDataAvailability");
long DataHGetDataAvailability(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHGetDataAvailability"); return 0; }

long DataHGetDataInBuffer(long a, long b, long c_, long d, long e, long f) __asm("_DataHGetDataInBuffer");
long DataHGetDataInBuffer(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHGetDataInBuffer"); return 0; }

long DataHGetDataRate(long a, long b, long c_, long d, long e, long f) __asm("_DataHGetDataRate");
long DataHGetDataRate(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHGetDataRate"); return 0; }

long DataHGetDataRef(long a, long b, long c_, long d, long e, long f) __asm("_DataHGetDataRef");
long DataHGetDataRef(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHGetDataRef"); return 0; }

long DataHGetDataRefAsType(long a, long b, long c_, long d, long e, long f) __asm("_DataHGetDataRefAsType");
long DataHGetDataRefAsType(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHGetDataRefAsType"); return 0; }

long DataHGetDataRefExtension(long a, long b, long c_, long d, long e, long f) __asm("_DataHGetDataRefExtension");
long DataHGetDataRefExtension(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHGetDataRefExtension"); return 0; }

long DataHGetDataRefWithAnchor(long a, long b, long c_, long d, long e, long f) __asm("_DataHGetDataRefWithAnchor");
long DataHGetDataRefWithAnchor(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHGetDataRefWithAnchor"); return 0; }

long DataHGetDeviceIndex(long a, long b, long c_, long d, long e, long f) __asm("_DataHGetDeviceIndex");
long DataHGetDeviceIndex(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHGetDeviceIndex"); return 0; }

long DataHGetFileName(long a, long b, long c_, long d, long e, long f) __asm("_DataHGetFileName");
long DataHGetFileName(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHGetFileName"); return 0; }

long DataHGetFileSize(long a, long b, long c_, long d, long e, long f) __asm("_DataHGetFileSize");
long DataHGetFileSize(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHGetFileSize"); return 0; }

long DataHGetFileSize64(long a, long b, long c_, long d, long e, long f) __asm("_DataHGetFileSize64");
long DataHGetFileSize64(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHGetFileSize64"); return 0; }

long DataHGetFileTypeOrdering(long a, long b, long c_, long d, long e, long f) __asm("_DataHGetFileTypeOrdering");
long DataHGetFileTypeOrdering(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHGetFileTypeOrdering"); return 0; }

long DataHGetFreeSpace(long a, long b, long c_, long d, long e, long f) __asm("_DataHGetFreeSpace");
long DataHGetFreeSpace(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHGetFreeSpace"); return 0; }

long DataHGetFreeSpace64(long a, long b, long c_, long d, long e, long f) __asm("_DataHGetFreeSpace64");
long DataHGetFreeSpace64(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHGetFreeSpace64"); return 0; }

long DataHGetInfo(long a, long b, long c_, long d, long e, long f) __asm("_DataHGetInfo");
long DataHGetInfo(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHGetInfo"); return 0; }

long DataHGetInfoFlags(long a, long b, long c_, long d, long e, long f) __asm("_DataHGetInfoFlags");
long DataHGetInfoFlags(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHGetInfoFlags"); return 0; }

long DataHGetMIMEType(long a, long b, long c_, long d, long e, long f) __asm("_DataHGetMIMEType");
long DataHGetMIMEType(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHGetMIMEType"); return 0; }

long DataHGetMacOSFileType(long a, long b, long c_, long d, long e, long f) __asm("_DataHGetMacOSFileType");
long DataHGetMacOSFileType(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHGetMacOSFileType"); return 0; }

long DataHGetMovie(long a, long b, long c_, long d, long e, long f) __asm("_DataHGetMovie");
long DataHGetMovie(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHGetMovie"); return 0; }

long DataHGetMovieWithFlags(long a, long b, long c_, long d, long e, long f) __asm("_DataHGetMovieWithFlags");
long DataHGetMovieWithFlags(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHGetMovieWithFlags"); return 0; }

long DataHGetPreferredBlockSize(long a, long b, long c_, long d, long e, long f) __asm("_DataHGetPreferredBlockSize");
long DataHGetPreferredBlockSize(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHGetPreferredBlockSize"); return 0; }

long DataHGetScheduleAheadTime(long a, long b, long c_, long d, long e, long f) __asm("_DataHGetScheduleAheadTime");
long DataHGetScheduleAheadTime(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHGetScheduleAheadTime"); return 0; }

long DataHGetTemporaryDataRefCapabilities(long a, long b, long c_, long d, long e, long f) __asm("_DataHGetTemporaryDataRefCapabilities");
long DataHGetTemporaryDataRefCapabilities(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHGetTemporaryDataRefCapabilities"); return 0; }

long DataHIsStreamingDataHandler(long a, long b, long c_, long d, long e, long f) __asm("_DataHIsStreamingDataHandler");
long DataHIsStreamingDataHandler(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHIsStreamingDataHandler"); return 0; }

long DataHOpenForRead(long a, long b, long c_, long d, long e, long f) __asm("_DataHOpenForRead");
long DataHOpenForRead(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHOpenForRead"); return 0; }

long DataHOpenForWrite(long a, long b, long c_, long d, long e, long f) __asm("_DataHOpenForWrite");
long DataHOpenForWrite(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHOpenForWrite"); return 0; }

long DataHPlaybackHints(long a, long b, long c_, long d, long e, long f) __asm("_DataHPlaybackHints");
long DataHPlaybackHints(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHPlaybackHints"); return 0; }

long DataHPlaybackHints64(long a, long b, long c_, long d, long e, long f) __asm("_DataHPlaybackHints64");
long DataHPlaybackHints64(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHPlaybackHints64"); return 0; }

long DataHPollRead(long a, long b, long c_, long d, long e, long f) __asm("_DataHPollRead");
long DataHPollRead(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHPollRead"); return 0; }

long DataHPreextend(long a, long b, long c_, long d, long e, long f) __asm("_DataHPreextend");
long DataHPreextend(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHPreextend"); return 0; }

long DataHPreextend64(long a, long b, long c_, long d, long e, long f) __asm("_DataHPreextend64");
long DataHPreextend64(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHPreextend64"); return 0; }

long DataHPutData(long a, long b, long c_, long d, long e, long f) __asm("_DataHPutData");
long DataHPutData(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHPutData"); return 0; }

long DataHRenameFile(long a, long b, long c_, long d, long e, long f) __asm("_DataHRenameFile");
long DataHRenameFile(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHRenameFile"); return 0; }

long DataHResolveDataRef(long a, long b, long c_, long d, long e, long f) __asm("_DataHResolveDataRef");
long DataHResolveDataRef(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHResolveDataRef"); return 0; }

long DataHSetCacheSizeLimit(long a, long b, long c_, long d, long e, long f) __asm("_DataHSetCacheSizeLimit");
long DataHSetCacheSizeLimit(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHSetCacheSizeLimit"); return 0; }

long DataHSetDataRef(long a, long b, long c_, long d, long e, long f) __asm("_DataHSetDataRef");
long DataHSetDataRef(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHSetDataRef"); return 0; }

long DataHSetDataRefExtension(long a, long b, long c_, long d, long e, long f) __asm("_DataHSetDataRefExtension");
long DataHSetDataRefExtension(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHSetDataRefExtension"); return 0; }

long DataHSetDataRefWithAnchor(long a, long b, long c_, long d, long e, long f) __asm("_DataHSetDataRefWithAnchor");
long DataHSetDataRefWithAnchor(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHSetDataRefWithAnchor"); return 0; }

long DataHSetFileSize(long a, long b, long c_, long d, long e, long f) __asm("_DataHSetFileSize");
long DataHSetFileSize(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHSetFileSize"); return 0; }

long DataHSetFileSize64(long a, long b, long c_, long d, long e, long f) __asm("_DataHSetFileSize64");
long DataHSetFileSize64(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHSetFileSize64"); return 0; }

long DataHSetIdleManager(long a, long b, long c_, long d, long e, long f) __asm("_DataHSetIdleManager");
long DataHSetIdleManager(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHSetIdleManager"); return 0; }

long DataHSetMacOSFileType(long a, long b, long c_, long d, long e, long f) __asm("_DataHSetMacOSFileType");
long DataHSetMacOSFileType(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHSetMacOSFileType"); return 0; }

long DataHSetMovieUsageFlags(long a, long b, long c_, long d, long e, long f) __asm("_DataHSetMovieUsageFlags");
long DataHSetMovieUsageFlags(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHSetMovieUsageFlags"); return 0; }

long DataHSetTimeBase(long a, long b, long c_, long d, long e, long f) __asm("_DataHSetTimeBase");
long DataHSetTimeBase(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHSetTimeBase"); return 0; }

long DataHSetTimeHints(long a, long b, long c_, long d, long e, long f) __asm("_DataHSetTimeHints");
long DataHSetTimeHints(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHSetTimeHints"); return 0; }

long DataHTask(long a, long b, long c_, long d, long e, long f) __asm("_DataHTask");
long DataHTask(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHTask"); return 0; }

long DataHUpdateMovie(long a, long b, long c_, long d, long e, long f) __asm("_DataHUpdateMovie");
long DataHUpdateMovie(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHUpdateMovie"); return 0; }

long DataHUseTemporaryDataRef(long a, long b, long c_, long d, long e, long f) __asm("_DataHUseTemporaryDataRef");
long DataHUseTemporaryDataRef(long a, long b, long c_, long d, long e, long f) { shim_note("_DataHUseTemporaryDataRef"); return 0; }

long DecompressImage(long a, long b, long c_, long d, long e, long f) __asm("_DecompressImage");
long DecompressImage(long a, long b, long c_, long d, long e, long f) { shim_note("_DecompressImage"); return 0; }

long DeleteMovieFile(long a, long b, long c_, long d, long e, long f) __asm("_DeleteMovieFile");
long DeleteMovieFile(long a, long b, long c_, long d, long e, long f) { shim_note("_DeleteMovieFile"); return 0; }

long DeleteMovieSegment(long a, long b, long c_, long d, long e, long f) __asm("_DeleteMovieSegment");
long DeleteMovieSegment(long a, long b, long c_, long d, long e, long f) { shim_note("_DeleteMovieSegment"); return 0; }

long DeleteMovieStorage(long a, long b, long c_, long d, long e, long f) __asm("_DeleteMovieStorage");
long DeleteMovieStorage(long a, long b, long c_, long d, long e, long f) { shim_note("_DeleteMovieStorage"); return 0; }

long DeleteTrackReference(long a, long b, long c_, long d, long e, long f) __asm("_DeleteTrackReference");
long DeleteTrackReference(long a, long b, long c_, long d, long e, long f) { shim_note("_DeleteTrackReference"); return 0; }

long DeleteTrackSegment(long a, long b, long c_, long d, long e, long f) __asm("_DeleteTrackSegment");
long DeleteTrackSegment(long a, long b, long c_, long d, long e, long f) { shim_note("_DeleteTrackSegment"); return 0; }

long DisposeMatte(long a, long b, long c_, long d, long e, long f) __asm("_DisposeMatte");
long DisposeMatte(long a, long b, long c_, long d, long e, long f) { shim_note("_DisposeMatte"); return 0; }

long DisposeMovieController(long a, long b, long c_, long d, long e, long f) __asm("_DisposeMovieController");
long DisposeMovieController(long a, long b, long c_, long d, long e, long f) { shim_note("_DisposeMovieController"); return 0; }

long DisposeMovieTrack(long a, long b, long c_, long d, long e, long f) __asm("_DisposeMovieTrack");
long DisposeMovieTrack(long a, long b, long c_, long d, long e, long f) { shim_note("_DisposeMovieTrack"); return 0; }

long DisposeTimeBase(long a, long b, long c_, long d, long e, long f) __asm("_DisposeTimeBase");
long DisposeTimeBase(long a, long b, long c_, long d, long e, long f) { shim_note("_DisposeTimeBase"); return 0; }

long DisposeTrackMedia(long a, long b, long c_, long d, long e, long f) __asm("_DisposeTrackMedia");
long DisposeTrackMedia(long a, long b, long c_, long d, long e, long f) { shim_note("_DisposeTrackMedia"); return 0; }

long DisposeUserData(long a, long b, long c_, long d, long e, long f) __asm("_DisposeUserData");
long DisposeUserData(long a, long b, long c_, long d, long e, long f) { shim_note("_DisposeUserData"); return 0; }

long DragAlignedWindow(long a, long b, long c_, long d, long e, long f) __asm("_DragAlignedWindow");
long DragAlignedWindow(long a, long b, long c_, long d, long e, long f) { shim_note("_DragAlignedWindow"); return 0; }

long EndFullScreen(long a, long b, long c_, long d, long e, long f) __asm("_EndFullScreen");
long EndFullScreen(long a, long b, long c_, long d, long e, long f) { shim_note("_EndFullScreen"); return 0; }

long EndMediaEdits(long a, long b, long c_, long d, long e, long f) __asm("_EndMediaEdits");
long EndMediaEdits(long a, long b, long c_, long d, long e, long f) { shim_note("_EndMediaEdits"); return 0; }

long FindCodec(long a, long b, long c_, long d, long e, long f) __asm("_FindCodec");
long FindCodec(long a, long b, long c_, long d, long e, long f) { shim_note("_FindCodec"); return 0; }

long FixExp2(long a, long b, long c_, long d, long e, long f) __asm("_FixExp2");
long FixExp2(long a, long b, long c_, long d, long e, long f) { shim_note("_FixExp2"); return 0; }

long FixLog2(long a, long b, long c_, long d, long e, long f) __asm("_FixLog2");
long FixLog2(long a, long b, long c_, long d, long e, long f) { shim_note("_FixLog2"); return 0; }

long FixMulDiv(long a, long b, long c_, long d, long e, long f) __asm("_FixMulDiv");
long FixMulDiv(long a, long b, long c_, long d, long e, long f) { shim_note("_FixMulDiv"); return 0; }

long FixPow(long a, long b, long c_, long d, long e, long f) __asm("_FixPow");
long FixPow(long a, long b, long c_, long d, long e, long f) { shim_note("_FixPow"); return 0; }

long FlashMediaDoButtonActions(long a, long b, long c_, long d, long e, long f) __asm("_FlashMediaDoButtonActions");
long FlashMediaDoButtonActions(long a, long b, long c_, long d, long e, long f) { shim_note("_FlashMediaDoButtonActions"); return 0; }

long FlashMediaFrameLabelToMovieTime(long a, long b, long c_, long d, long e, long f) __asm("_FlashMediaFrameLabelToMovieTime");
long FlashMediaFrameLabelToMovieTime(long a, long b, long c_, long d, long e, long f) { shim_note("_FlashMediaFrameLabelToMovieTime"); return 0; }

long FlashMediaFrameNumberToMovieTime(long a, long b, long c_, long d, long e, long f) __asm("_FlashMediaFrameNumberToMovieTime");
long FlashMediaFrameNumberToMovieTime(long a, long b, long c_, long d, long e, long f) { shim_note("_FlashMediaFrameNumberToMovieTime"); return 0; }

long FlashMediaGetDisplayedFrameNumber(long a, long b, long c_, long d, long e, long f) __asm("_FlashMediaGetDisplayedFrameNumber");
long FlashMediaGetDisplayedFrameNumber(long a, long b, long c_, long d, long e, long f) { shim_note("_FlashMediaGetDisplayedFrameNumber"); return 0; }

long FlashMediaGetFlashVariable(long a, long b, long c_, long d, long e, long f) __asm("_FlashMediaGetFlashVariable");
long FlashMediaGetFlashVariable(long a, long b, long c_, long d, long e, long f) { shim_note("_FlashMediaGetFlashVariable"); return 0; }

long FlashMediaGetRefConBounds(long a, long b, long c_, long d, long e, long f) __asm("_FlashMediaGetRefConBounds");
long FlashMediaGetRefConBounds(long a, long b, long c_, long d, long e, long f) { shim_note("_FlashMediaGetRefConBounds"); return 0; }

long FlashMediaGetRefConID(long a, long b, long c_, long d, long e, long f) __asm("_FlashMediaGetRefConID");
long FlashMediaGetRefConID(long a, long b, long c_, long d, long e, long f) { shim_note("_FlashMediaGetRefConID"); return 0; }

long FlashMediaGetSupportedSwfVersion(long a, long b, long c_, long d, long e, long f) __asm("_FlashMediaGetSupportedSwfVersion");
long FlashMediaGetSupportedSwfVersion(long a, long b, long c_, long d, long e, long f) { shim_note("_FlashMediaGetSupportedSwfVersion"); return 0; }

long FlashMediaIDToRefCon(long a, long b, long c_, long d, long e, long f) __asm("_FlashMediaIDToRefCon");
long FlashMediaIDToRefCon(long a, long b, long c_, long d, long e, long f) { shim_note("_FlashMediaIDToRefCon"); return 0; }

long FlashMediaSetFlashVariable(long a, long b, long c_, long d, long e, long f) __asm("_FlashMediaSetFlashVariable");
long FlashMediaSetFlashVariable(long a, long b, long c_, long d, long e, long f) { shim_note("_FlashMediaSetFlashVariable"); return 0; }

long FlashMediaSetPan(long a, long b, long c_, long d, long e, long f) __asm("_FlashMediaSetPan");
long FlashMediaSetPan(long a, long b, long c_, long d, long e, long f) { shim_note("_FlashMediaSetPan"); return 0; }

long FlashMediaSetZoom(long a, long b, long c_, long d, long e, long f) __asm("_FlashMediaSetZoom");
long FlashMediaSetZoom(long a, long b, long c_, long d, long e, long f) { shim_note("_FlashMediaSetZoom"); return 0; }

long FlashMediaSetZoomRect(long a, long b, long c_, long d, long e, long f) __asm("_FlashMediaSetZoomRect");
long FlashMediaSetZoomRect(long a, long b, long c_, long d, long e, long f) { shim_note("_FlashMediaSetZoomRect"); return 0; }

long FlattenMovie(long a, long b, long c_, long d, long e, long f) __asm("_FlattenMovie");
long FlattenMovie(long a, long b, long c_, long d, long e, long f) { shim_note("_FlattenMovie"); return 0; }

long FlattenMovieData(long a, long b, long c_, long d, long e, long f) __asm("_FlattenMovieData");
long FlattenMovieData(long a, long b, long c_, long d, long e, long f) { shim_note("_FlattenMovieData"); return 0; }

long FlattenMovieDataToDataRef(long a, long b, long c_, long d, long e, long f) __asm("_FlattenMovieDataToDataRef");
long FlattenMovieDataToDataRef(long a, long b, long c_, long d, long e, long f) { shim_note("_FlattenMovieDataToDataRef"); return 0; }

long GDGetScale(long a, long b, long c_, long d, long e, long f) __asm("_GDGetScale");
long GDGetScale(long a, long b, long c_, long d, long e, long f) { shim_note("_GDGetScale"); return 0; }

long GDHasScale(long a, long b, long c_, long d, long e, long f) __asm("_GDHasScale");
long GDHasScale(long a, long b, long c_, long d, long e, long f) { shim_note("_GDHasScale"); return 0; }

long GDSetScale(long a, long b, long c_, long d, long e, long f) __asm("_GDSetScale");
long GDSetScale(long a, long b, long c_, long d, long e, long f) { shim_note("_GDSetScale"); return 0; }

long GetBestDeviceRect(long a, long b, long c_, long d, long e, long f) __asm("_GetBestDeviceRect");
long GetBestDeviceRect(long a, long b, long c_, long d, long e, long f) { shim_note("_GetBestDeviceRect"); return 0; }

long GetCompressionTime(long a, long b, long c_, long d, long e, long f) __asm("_GetCompressionTime");
long GetCompressionTime(long a, long b, long c_, long d, long e, long f) { shim_note("_GetCompressionTime"); return 0; }

long GetDataHandler(long a, long b, long c_, long d, long e, long f) __asm("_GetDataHandler");
long GetDataHandler(long a, long b, long c_, long d, long e, long f) { shim_note("_GetDataHandler"); return 0; }

long GetGraphicsImporterForDataRefWithFlags(long a, long b, long c_, long d, long e, long f) __asm("_GetGraphicsImporterForDataRefWithFlags");
long GetGraphicsImporterForDataRefWithFlags(long a, long b, long c_, long d, long e, long f) { shim_note("_GetGraphicsImporterForDataRefWithFlags"); return 0; }

long GetGraphicsImporterForFileWithFlags(long a, long b, long c_, long d, long e, long f) __asm("_GetGraphicsImporterForFileWithFlags");
long GetGraphicsImporterForFileWithFlags(long a, long b, long c_, long d, long e, long f) { shim_note("_GetGraphicsImporterForFileWithFlags"); return 0; }

long GetImageDescriptionCTable(long a, long b, long c_, long d, long e, long f) __asm("_GetImageDescriptionCTable");
long GetImageDescriptionCTable(long a, long b, long c_, long d, long e, long f) { shim_note("_GetImageDescriptionCTable"); return 0; }

long GetImageDescriptionExtension(long a, long b, long c_, long d, long e, long f) __asm("_GetImageDescriptionExtension");
long GetImageDescriptionExtension(long a, long b, long c_, long d, long e, long f) { shim_note("_GetImageDescriptionExtension"); return 0; }

long GetMaxCompressionSize(long a, long b, long c_, long d, long e, long f) __asm("_GetMaxCompressionSize");
long GetMaxCompressionSize(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMaxCompressionSize"); return 0; }

long GetMaxLoadedTimeInMovie(long a, long b, long c_, long d, long e, long f) __asm("_GetMaxLoadedTimeInMovie");
long GetMaxLoadedTimeInMovie(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMaxLoadedTimeInMovie"); return 0; }

long GetMediaCreationTime(long a, long b, long c_, long d, long e, long f) __asm("_GetMediaCreationTime");
long GetMediaCreationTime(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMediaCreationTime"); return 0; }

long GetMediaDataHandler(long a, long b, long c_, long d, long e, long f) __asm("_GetMediaDataHandler");
long GetMediaDataHandler(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMediaDataHandler"); return 0; }

long GetMediaDataHandlerDescription(long a, long b, long c_, long d, long e, long f) __asm("_GetMediaDataHandlerDescription");
long GetMediaDataHandlerDescription(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMediaDataHandlerDescription"); return 0; }

long GetMediaDataRef(long a, long b, long c_, long d, long e, long f) __asm("_GetMediaDataRef");
long GetMediaDataRef(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMediaDataRef"); return 0; }

long GetMediaDataRefCount(long a, long b, long c_, long d, long e, long f) __asm("_GetMediaDataRefCount");
long GetMediaDataRefCount(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMediaDataRefCount"); return 0; }

long GetMediaDataSize(long a, long b, long c_, long d, long e, long f) __asm("_GetMediaDataSize");
long GetMediaDataSize(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMediaDataSize"); return 0; }

long GetMediaDataSize64(long a, long b, long c_, long d, long e, long f) __asm("_GetMediaDataSize64");
long GetMediaDataSize64(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMediaDataSize64"); return 0; }

long GetMediaDuration(long a, long b, long c_, long d, long e, long f) __asm("_GetMediaDuration");
long GetMediaDuration(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMediaDuration"); return 0; }

long GetMediaHandler(long a, long b, long c_, long d, long e, long f) __asm("_GetMediaHandler");
long GetMediaHandler(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMediaHandler"); return 0; }

long GetMediaHandlerDescription(long a, long b, long c_, long d, long e, long f) __asm("_GetMediaHandlerDescription");
long GetMediaHandlerDescription(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMediaHandlerDescription"); return 0; }

long GetMediaLanguage(long a, long b, long c_, long d, long e, long f) __asm("_GetMediaLanguage");
long GetMediaLanguage(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMediaLanguage"); return 0; }

long GetMediaModificationTime(long a, long b, long c_, long d, long e, long f) __asm("_GetMediaModificationTime");
long GetMediaModificationTime(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMediaModificationTime"); return 0; }

long GetMediaNextInterestingTime(long a, long b, long c_, long d, long e, long f) __asm("_GetMediaNextInterestingTime");
long GetMediaNextInterestingTime(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMediaNextInterestingTime"); return 0; }

long GetMediaPlayHints(long a, long b, long c_, long d, long e, long f) __asm("_GetMediaPlayHints");
long GetMediaPlayHints(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMediaPlayHints"); return 0; }

long GetMediaPreferredChunkSize(long a, long b, long c_, long d, long e, long f) __asm("_GetMediaPreferredChunkSize");
long GetMediaPreferredChunkSize(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMediaPreferredChunkSize"); return 0; }

long GetMediaQuality(long a, long b, long c_, long d, long e, long f) __asm("_GetMediaQuality");
long GetMediaQuality(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMediaQuality"); return 0; }

long GetMediaSample(long a, long b, long c_, long d, long e, long f) __asm("_GetMediaSample");
long GetMediaSample(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMediaSample"); return 0; }

long GetMediaSampleCount(long a, long b, long c_, long d, long e, long f) __asm("_GetMediaSampleCount");
long GetMediaSampleCount(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMediaSampleCount"); return 0; }

long GetMediaSampleDescription(long a, long b, long c_, long d, long e, long f) __asm("_GetMediaSampleDescription");
long GetMediaSampleDescription(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMediaSampleDescription"); return 0; }

long GetMediaSampleDescriptionCount(long a, long b, long c_, long d, long e, long f) __asm("_GetMediaSampleDescriptionCount");
long GetMediaSampleDescriptionCount(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMediaSampleDescriptionCount"); return 0; }

long GetMediaSampleReference(long a, long b, long c_, long d, long e, long f) __asm("_GetMediaSampleReference");
long GetMediaSampleReference(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMediaSampleReference"); return 0; }

long GetMediaShadowSync(long a, long b, long c_, long d, long e, long f) __asm("_GetMediaShadowSync");
long GetMediaShadowSync(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMediaShadowSync"); return 0; }

long GetMediaSyncSampleCount(long a, long b, long c_, long d, long e, long f) __asm("_GetMediaSyncSampleCount");
long GetMediaSyncSampleCount(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMediaSyncSampleCount"); return 0; }

long GetMediaTimeScale(long a, long b, long c_, long d, long e, long f) __asm("_GetMediaTimeScale");
long GetMediaTimeScale(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMediaTimeScale"); return 0; }

long GetMediaTrack(long a, long b, long c_, long d, long e, long f) __asm("_GetMediaTrack");
long GetMediaTrack(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMediaTrack"); return 0; }

long GetMediaUserData(long a, long b, long c_, long d, long e, long f) __asm("_GetMediaUserData");
long GetMediaUserData(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMediaUserData"); return 0; }

long GetMovieActive(long a, long b, long c_, long d, long e, long f) __asm("_GetMovieActive");
long GetMovieActive(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMovieActive"); return 0; }

long GetMovieActiveSegment(long a, long b, long c_, long d, long e, long f) __asm("_GetMovieActiveSegment");
long GetMovieActiveSegment(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMovieActiveSegment"); return 0; }

long GetMovieBoundsRgn(long a, long b, long c_, long d, long e, long f) __asm("_GetMovieBoundsRgn");
long GetMovieBoundsRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMovieBoundsRgn"); return 0; }

long GetMovieClipRgn(long a, long b, long c_, long d, long e, long f) __asm("_GetMovieClipRgn");
long GetMovieClipRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMovieClipRgn"); return 0; }

long GetMovieColorTable(long a, long b, long c_, long d, long e, long f) __asm("_GetMovieColorTable");
long GetMovieColorTable(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMovieColorTable"); return 0; }

long GetMovieCreationTime(long a, long b, long c_, long d, long e, long f) __asm("_GetMovieCreationTime");
long GetMovieCreationTime(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMovieCreationTime"); return 0; }

long GetMovieDataSize(long a, long b, long c_, long d, long e, long f) __asm("_GetMovieDataSize");
long GetMovieDataSize(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMovieDataSize"); return 0; }

long GetMovieDataSize64(long a, long b, long c_, long d, long e, long f) __asm("_GetMovieDataSize64");
long GetMovieDataSize64(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMovieDataSize64"); return 0; }

long GetMovieDefaultDataRef(long a, long b, long c_, long d, long e, long f) __asm("_GetMovieDefaultDataRef");
long GetMovieDefaultDataRef(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMovieDefaultDataRef"); return 0; }

long GetMovieDisplayBoundsRgn(long a, long b, long c_, long d, long e, long f) __asm("_GetMovieDisplayBoundsRgn");
long GetMovieDisplayBoundsRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMovieDisplayBoundsRgn"); return 0; }

long GetMovieDisplayClipRgn(long a, long b, long c_, long d, long e, long f) __asm("_GetMovieDisplayClipRgn");
long GetMovieDisplayClipRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMovieDisplayClipRgn"); return 0; }

long GetMovieImporterForDataRef(long a, long b, long c_, long d, long e, long f) __asm("_GetMovieImporterForDataRef");
long GetMovieImporterForDataRef(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMovieImporterForDataRef"); return 0; }

long GetMovieIndTrack(long a, long b, long c_, long d, long e, long f) __asm("_GetMovieIndTrack");
long GetMovieIndTrack(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMovieIndTrack"); return 0; }

long GetMovieIndTrackType(long a, long b, long c_, long d, long e, long f) __asm("_GetMovieIndTrackType");
long GetMovieIndTrackType(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMovieIndTrackType"); return 0; }

long GetMovieModificationTime(long a, long b, long c_, long d, long e, long f) __asm("_GetMovieModificationTime");
long GetMovieModificationTime(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMovieModificationTime"); return 0; }

long GetMovieNextInterestingTime(long a, long b, long c_, long d, long e, long f) __asm("_GetMovieNextInterestingTime");
long GetMovieNextInterestingTime(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMovieNextInterestingTime"); return 0; }

long GetMoviePosterPict(long a, long b, long c_, long d, long e, long f) __asm("_GetMoviePosterPict");
long GetMoviePosterPict(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMoviePosterPict"); return 0; }

long GetMoviePosterTime(long a, long b, long c_, long d, long e, long f) __asm("_GetMoviePosterTime");
long GetMoviePosterTime(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMoviePosterTime"); return 0; }

long GetMoviePreferredRate(long a, long b, long c_, long d, long e, long f) __asm("_GetMoviePreferredRate");
long GetMoviePreferredRate(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMoviePreferredRate"); return 0; }

long GetMoviePreferredVolume(long a, long b, long c_, long d, long e, long f) __asm("_GetMoviePreferredVolume");
long GetMoviePreferredVolume(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMoviePreferredVolume"); return 0; }

long GetMoviePreviewMode(long a, long b, long c_, long d, long e, long f) __asm("_GetMoviePreviewMode");
long GetMoviePreviewMode(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMoviePreviewMode"); return 0; }

long GetMoviePreviewTime(long a, long b, long c_, long d, long e, long f) __asm("_GetMoviePreviewTime");
long GetMoviePreviewTime(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMoviePreviewTime"); return 0; }

long GetMovieSegmentDisplayBoundsRgn(long a, long b, long c_, long d, long e, long f) __asm("_GetMovieSegmentDisplayBoundsRgn");
long GetMovieSegmentDisplayBoundsRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMovieSegmentDisplayBoundsRgn"); return 0; }

long GetMovieSelection(long a, long b, long c_, long d, long e, long f) __asm("_GetMovieSelection");
long GetMovieSelection(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMovieSelection"); return 0; }

long GetMovieStatus(long a, long b, long c_, long d, long e, long f) __asm("_GetMovieStatus");
long GetMovieStatus(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMovieStatus"); return 0; }

long GetMovieTimeBase(long a, long b, long c_, long d, long e, long f) __asm("_GetMovieTimeBase");
long GetMovieTimeBase(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMovieTimeBase"); return 0; }

long GetMovieTrack(long a, long b, long c_, long d, long e, long f) __asm("_GetMovieTrack");
long GetMovieTrack(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMovieTrack"); return 0; }

long GetMovieTrackCount(long a, long b, long c_, long d, long e, long f) __asm("_GetMovieTrackCount");
long GetMovieTrackCount(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMovieTrackCount"); return 0; }

long GetMovieUserData(long a, long b, long c_, long d, long e, long f) __asm("_GetMovieUserData");
long GetMovieUserData(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMovieUserData"); return 0; }

long GetMovieVolume(long a, long b, long c_, long d, long e, long f) __asm("_GetMovieVolume");
long GetMovieVolume(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMovieVolume"); return 0; }

long GetMoviesError(long a, long b, long c_, long d, long e, long f) __asm("_GetMoviesError");
long GetMoviesError(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMoviesError"); return 0; }

long GetMoviesStickyError(long a, long b, long c_, long d, long e, long f) __asm("_GetMoviesStickyError");
long GetMoviesStickyError(long a, long b, long c_, long d, long e, long f) { shim_note("_GetMoviesStickyError"); return 0; }

long GetNextImageDescriptionExtensionType(long a, long b, long c_, long d, long e, long f) __asm("_GetNextImageDescriptionExtensionType");
long GetNextImageDescriptionExtensionType(long a, long b, long c_, long d, long e, long f) { shim_note("_GetNextImageDescriptionExtensionType"); return 0; }

long GetNextTrackForCompositing(long a, long b, long c_, long d, long e, long f) __asm("_GetNextTrackForCompositing");
long GetNextTrackForCompositing(long a, long b, long c_, long d, long e, long f) { shim_note("_GetNextTrackForCompositing"); return 0; }

long GetNextTrackReferenceType(long a, long b, long c_, long d, long e, long f) __asm("_GetNextTrackReferenceType");
long GetNextTrackReferenceType(long a, long b, long c_, long d, long e, long f) { shim_note("_GetNextTrackReferenceType"); return 0; }

long GetNextUserDataType(long a, long b, long c_, long d, long e, long f) __asm("_GetNextUserDataType");
long GetNextUserDataType(long a, long b, long c_, long d, long e, long f) { shim_note("_GetNextUserDataType"); return 0; }

long GetPosterBox(long a, long b, long c_, long d, long e, long f) __asm("_GetPosterBox");
long GetPosterBox(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPosterBox"); return 0; }

long GetPrevTrackForCompositing(long a, long b, long c_, long d, long e, long f) __asm("_GetPrevTrackForCompositing");
long GetPrevTrackForCompositing(long a, long b, long c_, long d, long e, long f) { shim_note("_GetPrevTrackForCompositing"); return 0; }

long GetSimilarity(long a, long b, long c_, long d, long e, long f) __asm("_GetSimilarity");
long GetSimilarity(long a, long b, long c_, long d, long e, long f) { shim_note("_GetSimilarity"); return 0; }

long GetSoundDescriptionExtension(long a, long b, long c_, long d, long e, long f) __asm("_GetSoundDescriptionExtension");
long GetSoundDescriptionExtension(long a, long b, long c_, long d, long e, long f) { shim_note("_GetSoundDescriptionExtension"); return 0; }

long GetTimeBaseEffectiveRate(long a, long b, long c_, long d, long e, long f) __asm("_GetTimeBaseEffectiveRate");
long GetTimeBaseEffectiveRate(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTimeBaseEffectiveRate"); return 0; }

long GetTimeBaseFlags(long a, long b, long c_, long d, long e, long f) __asm("_GetTimeBaseFlags");
long GetTimeBaseFlags(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTimeBaseFlags"); return 0; }

long GetTimeBaseMasterClock(long a, long b, long c_, long d, long e, long f) __asm("_GetTimeBaseMasterClock");
long GetTimeBaseMasterClock(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTimeBaseMasterClock"); return 0; }

long GetTimeBaseMasterTimeBase(long a, long b, long c_, long d, long e, long f) __asm("_GetTimeBaseMasterTimeBase");
long GetTimeBaseMasterTimeBase(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTimeBaseMasterTimeBase"); return 0; }

long GetTimeBaseRate(long a, long b, long c_, long d, long e, long f) __asm("_GetTimeBaseRate");
long GetTimeBaseRate(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTimeBaseRate"); return 0; }

long GetTimeBaseStartTime(long a, long b, long c_, long d, long e, long f) __asm("_GetTimeBaseStartTime");
long GetTimeBaseStartTime(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTimeBaseStartTime"); return 0; }

long GetTimeBaseStatus(long a, long b, long c_, long d, long e, long f) __asm("_GetTimeBaseStatus");
long GetTimeBaseStatus(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTimeBaseStatus"); return 0; }

long GetTimeBaseStopTime(long a, long b, long c_, long d, long e, long f) __asm("_GetTimeBaseStopTime");
long GetTimeBaseStopTime(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTimeBaseStopTime"); return 0; }

long GetTimeBaseTime(long a, long b, long c_, long d, long e, long f) __asm("_GetTimeBaseTime");
long GetTimeBaseTime(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTimeBaseTime"); return 0; }

long GetTrackAlternate(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackAlternate");
long GetTrackAlternate(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackAlternate"); return 0; }

long GetTrackBoundsRgn(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackBoundsRgn");
long GetTrackBoundsRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackBoundsRgn"); return 0; }

long GetTrackClipRgn(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackClipRgn");
long GetTrackClipRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackClipRgn"); return 0; }

long GetTrackCreationTime(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackCreationTime");
long GetTrackCreationTime(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackCreationTime"); return 0; }

long GetTrackDataSize(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackDataSize");
long GetTrackDataSize(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackDataSize"); return 0; }

long GetTrackDataSize64(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackDataSize64");
long GetTrackDataSize64(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackDataSize64"); return 0; }

long GetTrackDimensions(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackDimensions");
long GetTrackDimensions(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackDimensions"); return 0; }

long GetTrackDisplayBoundsRgn(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackDisplayBoundsRgn");
long GetTrackDisplayBoundsRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackDisplayBoundsRgn"); return 0; }

long GetTrackDuration(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackDuration");
long GetTrackDuration(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackDuration"); return 0; }

long GetTrackEditRate(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackEditRate");
long GetTrackEditRate(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackEditRate"); return 0; }

long GetTrackEnabled(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackEnabled");
long GetTrackEnabled(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackEnabled"); return 0; }

long GetTrackID(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackID");
long GetTrackID(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackID"); return 0; }

long GetTrackLayer(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackLayer");
long GetTrackLayer(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackLayer"); return 0; }

long GetTrackLoadSettings(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackLoadSettings");
long GetTrackLoadSettings(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackLoadSettings"); return 0; }

long GetTrackMatte(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackMatte");
long GetTrackMatte(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackMatte"); return 0; }

long GetTrackMedia(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackMedia");
long GetTrackMedia(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackMedia"); return 0; }

long GetTrackModificationTime(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackModificationTime");
long GetTrackModificationTime(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackModificationTime"); return 0; }

long GetTrackMovie(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackMovie");
long GetTrackMovie(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackMovie"); return 0; }

long GetTrackMovieBoundsRgn(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackMovieBoundsRgn");
long GetTrackMovieBoundsRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackMovieBoundsRgn"); return 0; }

long GetTrackNextInterestingTime(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackNextInterestingTime");
long GetTrackNextInterestingTime(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackNextInterestingTime"); return 0; }

long GetTrackOffset(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackOffset");
long GetTrackOffset(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackOffset"); return 0; }

long GetTrackPict(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackPict");
long GetTrackPict(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackPict"); return 0; }

long GetTrackReference(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackReference");
long GetTrackReference(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackReference"); return 0; }

long GetTrackReferenceCount(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackReferenceCount");
long GetTrackReferenceCount(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackReferenceCount"); return 0; }

long GetTrackSegmentDisplayBoundsRgn(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackSegmentDisplayBoundsRgn");
long GetTrackSegmentDisplayBoundsRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackSegmentDisplayBoundsRgn"); return 0; }

long GetTrackSoundLocalizationSettings(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackSoundLocalizationSettings");
long GetTrackSoundLocalizationSettings(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackSoundLocalizationSettings"); return 0; }

long GetTrackStatus(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackStatus");
long GetTrackStatus(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackStatus"); return 0; }

long GetTrackUsage(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackUsage");
long GetTrackUsage(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackUsage"); return 0; }

long GetTrackUserData(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackUserData");
long GetTrackUserData(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackUserData"); return 0; }

long GetTrackVolume(long a, long b, long c_, long d, long e, long f) __asm("_GetTrackVolume");
long GetTrackVolume(long a, long b, long c_, long d, long e, long f) { shim_note("_GetTrackVolume"); return 0; }

long GetUserData(long a, long b, long c_, long d, long e, long f) __asm("_GetUserData");
long GetUserData(long a, long b, long c_, long d, long e, long f) { shim_note("_GetUserData"); return 0; }

long GetUserDataText(long a, long b, long c_, long d, long e, long f) __asm("_GetUserDataText");
long GetUserDataText(long a, long b, long c_, long d, long e, long f) { shim_note("_GetUserDataText"); return 0; }

long GoToEndOfMovie(long a, long b, long c_, long d, long e, long f) __asm("_GoToEndOfMovie");
long GoToEndOfMovie(long a, long b, long c_, long d, long e, long f) { shim_note("_GoToEndOfMovie"); return 0; }

long GraphicsExportCanTranscode(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportCanTranscode");
long GraphicsExportCanTranscode(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportCanTranscode"); return 0; }

long GraphicsExportCanUseCompressor(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportCanUseCompressor");
long GraphicsExportCanUseCompressor(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportCanUseCompressor"); return 0; }

long GraphicsExportDoExport(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportDoExport");
long GraphicsExportDoExport(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportDoExport"); return 0; }

long GraphicsExportDoStandaloneExport(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportDoStandaloneExport");
long GraphicsExportDoStandaloneExport(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportDoStandaloneExport"); return 0; }

long GraphicsExportDoTranscode(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportDoTranscode");
long GraphicsExportDoTranscode(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportDoTranscode"); return 0; }

long GraphicsExportDoUseCompressor(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportDoUseCompressor");
long GraphicsExportDoUseCompressor(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportDoUseCompressor"); return 0; }

long GraphicsExportDrawInputImage(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportDrawInputImage");
long GraphicsExportDrawInputImage(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportDrawInputImage"); return 0; }

long GraphicsExportGetColorSyncProfile(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetColorSyncProfile");
long GraphicsExportGetColorSyncProfile(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetColorSyncProfile"); return 0; }

long GraphicsExportGetCompressionMethod(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetCompressionMethod");
long GraphicsExportGetCompressionMethod(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetCompressionMethod"); return 0; }

long GraphicsExportGetCompressionQuality(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetCompressionQuality");
long GraphicsExportGetCompressionQuality(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetCompressionQuality"); return 0; }

long GraphicsExportGetDefaultFileNameExtension(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetDefaultFileNameExtension");
long GraphicsExportGetDefaultFileNameExtension(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetDefaultFileNameExtension"); return 0; }

long GraphicsExportGetDefaultFileTypeAndCreator(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetDefaultFileTypeAndCreator");
long GraphicsExportGetDefaultFileTypeAndCreator(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetDefaultFileTypeAndCreator"); return 0; }

long GraphicsExportGetDepth(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetDepth");
long GraphicsExportGetDepth(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetDepth"); return 0; }

long GraphicsExportGetDontRecompress(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetDontRecompress");
long GraphicsExportGetDontRecompress(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetDontRecompress"); return 0; }

long GraphicsExportGetExifEnabled(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetExifEnabled");
long GraphicsExportGetExifEnabled(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetExifEnabled"); return 0; }

long GraphicsExportGetInputDataReference(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetInputDataReference");
long GraphicsExportGetInputDataReference(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetInputDataReference"); return 0; }

long GraphicsExportGetInputDataSize(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetInputDataSize");
long GraphicsExportGetInputDataSize(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetInputDataSize"); return 0; }

long GraphicsExportGetInputFile(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetInputFile");
long GraphicsExportGetInputFile(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetInputFile"); return 0; }

long GraphicsExportGetInputGWorld(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetInputGWorld");
long GraphicsExportGetInputGWorld(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetInputGWorld"); return 0; }

long GraphicsExportGetInputGraphicsImporter(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetInputGraphicsImporter");
long GraphicsExportGetInputGraphicsImporter(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetInputGraphicsImporter"); return 0; }

long GraphicsExportGetInputHandle(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetInputHandle");
long GraphicsExportGetInputHandle(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetInputHandle"); return 0; }

long GraphicsExportGetInputImageDepth(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetInputImageDepth");
long GraphicsExportGetInputImageDepth(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetInputImageDepth"); return 0; }

long GraphicsExportGetInputImageDescription(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetInputImageDescription");
long GraphicsExportGetInputImageDescription(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetInputImageDescription"); return 0; }

long GraphicsExportGetInputImageDimensions(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetInputImageDimensions");
long GraphicsExportGetInputImageDimensions(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetInputImageDimensions"); return 0; }

long GraphicsExportGetInputOffsetAndLimit(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetInputOffsetAndLimit");
long GraphicsExportGetInputOffsetAndLimit(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetInputOffsetAndLimit"); return 0; }

long GraphicsExportGetInputPicture(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetInputPicture");
long GraphicsExportGetInputPicture(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetInputPicture"); return 0; }

long GraphicsExportGetInputPixmap(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetInputPixmap");
long GraphicsExportGetInputPixmap(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetInputPixmap"); return 0; }

long GraphicsExportGetInterlaceStyle(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetInterlaceStyle");
long GraphicsExportGetInterlaceStyle(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetInterlaceStyle"); return 0; }

long GraphicsExportGetMIMETypeList(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetMIMETypeList");
long GraphicsExportGetMIMETypeList(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetMIMETypeList"); return 0; }

long GraphicsExportGetMetaData(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetMetaData");
long GraphicsExportGetMetaData(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetMetaData"); return 0; }

long GraphicsExportGetOutputDataReference(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetOutputDataReference");
long GraphicsExportGetOutputDataReference(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetOutputDataReference"); return 0; }

long GraphicsExportGetOutputFile(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetOutputFile");
long GraphicsExportGetOutputFile(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetOutputFile"); return 0; }

long GraphicsExportGetOutputFileTypeAndCreator(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetOutputFileTypeAndCreator");
long GraphicsExportGetOutputFileTypeAndCreator(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetOutputFileTypeAndCreator"); return 0; }

long GraphicsExportGetOutputHandle(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetOutputHandle");
long GraphicsExportGetOutputHandle(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetOutputHandle"); return 0; }

long GraphicsExportGetOutputMark(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetOutputMark");
long GraphicsExportGetOutputMark(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetOutputMark"); return 0; }

long GraphicsExportGetOutputOffsetAndMaxSize(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetOutputOffsetAndMaxSize");
long GraphicsExportGetOutputOffsetAndMaxSize(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetOutputOffsetAndMaxSize"); return 0; }

long GraphicsExportGetResolution(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetResolution");
long GraphicsExportGetResolution(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetResolution"); return 0; }

long GraphicsExportGetSettingsAsAtomContainer(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetSettingsAsAtomContainer");
long GraphicsExportGetSettingsAsAtomContainer(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetSettingsAsAtomContainer"); return 0; }

long GraphicsExportGetSettingsAsText(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetSettingsAsText");
long GraphicsExportGetSettingsAsText(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetSettingsAsText"); return 0; }

long GraphicsExportGetTargetDataSize(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetTargetDataSize");
long GraphicsExportGetTargetDataSize(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetTargetDataSize"); return 0; }

long GraphicsExportGetThumbnailEnabled(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportGetThumbnailEnabled");
long GraphicsExportGetThumbnailEnabled(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportGetThumbnailEnabled"); return 0; }

long GraphicsExportMayExporterReadInputData(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportMayExporterReadInputData");
long GraphicsExportMayExporterReadInputData(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportMayExporterReadInputData"); return 0; }

long GraphicsExportReadInputData(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportReadInputData");
long GraphicsExportReadInputData(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportReadInputData"); return 0; }

long GraphicsExportReadOutputData(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportReadOutputData");
long GraphicsExportReadOutputData(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportReadOutputData"); return 0; }

long GraphicsExportSetColorSyncProfile(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportSetColorSyncProfile");
long GraphicsExportSetColorSyncProfile(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportSetColorSyncProfile"); return 0; }

long GraphicsExportSetCompressionMethod(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportSetCompressionMethod");
long GraphicsExportSetCompressionMethod(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportSetCompressionMethod"); return 0; }

long GraphicsExportSetCompressionQuality(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportSetCompressionQuality");
long GraphicsExportSetCompressionQuality(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportSetCompressionQuality"); return 0; }

long GraphicsExportSetDepth(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportSetDepth");
long GraphicsExportSetDepth(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportSetDepth"); return 0; }

long GraphicsExportSetDontRecompress(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportSetDontRecompress");
long GraphicsExportSetDontRecompress(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportSetDontRecompress"); return 0; }

long GraphicsExportSetExifEnabled(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportSetExifEnabled");
long GraphicsExportSetExifEnabled(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportSetExifEnabled"); return 0; }

long GraphicsExportSetInputDataReference(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportSetInputDataReference");
long GraphicsExportSetInputDataReference(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportSetInputDataReference"); return 0; }

long GraphicsExportSetInputFile(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportSetInputFile");
long GraphicsExportSetInputFile(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportSetInputFile"); return 0; }

long GraphicsExportSetInputGWorld(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportSetInputGWorld");
long GraphicsExportSetInputGWorld(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportSetInputGWorld"); return 0; }

long GraphicsExportSetInputGraphicsImporter(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportSetInputGraphicsImporter");
long GraphicsExportSetInputGraphicsImporter(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportSetInputGraphicsImporter"); return 0; }

long GraphicsExportSetInputHandle(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportSetInputHandle");
long GraphicsExportSetInputHandle(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportSetInputHandle"); return 0; }

long GraphicsExportSetInputOffsetAndLimit(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportSetInputOffsetAndLimit");
long GraphicsExportSetInputOffsetAndLimit(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportSetInputOffsetAndLimit"); return 0; }

long GraphicsExportSetInputPicture(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportSetInputPicture");
long GraphicsExportSetInputPicture(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportSetInputPicture"); return 0; }

long GraphicsExportSetInputPixmap(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportSetInputPixmap");
long GraphicsExportSetInputPixmap(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportSetInputPixmap"); return 0; }

long GraphicsExportSetInputPtr(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportSetInputPtr");
long GraphicsExportSetInputPtr(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportSetInputPtr"); return 0; }

long GraphicsExportSetInterlaceStyle(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportSetInterlaceStyle");
long GraphicsExportSetInterlaceStyle(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportSetInterlaceStyle"); return 0; }

long GraphicsExportSetMetaData(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportSetMetaData");
long GraphicsExportSetMetaData(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportSetMetaData"); return 0; }

long GraphicsExportSetOutputDataReference(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportSetOutputDataReference");
long GraphicsExportSetOutputDataReference(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportSetOutputDataReference"); return 0; }

long GraphicsExportSetOutputFile(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportSetOutputFile");
long GraphicsExportSetOutputFile(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportSetOutputFile"); return 0; }

long GraphicsExportSetOutputFileTypeAndCreator(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportSetOutputFileTypeAndCreator");
long GraphicsExportSetOutputFileTypeAndCreator(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportSetOutputFileTypeAndCreator"); return 0; }

long GraphicsExportSetOutputHandle(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportSetOutputHandle");
long GraphicsExportSetOutputHandle(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportSetOutputHandle"); return 0; }

long GraphicsExportSetOutputMark(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportSetOutputMark");
long GraphicsExportSetOutputMark(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportSetOutputMark"); return 0; }

long GraphicsExportSetOutputOffsetAndMaxSize(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportSetOutputOffsetAndMaxSize");
long GraphicsExportSetOutputOffsetAndMaxSize(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportSetOutputOffsetAndMaxSize"); return 0; }

long GraphicsExportSetResolution(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportSetResolution");
long GraphicsExportSetResolution(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportSetResolution"); return 0; }

long GraphicsExportSetSettingsFromAtomContainer(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportSetSettingsFromAtomContainer");
long GraphicsExportSetSettingsFromAtomContainer(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportSetSettingsFromAtomContainer"); return 0; }

long GraphicsExportSetTargetDataSize(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportSetTargetDataSize");
long GraphicsExportSetTargetDataSize(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportSetTargetDataSize"); return 0; }

long GraphicsExportSetThumbnailEnabled(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsExportSetThumbnailEnabled");
long GraphicsExportSetThumbnailEnabled(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsExportSetThumbnailEnabled"); return 0; }

long GraphicsImageImportGetSequenceEnabled(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImageImportGetSequenceEnabled");
long GraphicsImageImportGetSequenceEnabled(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImageImportGetSequenceEnabled"); return 0; }

long GraphicsImageImportSetSequenceEnabled(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImageImportSetSequenceEnabled");
long GraphicsImageImportSetSequenceEnabled(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImageImportSetSequenceEnabled"); return 0; }

long GraphicsImportDoesDrawAllPixels(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportDoesDrawAllPixels");
long GraphicsImportDoesDrawAllPixels(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportDoesDrawAllPixels"); return 0; }

long GraphicsImportExportImageFile(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportExportImageFile");
long GraphicsImportExportImageFile(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportExportImageFile"); return 0; }

long GraphicsImportGetAliasedDataReference(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportGetAliasedDataReference");
long GraphicsImportGetAliasedDataReference(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportGetAliasedDataReference"); return 0; }

long GraphicsImportGetAsPicture(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportGetAsPicture");
long GraphicsImportGetAsPicture(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportGetAsPicture"); return 0; }

long GraphicsImportGetBaseDataOffsetAndSize64(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportGetBaseDataOffsetAndSize64");
long GraphicsImportGetBaseDataOffsetAndSize64(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportGetBaseDataOffsetAndSize64"); return 0; }

long GraphicsImportGetClip(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportGetClip");
long GraphicsImportGetClip(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportGetClip"); return 0; }

long GraphicsImportGetDataFile(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportGetDataFile");
long GraphicsImportGetDataFile(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportGetDataFile"); return 0; }

long GraphicsImportGetDataHandle(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportGetDataHandle");
long GraphicsImportGetDataHandle(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportGetDataHandle"); return 0; }

long GraphicsImportGetDataOffsetAndSize64(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportGetDataOffsetAndSize64");
long GraphicsImportGetDataOffsetAndSize64(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportGetDataOffsetAndSize64"); return 0; }

long GraphicsImportGetDataReference(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportGetDataReference");
long GraphicsImportGetDataReference(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportGetDataReference"); return 0; }

long GraphicsImportGetDataReferenceOffsetAndLimit(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportGetDataReferenceOffsetAndLimit");
long GraphicsImportGetDataReferenceOffsetAndLimit(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportGetDataReferenceOffsetAndLimit"); return 0; }

long GraphicsImportGetDataReferenceOffsetAndLimit64(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportGetDataReferenceOffsetAndLimit64");
long GraphicsImportGetDataReferenceOffsetAndLimit64(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportGetDataReferenceOffsetAndLimit64"); return 0; }

long GraphicsImportGetDefaultClip(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportGetDefaultClip");
long GraphicsImportGetDefaultClip(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportGetDefaultClip"); return 0; }

long GraphicsImportGetDefaultGraphicsMode(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportGetDefaultGraphicsMode");
long GraphicsImportGetDefaultGraphicsMode(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportGetDefaultGraphicsMode"); return 0; }

long GraphicsImportGetDefaultSourceRect(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportGetDefaultSourceRect");
long GraphicsImportGetDefaultSourceRect(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportGetDefaultSourceRect"); return 0; }

long GraphicsImportGetDestRect(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportGetDestRect");
long GraphicsImportGetDestRect(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportGetDestRect"); return 0; }

long GraphicsImportGetExportImageTypeList(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportGetExportImageTypeList");
long GraphicsImportGetExportImageTypeList(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportGetExportImageTypeList"); return 0; }

long GraphicsImportGetExportSettingsAsAtomContainer(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportGetExportSettingsAsAtomContainer");
long GraphicsImportGetExportSettingsAsAtomContainer(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportGetExportSettingsAsAtomContainer"); return 0; }

long GraphicsImportGetFlags(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportGetFlags");
long GraphicsImportGetFlags(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportGetFlags"); return 0; }

long GraphicsImportGetGWorld(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportGetGWorld");
long GraphicsImportGetGWorld(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportGetGWorld"); return 0; }

long GraphicsImportGetGraphicsMode(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportGetGraphicsMode");
long GraphicsImportGetGraphicsMode(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportGetGraphicsMode"); return 0; }

long GraphicsImportGetImageCount(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportGetImageCount");
long GraphicsImportGetImageCount(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportGetImageCount"); return 0; }

long GraphicsImportGetImageIndex(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportGetImageIndex");
long GraphicsImportGetImageIndex(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportGetImageIndex"); return 0; }

long GraphicsImportGetMIMETypeList(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportGetMIMETypeList");
long GraphicsImportGetMIMETypeList(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportGetMIMETypeList"); return 0; }

long GraphicsImportGetQuality(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportGetQuality");
long GraphicsImportGetQuality(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportGetQuality"); return 0; }

long GraphicsImportGetSourceRect(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportGetSourceRect");
long GraphicsImportGetSourceRect(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportGetSourceRect"); return 0; }

long GraphicsImportReadData64(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportReadData64");
long GraphicsImportReadData64(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportReadData64"); return 0; }

long GraphicsImportSaveAsPicture(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportSaveAsPicture");
long GraphicsImportSaveAsPicture(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportSaveAsPicture"); return 0; }

long GraphicsImportSaveAsQuickTimeImageFile(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportSaveAsQuickTimeImageFile");
long GraphicsImportSaveAsQuickTimeImageFile(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportSaveAsQuickTimeImageFile"); return 0; }

long GraphicsImportSetClip(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportSetClip");
long GraphicsImportSetClip(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportSetClip"); return 0; }

long GraphicsImportSetDataReferenceOffsetAndLimit(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportSetDataReferenceOffsetAndLimit");
long GraphicsImportSetDataReferenceOffsetAndLimit(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportSetDataReferenceOffsetAndLimit"); return 0; }

long GraphicsImportSetDataReferenceOffsetAndLimit64(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportSetDataReferenceOffsetAndLimit64");
long GraphicsImportSetDataReferenceOffsetAndLimit64(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportSetDataReferenceOffsetAndLimit64"); return 0; }

long GraphicsImportSetDestRect(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportSetDestRect");
long GraphicsImportSetDestRect(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportSetDestRect"); return 0; }

long GraphicsImportSetExportSettingsFromAtomContainer(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportSetExportSettingsFromAtomContainer");
long GraphicsImportSetExportSettingsFromAtomContainer(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportSetExportSettingsFromAtomContainer"); return 0; }

long GraphicsImportSetGraphicsMode(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportSetGraphicsMode");
long GraphicsImportSetGraphicsMode(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportSetGraphicsMode"); return 0; }

long GraphicsImportSetImageIndex(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportSetImageIndex");
long GraphicsImportSetImageIndex(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportSetImageIndex"); return 0; }

long GraphicsImportSetImageIndexToThumbnail(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportSetImageIndexToThumbnail");
long GraphicsImportSetImageIndexToThumbnail(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportSetImageIndexToThumbnail"); return 0; }

long GraphicsImportSetSourceRect(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportSetSourceRect");
long GraphicsImportSetSourceRect(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportSetSourceRect"); return 0; }

long GraphicsImportValidate(long a, long b, long c_, long d, long e, long f) __asm("_GraphicsImportValidate");
long GraphicsImportValidate(long a, long b, long c_, long d, long e, long f) { shim_note("_GraphicsImportValidate"); return 0; }

long HasMovieChanged(long a, long b, long c_, long d, long e, long f) __asm("_HasMovieChanged");
long HasMovieChanged(long a, long b, long c_, long d, long e, long f) { shim_note("_HasMovieChanged"); return 0; }

long ImageTranscoderBeginSequence(long a, long b, long c_, long d, long e, long f) __asm("_ImageTranscoderBeginSequence");
long ImageTranscoderBeginSequence(long a, long b, long c_, long d, long e, long f) { shim_note("_ImageTranscoderBeginSequence"); return 0; }

long ImageTranscoderDisposeData(long a, long b, long c_, long d, long e, long f) __asm("_ImageTranscoderDisposeData");
long ImageTranscoderDisposeData(long a, long b, long c_, long d, long e, long f) { shim_note("_ImageTranscoderDisposeData"); return 0; }

long ImageTranscoderEndSequence(long a, long b, long c_, long d, long e, long f) __asm("_ImageTranscoderEndSequence");
long ImageTranscoderEndSequence(long a, long b, long c_, long d, long e, long f) { shim_note("_ImageTranscoderEndSequence"); return 0; }

long InsertEmptyMovieSegment(long a, long b, long c_, long d, long e, long f) __asm("_InsertEmptyMovieSegment");
long InsertEmptyMovieSegment(long a, long b, long c_, long d, long e, long f) { shim_note("_InsertEmptyMovieSegment"); return 0; }

long InsertEmptyTrackSegment(long a, long b, long c_, long d, long e, long f) __asm("_InsertEmptyTrackSegment");
long InsertEmptyTrackSegment(long a, long b, long c_, long d, long e, long f) { shim_note("_InsertEmptyTrackSegment"); return 0; }

long InsertMediaIntoTrack(long a, long b, long c_, long d, long e, long f) __asm("_InsertMediaIntoTrack");
long InsertMediaIntoTrack(long a, long b, long c_, long d, long e, long f) { shim_note("_InsertMediaIntoTrack"); return 0; }

long InsertMovieSegment(long a, long b, long c_, long d, long e, long f) __asm("_InsertMovieSegment");
long InsertMovieSegment(long a, long b, long c_, long d, long e, long f) { shim_note("_InsertMovieSegment"); return 0; }

long InsertTrackSegment(long a, long b, long c_, long d, long e, long f) __asm("_InsertTrackSegment");
long InsertTrackSegment(long a, long b, long c_, long d, long e, long f) { shim_note("_InsertTrackSegment"); return 0; }

long InvalidateMovieRegion(long a, long b, long c_, long d, long e, long f) __asm("_InvalidateMovieRegion");
long InvalidateMovieRegion(long a, long b, long c_, long d, long e, long f) { shim_note("_InvalidateMovieRegion"); return 0; }

long IsScrapMovie(long a, long b, long c_, long d, long e, long f) __asm("_IsScrapMovie");
long IsScrapMovie(long a, long b, long c_, long d, long e, long f) { shim_note("_IsScrapMovie"); return 0; }

long LoadMediaIntoRam(long a, long b, long c_, long d, long e, long f) __asm("_LoadMediaIntoRam");
long LoadMediaIntoRam(long a, long b, long c_, long d, long e, long f) { shim_note("_LoadMediaIntoRam"); return 0; }

long LoadMovieIntoRam(long a, long b, long c_, long d, long e, long f) __asm("_LoadMovieIntoRam");
long LoadMovieIntoRam(long a, long b, long c_, long d, long e, long f) { shim_note("_LoadMovieIntoRam"); return 0; }

long LoadTrackIntoRam(long a, long b, long c_, long d, long e, long f) __asm("_LoadTrackIntoRam");
long LoadTrackIntoRam(long a, long b, long c_, long d, long e, long f) { shim_note("_LoadTrackIntoRam"); return 0; }

long MCActivate(long a, long b, long c_, long d, long e, long f) __asm("_MCActivate");
long MCActivate(long a, long b, long c_, long d, long e, long f) { shim_note("_MCActivate"); return 0; }

long MCAddMovieSegment(long a, long b, long c_, long d, long e, long f) __asm("_MCAddMovieSegment");
long MCAddMovieSegment(long a, long b, long c_, long d, long e, long f) { shim_note("_MCAddMovieSegment"); return 0; }

long MCAdjustCursor(long a, long b, long c_, long d, long e, long f) __asm("_MCAdjustCursor");
long MCAdjustCursor(long a, long b, long c_, long d, long e, long f) { shim_note("_MCAdjustCursor"); return 0; }

long MCClear(long a, long b, long c_, long d, long e, long f) __asm("_MCClear");
long MCClear(long a, long b, long c_, long d, long e, long f) { shim_note("_MCClear"); return 0; }

long MCClick(long a, long b, long c_, long d, long e, long f) __asm("_MCClick");
long MCClick(long a, long b, long c_, long d, long e, long f) { shim_note("_MCClick"); return 0; }

long MCCopy(long a, long b, long c_, long d, long e, long f) __asm("_MCCopy");
long MCCopy(long a, long b, long c_, long d, long e, long f) { shim_note("_MCCopy"); return 0; }

long MCCut(long a, long b, long c_, long d, long e, long f) __asm("_MCCut");
long MCCut(long a, long b, long c_, long d, long e, long f) { shim_note("_MCCut"); return 0; }

long MCDoAction(long a, long b, long c_, long d, long e, long f) __asm("_MCDoAction");
long MCDoAction(long a, long b, long c_, long d, long e, long f) { shim_note("_MCDoAction"); return 0; }

long MCDraw(long a, long b, long c_, long d, long e, long f) __asm("_MCDraw");
long MCDraw(long a, long b, long c_, long d, long e, long f) { shim_note("_MCDraw"); return 0; }

long MCDrawBadge(long a, long b, long c_, long d, long e, long f) __asm("_MCDrawBadge");
long MCDrawBadge(long a, long b, long c_, long d, long e, long f) { shim_note("_MCDrawBadge"); return 0; }

long MCEnableEditing(long a, long b, long c_, long d, long e, long f) __asm("_MCEnableEditing");
long MCEnableEditing(long a, long b, long c_, long d, long e, long f) { shim_note("_MCEnableEditing"); return 0; }

long MCGetClip(long a, long b, long c_, long d, long e, long f) __asm("_MCGetClip");
long MCGetClip(long a, long b, long c_, long d, long e, long f) { shim_note("_MCGetClip"); return 0; }

long MCGetControllerBoundsRect(long a, long b, long c_, long d, long e, long f) __asm("_MCGetControllerBoundsRect");
long MCGetControllerBoundsRect(long a, long b, long c_, long d, long e, long f) { shim_note("_MCGetControllerBoundsRect"); return 0; }

long MCGetControllerBoundsRgn(long a, long b, long c_, long d, long e, long f) __asm("_MCGetControllerBoundsRgn");
long MCGetControllerBoundsRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_MCGetControllerBoundsRgn"); return 0; }

long MCGetControllerInfo(long a, long b, long c_, long d, long e, long f) __asm("_MCGetControllerInfo");
long MCGetControllerInfo(long a, long b, long c_, long d, long e, long f) { shim_note("_MCGetControllerInfo"); return 0; }

long MCGetControllerPort(long a, long b, long c_, long d, long e, long f) __asm("_MCGetControllerPort");
long MCGetControllerPort(long a, long b, long c_, long d, long e, long f) { shim_note("_MCGetControllerPort"); return 0; }

long MCGetCurrentTime(long a, long b, long c_, long d, long e, long f) __asm("_MCGetCurrentTime");
long MCGetCurrentTime(long a, long b, long c_, long d, long e, long f) { shim_note("_MCGetCurrentTime"); return 0; }

long MCGetIndMovie(long a, long b, long c_, long d, long e, long f) __asm("_MCGetIndMovie");
long MCGetIndMovie(long a, long b, long c_, long d, long e, long f) { shim_note("_MCGetIndMovie"); return 0; }

long MCGetInterfaceElement(long a, long b, long c_, long d, long e, long f) __asm("_MCGetInterfaceElement");
long MCGetInterfaceElement(long a, long b, long c_, long d, long e, long f) { shim_note("_MCGetInterfaceElement"); return 0; }

long MCGetMenuString(long a, long b, long c_, long d, long e, long f) __asm("_MCGetMenuString");
long MCGetMenuString(long a, long b, long c_, long d, long e, long f) { shim_note("_MCGetMenuString"); return 0; }

long MCGetVisible(long a, long b, long c_, long d, long e, long f) __asm("_MCGetVisible");
long MCGetVisible(long a, long b, long c_, long d, long e, long f) { shim_note("_MCGetVisible"); return 0; }

long MCGetWindowRgn(long a, long b, long c_, long d, long e, long f) __asm("_MCGetWindowRgn");
long MCGetWindowRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_MCGetWindowRgn"); return 0; }

long MCIdle(long a, long b, long c_, long d, long e, long f) __asm("_MCIdle");
long MCIdle(long a, long b, long c_, long d, long e, long f) { shim_note("_MCIdle"); return 0; }

long MCInvalidate(long a, long b, long c_, long d, long e, long f) __asm("_MCInvalidate");
long MCInvalidate(long a, long b, long c_, long d, long e, long f) { shim_note("_MCInvalidate"); return 0; }

long MCIsControllerAttached(long a, long b, long c_, long d, long e, long f) __asm("_MCIsControllerAttached");
long MCIsControllerAttached(long a, long b, long c_, long d, long e, long f) { shim_note("_MCIsControllerAttached"); return 0; }

long MCIsEditingEnabled(long a, long b, long c_, long d, long e, long f) __asm("_MCIsEditingEnabled");
long MCIsEditingEnabled(long a, long b, long c_, long d, long e, long f) { shim_note("_MCIsEditingEnabled"); return 0; }

long MCIsPlayerEvent(long a, long b, long c_, long d, long e, long f) __asm("_MCIsPlayerEvent");
long MCIsPlayerEvent(long a, long b, long c_, long d, long e, long f) { shim_note("_MCIsPlayerEvent"); return 0; }

long MCKey(long a, long b, long c_, long d, long e, long f) __asm("_MCKey");
long MCKey(long a, long b, long c_, long d, long e, long f) { shim_note("_MCKey"); return 0; }

long MCMovieChanged(long a, long b, long c_, long d, long e, long f) __asm("_MCMovieChanged");
long MCMovieChanged(long a, long b, long c_, long d, long e, long f) { shim_note("_MCMovieChanged"); return 0; }

long MCNewAttachedController(long a, long b, long c_, long d, long e, long f) __asm("_MCNewAttachedController");
long MCNewAttachedController(long a, long b, long c_, long d, long e, long f) { shim_note("_MCNewAttachedController"); return 0; }

long MCPaste(long a, long b, long c_, long d, long e, long f) __asm("_MCPaste");
long MCPaste(long a, long b, long c_, long d, long e, long f) { shim_note("_MCPaste"); return 0; }

long MCPositionController(long a, long b, long c_, long d, long e, long f) __asm("_MCPositionController");
long MCPositionController(long a, long b, long c_, long d, long e, long f) { shim_note("_MCPositionController"); return 0; }

long MCPtInController(long a, long b, long c_, long d, long e, long f) __asm("_MCPtInController");
long MCPtInController(long a, long b, long c_, long d, long e, long f) { shim_note("_MCPtInController"); return 0; }

long MCRemoveAMovie(long a, long b, long c_, long d, long e, long f) __asm("_MCRemoveAMovie");
long MCRemoveAMovie(long a, long b, long c_, long d, long e, long f) { shim_note("_MCRemoveAMovie"); return 0; }

long MCRemoveAllMovies(long a, long b, long c_, long d, long e, long f) __asm("_MCRemoveAllMovies");
long MCRemoveAllMovies(long a, long b, long c_, long d, long e, long f) { shim_note("_MCRemoveAllMovies"); return 0; }

long MCRemoveMovie(long a, long b, long c_, long d, long e, long f) __asm("_MCRemoveMovie");
long MCRemoveMovie(long a, long b, long c_, long d, long e, long f) { shim_note("_MCRemoveMovie"); return 0; }

long MCSetClip(long a, long b, long c_, long d, long e, long f) __asm("_MCSetClip");
long MCSetClip(long a, long b, long c_, long d, long e, long f) { shim_note("_MCSetClip"); return 0; }

long MCSetControllerAttached(long a, long b, long c_, long d, long e, long f) __asm("_MCSetControllerAttached");
long MCSetControllerAttached(long a, long b, long c_, long d, long e, long f) { shim_note("_MCSetControllerAttached"); return 0; }

long MCSetControllerBoundsRect(long a, long b, long c_, long d, long e, long f) __asm("_MCSetControllerBoundsRect");
long MCSetControllerBoundsRect(long a, long b, long c_, long d, long e, long f) { shim_note("_MCSetControllerBoundsRect"); return 0; }

long MCSetControllerCapabilities(long a, long b, long c_, long d, long e, long f) __asm("_MCSetControllerCapabilities");
long MCSetControllerCapabilities(long a, long b, long c_, long d, long e, long f) { shim_note("_MCSetControllerCapabilities"); return 0; }

long MCSetControllerPort(long a, long b, long c_, long d, long e, long f) __asm("_MCSetControllerPort");
long MCSetControllerPort(long a, long b, long c_, long d, long e, long f) { shim_note("_MCSetControllerPort"); return 0; }

long MCSetDuration(long a, long b, long c_, long d, long e, long f) __asm("_MCSetDuration");
long MCSetDuration(long a, long b, long c_, long d, long e, long f) { shim_note("_MCSetDuration"); return 0; }

long MCSetIdleManager(long a, long b, long c_, long d, long e, long f) __asm("_MCSetIdleManager");
long MCSetIdleManager(long a, long b, long c_, long d, long e, long f) { shim_note("_MCSetIdleManager"); return 0; }

long MCSetMovie(long a, long b, long c_, long d, long e, long f) __asm("_MCSetMovie");
long MCSetMovie(long a, long b, long c_, long d, long e, long f) { shim_note("_MCSetMovie"); return 0; }

long MCSetUpEditMenu(long a, long b, long c_, long d, long e, long f) __asm("_MCSetUpEditMenu");
long MCSetUpEditMenu(long a, long b, long c_, long d, long e, long f) { shim_note("_MCSetUpEditMenu"); return 0; }

long MCSetVisible(long a, long b, long c_, long d, long e, long f) __asm("_MCSetVisible");
long MCSetVisible(long a, long b, long c_, long d, long e, long f) { shim_note("_MCSetVisible"); return 0; }

long MCTrimMovieSegment(long a, long b, long c_, long d, long e, long f) __asm("_MCTrimMovieSegment");
long MCTrimMovieSegment(long a, long b, long c_, long d, long e, long f) { shim_note("_MCTrimMovieSegment"); return 0; }

long MCUndo(long a, long b, long c_, long d, long e, long f) __asm("_MCUndo");
long MCUndo(long a, long b, long c_, long d, long e, long f) { shim_note("_MCUndo"); return 0; }

long MIDIImportGetSettings(long a, long b, long c_, long d, long e, long f) __asm("_MIDIImportGetSettings");
long MIDIImportGetSettings(long a, long b, long c_, long d, long e, long f) { shim_note("_MIDIImportGetSettings"); return 0; }

long MIDIImportSetSettings(long a, long b, long c_, long d, long e, long f) __asm("_MIDIImportSetSettings");
long MIDIImportSetSettings(long a, long b, long c_, long d, long e, long f) { shim_note("_MIDIImportSetSettings"); return 0; }

long MakeImageDescriptionForEffect(long a, long b, long c_, long d, long e, long f) __asm("_MakeImageDescriptionForEffect");
long MakeImageDescriptionForEffect(long a, long b, long c_, long d, long e, long f) { shim_note("_MakeImageDescriptionForEffect"); return 0; }

long MakeImageDescriptionForPixMap(long a, long b, long c_, long d, long e, long f) __asm("_MakeImageDescriptionForPixMap");
long MakeImageDescriptionForPixMap(long a, long b, long c_, long d, long e, long f) { shim_note("_MakeImageDescriptionForPixMap"); return 0; }

long Media3DGetCameraAngleAspect(long a, long b, long c_, long d, long e, long f) __asm("_Media3DGetCameraAngleAspect");
long Media3DGetCameraAngleAspect(long a, long b, long c_, long d, long e, long f) { shim_note("_Media3DGetCameraAngleAspect"); return 0; }

long Media3DGetCameraData(long a, long b, long c_, long d, long e, long f) __asm("_Media3DGetCameraData");
long Media3DGetCameraData(long a, long b, long c_, long d, long e, long f) { shim_note("_Media3DGetCameraData"); return 0; }

long Media3DGetCameraRange(long a, long b, long c_, long d, long e, long f) __asm("_Media3DGetCameraRange");
long Media3DGetCameraRange(long a, long b, long c_, long d, long e, long f) { shim_note("_Media3DGetCameraRange"); return 0; }

long Media3DGetCurrentGroup(long a, long b, long c_, long d, long e, long f) __asm("_Media3DGetCurrentGroup");
long Media3DGetCurrentGroup(long a, long b, long c_, long d, long e, long f) { shim_note("_Media3DGetCurrentGroup"); return 0; }

long Media3DRotateNamedObjectTo(long a, long b, long c_, long d, long e, long f) __asm("_Media3DRotateNamedObjectTo");
long Media3DRotateNamedObjectTo(long a, long b, long c_, long d, long e, long f) { shim_note("_Media3DRotateNamedObjectTo"); return 0; }

long Media3DScaleNamedObjectTo(long a, long b, long c_, long d, long e, long f) __asm("_Media3DScaleNamedObjectTo");
long Media3DScaleNamedObjectTo(long a, long b, long c_, long d, long e, long f) { shim_note("_Media3DScaleNamedObjectTo"); return 0; }

long Media3DSetCameraAngleAspect(long a, long b, long c_, long d, long e, long f) __asm("_Media3DSetCameraAngleAspect");
long Media3DSetCameraAngleAspect(long a, long b, long c_, long d, long e, long f) { shim_note("_Media3DSetCameraAngleAspect"); return 0; }

long Media3DSetCameraData(long a, long b, long c_, long d, long e, long f) __asm("_Media3DSetCameraData");
long Media3DSetCameraData(long a, long b, long c_, long d, long e, long f) { shim_note("_Media3DSetCameraData"); return 0; }

long Media3DSetCameraRange(long a, long b, long c_, long d, long e, long f) __asm("_Media3DSetCameraRange");
long Media3DSetCameraRange(long a, long b, long c_, long d, long e, long f) { shim_note("_Media3DSetCameraRange"); return 0; }

long Media3DTranslateNamedObjectTo(long a, long b, long c_, long d, long e, long f) __asm("_Media3DTranslateNamedObjectTo");
long Media3DTranslateNamedObjectTo(long a, long b, long c_, long d, long e, long f) { shim_note("_Media3DTranslateNamedObjectTo"); return 0; }

long MediaChangedNonPrimarySource(long a, long b, long c_, long d, long e, long f) __asm("_MediaChangedNonPrimarySource");
long MediaChangedNonPrimarySource(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaChangedNonPrimarySource"); return 0; }

long MediaCompare(long a, long b, long c_, long d, long e, long f) __asm("_MediaCompare");
long MediaCompare(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaCompare"); return 0; }

long MediaCurrentMediaQueuedData(long a, long b, long c_, long d, long e, long f) __asm("_MediaCurrentMediaQueuedData");
long MediaCurrentMediaQueuedData(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaCurrentMediaQueuedData"); return 0; }

long MediaDisposeTargetRefCon(long a, long b, long c_, long d, long e, long f) __asm("_MediaDisposeTargetRefCon");
long MediaDisposeTargetRefCon(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaDisposeTargetRefCon"); return 0; }

long MediaDoIdleActions(long a, long b, long c_, long d, long e, long f) __asm("_MediaDoIdleActions");
long MediaDoIdleActions(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaDoIdleActions"); return 0; }

long MediaEmptyAllPurgeableChunks(long a, long b, long c_, long d, long e, long f) __asm("_MediaEmptyAllPurgeableChunks");
long MediaEmptyAllPurgeableChunks(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaEmptyAllPurgeableChunks"); return 0; }

long MediaEmptySampleCache(long a, long b, long c_, long d, long e, long f) __asm("_MediaEmptySampleCache");
long MediaEmptySampleCache(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaEmptySampleCache"); return 0; }

long MediaEnterEmptyEdit(long a, long b, long c_, long d, long e, long f) __asm("_MediaEnterEmptyEdit");
long MediaEnterEmptyEdit(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaEnterEmptyEdit"); return 0; }

long MediaFlushNonPrimarySourceData(long a, long b, long c_, long d, long e, long f) __asm("_MediaFlushNonPrimarySourceData");
long MediaFlushNonPrimarySourceData(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaFlushNonPrimarySourceData"); return 0; }

long MediaForceUpdate(long a, long b, long c_, long d, long e, long f) __asm("_MediaForceUpdate");
long MediaForceUpdate(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaForceUpdate"); return 0; }

long MediaGGetIdleManager(long a, long b, long c_, long d, long e, long f) __asm("_MediaGGetIdleManager");
long MediaGGetIdleManager(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGGetIdleManager"); return 0; }

long MediaGGetStatus(long a, long b, long c_, long d, long e, long f) __asm("_MediaGGetStatus");
long MediaGGetStatus(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGGetStatus"); return 0; }

long MediaGSetActiveSegment(long a, long b, long c_, long d, long e, long f) __asm("_MediaGSetActiveSegment");
long MediaGSetActiveSegment(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGSetActiveSegment"); return 0; }

long MediaGSetIdleManager(long a, long b, long c_, long d, long e, long f) __asm("_MediaGSetIdleManager");
long MediaGSetIdleManager(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGSetIdleManager"); return 0; }

long MediaGSetVolume(long a, long b, long c_, long d, long e, long f) __asm("_MediaGSetVolume");
long MediaGSetVolume(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGSetVolume"); return 0; }

long MediaGetChunkManagementFlags(long a, long b, long c_, long d, long e, long f) __asm("_MediaGetChunkManagementFlags");
long MediaGetChunkManagementFlags(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGetChunkManagementFlags"); return 0; }

long MediaGetClock(long a, long b, long c_, long d, long e, long f) __asm("_MediaGetClock");
long MediaGetClock(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGetClock"); return 0; }

long MediaGetDrawingRgn(long a, long b, long c_, long d, long e, long f) __asm("_MediaGetDrawingRgn");
long MediaGetDrawingRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGetDrawingRgn"); return 0; }

long MediaGetEffectiveSoundBalance(long a, long b, long c_, long d, long e, long f) __asm("_MediaGetEffectiveSoundBalance");
long MediaGetEffectiveSoundBalance(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGetEffectiveSoundBalance"); return 0; }

long MediaGetEffectiveVolume(long a, long b, long c_, long d, long e, long f) __asm("_MediaGetEffectiveVolume");
long MediaGetEffectiveVolume(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGetEffectiveVolume"); return 0; }

long MediaGetErrorString(long a, long b, long c_, long d, long e, long f) __asm("_MediaGetErrorString");
long MediaGetErrorString(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGetErrorString"); return 0; }

long MediaGetGraphicsMode(long a, long b, long c_, long d, long e, long f) __asm("_MediaGetGraphicsMode");
long MediaGetGraphicsMode(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGetGraphicsMode"); return 0; }

long MediaGetInvalidRegion(long a, long b, long c_, long d, long e, long f) __asm("_MediaGetInvalidRegion");
long MediaGetInvalidRegion(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGetInvalidRegion"); return 0; }

long MediaGetMediaInfo(long a, long b, long c_, long d, long e, long f) __asm("_MediaGetMediaInfo");
long MediaGetMediaInfo(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGetMediaInfo"); return 0; }

long MediaGetMediaLoadState(long a, long b, long c_, long d, long e, long f) __asm("_MediaGetMediaLoadState");
long MediaGetMediaLoadState(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGetMediaLoadState"); return 0; }

long MediaGetName(long a, long b, long c_, long d, long e, long f) __asm("_MediaGetName");
long MediaGetName(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGetName"); return 0; }

long MediaGetNextBoundsChange(long a, long b, long c_, long d, long e, long f) __asm("_MediaGetNextBoundsChange");
long MediaGetNextBoundsChange(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGetNextBoundsChange"); return 0; }

long MediaGetNextStepTime(long a, long b, long c_, long d, long e, long f) __asm("_MediaGetNextStepTime");
long MediaGetNextStepTime(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGetNextStepTime"); return 0; }

long MediaGetOffscreenBufferSize(long a, long b, long c_, long d, long e, long f) __asm("_MediaGetOffscreenBufferSize");
long MediaGetOffscreenBufferSize(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGetOffscreenBufferSize"); return 0; }

long MediaGetPublicInfo(long a, long b, long c_, long d, long e, long f) __asm("_MediaGetPublicInfo");
long MediaGetPublicInfo(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGetPublicInfo"); return 0; }

long MediaGetPurgeableChunkMemoryAllowance(long a, long b, long c_, long d, long e, long f) __asm("_MediaGetPurgeableChunkMemoryAllowance");
long MediaGetPurgeableChunkMemoryAllowance(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGetPurgeableChunkMemoryAllowance"); return 0; }

long MediaGetSoundBalance(long a, long b, long c_, long d, long e, long f) __asm("_MediaGetSoundBalance");
long MediaGetSoundBalance(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGetSoundBalance"); return 0; }

long MediaGetSoundBassAndTreble(long a, long b, long c_, long d, long e, long f) __asm("_MediaGetSoundBassAndTreble");
long MediaGetSoundBassAndTreble(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGetSoundBassAndTreble"); return 0; }

long MediaGetSoundEqualizerBandLevels(long a, long b, long c_, long d, long e, long f) __asm("_MediaGetSoundEqualizerBandLevels");
long MediaGetSoundEqualizerBandLevels(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGetSoundEqualizerBandLevels"); return 0; }

long MediaGetSoundLevelMeteringEnabled(long a, long b, long c_, long d, long e, long f) __asm("_MediaGetSoundLevelMeteringEnabled");
long MediaGetSoundLevelMeteringEnabled(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGetSoundLevelMeteringEnabled"); return 0; }

long MediaGetSoundOutputComponent(long a, long b, long c_, long d, long e, long f) __asm("_MediaGetSoundOutputComponent");
long MediaGetSoundOutputComponent(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGetSoundOutputComponent"); return 0; }

long MediaGetSrcRgn(long a, long b, long c_, long d, long e, long f) __asm("_MediaGetSrcRgn");
long MediaGetSrcRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGetSrcRgn"); return 0; }

long MediaGetTrackOpaque(long a, long b, long c_, long d, long e, long f) __asm("_MediaGetTrackOpaque");
long MediaGetTrackOpaque(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGetTrackOpaque"); return 0; }

long MediaGetURLLink(long a, long b, long c_, long d, long e, long f) __asm("_MediaGetURLLink");
long MediaGetURLLink(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGetURLLink"); return 0; }

long MediaGetVideoParam(long a, long b, long c_, long d, long e, long f) __asm("_MediaGetVideoParam");
long MediaGetVideoParam(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaGetVideoParam"); return 0; }

long MediaHasCharacteristic(long a, long b, long c_, long d, long e, long f) __asm("_MediaHasCharacteristic");
long MediaHasCharacteristic(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaHasCharacteristic"); return 0; }

long MediaHitTestForTargetRefCon(long a, long b, long c_, long d, long e, long f) __asm("_MediaHitTestForTargetRefCon");
long MediaHitTestForTargetRefCon(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaHitTestForTargetRefCon"); return 0; }

long MediaHitTestTargetRefCon(long a, long b, long c_, long d, long e, long f) __asm("_MediaHitTestTargetRefCon");
long MediaHitTestTargetRefCon(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaHitTestTargetRefCon"); return 0; }

long MediaIdle(long a, long b, long c_, long d, long e, long f) __asm("_MediaIdle");
long MediaIdle(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaIdle"); return 0; }

long MediaInvalidateRegion(long a, long b, long c_, long d, long e, long f) __asm("_MediaInvalidateRegion");
long MediaInvalidateRegion(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaInvalidateRegion"); return 0; }

long MediaMCIsPlayerEvent(long a, long b, long c_, long d, long e, long f) __asm("_MediaMCIsPlayerEvent");
long MediaMCIsPlayerEvent(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaMCIsPlayerEvent"); return 0; }

long MediaNavigateTargetRefCon(long a, long b, long c_, long d, long e, long f) __asm("_MediaNavigateTargetRefCon");
long MediaNavigateTargetRefCon(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaNavigateTargetRefCon"); return 0; }

long MediaPrePrerollCancel(long a, long b, long c_, long d, long e, long f) __asm("_MediaPrePrerollCancel");
long MediaPrePrerollCancel(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaPrePrerollCancel"); return 0; }

long MediaPreroll(long a, long b, long c_, long d, long e, long f) __asm("_MediaPreroll");
long MediaPreroll(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaPreroll"); return 0; }

long MediaPutMediaInfo(long a, long b, long c_, long d, long e, long f) __asm("_MediaPutMediaInfo");
long MediaPutMediaInfo(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaPutMediaInfo"); return 0; }

long MediaRefConGetProperty(long a, long b, long c_, long d, long e, long f) __asm("_MediaRefConGetProperty");
long MediaRefConGetProperty(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaRefConGetProperty"); return 0; }

long MediaRefConSetProperty(long a, long b, long c_, long d, long e, long f) __asm("_MediaRefConSetProperty");
long MediaRefConSetProperty(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaRefConSetProperty"); return 0; }

long MediaReleaseSampleDataPointer(long a, long b, long c_, long d, long e, long f) __asm("_MediaReleaseSampleDataPointer");
long MediaReleaseSampleDataPointer(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaReleaseSampleDataPointer"); return 0; }

long MediaSampleDescriptionB2N(long a, long b, long c_, long d, long e, long f) __asm("_MediaSampleDescriptionB2N");
long MediaSampleDescriptionB2N(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaSampleDescriptionB2N"); return 0; }

long MediaSampleDescriptionChanged(long a, long b, long c_, long d, long e, long f) __asm("_MediaSampleDescriptionChanged");
long MediaSampleDescriptionChanged(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaSampleDescriptionChanged"); return 0; }

long MediaSampleDescriptionN2B(long a, long b, long c_, long d, long e, long f) __asm("_MediaSampleDescriptionN2B");
long MediaSampleDescriptionN2B(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaSampleDescriptionN2B"); return 0; }

long MediaSetActive(long a, long b, long c_, long d, long e, long f) __asm("_MediaSetActive");
long MediaSetActive(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaSetActive"); return 0; }

long MediaSetChunkManagementFlags(long a, long b, long c_, long d, long e, long f) __asm("_MediaSetChunkManagementFlags");
long MediaSetChunkManagementFlags(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaSetChunkManagementFlags"); return 0; }

long MediaSetClip(long a, long b, long c_, long d, long e, long f) __asm("_MediaSetClip");
long MediaSetClip(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaSetClip"); return 0; }

long MediaSetDimensions(long a, long b, long c_, long d, long e, long f) __asm("_MediaSetDimensions");
long MediaSetDimensions(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaSetDimensions"); return 0; }

long MediaSetGWorld(long a, long b, long c_, long d, long e, long f) __asm("_MediaSetGWorld");
long MediaSetGWorld(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaSetGWorld"); return 0; }

long MediaSetGraphicsMode(long a, long b, long c_, long d, long e, long f) __asm("_MediaSetGraphicsMode");
long MediaSetGraphicsMode(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaSetGraphicsMode"); return 0; }

long MediaSetHandlerCapabilities(long a, long b, long c_, long d, long e, long f) __asm("_MediaSetHandlerCapabilities");
long MediaSetHandlerCapabilities(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaSetHandlerCapabilities"); return 0; }

long MediaSetHints(long a, long b, long c_, long d, long e, long f) __asm("_MediaSetHints");
long MediaSetHints(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaSetHints"); return 0; }

long MediaSetMediaTimeScale(long a, long b, long c_, long d, long e, long f) __asm("_MediaSetMediaTimeScale");
long MediaSetMediaTimeScale(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaSetMediaTimeScale"); return 0; }

long MediaSetMovieTimeScale(long a, long b, long c_, long d, long e, long f) __asm("_MediaSetMovieTimeScale");
long MediaSetMovieTimeScale(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaSetMovieTimeScale"); return 0; }

long MediaSetPublicInfo(long a, long b, long c_, long d, long e, long f) __asm("_MediaSetPublicInfo");
long MediaSetPublicInfo(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaSetPublicInfo"); return 0; }

long MediaSetPurgeableChunkMemoryAllowance(long a, long b, long c_, long d, long e, long f) __asm("_MediaSetPurgeableChunkMemoryAllowance");
long MediaSetPurgeableChunkMemoryAllowance(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaSetPurgeableChunkMemoryAllowance"); return 0; }

long MediaSetRate(long a, long b, long c_, long d, long e, long f) __asm("_MediaSetRate");
long MediaSetRate(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaSetRate"); return 0; }

long MediaSetScreenLock(long a, long b, long c_, long d, long e, long f) __asm("_MediaSetScreenLock");
long MediaSetScreenLock(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaSetScreenLock"); return 0; }

long MediaSetSoundBalance(long a, long b, long c_, long d, long e, long f) __asm("_MediaSetSoundBalance");
long MediaSetSoundBalance(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaSetSoundBalance"); return 0; }

long MediaSetSoundBassAndTreble(long a, long b, long c_, long d, long e, long f) __asm("_MediaSetSoundBassAndTreble");
long MediaSetSoundBassAndTreble(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaSetSoundBassAndTreble"); return 0; }

long MediaSetSoundLevelMeteringEnabled(long a, long b, long c_, long d, long e, long f) __asm("_MediaSetSoundLevelMeteringEnabled");
long MediaSetSoundLevelMeteringEnabled(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaSetSoundLevelMeteringEnabled"); return 0; }

long MediaSetSoundLocalizationData(long a, long b, long c_, long d, long e, long f) __asm("_MediaSetSoundLocalizationData");
long MediaSetSoundLocalizationData(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaSetSoundLocalizationData"); return 0; }

long MediaSetSoundOutputComponent(long a, long b, long c_, long d, long e, long f) __asm("_MediaSetSoundOutputComponent");
long MediaSetSoundOutputComponent(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaSetSoundOutputComponent"); return 0; }

long MediaSetVideoParam(long a, long b, long c_, long d, long e, long f) __asm("_MediaSetVideoParam");
long MediaSetVideoParam(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaSetVideoParam"); return 0; }

long MediaTargetRefConsEqual(long a, long b, long c_, long d, long e, long f) __asm("_MediaTargetRefConsEqual");
long MediaTargetRefConsEqual(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaTargetRefConsEqual"); return 0; }

long MediaTimeBaseChanged(long a, long b, long c_, long d, long e, long f) __asm("_MediaTimeBaseChanged");
long MediaTimeBaseChanged(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaTimeBaseChanged"); return 0; }

long MediaTimeToSampleNum(long a, long b, long c_, long d, long e, long f) __asm("_MediaTimeToSampleNum");
long MediaTimeToSampleNum(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaTimeToSampleNum"); return 0; }

long MediaTrackEdited(long a, long b, long c_, long d, long e, long f) __asm("_MediaTrackEdited");
long MediaTrackEdited(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaTrackEdited"); return 0; }

long MediaTrackPropertyAtomChanged(long a, long b, long c_, long d, long e, long f) __asm("_MediaTrackPropertyAtomChanged");
long MediaTrackPropertyAtomChanged(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaTrackPropertyAtomChanged"); return 0; }

long MediaTrackReferencesChanged(long a, long b, long c_, long d, long e, long f) __asm("_MediaTrackReferencesChanged");
long MediaTrackReferencesChanged(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaTrackReferencesChanged"); return 0; }

long MediaVideoOutputChanged(long a, long b, long c_, long d, long e, long f) __asm("_MediaVideoOutputChanged");
long MediaVideoOutputChanged(long a, long b, long c_, long d, long e, long f) { shim_note("_MediaVideoOutputChanged"); return 0; }

long MovieExportDoUserDialog(long a, long b, long c_, long d, long e, long f) __asm("_MovieExportDoUserDialog");
long MovieExportDoUserDialog(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieExportDoUserDialog"); return 0; }

long MovieExportFromProceduresToDataRef(long a, long b, long c_, long d, long e, long f) __asm("_MovieExportFromProceduresToDataRef");
long MovieExportFromProceduresToDataRef(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieExportFromProceduresToDataRef"); return 0; }

long MovieExportGetAuxiliaryData(long a, long b, long c_, long d, long e, long f) __asm("_MovieExportGetAuxiliaryData");
long MovieExportGetAuxiliaryData(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieExportGetAuxiliaryData"); return 0; }

long MovieExportGetCreatorType(long a, long b, long c_, long d, long e, long f) __asm("_MovieExportGetCreatorType");
long MovieExportGetCreatorType(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieExportGetCreatorType"); return 0; }

long MovieExportGetFileNameExtension(long a, long b, long c_, long d, long e, long f) __asm("_MovieExportGetFileNameExtension");
long MovieExportGetFileNameExtension(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieExportGetFileNameExtension"); return 0; }

long MovieExportGetShortFileTypeString(long a, long b, long c_, long d, long e, long f) __asm("_MovieExportGetShortFileTypeString");
long MovieExportGetShortFileTypeString(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieExportGetShortFileTypeString"); return 0; }

long MovieExportGetSourceMediaType(long a, long b, long c_, long d, long e, long f) __asm("_MovieExportGetSourceMediaType");
long MovieExportGetSourceMediaType(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieExportGetSourceMediaType"); return 0; }

long MovieExportSetSampleDescription(long a, long b, long c_, long d, long e, long f) __asm("_MovieExportSetSampleDescription");
long MovieExportSetSampleDescription(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieExportSetSampleDescription"); return 0; }

long MovieExportToDataRef(long a, long b, long c_, long d, long e, long f) __asm("_MovieExportToDataRef");
long MovieExportToDataRef(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieExportToDataRef"); return 0; }

long MovieExportToFile(long a, long b, long c_, long d, long e, long f) __asm("_MovieExportToFile");
long MovieExportToFile(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieExportToFile"); return 0; }

long MovieExportToHandle(long a, long b, long c_, long d, long e, long f) __asm("_MovieExportToHandle");
long MovieExportToHandle(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieExportToHandle"); return 0; }

long MovieExportValidate(long a, long b, long c_, long d, long e, long f) __asm("_MovieExportValidate");
long MovieExportValidate(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieExportValidate"); return 0; }

long MovieImportDataRef(long a, long b, long c_, long d, long e, long f) __asm("_MovieImportDataRef");
long MovieImportDataRef(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieImportDataRef"); return 0; }

long MovieImportDoUserDialog(long a, long b, long c_, long d, long e, long f) __asm("_MovieImportDoUserDialog");
long MovieImportDoUserDialog(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieImportDoUserDialog"); return 0; }

long MovieImportEstimateCompletionTime(long a, long b, long c_, long d, long e, long f) __asm("_MovieImportEstimateCompletionTime");
long MovieImportEstimateCompletionTime(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieImportEstimateCompletionTime"); return 0; }

long MovieImportFile(long a, long b, long c_, long d, long e, long f) __asm("_MovieImportFile");
long MovieImportFile(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieImportFile"); return 0; }

long MovieImportGetAuxiliaryDataType(long a, long b, long c_, long d, long e, long f) __asm("_MovieImportGetAuxiliaryDataType");
long MovieImportGetAuxiliaryDataType(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieImportGetAuxiliaryDataType"); return 0; }

long MovieImportGetDestinationMediaType(long a, long b, long c_, long d, long e, long f) __asm("_MovieImportGetDestinationMediaType");
long MovieImportGetDestinationMediaType(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieImportGetDestinationMediaType"); return 0; }

long MovieImportGetDontBlock(long a, long b, long c_, long d, long e, long f) __asm("_MovieImportGetDontBlock");
long MovieImportGetDontBlock(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieImportGetDontBlock"); return 0; }

long MovieImportGetFileType(long a, long b, long c_, long d, long e, long f) __asm("_MovieImportGetFileType");
long MovieImportGetFileType(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieImportGetFileType"); return 0; }

long MovieImportGetLoadState(long a, long b, long c_, long d, long e, long f) __asm("_MovieImportGetLoadState");
long MovieImportGetLoadState(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieImportGetLoadState"); return 0; }

long MovieImportGetMaxLoadedTime(long a, long b, long c_, long d, long e, long f) __asm("_MovieImportGetMaxLoadedTime");
long MovieImportGetMaxLoadedTime(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieImportGetMaxLoadedTime"); return 0; }

long MovieImportGetSampleDescription(long a, long b, long c_, long d, long e, long f) __asm("_MovieImportGetSampleDescription");
long MovieImportGetSampleDescription(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieImportGetSampleDescription"); return 0; }

long MovieImportHandle(long a, long b, long c_, long d, long e, long f) __asm("_MovieImportHandle");
long MovieImportHandle(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieImportHandle"); return 0; }

long MovieImportIdle(long a, long b, long c_, long d, long e, long f) __asm("_MovieImportIdle");
long MovieImportIdle(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieImportIdle"); return 0; }

long MovieImportSetAuxiliaryData(long a, long b, long c_, long d, long e, long f) __asm("_MovieImportSetAuxiliaryData");
long MovieImportSetAuxiliaryData(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieImportSetAuxiliaryData"); return 0; }

long MovieImportSetChunkSize(long a, long b, long c_, long d, long e, long f) __asm("_MovieImportSetChunkSize");
long MovieImportSetChunkSize(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieImportSetChunkSize"); return 0; }

long MovieImportSetDimensions(long a, long b, long c_, long d, long e, long f) __asm("_MovieImportSetDimensions");
long MovieImportSetDimensions(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieImportSetDimensions"); return 0; }

long MovieImportSetDontBlock(long a, long b, long c_, long d, long e, long f) __asm("_MovieImportSetDontBlock");
long MovieImportSetDontBlock(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieImportSetDontBlock"); return 0; }

long MovieImportSetDuration(long a, long b, long c_, long d, long e, long f) __asm("_MovieImportSetDuration");
long MovieImportSetDuration(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieImportSetDuration"); return 0; }

long MovieImportSetFromScrap(long a, long b, long c_, long d, long e, long f) __asm("_MovieImportSetFromScrap");
long MovieImportSetFromScrap(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieImportSetFromScrap"); return 0; }

long MovieImportSetIdleManager(long a, long b, long c_, long d, long e, long f) __asm("_MovieImportSetIdleManager");
long MovieImportSetIdleManager(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieImportSetIdleManager"); return 0; }

long MovieImportSetMediaFile(long a, long b, long c_, long d, long e, long f) __asm("_MovieImportSetMediaFile");
long MovieImportSetMediaFile(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieImportSetMediaFile"); return 0; }

long MovieImportSetNewMovieFlags(long a, long b, long c_, long d, long e, long f) __asm("_MovieImportSetNewMovieFlags");
long MovieImportSetNewMovieFlags(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieImportSetNewMovieFlags"); return 0; }

long MovieImportSetOffsetAndLimit(long a, long b, long c_, long d, long e, long f) __asm("_MovieImportSetOffsetAndLimit");
long MovieImportSetOffsetAndLimit(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieImportSetOffsetAndLimit"); return 0; }

long MovieImportSetOffsetAndLimit64(long a, long b, long c_, long d, long e, long f) __asm("_MovieImportSetOffsetAndLimit64");
long MovieImportSetOffsetAndLimit64(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieImportSetOffsetAndLimit64"); return 0; }

long MovieImportSetSampleDescription(long a, long b, long c_, long d, long e, long f) __asm("_MovieImportSetSampleDescription");
long MovieImportSetSampleDescription(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieImportSetSampleDescription"); return 0; }

long MovieImportSetSampleDuration(long a, long b, long c_, long d, long e, long f) __asm("_MovieImportSetSampleDuration");
long MovieImportSetSampleDuration(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieImportSetSampleDuration"); return 0; }

long MovieImportValidate(long a, long b, long c_, long d, long e, long f) __asm("_MovieImportValidate");
long MovieImportValidate(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieImportValidate"); return 0; }

long MovieImportValidateDataRef(long a, long b, long c_, long d, long e, long f) __asm("_MovieImportValidateDataRef");
long MovieImportValidateDataRef(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieImportValidateDataRef"); return 0; }

long MovieSearchText(long a, long b, long c_, long d, long e, long f) __asm("_MovieSearchText");
long MovieSearchText(long a, long b, long c_, long d, long e, long f) { shim_note("_MovieSearchText"); return 0; }

long MusicDerivedCloseResFile(long a, long b, long c_, long d, long e, long f) __asm("_MusicDerivedCloseResFile");
long MusicDerivedCloseResFile(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicDerivedCloseResFile"); return 0; }

long MusicDerivedMIDISend(long a, long b, long c_, long d, long e, long f) __asm("_MusicDerivedMIDISend");
long MusicDerivedMIDISend(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicDerivedMIDISend"); return 0; }

long MusicDerivedOpenResFile(long a, long b, long c_, long d, long e, long f) __asm("_MusicDerivedOpenResFile");
long MusicDerivedOpenResFile(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicDerivedOpenResFile"); return 0; }

long MusicGenericConfigure(long a, long b, long c_, long d, long e, long f) __asm("_MusicGenericConfigure");
long MusicGenericConfigure(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicGenericConfigure"); return 0; }

long MusicGenericGetKnobList(long a, long b, long c_, long d, long e, long f) __asm("_MusicGenericGetKnobList");
long MusicGenericGetKnobList(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicGenericGetKnobList"); return 0; }

long MusicGenericSetResourceNumbers(long a, long b, long c_, long d, long e, long f) __asm("_MusicGenericSetResourceNumbers");
long MusicGenericSetResourceNumbers(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicGenericSetResourceNumbers"); return 0; }

long MusicGetDeviceConnection(long a, long b, long c_, long d, long e, long f) __asm("_MusicGetDeviceConnection");
long MusicGetDeviceConnection(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicGetDeviceConnection"); return 0; }

long MusicGetDrumNames(long a, long b, long c_, long d, long e, long f) __asm("_MusicGetDrumNames");
long MusicGetDrumNames(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicGetDrumNames"); return 0; }

long MusicGetInfoText(long a, long b, long c_, long d, long e, long f) __asm("_MusicGetInfoText");
long MusicGetInfoText(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicGetInfoText"); return 0; }

long MusicGetInstrumentInfo(long a, long b, long c_, long d, long e, long f) __asm("_MusicGetInstrumentInfo");
long MusicGetInstrumentInfo(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicGetInstrumentInfo"); return 0; }

long MusicGetInstrumentNames(long a, long b, long c_, long d, long e, long f) __asm("_MusicGetInstrumentNames");
long MusicGetInstrumentNames(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicGetInstrumentNames"); return 0; }

long MusicGetKnob(long a, long b, long c_, long d, long e, long f) __asm("_MusicGetKnob");
long MusicGetKnob(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicGetKnob"); return 0; }

long MusicGetKnobSettingStrings(long a, long b, long c_, long d, long e, long f) __asm("_MusicGetKnobSettingStrings");
long MusicGetKnobSettingStrings(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicGetKnobSettingStrings"); return 0; }

long MusicGetMIDIPorts(long a, long b, long c_, long d, long e, long f) __asm("_MusicGetMIDIPorts");
long MusicGetMIDIPorts(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicGetMIDIPorts"); return 0; }

long MusicGetMasterTune(long a, long b, long c_, long d, long e, long f) __asm("_MusicGetMasterTune");
long MusicGetMasterTune(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicGetMasterTune"); return 0; }

long MusicGetPart(long a, long b, long c_, long d, long e, long f) __asm("_MusicGetPart");
long MusicGetPart(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicGetPart"); return 0; }

long MusicGetPartAtomicInstrument(long a, long b, long c_, long d, long e, long f) __asm("_MusicGetPartAtomicInstrument");
long MusicGetPartAtomicInstrument(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicGetPartAtomicInstrument"); return 0; }

long MusicGetPartController(long a, long b, long c_, long d, long e, long f) __asm("_MusicGetPartController");
long MusicGetPartController(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicGetPartController"); return 0; }

long MusicGetPartInstrumentNumber(long a, long b, long c_, long d, long e, long f) __asm("_MusicGetPartInstrumentNumber");
long MusicGetPartInstrumentNumber(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicGetPartInstrumentNumber"); return 0; }

long MusicGetPartKnob(long a, long b, long c_, long d, long e, long f) __asm("_MusicGetPartKnob");
long MusicGetPartKnob(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicGetPartKnob"); return 0; }

long MusicGetPartName(long a, long b, long c_, long d, long e, long f) __asm("_MusicGetPartName");
long MusicGetPartName(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicGetPartName"); return 0; }

long MusicMediaGetIndexedTunePlayer(long a, long b, long c_, long d, long e, long f) __asm("_MusicMediaGetIndexedTunePlayer");
long MusicMediaGetIndexedTunePlayer(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicMediaGetIndexedTunePlayer"); return 0; }

long MusicPlayNote(long a, long b, long c_, long d, long e, long f) __asm("_MusicPlayNote");
long MusicPlayNote(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicPlayNote"); return 0; }

long MusicResetPart(long a, long b, long c_, long d, long e, long f) __asm("_MusicResetPart");
long MusicResetPart(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicResetPart"); return 0; }

long MusicSendMIDI(long a, long b, long c_, long d, long e, long f) __asm("_MusicSendMIDI");
long MusicSendMIDI(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicSendMIDI"); return 0; }

long MusicSetKnob(long a, long b, long c_, long d, long e, long f) __asm("_MusicSetKnob");
long MusicSetKnob(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicSetKnob"); return 0; }

long MusicSetMasterTune(long a, long b, long c_, long d, long e, long f) __asm("_MusicSetMasterTune");
long MusicSetMasterTune(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicSetMasterTune"); return 0; }

long MusicSetOfflineTimeTo(long a, long b, long c_, long d, long e, long f) __asm("_MusicSetOfflineTimeTo");
long MusicSetOfflineTimeTo(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicSetOfflineTimeTo"); return 0; }

long MusicSetPart(long a, long b, long c_, long d, long e, long f) __asm("_MusicSetPart");
long MusicSetPart(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicSetPart"); return 0; }

long MusicSetPartAtomicInstrument(long a, long b, long c_, long d, long e, long f) __asm("_MusicSetPartAtomicInstrument");
long MusicSetPartAtomicInstrument(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicSetPartAtomicInstrument"); return 0; }

long MusicSetPartController(long a, long b, long c_, long d, long e, long f) __asm("_MusicSetPartController");
long MusicSetPartController(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicSetPartController"); return 0; }

long MusicSetPartInstrumentNumber(long a, long b, long c_, long d, long e, long f) __asm("_MusicSetPartInstrumentNumber");
long MusicSetPartInstrumentNumber(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicSetPartInstrumentNumber"); return 0; }

long MusicSetPartInstrumentNumberInterruptSafe(long a, long b, long c_, long d, long e, long f) __asm("_MusicSetPartInstrumentNumberInterruptSafe");
long MusicSetPartInstrumentNumberInterruptSafe(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicSetPartInstrumentNumberInterruptSafe"); return 0; }

long MusicSetPartKnob(long a, long b, long c_, long d, long e, long f) __asm("_MusicSetPartKnob");
long MusicSetPartKnob(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicSetPartKnob"); return 0; }

long MusicSetPartName(long a, long b, long c_, long d, long e, long f) __asm("_MusicSetPartName");
long MusicSetPartName(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicSetPartName"); return 0; }

long MusicSetPartSoundLocalization(long a, long b, long c_, long d, long e, long f) __asm("_MusicSetPartSoundLocalization");
long MusicSetPartSoundLocalization(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicSetPartSoundLocalization"); return 0; }

long MusicStorePartInstrument(long a, long b, long c_, long d, long e, long f) __asm("_MusicStorePartInstrument");
long MusicStorePartInstrument(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicStorePartInstrument"); return 0; }

long MusicTask(long a, long b, long c_, long d, long e, long f) __asm("_MusicTask");
long MusicTask(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicTask"); return 0; }

long MusicUseDeviceConnection(long a, long b, long c_, long d, long e, long f) __asm("_MusicUseDeviceConnection");
long MusicUseDeviceConnection(long a, long b, long c_, long d, long e, long f) { shim_note("_MusicUseDeviceConnection"); return 0; }

long NAGetMIDIPorts(long a, long b, long c_, long d, long e, long f) __asm("_NAGetMIDIPorts");
long NAGetMIDIPorts(long a, long b, long c_, long d, long e, long f) { shim_note("_NAGetMIDIPorts"); return 0; }

long NASaveMusicConfiguration(long a, long b, long c_, long d, long e, long f) __asm("_NASaveMusicConfiguration");
long NASaveMusicConfiguration(long a, long b, long c_, long d, long e, long f) { shim_note("_NASaveMusicConfiguration"); return 0; }

long NATask(long a, long b, long c_, long d, long e, long f) __asm("_NATask");
long NATask(long a, long b, long c_, long d, long e, long f) { shim_note("_NATask"); return 0; }

long NAUnregisterMusicDevice(long a, long b, long c_, long d, long e, long f) __asm("_NAUnregisterMusicDevice");
long NAUnregisterMusicDevice(long a, long b, long c_, long d, long e, long f) { shim_note("_NAUnregisterMusicDevice"); return 0; }

long NewMovie(long a, long b, long c_, long d, long e, long f) __asm("_NewMovie");
long NewMovie(long a, long b, long c_, long d, long e, long f) { shim_note("_NewMovie"); return 0; }

long NewMovieController(long a, long b, long c_, long d, long e, long f) __asm("_NewMovieController");
long NewMovieController(long a, long b, long c_, long d, long e, long f) { shim_note("_NewMovieController"); return 0; }

long NewMovieForDataRefFromHandle(long a, long b, long c_, long d, long e, long f) __asm("_NewMovieForDataRefFromHandle");
long NewMovieForDataRefFromHandle(long a, long b, long c_, long d, long e, long f) { shim_note("_NewMovieForDataRefFromHandle"); return 0; }

long NewMovieFromDataFork(long a, long b, long c_, long d, long e, long f) __asm("_NewMovieFromDataFork");
long NewMovieFromDataFork(long a, long b, long c_, long d, long e, long f) { shim_note("_NewMovieFromDataFork"); return 0; }

long NewMovieFromDataFork64(long a, long b, long c_, long d, long e, long f) __asm("_NewMovieFromDataFork64");
long NewMovieFromDataFork64(long a, long b, long c_, long d, long e, long f) { shim_note("_NewMovieFromDataFork64"); return 0; }

long NewMovieFromFile(long a, long b, long c_, long d, long e, long f) __asm("_NewMovieFromFile");
long NewMovieFromFile(long a, long b, long c_, long d, long e, long f) { shim_note("_NewMovieFromFile"); return 0; }

long NewMovieFromHandle(long a, long b, long c_, long d, long e, long f) __asm("_NewMovieFromHandle");
long NewMovieFromHandle(long a, long b, long c_, long d, long e, long f) { shim_note("_NewMovieFromHandle"); return 0; }

long NewMovieFromScrap(long a, long b, long c_, long d, long e, long f) __asm("_NewMovieFromScrap");
long NewMovieFromScrap(long a, long b, long c_, long d, long e, long f) { shim_note("_NewMovieFromScrap"); return 0; }

long NewMovieFromStorageOffset(long a, long b, long c_, long d, long e, long f) __asm("_NewMovieFromStorageOffset");
long NewMovieFromStorageOffset(long a, long b, long c_, long d, long e, long f) { shim_note("_NewMovieFromStorageOffset"); return 0; }

long NewMovieTrack(long a, long b, long c_, long d, long e, long f) __asm("_NewMovieTrack");
long NewMovieTrack(long a, long b, long c_, long d, long e, long f) { shim_note("_NewMovieTrack"); return 0; }

long NewTimeBase(long a, long b, long c_, long d, long e, long f) __asm("_NewTimeBase");
long NewTimeBase(long a, long b, long c_, long d, long e, long f) { shim_note("_NewTimeBase"); return 0; }

long NewTrackMedia(long a, long b, long c_, long d, long e, long f) __asm("_NewTrackMedia");
long NewTrackMedia(long a, long b, long c_, long d, long e, long f) { shim_note("_NewTrackMedia"); return 0; }

long NewUserData(long a, long b, long c_, long d, long e, long f) __asm("_NewUserData");
long NewUserData(long a, long b, long c_, long d, long e, long f) { shim_note("_NewUserData"); return 0; }

long NewUserDataFromHandle(long a, long b, long c_, long d, long e, long f) __asm("_NewUserDataFromHandle");
long NewUserDataFromHandle(long a, long b, long c_, long d, long e, long f) { shim_note("_NewUserDataFromHandle"); return 0; }

long OpenMovieFile(long a, long b, long c_, long d, long e, long f) __asm("_OpenMovieFile");
long OpenMovieFile(long a, long b, long c_, long d, long e, long f) { shim_note("_OpenMovieFile"); return 0; }

long OpenMovieStorage(long a, long b, long c_, long d, long e, long f) __asm("_OpenMovieStorage");
long OpenMovieStorage(long a, long b, long c_, long d, long e, long f) { shim_note("_OpenMovieStorage"); return 0; }

long PasteHandleIntoMovie(long a, long b, long c_, long d, long e, long f) __asm("_PasteHandleIntoMovie");
long PasteHandleIntoMovie(long a, long b, long c_, long d, long e, long f) { shim_note("_PasteHandleIntoMovie"); return 0; }

long PasteMovieSelection(long a, long b, long c_, long d, long e, long f) __asm("_PasteMovieSelection");
long PasteMovieSelection(long a, long b, long c_, long d, long e, long f) { shim_note("_PasteMovieSelection"); return 0; }

long PreviewEvent(long a, long b, long c_, long d, long e, long f) __asm("_PreviewEvent");
long PreviewEvent(long a, long b, long c_, long d, long e, long f) { shim_note("_PreviewEvent"); return 0; }

long PreviewMakePreviewReference(long a, long b, long c_, long d, long e, long f) __asm("_PreviewMakePreviewReference");
long PreviewMakePreviewReference(long a, long b, long c_, long d, long e, long f) { shim_note("_PreviewMakePreviewReference"); return 0; }

long PreviewShowData(long a, long b, long c_, long d, long e, long f) __asm("_PreviewShowData");
long PreviewShowData(long a, long b, long c_, long d, long e, long f) { shim_note("_PreviewShowData"); return 0; }

long PtInMovie(long a, long b, long c_, long d, long e, long f) __asm("_PtInMovie");
long PtInMovie(long a, long b, long c_, long d, long e, long f) { shim_note("_PtInMovie"); return 0; }

long PtInTrack(long a, long b, long c_, long d, long e, long f) __asm("_PtInTrack");
long PtInTrack(long a, long b, long c_, long d, long e, long f) { shim_note("_PtInTrack"); return 0; }

long PutMovieForDataRefIntoHandle(long a, long b, long c_, long d, long e, long f) __asm("_PutMovieForDataRefIntoHandle");
long PutMovieForDataRefIntoHandle(long a, long b, long c_, long d, long e, long f) { shim_note("_PutMovieForDataRefIntoHandle"); return 0; }

long PutMovieIntoDataFork(long a, long b, long c_, long d, long e, long f) __asm("_PutMovieIntoDataFork");
long PutMovieIntoDataFork(long a, long b, long c_, long d, long e, long f) { shim_note("_PutMovieIntoDataFork"); return 0; }

long PutMovieIntoDataFork64(long a, long b, long c_, long d, long e, long f) __asm("_PutMovieIntoDataFork64");
long PutMovieIntoDataFork64(long a, long b, long c_, long d, long e, long f) { shim_note("_PutMovieIntoDataFork64"); return 0; }

long PutMovieIntoHandle(long a, long b, long c_, long d, long e, long f) __asm("_PutMovieIntoHandle");
long PutMovieIntoHandle(long a, long b, long c_, long d, long e, long f) { shim_note("_PutMovieIntoHandle"); return 0; }

long PutMovieIntoStorage(long a, long b, long c_, long d, long e, long f) __asm("_PutMovieIntoStorage");
long PutMovieIntoStorage(long a, long b, long c_, long d, long e, long f) { shim_note("_PutMovieIntoStorage"); return 0; }

long PutMovieIntoTypedHandle(long a, long b, long c_, long d, long e, long f) __asm("_PutMovieIntoTypedHandle");
long PutMovieIntoTypedHandle(long a, long b, long c_, long d, long e, long f) { shim_note("_PutMovieIntoTypedHandle"); return 0; }

long PutMovieOnScrap(long a, long b, long c_, long d, long e, long f) __asm("_PutMovieOnScrap");
long PutMovieOnScrap(long a, long b, long c_, long d, long e, long f) { shim_note("_PutMovieOnScrap"); return 0; }

long PutUserDataIntoHandle(long a, long b, long c_, long d, long e, long f) __asm("_PutUserDataIntoHandle");
long PutUserDataIntoHandle(long a, long b, long c_, long d, long e, long f) { shim_note("_PutUserDataIntoHandle"); return 0; }

long QTDismissStandardParameterDialog(long a, long b, long c_, long d, long e, long f) __asm("_QTDismissStandardParameterDialog");
long QTDismissStandardParameterDialog(long a, long b, long c_, long d, long e, long f) { shim_note("_QTDismissStandardParameterDialog"); return 0; }

long QTGetDataRefMaxFileOffset(long a, long b, long c_, long d, long e, long f) __asm("_QTGetDataRefMaxFileOffset");
long QTGetDataRefMaxFileOffset(long a, long b, long c_, long d, long e, long f) { shim_note("_QTGetDataRefMaxFileOffset"); return 0; }

long QTGetMIMETypeInfo(long a, long b, long c_, long d, long e, long f) __asm("_QTGetMIMETypeInfo");
long QTGetMIMETypeInfo(long a, long b, long c_, long d, long e, long f) { shim_note("_QTGetMIMETypeInfo"); return 0; }

long QTGetPixMapHandleGammaLevel(long a, long b, long c_, long d, long e, long f) __asm("_QTGetPixMapHandleGammaLevel");
long QTGetPixMapHandleGammaLevel(long a, long b, long c_, long d, long e, long f) { shim_note("_QTGetPixMapHandleGammaLevel"); return 0; }

long QTGetPixMapHandleRequestedGammaLevel(long a, long b, long c_, long d, long e, long f) __asm("_QTGetPixMapHandleRequestedGammaLevel");
long QTGetPixMapHandleRequestedGammaLevel(long a, long b, long c_, long d, long e, long f) { shim_note("_QTGetPixMapHandleRequestedGammaLevel"); return 0; }

long QTGetPixMapHandleRowBytes(long a, long b, long c_, long d, long e, long f) __asm("_QTGetPixMapHandleRowBytes");
long QTGetPixMapHandleRowBytes(long a, long b, long c_, long d, long e, long f) { shim_note("_QTGetPixMapHandleRowBytes"); return 0; }

long QTGetPixelFormatDepthForImageDescription(long a, long b, long c_, long d, long e, long f) __asm("_QTGetPixelFormatDepthForImageDescription");
long QTGetPixelFormatDepthForImageDescription(long a, long b, long c_, long d, long e, long f) { shim_note("_QTGetPixelFormatDepthForImageDescription"); return 0; }

long QTGetPixelSize(long a, long b, long c_, long d, long e, long f) __asm("_QTGetPixelSize");
long QTGetPixelSize(long a, long b, long c_, long d, long e, long f) { shim_note("_QTGetPixelSize"); return 0; }

long QTGetSupportedRestrictions(long a, long b, long c_, long d, long e, long f) __asm("_QTGetSupportedRestrictions");
long QTGetSupportedRestrictions(long a, long b, long c_, long d, long e, long f) { shim_note("_QTGetSupportedRestrictions"); return 0; }

long QTGetTimeUntilNextTask(long a, long b, long c_, long d, long e, long f) __asm("_QTGetTimeUntilNextTask");
long QTGetTimeUntilNextTask(long a, long b, long c_, long d, long e, long f) { shim_note("_QTGetTimeUntilNextTask"); return 0; }

long QTGetWallClockTimeBase(long a, long b, long c_, long d, long e, long f) __asm("_QTGetWallClockTimeBase");
long QTGetWallClockTimeBase(long a, long b, long c_, long d, long e, long f) { shim_note("_QTGetWallClockTimeBase"); return 0; }

long QTIdleManagerOpen(long a, long b, long c_, long d, long e, long f) __asm("_QTIdleManagerOpen");
long QTIdleManagerOpen(long a, long b, long c_, long d, long e, long f) { shim_note("_QTIdleManagerOpen"); return 0; }

long QTIsStandardParameterDialogEvent(long a, long b, long c_, long d, long e, long f) __asm("_QTIsStandardParameterDialogEvent");
long QTIsStandardParameterDialogEvent(long a, long b, long c_, long d, long e, long f) { shim_note("_QTIsStandardParameterDialogEvent"); return 0; }

long QTMIDIGetMIDIPorts(long a, long b, long c_, long d, long e, long f) __asm("_QTMIDIGetMIDIPorts");
long QTMIDIGetMIDIPorts(long a, long b, long c_, long d, long e, long f) { shim_note("_QTMIDIGetMIDIPorts"); return 0; }

long QTMIDISendMIDI(long a, long b, long c_, long d, long e, long f) __asm("_QTMIDISendMIDI");
long QTMIDISendMIDI(long a, long b, long c_, long d, long e, long f) { shim_note("_QTMIDISendMIDI"); return 0; }

long QTMIDIUseSendPort(long a, long b, long c_, long d, long e, long f) __asm("_QTMIDIUseSendPort");
long QTMIDIUseSendPort(long a, long b, long c_, long d, long e, long f) { shim_note("_QTMIDIUseSendPort"); return 0; }

long QTMovieNeedsTimeTable(long a, long b, long c_, long d, long e, long f) __asm("_QTMovieNeedsTimeTable");
long QTMovieNeedsTimeTable(long a, long b, long c_, long d, long e, long f) { shim_note("_QTMovieNeedsTimeTable"); return 0; }

long QTRegisterAccessKey(long a, long b, long c_, long d, long e, long f) __asm("_QTRegisterAccessKey");
long QTRegisterAccessKey(long a, long b, long c_, long d, long e, long f) { shim_note("_QTRegisterAccessKey"); return 0; }

long QTSetPixMapHandleGammaLevel(long a, long b, long c_, long d, long e, long f) __asm("_QTSetPixMapHandleGammaLevel");
long QTSetPixMapHandleGammaLevel(long a, long b, long c_, long d, long e, long f) { shim_note("_QTSetPixMapHandleGammaLevel"); return 0; }

long QTSetPixMapHandleRequestedGammaLevel(long a, long b, long c_, long d, long e, long f) __asm("_QTSetPixMapHandleRequestedGammaLevel");
long QTSetPixMapHandleRequestedGammaLevel(long a, long b, long c_, long d, long e, long f) { shim_note("_QTSetPixMapHandleRequestedGammaLevel"); return 0; }

long QTSetPixMapHandleRowBytes(long a, long b, long c_, long d, long e, long f) __asm("_QTSetPixMapHandleRowBytes");
long QTSetPixMapHandleRowBytes(long a, long b, long c_, long d, long e, long f) { shim_note("_QTSetPixMapHandleRowBytes"); return 0; }

long QTStandardParameterDialogDoAction(long a, long b, long c_, long d, long e, long f) __asm("_QTStandardParameterDialogDoAction");
long QTStandardParameterDialogDoAction(long a, long b, long c_, long d, long e, long f) { shim_note("_QTStandardParameterDialogDoAction"); return 0; }

long QTTextToNativeText(long a, long b, long c_, long d, long e, long f) __asm("_QTTextToNativeText");
long QTTextToNativeText(long a, long b, long c_, long d, long e, long f) { shim_note("_QTTextToNativeText"); return 0; }

long QTUnregisterAccessKey(long a, long b, long c_, long d, long e, long f) __asm("_QTUnregisterAccessKey");
long QTUnregisterAccessKey(long a, long b, long c_, long d, long e, long f) { shim_note("_QTUnregisterAccessKey"); return 0; }

long QTVideoOutputBaseSetEchoPort(long a, long b, long c_, long d, long e, long f) __asm("_QTVideoOutputBaseSetEchoPort");
long QTVideoOutputBaseSetEchoPort(long a, long b, long c_, long d, long e, long f) { shim_note("_QTVideoOutputBaseSetEchoPort"); return 0; }

long QTVideoOutputBegin(long a, long b, long c_, long d, long e, long f) __asm("_QTVideoOutputBegin");
long QTVideoOutputBegin(long a, long b, long c_, long d, long e, long f) { shim_note("_QTVideoOutputBegin"); return 0; }

long QTVideoOutputEnd(long a, long b, long c_, long d, long e, long f) __asm("_QTVideoOutputEnd");
long QTVideoOutputEnd(long a, long b, long c_, long d, long e, long f) { shim_note("_QTVideoOutputEnd"); return 0; }

long QTVideoOutputGetClientName(long a, long b, long c_, long d, long e, long f) __asm("_QTVideoOutputGetClientName");
long QTVideoOutputGetClientName(long a, long b, long c_, long d, long e, long f) { shim_note("_QTVideoOutputGetClientName"); return 0; }

long QTVideoOutputGetClock(long a, long b, long c_, long d, long e, long f) __asm("_QTVideoOutputGetClock");
long QTVideoOutputGetClock(long a, long b, long c_, long d, long e, long f) { shim_note("_QTVideoOutputGetClock"); return 0; }

long QTVideoOutputGetCurrentClientName(long a, long b, long c_, long d, long e, long f) __asm("_QTVideoOutputGetCurrentClientName");
long QTVideoOutputGetCurrentClientName(long a, long b, long c_, long d, long e, long f) { shim_note("_QTVideoOutputGetCurrentClientName"); return 0; }

long QTVideoOutputGetDisplayMode(long a, long b, long c_, long d, long e, long f) __asm("_QTVideoOutputGetDisplayMode");
long QTVideoOutputGetDisplayMode(long a, long b, long c_, long d, long e, long f) { shim_note("_QTVideoOutputGetDisplayMode"); return 0; }

long QTVideoOutputGetGWorld(long a, long b, long c_, long d, long e, long f) __asm("_QTVideoOutputGetGWorld");
long QTVideoOutputGetGWorld(long a, long b, long c_, long d, long e, long f) { shim_note("_QTVideoOutputGetGWorld"); return 0; }

long QTVideoOutputGetIndImageDecompressor(long a, long b, long c_, long d, long e, long f) __asm("_QTVideoOutputGetIndImageDecompressor");
long QTVideoOutputGetIndImageDecompressor(long a, long b, long c_, long d, long e, long f) { shim_note("_QTVideoOutputGetIndImageDecompressor"); return 0; }

long QTVideoOutputGetIndSoundOutput(long a, long b, long c_, long d, long e, long f) __asm("_QTVideoOutputGetIndSoundOutput");
long QTVideoOutputGetIndSoundOutput(long a, long b, long c_, long d, long e, long f) { shim_note("_QTVideoOutputGetIndSoundOutput"); return 0; }

long QTVideoOutputSetClientName(long a, long b, long c_, long d, long e, long f) __asm("_QTVideoOutputSetClientName");
long QTVideoOutputSetClientName(long a, long b, long c_, long d, long e, long f) { shim_note("_QTVideoOutputSetClientName"); return 0; }

long QTVideoOutputSetDisplayMode(long a, long b, long c_, long d, long e, long f) __asm("_QTVideoOutputSetDisplayMode");
long QTVideoOutputSetDisplayMode(long a, long b, long c_, long d, long e, long f) { shim_note("_QTVideoOutputSetDisplayMode"); return 0; }

long QTVideoOutputSetEchoPort(long a, long b, long c_, long d, long e, long f) __asm("_QTVideoOutputSetEchoPort");
long QTVideoOutputSetEchoPort(long a, long b, long c_, long d, long e, long f) { shim_note("_QTVideoOutputSetEchoPort"); return 0; }

long RemoveImageDescriptionExtension(long a, long b, long c_, long d, long e, long f) __asm("_RemoveImageDescriptionExtension");
long RemoveImageDescriptionExtension(long a, long b, long c_, long d, long e, long f) { shim_note("_RemoveImageDescriptionExtension"); return 0; }

long RemoveMovieResource(long a, long b, long c_, long d, long e, long f) __asm("_RemoveMovieResource");
long RemoveMovieResource(long a, long b, long c_, long d, long e, long f) { shim_note("_RemoveMovieResource"); return 0; }

long RemoveSoundDescriptionExtension(long a, long b, long c_, long d, long e, long f) __asm("_RemoveSoundDescriptionExtension");
long RemoveSoundDescriptionExtension(long a, long b, long c_, long d, long e, long f) { shim_note("_RemoveSoundDescriptionExtension"); return 0; }

long RemoveUserData(long a, long b, long c_, long d, long e, long f) __asm("_RemoveUserData");
long RemoveUserData(long a, long b, long c_, long d, long e, long f) { shim_note("_RemoveUserData"); return 0; }

long RemoveUserDataText(long a, long b, long c_, long d, long e, long f) __asm("_RemoveUserDataText");
long RemoveUserDataText(long a, long b, long c_, long d, long e, long f) { shim_note("_RemoveUserDataText"); return 0; }

long SCAsyncIdle(long a, long b, long c_, long d, long e, long f) __asm("_SCAsyncIdle");
long SCAsyncIdle(long a, long b, long c_, long d, long e, long f) { shim_note("_SCAsyncIdle"); return 0; }

long SCCompressImage(long a, long b, long c_, long d, long e, long f) __asm("_SCCompressImage");
long SCCompressImage(long a, long b, long c_, long d, long e, long f) { shim_note("_SCCompressImage"); return 0; }

long SCCompressPicture(long a, long b, long c_, long d, long e, long f) __asm("_SCCompressPicture");
long SCCompressPicture(long a, long b, long c_, long d, long e, long f) { shim_note("_SCCompressPicture"); return 0; }

long SCCompressPictureFile(long a, long b, long c_, long d, long e, long f) __asm("_SCCompressPictureFile");
long SCCompressPictureFile(long a, long b, long c_, long d, long e, long f) { shim_note("_SCCompressPictureFile"); return 0; }

long SCCompressSequenceBegin(long a, long b, long c_, long d, long e, long f) __asm("_SCCompressSequenceBegin");
long SCCompressSequenceBegin(long a, long b, long c_, long d, long e, long f) { shim_note("_SCCompressSequenceBegin"); return 0; }

long SCCompressSequenceEnd(long a, long b, long c_, long d, long e, long f) __asm("_SCCompressSequenceEnd");
long SCCompressSequenceEnd(long a, long b, long c_, long d, long e, long f) { shim_note("_SCCompressSequenceEnd"); return 0; }

long SCCompressSequenceFrame(long a, long b, long c_, long d, long e, long f) __asm("_SCCompressSequenceFrame");
long SCCompressSequenceFrame(long a, long b, long c_, long d, long e, long f) { shim_note("_SCCompressSequenceFrame"); return 0; }

long SCDefaultPictFileSettings(long a, long b, long c_, long d, long e, long f) __asm("_SCDefaultPictFileSettings");
long SCDefaultPictFileSettings(long a, long b, long c_, long d, long e, long f) { shim_note("_SCDefaultPictFileSettings"); return 0; }

long SCDefaultPictHandleSettings(long a, long b, long c_, long d, long e, long f) __asm("_SCDefaultPictHandleSettings");
long SCDefaultPictHandleSettings(long a, long b, long c_, long d, long e, long f) { shim_note("_SCDefaultPictHandleSettings"); return 0; }

long SCDefaultPixMapSettings(long a, long b, long c_, long d, long e, long f) __asm("_SCDefaultPixMapSettings");
long SCDefaultPixMapSettings(long a, long b, long c_, long d, long e, long f) { shim_note("_SCDefaultPixMapSettings"); return 0; }

long SCGetBestDeviceRect(long a, long b, long c_, long d, long e, long f) __asm("_SCGetBestDeviceRect");
long SCGetBestDeviceRect(long a, long b, long c_, long d, long e, long f) { shim_note("_SCGetBestDeviceRect"); return 0; }

long SCGetCompressFlags(long a, long b, long c_, long d, long e, long f) __asm("_SCGetCompressFlags");
long SCGetCompressFlags(long a, long b, long c_, long d, long e, long f) { shim_note("_SCGetCompressFlags"); return 0; }

long SCGetInfo(long a, long b, long c_, long d, long e, long f) __asm("_SCGetInfo");
long SCGetInfo(long a, long b, long c_, long d, long e, long f) { shim_note("_SCGetInfo"); return 0; }

long SCGetSettingsAsText(long a, long b, long c_, long d, long e, long f) __asm("_SCGetSettingsAsText");
long SCGetSettingsAsText(long a, long b, long c_, long d, long e, long f) { shim_note("_SCGetSettingsAsText"); return 0; }

long SCPositionDialog(long a, long b, long c_, long d, long e, long f) __asm("_SCPositionDialog");
long SCPositionDialog(long a, long b, long c_, long d, long e, long f) { shim_note("_SCPositionDialog"); return 0; }

long SCPositionRect(long a, long b, long c_, long d, long e, long f) __asm("_SCPositionRect");
long SCPositionRect(long a, long b, long c_, long d, long e, long f) { shim_note("_SCPositionRect"); return 0; }

long SCRequestImageSettings(long a, long b, long c_, long d, long e, long f) __asm("_SCRequestImageSettings");
long SCRequestImageSettings(long a, long b, long c_, long d, long e, long f) { shim_note("_SCRequestImageSettings"); return 0; }

long SCRequestSequenceSettings(long a, long b, long c_, long d, long e, long f) __asm("_SCRequestSequenceSettings");
long SCRequestSequenceSettings(long a, long b, long c_, long d, long e, long f) { shim_note("_SCRequestSequenceSettings"); return 0; }

long SCSetCompressFlags(long a, long b, long c_, long d, long e, long f) __asm("_SCSetCompressFlags");
long SCSetCompressFlags(long a, long b, long c_, long d, long e, long f) { shim_note("_SCSetCompressFlags"); return 0; }

long SCSetInfo(long a, long b, long c_, long d, long e, long f) __asm("_SCSetInfo");
long SCSetInfo(long a, long b, long c_, long d, long e, long f) { shim_note("_SCSetInfo"); return 0; }

long SCSetTestImagePictFile(long a, long b, long c_, long d, long e, long f) __asm("_SCSetTestImagePictFile");
long SCSetTestImagePictFile(long a, long b, long c_, long d, long e, long f) { shim_note("_SCSetTestImagePictFile"); return 0; }

long SCSetTestImagePictHandle(long a, long b, long c_, long d, long e, long f) __asm("_SCSetTestImagePictHandle");
long SCSetTestImagePictHandle(long a, long b, long c_, long d, long e, long f) { shim_note("_SCSetTestImagePictHandle"); return 0; }

long SCSetTestImagePixMap(long a, long b, long c_, long d, long e, long f) __asm("_SCSetTestImagePixMap");
long SCSetTestImagePixMap(long a, long b, long c_, long d, long e, long f) { shim_note("_SCSetTestImagePixMap"); return 0; }

long SGAddExtendedMovieData(long a, long b, long c_, long d, long e, long f) __asm("_SGAddExtendedMovieData");
long SGAddExtendedMovieData(long a, long b, long c_, long d, long e, long f) { shim_note("_SGAddExtendedMovieData"); return 0; }

long SGAddMovieData(long a, long b, long c_, long d, long e, long f) __asm("_SGAddMovieData");
long SGAddMovieData(long a, long b, long c_, long d, long e, long f) { shim_note("_SGAddMovieData"); return 0; }

long SGAddOutputDataRefToMedia(long a, long b, long c_, long d, long e, long f) __asm("_SGAddOutputDataRefToMedia");
long SGAddOutputDataRefToMedia(long a, long b, long c_, long d, long e, long f) { shim_note("_SGAddOutputDataRefToMedia"); return 0; }

long SGAlignChannelRect(long a, long b, long c_, long d, long e, long f) __asm("_SGAlignChannelRect");
long SGAlignChannelRect(long a, long b, long c_, long d, long e, long f) { shim_note("_SGAlignChannelRect"); return 0; }

long SGChangedSource(long a, long b, long c_, long d, long e, long f) __asm("_SGChangedSource");
long SGChangedSource(long a, long b, long c_, long d, long e, long f) { shim_note("_SGChangedSource"); return 0; }

long SGChannelGetCodecSettings(long a, long b, long c_, long d, long e, long f) __asm("_SGChannelGetCodecSettings");
long SGChannelGetCodecSettings(long a, long b, long c_, long d, long e, long f) { shim_note("_SGChannelGetCodecSettings"); return 0; }

long SGChannelGetDataSourceName(long a, long b, long c_, long d, long e, long f) __asm("_SGChannelGetDataSourceName");
long SGChannelGetDataSourceName(long a, long b, long c_, long d, long e, long f) { shim_note("_SGChannelGetDataSourceName"); return 0; }

long SGChannelGetRequestedDataRate(long a, long b, long c_, long d, long e, long f) __asm("_SGChannelGetRequestedDataRate");
long SGChannelGetRequestedDataRate(long a, long b, long c_, long d, long e, long f) { shim_note("_SGChannelGetRequestedDataRate"); return 0; }

long SGChannelPutPicture(long a, long b, long c_, long d, long e, long f) __asm("_SGChannelPutPicture");
long SGChannelPutPicture(long a, long b, long c_, long d, long e, long f) { shim_note("_SGChannelPutPicture"); return 0; }

long SGChannelSetCodecSettings(long a, long b, long c_, long d, long e, long f) __asm("_SGChannelSetCodecSettings");
long SGChannelSetCodecSettings(long a, long b, long c_, long d, long e, long f) { shim_note("_SGChannelSetCodecSettings"); return 0; }

long SGChannelSetDataSourceName(long a, long b, long c_, long d, long e, long f) __asm("_SGChannelSetDataSourceName");
long SGChannelSetDataSourceName(long a, long b, long c_, long d, long e, long f) { shim_note("_SGChannelSetDataSourceName"); return 0; }

long SGChannelSetRequestedDataRate(long a, long b, long c_, long d, long e, long f) __asm("_SGChannelSetRequestedDataRate");
long SGChannelSetRequestedDataRate(long a, long b, long c_, long d, long e, long f) { shim_note("_SGChannelSetRequestedDataRate"); return 0; }

long SGCompressFrame(long a, long b, long c_, long d, long e, long f) __asm("_SGCompressFrame");
long SGCompressFrame(long a, long b, long c_, long d, long e, long f) { shim_note("_SGCompressFrame"); return 0; }

long SGDisposeChannel(long a, long b, long c_, long d, long e, long f) __asm("_SGDisposeChannel");
long SGDisposeChannel(long a, long b, long c_, long d, long e, long f) { shim_note("_SGDisposeChannel"); return 0; }

long SGDisposeOutput(long a, long b, long c_, long d, long e, long f) __asm("_SGDisposeOutput");
long SGDisposeOutput(long a, long b, long c_, long d, long e, long f) { shim_note("_SGDisposeOutput"); return 0; }

long SGGetAdditionalSoundRates(long a, long b, long c_, long d, long e, long f) __asm("_SGGetAdditionalSoundRates");
long SGGetAdditionalSoundRates(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetAdditionalSoundRates"); return 0; }

long SGGetBufferInfo(long a, long b, long c_, long d, long e, long f) __asm("_SGGetBufferInfo");
long SGGetBufferInfo(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetBufferInfo"); return 0; }

long SGGetChannelBounds(long a, long b, long c_, long d, long e, long f) __asm("_SGGetChannelBounds");
long SGGetChannelBounds(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetChannelBounds"); return 0; }

long SGGetChannelClip(long a, long b, long c_, long d, long e, long f) __asm("_SGGetChannelClip");
long SGGetChannelClip(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetChannelClip"); return 0; }

long SGGetChannelDeviceAndInputNames(long a, long b, long c_, long d, long e, long f) __asm("_SGGetChannelDeviceAndInputNames");
long SGGetChannelDeviceAndInputNames(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetChannelDeviceAndInputNames"); return 0; }

long SGGetChannelInfo(long a, long b, long c_, long d, long e, long f) __asm("_SGGetChannelInfo");
long SGGetChannelInfo(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetChannelInfo"); return 0; }

long SGGetChannelMaxFrames(long a, long b, long c_, long d, long e, long f) __asm("_SGGetChannelMaxFrames");
long SGGetChannelMaxFrames(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetChannelMaxFrames"); return 0; }

long SGGetChannelPlayFlags(long a, long b, long c_, long d, long e, long f) __asm("_SGGetChannelPlayFlags");
long SGGetChannelPlayFlags(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetChannelPlayFlags"); return 0; }

long SGGetChannelRefCon(long a, long b, long c_, long d, long e, long f) __asm("_SGGetChannelRefCon");
long SGGetChannelRefCon(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetChannelRefCon"); return 0; }

long SGGetChannelSampleDescription(long a, long b, long c_, long d, long e, long f) __asm("_SGGetChannelSampleDescription");
long SGGetChannelSampleDescription(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetChannelSampleDescription"); return 0; }

long SGGetChannelSettings(long a, long b, long c_, long d, long e, long f) __asm("_SGGetChannelSettings");
long SGGetChannelSettings(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetChannelSettings"); return 0; }

long SGGetChannelTimeBase(long a, long b, long c_, long d, long e, long f) __asm("_SGGetChannelTimeBase");
long SGGetChannelTimeBase(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetChannelTimeBase"); return 0; }

long SGGetChannelTimeScale(long a, long b, long c_, long d, long e, long f) __asm("_SGGetChannelTimeScale");
long SGGetChannelTimeScale(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetChannelTimeScale"); return 0; }

long SGGetChannelUsage(long a, long b, long c_, long d, long e, long f) __asm("_SGGetChannelUsage");
long SGGetChannelUsage(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetChannelUsage"); return 0; }

long SGGetChannelVolume(long a, long b, long c_, long d, long e, long f) __asm("_SGGetChannelVolume");
long SGGetChannelVolume(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetChannelVolume"); return 0; }

long SGGetCompressBuffer(long a, long b, long c_, long d, long e, long f) __asm("_SGGetCompressBuffer");
long SGGetCompressBuffer(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetCompressBuffer"); return 0; }

long SGGetDataOutput(long a, long b, long c_, long d, long e, long f) __asm("_SGGetDataOutput");
long SGGetDataOutput(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetDataOutput"); return 0; }

long SGGetDataOutputStorageSpaceRemaining(long a, long b, long c_, long d, long e, long f) __asm("_SGGetDataOutputStorageSpaceRemaining");
long SGGetDataOutputStorageSpaceRemaining(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetDataOutputStorageSpaceRemaining"); return 0; }

long SGGetDataOutputStorageSpaceRemaining64(long a, long b, long c_, long d, long e, long f) __asm("_SGGetDataOutputStorageSpaceRemaining64");
long SGGetDataOutputStorageSpaceRemaining64(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetDataOutputStorageSpaceRemaining64"); return 0; }

long SGGetDataRate(long a, long b, long c_, long d, long e, long f) __asm("_SGGetDataRate");
long SGGetDataRate(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetDataRate"); return 0; }

long SGGetDataRef(long a, long b, long c_, long d, long e, long f) __asm("_SGGetDataRef");
long SGGetDataRef(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetDataRef"); return 0; }

long SGGetFlags(long a, long b, long c_, long d, long e, long f) __asm("_SGGetFlags");
long SGGetFlags(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetFlags"); return 0; }

long SGGetFrameRate(long a, long b, long c_, long d, long e, long f) __asm("_SGGetFrameRate");
long SGGetFrameRate(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetFrameRate"); return 0; }

long SGGetGWorld(long a, long b, long c_, long d, long e, long f) __asm("_SGGetGWorld");
long SGGetGWorld(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetGWorld"); return 0; }

long SGGetIndChannel(long a, long b, long c_, long d, long e, long f) __asm("_SGGetIndChannel");
long SGGetIndChannel(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetIndChannel"); return 0; }

long SGGetLastMovieResID(long a, long b, long c_, long d, long e, long f) __asm("_SGGetLastMovieResID");
long SGGetLastMovieResID(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetLastMovieResID"); return 0; }

long SGGetMaximumRecordTime(long a, long b, long c_, long d, long e, long f) __asm("_SGGetMaximumRecordTime");
long SGGetMaximumRecordTime(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetMaximumRecordTime"); return 0; }

long SGGetMode(long a, long b, long c_, long d, long e, long f) __asm("_SGGetMode");
long SGGetMode(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetMode"); return 0; }

long SGGetMovie(long a, long b, long c_, long d, long e, long f) __asm("_SGGetMovie");
long SGGetMovie(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetMovie"); return 0; }

long SGGetOutputDataReference(long a, long b, long c_, long d, long e, long f) __asm("_SGGetOutputDataReference");
long SGGetOutputDataReference(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetOutputDataReference"); return 0; }

long SGGetOutputMaximumOffset(long a, long b, long c_, long d, long e, long f) __asm("_SGGetOutputMaximumOffset");
long SGGetOutputMaximumOffset(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetOutputMaximumOffset"); return 0; }

long SGGetOutputNextOutput(long a, long b, long c_, long d, long e, long f) __asm("_SGGetOutputNextOutput");
long SGGetOutputNextOutput(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetOutputNextOutput"); return 0; }

long SGGetPause(long a, long b, long c_, long d, long e, long f) __asm("_SGGetPause");
long SGGetPause(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetPause"); return 0; }

long SGGetPreferredPacketSize(long a, long b, long c_, long d, long e, long f) __asm("_SGGetPreferredPacketSize");
long SGGetPreferredPacketSize(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetPreferredPacketSize"); return 0; }

long SGGetSettings(long a, long b, long c_, long d, long e, long f) __asm("_SGGetSettings");
long SGGetSettings(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetSettings"); return 0; }

long SGGetSoundInputDriver(long a, long b, long c_, long d, long e, long f) __asm("_SGGetSoundInputDriver");
long SGGetSoundInputDriver(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetSoundInputDriver"); return 0; }

long SGGetSoundInputParameters(long a, long b, long c_, long d, long e, long f) __asm("_SGGetSoundInputParameters");
long SGGetSoundInputParameters(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetSoundInputParameters"); return 0; }

long SGGetSoundInputRate(long a, long b, long c_, long d, long e, long f) __asm("_SGGetSoundInputRate");
long SGGetSoundInputRate(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetSoundInputRate"); return 0; }

long SGGetSoundRecordChunkSize(long a, long b, long c_, long d, long e, long f) __asm("_SGGetSoundRecordChunkSize");
long SGGetSoundRecordChunkSize(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetSoundRecordChunkSize"); return 0; }

long SGGetSrcVideoBounds(long a, long b, long c_, long d, long e, long f) __asm("_SGGetSrcVideoBounds");
long SGGetSrcVideoBounds(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetSrcVideoBounds"); return 0; }

long SGGetStorageSpaceRemaining(long a, long b, long c_, long d, long e, long f) __asm("_SGGetStorageSpaceRemaining");
long SGGetStorageSpaceRemaining(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetStorageSpaceRemaining"); return 0; }

long SGGetStorageSpaceRemaining64(long a, long b, long c_, long d, long e, long f) __asm("_SGGetStorageSpaceRemaining64");
long SGGetStorageSpaceRemaining64(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetStorageSpaceRemaining64"); return 0; }

long SGGetTextReturnToSpaceValue(long a, long b, long c_, long d, long e, long f) __asm("_SGGetTextReturnToSpaceValue");
long SGGetTextReturnToSpaceValue(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetTextReturnToSpaceValue"); return 0; }

long SGGetTimeBase(long a, long b, long c_, long d, long e, long f) __asm("_SGGetTimeBase");
long SGGetTimeBase(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetTimeBase"); return 0; }

long SGGetTimeRemaining(long a, long b, long c_, long d, long e, long f) __asm("_SGGetTimeRemaining");
long SGGetTimeRemaining(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetTimeRemaining"); return 0; }

long SGGetUseScreenBuffer(long a, long b, long c_, long d, long e, long f) __asm("_SGGetUseScreenBuffer");
long SGGetUseScreenBuffer(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetUseScreenBuffer"); return 0; }

long SGGetUserVideoCompressorList(long a, long b, long c_, long d, long e, long f) __asm("_SGGetUserVideoCompressorList");
long SGGetUserVideoCompressorList(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetUserVideoCompressorList"); return 0; }

long SGGetVideoCompressor(long a, long b, long c_, long d, long e, long f) __asm("_SGGetVideoCompressor");
long SGGetVideoCompressor(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetVideoCompressor"); return 0; }

long SGGetVideoCompressorType(long a, long b, long c_, long d, long e, long f) __asm("_SGGetVideoCompressorType");
long SGGetVideoCompressorType(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetVideoCompressorType"); return 0; }

long SGGetVideoDigitizerComponent(long a, long b, long c_, long d, long e, long f) __asm("_SGGetVideoDigitizerComponent");
long SGGetVideoDigitizerComponent(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetVideoDigitizerComponent"); return 0; }

long SGGetVideoRect(long a, long b, long c_, long d, long e, long f) __asm("_SGGetVideoRect");
long SGGetVideoRect(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGetVideoRect"); return 0; }

long SGGrabFrame(long a, long b, long c_, long d, long e, long f) __asm("_SGGrabFrame");
long SGGrabFrame(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGrabFrame"); return 0; }

long SGGrabFrameComplete(long a, long b, long c_, long d, long e, long f) __asm("_SGGrabFrameComplete");
long SGGrabFrameComplete(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGrabFrameComplete"); return 0; }

long SGGrabPict(long a, long b, long c_, long d, long e, long f) __asm("_SGGrabPict");
long SGGrabPict(long a, long b, long c_, long d, long e, long f) { shim_note("_SGGrabPict"); return 0; }

long SGHandleUpdateEvent(long a, long b, long c_, long d, long e, long f) __asm("_SGHandleUpdateEvent");
long SGHandleUpdateEvent(long a, long b, long c_, long d, long e, long f) { shim_note("_SGHandleUpdateEvent"); return 0; }

long SGIdle(long a, long b, long c_, long d, long e, long f) __asm("_SGIdle");
long SGIdle(long a, long b, long c_, long d, long e, long f) { shim_note("_SGIdle"); return 0; }

long SGInitChannel(long a, long b, long c_, long d, long e, long f) __asm("_SGInitChannel");
long SGInitChannel(long a, long b, long c_, long d, long e, long f) { shim_note("_SGInitChannel"); return 0; }

long SGInitialize(long a, long b, long c_, long d, long e, long f) __asm("_SGInitialize");
long SGInitialize(long a, long b, long c_, long d, long e, long f) { shim_note("_SGInitialize"); return 0; }

long SGNewChannel(long a, long b, long c_, long d, long e, long f) __asm("_SGNewChannel");
long SGNewChannel(long a, long b, long c_, long d, long e, long f) { shim_note("_SGNewChannel"); return 0; }

long SGNewChannelFromComponent(long a, long b, long c_, long d, long e, long f) __asm("_SGNewChannelFromComponent");
long SGNewChannelFromComponent(long a, long b, long c_, long d, long e, long f) { shim_note("_SGNewChannelFromComponent"); return 0; }

long SGNewOutput(long a, long b, long c_, long d, long e, long f) __asm("_SGNewOutput");
long SGNewOutput(long a, long b, long c_, long d, long e, long f) { shim_note("_SGNewOutput"); return 0; }

long SGPanelCanRun(long a, long b, long c_, long d, long e, long f) __asm("_SGPanelCanRun");
long SGPanelCanRun(long a, long b, long c_, long d, long e, long f) { shim_note("_SGPanelCanRun"); return 0; }

long SGPanelEvent(long a, long b, long c_, long d, long e, long f) __asm("_SGPanelEvent");
long SGPanelEvent(long a, long b, long c_, long d, long e, long f) { shim_note("_SGPanelEvent"); return 0; }

long SGPanelGetDITLForSize(long a, long b, long c_, long d, long e, long f) __asm("_SGPanelGetDITLForSize");
long SGPanelGetDITLForSize(long a, long b, long c_, long d, long e, long f) { shim_note("_SGPanelGetDITLForSize"); return 0; }

long SGPanelGetDitl(long a, long b, long c_, long d, long e, long f) __asm("_SGPanelGetDitl");
long SGPanelGetDitl(long a, long b, long c_, long d, long e, long f) { shim_note("_SGPanelGetDitl"); return 0; }

long SGPanelGetSettings(long a, long b, long c_, long d, long e, long f) __asm("_SGPanelGetSettings");
long SGPanelGetSettings(long a, long b, long c_, long d, long e, long f) { shim_note("_SGPanelGetSettings"); return 0; }

long SGPanelGetTitle(long a, long b, long c_, long d, long e, long f) __asm("_SGPanelGetTitle");
long SGPanelGetTitle(long a, long b, long c_, long d, long e, long f) { shim_note("_SGPanelGetTitle"); return 0; }

long SGPanelInstall(long a, long b, long c_, long d, long e, long f) __asm("_SGPanelInstall");
long SGPanelInstall(long a, long b, long c_, long d, long e, long f) { shim_note("_SGPanelInstall"); return 0; }

long SGPanelItem(long a, long b, long c_, long d, long e, long f) __asm("_SGPanelItem");
long SGPanelItem(long a, long b, long c_, long d, long e, long f) { shim_note("_SGPanelItem"); return 0; }

long SGPanelRemove(long a, long b, long c_, long d, long e, long f) __asm("_SGPanelRemove");
long SGPanelRemove(long a, long b, long c_, long d, long e, long f) { shim_note("_SGPanelRemove"); return 0; }

long SGPanelSetGrabber(long a, long b, long c_, long d, long e, long f) __asm("_SGPanelSetGrabber");
long SGPanelSetGrabber(long a, long b, long c_, long d, long e, long f) { shim_note("_SGPanelSetGrabber"); return 0; }

long SGPanelSetResFile(long a, long b, long c_, long d, long e, long f) __asm("_SGPanelSetResFile");
long SGPanelSetResFile(long a, long b, long c_, long d, long e, long f) { shim_note("_SGPanelSetResFile"); return 0; }

long SGPanelSetSettings(long a, long b, long c_, long d, long e, long f) __asm("_SGPanelSetSettings");
long SGPanelSetSettings(long a, long b, long c_, long d, long e, long f) { shim_note("_SGPanelSetSettings"); return 0; }

long SGPanelValidateInput(long a, long b, long c_, long d, long e, long f) __asm("_SGPanelValidateInput");
long SGPanelValidateInput(long a, long b, long c_, long d, long e, long f) { shim_note("_SGPanelValidateInput"); return 0; }

long SGPause(long a, long b, long c_, long d, long e, long f) __asm("_SGPause");
long SGPause(long a, long b, long c_, long d, long e, long f) { shim_note("_SGPause"); return 0; }

long SGPrepare(long a, long b, long c_, long d, long e, long f) __asm("_SGPrepare");
long SGPrepare(long a, long b, long c_, long d, long e, long f) { shim_note("_SGPrepare"); return 0; }

long SGRelease(long a, long b, long c_, long d, long e, long f) __asm("_SGRelease");
long SGRelease(long a, long b, long c_, long d, long e, long f) { shim_note("_SGRelease"); return 0; }

long SGSetAdditionalSoundRates(long a, long b, long c_, long d, long e, long f) __asm("_SGSetAdditionalSoundRates");
long SGSetAdditionalSoundRates(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetAdditionalSoundRates"); return 0; }

long SGSetChannelBounds(long a, long b, long c_, long d, long e, long f) __asm("_SGSetChannelBounds");
long SGSetChannelBounds(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetChannelBounds"); return 0; }

long SGSetChannelClip(long a, long b, long c_, long d, long e, long f) __asm("_SGSetChannelClip");
long SGSetChannelClip(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetChannelClip"); return 0; }

long SGSetChannelDevice(long a, long b, long c_, long d, long e, long f) __asm("_SGSetChannelDevice");
long SGSetChannelDevice(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetChannelDevice"); return 0; }

long SGSetChannelDeviceInput(long a, long b, long c_, long d, long e, long f) __asm("_SGSetChannelDeviceInput");
long SGSetChannelDeviceInput(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetChannelDeviceInput"); return 0; }

long SGSetChannelMaxFrames(long a, long b, long c_, long d, long e, long f) __asm("_SGSetChannelMaxFrames");
long SGSetChannelMaxFrames(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetChannelMaxFrames"); return 0; }

long SGSetChannelOutput(long a, long b, long c_, long d, long e, long f) __asm("_SGSetChannelOutput");
long SGSetChannelOutput(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetChannelOutput"); return 0; }

long SGSetChannelPlayFlags(long a, long b, long c_, long d, long e, long f) __asm("_SGSetChannelPlayFlags");
long SGSetChannelPlayFlags(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetChannelPlayFlags"); return 0; }

long SGSetChannelRefCon(long a, long b, long c_, long d, long e, long f) __asm("_SGSetChannelRefCon");
long SGSetChannelRefCon(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetChannelRefCon"); return 0; }

long SGSetChannelSettings(long a, long b, long c_, long d, long e, long f) __asm("_SGSetChannelSettings");
long SGSetChannelSettings(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetChannelSettings"); return 0; }

long SGSetChannelSettingsStateChanging(long a, long b, long c_, long d, long e, long f) __asm("_SGSetChannelSettingsStateChanging");
long SGSetChannelSettingsStateChanging(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetChannelSettingsStateChanging"); return 0; }

long SGSetChannelUsage(long a, long b, long c_, long d, long e, long f) __asm("_SGSetChannelUsage");
long SGSetChannelUsage(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetChannelUsage"); return 0; }

long SGSetChannelVolume(long a, long b, long c_, long d, long e, long f) __asm("_SGSetChannelVolume");
long SGSetChannelVolume(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetChannelVolume"); return 0; }

long SGSetCompressBuffer(long a, long b, long c_, long d, long e, long f) __asm("_SGSetCompressBuffer");
long SGSetCompressBuffer(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetCompressBuffer"); return 0; }

long SGSetDataOutput(long a, long b, long c_, long d, long e, long f) __asm("_SGSetDataOutput");
long SGSetDataOutput(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetDataOutput"); return 0; }

long SGSetDataRef(long a, long b, long c_, long d, long e, long f) __asm("_SGSetDataRef");
long SGSetDataRef(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetDataRef"); return 0; }

long SGSetFlags(long a, long b, long c_, long d, long e, long f) __asm("_SGSetFlags");
long SGSetFlags(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetFlags"); return 0; }

long SGSetFontName(long a, long b, long c_, long d, long e, long f) __asm("_SGSetFontName");
long SGSetFontName(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetFontName"); return 0; }

long SGSetFontSize(long a, long b, long c_, long d, long e, long f) __asm("_SGSetFontSize");
long SGSetFontSize(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetFontSize"); return 0; }

long SGSetFrameRate(long a, long b, long c_, long d, long e, long f) __asm("_SGSetFrameRate");
long SGSetFrameRate(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetFrameRate"); return 0; }

long SGSetGWorld(long a, long b, long c_, long d, long e, long f) __asm("_SGSetGWorld");
long SGSetGWorld(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetGWorld"); return 0; }

long SGSetJustification(long a, long b, long c_, long d, long e, long f) __asm("_SGSetJustification");
long SGSetJustification(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetJustification"); return 0; }

long SGSetMaximumRecordTime(long a, long b, long c_, long d, long e, long f) __asm("_SGSetMaximumRecordTime");
long SGSetMaximumRecordTime(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetMaximumRecordTime"); return 0; }

long SGSetOutputFlags(long a, long b, long c_, long d, long e, long f) __asm("_SGSetOutputFlags");
long SGSetOutputFlags(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetOutputFlags"); return 0; }

long SGSetOutputMaximumOffset(long a, long b, long c_, long d, long e, long f) __asm("_SGSetOutputMaximumOffset");
long SGSetOutputMaximumOffset(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetOutputMaximumOffset"); return 0; }

long SGSetOutputNextOutput(long a, long b, long c_, long d, long e, long f) __asm("_SGSetOutputNextOutput");
long SGSetOutputNextOutput(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetOutputNextOutput"); return 0; }

long SGSetPreferredPacketSize(long a, long b, long c_, long d, long e, long f) __asm("_SGSetPreferredPacketSize");
long SGSetPreferredPacketSize(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetPreferredPacketSize"); return 0; }

long SGSetSettings(long a, long b, long c_, long d, long e, long f) __asm("_SGSetSettings");
long SGSetSettings(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetSettings"); return 0; }

long SGSetSettingsSummary(long a, long b, long c_, long d, long e, long f) __asm("_SGSetSettingsSummary");
long SGSetSettingsSummary(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetSettingsSummary"); return 0; }

long SGSetSoundInputDriver(long a, long b, long c_, long d, long e, long f) __asm("_SGSetSoundInputDriver");
long SGSetSoundInputDriver(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetSoundInputDriver"); return 0; }

long SGSetSoundInputParameters(long a, long b, long c_, long d, long e, long f) __asm("_SGSetSoundInputParameters");
long SGSetSoundInputParameters(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetSoundInputParameters"); return 0; }

long SGSetSoundInputRate(long a, long b, long c_, long d, long e, long f) __asm("_SGSetSoundInputRate");
long SGSetSoundInputRate(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetSoundInputRate"); return 0; }

long SGSetSoundRecordChunkSize(long a, long b, long c_, long d, long e, long f) __asm("_SGSetSoundRecordChunkSize");
long SGSetSoundRecordChunkSize(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetSoundRecordChunkSize"); return 0; }

long SGSetTextBackColor(long a, long b, long c_, long d, long e, long f) __asm("_SGSetTextBackColor");
long SGSetTextBackColor(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetTextBackColor"); return 0; }

long SGSetTextForeColor(long a, long b, long c_, long d, long e, long f) __asm("_SGSetTextForeColor");
long SGSetTextForeColor(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetTextForeColor"); return 0; }

long SGSetTextReturnToSpaceValue(long a, long b, long c_, long d, long e, long f) __asm("_SGSetTextReturnToSpaceValue");
long SGSetTextReturnToSpaceValue(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetTextReturnToSpaceValue"); return 0; }

long SGSetUseScreenBuffer(long a, long b, long c_, long d, long e, long f) __asm("_SGSetUseScreenBuffer");
long SGSetUseScreenBuffer(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetUseScreenBuffer"); return 0; }

long SGSetUserVideoCompressorList(long a, long b, long c_, long d, long e, long f) __asm("_SGSetUserVideoCompressorList");
long SGSetUserVideoCompressorList(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetUserVideoCompressorList"); return 0; }

long SGSetVideoCompressor(long a, long b, long c_, long d, long e, long f) __asm("_SGSetVideoCompressor");
long SGSetVideoCompressor(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetVideoCompressor"); return 0; }

long SGSetVideoCompressorType(long a, long b, long c_, long d, long e, long f) __asm("_SGSetVideoCompressorType");
long SGSetVideoCompressorType(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetVideoCompressorType"); return 0; }

long SGSetVideoDigitizerComponent(long a, long b, long c_, long d, long e, long f) __asm("_SGSetVideoDigitizerComponent");
long SGSetVideoDigitizerComponent(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetVideoDigitizerComponent"); return 0; }

long SGSetVideoRect(long a, long b, long c_, long d, long e, long f) __asm("_SGSetVideoRect");
long SGSetVideoRect(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSetVideoRect"); return 0; }

long SGSoundInputDriverChanged(long a, long b, long c_, long d, long e, long f) __asm("_SGSoundInputDriverChanged");
long SGSoundInputDriverChanged(long a, long b, long c_, long d, long e, long f) { shim_note("_SGSoundInputDriverChanged"); return 0; }

long SGStartPreview(long a, long b, long c_, long d, long e, long f) __asm("_SGStartPreview");
long SGStartPreview(long a, long b, long c_, long d, long e, long f) { shim_note("_SGStartPreview"); return 0; }

long SGStartRecord(long a, long b, long c_, long d, long e, long f) __asm("_SGStartRecord");
long SGStartRecord(long a, long b, long c_, long d, long e, long f) { shim_note("_SGStartRecord"); return 0; }

long SGStop(long a, long b, long c_, long d, long e, long f) __asm("_SGStop");
long SGStop(long a, long b, long c_, long d, long e, long f) { shim_note("_SGStop"); return 0; }

long SGUpdate(long a, long b, long c_, long d, long e, long f) __asm("_SGUpdate");
long SGUpdate(long a, long b, long c_, long d, long e, long f) { shim_note("_SGUpdate"); return 0; }

long SGVideoDigitizerChanged(long a, long b, long c_, long d, long e, long f) __asm("_SGVideoDigitizerChanged");
long SGVideoDigitizerChanged(long a, long b, long c_, long d, long e, long f) { shim_note("_SGVideoDigitizerChanged"); return 0; }

long SGWriteExtendedMovieData(long a, long b, long c_, long d, long e, long f) __asm("_SGWriteExtendedMovieData");
long SGWriteExtendedMovieData(long a, long b, long c_, long d, long e, long f) { shim_note("_SGWriteExtendedMovieData"); return 0; }

long SGWriteMovieData(long a, long b, long c_, long d, long e, long f) __asm("_SGWriteMovieData");
long SGWriteMovieData(long a, long b, long c_, long d, long e, long f) { shim_note("_SGWriteMovieData"); return 0; }

long SGWriteSamples(long a, long b, long c_, long d, long e, long f) __asm("_SGWriteSamples");
long SGWriteSamples(long a, long b, long c_, long d, long e, long f) { shim_note("_SGWriteSamples"); return 0; }

long SampleNumToMediaTime(long a, long b, long c_, long d, long e, long f) __asm("_SampleNumToMediaTime");
long SampleNumToMediaTime(long a, long b, long c_, long d, long e, long f) { shim_note("_SampleNumToMediaTime"); return 0; }

long ScaleMovieSegment(long a, long b, long c_, long d, long e, long f) __asm("_ScaleMovieSegment");
long ScaleMovieSegment(long a, long b, long c_, long d, long e, long f) { shim_note("_ScaleMovieSegment"); return 0; }

long ScaleTrackSegment(long a, long b, long c_, long d, long e, long f) __asm("_ScaleTrackSegment");
long ScaleTrackSegment(long a, long b, long c_, long d, long e, long f) { shim_note("_ScaleTrackSegment"); return 0; }

long SelectMovieAlternates(long a, long b, long c_, long d, long e, long f) __asm("_SelectMovieAlternates");
long SelectMovieAlternates(long a, long b, long c_, long d, long e, long f) { shim_note("_SelectMovieAlternates"); return 0; }

long SetAutoTrackAlternatesEnabled(long a, long b, long c_, long d, long e, long f) __asm("_SetAutoTrackAlternatesEnabled");
long SetAutoTrackAlternatesEnabled(long a, long b, long c_, long d, long e, long f) { shim_note("_SetAutoTrackAlternatesEnabled"); return 0; }

long SetImageDescriptionCTable(long a, long b, long c_, long d, long e, long f) __asm("_SetImageDescriptionCTable");
long SetImageDescriptionCTable(long a, long b, long c_, long d, long e, long f) { shim_note("_SetImageDescriptionCTable"); return 0; }

long SetMediaDataHandler(long a, long b, long c_, long d, long e, long f) __asm("_SetMediaDataHandler");
long SetMediaDataHandler(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMediaDataHandler"); return 0; }

long SetMediaDataRef(long a, long b, long c_, long d, long e, long f) __asm("_SetMediaDataRef");
long SetMediaDataRef(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMediaDataRef"); return 0; }

long SetMediaDataRefAttributes(long a, long b, long c_, long d, long e, long f) __asm("_SetMediaDataRefAttributes");
long SetMediaDataRefAttributes(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMediaDataRefAttributes"); return 0; }

long SetMediaDefaultDataRefIndex(long a, long b, long c_, long d, long e, long f) __asm("_SetMediaDefaultDataRefIndex");
long SetMediaDefaultDataRefIndex(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMediaDefaultDataRefIndex"); return 0; }

long SetMediaHandler(long a, long b, long c_, long d, long e, long f) __asm("_SetMediaHandler");
long SetMediaHandler(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMediaHandler"); return 0; }

long SetMediaLanguage(long a, long b, long c_, long d, long e, long f) __asm("_SetMediaLanguage");
long SetMediaLanguage(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMediaLanguage"); return 0; }

long SetMediaPlayHints(long a, long b, long c_, long d, long e, long f) __asm("_SetMediaPlayHints");
long SetMediaPlayHints(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMediaPlayHints"); return 0; }

long SetMediaPreferredChunkSize(long a, long b, long c_, long d, long e, long f) __asm("_SetMediaPreferredChunkSize");
long SetMediaPreferredChunkSize(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMediaPreferredChunkSize"); return 0; }

long SetMediaQuality(long a, long b, long c_, long d, long e, long f) __asm("_SetMediaQuality");
long SetMediaQuality(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMediaQuality"); return 0; }

long SetMediaSampleDescription(long a, long b, long c_, long d, long e, long f) __asm("_SetMediaSampleDescription");
long SetMediaSampleDescription(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMediaSampleDescription"); return 0; }

long SetMediaShadowSync(long a, long b, long c_, long d, long e, long f) __asm("_SetMediaShadowSync");
long SetMediaShadowSync(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMediaShadowSync"); return 0; }

long SetMediaTimeScale(long a, long b, long c_, long d, long e, long f) __asm("_SetMediaTimeScale");
long SetMediaTimeScale(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMediaTimeScale"); return 0; }

long SetMovieActiveSegment(long a, long b, long c_, long d, long e, long f) __asm("_SetMovieActiveSegment");
long SetMovieActiveSegment(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMovieActiveSegment"); return 0; }

long SetMovieClipRgn(long a, long b, long c_, long d, long e, long f) __asm("_SetMovieClipRgn");
long SetMovieClipRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMovieClipRgn"); return 0; }

long SetMovieColorTable(long a, long b, long c_, long d, long e, long f) __asm("_SetMovieColorTable");
long SetMovieColorTable(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMovieColorTable"); return 0; }

long SetMovieDefaultDataRef(long a, long b, long c_, long d, long e, long f) __asm("_SetMovieDefaultDataRef");
long SetMovieDefaultDataRef(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMovieDefaultDataRef"); return 0; }

long SetMovieDisplayClipRgn(long a, long b, long c_, long d, long e, long f) __asm("_SetMovieDisplayClipRgn");
long SetMovieDisplayClipRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMovieDisplayClipRgn"); return 0; }

long SetMovieLanguage(long a, long b, long c_, long d, long e, long f) __asm("_SetMovieLanguage");
long SetMovieLanguage(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMovieLanguage"); return 0; }

long SetMovieMasterClock(long a, long b, long c_, long d, long e, long f) __asm("_SetMovieMasterClock");
long SetMovieMasterClock(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMovieMasterClock"); return 0; }

long SetMovieMasterTimeBase(long a, long b, long c_, long d, long e, long f) __asm("_SetMovieMasterTimeBase");
long SetMovieMasterTimeBase(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMovieMasterTimeBase"); return 0; }

long SetMoviePlayHints(long a, long b, long c_, long d, long e, long f) __asm("_SetMoviePlayHints");
long SetMoviePlayHints(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMoviePlayHints"); return 0; }

long SetMoviePosterTime(long a, long b, long c_, long d, long e, long f) __asm("_SetMoviePosterTime");
long SetMoviePosterTime(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMoviePosterTime"); return 0; }

long SetMoviePreferredRate(long a, long b, long c_, long d, long e, long f) __asm("_SetMoviePreferredRate");
long SetMoviePreferredRate(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMoviePreferredRate"); return 0; }

long SetMoviePreferredVolume(long a, long b, long c_, long d, long e, long f) __asm("_SetMoviePreferredVolume");
long SetMoviePreferredVolume(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMoviePreferredVolume"); return 0; }

long SetMoviePreviewMode(long a, long b, long c_, long d, long e, long f) __asm("_SetMoviePreviewMode");
long SetMoviePreviewMode(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMoviePreviewMode"); return 0; }

long SetMoviePreviewTime(long a, long b, long c_, long d, long e, long f) __asm("_SetMoviePreviewTime");
long SetMoviePreviewTime(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMoviePreviewTime"); return 0; }

long SetMovieSelection(long a, long b, long c_, long d, long e, long f) __asm("_SetMovieSelection");
long SetMovieSelection(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMovieSelection"); return 0; }

long SetMovieTime(long a, long b, long c_, long d, long e, long f) __asm("_SetMovieTime");
long SetMovieTime(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMovieTime"); return 0; }

long SetMovieTimeScale(long a, long b, long c_, long d, long e, long f) __asm("_SetMovieTimeScale");
long SetMovieTimeScale(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMovieTimeScale"); return 0; }

long SetMovieVideoOutput(long a, long b, long c_, long d, long e, long f) __asm("_SetMovieVideoOutput");
long SetMovieVideoOutput(long a, long b, long c_, long d, long e, long f) { shim_note("_SetMovieVideoOutput"); return 0; }

long SetPosterBox(long a, long b, long c_, long d, long e, long f) __asm("_SetPosterBox");
long SetPosterBox(long a, long b, long c_, long d, long e, long f) { shim_note("_SetPosterBox"); return 0; }

long SetTimeBaseFlags(long a, long b, long c_, long d, long e, long f) __asm("_SetTimeBaseFlags");
long SetTimeBaseFlags(long a, long b, long c_, long d, long e, long f) { shim_note("_SetTimeBaseFlags"); return 0; }

long SetTimeBaseMasterClock(long a, long b, long c_, long d, long e, long f) __asm("_SetTimeBaseMasterClock");
long SetTimeBaseMasterClock(long a, long b, long c_, long d, long e, long f) { shim_note("_SetTimeBaseMasterClock"); return 0; }

long SetTimeBaseMasterTimeBase(long a, long b, long c_, long d, long e, long f) __asm("_SetTimeBaseMasterTimeBase");
long SetTimeBaseMasterTimeBase(long a, long b, long c_, long d, long e, long f) { shim_note("_SetTimeBaseMasterTimeBase"); return 0; }

long SetTimeBaseRate(long a, long b, long c_, long d, long e, long f) __asm("_SetTimeBaseRate");
long SetTimeBaseRate(long a, long b, long c_, long d, long e, long f) { shim_note("_SetTimeBaseRate"); return 0; }

long SetTimeBaseStartTime(long a, long b, long c_, long d, long e, long f) __asm("_SetTimeBaseStartTime");
long SetTimeBaseStartTime(long a, long b, long c_, long d, long e, long f) { shim_note("_SetTimeBaseStartTime"); return 0; }

long SetTimeBaseStopTime(long a, long b, long c_, long d, long e, long f) __asm("_SetTimeBaseStopTime");
long SetTimeBaseStopTime(long a, long b, long c_, long d, long e, long f) { shim_note("_SetTimeBaseStopTime"); return 0; }

long SetTimeBaseTime(long a, long b, long c_, long d, long e, long f) __asm("_SetTimeBaseTime");
long SetTimeBaseTime(long a, long b, long c_, long d, long e, long f) { shim_note("_SetTimeBaseTime"); return 0; }

long SetTimeBaseValue(long a, long b, long c_, long d, long e, long f) __asm("_SetTimeBaseValue");
long SetTimeBaseValue(long a, long b, long c_, long d, long e, long f) { shim_note("_SetTimeBaseValue"); return 0; }

long SetTimeBaseZero(long a, long b, long c_, long d, long e, long f) __asm("_SetTimeBaseZero");
long SetTimeBaseZero(long a, long b, long c_, long d, long e, long f) { shim_note("_SetTimeBaseZero"); return 0; }

long SetTrackAlternate(long a, long b, long c_, long d, long e, long f) __asm("_SetTrackAlternate");
long SetTrackAlternate(long a, long b, long c_, long d, long e, long f) { shim_note("_SetTrackAlternate"); return 0; }

long SetTrackClipRgn(long a, long b, long c_, long d, long e, long f) __asm("_SetTrackClipRgn");
long SetTrackClipRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_SetTrackClipRgn"); return 0; }

long SetTrackDimensions(long a, long b, long c_, long d, long e, long f) __asm("_SetTrackDimensions");
long SetTrackDimensions(long a, long b, long c_, long d, long e, long f) { shim_note("_SetTrackDimensions"); return 0; }

long SetTrackEnabled(long a, long b, long c_, long d, long e, long f) __asm("_SetTrackEnabled");
long SetTrackEnabled(long a, long b, long c_, long d, long e, long f) { shim_note("_SetTrackEnabled"); return 0; }

long SetTrackLayer(long a, long b, long c_, long d, long e, long f) __asm("_SetTrackLayer");
long SetTrackLayer(long a, long b, long c_, long d, long e, long f) { shim_note("_SetTrackLayer"); return 0; }

long SetTrackLoadSettings(long a, long b, long c_, long d, long e, long f) __asm("_SetTrackLoadSettings");
long SetTrackLoadSettings(long a, long b, long c_, long d, long e, long f) { shim_note("_SetTrackLoadSettings"); return 0; }

long SetTrackMatte(long a, long b, long c_, long d, long e, long f) __asm("_SetTrackMatte");
long SetTrackMatte(long a, long b, long c_, long d, long e, long f) { shim_note("_SetTrackMatte"); return 0; }

long SetTrackOffset(long a, long b, long c_, long d, long e, long f) __asm("_SetTrackOffset");
long SetTrackOffset(long a, long b, long c_, long d, long e, long f) { shim_note("_SetTrackOffset"); return 0; }

long SetTrackReference(long a, long b, long c_, long d, long e, long f) __asm("_SetTrackReference");
long SetTrackReference(long a, long b, long c_, long d, long e, long f) { shim_note("_SetTrackReference"); return 0; }

long SetTrackSoundLocalizationSettings(long a, long b, long c_, long d, long e, long f) __asm("_SetTrackSoundLocalizationSettings");
long SetTrackSoundLocalizationSettings(long a, long b, long c_, long d, long e, long f) { shim_note("_SetTrackSoundLocalizationSettings"); return 0; }

long SetTrackUsage(long a, long b, long c_, long d, long e, long f) __asm("_SetTrackUsage");
long SetTrackUsage(long a, long b, long c_, long d, long e, long f) { shim_note("_SetTrackUsage"); return 0; }

long SetTrackVolume(long a, long b, long c_, long d, long e, long f) __asm("_SetTrackVolume");
long SetTrackVolume(long a, long b, long c_, long d, long e, long f) { shim_note("_SetTrackVolume"); return 0; }

long ShowMoviePoster(long a, long b, long c_, long d, long e, long f) __asm("_ShowMoviePoster");
long ShowMoviePoster(long a, long b, long c_, long d, long e, long f) { shim_note("_ShowMoviePoster"); return 0; }

long SpriteMediaCountImages(long a, long b, long c_, long d, long e, long f) __asm("_SpriteMediaCountImages");
long SpriteMediaCountImages(long a, long b, long c_, long d, long e, long f) { shim_note("_SpriteMediaCountImages"); return 0; }

long SpriteMediaCountSprites(long a, long b, long c_, long d, long e, long f) __asm("_SpriteMediaCountSprites");
long SpriteMediaCountSprites(long a, long b, long c_, long d, long e, long f) { shim_note("_SpriteMediaCountSprites"); return 0; }

long SpriteMediaDisposeImage(long a, long b, long c_, long d, long e, long f) __asm("_SpriteMediaDisposeImage");
long SpriteMediaDisposeImage(long a, long b, long c_, long d, long e, long f) { shim_note("_SpriteMediaDisposeImage"); return 0; }

long SpriteMediaDisposeSprite(long a, long b, long c_, long d, long e, long f) __asm("_SpriteMediaDisposeSprite");
long SpriteMediaDisposeSprite(long a, long b, long c_, long d, long e, long f) { shim_note("_SpriteMediaDisposeSprite"); return 0; }

long SpriteMediaGetActionVariable(long a, long b, long c_, long d, long e, long f) __asm("_SpriteMediaGetActionVariable");
long SpriteMediaGetActionVariable(long a, long b, long c_, long d, long e, long f) { shim_note("_SpriteMediaGetActionVariable"); return 0; }

long SpriteMediaGetActionVariableAsString(long a, long b, long c_, long d, long e, long f) __asm("_SpriteMediaGetActionVariableAsString");
long SpriteMediaGetActionVariableAsString(long a, long b, long c_, long d, long e, long f) { shim_note("_SpriteMediaGetActionVariableAsString"); return 0; }

long SpriteMediaGetDisplayedSampleNumber(long a, long b, long c_, long d, long e, long f) __asm("_SpriteMediaGetDisplayedSampleNumber");
long SpriteMediaGetDisplayedSampleNumber(long a, long b, long c_, long d, long e, long f) { shim_note("_SpriteMediaGetDisplayedSampleNumber"); return 0; }

long SpriteMediaGetImageName(long a, long b, long c_, long d, long e, long f) __asm("_SpriteMediaGetImageName");
long SpriteMediaGetImageName(long a, long b, long c_, long d, long e, long f) { shim_note("_SpriteMediaGetImageName"); return 0; }

long SpriteMediaGetIndImageDescription(long a, long b, long c_, long d, long e, long f) __asm("_SpriteMediaGetIndImageDescription");
long SpriteMediaGetIndImageDescription(long a, long b, long c_, long d, long e, long f) { shim_note("_SpriteMediaGetIndImageDescription"); return 0; }

long SpriteMediaGetProperty(long a, long b, long c_, long d, long e, long f) __asm("_SpriteMediaGetProperty");
long SpriteMediaGetProperty(long a, long b, long c_, long d, long e, long f) { shim_note("_SpriteMediaGetProperty"); return 0; }

long SpriteMediaGetSpriteName(long a, long b, long c_, long d, long e, long f) __asm("_SpriteMediaGetSpriteName");
long SpriteMediaGetSpriteName(long a, long b, long c_, long d, long e, long f) { shim_note("_SpriteMediaGetSpriteName"); return 0; }

long SpriteMediaGetSpriteProperty(long a, long b, long c_, long d, long e, long f) __asm("_SpriteMediaGetSpriteProperty");
long SpriteMediaGetSpriteProperty(long a, long b, long c_, long d, long e, long f) { shim_note("_SpriteMediaGetSpriteProperty"); return 0; }

long SpriteMediaHitTestAllSprites(long a, long b, long c_, long d, long e, long f) __asm("_SpriteMediaHitTestAllSprites");
long SpriteMediaHitTestAllSprites(long a, long b, long c_, long d, long e, long f) { shim_note("_SpriteMediaHitTestAllSprites"); return 0; }

long SpriteMediaHitTestOneSprite(long a, long b, long c_, long d, long e, long f) __asm("_SpriteMediaHitTestOneSprite");
long SpriteMediaHitTestOneSprite(long a, long b, long c_, long d, long e, long f) { shim_note("_SpriteMediaHitTestOneSprite"); return 0; }

long SpriteMediaHitTestSprites(long a, long b, long c_, long d, long e, long f) __asm("_SpriteMediaHitTestSprites");
long SpriteMediaHitTestSprites(long a, long b, long c_, long d, long e, long f) { shim_note("_SpriteMediaHitTestSprites"); return 0; }

long SpriteMediaImageIDToIndex(long a, long b, long c_, long d, long e, long f) __asm("_SpriteMediaImageIDToIndex");
long SpriteMediaImageIDToIndex(long a, long b, long c_, long d, long e, long f) { shim_note("_SpriteMediaImageIDToIndex"); return 0; }

long SpriteMediaImageIndexToID(long a, long b, long c_, long d, long e, long f) __asm("_SpriteMediaImageIndexToID");
long SpriteMediaImageIndexToID(long a, long b, long c_, long d, long e, long f) { shim_note("_SpriteMediaImageIndexToID"); return 0; }

long SpriteMediaNewImage(long a, long b, long c_, long d, long e, long f) __asm("_SpriteMediaNewImage");
long SpriteMediaNewImage(long a, long b, long c_, long d, long e, long f) { shim_note("_SpriteMediaNewImage"); return 0; }

long SpriteMediaSetActionVariable(long a, long b, long c_, long d, long e, long f) __asm("_SpriteMediaSetActionVariable");
long SpriteMediaSetActionVariable(long a, long b, long c_, long d, long e, long f) { shim_note("_SpriteMediaSetActionVariable"); return 0; }

long SpriteMediaSetActionVariableToString(long a, long b, long c_, long d, long e, long f) __asm("_SpriteMediaSetActionVariableToString");
long SpriteMediaSetActionVariableToString(long a, long b, long c_, long d, long e, long f) { shim_note("_SpriteMediaSetActionVariableToString"); return 0; }

long SpriteMediaSetProperty(long a, long b, long c_, long d, long e, long f) __asm("_SpriteMediaSetProperty");
long SpriteMediaSetProperty(long a, long b, long c_, long d, long e, long f) { shim_note("_SpriteMediaSetProperty"); return 0; }

long SpriteMediaSetSpriteProperty(long a, long b, long c_, long d, long e, long f) __asm("_SpriteMediaSetSpriteProperty");
long SpriteMediaSetSpriteProperty(long a, long b, long c_, long d, long e, long f) { shim_note("_SpriteMediaSetSpriteProperty"); return 0; }

long SpriteMediaSpriteIDToIndex(long a, long b, long c_, long d, long e, long f) __asm("_SpriteMediaSpriteIDToIndex");
long SpriteMediaSpriteIDToIndex(long a, long b, long c_, long d, long e, long f) { shim_note("_SpriteMediaSpriteIDToIndex"); return 0; }

long SpriteMediaSpriteIndexToID(long a, long b, long c_, long d, long e, long f) __asm("_SpriteMediaSpriteIndexToID");
long SpriteMediaSpriteIndexToID(long a, long b, long c_, long d, long e, long f) { shim_note("_SpriteMediaSpriteIndexToID"); return 0; }

long SubtractTime(long a, long b, long c_, long d, long e, long f) __asm("_SubtractTime");
long SubtractTime(long a, long b, long c_, long d, long e, long f) { shim_note("_SubtractTime"); return 0; }

long TCGetSourceRef(long a, long b, long c_, long d, long e, long f) __asm("_TCGetSourceRef");
long TCGetSourceRef(long a, long b, long c_, long d, long e, long f) { shim_note("_TCGetSourceRef"); return 0; }

long TCGetTimeCodeFlags(long a, long b, long c_, long d, long e, long f) __asm("_TCGetTimeCodeFlags");
long TCGetTimeCodeFlags(long a, long b, long c_, long d, long e, long f) { shim_note("_TCGetTimeCodeFlags"); return 0; }

long TCSetSourceRef(long a, long b, long c_, long d, long e, long f) __asm("_TCSetSourceRef");
long TCSetSourceRef(long a, long b, long c_, long d, long e, long f) { shim_note("_TCSetSourceRef"); return 0; }

long TCSetTimeCodeFlags(long a, long b, long c_, long d, long e, long f) __asm("_TCSetTimeCodeFlags");
long TCSetTimeCodeFlags(long a, long b, long c_, long d, long e, long f) { shim_note("_TCSetTimeCodeFlags"); return 0; }

long TextExportGetSettings(long a, long b, long c_, long d, long e, long f) __asm("_TextExportGetSettings");
long TextExportGetSettings(long a, long b, long c_, long d, long e, long f) { shim_note("_TextExportGetSettings"); return 0; }

long TextExportGetTimeFraction(long a, long b, long c_, long d, long e, long f) __asm("_TextExportGetTimeFraction");
long TextExportGetTimeFraction(long a, long b, long c_, long d, long e, long f) { shim_note("_TextExportGetTimeFraction"); return 0; }

long TextExportSetSettings(long a, long b, long c_, long d, long e, long f) __asm("_TextExportSetSettings");
long TextExportSetSettings(long a, long b, long c_, long d, long e, long f) { shim_note("_TextExportSetSettings"); return 0; }

long TextExportSetTimeFraction(long a, long b, long c_, long d, long e, long f) __asm("_TextExportSetTimeFraction");
long TextExportSetTimeFraction(long a, long b, long c_, long d, long e, long f) { shim_note("_TextExportSetTimeFraction"); return 0; }

long TextMediaAddHiliteSample(long a, long b, long c_, long d, long e, long f) __asm("_TextMediaAddHiliteSample");
long TextMediaAddHiliteSample(long a, long b, long c_, long d, long e, long f) { shim_note("_TextMediaAddHiliteSample"); return 0; }

long TextMediaAddTESample(long a, long b, long c_, long d, long e, long f) __asm("_TextMediaAddTESample");
long TextMediaAddTESample(long a, long b, long c_, long d, long e, long f) { shim_note("_TextMediaAddTESample"); return 0; }

long TextMediaAddTextSample(long a, long b, long c_, long d, long e, long f) __asm("_TextMediaAddTextSample");
long TextMediaAddTextSample(long a, long b, long c_, long d, long e, long f) { shim_note("_TextMediaAddTextSample"); return 0; }

long TextMediaDrawRaw(long a, long b, long c_, long d, long e, long f) __asm("_TextMediaDrawRaw");
long TextMediaDrawRaw(long a, long b, long c_, long d, long e, long f) { shim_note("_TextMediaDrawRaw"); return 0; }

long TextMediaFindNextText(long a, long b, long c_, long d, long e, long f) __asm("_TextMediaFindNextText");
long TextMediaFindNextText(long a, long b, long c_, long d, long e, long f) { shim_note("_TextMediaFindNextText"); return 0; }

long TextMediaGetTextProperty(long a, long b, long c_, long d, long e, long f) __asm("_TextMediaGetTextProperty");
long TextMediaGetTextProperty(long a, long b, long c_, long d, long e, long f) { shim_note("_TextMediaGetTextProperty"); return 0; }

long TextMediaHiliteTextSample(long a, long b, long c_, long d, long e, long f) __asm("_TextMediaHiliteTextSample");
long TextMediaHiliteTextSample(long a, long b, long c_, long d, long e, long f) { shim_note("_TextMediaHiliteTextSample"); return 0; }

long TextMediaRawIdle(long a, long b, long c_, long d, long e, long f) __asm("_TextMediaRawIdle");
long TextMediaRawIdle(long a, long b, long c_, long d, long e, long f) { shim_note("_TextMediaRawIdle"); return 0; }

long TextMediaRawSetup(long a, long b, long c_, long d, long e, long f) __asm("_TextMediaRawSetup");
long TextMediaRawSetup(long a, long b, long c_, long d, long e, long f) { shim_note("_TextMediaRawSetup"); return 0; }

long TextMediaSetTextProperty(long a, long b, long c_, long d, long e, long f) __asm("_TextMediaSetTextProperty");
long TextMediaSetTextProperty(long a, long b, long c_, long d, long e, long f) { shim_note("_TextMediaSetTextProperty"); return 0; }

long TextMediaSetTextSampleData(long a, long b, long c_, long d, long e, long f) __asm("_TextMediaSetTextSampleData");
long TextMediaSetTextSampleData(long a, long b, long c_, long d, long e, long f) { shim_note("_TextMediaSetTextSampleData"); return 0; }

long TrackTimeToMediaTime(long a, long b, long c_, long d, long e, long f) __asm("_TrackTimeToMediaTime");
long TrackTimeToMediaTime(long a, long b, long c_, long d, long e, long f) { shim_note("_TrackTimeToMediaTime"); return 0; }

long TuneGetNoteAllocator(long a, long b, long c_, long d, long e, long f) __asm("_TuneGetNoteAllocator");
long TuneGetNoteAllocator(long a, long b, long c_, long d, long e, long f) { shim_note("_TuneGetNoteAllocator"); return 0; }

long TuneGetPartMix(long a, long b, long c_, long d, long e, long f) __asm("_TuneGetPartMix");
long TuneGetPartMix(long a, long b, long c_, long d, long e, long f) { shim_note("_TuneGetPartMix"); return 0; }

long TuneGetTimeBase(long a, long b, long c_, long d, long e, long f) __asm("_TuneGetTimeBase");
long TuneGetTimeBase(long a, long b, long c_, long d, long e, long f) { shim_note("_TuneGetTimeBase"); return 0; }

long TuneGetTimeScale(long a, long b, long c_, long d, long e, long f) __asm("_TuneGetTimeScale");
long TuneGetTimeScale(long a, long b, long c_, long d, long e, long f) { shim_note("_TuneGetTimeScale"); return 0; }

long TuneGetVolume(long a, long b, long c_, long d, long e, long f) __asm("_TuneGetVolume");
long TuneGetVolume(long a, long b, long c_, long d, long e, long f) { shim_note("_TuneGetVolume"); return 0; }

long TuneInstant(long a, long b, long c_, long d, long e, long f) __asm("_TuneInstant");
long TuneInstant(long a, long b, long c_, long d, long e, long f) { shim_note("_TuneInstant"); return 0; }

long TunePreroll(long a, long b, long c_, long d, long e, long f) __asm("_TunePreroll");
long TunePreroll(long a, long b, long c_, long d, long e, long f) { shim_note("_TunePreroll"); return 0; }

long TuneSetBalance(long a, long b, long c_, long d, long e, long f) __asm("_TuneSetBalance");
long TuneSetBalance(long a, long b, long c_, long d, long e, long f) { shim_note("_TuneSetBalance"); return 0; }

long TuneSetHeader(long a, long b, long c_, long d, long e, long f) __asm("_TuneSetHeader");
long TuneSetHeader(long a, long b, long c_, long d, long e, long f) { shim_note("_TuneSetHeader"); return 0; }

long TuneSetHeaderWithSize(long a, long b, long c_, long d, long e, long f) __asm("_TuneSetHeaderWithSize");
long TuneSetHeaderWithSize(long a, long b, long c_, long d, long e, long f) { shim_note("_TuneSetHeaderWithSize"); return 0; }

long TuneSetPartMix(long a, long b, long c_, long d, long e, long f) __asm("_TuneSetPartMix");
long TuneSetPartMix(long a, long b, long c_, long d, long e, long f) { shim_note("_TuneSetPartMix"); return 0; }

long TuneSetPartTranspose(long a, long b, long c_, long d, long e, long f) __asm("_TuneSetPartTranspose");
long TuneSetPartTranspose(long a, long b, long c_, long d, long e, long f) { shim_note("_TuneSetPartTranspose"); return 0; }

long TuneSetSofter(long a, long b, long c_, long d, long e, long f) __asm("_TuneSetSofter");
long TuneSetSofter(long a, long b, long c_, long d, long e, long f) { shim_note("_TuneSetSofter"); return 0; }

long TuneSetSoundLocalization(long a, long b, long c_, long d, long e, long f) __asm("_TuneSetSoundLocalization");
long TuneSetSoundLocalization(long a, long b, long c_, long d, long e, long f) { shim_note("_TuneSetSoundLocalization"); return 0; }

long TuneSetTimeScale(long a, long b, long c_, long d, long e, long f) __asm("_TuneSetTimeScale");
long TuneSetTimeScale(long a, long b, long c_, long d, long e, long f) { shim_note("_TuneSetTimeScale"); return 0; }

long TuneSetVolume(long a, long b, long c_, long d, long e, long f) __asm("_TuneSetVolume");
long TuneSetVolume(long a, long b, long c_, long d, long e, long f) { shim_note("_TuneSetVolume"); return 0; }

long TuneStop(long a, long b, long c_, long d, long e, long f) __asm("_TuneStop");
long TuneStop(long a, long b, long c_, long d, long e, long f) { shim_note("_TuneStop"); return 0; }

long TuneTask(long a, long b, long c_, long d, long e, long f) __asm("_TuneTask");
long TuneTask(long a, long b, long c_, long d, long e, long f) { shim_note("_TuneTask"); return 0; }

long TuneUnroll(long a, long b, long c_, long d, long e, long f) __asm("_TuneUnroll");
long TuneUnroll(long a, long b, long c_, long d, long e, long f) { shim_note("_TuneUnroll"); return 0; }

long TweenerReset(long a, long b, long c_, long d, long e, long f) __asm("_TweenerReset");
long TweenerReset(long a, long b, long c_, long d, long e, long f) { shim_note("_TweenerReset"); return 0; }

long UnsignedFixMulDiv(long a, long b, long c_, long d, long e, long f) __asm("_UnsignedFixMulDiv");
long UnsignedFixMulDiv(long a, long b, long c_, long d, long e, long f) { shim_note("_UnsignedFixMulDiv"); return 0; }

long UpdateMovieInStorage(long a, long b, long c_, long d, long e, long f) __asm("_UpdateMovieInStorage");
long UpdateMovieInStorage(long a, long b, long c_, long d, long e, long f) { shim_note("_UpdateMovieInStorage"); return 0; }

long UpdateMovieResource(long a, long b, long c_, long d, long e, long f) __asm("_UpdateMovieResource");
long UpdateMovieResource(long a, long b, long c_, long d, long e, long f) { shim_note("_UpdateMovieResource"); return 0; }

long VDAddKeyColor(long a, long b, long c_, long d, long e, long f) __asm("_VDAddKeyColor");
long VDAddKeyColor(long a, long b, long c_, long d, long e, long f) { shim_note("_VDAddKeyColor"); return 0; }

long VDCaptureStateChanging(long a, long b, long c_, long d, long e, long f) __asm("_VDCaptureStateChanging");
long VDCaptureStateChanging(long a, long b, long c_, long d, long e, long f) { shim_note("_VDCaptureStateChanging"); return 0; }

long VDClearClipRgn(long a, long b, long c_, long d, long e, long f) __asm("_VDClearClipRgn");
long VDClearClipRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_VDClearClipRgn"); return 0; }

long VDCompressOneFrameAsync(long a, long b, long c_, long d, long e, long f) __asm("_VDCompressOneFrameAsync");
long VDCompressOneFrameAsync(long a, long b, long c_, long d, long e, long f) { shim_note("_VDCompressOneFrameAsync"); return 0; }

long VDDone(long a, long b, long c_, long d, long e, long f) __asm("_VDDone");
long VDDone(long a, long b, long c_, long d, long e, long f) { shim_note("_VDDone"); return 0; }

long VDGetActiveSrcRect(long a, long b, long c_, long d, long e, long f) __asm("_VDGetActiveSrcRect");
long VDGetActiveSrcRect(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetActiveSrcRect"); return 0; }

long VDGetBlackLevelValue(long a, long b, long c_, long d, long e, long f) __asm("_VDGetBlackLevelValue");
long VDGetBlackLevelValue(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetBlackLevelValue"); return 0; }

long VDGetBrightness(long a, long b, long c_, long d, long e, long f) __asm("_VDGetBrightness");
long VDGetBrightness(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetBrightness"); return 0; }

long VDGetCLUTInUse(long a, long b, long c_, long d, long e, long f) __asm("_VDGetCLUTInUse");
long VDGetCLUTInUse(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetCLUTInUse"); return 0; }

long VDGetClipState(long a, long b, long c_, long d, long e, long f) __asm("_VDGetClipState");
long VDGetClipState(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetClipState"); return 0; }

long VDGetCompressionTime(long a, long b, long c_, long d, long e, long f) __asm("_VDGetCompressionTime");
long VDGetCompressionTime(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetCompressionTime"); return 0; }

long VDGetCompressionTypes(long a, long b, long c_, long d, long e, long f) __asm("_VDGetCompressionTypes");
long VDGetCompressionTypes(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetCompressionTypes"); return 0; }

long VDGetContrast(long a, long b, long c_, long d, long e, long f) __asm("_VDGetContrast");
long VDGetContrast(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetContrast"); return 0; }

long VDGetCurrentFlags(long a, long b, long c_, long d, long e, long f) __asm("_VDGetCurrentFlags");
long VDGetCurrentFlags(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetCurrentFlags"); return 0; }

long VDGetDMADepths(long a, long b, long c_, long d, long e, long f) __asm("_VDGetDMADepths");
long VDGetDMADepths(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetDMADepths"); return 0; }

long VDGetDataRate(long a, long b, long c_, long d, long e, long f) __asm("_VDGetDataRate");
long VDGetDataRate(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetDataRate"); return 0; }

long VDGetDeviceNameAndFlags(long a, long b, long c_, long d, long e, long f) __asm("_VDGetDeviceNameAndFlags");
long VDGetDeviceNameAndFlags(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetDeviceNameAndFlags"); return 0; }

long VDGetDigitizerRect(long a, long b, long c_, long d, long e, long f) __asm("_VDGetDigitizerRect");
long VDGetDigitizerRect(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetDigitizerRect"); return 0; }

long VDGetFieldPreference(long a, long b, long c_, long d, long e, long f) __asm("_VDGetFieldPreference");
long VDGetFieldPreference(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetFieldPreference"); return 0; }

long VDGetHue(long a, long b, long c_, long d, long e, long f) __asm("_VDGetHue");
long VDGetHue(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetHue"); return 0; }

long VDGetImageDescription(long a, long b, long c_, long d, long e, long f) __asm("_VDGetImageDescription");
long VDGetImageDescription(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetImageDescription"); return 0; }

long VDGetInput(long a, long b, long c_, long d, long e, long f) __asm("_VDGetInput");
long VDGetInput(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetInput"); return 0; }

long VDGetInputColorSpaceMode(long a, long b, long c_, long d, long e, long f) __asm("_VDGetInputColorSpaceMode");
long VDGetInputColorSpaceMode(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetInputColorSpaceMode"); return 0; }

long VDGetInputFormat(long a, long b, long c_, long d, long e, long f) __asm("_VDGetInputFormat");
long VDGetInputFormat(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetInputFormat"); return 0; }

long VDGetInputGammaValue(long a, long b, long c_, long d, long e, long f) __asm("_VDGetInputGammaValue");
long VDGetInputGammaValue(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetInputGammaValue"); return 0; }

long VDGetInputName(long a, long b, long c_, long d, long e, long f) __asm("_VDGetInputName");
long VDGetInputName(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetInputName"); return 0; }

long VDGetKeyColor(long a, long b, long c_, long d, long e, long f) __asm("_VDGetKeyColor");
long VDGetKeyColor(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetKeyColor"); return 0; }

long VDGetKeyColorRange(long a, long b, long c_, long d, long e, long f) __asm("_VDGetKeyColorRange");
long VDGetKeyColorRange(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetKeyColorRange"); return 0; }

long VDGetMaskPixMap(long a, long b, long c_, long d, long e, long f) __asm("_VDGetMaskPixMap");
long VDGetMaskPixMap(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetMaskPixMap"); return 0; }

long VDGetMaskandValue(long a, long b, long c_, long d, long e, long f) __asm("_VDGetMaskandValue");
long VDGetMaskandValue(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetMaskandValue"); return 0; }

long VDGetMaxAuxBuffer(long a, long b, long c_, long d, long e, long f) __asm("_VDGetMaxAuxBuffer");
long VDGetMaxAuxBuffer(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetMaxAuxBuffer"); return 0; }

long VDGetMaxSrcRect(long a, long b, long c_, long d, long e, long f) __asm("_VDGetMaxSrcRect");
long VDGetMaxSrcRect(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetMaxSrcRect"); return 0; }

long VDGetNextKeyColor(long a, long b, long c_, long d, long e, long f) __asm("_VDGetNextKeyColor");
long VDGetNextKeyColor(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetNextKeyColor"); return 0; }

long VDGetNumberOfInputs(long a, long b, long c_, long d, long e, long f) __asm("_VDGetNumberOfInputs");
long VDGetNumberOfInputs(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetNumberOfInputs"); return 0; }

long VDGetPLLFilterType(long a, long b, long c_, long d, long e, long f) __asm("_VDGetPLLFilterType");
long VDGetPLLFilterType(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetPLLFilterType"); return 0; }

long VDGetPreferredImageDimensions(long a, long b, long c_, long d, long e, long f) __asm("_VDGetPreferredImageDimensions");
long VDGetPreferredImageDimensions(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetPreferredImageDimensions"); return 0; }

long VDGetPreferredTimeScale(long a, long b, long c_, long d, long e, long f) __asm("_VDGetPreferredTimeScale");
long VDGetPreferredTimeScale(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetPreferredTimeScale"); return 0; }

long VDGetSaturation(long a, long b, long c_, long d, long e, long f) __asm("_VDGetSaturation");
long VDGetSaturation(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetSaturation"); return 0; }

long VDGetSharpness(long a, long b, long c_, long d, long e, long f) __asm("_VDGetSharpness");
long VDGetSharpness(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetSharpness"); return 0; }

long VDGetSoundInputDriver(long a, long b, long c_, long d, long e, long f) __asm("_VDGetSoundInputDriver");
long VDGetSoundInputDriver(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetSoundInputDriver"); return 0; }

long VDGetSoundInputSource(long a, long b, long c_, long d, long e, long f) __asm("_VDGetSoundInputSource");
long VDGetSoundInputSource(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetSoundInputSource"); return 0; }

long VDGetTimeCode(long a, long b, long c_, long d, long e, long f) __asm("_VDGetTimeCode");
long VDGetTimeCode(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetTimeCode"); return 0; }

long VDGetVBlankRect(long a, long b, long c_, long d, long e, long f) __asm("_VDGetVBlankRect");
long VDGetVBlankRect(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetVBlankRect"); return 0; }

long VDGetVideoDefaults(long a, long b, long c_, long d, long e, long f) __asm("_VDGetVideoDefaults");
long VDGetVideoDefaults(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetVideoDefaults"); return 0; }

long VDGetWhiteLevelValue(long a, long b, long c_, long d, long e, long f) __asm("_VDGetWhiteLevelValue");
long VDGetWhiteLevelValue(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGetWhiteLevelValue"); return 0; }

long VDGrabOneFrame(long a, long b, long c_, long d, long e, long f) __asm("_VDGrabOneFrame");
long VDGrabOneFrame(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGrabOneFrame"); return 0; }

long VDGrabOneFrameAsync(long a, long b, long c_, long d, long e, long f) __asm("_VDGrabOneFrameAsync");
long VDGrabOneFrameAsync(long a, long b, long c_, long d, long e, long f) { shim_note("_VDGrabOneFrameAsync"); return 0; }

long VDPreflightGlobalRect(long a, long b, long c_, long d, long e, long f) __asm("_VDPreflightGlobalRect");
long VDPreflightGlobalRect(long a, long b, long c_, long d, long e, long f) { shim_note("_VDPreflightGlobalRect"); return 0; }

long VDReleaseAsyncBuffers(long a, long b, long c_, long d, long e, long f) __asm("_VDReleaseAsyncBuffers");
long VDReleaseAsyncBuffers(long a, long b, long c_, long d, long e, long f) { shim_note("_VDReleaseAsyncBuffers"); return 0; }

long VDResetCompressSequence(long a, long b, long c_, long d, long e, long f) __asm("_VDResetCompressSequence");
long VDResetCompressSequence(long a, long b, long c_, long d, long e, long f) { shim_note("_VDResetCompressSequence"); return 0; }

long VDSetBlackLevelValue(long a, long b, long c_, long d, long e, long f) __asm("_VDSetBlackLevelValue");
long VDSetBlackLevelValue(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetBlackLevelValue"); return 0; }

long VDSetBrightness(long a, long b, long c_, long d, long e, long f) __asm("_VDSetBrightness");
long VDSetBrightness(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetBrightness"); return 0; }

long VDSetClipRgn(long a, long b, long c_, long d, long e, long f) __asm("_VDSetClipRgn");
long VDSetClipRgn(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetClipRgn"); return 0; }

long VDSetClipState(long a, long b, long c_, long d, long e, long f) __asm("_VDSetClipState");
long VDSetClipState(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetClipState"); return 0; }

long VDSetCompression(long a, long b, long c_, long d, long e, long f) __asm("_VDSetCompression");
long VDSetCompression(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetCompression"); return 0; }

long VDSetCompressionOnOff(long a, long b, long c_, long d, long e, long f) __asm("_VDSetCompressionOnOff");
long VDSetCompressionOnOff(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetCompressionOnOff"); return 0; }

long VDSetContrast(long a, long b, long c_, long d, long e, long f) __asm("_VDSetContrast");
long VDSetContrast(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetContrast"); return 0; }

long VDSetDataRate(long a, long b, long c_, long d, long e, long f) __asm("_VDSetDataRate");
long VDSetDataRate(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetDataRate"); return 0; }

long VDSetDestinationPort(long a, long b, long c_, long d, long e, long f) __asm("_VDSetDestinationPort");
long VDSetDestinationPort(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetDestinationPort"); return 0; }

long VDSetDigitizerRect(long a, long b, long c_, long d, long e, long f) __asm("_VDSetDigitizerRect");
long VDSetDigitizerRect(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetDigitizerRect"); return 0; }

long VDSetFieldPreference(long a, long b, long c_, long d, long e, long f) __asm("_VDSetFieldPreference");
long VDSetFieldPreference(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetFieldPreference"); return 0; }

long VDSetFrameRate(long a, long b, long c_, long d, long e, long f) __asm("_VDSetFrameRate");
long VDSetFrameRate(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetFrameRate"); return 0; }

long VDSetHue(long a, long b, long c_, long d, long e, long f) __asm("_VDSetHue");
long VDSetHue(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetHue"); return 0; }

long VDSetInput(long a, long b, long c_, long d, long e, long f) __asm("_VDSetInput");
long VDSetInput(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetInput"); return 0; }

long VDSetInputColorSpaceMode(long a, long b, long c_, long d, long e, long f) __asm("_VDSetInputColorSpaceMode");
long VDSetInputColorSpaceMode(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetInputColorSpaceMode"); return 0; }

long VDSetInputGammaValue(long a, long b, long c_, long d, long e, long f) __asm("_VDSetInputGammaValue");
long VDSetInputGammaValue(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetInputGammaValue"); return 0; }

long VDSetInputStandard(long a, long b, long c_, long d, long e, long f) __asm("_VDSetInputStandard");
long VDSetInputStandard(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetInputStandard"); return 0; }

long VDSetKeyColor(long a, long b, long c_, long d, long e, long f) __asm("_VDSetKeyColor");
long VDSetKeyColor(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetKeyColor"); return 0; }

long VDSetKeyColorRange(long a, long b, long c_, long d, long e, long f) __asm("_VDSetKeyColorRange");
long VDSetKeyColorRange(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetKeyColorRange"); return 0; }

long VDSetMasterBlendLevel(long a, long b, long c_, long d, long e, long f) __asm("_VDSetMasterBlendLevel");
long VDSetMasterBlendLevel(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetMasterBlendLevel"); return 0; }

long VDSetPLLFilterType(long a, long b, long c_, long d, long e, long f) __asm("_VDSetPLLFilterType");
long VDSetPLLFilterType(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetPLLFilterType"); return 0; }

long VDSetPlayThruGlobalRect(long a, long b, long c_, long d, long e, long f) __asm("_VDSetPlayThruGlobalRect");
long VDSetPlayThruGlobalRect(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetPlayThruGlobalRect"); return 0; }

long VDSetPlayThruOnOff(long a, long b, long c_, long d, long e, long f) __asm("_VDSetPlayThruOnOff");
long VDSetPlayThruOnOff(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetPlayThruOnOff"); return 0; }

long VDSetPreferredImageDimensions(long a, long b, long c_, long d, long e, long f) __asm("_VDSetPreferredImageDimensions");
long VDSetPreferredImageDimensions(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetPreferredImageDimensions"); return 0; }

long VDSetPreferredPacketSize(long a, long b, long c_, long d, long e, long f) __asm("_VDSetPreferredPacketSize");
long VDSetPreferredPacketSize(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetPreferredPacketSize"); return 0; }

long VDSetSaturation(long a, long b, long c_, long d, long e, long f) __asm("_VDSetSaturation");
long VDSetSaturation(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetSaturation"); return 0; }

long VDSetSharpness(long a, long b, long c_, long d, long e, long f) __asm("_VDSetSharpness");
long VDSetSharpness(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetSharpness"); return 0; }

long VDSetTimeBase(long a, long b, long c_, long d, long e, long f) __asm("_VDSetTimeBase");
long VDSetTimeBase(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetTimeBase"); return 0; }

long VDSetWhiteLevelValue(long a, long b, long c_, long d, long e, long f) __asm("_VDSetWhiteLevelValue");
long VDSetWhiteLevelValue(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetWhiteLevelValue"); return 0; }

long VDSetupBuffers(long a, long b, long c_, long d, long e, long f) __asm("_VDSetupBuffers");
long VDSetupBuffers(long a, long b, long c_, long d, long e, long f) { shim_note("_VDSetupBuffers"); return 0; }

long VDUseSafeBuffers(long a, long b, long c_, long d, long e, long f) __asm("_VDUseSafeBuffers");
long VDUseSafeBuffers(long a, long b, long c_, long d, long e, long f) { shim_note("_VDUseSafeBuffers"); return 0; }

long VDUseThisCLUT(long a, long b, long c_, long d, long e, long f) __asm("_VDUseThisCLUT");
long VDUseThisCLUT(long a, long b, long c_, long d, long e, long f) { shim_note("_VDUseThisCLUT"); return 0; }

long VideoMediaGetCodecParameter(long a, long b, long c_, long d, long e, long f) __asm("_VideoMediaGetCodecParameter");
long VideoMediaGetCodecParameter(long a, long b, long c_, long d, long e, long f) { shim_note("_VideoMediaGetCodecParameter"); return 0; }

long VideoMediaGetStallCount(long a, long b, long c_, long d, long e, long f) __asm("_VideoMediaGetStallCount");
long VideoMediaGetStallCount(long a, long b, long c_, long d, long e, long f) { shim_note("_VideoMediaGetStallCount"); return 0; }

long VideoMediaGetStatistics(long a, long b, long c_, long d, long e, long f) __asm("_VideoMediaGetStatistics");
long VideoMediaGetStatistics(long a, long b, long c_, long d, long e, long f) { shim_note("_VideoMediaGetStatistics"); return 0; }

long VideoMediaResetStatistics(long a, long b, long c_, long d, long e, long f) __asm("_VideoMediaResetStatistics");
long VideoMediaResetStatistics(long a, long b, long c_, long d, long e, long f) { shim_note("_VideoMediaResetStatistics"); return 0; }

long VideoMediaSetCodecParameter(long a, long b, long c_, long d, long e, long f) __asm("_VideoMediaSetCodecParameter");
long VideoMediaSetCodecParameter(long a, long b, long c_, long d, long e, long f) { shim_note("_VideoMediaSetCodecParameter"); return 0; }

long XMLParseAddAttribute(long a, long b, long c_, long d, long e, long f) __asm("_XMLParseAddAttribute");
long XMLParseAddAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("_XMLParseAddAttribute"); return 0; }

long XMLParseAddAttributeAndValue(long a, long b, long c_, long d, long e, long f) __asm("_XMLParseAddAttributeAndValue");
long XMLParseAddAttributeAndValue(long a, long b, long c_, long d, long e, long f) { shim_note("_XMLParseAddAttributeAndValue"); return 0; }

long XMLParseAddAttributeValueKind(long a, long b, long c_, long d, long e, long f) __asm("_XMLParseAddAttributeValueKind");
long XMLParseAddAttributeValueKind(long a, long b, long c_, long d, long e, long f) { shim_note("_XMLParseAddAttributeValueKind"); return 0; }

long XMLParseAddElement(long a, long b, long c_, long d, long e, long f) __asm("_XMLParseAddElement");
long XMLParseAddElement(long a, long b, long c_, long d, long e, long f) { shim_note("_XMLParseAddElement"); return 0; }

long XMLParseAddMultipleAttributes(long a, long b, long c_, long d, long e, long f) __asm("_XMLParseAddMultipleAttributes");
long XMLParseAddMultipleAttributes(long a, long b, long c_, long d, long e, long f) { shim_note("_XMLParseAddMultipleAttributes"); return 0; }

long XMLParseAddNameSpace(long a, long b, long c_, long d, long e, long f) __asm("_XMLParseAddNameSpace");
long XMLParseAddNameSpace(long a, long b, long c_, long d, long e, long f) { shim_note("_XMLParseAddNameSpace"); return 0; }

long XMLParseGetDetailedParseError(long a, long b, long c_, long d, long e, long f) __asm("_XMLParseGetDetailedParseError");
long XMLParseGetDetailedParseError(long a, long b, long c_, long d, long e, long f) { shim_note("_XMLParseGetDetailedParseError"); return 0; }

long XMLParseSetEventParseRefCon(long a, long b, long c_, long d, long e, long f) __asm("_XMLParseSetEventParseRefCon");
long XMLParseSetEventParseRefCon(long a, long b, long c_, long d, long e, long f) { shim_note("_XMLParseSetEventParseRefCon"); return 0; }

long XMLParseSetOffsetAndLimit(long a, long b, long c_, long d, long e, long f) __asm("_XMLParseSetOffsetAndLimit");
long XMLParseSetOffsetAndLimit(long a, long b, long c_, long d, long e, long f) { shim_note("_XMLParseSetOffsetAndLimit"); return 0; }
