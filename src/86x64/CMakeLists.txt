add_library(wrapper STATIC wrapper.asm wrapper_setup.c)
add_library(interpose SHARED interpose.c)
# The CFPreferences NULL-appID guard references CoreFoundation symbols only to
# carry the __interpose replacee reference + dlsym the real impls; resolve them
# at load (dynamic_lookup) so libinterpose keeps its no-CoreFoundation link and
# early load order.
target_link_options(interpose PRIVATE "-Wl,-undefined,dynamic_lookup")

# libobjc_compat: a single-purpose drop-in dylib that re-supplies the ObjC1
# runtime symbols CLASSIC (pre-ObjC2, zero-LC_DYLD_INFO) translated binaries
# import but modern /usr/lib/libobjc.A.dylib no longer exports
# (objc_exception_try_enter/_try_exit/_extract/_match, class_nextMethodList,
# _objc_setNilReceiver, and the _dealloc DATA IMP).  It LC_REEXPORTs the real
# libobjc (so surviving symbols still resolve through it) and adds the removed
# names back as bare-jmp thunks forwarding to libabiconv's i386-discipline
# impls (the callers are translated-i386, so those impls are ABI-correct).
# Deploy: install_name_tool -change /usr/lib/libobjc.A.dylib
# @rpath/libobjc_compat.dylib on each classic binary.  Built x86_64 (runs under
# Rosetta).  See libobjc_compat.c.  Output: libobjc_compat.dylib.
add_library(objc_compat SHARED libobjc_compat.c)
set_target_properties(objc_compat PROPERTIES
  OUTPUT_NAME objc_compat
  OSX_ARCHITECTURES x86_64)
add_dependencies(objc_compat abiconv)
# Link the libabiconv dylib FILE (not the target) so the impl symbols resolve
# two-level against @rpath/libabiconv.dylib WITHOUT inheriting abiconv's own
# framework deps -- this stays a minimal single-purpose shim (libabiconv +
# reexported libobjc + libSystem).  Reexport the real libobjc
# (LC_REEXPORT_DYLIB /usr/lib/libobjc.A.dylib) so surviving symbols pass through.
target_link_libraries(objc_compat
  "$<TARGET_FILE:abiconv>"
  "-Wl,-reexport-lobjc")

# webnoop: a single-purpose DYLD-insert shim that suppresses legacy-WebKit
# (WebView) instantiation in the translated process (the JSC stack-bounds trap
# under our low-4GB stack). Built x86_64 (runs under Rosetta). See webnoop.m.
add_library(webnoop SHARED webnoop.m)
set_target_properties(webnoop PROPERTIES
  OSX_ARCHITECTURES x86_64
  COMPILE_OPTIONS "-fno-objc-arc")
target_link_libraries(webnoop "-framework Foundation" "-framework AppKit")

# acctnoop: a single-purpose DYLD-insert shim that neutralizes iPhoto's DEAD
# MobileMe / unknown-account startup discovery (IPAccountConfigurationManager
# findUnknownAccounts / findUnknownSystemMobileMeAccount), which crashes ~50% of
# launches under translation. Built x86_64 (runs under Rosetta). See acctnoop.m.
add_library(acctnoop SHARED acctnoop.m)
set_target_properties(acctnoop PROPERTIES
  OSX_ARCHITECTURES x86_64
  COMPILE_OPTIONS "-fno-objc-arc")
target_link_libraries(acctnoop "-framework Foundation")

# jscstackpatch: a single-purpose shim that lets a legacy WebKit1 WebView spin
# up under translation by widening JSC's per-thread cached native-stack bounds
# to also cover the low-4GB translation stack (so JSC::sanitizeStackForVM passes
# instead of trapping). env-gated JSCSTACKPATCH=1, OFF by default. UNTESTED /
# not bundled yet — preserved here for the WebKit-B work. See jscstackpatch.m.
add_library(jscstackpatch SHARED jscstackpatch.m)
set_target_properties(jscstackpatch PROPERTIES
  OSX_ARCHITECTURES x86_64
  COMPILE_OPTIONS "-fno-objc-arc")
target_link_libraries(jscstackpatch "-framework Foundation")

install(TARGETS wrapper interpose objc_compat webnoop acctnoop jscstackpatch
  DESTINATION opt/86x64
  )

install(PROGRAMS 86x64 86x64.sh static-interpose.sh abiconv.sh
  DESTINATION opt/86x64
  )

install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"${CMAKE_INSTALL_PREFIX}/opt/86x64/86x64.sh\" \"${CMAKE_INSTALL_PREFIX}/bin/86x64\")")
