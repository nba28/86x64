cmake_minimum_required(VERSION 3.10)

# Force x86_64 build. The tool's outputs are x86_64 Mach-O binaries
# (abiconv.dylib, wrapper, generated runtime) and our available libclang
# is x86_64-only, so building host tools as x86_64 keeps everything
# consistent. On Apple Silicon hosts the tools run under Rosetta.
if(NOT CMAKE_OSX_ARCHITECTURES)
  set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Build architectures for macOS" FORCE)
endif()

project(86x64 VERSION 0.0)

enable_testing()

enable_language(ASM_NASM)
# The generated abiconv.asm is ~600K lines; NASM 3.01's multi-pass jump-size
# optimizer fails to reach a fixpoint on it (a 2-state oscillation of a few
# labels, e.g. ___gluUnProject4 "label changed during code generation") and, by
# default, promotes that to a hard error (-w+error=label-redef-late). The final
# pass is internally self-consistent (every jump is encodable and every symbol
# offset matches its code), so the emitted object is correct; downgrade the
# non-convergence back to a warning so the build completes.
set(CMAKE_ASM_NASM_FLAGS "${CMAKE_ASM_NASM_FLAGS} -w-error=label-redef-late")
# NASM's macho64 writer cannot emit LC_BUILD_VERSION, so ld warns "no platform
# load command found in '<obj>', assuming: macOS" once per asm object at the final
# link. Re-link each object with `ld -r -platform_version` to stamp the platform
# (-w: that relink step would otherwise print the same warning about its input).
set(CMAKE_ASM_NASM_COMPILE_OBJECT
  "<CMAKE_ASM_NASM_COMPILER> <DEFINES> <INCLUDES> -f macho64 <FLAGS> -o <OBJECT> <SOURCE>"
  "ld -r -w -arch x86_64 -platform_version macos 11.0 11.0 <OBJECT> -o <OBJECT>.stamped"
  "mv <OBJECT>.stamped <OBJECT>")
set(CMAKE_ASM_NASM_LINK_EXECUTABLE "ld <FLAGS> <CMAKE_ASM_NASM_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>")
set(CMAKE_ASM_NASM_CREATE_SHARED_LIBRARY "ld -dylib <CMAKE_ASM_NASM_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# find_package(xed)

include_directories("${PROJECT_SOURCE_DIR}/include")

include(cmake/86x64.cmake)
include(cmake/paths.cmake)   # M64_WORKSPACE and the local paths derived from it

add_subdirectory(extern)
add_subdirectory(src)
add_subdirectory(include)
add_subdirectory(scripts)

set(86x64 name)
