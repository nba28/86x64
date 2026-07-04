// qt_hitoolbox_shim.c - i386-frame no-op shims for the REMOVED HIToolbox surface
// that translated QuickTime.framework links (Civ IV / Halo s32).
//
// A macho-tool-translated QuickTime (i386 -> x86_64) imports 249 HIToolbox
// symbols; 135 are REMOVED from modern 64-bit HIToolbox (the classic Dialog /
// Control / List / TextEdit / Appearance-Theme / Scrap / Menu Manager UI that
// only ever shipped 32-bit). libabiconv already marshals 20 of them (shared
// with Civ IV's main binary); these are the remaining 115. QuickTime runs its
// real work through the Movie Toolbox + graphics importers, so this classic UI
// chrome is never exercised on the boot / decode path - but the FIRST such call
// (INIT_QuickTimeLibInternal -> _InitHLTB during library init) crashed: the
// symbol was weak-bound NULL (removed, no native to resolve) -> rip=0, and even
// a native stub would OVER-POP the translated i386 4-byte return frame with its
// 8-byte `ret` (the s28 ABI-fusion family). So each removed symbol needs an
// i386-FRAME-aware shim, exactly like carbon_ui_shim.c / the 1122 ___X Carbon
// shims Civ IV's main binary already uses (it binds ZERO native ShimAuto).
//
// These are REMOVED APIs: no native function to forward to, so each shim returns
// the benign "nothing to do / not available" value of its 10.6-SDK signature
// (noErr / NULL / false / no-part) - the same contract as carbon_ui_shim.c. A
// getter's out-parameter is left as the caller staged it (callers allocate their
// own out storage). If a specific symbol later proves to be CALLED on a live path
// and its result matters, promote it to a real impl here or in carbon_ui_shim.c
// (as GetThemeTextDimensions is there).
//
// MTSHIM convention (maptable_tramp.asm): rdi -> &i386 args[0] (4-byte cdecl
// slots), uint32_t result in eax. Wired ___<Name> -> _shim_<Name>. These classic
// APIs are #if !__LP64__-gated in the SDK headers, so abigen (x86_64 parse) never
// emits them -> no custom.syms entry and no symbol collision.
//
// UNIVERSAL: any translated i386 program linking removed HIToolbox UI benefits
// (Civ IV + Halo both ship this same QuickTime and hit the identical wall).

#include <stdint.h>

// Every removed HIToolbox UI entry returns the "benign unavailable" value for its
// signature: OSStatus noErr(0) / Boolean false(0) / Handle|Ref NULL(0) / control
// part 0. A single uniform stub expresses all of these as 0.


// ---- 0 Toolbox init (1) ----
uint32_t shim_InitHLTB(uint32_t *args) { (void)args; return 0; }

// ---- Appearance / Theme (15) ----
uint32_t shim_DrawThemeButton(uint32_t *args) { (void)args; return 0; }
uint32_t shim_DrawThemeEditTextFrame(uint32_t *args) { (void)args; return 0; }
uint32_t shim_DrawThemeFocusRect(uint32_t *args) { (void)args; return 0; }
uint32_t shim_DrawThemeListBoxFrame(uint32_t *args) { (void)args; return 0; }
uint32_t shim_DrawThemePrimaryGroup(uint32_t *args) { (void)args; return 0; }
uint32_t shim_DrawThemeSeparator(uint32_t *args) { (void)args; return 0; }
uint32_t shim_DrawThemeText(uint32_t *args) { (void)args; return 0; }
uint32_t shim_DrawThemeTickMark(uint32_t *args) { (void)args; return 0; }
uint32_t shim_DrawThemeTrack(uint32_t *args) { (void)args; return 0; }
uint32_t shim_GetThemeDrawingState(uint32_t *args) { (void)args; return 0; }
uint32_t shim_GetThemeFont(uint32_t *args) { (void)args; return 0; }
uint32_t shim_GetThemeTrackThumbRgn(uint32_t *args) { (void)args; return 0; }
uint32_t shim_NormalizeThemeDrawingState(uint32_t *args) { (void)args; return 0; }
uint32_t shim_SetThemeBackground(uint32_t *args) { (void)args; return 0; }
uint32_t shim_SetThemeDrawingState(uint32_t *args) { (void)args; return 0; }

