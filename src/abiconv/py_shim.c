// py_shim.c — single-purpose shim for the i386 Python C-API entry _Py_InitModule4.
//
// Civ IV embeds Python 2.3 and registers its C extension modules via Py_InitModule4. On
// 64-bit, CPython renames this symbol to Py_InitModule4_64 (the bundled Python 2.6 exports
// only the _64 form), so the translated i386 reference to _Py_InitModule4 fails to bind at
// dyld LOAD. This shim provides the missing symbol so the image loads.
//
// ⚠NOT a working forward yet (deliberately a loud stub, not a silent/half-working one):
// a correct Py_InitModule4 -> Py_InitModule4_64 forward requires BOTH
//   (1) repacking the caller's PyMethodDef[] from i386 layout (16-byte entries) to x86_64
//       layout (32-byte entries), and
//   (2) wrapping every ml_meth in an i386->x86_64 callback trampoline (cb_bridge.c style),
//       because CPython (native x86_64) will later CALL those methods, which are translated
//       i386 functions expecting the i386 cdecl ABI.
// Shipping a partial forward would half-register modules and then crash unpredictably when
// Python invokes a method, so until the bridge exists we fail the registration cleanly
// (return NULL) and log once. Python init happens long after the LOAD/static-init wall this
// shim is unblocking, so this is sufficient to advance Civ IV now.
//
// MTSHIM convention: rdi -> &i386 args[0]; PyObject* result (32-bit) in eax.

#include <stdint.h>
#include <stdio.h>

uint32_t shim_Py_InitModule4(uint32_t *args) {
    const char *name = (const char *)(uintptr_t)args[0];
    static int warned = 0;
    if (!warned) {
        warned = 1;
        fprintf(stderr,
            "[py_shim] Py_InitModule4(\"%s\", ...) is a stub: i386->x86_64 PyMethodDef "
            "repack + method callback bridge not yet implemented; module not registered.\n",
            name ? name : "?");
    }
    return 0;   // NULL PyObject* -> registration fails (clean), Python sets an error
}
