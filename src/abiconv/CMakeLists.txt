find_package(Clang REQUIRED PATHS /usr/local/opt/llvm)
include_directories(${CMAKE_SOURCE_DIR}/include/abiconv ${CLANG_INCLUDE_DIRS})

add_executable(abigen abigen.cc typeinfo.cc typeconv.cc loc.cc util.cc)
target_link_libraries(abigen libclang)
add_definitions(${LLVM_DEFINITIONS})
target_compile_options(abigen PRIVATE -Wno-format-security)

find_path(c_headers stdlib.h)
message(${c_headers})

# The "consider set" of symbols abigen will generate ABI-conversion shims
# for. Built from libsystem plus the core C frameworks that legacy i386 apps
# call (CoreFoundation, CoreServices/CarbonCore/LaunchServices/Metadata,
# ApplicationServices). abigen only emits a shim for a symbol that is BOTH in
# this set AND has a parseable function declaration in includes.h, so the two
# are kept in sync. Add a framework here and its umbrella header to includes.h
# to broaden coverage. print_exports_trie.sh falls back to the SDK .tbd when
# the on-disk dylib is absent (macOS 11+ ships them only in the dyld cache).
set(ABICONV_SYM_SOURCES
  /usr/lib/system/libsystem_c.dylib
  /usr/lib/system/libsystem_kernel.dylib
  /usr/lib/system/libsystem_info.dylib
  /usr/lib/system/libsystem_malloc.dylib
  /usr/lib/system/libsystem_pthread.dylib
  # libsystem_m exports the libm math family (floorf/ceilf/roundf/sqrtf/sinf/...,
  # floor/ceil/round/pow/...). Legacy i386 code calls these via cdecl (float arg
  # on the stack, result in st0); unshimmed they bind to NATIVE libSystem (xmm0
  # arg/result, 8-byte `ret`) -> the 8-byte ret over-pops the i386 4-byte return
  # push, fusing it with an adjacent stack value into a bogus PC (iPhoto
  # -[MWLoadingView _updateProgressOrigin] -> floorf). See includes.h (<math.h>).
  /usr/lib/system/libsystem_m.dylib
  # libsqlite3 exports the SQLite C API (sqlite3_open/exec/prepare_v2/step/
  # bind_*/column_*/finalize/close/...). Legacy i386 apps that own a SQLite DB
  # call these via i386 cdecl (4-byte pushed return addr); unshimmed they bind
  # to NATIVE libsqlite3 whose 8-byte `ret` over-pops the i386 4-byte frame,
  # fusing the return addr with an adjacent stack word into a bogus PC. This is
  # the iPhoto library-CREATE-path crash (-> sqlite3_open), which masqueraded as
  # an NSPathStore2 fault. See includes.h (<sqlite3.h>). On macOS 11+ the dylib
  # is cache-only, so print_exports_trie.sh falls back to the SDK .tbd.
  /usr/lib/libsqlite3.dylib
  # libsystem_platform exports the OSAtomic* / OSSpinLock primitives. Legacy
  # iLife code calls OSAtomicAdd32/Barrier directly (i386 cdecl, stack args);
  # unshimmed the call reaches native OSAtomicAdd32 which reads args from
  # registers -> derefs a garbage register -> EXC_BAD_ACCESS (iPhoto Thumbnailer
  # teardown). See includes.h (<libkern/OSAtomic.h>).
  /usr/lib/system/libsystem_platform.dylib
  /System/Library/Frameworks/CoreFoundation.framework/CoreFoundation
  /System/Library/Frameworks/CoreServices.framework/CoreServices
  /System/Library/Frameworks/ApplicationServices.framework/ApplicationServices
  # CoreGraphics is re-exported by ApplicationServices, but re-exports are not in
  # AS's own export trie, so its CG* C functions never got ABI shims and its
  # __CF* data constants (kCGColorSpaceGenericRGB, kCGImageProperty*, ...) never
  # got low-4GB shadows. An i386 `movl [nl_ptr],%eax` of such a 64-bit CFString
  # const truncates -> deref fault (iPhoto main color-space setup, exposed once
  # the PIC-anchor fix resolved the load to the real nl_ptr). List directly so
  # both the functions and the data constants enter the consider set.
  /System/Library/Frameworks/CoreGraphics.framework/CoreGraphics
  # ImageIO is re-exported by ApplicationServices too, so the SAME re-export gap
  # as CoreGraphics: its CGImageSource* / CGImageDestination* C functions never
  # entered the consider set, so abigen emitted no ABI shims and the i386 calls
  # bound straight to native ImageIO. CGImageSourceCreateWithURL(CFURLRef url,
  # CFDictionaryRef options) then received the i386 url arg as a raw proxy-arena
  # HANDLE (never unwrapped) and immediately did CFGetTypeID(url) ->
  # objc_msgSend(handle,_cfTypeID) -> reads arena[slot] as the isa -> SIGSEGV in
  # objc_class::demangledName (iPhoto -[IP_QTUtils bitsPerComponentForFormat:] ->
  # CGImageSourceCreateWithURL, image-format probe). Listing ImageIO directly
  # puts the whole family in the consider set; abigen's CF-record-ptr path
  # (convert_cf_ptr) then unwraps the CFURLRef/CFDictionaryRef args. Generic:
  # any i386 app that decodes images via ImageIO. See includes.h (<ImageIO/*>).
  /System/Library/Frameworks/ImageIO.framework/ImageIO
  # DiskArbitration: iPhoto's PhotoCDManager registerWithDiskArb: calls
  # DASessionCreate / DADiskCreateFromBSDName / DADiskCopyDescription (opaque
  # DASessionRef/DADiskRef + CFDictionaryRef). Unshimmed they reached native
  # DiskArbitration with i386 cdecl stack args -> garbage -> jump-to-garbage
  # crash. The opaque *Ref args/returns bridge via the generic CF-handle path.
  /System/Library/Frameworks/DiskArbitration.framework/DiskArbitration
  # SystemConfiguration: iPhoto's IP_IPHostReachabilityMgr/IP_ReachableHost call
  # SCNetworkReachabilityCreateWithName/GetFlags/ScheduleWithRunLoop/SetCallback,
  # SCDynamicStore*, SCNetworkInterface* — unshimmed they crashed the same way.
  /System/Library/Frameworks/SystemConfiguration.framework/SystemConfiguration
  /System/Library/Frameworks/Foundation.framework/Foundation
  /System/Library/Frameworks/AppKit.framework/AppKit
  # OpenGL: the CGL context API (CGLChoosePixelFormat / CGLCreateContext /
  # CGLSetParameter / CGLFlushDrawable ...) PLUS the classic GL 1.x rendering API
  # (glBegin / glBindTexture / glTexImage2D ...) and GLU (gluPerspective ...,
  # bundled in OpenGL.framework). Legacy i386 apps drive OpenGL directly — the
  # photo-grid / slideshow GL rendering in iPhoto, and games / screensavers /
  # media apps generally. Unshimmed, an i386 cdecl `CGLChoosePixelFormat(attribs,
  # &pix, &n)` (args on the stack) reaches the native x86_64 entry that reads
  # args from registers -> attribs = garbage register -> near-null deref inside
  # OpenGL (the iPhoto CGLChoosePixelFormat crash). Generic: benefits every
  # OpenGL-using target, not just iPhoto. See includes.h (<OpenGL/*>).
  /System/Library/Frameworks/OpenGL.framework/OpenGL
  # libxml2 exports the libxml2 C API (xmlParseMemory/xmlNewNode/xmlAddChild/
  # xmlDocDumpFormatMemory/xmlXPathEvalExpression/htmlParseChunk/xmlStr*/...).
  # Legacy i386 apps that read/write XML or HTML call these via i386 cdecl
  # (4-byte pushed return addr); unshimmed they bind to NATIVE libxml2 whose
  # 8-byte `ret` over-pops the i386 4-byte frame, fusing the return addr with an
  # adjacent stack word into a bogus PC. This is the iWeb document-archiving
  # path (SFArchiving's SFAXMLUnarchiver/SFAXMLBundleArchiver -> sfaxml* ->
  # libxml2, and BLWebView -> htmlParseChunk). The opaque tree pointers
  # (xmlDocPtr/xmlNodePtr/xmlChar*) pass through the generic pointer path; like
  # every other shimmed C library, that relies on the process heap being
  # 32-bit-representable (low-4GB) so the truncated pointer round-trips. Generic:
  # any i386 app using libxml2 (iLife, etc.). On macOS 11+ the dylib is
  # cache-only, so print_exports_trie.sh falls back to the SDK .tbd. See
  # includes.h (<libxml/*>).
  /usr/lib/libxml2.2.dylib
  )

