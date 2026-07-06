#include <sys/attr.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
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
 * calls them — see todo_gaps, cf. the ICA/FSSpec dead-API shims.) */
extern CFArrayRef  CGImageSourceCopyTypeExtensions(CGImageSourceRef isrc);
extern CFStringRef CGImageSourceGetTypeWithExtension(CFStringRef ext);
extern CFStringRef CGImageSourceGetTypeWithURL(CFURLRef url);
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


//// TESTING ////
struct coords {
   int x;
   int y;
};

void print_coords(const struct coords *coords);
