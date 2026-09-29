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
# only widens coverage. The discovery input is the Apps32 i386 ORIGINAL executable
# (--arch i386, matching ABICONV_MODERN_TARGETS below): it carries the true, un-
# renamed import names. (A DEPLOYED/translated dylib's nlists are rewritten to
# _____X by static-interpose and vanish from `nm`, silently emptying the set — and
# an x86_64 nm of these i386/ppc originals returns 0 undefineds, which is exactly
# the bug the pre-reorg `--arch x86_64` caused: the whole modern surface incl. the
# wchar family dropped to 0, leaving Civ's ___wcslen slot NULL -> ud2.)
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
# ★2026-07-05: seed from the Apps32 i386 ORIGINALS, NOT translated dylibs — a
# translated dylib's nlists are RENAMED by static-interpose to _____X and vanish
# from the abigen consider-set (silently emptying the shim surface). Originals
# carry the true import names. (Old ~/Downloads/iLife11 paths were deleted in the
# 2026-07-05 reorg → the whole legacy Carbon/QT surface silently dropped to 0.)
set(ABICONV_MODERN_TARGETS
    "$ENV{HOME}/projects/translations/Apps32/Civilization IV.app/Contents/MacOS/Civilization IV"
    "$ENV{HOME}/projects/translations/Apps32/Halo.app/Contents/MacOS/Halo"
    "$ENV{HOME}/projects/translations/Apps32/iMovie.app/Contents/MacOS/iMovie"
    "$ENV{HOME}/projects/translations/Apps32/iPhoto.app/Contents/MacOS/iPhoto"
    "$ENV{HOME}/projects/translations/Apps32/iWeb.app/Contents/MacOS/iWeb"
    "$ENV{HOME}/projects/translations/Apps32/Keynote.app/Contents/MacOS/Keynote"
    "$ENV{HOME}/projects/translations/Apps32/Numbers.app/Contents/MacOS/Numbers"
    "$ENV{HOME}/projects/translations/Apps32/Pages.app/Contents/MacOS/Pages"
    "$ENV{HOME}/projects/translations/Apps32/Quinn.app/Contents/MacOS/Quinn"
    CACHE STRING "i386 ORIGINAL app executables (Apps32) whose imports drive the universal modern shim set")
# Extra source pools for the import-closure dep resolution (_i386_closure.py
# --source; searched before _vendor_deps' DEFAULT_SOURCES). Normally empty —
# the default pools cover the curated i386 originals. Useful when verifying
# against a scratch pool without touching the shared framework library.
set(ABICONV_CLOSURE_SOURCES "" CACHE STRING
    "extra source pool dirs (semicolon list) for import-closure dep resolution")
set(ABICONV_CLOSURE_SOURCE_ARGS "")
foreach(s IN LISTS ABICONV_CLOSURE_SOURCES)
  list(APPEND ABICONV_CLOSURE_SOURCE_ARGS --source "${s}")
endforeach()

# Every Apps32 bundle's main executable joins automatically — a new target
# (Plants vs. Zombies) was missing from the hand list above, so its imports
# (CoreAudio AudioGetCurrentHostTime/...) got no bridge and bound raw.
file(GLOB ABICONV_APPS32_BUNDLES "$ENV{HOME}/projects/translations/Apps32/*.app")
foreach(app IN LISTS ABICONV_APPS32_BUNDLES)
  execute_process(COMMAND /usr/libexec/PlistBuddy -c "Print CFBundleExecutable"
                          "${app}/Contents/Info.plist"
                  OUTPUT_VARIABLE exe OUTPUT_STRIP_TRAILING_WHITESPACE
                  ERROR_QUIET RESULT_VARIABLE rc)
  if(rc EQUAL 0 AND EXISTS "${app}/Contents/MacOS/${exe}")
    list(APPEND ABICONV_MODERN_TARGETS "${app}/Contents/MacOS/${exe}")
  endif()
endforeach()
# A non-bundle game tree (Source engine): every module is dlopen'd, so each sits
# outside the launcher's import closure and must be a target itself (Portal 2's
# sound/input/tier0 modules left OpenAL, ForceFeedback, IOKit, libz, NSModule...
# as raw cross-ABI binds while only engine.dylib + libcef were listed).
set(ABICONV_GAME_TREES
    "$ENV{HOME}/Library/Application Support/Steam/steamapps/common/Portal 2"
    CACHE STRING "non-bundle i386 game roots: <root>/bin/osx32/*.dylib + the root executables join the targets")
