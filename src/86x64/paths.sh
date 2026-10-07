# Local paths of the 86x64 workspace (shell side). Source this file:
#
#     . "/path/to/86x64/src/86x64/paths.sh"
#
# Every variable can be set in the environment beforehand. Anything left unset
# is derived from M64_WORKSPACE, so moving the whole layout takes one variable.
# The same names and defaults are used by m64_paths.py (Python),
# cmake/paths.cmake (CMake) and tests-i386/Makefile. `m64 paths` prints the
# resolved values and whether each one exists.

: "${M64_WORKSPACE:=$HOME/projects}"                         # root of the layout
: "${M64_LIBRARY:=$M64_WORKSPACE/Library}"                   # SDKs, toolchains, frameworks
: "${M64_SDK106:=$M64_LIBRARY/SDKs/MacOSX10.6.sdk}"          # macOS 10.6 SDK
: "${M64_I386_LD:=$M64_LIBRARY/Toolchains/sl-ld64/ld-i386}"  # Snow Leopard ld64 wrapper
: "${M64_FRAMEWORKS:=$M64_LIBRARY/Frameworks}"               # reusable translated frameworks
: "${M64_APPS32:=$M64_WORKSPACE/translations/Apps32}"        # pristine i386 originals (read-only)
: "${M64_APPS64:=$M64_WORKSPACE/translations/Apps64}"        # translated output

export M64_WORKSPACE M64_LIBRARY M64_SDK106 M64_I386_LD M64_FRAMEWORKS M64_APPS32 M64_APPS64
