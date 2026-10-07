#include <sys/attr.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
/* <wchar.h>: the wide-char stdio family (fgetwc/fputwc/fgetws/fputws/getwc/
 * putwc/ungetwc/fwide) is FILE*-taking and was in the abigen consider set
 * (from libSystem) but had NO prototype here, so abigen emitted no ___fgetwc
 * shim and the bind fell through to native libc — the i386 4-byte-ret call
 * over-popped by the native 8-byte ret (fused PC) AND, on a std/fopen'd shim,
 * the raw shim FILE* faulted (the fflush/file_shim class). Providing the
 * prototypes lets abigen emit the pointer-widening shims, whose `call _fgetwc`
 * reaches file_shim.c's FILE*-unwrapping definitions. This also brings the
 * rest of the wide-char surface abigen can marshal (mb/wc conversion:
 * mbrtowc/wcrtomb/mbsrtowcs/..., wmem*, wcscoll/wcsxfrm/wcwidth, wcstoul/...);
 * the hand-shimmed _wcs* string family (wchar_shim.c, in custom.syms) is
 * skipped by abigen so there is no double-emission. The varargs wide-printf/
 * scanf (fwprintf/fwscanf/vfwprintf/vfwscanf) are variadic → abigen skips them
 * (file_shim.c still defines them for a future hand-wire). */
#include <wchar.h>
#include <unistd.h>
#include <sys/ptrace.h>   /* ptrace(): legacy anti-debug call sites (e.g. iWeb
                           * SFUAssertionHandler). Without a shim the i386
                           * 4-byte-ret call reaches native ptrace whose 8-byte
                           * ret over-pops -> fused PC crash. */
#include <errno.h>
#include <pwd.h>
#include <sys/types.h>
#include <uuid/uuid.h>
#include <string.h>
#include <sys/stat.h>
#include <grp.h>
#include <signal.h>
/* pthread mutex/thread primitives live in libsystem_pthread.dylib (added to
 * ABICONV_SYM_SOURCES). Legacy i386 apps call pthread_mutex_init/lock/unlock,
 * pthread_create/once/key_create directly; without a shim the i386 cdecl call
 * (4-byte pushed return address) lands in the real x86_64 pthread function
 * whose `ret` pops 8 bytes, swallowing the adjacent arg into the high half of
 * the return address (control-flow corruption). */
#include <pthread.h>
/* OSAtomic* (OSAtomicAdd32 / OSAtomicAdd32Barrier / OSAtomicCompareAndSwap*,
 * OSSpinLock, ...) live in libsystem_platform.dylib (added to ABICONV_SYM_SOURCES).
 * Legacy i386 iLife code (e.g. Thumbnailer teardown) calls them directly via
 * i386 cdecl (args on the stack); without a shim the call lands in the real
 * x86_64 OSAtomicAdd32, which reads its args from REGISTERS (edi/rsi) — so it
 * dereferences a garbage register as the address pointer -> EXC_BAD_ACCESS
 * (timing-dependent, since the garbage register varies). OSAtomicDeprecated.h
 * declares them extern by default (OSATOMIC_USE_INLINED is unset), so abigen
 * emits a proper marshalling shim. */
#include <libkern/OSAtomic.h>
/* libm single/double-precision math (floorf/ceilf/roundf/sqrtf/sinf/...,
 * floor/ceil/round/pow/...) lives in libsystem_m.dylib (added to
 * ABICONV_SYM_SOURCES). Legacy i386 code calls these via cdecl (float arg on
 * the stack, result in st0); unshimmed the call lands in the native x86_64 fn
 * (arg in xmm0, result in xmm0, 8-byte `ret`) — wrong arg, wrong result, and
 * the 8-byte ret OVER-POPS the i386 4-byte return push, fusing it with an
 * adjacent stack value into a bogus PC (iPhoto -[MWLoadingView
 * _updateProgressOrigin] -> floorf). abigen's float/double-return conversion
 * (xmm0 -> st0) makes the generated shim correct. */
#include <math.h>
/* SQLite C API (sqlite3_open, exec, prepare_v2, step, bind_, column_, finalize,
 * close, ...) lives in libsqlite3.dylib (added to ABICONV_SYM_SOURCES). Legacy
 * i386 apps that own a SQLite database call these directly via i386 cdecl
 * (4-byte pushed return address); unshimmed the call lands in the native
 * x86_64 sqlite3_* whose `ret` pops 8 bytes, over-popping the i386 4-byte
 * return push and fusing it with an adjacent stack value into a bogus PC.
 * This is the iPhoto library-CREATE-path crash: -[... createDummyOldDBFiles]
 * -> sqlite3_open, whose fused PC made it LOOK like an NSPathStore2
 * fileSystemRepresentation fault (the char* return value was just the stack
 * word that got fused in). Generic: any i386 app using SQLite (iLife, etc.). */
