/* shimdb curated impl: CoreMedia "kFig*" private DATA constants forwarded to
 * their modern public kCM* equivalents. These are CFStringRef keys (and 3
 * CMTime structs) read by value; we copy the live modern value into the
 * exported legacy slot in a load-time constructor (runs before any media op).
 * dlsym(RTLD_DEFAULT, "kCM...") finds CoreMedia, already loaded; names differ
 * from ours so there is no self-resolution.
 */
#include <CoreFoundation/CoreFoundation.h>
#include <CoreMedia/CMTime.h>
#include <dlfcn.h>
#include <string.h>

CFStringRef kFigByteStreamProperty_URL;
CFStringRef kFigFormatDescriptionExtension_BytesPerRow;
CFStringRef kFigFormatDescriptionExtension_SampleDescriptionExtensionAtoms;
CFStringRef kFigFormatDescriptionExtension_VerbatimSampleDescription;
CFStringRef kFigFormatDescriptionTransferFunction_SMPTE_240M_1995;
CFStringRef kFigFormatDescriptionTransferFunction_UseGamma;
CFStringRef kFigSampleAttachmentKey_DoNotDisplay;
CFStringRef kFigSampleAttachmentKey_IsDependedOnByOthers;
CFStringRef kFigSampleAttachmentKey_NotSync;
CFStringRef kFigSampleAttachmentKey_PartialSync;
CFStringRef kFigSampleBufferAttachmentKey_SpeedMultiplier;
CFStringRef kFigSampleBufferAttachmentKey_TrimDurationAtEnd;
CFStringRef kFigSampleBufferAttachmentKey_TrimDurationAtStart;
CMTime kFigTimeInvalid;
CMTime kFigTimePositiveInfinity;
CMTime kFigTimeZero;

__attribute__((constructor)) static void cm_data_init(void) {
  void *s;
  if ((s = dlsym(RTLD_DEFAULT, "kCMByteStreamProperty_URL"))) kFigByteStreamProperty_URL = *(CFStringRef *)s;
  if ((s = dlsym(RTLD_DEFAULT, "kCMFormatDescriptionExtension_BytesPerRow"))) kFigFormatDescriptionExtension_BytesPerRow = *(CFStringRef *)s;
  if ((s = dlsym(RTLD_DEFAULT, "kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms"))) kFigFormatDescriptionExtension_SampleDescriptionExtensionAtoms = *(CFStringRef *)s;
  if ((s = dlsym(RTLD_DEFAULT, "kCMFormatDescriptionExtension_VerbatimSampleDescription"))) kFigFormatDescriptionExtension_VerbatimSampleDescription = *(CFStringRef *)s;
  if ((s = dlsym(RTLD_DEFAULT, "kCMFormatDescriptionTransferFunction_SMPTE_240M_1995"))) kFigFormatDescriptionTransferFunction_SMPTE_240M_1995 = *(CFStringRef *)s;
  if ((s = dlsym(RTLD_DEFAULT, "kCMFormatDescriptionTransferFunction_UseGamma"))) kFigFormatDescriptionTransferFunction_UseGamma = *(CFStringRef *)s;
  if ((s = dlsym(RTLD_DEFAULT, "kCMSampleAttachmentKey_DoNotDisplay"))) kFigSampleAttachmentKey_DoNotDisplay = *(CFStringRef *)s;
  if ((s = dlsym(RTLD_DEFAULT, "kCMSampleAttachmentKey_IsDependedOnByOthers"))) kFigSampleAttachmentKey_IsDependedOnByOthers = *(CFStringRef *)s;
  if ((s = dlsym(RTLD_DEFAULT, "kCMSampleAttachmentKey_NotSync"))) kFigSampleAttachmentKey_NotSync = *(CFStringRef *)s;
  if ((s = dlsym(RTLD_DEFAULT, "kCMSampleAttachmentKey_PartialSync"))) kFigSampleAttachmentKey_PartialSync = *(CFStringRef *)s;
  if ((s = dlsym(RTLD_DEFAULT, "kCMSampleBufferAttachmentKey_SpeedMultiplier"))) kFigSampleBufferAttachmentKey_SpeedMultiplier = *(CFStringRef *)s;
  if ((s = dlsym(RTLD_DEFAULT, "kCMSampleBufferAttachmentKey_TrimDurationAtEnd"))) kFigSampleBufferAttachmentKey_TrimDurationAtEnd = *(CFStringRef *)s;
  if ((s = dlsym(RTLD_DEFAULT, "kCMSampleBufferAttachmentKey_TrimDurationAtStart"))) kFigSampleBufferAttachmentKey_TrimDurationAtStart = *(CFStringRef *)s;
  if ((s = dlsym(RTLD_DEFAULT, "kCMTimeInvalid"))) memcpy(&kFigTimeInvalid, s, sizeof(CMTime));
  if ((s = dlsym(RTLD_DEFAULT, "kCMTimePositiveInfinity"))) memcpy(&kFigTimePositiveInfinity, s, sizeof(CMTime));
  if ((s = dlsym(RTLD_DEFAULT, "kCMTimeZero"))) memcpy(&kFigTimeZero, s, sizeof(CMTime));
}
