/*
 * mediatoolbox_shim.m — self-contained substitute for the MediaToolbox
 * symbols that iLife-era QTKit imports but that modern macOS MediaToolbox
 * no longer exports. Replaces the old split MediaToolboxShim.dylib +
 * MediaToolboxShim_objc.dylib pair with one umbrella dylib so QTKit's
 * single (two-level-namespace) MediaToolbox ordinal resolves entirely here.
 *
 * Functions are no-op stubs returning 0; data keys are NULL pointers
 * (CFString constants the dropped framework used to vend). This matches the
 * behavior of the original hand-built shims — see the project notes.
 *
 * GENERATED: symbol lists extracted from the original shims via nm.
 * Build: clang -dynamiclib -fobjc-arc -install_name @rpath/MediaToolboxShim.dylib \
 *          -framework Foundation -lobjc mediatoolbox_shim.m -o MediaToolboxShim.dylib
 */
#import <Foundation/Foundation.h>

/* --- function stubs (return 0) --- */
long FigCaptionCommandCreateFromPropertyList(void) { return 0; }
long FigCopySetOfFormatReaderSupportedFileExtensions(void) { return 0; }
long FigCopySetOfFormatReaderSupportedMIMETypes(void) { return 0; }
long FigFormatReaderCreateForStream(void) { return 0; }
long FigMediaValidatorCreate(void) { return 0; }
long FigMediaValidatorRelease(void) { return 0; }
long FigPlaybackItemGetFigBaseObject(void) { return 0; }
long FigPlayerFileCreate(void) { return 0; }
long FigPlayerGetFigBaseObject(void) { return 0; }
long FigPlayerStreamCreate(void) { return 0; }
long FigRemakerIsFormatDescriptionProtected(void) { return 0; }
long FigShared_CopyDiskCacheCheckedInIDs(void) { return 0; }
long FigShared_DeleteFromDiskCache(void) { return 0; }
long FigSubtitleSampleCreateFromPropertyList(void) { return 0; }
long FigTrackReaderGetFigBaseObject(void) { return 0; }
long FigVisualContextCopyImageForTime2(void) { return 0; }
long FigVisualContextCreateBasic(void) { return 0; }
long FigVisualContextIsNewImageAvailable(void) { return 0; }
long FigVisualContextSetImageAvailableImmediateCallback(void) { return 0; }
long FigVisualContextSetImageAvailableSequentialCallback(void) { return 0; }
long FigVisualContextTask(void) { return 0; }
long MTCopyStringsForMediaTypeAndSubType(void) { return 0; }

