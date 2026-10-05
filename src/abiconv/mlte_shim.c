// mlte_shim.c — graceful shims for the dead MLTE (TXN) + Carbon HIView text/image API.
//
// MLTE (Multilingual Text Engine: TXNNewObject/TXNSetDataFromFile/...) and the classic
// HITextView/HIImageView were removed from 64-bit/modern macOS. Civ IV uses them for a few
// text fields / image views; its primary text path is its own OpenGL renderer. Object
// creators report failure (a non-zero OSStatus) so callers take their "no text object" path;
// library init reports success so an MLTE-availability check passes; setters/disposers no-op.
//
// NOTE: creators return an error and intentionally do NOT write their out-parameters — the
// out-pointer slot index varies per (long, struct-heavy) prototype and dereferencing the
// wrong i386 arg slot as a pointer would fault. Callers that check the returned OSStatus
// (the documented contract) never read the out value on failure.
//
// MTSHIM convention: rdi -> &i386 args[0]; OSStatus result in eax.

#include <stdint.h>
#include "gap.h"

#define MLTE_NO_ERR   (0)
#define MLTE_PARAM_ERR (-50)

// ---- MLTE library + object lifecycle ----
uint32_t shim_TXNInitTextension(uint32_t *args) { (void)args; return MLTE_NO_ERR; }   // init ok
uint32_t shim_TXNNewObject(uint32_t *args)      { (void)args; return (uint32_t)MLTE_PARAM_ERR; }
void     shim_TXNDeleteObject(uint32_t *args)   { (void)args; }

// ---- MLTE data / geometry ----
uint32_t shim_TXNGetDataEncoded(uint32_t *args)      { (void)args; return (uint32_t)MLTE_PARAM_ERR; }
uint32_t shim_TXNGetHIRect(uint32_t *args)           { (void)args; return (uint32_t)MLTE_PARAM_ERR; }
uint32_t shim_TXNSetDataFromCFURLRef(uint32_t *args) { (void)args; return (uint32_t)MLTE_PARAM_ERR; }
uint32_t shim_TXNSetDataFromFile(uint32_t *args)     { (void)args; return (uint32_t)MLTE_PARAM_ERR; }
void     shim_TXNSetHIRectBounds(uint32_t *args)     { GAP_STUB(args); }
// No TXNObject can exist (HITextViewGetTXNObject is the loud gap), so these
// editing calls on one answer like their siblings: paramErr / nothing to do.
// (Call of Duty 4 Multiplayer's console view.)
void     shim_TXNClear(uint32_t *args)               { (void)args; }
uint32_t shim_TXNSetData(uint32_t *args)             { (void)args; return (uint32_t)MLTE_PARAM_ERR; }
uint32_t shim_TXNSetSelection(uint32_t *args)        { (void)args; return (uint32_t)MLTE_PARAM_ERR; }
void     shim_TXNShowSelection(uint32_t *args)       { (void)args; }
uint32_t shim_TXNSetTypeAttributes(uint32_t *args)   { (void)args; return (uint32_t)MLTE_PARAM_ERR; }

// ---- Carbon HIView text/image ----
uint32_t shim_HITextViewCreate(uint32_t *args)       { (void)args; return (uint32_t)MLTE_PARAM_ERR; }
uint32_t shim_HITextViewGetTXNObject(uint32_t *args) { GAP_STUB(args); return 0; }   // TXNObject NULL
uint32_t shim_HIImageViewSetImage(uint32_t *args)    { GAP_STUB(args); return MLTE_NO_ERR; }
