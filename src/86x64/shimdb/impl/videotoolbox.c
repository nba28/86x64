/* shimdb curated impl: VideoToolbox legacy symbols removed on modern macOS.
 *
 * VT*SessionRelease: the legacy reference-release entry points. A VT*Session is
 * a CFTypeRef, so the release is exactly a balanced CFRelease (the modern API
 * dropped the named wrappers in favour of plain CFRelease). A return-0 auto-stub
 * leaks the session AND skips teardown; this is the correct reference semantics.
 */
#include <CoreFoundation/CoreFoundation.h>

void VTCompressionSessionRelease(void *session)   { if (session) CFRelease(session); }
void VTDecompressionSessionRelease(void *session) { if (session) CFRelease(session); }
void VTPixelTransferSessionRelease(void *session) { if (session) CFRelease(session); }

/* kVTDecompressionPropertyKey_CPECryptor: a private Content-Protection-Engine
 * (FairPlay) decryption property key with no modern equivalent. Reading it as a
 * function-pointer stub would crash CF if used as a dictionary key, so export a
 * real, unique CFString. The DRM path it feeds is dead surface on modern macOS. */
const CFStringRef kVTDecompressionPropertyKey_CPECryptor =
    CFSTR("VTDecompressionPropertyKey_CPECryptor");
