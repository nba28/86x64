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
  fprintf(stderr, "[shimauto:%s] %s: removed-OS symbol CALLED with no curated impl -- returning 0 (write one in shimdb/impl/ if its behavior matters)\n", "QTKitShim", sym);
  if (getenv("SHIMGEN_STUB_ABORT")) abort();
  pthread_mutex_unlock(&mtx);
}

@interface QTCaptureDevice : NSObject
@end
@implementation QTCaptureDevice
@end

@interface QTHUDBackgroundView : NSObject
@end
@implementation QTHUDBackgroundView
@end

@interface QTHUDButtonCell : NSObject
@end
@implementation QTHUDButtonCell
@end

@interface QTHUDPopUpButtonCell : NSObject
@end
@implementation QTHUDPopUpButtonCell
@end

@interface QTHUDSlider : NSObject
@end
@implementation QTHUDSlider
@end

@interface QTHUDTimeFormatter : NSObject
@end
@implementation QTHUDTimeFormatter
@end

@interface QTHUDTimelineCell : NSObject
@end
@implementation QTHUDTimelineCell
@end

@interface QTMovie : NSObject
@end
@implementation QTMovie
@end

@interface QTMovieLayer : NSObject
@end
@implementation QTMovieLayer
@end

@interface QTMoviePlaybackController : NSObject
@end
@implementation QTMoviePlaybackController
@end

@interface QTMovieView : NSObject
@end
@implementation QTMovieView
@end

@interface QTMutableMetadataItem : NSObject
@end
@implementation QTMutableMetadataItem
@end

long QTAlternates_LanguageCodeEncoding_ISO_639_2T(long a, long b, long c_, long d, long e, long f) { shim_note("QTAlternates_LanguageCodeEncoding_ISO_639_2T"); return 0; }

long QTAlternates_LanguageCodeEncoding_MacType_LangCode(long a, long b, long c_, long d, long e, long f) { shim_note("QTAlternates_LanguageCodeEncoding_MacType_LangCode"); return 0; }

long QTAlternates_LanguageCodeEncoding_RFC_4646(long a, long b, long c_, long d, long e, long f) { shim_note("QTAlternates_LanguageCodeEncoding_RFC_4646"); return 0; }

long QTAlternates_QTTrack(long a, long b, long c_, long d, long e, long f) { shim_note("QTAlternates_QTTrack"); return 0; }

long QTAlternates_TrackIsAutoExcluded(long a, long b, long c_, long d, long e, long f) { shim_note("QTAlternates_TrackIsAutoExcluded"); return 0; }

long QTGetComponentsFromRationalTime(long a, long b, long c_, long d, long e, long f) { shim_note("QTGetComponentsFromRationalTime"); return 0; }

long QTGetTimeInterval(long a, long b, long c_, long d, long e, long f) { shim_note("QTGetTimeInterval"); return 0; }

long QTHUDTimeFormatterComponentDays(long a, long b, long c_, long d, long e, long f) { shim_note("QTHUDTimeFormatterComponentDays"); return 0; }

long QTHUDTimeFormatterComponentHours(long a, long b, long c_, long d, long e, long f) { shim_note("QTHUDTimeFormatterComponentHours"); return 0; }

long QTHUDTimeFormatterComponentMinutes(long a, long b, long c_, long d, long e, long f) { shim_note("QTHUDTimeFormatterComponentMinutes"); return 0; }

long QTHUDTimeFormatterComponentSeconds(long a, long b, long c_, long d, long e, long f) { shim_note("QTHUDTimeFormatterComponentSeconds"); return 0; }

long QTIndefiniteTime(long a, long b, long c_, long d, long e, long f) { shim_note("QTIndefiniteTime"); return 0; }

long QTIntersectionTimeRange(long a, long b, long c_, long d, long e, long f) { shim_note("QTIntersectionTimeRange"); return 0; }

long QTLocalizedStringFromErrorCode(long a, long b, long c_, long d, long e, long f) { shim_note("QTLocalizedStringFromErrorCode"); return 0; }