// ---- Control Manager (20) ----
uint32_t shim_AutoEmbedControl(uint32_t *args) { (void)args; return 0; }
uint32_t shim_CreateCustomControl(uint32_t *args) { (void)args; return 0; }
uint32_t shim_CreateUserPaneControl(uint32_t *args) { (void)args; return 0; }
uint32_t shim_FindControl(uint32_t *args) { (void)args; return 0; }
uint32_t shim_GetControlAction(uint32_t *args) { (void)args; return 0; }
uint32_t shim_GetControlDataHandle(uint32_t *args) { (void)args; return 0; }
uint32_t shim_GetControlReference(uint32_t *args) { (void)args; return 0; }
uint32_t shim_GetControlTitle(uint32_t *args) { (void)args; return 0; }
uint32_t shim_IdleControls(uint32_t *args) { (void)args; return 0; }
uint32_t shim_NewControl(uint32_t *args) { (void)args; return 0; }
uint32_t shim_RegisterSystemControlDefinition(uint32_t *args) { (void)args; return 0; }
uint32_t shim_SetControlBounds(uint32_t *args) { (void)args; return 0; }
uint32_t shim_SetControlDataHandle(uint32_t *args) { (void)args; return 0; }
uint32_t shim_SetControlPopupMenuHandle(uint32_t *args) { (void)args; return 0; }
uint32_t shim_SetControlReference(uint32_t *args) { (void)args; return 0; }
uint32_t shim_SetControlSupervisor(uint32_t *args) { (void)args; return 0; }
uint32_t shim_SetControlTitle(uint32_t *args) { (void)args; return 0; }
uint32_t shim_SetUpControlBackground(uint32_t *args) { (void)args; return 0; }
uint32_t shim_TestControl(uint32_t *args) { (void)args; return 0; }
uint32_t shim_TrackControl(uint32_t *args) { (void)args; return 0; }

// ---- Dialog Manager (21) ----
uint32_t shim_Alert(uint32_t *args) { (void)args; return 0; }
uint32_t shim_DialogCopy(uint32_t *args) { (void)args; return 0; }
uint32_t shim_DialogCut(uint32_t *args) { (void)args; return 0; }
uint32_t shim_DialogDelete(uint32_t *args) { (void)args; return 0; }
uint32_t shim_DialogPaste(uint32_t *args) { (void)args; return 0; }
uint32_t shim_DrawDialog(uint32_t *args) { (void)args; return 0; }
uint32_t shim_FindDialogItem(uint32_t *args) { (void)args; return 0; }
uint32_t shim_GetDialogKeyboardFocusItem(uint32_t *args) { (void)args; return 0; }
uint32_t shim_GetDialogPort(uint32_t *args) { (void)args; return 0; }
uint32_t shim_HideDialogItem(uint32_t *args) { (void)args; return 0; }
uint32_t shim_InsertDialogItem(uint32_t *args) { (void)args; return 0; }
uint32_t shim_IsDialogEvent(uint32_t *args) { (void)args; return 0; }
uint32_t shim_NewColorDialog(uint32_t *args) { (void)args; return 0; }
uint32_t shim_NewFeaturesDialog(uint32_t *args) { (void)args; return 0; }
uint32_t shim_RemoveDialogItems(uint32_t *args) { (void)args; return 0; }
uint32_t shim_SelectDialogItemText(uint32_t *args) { (void)args; return 0; }
uint32_t shim_SetDialogCancelItem(uint32_t *args) { (void)args; return 0; }
uint32_t shim_SetDialogItem(uint32_t *args) { (void)args; return 0; }
uint32_t shim_SetDialogTracksCursor(uint32_t *args) { (void)args; return 0; }
uint32_t shim_ShowDialogItem(uint32_t *args) { (void)args; return 0; }
uint32_t shim_UpdateDialog(uint32_t *args) { (void)args; return 0; }

// ---- Drag (1) ----
uint32_t shim_DragGrayRgn(uint32_t *args) { (void)args; return 0; }

// ---- Event Manager (3) ----
uint32_t shim_GetNextEvent(uint32_t *args) { (void)args; return 0; }
uint32_t shim_StillDown(uint32_t *args) { (void)args; return 0; }
uint32_t shim_WaitMouseUp(uint32_t *args) { (void)args; return 0; }