add_custom_command(OUTPUT abiconv.syms
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/print_exports_trie.sh ${ABICONV_SYM_SOURCES} > abiconv.syms
  DEPENDS print_exports_trie.sh
  )

add_custom_target(abiconv_syms
  DEPENDS abiconv.syms
  )

# --- Universal import-driven coverage for the MODERN pass (Phase 1a) ---
# The curated ABICONV_SYM_SOURCES list above was grown reactively, one framework
# at a time, each time a target crashed on an unshimmed present-native symbol —
# and any symbol no listed framework covered (mach_timebase_info from libSystem,
# CGImageCreateWithImageInRect from CoreGraphics, ...) bound DIRECT-to-native and
# mismarshalled (i386 cdecl stack args vs the native fn's SysV registers; the
# native 8-byte ret over-pops the i386 4-byte frame -> fused PC). Those gaps used
# to be patched with per-target hand shims (halo_shim.c). This pass makes the
# coverage UNIVERSAL + import-driven: abigen_modern_manifest.py reads each target
# app-code dylib's undefined externals (`nm -m`, which names each symbol's source
# framework via the two-level bind), maps each source framework to its umbrella
# header, and emits (a) the consider set (union of all targets' imports) and (b)
# a generated header (includes_auto.h) #including those umbrellas so abigen has a
# prototype to marshal from. abigen SELF-FILTERS: a removed-in-64-bit symbol has
# no modern declaration -> abigen skips it -> the SEPARATE legacy pass handles it.
#
# ADDITIVE, low blast radius: the import-driven consider set is UNIONED with the
# curated abiconv.syms (below), so every existing curated shim is preserved; this
# only widens coverage. The translated app-code dylib (NOT the wrapper exec) is
# the discovery input — it preserves the original import names (binds are
# redirected at the opcode level, not in the nlist), so nm -arch x86_64 shows them.
#
# Why --exclude-asm maptable_tramp.asm (and NOT abiconv.asm): maptable_tramp.asm
# hand-defines ~364 `MTSHIM ___sym` shims, 74 of which are NOT also in custom.syms
# -- if the import set re-listed them abigen would emit a second `global ___sym`
# and the link would fail with a duplicate symbol. Excluding the hand-shim asm
# (the designed collision guard, same as the legacy pass) prevents that. We do
# NOT exclude abiconv.asm: it is the modern pass's OWN output, so depending on it
# here would be a build cycle (abiconv_import.syms -> abiconv.asm -> abiconv_all.syms
# -> abiconv_import.syms); and it is redundant anyway, because abigen reads the
# consider set into a std::set and ERASES each symbol on emit, so unioning the
# import set with the curated abiconv.syms already prevents any double-emission
# (abiconv.asm's shim set is a subset of abigen(abiconv.syms)).
#
# Targets contribute their imports; missing targets are skipped (EXISTS guard),
# so this is portable. Extend the list when a new i386 target needs coverage.
set(ABICONV_MODERN_TARGETS
    "$ENV{HOME}/Downloads/iLife11/Quinn.app/Contents/MacOS/Quinn.dylib"
    "$ENV{HOME}/Downloads/iLife11/civ4/Civilization IV.app/Contents/MacOS/civ4.out.dylib"
    "$ENV{HOME}/Downloads/iLife11/Halo.app/Contents/MacOS/Halo.dylib"
    "$ENV{HOME}/Downloads/iLife11/Applications/iPhoto.app/Contents/MacOS/iPhoto.dylib"
    "$ENV{HOME}/Downloads/iLife11/Applications/iWeb.app/Contents/MacOS/iWeb.dylib"
    "$ENV{HOME}/Library/Application Support/Steam/steamapps/common/Portal 2/bin/osx32/engine.dylib"
    CACHE STRING "translated app-code dylibs whose imports drive the universal modern shim set")
