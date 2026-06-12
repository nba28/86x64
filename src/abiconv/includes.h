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

/* Core C frameworks legacy i386 apps call. Their exports are added to the
 * abigen "consider set" via ABICONV_SYM_SOURCES in CMakeLists.txt; abigen
 * emits an ABI-conversion shim for every plain C function declared here that
 * it can marshal (struct/union-by-value, long double, and variadic functions
 * are skipped). Keep this list in sync with ABICONV_SYM_SOURCES. */
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <ApplicationServices/ApplicationServices.h>

/* Foundation + AppKit C functions (NSBeep, NSLog, NSSearchPathForDirectories-
 * InDomains, NSStringFromClass, ...) — legacy i386 apps call these directly,
 * and without a shim the i386 cdecl call (4-byte pushed return address) lands
 * in the real x86_64 function whose `ret` pops 8 bytes, corrupting control
 * flow. These are Objective-C umbrella headers, so abigen parses includes.h
 * with -x objective-c (see CMakeLists.txt). abigen still only emits shims for
 * plain C functions it can marshal; ObjC methods/classes are ignored. */
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

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