long QTMakeTime(long a, long b, long c_, long d, long e, long f) { shim_note("QTMakeTime"); return 0; }

long QTMakeTimeRange(long a, long b, long c_, long d, long e, long f) { shim_note("QTMakeTimeRange"); return 0; }

long QTMakeTimeScaled(long a, long b, long c_, long d, long e, long f) { shim_note("QTMakeTimeScaled"); return 0; }

long QTMakeTimeWithTimeInterval(long a, long b, long c_, long d, long e, long f) { shim_note("QTMakeTimeWithTimeInterval"); return 0; }

long QTMediaCharacteristicVisual(long a, long b, long c_, long d, long e, long f) { shim_note("QTMediaCharacteristicVisual"); return 0; }

long QTMediaType3D(long a, long b, long c_, long d, long e, long f) { shim_note("QTMediaType3D"); return 0; }

long QTMediaTypeClosedCaption(long a, long b, long c_, long d, long e, long f) { shim_note("QTMediaTypeClosedCaption"); return 0; }

long QTMediaTypeFlash(long a, long b, long c_, long d, long e, long f) { shim_note("QTMediaTypeFlash"); return 0; }

long QTMediaTypeMPEG(long a, long b, long c_, long d, long e, long f) { shim_note("QTMediaTypeMPEG"); return 0; }

long QTMediaTypeMovie(long a, long b, long c_, long d, long e, long f) { shim_note("QTMediaTypeMovie"); return 0; }

long QTMediaTypeMusic(long a, long b, long c_, long d, long e, long f) { shim_note("QTMediaTypeMusic"); return 0; }

long QTMediaTypeQTVR(long a, long b, long c_, long d, long e, long f) { shim_note("QTMediaTypeQTVR"); return 0; }

long QTMediaTypeSkin(long a, long b, long c_, long d, long e, long f) { shim_note("QTMediaTypeSkin"); return 0; }

long QTMediaTypeSound(long a, long b, long c_, long d, long e, long f) { shim_note("QTMediaTypeSound"); return 0; }

long QTMediaTypeSprite(long a, long b, long c_, long d, long e, long f) { shim_note("QTMediaTypeSprite"); return 0; }

long QTMediaTypeStream(long a, long b, long c_, long d, long e, long f) { shim_note("QTMediaTypeStream"); return 0; }

long QTMediaTypeSubtitle(long a, long b, long c_, long d, long e, long f) { shim_note("QTMediaTypeSubtitle"); return 0; }

long QTMediaTypeText(long a, long b, long c_, long d, long e, long f) { shim_note("QTMediaTypeText"); return 0; }

long QTMediaTypeTween(long a, long b, long c_, long d, long e, long f) { shim_note("QTMediaTypeTween"); return 0; }

long QTMediaTypeVideo(long a, long b, long c_, long d, long e, long f) { shim_note("QTMediaTypeVideo"); return 0; }

long QTMovieAllowDRMContentAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieAllowDRMContentAttribute"); return 0; }

long QTMovieApertureModeAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieApertureModeAttribute"); return 0; }