set(ABICONV_MODERN_TARGET_ARGS "")
set(ABICONV_MODERN_TARGET_DEPS "")
foreach(t IN LISTS ABICONV_MODERN_TARGETS)
  list(APPEND ABICONV_MODERN_TARGET_ARGS --target "${t}")
  if(EXISTS "${t}")
    list(APPEND ABICONV_MODERN_TARGET_DEPS "${t}")
  endif()
endforeach()

add_custom_command(OUTPUT abiconv_import.syms includes_auto.h abiconv_handshim.syms
  COMMAND python3 ${CMAKE_SOURCE_DIR}/src/86x64/abigen_modern_manifest.py
          ${ABICONV_MODERN_TARGET_ARGS}
          --arch x86_64
          --exclude-asm ${CMAKE_CURRENT_SOURCE_DIR}/maptable_tramp.asm
          --out-syms abiconv_import.syms
          --out-includes includes_auto.h
          --out-ignore abiconv_handshim.syms
  DEPENDS ${CMAKE_SOURCE_DIR}/src/86x64/abigen_modern_manifest.py
          ${CMAKE_CURRENT_SOURCE_DIR}/maptable_tramp.asm
          ${ABICONV_MODERN_TARGET_DEPS}
  )

# The final modern consider set = curated (print_exports_trie) UNION import-driven.
# abigen's -s takes a single file and loads it into a std::set, so a plain cat is
# a correct, dedup-safe union.
add_custom_command(OUTPUT abiconv_all.syms
  COMMAND cat abiconv.syms abiconv_import.syms > abiconv_all.syms
  DEPENDS abiconv.syms abiconv_import.syms
  )