foreach(root IN LISTS ABICONV_GAME_TREES)
  file(GLOB tree_mods "${root}/bin/osx32/*.dylib" "${root}/*_osx")
  list(APPEND ABICONV_MODERN_TARGETS ${tree_mods})
endforeach()
list(REMOVE_DUPLICATES ABICONV_MODERN_TARGETS)
set(ABICONV_APPS32_EXECS "${ABICONV_MODERN_TARGETS}")

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
          ${ABICONV_CLOSURE_SOURCE_ARGS}
          --arch i386
          --exclude-asm ${CMAKE_CURRENT_SOURCE_DIR}/maptable_tramp.asm
          --out-syms abiconv_import.syms
          --out-includes includes_auto.h
          --out-ignore abiconv_handshim.syms
  DEPENDS ${CMAKE_SOURCE_DIR}/src/86x64/abigen_modern_manifest.py
          ${CMAKE_SOURCE_DIR}/src/86x64/_i386_closure.py
          ${CMAKE_SOURCE_DIR}/src/86x64/_vendor_deps.py
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
    "$ENV{HOME}/projects/translations/Apps32/Civilization IV.app/Contents/MacOS/Civilization IV"
    "$ENV{HOME}/projects/translations/Apps32/Halo.app/Contents/MacOS/Halo"
    "$ENV{HOME}/projects/translations/Apps32/iMovie.app/Contents/MacOS/iMovie"
    "$ENV{HOME}/projects/translations/Apps32/iPhoto.app/Contents/MacOS/iPhoto"
    "$ENV{HOME}/projects/translations/Apps32/iWeb.app/Contents/MacOS/iWeb"
    "$ENV{HOME}/projects/translations/Apps32/Keynote.app/Contents/MacOS/Keynote"
    "$ENV{HOME}/projects/translations/Apps32/Numbers.app/Contents/MacOS/Numbers"
    "$ENV{HOME}/projects/translations/Apps32/Pages.app/Contents/MacOS/Pages"
    "$ENV{HOME}/projects/translations/Apps32/Quinn.app/Contents/MacOS/Quinn"
    CACHE STRING "i386 ORIGINAL app executables (Apps32) whose imports define the legacy Carbon/QT shim set")
# Same Apps32 auto-discovery as the modern pass (a new target like PvZ got
# no legacy Carbon bridges: SetWindowAlpha/window groups stayed NULL).
list(APPEND ABICONV_LEGACY_TARGETS ${ABICONV_APPS32_EXECS})
list(REMOVE_DUPLICATES ABICONV_LEGACY_TARGETS)
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
            ${ABICONV_CLOSURE_SOURCE_ARGS}
            -o abiconv_legacy.syms
    DEPENDS abiconv.asm ${CMAKE_CURRENT_SOURCE_DIR}/maptable_tramp.asm
            ${CMAKE_SOURCE_DIR}/src/86x64/shimdb_legacy_considerset.py
            ${CMAKE_SOURCE_DIR}/src/86x64/_i386_closure.py
            ${CMAKE_SOURCE_DIR}/src/86x64/_vendor_deps.py
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
  vsnprintf vsprintf vfprintf vprintf vasprintf __vsnprintf_chk __snprintf_chk
  __vsprintf_chk printf sprintf fprintf snprintf asprintf dprintf)

