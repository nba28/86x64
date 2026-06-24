#include <sys/attr.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
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