# The modern pass's -i ignore set = curated custom.syms UNION every hand-shim symbol
# (abiconv_handshim.syms). Broadening the parsed headers (includes_auto.h) can make a
# hand-shimmed symbol that is ALSO a curated-framework export newly parseable (e.g.
# ___sysctl: a maptable_tramp.asm MTSHIM that is also a libsystem export, exposed once
# <sys/sysctl.h> is parsed) — abigen would then emit a second `global ___sysctl` and the
# link would fail. Ignoring every hand-shim symbol closes that gap permanently.
add_custom_command(OUTPUT custom_all.syms
  COMMAND cat ${CMAKE_CURRENT_SOURCE_DIR}/custom.syms abiconv_handshim.syms > custom_all.syms
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/custom.syms abiconv_handshim.syms
  )

execute_process(
  COMMAND xcrun --show-sdk-path
  OUTPUT_VARIABLE ABICONV_SDK_PATH
  OUTPUT_STRIP_TRAILING_WHITESPACE
  )
if(NOT ABICONV_SDK_PATH)
  message(WARNING "xcrun --show-sdk-path returned empty; abigen will rely on libclang defaults and likely miss system headers")
endif()

# abigen runs the SDK headers through libclang. The framework umbrella headers
# (CoreFoundation.h etc.) pull in clang builtin headers like <stdarg.h>, which
# live in the clang resource dir, not the SDK. Pass it explicitly or abigen
# fails to parse with "'stdarg.h' file not found".
find_program(ABICONV_CLANG clang HINTS /usr/local/opt/llvm/bin /usr/bin)
execute_process(
  COMMAND ${ABICONV_CLANG} -print-resource-dir
  OUTPUT_VARIABLE ABICONV_CLANG_RESOURCE_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  )
if(NOT ABICONV_CLANG_RESOURCE_DIR)
  message(WARNING "could not determine clang resource dir; abigen may fail to find builtin headers like stdarg.h")
endif()