long QTMovieApertureModeClean(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieApertureModeClean"); return 0; }

long QTMovieAvailableRangesAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieAvailableRangesAttribute"); return 0; }

long QTMovieChapterListDidChangeNotification(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieChapterListDidChangeNotification"); return 0; }

long QTMovieChapterName(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieChapterName"); return 0; }

long QTMovieChapterStartTime(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieChapterStartTime"); return 0; }

long QTMovieDisplayNameAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieDisplayNameAttribute"); return 0; }

long QTMovieEditedNotification(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieEditedNotification"); return 0; }

long QTMovieEnableForkedH264DecodingAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieEnableForkedH264DecodingAttribute"); return 0; }

long QTMovieHasVideoAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieHasVideoAttribute"); return 0; }

long QTMovieInteractivityFeatureHyperlinks(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieInteractivityFeatureHyperlinks"); return 0; }

long QTMovieInteractivityFeaturesAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieInteractivityFeaturesAttribute"); return 0; }

long QTMovieIsLinearAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieIsLinearAttribute"); return 0; }

long QTMovieIsSteppableAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieIsSteppableAttribute"); return 0; }

long QTMovieLoadStateAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieLoadStateAttribute"); return 0; }

long QTMovieLoadStateDidChangeNotification(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieLoadStateDidChangeNotification"); return 0; }

long QTMovieLoadStateErrorAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieLoadStateErrorAttribute"); return 0; }

long QTMovieLoopsAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieLoopsAttribute"); return 0; }

long QTMovieMessageNotificationParameter(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieMessageNotificationParameter"); return 0; }

long QTMovieMessageStringPostedNotification(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieMessageStringPostedNotification"); return 0; }

long QTMovieMissingComponentsOptionsComponentsToIgnoreData(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieMissingComponentsOptionsComponentsToIgnoreData"); return 0; }

long QTMovieMissingComponentsOptionsConsiderAllTracks(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieMissingComponentsOptionsConsiderAllTracks"); return 0; }

long QTMovieMissingComponentsResultComponentsData(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieMissingComponentsResultComponentsData"); return 0; }

long QTMovieMissingComponentsResultMediaType(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieMissingComponentsResultMediaType"); return 0; }

long QTMovieNaturalSizeAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieNaturalSizeAttribute"); return 0; }

long QTMovieNaturalSizeDidChangeNotification(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieNaturalSizeDidChangeNotification"); return 0; }

long QTMovieOpenForPlaybackAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieOpenForPlaybackAttribute"); return 0; }

long QTMovieRateDidChangeNotification(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieRateDidChangeNotification"); return 0; }

long QTMovieRateDidChangeNotificationParameter(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieRateDidChangeNotificationParameter"); return 0; }

long QTMovieStatusCodeNotificationParameter(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieStatusCodeNotificationParameter"); return 0; }

long QTMovieStatusFlagsNotificationParameter(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieStatusFlagsNotificationParameter"); return 0; }

long QTMovieStatusStringNotificationParameter(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieStatusStringNotificationParameter"); return 0; }

long QTMovieStatusStringPostedNotification(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieStatusStringPostedNotification"); return 0; }

long QTMovieURLAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieURLAttribute"); return 0; }

long QTMovieUsesHardwareVideoDecoderAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("QTMovieUsesHardwareVideoDecoderAttribute"); return 0; }

long QTOSTypeForString(long a, long b, long c_, long d, long e, long f) { shim_note("QTOSTypeForString"); return 0; }

long QTSecurityPolicyBlockAllWiredActionsAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("QTSecurityPolicyBlockAllWiredActionsAttribute"); return 0; }

long QTSecurityPolicyNoRemoteToLocalSiteAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("QTSecurityPolicyNoRemoteToLocalSiteAttribute"); return 0; }

long QTStringForOSType(long a, long b, long c_, long d, long e, long f) { shim_note("QTStringForOSType"); return 0; }

long QTStringFromTime(long a, long b, long c_, long d, long e, long f) { shim_note("QTStringFromTime"); return 0; }

long QTTimeCompare(long a, long b, long c_, long d, long e, long f) { shim_note("QTTimeCompare"); return 0; }

long QTTimeDecrement(long a, long b, long c_, long d, long e, long f) { shim_note("QTTimeDecrement"); return 0; }

long QTTimeInTimeRange(long a, long b, long c_, long d, long e, long f) { shim_note("QTTimeInTimeRange"); return 0; }

long QTTimeIncrement(long a, long b, long c_, long d, long e, long f) { shim_note("QTTimeIncrement"); return 0; }

long QTTimeIsIndefinite(long a, long b, long c_, long d, long e, long f) { shim_note("QTTimeIsIndefinite"); return 0; }

long QTTimeRangeEnd(long a, long b, long c_, long d, long e, long f) { shim_note("QTTimeRangeEnd"); return 0; }

long QTTimeRangeFromString(long a, long b, long c_, long d, long e, long f) { shim_note("QTTimeRangeFromString"); return 0; }

long QTTrackDeinterlaceVideoAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("QTTrackDeinterlaceVideoAttribute"); return 0; }

long QTTrackDisplayNameAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("QTTrackDisplayNameAttribute"); return 0; }

long QTTrackHighQualityVideoAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("QTTrackHighQualityVideoAttribute"); return 0; }

long QTTrackIDAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("QTTrackIDAttribute"); return 0; }

long QTTrackIsChapterTrackAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("QTTrackIsChapterTrackAttribute"); return 0; }

long QTTrackMediaTypeAttribute(long a, long b, long c_, long d, long e, long f) { shim_note("QTTrackMediaTypeAttribute"); return 0; }

long QTUIDraw(long a, long b, long c_, long d, long e, long f) { shim_note("QTUIDraw"); return 0; }

long QTUIIsFlippedKey(long a, long b, long c_, long d, long e, long f) { shim_note("QTUIIsFlippedKey"); return 0; }

long QTUIIsFocusedKey(long a, long b, long c_, long d, long e, long f) { shim_note("QTUIIsFocusedKey"); return 0; }

long QTUISizeKey(long a, long b, long c_, long d, long e, long f) { shim_note("QTUISizeKey"); return 0; }

long QTUISizeMini(long a, long b, long c_, long d, long e, long f) { shim_note("QTUISizeMini"); return 0; }

long QTUISizeRegular(long a, long b, long c_, long d, long e, long f) { shim_note("QTUISizeRegular"); return 0; }

long QTUISizeSmall(long a, long b, long c_, long d, long e, long f) { shim_note("QTUISizeSmall"); return 0; }

long QTUIStateActive(long a, long b, long c_, long d, long e, long f) { shim_note("QTUIStateActive"); return 0; }

long QTUIStateDisabled(long a, long b, long c_, long d, long e, long f) { shim_note("QTUIStateDisabled"); return 0; }

long QTUIStateKey(long a, long b, long c_, long d, long e, long f) { shim_note("QTUIStateKey"); return 0; }

long QTUIStatePressed(long a, long b, long c_, long d, long e, long f) { shim_note("QTUIStatePressed"); return 0; }

long QTUIValueKey(long a, long b, long c_, long d, long e, long f) { shim_note("QTUIValueKey"); return 0; }

long QTUIWidgetBackground(long a, long b, long c_, long d, long e, long f) { shim_note("QTUIWidgetBackground"); return 0; }

long QTUIWidgetBackgroundHighlightedFractionKey(long a, long b, long c_, long d, long e, long f) { shim_note("QTUIWidgetBackgroundHighlightedFractionKey"); return 0; }

long QTUIWidgetKey(long a, long b, long c_, long d, long e, long f) { shim_note("QTUIWidgetKey"); return 0; }

long QTUIWidgetSlider(long a, long b, long c_, long d, long e, long f) { shim_note("QTUIWidgetSlider"); return 0; }

long QTUIWidgetSliderDrawOutlineKey(long a, long b, long c_, long d, long e, long f) { shim_note("QTUIWidgetSliderDrawOutlineKey"); return 0; }

long QTUIWidgetSliderHighlightedRangesKey(long a, long b, long c_, long d, long e, long f) { shim_note("QTUIWidgetSliderHighlightedRangesKey"); return 0; }

long QTUIWidgetSliderIndicatorOnlyKey(long a, long b, long c_, long d, long e, long f) { shim_note("QTUIWidgetSliderIndicatorOnlyKey"); return 0; }

long QTUIWidgetSliderNoIndicatorKey(long a, long b, long c_, long d, long e, long f) { shim_note("QTUIWidgetSliderNoIndicatorKey"); return 0; }

long QTZeroTime(long a, long b, long c_, long d, long e, long f) { shim_note("QTZeroTime"); return 0; }
