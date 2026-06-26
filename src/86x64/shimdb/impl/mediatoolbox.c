/* shimdb curated impl: MediaToolbox "Fig*" symbols.
 *
 * Two cases, both universal:
 *  (A) symbols that STILL EXIST on modern macOS under their original private
 *      name but were shadowed by an auto return-0 stub (shimgen's runtime probe
 *      missed them). We forward to the live MediaToolbox symbol -- data keys are
 *      copied by value, the one function is reached via a load-time-resolved
 *      tail JMP. dlsym is taken from an EXPLICIT MediaToolbox handle so it can
 *      never resolve back to our own same-named export.
 *  (B) FigPlaybackItem image-generation option KEYS whose engine
 *      (FigPlayerFigCreate et al.) is gone with no modern equivalent. The app
 *      may still build an options dictionary with these as keys before the
 *      (stubbed) engine consumes it, so export a real, unique CFString -- never
 *      a function-pointer stub that CF would dereference as a CFString.
 */
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>

/* (A) still-present MediaToolbox DATA keys (writer / metadata / track options) */
CFStringRef kFigFormatWriterOption_FileFormat_QuickTimeMovie;
CFStringRef kFigFormatWriterOption_FileFormat_iTunesFamily;
CFStringRef kFigMetadataItemProperty_Key;
CFStringRef kFigMetadataItemProperty_Keyspace;
CFStringRef kFigMetadataItemProperty_Locale;
CFStringRef kFigTrackProperty_Enabled;
CFStringRef kFigTrackProperty_UneditedDuration;

/* (A) still-present MediaToolbox FUNCTION: resolved at load, reached via a
 * register-preserving tail JMP through a load-time-populated pointer. */
static void *p_FigTrackReaderGetFigBaseObject;
__asm__(
"  .text\n"
"  .globl _FigTrackReaderGetFigBaseObject\n"
"_FigTrackReaderGetFigBaseObject:\n"
"  jmp *_p_FigTrackReaderGetFigBaseObject(%rip)\n"
);

/* (B) dead FigPlaybackItem image-options keys -- real unique CFStrings */
const CFStringRef kFigPlaybackItemImageOptionsKey_ApertureMode =
    CFSTR("FigPlaybackItemImageOptionsKey_ApertureMode");
const CFStringRef kFigPlaybackItemImageOptionsKey_Deinterlaced =
    CFSTR("FigPlaybackItemImageOptionsKey_Deinterlaced");
const CFStringRef kFigPlaybackItemImageOptionsKey_HighQuality =
    CFSTR("FigPlaybackItemImageOptionsKey_HighQuality");
const CFStringRef kFigPlaybackItemImageOptionsKey_MaxHeight =
    CFSTR("FigPlaybackItemImageOptionsKey_MaxHeight");
const CFStringRef kFigPlaybackItemImageOptionsKey_MaxWidth =
    CFSTR("FigPlaybackItemImageOptionsKey_MaxWidth");

__attribute__((constructor)) static void mtb_init(void) {
    void *h = dlopen("/System/Library/Frameworks/MediaToolbox.framework/MediaToolbox",
                     RTLD_LAZY);
    if (!h)
        h = dlopen("/System/Library/PrivateFrameworks/MediaToolbox.framework/MediaToolbox",
                   RTLD_LAZY);
    if (!h) return;
    void *s;
#define CP(n) do { if ((s = dlsym(h, #n))) n = *(CFStringRef *)s; } while (0)
    CP(kFigFormatWriterOption_FileFormat_QuickTimeMovie);
    CP(kFigFormatWriterOption_FileFormat_iTunesFamily);
    CP(kFigMetadataItemProperty_Key);
    CP(kFigMetadataItemProperty_Keyspace);
    CP(kFigMetadataItemProperty_Locale);
    CP(kFigTrackProperty_Enabled);
    CP(kFigTrackProperty_UneditedDuration);
#undef CP
    p_FigTrackReaderGetFigBaseObject = dlsym(h, "FigTrackReaderGetFigBaseObject");
}