# libxml2's public headers live in <sdk>/usr/include/libxml2/ and internally
# include each other as <libxml/...>, so that directory must be on the header
# search path for includes.h's <libxml/parser.h> (and its transitive includes)
# to resolve. -isystem (not -I) so libxml2's own header warnings stay quiet.
# Consider set = abiconv_all.syms (curated UNION import-driven). Headers parsed =
# the hand-curated includes.h AND the import-driven includes_auto.h (abigen treats
# each positional arg as an independent TU and erases each symbol from the consider
# set on emit, so symbols declared in both headers are emitted exactly once).
add_custom_command(OUTPUT abiconv.asm
  COMMAND abigen -o abiconv.asm -s abiconv_all.syms -i custom_all.syms --structfile ${CMAKE_CURRENT_SOURCE_DIR}/ignore.structs --data-shadow-file ${CMAKE_CURRENT_SOURCE_DIR}/data_shadows.syms --isysroot "${ABICONV_SDK_PATH}" -X -x -X objective-c -X -resource-dir -X "${ABICONV_CLANG_RESOURCE_DIR}" -X -isystem -X "${ABICONV_SDK_PATH}/usr/include/libxml2" ${CMAKE_CURRENT_SOURCE_DIR}/includes.h ${CMAKE_CURRENT_BINARY_DIR}/includes_auto.h
  DEPENDS abigen abiconv_all.syms custom_all.syms ${CMAKE_CURRENT_SOURCE_DIR}/ignore.structs ${CMAKE_CURRENT_SOURCE_DIR}/data_shadows.syms ${CMAKE_CURRENT_SOURCE_DIR}/includes.h ${CMAKE_CURRENT_BINARY_DIR}/includes_auto.h
  )

add_custom_target(abiconv_asm DEPENDS abiconv.asm)

# --- Legacy shim pass (see src/abiconv/includes_legacy.h, the shimdb legacy expansion) ---
# A SECOND abigen invocation that gives abigen the PROTOTYPES for framework C
# functions whose headers modern macOS deleted, so it emits real ABI-marshalling
# shims for the whole legacy (Carbon/QuickTime/AGL/ICA/...) surface. Two classes
# both get a marshalling shim and differ only in where `call _sym` resolves at
# runtime: SURVIVED symbols (still present on modern macOS, header-deleted) call
# the live native function; DELETED symbols (32-bit-only Carbon/QuickDraw/
# QuickTime, gone from modern macOS) resolve to a per-target graceful stub
# provided by shimgen/shimdb. Both directions are correct because the shim runs
# the i386 cdecl frame -> x86_64 SysV marshalling either way.
#
# Parsed under the extracted MacOSX10.6 SDK as -arch i386: the legacy headers
# gate the classic Carbon APIs behind `#if !__LP64__`, so an x86_64 parse hides
# exactly the DELETED 32-bit-only functions we most want to marshal. abigen's
# arg/return widths come from the type KIND + a hardcoded i386->x86_64 direction
# (get_type_width / sizeof_type are arch-parameterized), NOT from the clang parse
# arch, so parsing i386 is safe and strictly broadens coverage.
#
# The consider set (abiconv_legacy.syms) is (all 10.6 SDK framework exports)
# MINUS (the modern pass's emitted `global ___sym` shims, read from abiconv.asm),
# so no function-shim label collides across the two .asm files. --secondary-pass
# suppresses the singleton data-shadow runtime tables (emitted only by the modern
# pass) for the same reason. Hand shims (carbon_memory.c, ...) are excluded via
# the shared -i custom.syms. If the 10.6 SDK is absent the pass is skipped and
# libabiconv builds with the modern shims only.
set(ABICONV_LEGACY_SDK "$ENV{HOME}/projects/Library/SDKs/MacOSX10.6.sdk"
    CACHE PATH "Extracted MacOSX10.6 SDK providing legacy framework headers")
