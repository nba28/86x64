// carbon_dialog_shim.c — graceful classic Dialog Manager for the abigen-dangling
// DialogRef entry points (Halo CE + iPhoto). The classic Dialog Manager (DialogRef
// built from 'DLOG'/'DITL' resources, ModalDialog run loops, 'ALRT' StopAlert) was
// removed from 64-bit macOS entirely, and its resource-fork inputs no longer exist,
// so there is no live native to forward to and no faithful reimplementation without
// the removed resources. Modern Carbon apps get their real dialogs from IBCarbon
// nibs (now materialized by carbon_nib_shim.c) or from the surviving Standard Alert
// (CreateStandardAlert/RunStandardAlert, already shimmed). These shims therefore
// return each call's signature-correct "no classic dialog present" value and zero
// any out-parameter, so a translated app that still probes the old Dialog Manager
// gets a clean, non-crashing negative answer instead of a dangling-symbol abort.
//
// MTSHIM convention: rdi -> &i386 args[0] (4-byte cdecl slots); uint32_t in eax.
// Wired ___<Name> -> _shim_<Name>; MTSHIM presence removes them from the abigen
// legacy pass, so no duplicate-symbol clash (validates on a RESYNC).

#include <stdint.h>

#define PTR(i) ((void *)(uintptr_t)a[(i)])

// DialogRef GetNewDialog(SInt16, void*, WindowRef) -> NULL (no 'DLOG' resource).
uint32_t shim_GetNewDialog(uint32_t *a) { (void)a; return 0; }

// void ModalDialog(ModalFilterUPP, DialogItemIndex *itemHit) -> report the default
// (OK) item so any modal loop that reached here exits cleanly instead of spinning.
uint32_t shim_ModalDialog(uint32_t *a) {
    int16_t *itemHit = (int16_t *)PTR(1);
    if (itemHit) *itemHit = 1;   // kStdOkItemIndex
    return 0;
}

// SInt16 StopAlert/CautionAlert/NoteAlert(SInt16 alertID, ModalFilterUPP) -> 1 (OK):
// no 'ALRT'/'DITL' resource to render, so answer as if the user pressed the default.
uint32_t shim_StopAlert(uint32_t *a)    { (void)a; return 1; }

// void DisposeDialog(DialogRef) -> nothing (we never hand out a real DialogRef).
uint32_t shim_DisposeDialog(uint32_t *a) { (void)a; return 0; }

// void GetDialogItemText(Handle item, Str255 text) -> empty pascal string.
uint32_t shim_GetDialogItemText(uint32_t *a) {
    uint8_t *text = (uint8_t *)PTR(1);
    if (text) text[0] = 0;
    return 0;
}
// void SetDialogItemText(Handle, ConstStr255Param) -> no-op.
uint32_t shim_SetDialogItemText(uint32_t *a) { (void)a; return 0; }

// TEHandle GetDialogTextEditHandle(DialogRef) -> NULL (no classic TextEdit).
uint32_t shim_GetDialogTextEditHandle(uint32_t *a) { (void)a; return 0; }

// void SetDialogDefaultItem(DialogRef, SInt16) -> noErr; getters -> "none".
uint32_t shim_SetDialogDefaultItem(uint32_t *a) { (void)a; return 0; }
uint32_t shim_GetDialogDefaultItem(uint32_t *a) { (void)a; return 0; }
uint32_t shim_GetDialogCancelItem(uint32_t *a)  { (void)a; return 0; }

// void SetPortDialogPort(DialogRef) -> no-op (no classic GrafPort coupling).
uint32_t shim_SetPortDialogPort(uint32_t *a) { (void)a; return 0; }

// OSStatus AppendDialogItemList / AutoSizeDialog / MoveDialogItem / SizeDialogItem
// (DialogRef, ...) -> noErr (nothing to lay out on a non-existent DialogRef).
uint32_t shim_AppendDialogItemList(uint32_t *a) { (void)a; return 0; }
uint32_t shim_AutoSizeDialog(uint32_t *a)       { (void)a; return 0; }
uint32_t shim_MoveDialogItem(uint32_t *a)       { (void)a; return 0; }
uint32_t shim_SizeDialogItem(uint32_t *a)       { (void)a; return 0; }

// OSStatus GetModalDialogEventMask(EventMask *outMask) -> everyEvent, noErr.
uint32_t shim_GetModalDialogEventMask(uint32_t *a) {
    int16_t *outMask = (int16_t *)PTR(0);
    if (outMask) *outMask = (int16_t)0xFFFF;   // everyEvent
    return 0;
}