// ---- List Manager (22) ----
uint32_t shim_CreateCustomList(uint32_t *args) { (void)args; return 0; }
uint32_t shim_GetListCellSize(uint32_t *args) { (void)args; return 0; }
uint32_t shim_GetListDataBounds(uint32_t *args) { (void)args; return 0; }
uint32_t shim_GetListDataHandle(uint32_t *args) { (void)args; return 0; }
uint32_t shim_GetListRefCon(uint32_t *args) { (void)args; return 0; }
uint32_t shim_GetListVerticalScrollBar(uint32_t *args) { (void)args; return 0; }
uint32_t shim_GetScrapFlavorInfoList(uint32_t *args) { (void)args; return 0; }
uint32_t shim_LAddRow(uint32_t *args) { (void)args; return 0; }
uint32_t shim_LAutoScroll(uint32_t *args) { (void)args; return 0; }
uint32_t shim_LCellSize(uint32_t *args) { (void)args; return 0; }
uint32_t shim_LClick(uint32_t *args) { (void)args; return 0; }
uint32_t shim_LDelRow(uint32_t *args) { (void)args; return 0; }
uint32_t shim_LDispose(uint32_t *args) { (void)args; return 0; }
uint32_t shim_LGetCell(uint32_t *args) { (void)args; return 0; }
uint32_t shim_LGetSelect(uint32_t *args) { (void)args; return 0; }
uint32_t shim_LNew(uint32_t *args) { (void)args; return 0; }
uint32_t shim_LSetCell(uint32_t *args) { (void)args; return 0; }
uint32_t shim_LSetDrawingMode(uint32_t *args) { (void)args; return 0; }
uint32_t shim_LSetSelect(uint32_t *args) { (void)args; return 0; }
uint32_t shim_LSize(uint32_t *args) { (void)args; return 0; }
uint32_t shim_LUpdate(uint32_t *args) { (void)args; return 0; }
uint32_t shim_SetListRefCon(uint32_t *args) { (void)args; return 0; }

// ---- Menu Manager (5) ----
uint32_t shim_AppendMenu(uint32_t *args) { (void)args; return 0; }
uint32_t shim_GetMenu(uint32_t *args) { (void)args; return 0; }
uint32_t shim_GetMenuItemText(uint32_t *args) { (void)args; return 0; }
uint32_t shim_MenuKey(uint32_t *args) { (void)args; return 0; }
uint32_t shim_SetMenuItemText(uint32_t *args) { (void)args; return 0; }

// ---- Scrap Manager (6) ----
uint32_t shim_ClearCurrentScrap(uint32_t *args) { (void)args; return 0; }
uint32_t shim_GetCurrentScrap(uint32_t *args) { (void)args; return 0; }
uint32_t shim_GetScrapFlavorCount(uint32_t *args) { (void)args; return 0; }
uint32_t shim_GetScrapFlavorData(uint32_t *args) { (void)args; return 0; }
uint32_t shim_GetScrapFlavorSize(uint32_t *args) { (void)args; return 0; }
uint32_t shim_PutScrapFlavor(uint32_t *args) { (void)args; return 0; }

// ---- TextEdit (9) ----
uint32_t shim_TECalText(uint32_t *args) { (void)args; return 0; }
uint32_t shim_TEDispose(uint32_t *args) { (void)args; return 0; }
uint32_t shim_TEGetHeight(uint32_t *args) { (void)args; return 0; }
uint32_t shim_TENew(uint32_t *args) { (void)args; return 0; }
uint32_t shim_TEScroll(uint32_t *args) { (void)args; return 0; }
uint32_t shim_TESetText(uint32_t *args) { (void)args; return 0; }
uint32_t shim_TETextBox(uint32_t *args) { (void)args; return 0; }
uint32_t shim_TEToScrap(uint32_t *args) { (void)args; return 0; }
uint32_t shim_TEUpdate(uint32_t *args) { (void)args; return 0; }

// ---- Toolbox misc (2) ----
uint32_t shim_GetStdFilterProc(uint32_t *args) { (void)args; return 0; }
uint32_t shim_RegisterToolboxObjectClass(uint32_t *args) { (void)args; return 0; }

// ---- Type-select (3) ----
uint32_t shim_TypeSelectClear(uint32_t *args) { (void)args; return 0; }
uint32_t shim_TypeSelectFindItem(uint32_t *args) { (void)args; return 0; }
uint32_t shim_TypeSelectNewKey(uint32_t *args) { (void)args; return 0; }

// ---- Window / HIView (7) ----
uint32_t shim_GetWVariant(uint32_t *args) { (void)args; return 0; }
uint32_t shim_GrowWindow(uint32_t *args) { (void)args; return 0; }
uint32_t shim_HIViewReshapeStructure(uint32_t *args) { (void)args; return 0; }
uint32_t shim_HIViewSetNeedsDisplayInRegion(uint32_t *args) { (void)args; return 0; }
uint32_t shim_InstallWindowContentPaintProc(uint32_t *args) { (void)args; return 0; }
uint32_t shim_InvalWindowRgn(uint32_t *args) { (void)args; return 0; }
uint32_t shim_SetWTitle(uint32_t *args) { (void)args; return 0; }