add_library(abiconv SHARED
  abiconv.asm
  ${ABICONV_LEGACY_SRC}
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
  $<TARGET_OBJECTS:custom_conv___snprintf_chk>
  $<TARGET_OBJECTS:custom_conv___vsprintf_chk>
  $<TARGET_OBJECTS:custom_conv_printf>
  $<TARGET_OBJECTS:custom_conv_sprintf>
  $<TARGET_OBJECTS:custom_conv_fprintf>
  $<TARGET_OBJECTS:custom_conv_snprintf>
  $<TARGET_OBJECTS:custom_conv_asprintf>
  $<TARGET_OBJECTS:custom_conv_dprintf>
  printf-conv.cc
  prefs_shim.c
  openssl_shim.c
  # nslot_repair.c: load-time repair of abigen's capture-proof native-call
  # slots — a shim's inner `call native_target` must NEVER land in another
  # marshalling shim (the cross-pass ___tolower double-wrap = Civ IV's
  # tolower()==0 "XML Load Error" root cause). See the file header.
  nslot_repair.c
  file_shim.c
  malloc_shim.c
  mmap_shim.c
  gestalt_shim.c
  objc_shim.c
  dyld_image_list.c
  callring_shim.c
  objc_slide.c
  maptable_shim.m
  sysctl_shim.c
  keystate_shim.c
  posix_shim.c
  tls_shim.c
  pthread_sync_shim.c
  coreaudio_hal_shim.c
  coreaudio_ioproc_shim.c
  coreaudio_au_shim.c
  qsort_shim.c
  cxx_shim.c
  cxx_typeinfo.c
  eh_shim.c
  proc_shim.c
  osatomic_shim.c
  xml_shim.c
  cfxml_shim.c
  cf_callback_shim.c
  cb_bridge.c
  lowstack_pool.c
  iokit_shim.c
  iokit_com_shim.c
  carbon_memory.c
  # mp_memory_shim.c: ONE job — Carbon Multiprocessing Services' MEMORY
  # allocators (MPAllocateAligned/MPAllocate/MPFree/MPGetAllocatedBlockSize) on
  # the low-4GB heap. abigen's generic void*-return rule was minting an 8-byte
  # PROXY ARENA HANDLE for their >4GB native pointer, so the app received a
  # handle where it asked for an N-byte buffer. See the file header.
  mp_memory_shim.c
  rm_shim.c
  carbon_component.c
  cg_function_shim.c
  cg_color_shim.c
  cg_geoarray_shim.c
  nscopybits_shim.c
  quicktime_image.c
  imagecapture_shim.c
  carbon_fsspec_shim.c
  # carbon_alias_shim.c: ONE job — the classic Alias Manager. Its FSSpec generation
  # (NewAlias/ResolveAlias/ResolveAliasFile/MatchAlias/...) was a NULL-jump orphan, but
  # MEASURED with src/86x64/nulljump_probe.c under Rosetta the FSRef generation
  # (FSNewAlias/FSResolveAlias/FSResolveAliasFile/FSIsAliasFile/GetAliasSize) is ALIVE on
  # x86_64 — so this BRIDGES onto Apple's own surviving engine (genuine alias records in,
  # Apple's own parser out) rather than reimplementing the format, and adds a CFURL
  # bookmark appendix for the one thing that engine no longer does: follow a moved file.
  # Shares the FSSpec<->POSIX resolver in carbon_fsspec_shim.c through carbon_fsspec.h.
  carbon_alias_shim.c
  # FSOpenResFile with the flattened-resource-file fallback: codesign forbids HFS
  # resource forks in a bundle, so `m64 forks --flatten` moves them to the data
  # fork / a .86x64rsrc sidecar and this finds them there (FSOpenResourceFile).
  rsrc_datafork_shim.c
  # Civ IV: removed Carbon/QuickDraw/QuickTime-callback subsystems (single-purpose shims,
  # wired through maptable_tramp.asm MTSHIM -> ___<sym> exports). See each file's header.
  # qd_gworld.c = the CG-backed classic QuickDraw substrate (GWorlds/PixMaps/CopyBits/
  # drawing/GDevice); qd_shim.c = the small port-independent slice (patterns/points/cursors).
  qd_gworld.c
  qd_shim.c
  carbon_ui_shim.c
  # agl_drawable_shim.c: ONE job — the AGL drawable binding of a Carbon window.
  # 64-bit macOS deleted GetWindowPort, so the universal Carbon+OpenGL idiom
  # `aglSetDrawable(ctx, GetWindowPort(win))` attached a NULL drawable and no GL
  # context ever came up. qd_gworld.c now returns a REAL window-backed port;
  # this file recognises it structurally and routes the attach to the surviving
  # native aglSetWindowRef, and owns the ctx->drawable association because
  # native aglGetDrawable is blind to a WindowRef attach (measured).
  agl_drawable_shim.c
  # cg_display_fullscreen_shim.c / cg_display_tramp.asm: ONE job — the classic
  # FULLSCREEN DISPLAY-MODE idiom (CGCaptureAllDisplays +
  # CGDisplayBestModeForParameters + CGDisplaySwitchToMode). PERFORMING it is
  # fatal on modern macOS: the reconfiguration makes AppKit setFrame: every
  # window in the process and -[NSCGSWindow _createContext] cannot be created
  # for an NSCarbonWindow -> SIGILL in NSCGSPanic. (Measured: a plain RESIZING
  # SetWindowBounds panics identically with no display API in the stack, so it
  # is the window class, not the display switch — and a MOVE-only survives.)
  # So the switch is never performed: a VIRTUAL mode is recorded, every mode
  # query (CurrentMode/Bounds/PixelsWide/High/BitsPerPixel) answers from it,
  # BestModeForParameters synthesises an exact match for the requested geometry
  # (native returns 1920x1200 for a 800x600 request — modern Macs have no small
  # modes), the capture family is tracked rather than performed, and the app's
  # own GL window is MOVED (never resized) to the display origin so virtual and
  # screen coordinates coincide. Symbols listed in custom.syms so abigen never
  # dup-emits them.
  cg_display_fullscreen_shim.c
  # cfstring_range_shim.c / cfstring_range_tramp.asm: ONE job — restore classic
  # CoreFoundation's TOLERANT CFRange handling for CFStringGetCharacters.
  # Classic CF never validated the range, so asking for length+1 to pick up a
  # NUL terminator was a normal idiom. Modern CF validates in SOME concrete
  # string classes only: -[NSTaggedPointerString getCharacters:range:] throws
  # while __NSCFString tolerates, so the SAME call site is fatal for a short
  # string and harmless for a longer one. The throw is an ObjC exception
  # crossing a 2006 C frame that cannot catch it -> _objc_terminate -> abort.
  # Clamps the copy to the characters that exist and zero-fills the tail (a
  # DETERMINISTIC terminator, better than the garbage classic CF copied).
  # Symbol listed in custom.syms so abigen never dup-emits it.
  cfstring_range_shim.c
  # cfnumber_width_shim.c / cfnumber_width_tramp.asm: ONE job — the CFNumber types
  # named after a C type (Long, CFIndex, NSInteger, CGFloat) have the i386 width.
  cfnumber_width_shim.c
  # carbon_datetime_shim.c / carbon_datetime_tramp.asm: ONE job — the classic
  # Date & Time Utilities (SecondsToDate / DateToSeconds / LongSecondsToDate /
  # LongDateToSeconds), which modern macOS REMOVED. abigen still emits a bridge,
  # the pipeline weakens the dangling bind to NULL, and the call jumps to 0.
  # Classic epoch is 1904-01-01 in LOCAL time (2082844800s before the Unix
  # epoch) — using gmtime here would silently shift every date by the timezone.
  carbon_datetime_shim.c
  # carbon_timemgr_shim.c: ONE job — the classic Time Manager (InsTime/PrimeTime/
  # RmvTime family) on dispatch timers, the i386 TMTask kept in place and tmAddr
  # called through the i386 callback bridge (Portal 2 Bink's RADCB timer).
  carbon_timemgr_shim.c
  # carbon_fscatinfo_shim.c / carbon_fscatinfo_tramp.asm: ONE job — marshal
  # FSCatalogInfo, the one classic struct whose two layouts genuinely differ
  # (FSPermissionInfo ends in an FSFileSecurityRef POINTER: 144 bytes on i386,
  # 148 on x86_64). typeconv's deep-copy path stages the pointee in a scratch
  # slot sized for ONE struct, so the BULK entry points — which fill
  # `actualObjects` entries — overrun the shim frame into the i386 caller's
  # locals. Measured on Civ IV: 11 * 148 into an 880-byte frame.
  carbon_fscatinfo_shim.c
  # agl_renderer_shim.c: ONE job — the AGL renderer-info ENUMERATION family.
  # aglQueryRendererInfo takes a QuickDraw GDHandle, so on 64-bit it is a dead
  # entry point (measured: NULL for every input, aglGetError()==0) and every
  # 32-bit-era "have I got an accelerated GPU?" check counts zero renderers.
  # Routed to the surviving aglQueryRendererInfoForCGDirectDisplayIDs through
  # the qd_gworld.c GDevice registry that minted the GDevice from a
  # CGDirectDisplayID in the first place.
  agl_renderer_shim.c
  # cgl_macro_shim.c: ONE job — the CGL/AGL CONTEXT OBJECT. <OpenGL/CGLMacro.h>
  # #defines every glXxx() to jump through the context's embedded 760-entry
  # GLIFunctionDispatch table, so a 32-bit game DEREFERENCES its AGLContext and
  # never calls a GL symbol. A proxy arena handle read as a _CGLContextObject
  # gives a truncated ->rend and 0 for every ->disp slot => jmp *0. This returns
  # an i386-layout SHADOW whose disp slots are gli_tramp.asm thunks.
  cgl_macro_shim.c
  # cgl_fullscreen_shim.m: ONE job — the CGL FULLSCREEN CONTEXT. Modern CGL has
  # no fullscreen drawables (CGLSetFullScreen -> 10012); present the context in
  # a window of the display's virtual size instead (PvZ).
  cgl_fullscreen_shim.m
  # fault_report_shim.c: ONE job — make a fatal fault in translated i386 code
  # speak. The OS unwinder cannot walk a translated frame (and walks NOTHING at
  # all when rip==0), so the call site is otherwise unrecoverable; on a rip==0
  # fault [rsp] IS the call site and this dumps it as <image>+0xOFFSET.
  # Diagnostic only, OFF unless M64_FAULT_REPORT=1, and it chains to the previous
  # handler so the process still dies identically.
  fault_report_shim.c
  # carbon_nib_shim.c: REAL IBCarbon nib window/control materialization (fixes the
  # -5601 non-compositing window failure + dropped IBCarbonEditText UserPane gap).
  carbon_nib_shim.c
  # carbon_window_shim.c: the PROGRAMMATIC half of the same -5601 rule — 64-bit
  # HIToolbox has no non-compositing window model, so every 32-bit-era
  # CreateNewWindow call must have kWindowCompositingAttribute forced on.
  carbon_window_shim.c
  # Reclassified abigen-DANGLING classic Carbon (removed on 64-bit macOS): abigen
  # forwarded these to now-removed natives (dangling `call _X` -> abort if reached).
  # Real HIView bridges where a modern equivalent survives, else signature-correct
  # graceful. MTSHIM presence removes them from the abigen legacy pass.
  carbon_control_shim.c    # Control/Window/Menu Manager (real bridges + graceful)
  # REAL CreateScrollingTextBoxControl: classic 'TEXT'/'styl' resources drawn
  # by a self-drawing compositing HIView via CoreText (Halo EULA license text).
  carbon_scrolltext_shim.c
  # One-shot AppKit window-host bootstrap (NSApplicationLoad) for pure-Carbon
  # translated apps: cures the "Unable to bridge WindowManagement interface -
  # no delegate has been set" -> NSCGSPanic _createContext death, letting
  # Carbon windows actually materialize on screen.
  carbon_appkit_host.c
  # REAL Appearance Manager theme text (DrawThemeTextBox/DrawThemeText/
  # GetThemeTextDimensions) via CoreText — replaces silent no-draw no-ops.
  carbon_themetext_shim.c
  # REAL classic Dialog Manager: materialize ANY 'DLOG'+'DITL' as a modal AppKit
  # window (editable NSTextField per editText item, NSButton per ctrlItem) run by
  # a real ModalDialog event pump — so an app's "prompt until valid input" modal
  # loop works (Halo's "Enter Your Halo Product Key" CD-key gate). Needs AppKit.
  carbon_dialog_shim.m
  # REAL clickable+movable StandardAlert/Alert family on AppKit NSAlert. The
  # Appearance-Manager standard-alert APIs (CreateStandardAlert/RunStandardAlert/
  # StandardAlert/GetStandardAlertDefaultParams) were unshimmed -> bound to live
  # 64-bit HIToolbox, which in a pure-Carbon translated process (never foreground,
  # starved event pump) drew a DEAD alert (unclickable, immovable). Route them
  # through the same real -runModal NSAlert as carbon_dialog_shim.m. Needs AppKit.
  carbon_standardalert_shim.m
  # The AUTHENTIC classic Carbon alert the standard-alert family now runs FIRST:
  # a real Carbon window (CreateNewWindow, movable modal) whose contents are
  # SELF-DRAWN classic HIViews (com.apple.hiview + kEventControlDraw/HitTest/
  # Track through CoreGraphics + CoreText, Lucida Grande) — the same live
  # substrate as the EULA scroll box / settings controls. Measured: the NATIVE
  # HIToolbox standard alert is inert on modern macOS (no click, no Return, not
  # even an AX press dismisses it) AND draws modern chrome, so it is neither
  # functional nor authentic. Falls back to the AppKit modal if it can't run.
  carbon_classic_alert.c
  # THE shared classic-Carbon widget substrate: the real Carbon compositing
  # window plus the SELF-DRAWN classic HIViews (push button / check box / radio,
  # Lucida Grande text, and the classic sunken EDIT FIELD with a real blinking
  # caret, click-to-place, drag selection and Tab focus) that 64-bit macOS
  # deleted outright.  Factored out of carbon_classic_alert.c so the classic
  # alert, the classic Dialog Manager and the IBCarbon nib materializer all draw
  # the SAME authentic classic chrome instead of each growing a private copy.
  carbon_classic_widgets.c
  # The classic Dialog Manager's VIEW layer: an already-parsed 'DLOG'+'DITL'
  # materialized as a real classic Carbon window on that substrate and run by a
  # real Carbon modal session, returning the classic item numbers.  Replaces the
  # modern AppKit dialog (kept in carbon_dialog_shim.m purely as the fallback).
  carbon_classic_dialog.c
  # Restore the user's system keyboard input source when a translated app exits
  # (Halo forces US in-game and leaves it there; snapshot at load, re-select at
  # exit). Non-invasive: does not suppress the app's in-run layout forcing.
  keyboard_input_shim.c
  carbon_classic_ui_shim.c # classic TextEdit/List/Drag/Theme (graceful dead surface)
  qt_hitoolbox_shim.c
  carbon_fwd_shim.c
  upp_shim.c
  # Navigation Services -> AppKit NSOpenPanel/NSSavePanel (classic Carbon file
  # dialogs, removed in 64-bit). Real bridge; reply.selection AEDescList round-
  # trips via the x64_objc_wrap AE-desc table. Needs -framework AppKit.
  nav_shim.m
  # SoundManager: classic Sound Manager reimplemented on CoreAudio AudioQueue.
  # sndmgr_shim.c = the API + AudioQueue lifecycle + i386 callback bridge;
  # snd_convert.c = the pure (framework-free) SoundHeader/'snd '-resource decoder
  # (unit-tested headless by snd_convert_test.c). Needs -framework AudioToolbox.
  sndmgr_shim.c
  snd_convert.c
  fontmgr_shim.c
  mlte_shim.c
  fmgr_shim.c
  osutil_shim.c
  # shared substrate for classic INPUT geometry (port origin) and mouse-button
  # state; used by GetMouse, GlobalToLocal/LocalToGlobal and StillDown/WaitMouseUp
  classic_input_coords.c
  carbon_event_appdown.c
  carbon_window_resize_guard.c
  carbon_window_show_shim.c
  tec_shim.c
  py_shim.c
  quicktime_movie_shim.c
  # quicktime_movie_bridge.m: GOLDEN functional QuickTime Movie Toolbox -> AVFoundation
  # video-playback bridge (open/transport/timing + frame pull into the QuickDraw GWorld
  # ARGB buffer). Needs -framework AVFoundation/CoreMedia/CoreVideo. Symbols listed in
  # custom.syms so abigen's legacy pass hands the family to this bridge (no dup).
  quicktime_movie_bridge.m
  keymgr_shim.c
  # libstdc++ i386 COW std::string/std::wstring surface (cow_string_tramp.asm
  # keeps its own MTSHIM + MTSHIM_SRET; wired the same way as the other modules).
  cow_string_shim.c
  # iostream_shim.c: i386 stringstream/ostringstream output + locale/numpunct
  # (trampolines in cow_string_tramp.asm).
  iostream_shim.c
  # wchar_shim.c / wchar_tramp.asm: the <wchar.h> wide-string family (wcslen/
  # wcscpy/wcsto*/... — PRESENT native fns static-interpose renames _wcs* ->
  # ___wcs* but abigen's modern pass didn't yet shim). Pointer-widening
  # marshalling; listed in custom.syms so abigen never dup-emits them.
  wchar_shim.c
  wchar_tramp.asm
  # ae_shim.c / ae_tramp.asm: Apple Event Manager native-shadow bridge. The AE
  # handler dispatch hands i386 a low-4GB SHADOW AEDesc keyed to the native
  # AEDesc; ___AEGetParamPtr resolves the shadow and forwards the ORIGINAL native
  # descriptor (full 64-bit Handle) instead of a 32-bit-truncated rebuild (Civ
  # 'oapp' AEGetParamDesc crash — the callback/down mirror of a4ca917). Exports
  # ___AEGetParamPtr / ___AEInstallEventHandler listed in custom.syms so abigen
  # never dup-emits them.
  ae_shim.c
  ae_tramp.asm
  # crypto_shim.c / crypto_tramp.asm: the OpenSSL 0.9.7 CD-key/product-key
  # RSA-signature verification surface Halo imports (CRYPTO_set_mem_functions,
  # RIPEMD160*, BN_bin2bn, RSA_new/verify/free). These resolve to the LIVE native
  # libcrypto.0.9.7 in the shared cache, so abigen left them as raw weak binds ->
  # native 8-byte ret over-popped the i386 4-byte frame -> fused PC crash (Halo
  # crashed at _CRYPTO_set_mem_functions, the first crypto call, right after the
  # GL renderer loaded). CRYPTO_set_mem_functions is a no-op (its 3 args are the
  # app's own i386 allocator callbacks — must NOT be handed to native OpenSSL);
  # RIPEMD160* forward directly (buffers only); RSA/BN use i386-layout low-4GB
  # shadow structs (Halo pokes rsa->n@0x10 / rsa->e@0x14 by hand) and rebuild a
  # native RSA at verify time. Symbols listed in custom.syms so abigen never
  # dup-emits them.
  crypto_shim.c
  crypto_tramp.asm
  # vdsp_legacy_shim.c: pre-vDSP_ FFT names (create_fftsetup/fft_zip) on
  # modern vDSP (BASS audio). MTSHIMs in maptable_tramp.asm.
  vdsp_legacy_shim.c
  # uckey_shim.c: UCKeyTranslate (incomplete-array param, UniCharCount width).
  uckey_shim.c
  # carbon_window_removed_shim.c: Window Manager calls 64-bit HIToolbox dropped.
  carbon_window_removed_shim.c
  # (halo_shim.c / halo_tramp.asm RETIRED: mach_timebase_info / mach_absolute_time
  # are now covered by the universal import-driven modern pass — libSystem maps to
  # <mach/mach_time.h> in includes_auto.h, so abigen emits ___mach_timebase_info /
  # ___mach_absolute_time directly. Phase 1a, see the import-driven block above.)
  lazy_bind.c
  import_repair.c
  objc_msgSend.asm
  objc_reverse.asm
  cb_tramp.asm
  block_tramp.asm
  dlsym_tramp.asm
  # gli_tramp.asm: the 760 dispatch-slot thunks for cgl_macro_shim.c's shadow
  # context. Each drops the leading `rend` argument (a signature-independent
  # frame edit, because i386 cdecl passes everything in 4-byte stack slots) and
  # tail-jumps to the abigen bridge ___gl<Name>.
  gli_tramp.asm
  init_trampoline.asm
  maptable_tramp.asm
  cow_string_tramp.asm
  cg_display_tramp.asm
  cfstring_range_tramp.asm
  cfnumber_width_tramp.asm
  carbon_datetime_tramp.asm
  carbon_fscatinfo_tramp.asm
  carbon_memory_tramp.asm
  mp_memory_tramp.asm
  carbon_alias_tramp.asm
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
  # nav_shim.m reimplements Navigation Services on NSOpenPanel/NSSavePanel.
  "-framework AppKit"
  "-framework IOKit"
  "-framework OpenGL"
  # quicktime_image.c reimplements the QuickTime GraphicsImporter on these:
  # CGImageSource (decode) + CGBitmapContext/CGImage (render into the GWorld).
  "-framework ImageIO"
  "-framework CoreGraphics"
  # carbon_scrolltext_shim.c + carbon_themetext_shim.c render classic text
  # (scrolling 'TEXT'/'styl' boxes, Appearance theme text) with CoreText.
  "-framework CoreText"
  # quicktime_movie_bridge.m maps the classic Movie Toolbox onto AVFoundation
  # (AVURLAsset/AVPlayer decode) + CoreMedia (CMTime) + CoreVideo (CVPixelBuffer
  # frame pull into the GWorld ARGB buffer).
  "-framework AVFoundation"
  "-framework CoreMedia"
  "-framework CoreVideo"
  # SoundManager: sndmgr_shim.c reimplements the classic Sound Manager on the
  # AudioQueue (playback) + AudioServices (SysBeep alert sound) APIs.
  "-framework AudioToolbox"
  "-framework Accelerate"
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
  # Present-native families bridged only through the import-driven pass: while
  # unlinked, the nulljump probe (a clean process) could not resolve them and
  # blacklisted every bridge, so Portal 2's OpenAL/ForceFeedback binds stayed
  # raw cross-ABI calls.
  "-framework OpenAL"
  "-framework ForceFeedback"
  )
