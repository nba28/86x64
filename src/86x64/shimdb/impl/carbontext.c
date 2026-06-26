/* shimdb curated impl: Carbon GetApplicationTextEncoding.
 *
 * Still present on modern macOS inside CoreServices (CarbonCore), but the app's
 * dependency was shimmed and the symbol got an auto return-0 stub. Forward to
 * the live CoreServices implementation (resolved from an explicit handle so it
 * cannot resolve back to our own export). Returning 0 (kTextEncodingMacRoman)
 * is the historical default only if CoreServices is somehow unavailable.
 */
#include <dlfcn.h>

typedef unsigned int TextEncoding;

TextEncoding GetApplicationTextEncoding(void) {
    static TextEncoding (*real)(void);
    static int tried;
    if (!tried) {
        tried = 1;
        void *h = dlopen("/System/Library/Frameworks/CoreServices.framework/CoreServices",
                         RTLD_LAZY);
        if (h) real = (TextEncoding (*)(void))dlsym(h, "GetApplicationTextEncoding");
    }
    return real ? real() : 0;
}
