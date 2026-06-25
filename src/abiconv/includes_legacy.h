/* includes_legacy.h — umbrella headers for the LEGACY abigen pass.
 *
 * This file is parsed by a SEPARATE abigen invocation whose -isysroot is the
 * extracted MacOSX10.6 SDK (~/projects/Library/SDKs/MacOSX10.6.sdk), parsed as
 * -arch i386, isolated from the modern pass (includes.h). Its job is to give
 * abigen the PROTOTYPES for framework C functions whose HEADERS modern macOS
 * deleted, so it can emit real i386->x86_64 ABI-marshalling shims for them.
 * See the shimdb legacy expansion.
 *
 * SCOPE: the legacy frameworks our actual targets (Civ IV, iPhoto, iWeb, Pages,
 * games) import — Carbon, ApplicationServices, CoreServices, AGL, QuickTime,
 * ICADevices, OpenAL, Python. NOT a blanket "every framework in the SDK": a
 * blanket pass emits ~9,700 shims / 24MB of asm (Accelerate vDSP/vImage,
 * Security/CSSM, the CoreAudio stack, IOBluetooth, ...) that no target calls and
 * that take >10 min to assemble. If a target turns out to need another
 * framework, add its umbrella here (and re-check the consider set) a few at a
 * time. The consider set (abiconv_legacy.syms) still gates emission, so a header
 * whose symbols are all already shimmed contributes nothing.
 *
 * Parsed with -x objective-c (some Carbon/QuickTime headers reference NSString);
 * headers that fail to parse contribute no cursors but do not abort the run.
 */

/* AGL — OpenGL context API for Carbon. Small, clean (scalars, pointers, enums). */
#include <AGL/agl.h>

/* The Carbon umbrella — the bulk of the legacy C API surface. Pulls in
 * CoreServices (CarbonCore: File/Memory/Resource/Thread Manager, OSUtils,
 * Multiprocessing; OSServices), ApplicationServices (ATS, ColorSync, the old
 * QuickDraw CoreGraphics, LaunchServices, PrintCore, QD, SpeechSynthesis,
 * HIServices) and HIToolbox (Window/Menu/Control/Dialog/Event/Appearance
 * Manager). The 32-bit-only classic managers (visible because we parse i386)
 * are the DELETED class (shimgen-backed); the survivors call the live native. */
#include <Carbon/Carbon.h>

/* Listed explicitly so their decls are unambiguous even though Carbon
 * re-includes them (a function emitted while processing Carbon.h is erased from
 * the consider set, so it is not re-emitted here). */
#include <ApplicationServices/ApplicationServices.h>
#include <CoreServices/CoreServices.h>

/* QuickTime — the classic Movie Toolbox / GraphicsImporter / Media Handler C
 * API. Removed from modern macOS, so the bulk is the DELETED class; with the
 * x86_64 QuickTime vendored into the target bundle, many `call _sym` resolve to
 * the real implementation, the rest to a graceful shimgen stub. Big win for any
 * QuickTime-era media app or game (Civ IV imports the Movie Toolbox). */
#include <QuickTime/QuickTime.h>

/* ICADevices / Image Capture — the camera/scanner C API (ICAGetDeviceList,
 * ICAObjectSendMessage, ...). Header deleted on modern macOS. */
#include <ICADevices/ICADevices.h>

/* OpenAL — positional audio for games (alGenSources/alSourcePlay/alBufferData,
 * alcOpenDevice/...). Small; survives on modern macOS (deprecated). */
#include <OpenAL/al.h>
#include <OpenAL/alc.h>

/* Python 2.x C-API (Py_Initialize, PyImport_ImportModule, PyObject_CallObject,
 * Py_BuildValue, PyRun_*, ...). Carbon-era games and apps embed a Python
 * interpreter and drive game/script logic through it (Civilization IV imports
 * ~109 Py* symbols). The framework is gone from modern macOS; a vendored x86_64
 * Python (e.g. iLife's Python 2.6) is bundled into the target and backs the
 * `call _Py*`. Python objects are OPAQUE HANDLES — listed in ignore.structs so
 * they pass through as widened pointers instead of being deep-copied (which is
 * both wrong, breaking pointer identity, and explosive: PyObject -> PyTypeObject
 * recursion unrolls to 80k+ asm lines). PyArg_ParseTuple / Py_BuildValue are
 * variadic (abigen skips variadics). */
#include <Python/Python.h>