add_dependencies(abiconv abiconv_asm)
if(TARGET abiconv_legacy_asm)
  add_dependencies(abiconv abiconv_legacy_asm)
endif()

install(TARGETS abiconv DESTINATION opt/86x64)

add_executable(info info.cc typeinfo.cc typeconv.cc loc.cc util.cc)
target_link_libraries(info libclang)
add_definitions(${LLVM_DEFINITIONS})


add_executable(syscall_tabgen
  syscall_tabgen.cc
  )

# SoundManager: headless unit test for the pure classic Sound Manager decoder
# (snd_convert.c). Framework-free — validates SoundHeader / ExtSoundHeader /
# 'snd ' resource -> PCM description (rate/channels/bits/sample location/bytes),
# the marshalling that feeds AudioQueueEnqueueBuffer in sndmgr_shim.c. No speaker
# needed. Run: `cmake --build build --target snd_convert_test && build/src/abiconv/snd_convert_test`
add_executable(snd_convert_test snd_convert_test.c snd_convert.c)
add_test(NAME snd_convert_test COMMAND snd_convert_test)

# ── libabiconv.nulljump ─────────────────────────────────────────────────────
# The set of bridges that can only jump to NULL: libabiconv exports ___X, leaves
# _X undefined, and _X no longer resolves on this OS (Apple removed the API).
# static-interpose.sh reads this and refuses to redirect an app's bind into one.
#
# Measured 2026-08-04: 536 of them, 422 shadowing a WORKING implementation the
# app itself ships (347 from its own bundled translated QuickTime.framework, 75
# from its bundled Python). Interposing those replaced working code with a
# guaranteed crash -- Civ IV's QTNewDataReferenceFromFSRef death.
#
# Generated at BUILD time rather than checked in, because "does _X resolve" is a
# runtime dlsym question whose answer changes with the OS version: a list frozen
# on one macOS would silently rot on the next.
#
# ⚠It DEPENDS on the finder and its x86_64 probe, not just on the dylib. As a
# plain POST_BUILD it only regenerated when abiconv happened to relink, so a
# corrected finder left the old list in place and every subsequent translate used
# it. The OUTPUT form below re-runs whenever any of the three inputs changes.
add_custom_command(
  OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/libabiconv.nulljump
  COMMAND ${CMAKE_COMMAND} -E echo "generating libabiconv.nulljump (bridges with no native target)"
  COMMAND python3 ${CMAKE_SOURCE_DIR}/src/86x64/find_null_jump_bridges.py --emit
          $<TARGET_FILE:abiconv> > ${CMAKE_CURRENT_BINARY_DIR}/libabiconv.nulljump
  DEPENDS abiconv
          ${CMAKE_SOURCE_DIR}/src/86x64/find_null_jump_bridges.py
          ${CMAKE_SOURCE_DIR}/src/86x64/nulljump_probe.c
  VERBATIM)
add_custom_target(abiconv_nulljump ALL
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libabiconv.nulljump)
