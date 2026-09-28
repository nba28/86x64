/*
 * cb_bridge.h — the i386-callback bridge's public surface (cb_bridge.c).
 *
 * x64_cb_wrap(fn32, sig) binds an i386 function to a native trampoline that
 * marshals its native arguments into i386 cdecl words per `sig`. The layout and
 * codes must match typeconv.cc (cb_arg_code / cb_ret_code / cb_sig_emit).
 */
#ifndef ABICONV_CB_BRIDGE_H
#define ABICONV_CB_BRIDGE_H

#include <stdint.h>

#define X64_CB_MAX_ARGS 16
typedef struct {
   uint32_t nargs;
   uint32_t ret_kind;
   uint8_t  arg_kinds[X64_CB_MAX_ARGS];
   uint32_t arg_sizes[X64_CB_MAX_ARGS];  /* CBA_PTR_REC pointee size | COPYBACK */
} x64_cb_sig;

enum { CBA_I32 = 0, CBA_I64 = 1, CBA_PTR = 2, CBA_OBJ = 3,
       CBA_F32 = 4, CBA_F64 = 5, CBA_PTR_REC = 6 };
#define CBA_SIZE_COPYBACK 0x80000000u
enum { CBR_VOID = 0, CBR_I32 = 1, CBR_PTR = 2, CBR_OBJ = 3, CBR_I32SX = 4,
       CBR_I64 = 5 };

uint64_t x64_cb_wrap(uint32_t fn32, const x64_cb_sig *sig);
uint32_t x64_objc_wrap(uint64_t real);     /* objc_shim.c */
uint64_t x64_objc_unwrap(uint32_t h);      /* objc_shim.c */

#endif