#include <sqlite3.h>

/* libxml2 C API (in ABICONV_SYM_SOURCES via /usr/lib/libxml2.2.dylib). Legacy
 * i386 apps that read/write XML or HTML call xmlParseMemory / xmlNewNode /
 * xmlAddChild / xmlDocDumpFormatMemory / xmlXPathEvalExpression /
 * htmlParseChunk / xmlStr* / ... directly via i386 cdecl (4-byte pushed return
 * address); unshimmed the call lands in native libxml2 whose 8-byte `ret`
 * over-pops the i386 4-byte return push and fuses it with an adjacent stack
 * word into a bogus PC. This is iWeb's document-archiving path (SFArchiving's
 * SFAXMLUnarchiver/SFAXMLBundleArchiver -> sfaxml* helpers -> libxml2, and
 * BLWebView -> htmlParseChunk). abigen emits ABI shims for the (almost entirely
 * pointer-based) libxml2 API; opaque tree pointers (xmlDocPtr/xmlNodePtr/
 * xmlChar*) round-trip through the generic low-4GB pointer path like every
 * other shimmed C library. parser.h transitively pulls in tree.h, xmlstring.h,
 * encoding.h, entities.h and xmlerror.h; HTMLparser.h/HTMLtree.h add the HTML
 * push-parser used by BLWebView; xpath.h/xpathInternals.h add the XPath API.
 * The header dir <sdk>/usr/include/libxml2 is added to the abigen clang search
 * path in CMakeLists.txt (libxml2 headers cross-include as <libxml/...>). */
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/entities.h>
#include <libxml/xmlerror.h>

/* Core C frameworks legacy i386 apps call. Their exports are added to the
 * abigen "consider set" via ABICONV_SYM_SOURCES in CMakeLists.txt; abigen
 * emits an ABI-conversion shim for every plain C function declared here that
 * it can marshal (struct/union-by-value, long double, and variadic functions
 * are skipped). Keep this list in sync with ABICONV_SYM_SOURCES. */
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <ApplicationServices/ApplicationServices.h>
/* CoreGraphics is now in ABICONV_SYM_SOURCES (its CG* functions + __CF* data
 * constants are otherwise invisible re-exports of ApplicationServices). AS
 * already transitively includes the CG headers, but list it explicitly so the
 * VarDecls (kCGColorSpaceGenericRGB, ...) are unambiguously parsed. */
#include <CoreGraphics/CoreGraphics.h>
/* ImageIO is now in ABICONV_SYM_SOURCES (its CGImageSource* / CGImageDestination*
 * functions + kCGImageSource* / kCGImageProperty* data constants are otherwise
 * invisible re-exports of ApplicationServices). List the umbrella explicitly so
 * the CGImageSourceCreateWithURL(CFURLRef,CFDictionaryRef) prototype is
 * unambiguously parsed and abigen unwraps its CF-object args (the iPhoto
 * -[IP_QTUtils bitsPerComponentForFormat:] -> CGImageSourceCreateWithURL crash:
 * a raw arena handle reached native ImageIO -> CFGetTypeID -> SIGSEGV). */
#include <ImageIO/ImageIO.h>
/* Headerless ImageIO type-detection SPIs: exported by the framework (present in
 * ImageIO.tbd, in the consider set via ABICONV_SYM_SOURCES) but DELETED from the
 * public header, so abigen never saw a prototype and emitted no shim. The symbol
 * was therefore left binding to the RAW native function — whose x86_64 8-byte
 * `ret` OVER-POPS the i386 4-byte return push, fusing the return address with the
 * adjacent stack word into a bogus PC. This is the File->Import crash: iPhoto's
 * -[... copyTypeExtensions:] calls CGImageSourceCopyTypeExtensions(src) and the
 * over-pop jumps to 0x<stale-code-ptr>_<real-retaddr> (it only CRASHES when that
 * adjacent slot is nonzero, hence the intermittency). Declaring the prototypes
 * here lets abigen emit i386-discipline shims that unwrap the CF-object args and
 * wrap the CF return. Signatures verified live against the system framework
 * (scratchpad/cg_spi_probe*). Universal: any i386 app calling these ImageIO
 * SPIs. (CGImageSourceGetTypeWithData / GetTypeWithDataProvider are intentionally
 * OMITTED — they SIGSEGV even from a clean native x86_64 caller, i.e. dead on
 * modern macOS; a graceful no-op shim is the right tool there if a target ever
 * calls them — a known gap, cf. the ICA/FSSpec dead-API shims.) */