# Restrict the legacy shim pass to the symbols our TARGETS actually import
# ("only useful symbols"): the whole legacy framework surface is ~9k functions /
# 20MB+ of asm (the classic 32-bit-only Carbon umbrella alone is ~5k), most of
# which no target calls and which take >10 min to assemble. Each target binary
# listed here contributes its imports; the consider set becomes (union of target
# imports) ∩ (legacy framework exports) - (already shimmed). Missing targets are
# skipped, so this is portable. Extend the list (semicolon-separated) when a new
# i386 target needs legacy coverage, or clear it to consider the full surface.
#
# Point at the TRANSLATED app-code dylib (civ4.out.dylib), NOT the bundle's main
# executable: after the first deploy the MacOS/<App> path becomes the x86_64
# WRAPPER (whose imports are just libabiconv/libSystem -> a near-empty 14-symbol
# consider set, silently disabling the legacy pass). The translated dylib keeps
# the original legacy import names verbatim (binds are redirected at the opcode
# level, not in the nlist), so nm -arch x86_64 lists Civ IV's real Carbon imports
# (CreateNibReferenceWithCFBundle/CreateWindowFromNib/SetMenuBarFromNib/... — the
# Carbon-nib UI load — plus the QuickDraw/Window/Menu Mgr surface). Same stable
# input + naming convention as ABICONV_MODERN_TARGETS above.
set(ABICONV_LEGACY_TARGETS
    "$ENV{HOME}/Downloads/iLife11/civ4/Civilization IV.app/Contents/MacOS/civ4.out.dylib"
    "$ENV{HOME}/Downloads/iLife11/Halo.app/Contents/MacOS/Halo.dylib"
    CACHE STRING "translated app-code dylibs whose imports define the legacy shim set")
set(ABICONV_LEGACY_TARGET_ARGS "")
set(ABICONV_LEGACY_TARGET_DEPS "")
foreach(t IN LISTS ABICONV_LEGACY_TARGETS)
  list(APPEND ABICONV_LEGACY_TARGET_ARGS --target ${t})
  if(EXISTS ${t})
    list(APPEND ABICONV_LEGACY_TARGET_DEPS ${t})
  endif()
endforeach()

set(ABICONV_LEGACY_SRC "")
if(EXISTS ${ABICONV_LEGACY_SDK})
  add_custom_command(OUTPUT abiconv_legacy.syms
    COMMAND python3 ${CMAKE_SOURCE_DIR}/src/86x64/shimdb_legacy_considerset.py
            --sdk ${ABICONV_LEGACY_SDK}
            --exclude-asm ${CMAKE_CURRENT_BINARY_DIR}/abiconv.asm
            --exclude-asm ${CMAKE_CURRENT_SOURCE_DIR}/maptable_tramp.asm
            ${ABICONV_LEGACY_TARGET_ARGS}
            -o abiconv_legacy.syms
    DEPENDS abiconv.asm ${CMAKE_CURRENT_SOURCE_DIR}/maptable_tramp.asm
            ${CMAKE_SOURCE_DIR}/src/86x64/shimdb_legacy_considerset.py
            ${ABICONV_LEGACY_TARGET_DEPS}
    )
  add_custom_command(OUTPUT abiconv_legacy.asm
    COMMAND abigen -o abiconv_legacy.asm -s abiconv_legacy.syms
            -i ${CMAKE_CURRENT_SOURCE_DIR}/custom.syms
            --structfile ${CMAKE_CURRENT_SOURCE_DIR}/ignore.structs
            --secondary-pass
            --isysroot "${ABICONV_LEGACY_SDK}"
            -X -x -X objective-c -X -arch -X i386
            -X -resource-dir -X "${ABICONV_CLANG_RESOURCE_DIR}"
            # Carbon's private sub-frameworks (HIToolbox, CarbonCore, ...) on the
            # framework search path so includes_legacy.h can reach a sub-framework
            # header the umbrella omits — specifically <HIToolbox/IBCarbonRuntime.h>
            # (the 32-bit-only Carbon NIB loader, not in Carbon.h/HIToolbox.h).
            -X -F -X "${ABICONV_LEGACY_SDK}/System/Library/Frameworks/Carbon.framework/Versions/A/Frameworks"
            ${CMAKE_CURRENT_SOURCE_DIR}/includes_legacy.h
    DEPENDS abigen abiconv_legacy.syms
            ${CMAKE_CURRENT_SOURCE_DIR}/custom.syms
            ${CMAKE_CURRENT_SOURCE_DIR}/ignore.structs
            ${CMAKE_CURRENT_SOURCE_DIR}/includes_legacy.h
    )
  add_custom_target(abiconv_legacy_asm DEPENDS abiconv_legacy.asm)
  set(ABICONV_LEGACY_SRC abiconv_legacy.asm)
