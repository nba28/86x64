/*
 * libobjc_compat.c — ObjC1-runtime compatibility shim for CLASSIC (pre-ObjC2,
 * zero-LC_DYLD_INFO) translated Mach-O binaries.  ONE job (Unix philosophy):
 * re-supply the handful of libobjc symbols that classic two-level binaries
 * import but that modern /usr/lib/libobjc.A.dylib no longer exports.  NOT
 * folded into libabiconv (which already holds the impls) — libabiconv is
 * loaded per-process via the wrapper/insert and does NOT define the libobjc
 * LC_ID; classic binaries can't be static-interposed, so the only mechanism
 * that redirects their two-level binds is an install_name swap onto a drop-in
 * dylib that carries libobjc's identity.  That drop-in is this file's product.
 *
 * Why: Numbers/Pages (iWork '09) ship classic-Mach-O frameworks (the main exec
 * + ~14 SF frameworks) that two-level-bind removed ObjC1 runtime symbols:
 *     _objc_exception_try_enter / _try_exit / _extract / _match  (exceptions)
 *     _class_nextMethodList                                      (method walk)
 *     __objc_setNilReceiver                                      (nil-recv hook)
 *     __dealloc                                                  (default-IMP DATA)
 * Modern libobjc dropped all of these -> hard dyld "Symbol not found:
 * __dealloc, Expected in libobjc.A.dylib" at launch.
 *
 * Fix: this dylib LC_REEXPORT_DYLIBs the real /usr/lib/libobjc.A.dylib (so all
 * the *surviving* libobjc symbols still resolve through it transparently) and
 * adds the removed names back, forwarding to libabiconv's i386-discipline
 * implementations (___objc_exception_*, ___class_nextMethodList,
 * ____objc_setNilReceiver, ____dealloc).  The callers are TRANSLATED-i386
 * (i386-cdecl), so libabiconv's i386-discipline impls are ABI-correct as-is.
 * The forwarders are bare jmp thunks (register/ABI-transparent — no signature
 * assumptions), so whatever the classic caller pushes flows straight through.
 *
 * Deploy: for each classic binary that imports these,
 *     install_name_tool -change /usr/lib/libobjc.A.dylib \
 *         @rpath/libobjc_compat.dylib <binary>
 * and drop this dylib next to them (Contents/MacOS) with a matching @rpath.
 * Universal: triggers on the structural property (a classic Mach-O binding a
 * removed ObjC1 libobjc symbol), not on any specific app.
 *
 * Built x86_64 (runs under Rosetta).
 */
#include <stdint.h>

/*
 * The six removed libobjc *functions*, re-exported here as bare jmp thunks that
 * tail-jump into libabiconv's impls.  A bare jmp preserves every argument
 * register and the stack frame untouched, so the classic i386-cdecl caller's
 * arguments pass straight through regardless of each function's exact prototype.
 *
 * Mach-O note: top-level asm symbols already carry the leading underscore, so
 * these names read one underscore "deeper" than the C identifiers would.  The
 * left-hand (exported) names mirror libobjc's removed exports; the jmp targets
 * are libabiconv's exports (one extra leading underscore = its i386-discipline
 * impl namespace).
 */
__asm__(
"	.text\n"
"	.globl _objc_exception_try_enter\n"
"_objc_exception_try_enter:\n"
"	jmp ___objc_exception_try_enter\n"
"	.globl _objc_exception_try_exit\n"
"_objc_exception_try_exit:\n"
"	jmp ___objc_exception_try_exit\n"
"	.globl _objc_exception_extract\n"
"_objc_exception_extract:\n"
"	jmp ___objc_exception_extract\n"
"	.globl _objc_exception_match\n"
"_objc_exception_match:\n"
"	jmp ___objc_exception_match\n"
"	.globl _class_nextMethodList\n"
"_class_nextMethodList:\n"
"	jmp ___class_nextMethodList\n"
"	.globl __objc_setNilReceiver\n"
"__objc_setNilReceiver:\n"
"	jmp ____objc_setNilReceiver\n"
);

/*
 * The seventh removed symbol, __dealloc, is DATA, not a function: the ObjC1
 * runtime exported the default-dealloc IMP as a global the classic binary binds
 * directly (a 4-byte i386-width pointer slot, since the importer is translated
 * i386).  libabiconv holds the i386-discipline value in ____dealloc; we mirror
 * its low 32 bits into our own __dealloc export at load (a DATA symbol can't be
 * a reexport/jmp — it must physically exist in this image).
 *
 * C-identifier -> Mach-O symbol (assembler adds one underscore):
 *     ___dealloc  ->  ____dealloc   (libabiconv impl we read)
 *     _dealloc    ->  __dealloc     (libobjc export we supply)
 */
extern uint32_t ___dealloc;   /* libabiconv's i386-discipline default-IMP value */
uint32_t _dealloc = 0;        /* the libobjc __dealloc DATA export we re-supply  */

__attribute__((constructor))
static void libobjc_compat_init(void)
{
	_dealloc = ___dealloc;
}