extern CFArrayRef  CGImageSourceCopyTypeExtensions(CGImageSourceRef isrc);
extern CFStringRef CGImageSourceGetTypeWithExtension(CFStringRef ext);
extern CFStringRef CGImageSourceGetTypeWithURL(CFURLRef url);

/* __powidf2: GCC's compiler-rt helper for pow(x, small-int-literal). See
 * ABICONV_SYM_SOURCES for the ABI-hazard writeup; hand-declared because no
 * public header ever names a compiler-rt intrinsic. */
extern double __powidf2(double, int);

/* <unwind.h>: the _Unwind_* personality-routine primitives (see
 * ABICONV_SYM_SOURCES for the libstdc++-redirect writeup). */
#include <unwind.h>
/* DiskArbitration: in ABICONV_SYM_SOURCES so DASessionCreate /
 * DADiskCreateFromBSDName / DADiskCopyDescription (iPhoto's PhotoCDManager
 * registerWithDiskArb:) get ABI shims; opaque DASessionRef/DADiskRef bridge
 * via the generic CF-handle path. */
#include <DiskArbitration/DiskArbitration.h>
/* SystemConfiguration: iPhoto's IP_IPHostReachabilityMgr / IP_ReachableHost
 * call SCNetworkReachabilityCreateWithName / GetFlags / ScheduleWithRunLoop /
 * SetCallback and SCDynamicStore* / SCNetworkInterface*. Opaque SC*Ref bridge
 * via the generic CF-handle path; the reachability callback fn-ptr via cb_bridge. */
#include <SystemConfiguration/SystemConfiguration.h>

/* Foundation + AppKit C functions (NSBeep, NSLog, NSSearchPathForDirectories-
 * InDomains, NSStringFromClass, ...) — legacy i386 apps call these directly,
 * and without a shim the i386 cdecl call (4-byte pushed return address) lands
 * in the real x86_64 function whose `ret` pops 8 bytes, corrupting control
 * flow. These are Objective-C umbrella headers, so abigen parses includes.h
 * with -x objective-c (see CMakeLists.txt). abigen still only emits shims for
 * plain C functions it can marshal; ObjC methods/classes are ignored. */
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

/* OpenGL C API — in ABICONV_SYM_SOURCES (OpenGL.framework). CGL context
 * management (<OpenGL/OpenGL.h>: CGLChoosePixelFormat/CreateContext/...), the
 * classic GL 1.x rendering API (<OpenGL/gl.h>: glBegin/glTexImage2D/...) and GLU
 * (<OpenGL/glu.h>). Legacy i386 apps call these via cdecl (scalar/pointer args
 * on the stack, GLfloat coords too); the native x86_64 entries read args from
 * GP/SSE registers, so unshimmed every call mismarshals — iPhoto's photo-grid
 * GL setup crashes in CGLChoosePixelFormat (garbage attribs ptr -> near-null
 * deref). Float coord args (glVertex3f, glColor4f) marshal through the REAL
 * domain; pointer/array data (glTexImage2D pixels, glGenTextures ids) pass as
 * widened pointers into the i386 caller's low-4GB buffer. */
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <OpenGL/OpenGL.h>

/* in_addr / inet_aton now come from the system headers pulled in by the
 * frameworks above (<netinet/in.h>, <arpa/inet.h>); a manual redefinition
 * here would clash. */
#include <arpa/inet.h>

/* libcurl (Portal 2 HTTP fallback: curl_easy_init/perform/cleanup/duphandle/
 * escape/unescape/strerror/...). See ABICONV_SYM_SOURCES for the ABI-hazard
 * writeup. curl_easy_setopt/curl_easy_getinfo are variadic and abigen skips
 * them (need a hand va_list shim, same family as printf); every other entry
 * point here gets a normal marshalling bridge. */
#include <curl/curl.h>

/* objc_assign_global / objc_enumerationMutation: the ObjC garbage-collector
 * write-barrier family. GC itself is long gone on 64-bit, but old i386 code
 * still calls these two directly as part of manual retain/release-adjacent
 * bookkeeping (clang used to emit objc_assign_global calls for `static id`
 * stores under -fobjc-gc); unshimmed they reach native x86_64 objc_assign_global
 * (SysV register args) from an i386 cdecl call site -> wrong args, and the
 * native 8-byte `ret` over-pops the i386 4-byte return push (fused PC), same
 * family as every other unbridged native call. Both are plain pointer-in/
 * pointer-out C functions, so abigen marshals them like any other. */