/* --- data key stubs (NULL CFString/CF constants) --- */
void *kFigChapterKey_Name = 0;
void *kFigChapterKey_Time = 0;
void *kFigFormatReaderInstantiationOption_PreferPreciseDurationAndTiming = 0;
void *kFigFormatWriterOption_FileFormat_iTunesFamily = 0;
void *kFigFormatWriterOption_FileFormat_QuickTimeMovie = 0;
void *kFigiTunesMetadataKey_EncodedBy = 0;
void *kFigMediaValidatorKey_RefMovieSecurityPolicy = 0;
void *kFigMetadataCommonKey_AlbumName = 0;
void *kFigMetadataCommonKey_Artist = 0;
void *kFigMetadataCommonKey_Artwork = 0;
void *kFigMetadataCommonKey_Author = 0;
void *kFigMetadataCommonKey_Copyrights = 0;
void *kFigMetadataCommonKey_CreationDate = 0;
void *kFigMetadataCommonKey_Description = 0;
void *kFigMetadataCommonKey_Format = 0;
void *kFigMetadataCommonKey_LastModifiedDate = 0;
void *kFigMetadataCommonKey_Title = 0;
void *kFigMetadataCommonKey_Type = 0;
void *kFigMetadataFormat_ID3 = 0;
void *kFigMetadataFormat_iTunes = 0;
void *kFigMetadataFormat_QuickTimeMetadata = 0;
void *kFigMetadataFormat_QuickTimeUserData = 0;
void *kFigMetadataItemProperty_DataType = 0;
void *kFigMetadataItemProperty_Key = 0;
void *kFigMetadataItemProperty_Keyspace = 0;
void *kFigMetadataItemProperty_LanguageCode = 0;
void *kFigMetadataItemProperty_Locale = 0;
void *kFigMetadataItemProperty_Timestamp = 0;
void *kFigMetadataItemProperty_Value = 0;
void *kFigMetadataProperty_Format = 0;
void *kFigMetadataProperty_Items = 0;
void *kFigPlaybackItemAlternateInfoKey_ExcludeFromAutoSelection = 0;
void *kFigPlaybackItemAlternateInfoKey_ExtendedLanguage = 0;
void *kFigPlaybackItemAlternateInfoKey_Language = 0;
void *kFigPlaybackItemAlternateInfoKey_Name = 0;
void *kFigPlaybackItemAlternateInfoKey_TrackID = 0;
void *kFigPlaybackItemAlternateType_Audio = 0;
void *kFigPlaybackItemAlternateType_ChapterName = 0;
void *kFigPlaybackItemAlternateType_Subtitle = 0;
void *kFigPlaybackItemApertureMode_CleanAperture = 0;
void *kFigPlaybackItemApertureMode_EncodedPixels = 0;
void *kFigPlaybackItemApertureMode_ProductionAperture = 0;
void *kFigPlaybackItemCommonMetadata_CommonKey = 0;
void *kFigPlaybackItemCommonMetadata_Properties = 0;
void *kFigPlaybackItemCommonMetadata_Value = 0;
void *kFigPlaybackItemImageOptionsKey_MaxHeight = 0;
void *kFigPlaybackItemImageOptionsKey_MaxWidth = 0;
void *kFigPlaybackItemNotification_DidPlayToTheEnd = 0;
void *kFigPlaybackItemNotification_DimensionsChanged = 0;
void *kFigPlaybackItemNotification_FailedToBecomeReadyForInspection = 0;
void *kFigPlaybackItemNotification_FailedToBecomeReadyForPlayback = 0;
void *kFigPlaybackItemNotification_ItemWasRemovedFromPlayQueue = 0;
void *kFigPlaybackItemNotification_PlayableRangeChanged = 0;
void *kFigPlaybackItemNotification_ReadyForInspection = 0;
void *kFigPlaybackItemNotification_ReadyForPlayback = 0;
void *kFigPlaybackItemNotification_SeekableRangeChanged = 0;
void *kFigPlaybackItemNotification_StreamLikelyToKeepUp = 0;
void *kFigPlaybackItemNotification_StreamRanDry = 0;
void *kFigPlaybackItemNotification_StreamUnlikelyToKeepUp = 0;
void *kFigPlaybackItemNotification_TimebaseChanged = 0;
void *kFigPlaybackItemNotification_TimeJumped = 0;
void *kFigPlaybackItemParameter_Result = 0;
void *kFigPlaybackItemPlayableTimeInterval_End = 0;
void *kFigPlaybackItemPlayableTimeInterval_Start = 0;
void *kFigPlaybackItemProperty_ApertureMode = 0;
void *kFigPlaybackItemProperty_AudioDeviceChannelMap = 0;
void *kFigPlaybackItemProperty_AvailableAlternateTrackGroups = 0;
void *kFigPlaybackItemProperty_ChapterNames = 0;
void *kFigPlaybackItemProperty_ChosenAlternateTrackIDDictionary = 0;
void *kFigPlaybackItemProperty_ChosenTrackIDArray = 0;
void *kFigPlaybackItemProperty_EnableDownloadWhenInPlayQueue = 0;
void *kFigPlaybackItemProperty_EndTime = 0;
void *kFigPlaybackItemProperty_FileSize = 0;
void *kFigPlaybackItemProperty_Metadata = 0;
void *kFigPlaybackItemProperty_MovieMatrix = 0;
void *kFigPlaybackItemProperty_PlayableTimeIntervals = 0;
void *kFigPlaybackItemProperty_ReverseEndTime = 0;
void *kFigPlaybackItemProperty_SeekableTimeIntervals = 0;
void *kFigPlaybackItemProperty_Timebase = 0;
void *kFigPlaybackItemProperty_TimePitchAlgorithm = 0;
void *kFigPlaybackItemProperty_TrackIDArray = 0;
void *kFigPlaybackItemTimePitchAlgorithm_Spectral = 0;
void *kFigPlaybackItemTimePitchAlgorithm_Varispeed = 0;
void *kFigPlaybackItemTrackProperty_Dimensions = 0;
void *kFigPlaybackItemTrackProperty_EstimatedDataRate = 0;
void *kFigPlaybackItemTrackProperty_FormatDescriptionArray = 0;
void *kFigPlaybackItemTrackProperty_MediaType = 0;
void *kFigPlaybackItemTrackProperty_NominalFrameRate = 0;
void *kFigPlayerAction_None = 0;
void *kFigPlayerAction_Stop = 0;
void *kFigPlayerNotification_ChapterNameChanged = 0;
void *kFigPlayerNotification_ClosedCaptionCommand = 0;
void *kFigPlayerNotification_RateDidChange = 0;
void *kFigPlayerNotification_SubtitleChanged = 0;
void *kFigPlayerNotification_TimedMetadata = 0;
void *kFigPlayerNotification_VolumeDidChange = 0;
void *kFigPlayerProperty_ActionAtEnd = 0;
void *kFigPlayerProperty_AudioDeviceUID = 0;
void *kFigPlayerProperty_AutoSwitchStreamQuality = 0;
void *kFigPlayerProperty_CurrentVideoFrameRate = 0;
void *kFigPlayerProperty_DestinationPixelBufferAttributes = 0;
void *kFigPlayerProperty_DisplayNonForcedSubtitles = 0;
void *kFigPlayerProperty_DisplayTimedMetadata = 0;
void *kFigPlayerProperty_EnableHardwareAcceleratedVideoDecoder = 0;
void *kFigPlayerProperty_PerformanceDictionary = 0;
void *kFigPlayerProperty_PostChapterNameChanges = 0;
void *kFigPlayerProperty_PostClosedCaptionCommands = 0;
void *kFigPlayerProperty_VisualContext = 0;
void *kFigPlayerProperty_Volume = 0;
void *kFigPlayerTimedMetadataNotificationKey_ValueArray = 0;
void *kFigTrackProperty_Enabled = 0;
void *kFigTrackProperty_Matrix = 0;
void *kFigTrackProperty_MetadataReaderTypes = 0;
void *kFigTrackProperty_QuickTimeMetadataReader = 0;
void *kFigTrackProperty_QuickTimeUserDataReader = 0;
void *kFigTrackProperty_Timescale = 0;
void *kFigTrackProperty_UneditedDuration = 0;
void *kFigTrackProperty_Volume = 0;
void *kFigUserDataKeyspace = 0;

