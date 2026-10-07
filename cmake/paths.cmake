# Local paths of the 86x64 workspace (CMake side of src/86x64/paths.sh).
#
# Each path is a cache variable. Its first value comes from -D<NAME>=..., else
# from the environment variable of the same name, else from the default, which
# derives from M64_WORKSPACE. Because they are cached, change them later with
# -D<NAME>=... (or delete build/CMakeCache.txt) rather than by changing the
# environment. Keep names and defaults in sync with src/86x64/paths.sh.

function(m64_path name default doc)
  if(DEFINED ENV{${name}} AND NOT "$ENV{${name}}" STREQUAL "")
    set(value "$ENV{${name}}")
  else()
    set(value "${default}")
  endif()
  set(${name} "${value}" CACHE PATH "${doc}")
endfunction()

m64_path(M64_WORKSPACE  "$ENV{HOME}/projects"                        "Root of the 86x64 workspace layout")
m64_path(M64_LIBRARY    "${M64_WORKSPACE}/Library"                   "SDKs, toolchains and reusable frameworks")
m64_path(M64_SDK106     "${M64_LIBRARY}/SDKs/MacOSX10.6.sdk"         "macOS 10.6 SDK")
m64_path(M64_I386_LD    "${M64_LIBRARY}/Toolchains/sl-ld64/ld-i386"  "Snow Leopard ld64 wrapper for i386 links")
m64_path(M64_FRAMEWORKS "${M64_LIBRARY}/Frameworks"                  "Reusable translated frameworks")
m64_path(M64_APPS32     "${M64_WORKSPACE}/translations/Apps32"       "Pristine i386 originals (read-only)")
m64_path(M64_APPS64     "${M64_WORKSPACE}/translations/Apps64"       "Translated output bundles")