#include <objc/objc-auto.h>
/* objc_enumerationMutation itself lives in <objc/runtime.h>, which nothing
 * above transitively includes (Foundation's NSObjCRuntime.h does not pull it
 * in) -- so it and every other plain-C runtime.h entry point abigen can
 * marshal (object_getClassName, sel_isEqual, class_conformsToProtocol, ...)
 * were invisible to abigen even though they were already in the consider
 * set. The runtime-mutation family that objc_shim.c/objc_msgSend.asm hand-
 * implement (objc_getClass, objc_retain, objc_msgSend*, ...) is already
 * excluded via custom.syms's ignore list, so this only ADDS coverage for the
 * plain marshalling-only entry points that list already anticipated. */
#include <objc/runtime.h>

/* <servers/bootstrap.h> — bootstrap_look_up & friends (Portal 2 2026-09-14).
 * WHY THIS MATTERS BEYOND ONE SYMBOL: _bootstrap_look_up was ALREADY in the
 * consider set, but with no prototype abigen could not emit a bridge, so
 * static-interpose left the bind pointing at libSystem's REAL function. A
 * translated caller then reached native code with i386 conventions -- and
 * crucially the callee's `ret` pops EIGHT bytes where the translator pushed
 * FOUR, so control returned to (adjacent stack word << 32 | real return addr).
 * Portal 2 died at rip=0xb03_051044eb, whose low half is a valid
 * libsteam_api address and whose high half is leftover stack. err=0x14
 * (instruction fetch) is the tell.
 * ⚠ A missing PROTOTYPE is therefore not a cosmetic gap: it silently converts a
 * bridged call into a raw cross-ABI one. Audit with
 * src/86x64/unbridged-native-calls.py, which lists exactly these. */
#include <servers/bootstrap.h>

/* <IOKit/IOCFPlugIn.h> — IOCreatePlugInInterfaceForService / IODestroyPlugInInterface
 * (Portal 2 2026-09-14). THE SAME MISSING-PROTOTYPE DEFECT as bootstrap.h above,
 * caught from the other end: most of IOKit arrives transitively through
 * <CoreServices>/<ApplicationServices> (hence ___IOMasterPort, ___IOIteratorNext,
 * ...), but IOCFPlugIn.h is pulled in by NEITHER — not even by IOKitLib.h — so
 * this one entry point had no bridge while its neighbours did.
 *
 * The damage was not the return-width hazard this time but a HANDLE LEAK.
 * CFUUIDGetConstantUUIDWithBytes IS bridged, so it hands the i386 caller a
 * low-4GB proxy-arena handle; IOCreatePlugInInterfaceForService was NOT, so
 * libsdl2 / inputsystem passed that raw handle to native IOKit as a CFUUIDRef.
 * IOKit autoreleased it, and draining the pool messaged the handle: libobjc read
 * the arena SLOT as the receiver's isa, so the real object posed as a Class and
 * the method-cache probe walked a wild bucket (SIGSEGV in objc_msgSend+0x29,
 * with r10 == the real object). See x64_objc_arena_describe(), which labels
 * exactly this in the fault report.
 *
 * ⚠ Bridging the ENTRY POINT does not finish the job: it returns
 * `IOCFPlugInInterface ***`, a COM-style vtable the i386 caller then calls
 * through, so those function pointers are a separate cross-ABI problem. */
#include <IOKit/IOCFPlugIn.h>

/* <iconv.h> — iconv_open / iconv / iconv_close (Portal 2 2026-09-14). The same
 * missing-prototype defect again: all three were in the consider set, so
 * static-interpose left them bound to the REAL libiconv and a translated caller
 * reached it with i386 stack args. vguimatsurface's _V_UTF8ToUCS2 (from
 * CMatSystemSurface::Init) died in iconv_open -> _citrus_iconv_open ->
 * strlcpy -> strlen(0x10): the name "argument" was whatever sat in rdi.
 * No hand marshalling needed: iconv_t is `struct __tag_iconv_t *`, an
 * INCOMPLETE record, so the generic opaque-handle policy applies (a native
 * iconv_t lives >4GB, so iconv_open wraps it and iconv/iconv_close unwrap it),
 * and iconv's `char **` / `size_t *` in-out args are deep-copied both ways. */
#include <iconv.h>


//// TESTING ////
struct coords {
   int x;
   int y;
};

void print_coords(const struct coords *coords);