else()
  message(WARNING "10.6 SDK not found at ${ABICONV_LEGACY_SDK}; legacy shim pass disabled")
endif()

# macro(create_abiconv TARGET_NAME INCLUDE_SYMS IGNORE_SYMS IGNORE_STRUCTS)
# endmacro()

macro(create_vararg_conv SYMBOL)
  add_library(vararg_conv_${SYMBOL} OBJECT vararg-conv-t.asm)
  target_compile_options(vararg_conv_${SYMBOL} PRIVATE "-DSYMBOL=${SYMBOL}")
endmacro()

create_vararg_conv(printf)
create_vararg_conv(sprintf)
create_vararg_conv(fprintf)
create_vararg_conv(snprintf)
create_vararg_conv(asprintf)
create_vararg_conv(dprintf)
create_vararg_conv(__sprintf_chk)
create_vararg_conv(scanf)
create_vararg_conv(sscanf)
create_vararg_conv(fscanf)

macro(create_custom_conv SOURCE)
  get_filename_component(SYMBOL ${SOURCE} NAME_WE)
  add_library(custom_conv_${SYMBOL} OBJECT ${SOURCE})
endmacro()

create_custom_conv(getopt.asm)
create_custom_conv(sigaction.asm)
create_custom_conv(libgcc_shim.asm)

macro(create_custom_conv_template SOURCE)
  foreach(SYMBOL ${ARGN})
    add_library(custom_conv_${SYMBOL} OBJECT ${SOURCE})
    target_compile_options(custom_conv_${SYMBOL} PRIVATE "-DSYMBOL=${SYMBOL}")
  endforeach()
endmacro()

create_custom_conv_template(exec.asm execvp)

# v-printf family: the va_list variants. Each instantiates vva-conv-t.asm with
# its symbol; the C shims live in printf-conv.cc.
create_custom_conv_template(vva-conv-t.asm
  vsnprintf vsprintf vfprintf vprintf vasprintf __vsnprintf_chk)

add_library(abiconv SHARED
  abiconv.asm
  ${ABICONV_LEGACY_SRC}
  $<TARGET_OBJECTS:vararg_conv_printf>
  $<TARGET_OBJECTS:vararg_conv_sprintf>
  $<TARGET_OBJECTS:vararg_conv_fprintf>
  $<TARGET_OBJECTS:vararg_conv_snprintf>
  $<TARGET_OBJECTS:vararg_conv_asprintf>
  $<TARGET_OBJECTS:vararg_conv_dprintf>
  $<TARGET_OBJECTS:vararg_conv___sprintf_chk>
  $<TARGET_OBJECTS:vararg_conv_scanf>
  $<TARGET_OBJECTS:vararg_conv_sscanf>
  $<TARGET_OBJECTS:vararg_conv_fscanf>
  $<TARGET_OBJECTS:custom_conv_getopt>
  $<TARGET_OBJECTS:custom_conv_sigaction>
  $<TARGET_OBJECTS:custom_conv_libgcc_shim>
  $<TARGET_OBJECTS:custom_conv_execvp>
  $<TARGET_OBJECTS:custom_conv_vsnprintf>
  $<TARGET_OBJECTS:custom_conv_vsprintf>
  $<TARGET_OBJECTS:custom_conv_vfprintf>
  $<TARGET_OBJECTS:custom_conv_vprintf>
  $<TARGET_OBJECTS:custom_conv_vasprintf>
  $<TARGET_OBJECTS:custom_conv___vsnprintf_chk>
  printf-conv.cc
  prefs_shim.c
  file_shim.c
  malloc_shim.c
  mmap_shim.c
  gestalt_shim.c
  objc_shim.c
  objc_slide.c
  maptable_shim.m
  sysctl_shim.c
  keystate_shim.c
  posix_shim.c
  tls_shim.c
  pthread_sync_shim.c
  cxx_shim.c
  cxx_typeinfo.c
  eh_shim.c
  proc_shim.c
  osatomic_shim.c
  xml_shim.c
  cf_callback_shim.c
  cb_bridge.c
  iokit_shim.c
  carbon_memory.c
  carbon_component.c
  cg_function_shim.c
  quicktime_image.c
  imagecapture_shim.c
  carbon_fsspec_shim.c
  # Civ IV: removed Carbon/QuickDraw/QuickTime-callback subsystems (single-purpose shims,
  # wired through maptable_tramp.asm MTSHIM -> ___<sym> exports). See each file's header.
  qd_shim.c
  carbon_ui_shim.c
  upp_shim.c
  sndmgr_shim.c
  fontmgr_shim.c
  mlte_shim.c
  fmgr_shim.c
  osutil_shim.c
  tec_shim.c
  py_shim.c
  quicktime_movie_shim.c
  keymgr_shim.c
  # libstdc++ i386 COW std::string/std::wstring surface (cow_string_tramp.asm
  # keeps its own MTSHIM + MTSHIM_SRET; wired the same way as the other modules).
  cow_string_shim.c
  # (halo_shim.c / halo_tramp.asm RETIRED: mach_timebase_info / mach_absolute_time
  # are now covered by the universal import-driven modern pass — libSystem maps to
  # <mach/mach_time.h> in includes_auto.h, so abigen emits ___mach_timebase_info /
  # ___mach_absolute_time directly. Phase 1a, see the import-driven block above.)
  lazy_bind.c
  objc_msgSend.asm
  objc_reverse.asm
  cb_tramp.asm
  block_tramp.asm
  dlsym_tramp.asm
  init_trampoline.asm
  maptable_tramp.asm
  cow_string_tramp.asm
  eh_tramp.asm
  dyld_stub_binder.asm
  dyld_func_lookup.asm
  )
