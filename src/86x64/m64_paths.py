"""Local paths of the 86x64 workspace (Python side of paths.sh).

Every path can be set in the environment. Anything left unset is derived from
M64_WORKSPACE, so moving the whole layout takes one variable. Keep the names
and defaults in sync with paths.sh, cmake/paths.cmake and tests-i386/Makefile.
"""
import os
from pathlib import Path


def _env(name, default):
    value = os.environ.get(name)
    return Path(os.path.expanduser(value)) if value else default


WORKSPACE  = _env("M64_WORKSPACE",  Path.home() / "projects")
LIBRARY    = _env("M64_LIBRARY",    WORKSPACE / "Library")
SDK106     = _env("M64_SDK106",     LIBRARY / "SDKs" / "MacOSX10.6.sdk")
I386_LD    = _env("M64_I386_LD",    LIBRARY / "Toolchains" / "sl-ld64" / "ld-i386")
FRAMEWORKS = _env("M64_FRAMEWORKS", LIBRARY / "Frameworks")
APPS32     = _env("M64_APPS32",     WORKSPACE / "translations" / "Apps32")
APPS64     = _env("M64_APPS64",     WORKSPACE / "translations" / "Apps64")

# (variable, resolved path, what it is) — what `m64 paths` prints.
ALL = [
    ("M64_WORKSPACE",  WORKSPACE,  "root of the layout; the defaults below derive from it"),
    ("M64_LIBRARY",    LIBRARY,    "SDKs, toolchains and reusable frameworks"),
    ("M64_SDK106",     SDK106,     "macOS 10.6 SDK (test sysroot headers, abigen legacy pass)"),
    ("M64_I386_LD",    I386_LD,    "Snow Leopard ld64 wrapper that links the i386 test fixtures"),
    ("M64_FRAMEWORKS", FRAMEWORKS, "reusable translated frameworks (golden QuickTime, ...)"),
    ("M64_APPS32",     APPS32,     "pristine i386 originals, read-only (seed abigen's bridge set)"),
    ("M64_APPS64",     APPS64,     "translated output bundles"),
]