/* --- ObjC class stubs ---
 * FigCaptionLayer is once again LIVE in modern MediaToolbox as a PRIVATE class
 * (objc_getClass("FigCaptionLayer") != nil); only its _OBJC_CLASS_$_ symbol is
 * absent from the export trie, so QTKit/CoreMediaAuthoring still bind it here.
 * Defining a class literally named "FigCaptionLayer" would register a DUPLICATE
 * and corrupt the runtime ("implemented in both ... mysterious crashes"). Give
 * the stub a UNIQUE objc name and ALIAS the _OBJC_CLASS_$_/_OBJC_METACLASS_$_
 * symbols to it: the dyld bind resolves but the live host class is never
 * shadowed. (Same technique shimgen/shimdb applies to live auto-stub classes.) */
@interface FigCaptionLayer_86x64fwd : NSObject @end
@implementation FigCaptionLayer_86x64fwd @end
__asm__(
"  .globl _OBJC_CLASS_$_FigCaptionLayer\n"
"  .set _OBJC_CLASS_$_FigCaptionLayer, _OBJC_CLASS_$_FigCaptionLayer_86x64fwd\n"
"  .globl _OBJC_METACLASS_$_FigCaptionLayer\n"
"  .set _OBJC_METACLASS_$_FigCaptionLayer, _OBJC_METACLASS_$_FigCaptionLayer_86x64fwd\n"
);