# -undefined dynamic_lookup lets file_shim.c reference symbols defined by
# the wrapper executable (_86x64_stderr_shim, _86x64_alloc_low_4gb) that
# only exist once the wrapper is in the process.
target_link_options(abiconv PRIVATE -Wl,-undefined,dynamic_lookup)
# objc_shim.c bridges into the live ObjC runtime; Foundation must be loaded
# so objc_getClass("NSString") etc. resolve. The C-function shims call the
# real x86_64 framework entry points (_Gestalt, _CFRetain, _LSCopy..., etc.),
# so link the frameworks whose exports we bridge.
target_link_libraries(abiconv System objc
  "-framework Foundation"
  "-framework CoreFoundation"
  "-framework CoreServices"
  "-framework ApplicationServices"
  "-framework IOKit"
  "-framework OpenGL"
  # quicktime_image.c reimplements the QuickTime GraphicsImporter on these:
  # CGImageSource (decode) + CGBitmapContext/CGImage (render into the GWorld).
  "-framework ImageIO"
  "-framework CoreGraphics"
  # Legacy shim pass (abiconv_legacy.asm): the SURVIVED (header-deleted but still
  # present) framework functions it marshals resolve their `call _sym` here.
  # Linked so the frameworks are loaded in-process and the flat dynamic_lookup
  # finds the live entry points without relying on the translated app's own load
  # commands. These all still link on modern macOS (AGL/QuickTime are GONE, so
  # those resolve via app-driven loading + shimgen instead — not linked here).
  "-framework Carbon"
  "-framework Security"
  "-framework AddressBook"
  "-framework DiscRecording"
  "-framework ICADevices"
  )
add_dependencies(abiconv abiconv_asm)
if(TARGET abiconv_legacy_asm)
  add_dependencies(abiconv abiconv_legacy_asm)
endif()

install(TARGETS abiconv DESTINATION opt/86x64)

add_executable(info info.cc typeinfo.cc typeconv.cc loc.cc util.cc)
target_link_libraries(info libclang)
add_definitions(${LLVM_DEFINITIONS})


add_executable(convgen
  convgen.cc
  util.cc
  )
target_link_libraries(convgen libclang)
add_definitions(${LLVM_DEFINITIONS})

add_executable(syscall_tabgen
  syscall_tabgen.cc
  )

