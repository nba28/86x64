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
  )

add_custom_command(OUTPUT abiconv.syms
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/print_exports_trie.sh ${ABICONV_SYM_SOURCES} > abiconv.syms
  DEPENDS print_exports_trie.sh
  )

add_custom_target(abiconv_syms
  DEPENDS abiconv.syms
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

add_custom_command(OUTPUT abiconv.asm
  COMMAND abigen -o abiconv.asm -s abiconv.syms -i ${CMAKE_CURRENT_SOURCE_DIR}/custom.syms --structfile ${CMAKE_CURRENT_SOURCE_DIR}/ignore.structs --data-shadow-file ${CMAKE_CURRENT_SOURCE_DIR}/data_shadows.syms --isysroot "${ABICONV_SDK_PATH}" -X -x -X objective-c -X -resource-dir -X "${ABICONV_CLANG_RESOURCE_DIR}" ${CMAKE_CURRENT_SOURCE_DIR}/includes.h
  DEPENDS abigen abiconv.syms ${CMAKE_CURRENT_SOURCE_DIR}/custom.syms ${CMAKE_CURRENT_SOURCE_DIR}/ignore.structs ${CMAKE_CURRENT_SOURCE_DIR}/data_shadows.syms ${CMAKE_CURRENT_SOURCE_DIR}/includes.h
  )

add_custom_target(abiconv_asm DEPENDS abiconv.asm)

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

macro(create_custom_conv_template SOURCE)
  foreach(SYMBOL ${ARGN})
    add_library(custom_conv_${SYMBOL} OBJECT ${SOURCE})
    target_compile_options(custom_conv_${SYMBOL} PRIVATE "-DSYMBOL=${SYMBOL}")
  endforeach()
endmacro()

create_custom_conv_template(exec.asm execvp)

add_library(abiconv SHARED
  abiconv.asm
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
  $<TARGET_OBJECTS:custom_conv_execvp>
  printf-conv.cc
  prefs_shim.c
  file_shim.c
  malloc_shim.c
  objc_shim.c
  objc_slide.c
  maptable_shim.m
  sysctl_shim.c
  cxx_shim.c
  proc_shim.c
  cf_callback_shim.c
  cb_bridge.c
  iokit_shim.c
  lazy_bind.c
  objc_msgSend.asm
  objc_reverse.asm
  cb_tramp.asm
  init_trampoline.asm
  maptable_tramp.asm
  dyld_stub_binder.asm
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
  )
add_dependencies(abiconv abiconv_asm)

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

