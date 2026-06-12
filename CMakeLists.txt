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
set(CMAKE_ASM_NASM_LINK_EXECUTABLE "ld <FLAGS> <CMAKE_ASM_NASM_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>")
set(CMAKE_ASM_NASM_CREATE_SHARED_LIBRARY "ld -dylib <CMAKE_ASM_NASM_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# find_package(xed)

include_directories("${PROJECT_SOURCE_DIR}/include")

include(cmake/86x64.cmake)

add_subdirectory(extern)
add_subdirectory(src)
add_subdirectory(include)
add_subdirectory(scripts)

# Tests compile C source with -m32 (i386), which Apple removed from the
# SDK with macOS 10.15. Disabled by default; re-enable with -DBUILD_TESTS=ON
# only if you have an SDK that still supports i386.
option(BUILD_TESTS "Build the tests/ subtree (requires -m32 support)" OFF)
if(BUILD_TESTS)
  add_subdirectory(tests)
endif()

set(86x64 name)
