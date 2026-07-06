// carbon_classic_ui_shim.c — graceful shims for the classic TextEdit, List
// Manager, Drag Manager and Theme-state entry points that abigen's legacy pass
// forwarded to now-removed 64-bit symbols (iPhoto's dangling set + a few Halo).
//
// These managers are dead on modern macOS: TextEdit (TERec/TEHandle), the List
// Manager (ListHandle/LDEF), the classic Drag Manager (DragRef/FlavorType) and the
// classic Theme drawing-state stack were all dropped when Carbon went 64-bit and
// have no surviving native to forward to. Real functionality moved to Cocoa
// (NSTextView / NSTableView / NSDraggingInfo) and cannot be bridged from a classic
// GrafPort-coupled call. Each shim returns the signature-correct benign value and
// zeroes any out-parameter, converting a dangling-symbol abort into a clean "empty
// / not available" answer so the translated app can keep running.
//
// MTSHIM convention: rdi -> &i386 args[0]; uint32_t result in eax. Wired
// ___<Name> -> _shim_<Name>; MTSHIM presence removes them from the abigen legacy
// pass (no duplicate-symbol clash; validates on a RESYNC).

#include <stdint.h>
#include <string.h>

#define PTR(i) ((void *)(uintptr_t)a[(i)])

// ---------------- TextEdit (TERec world removed) ----------------
// Editing/redraw entry points are void -> no-op.
uint32_t shim_TEActivate(uint32_t *a)   { (void)a; return 0; }
uint32_t shim_TEDeactivate(uint32_t *a) { (void)a; return 0; }
uint32_t shim_TEIdle(uint32_t *a)       { (void)a; return 0; }
uint32_t shim_TEKey(uint32_t *a)        { (void)a; return 0; }
uint32_t shim_TECut(uint32_t *a)        { (void)a; return 0; }
uint32_t shim_TECopy(uint32_t *a)       { (void)a; return 0; }
uint32_t shim_TEPaste(uint32_t *a)      { (void)a; return 0; }
uint32_t shim_TEDelete(uint32_t *a)     { (void)a; return 0; }
uint32_t shim_TEInsert(uint32_t *a)     { (void)a; return 0; }
uint32_t shim_TESetSelect(uint32_t *a)  { (void)a; return 0; }
uint32_t shim_TEAutoView(uint32_t *a)   { (void)a; return 0; }
// CharsHandle TEGetText(TEHandle) -> NULL ; TEHandle TEStyleNew(...) -> NULL.
uint32_t shim_TEGetText(uint32_t *a)    { (void)a; return 0; }
uint32_t shim_TEStyleNew(uint32_t *a)   { (void)a; return 0; }

// ---------------- List Manager (ListHandle/LDEF removed) ----------------
uint32_t shim_LActivate(uint32_t *a) { (void)a; return 0; }
uint32_t shim_LScroll(uint32_t *a)   { (void)a; return 0; }
// Boolean LNextCell(Boolean hNext, Boolean vNext, Cell *ioCell, ListHandle) -> false
// (no next selected cell); leave *ioCell as staged.
uint32_t shim_LNextCell(uint32_t *a) { (void)a; return 0; }
// Boolean GetListActive(ListHandle) -> false.
uint32_t shim_GetListActive(uint32_t *a)        { (void)a; return 0; }
uint32_t shim_SetListSelectionFlags(uint32_t *a){ (void)a; return 0; }

// ---------------- Drag Manager (classic DragRef removed) ----------------
// Getters zero their out-parameter and report no drag; setters/adders -> noErr.
uint32_t shim_CountDragItems(uint32_t *a) {           // (DragRef, UInt16 *outItems)
    uint16_t *o = (uint16_t *)PTR(1); if (o) *o = 0; return 0;
}
uint32_t shim_CountDragItemFlavors(uint32_t *a) {     // (DragRef, ItemRef, UInt16 *out)
    uint16_t *o = (uint16_t *)PTR(2); if (o) *o = 0; return 0;
}
uint32_t shim_GetDragItemReferenceNumber(uint32_t *a) { // (DragRef, UInt16 idx, ItemRef*)
    uint32_t *o = (uint32_t *)PTR(2); if (o) *o = 0; return 0;
}
uint32_t shim_GetDragAttributes(uint32_t *a) {        // (DragRef, DragAttributes *out)
    uint32_t *o = (uint32_t *)PTR(1); if (o) *o = 0; return 0;
}
uint32_t shim_GetDragModifiers(uint32_t *a) {         // (DragRef, SInt16*, SInt16*, SInt16*)
    for (int i = 1; i <= 3; i++) { int16_t *o = (int16_t *)PTR(i); if (o) *o = 0; }
    return 0;
}
uint32_t shim_GetDragMouse(uint32_t *a) {             // (DragRef, Point *mouse, Point *pinned)
    int32_t *m = (int32_t *)PTR(1); if (m) *m = 0;
    int32_t *p = (int32_t *)PTR(2); if (p) *p = 0;
    return 0;
}
uint32_t shim_GetDropLocation(uint32_t *a) {          // (DragRef, AEDesc *dropLocation)
    void *o = PTR(1); if (o) memset(o, 0, 8); return 0;   // AEDesc {DescType, AEDataStorage}
}
uint32_t shim_GetDragHiliteColor(uint32_t *a) {       // (WindowRef, RGBColor *color)
    void *o = PTR(1); if (o) memset(o, 0, 6); return 0;   // RGBColor = 3*UInt16
}
uint32_t shim_GetFlavorType(uint32_t *a) {            // (DragRef, ItemRef, UInt16 idx, FlavorType*)
    uint32_t *o = (uint32_t *)PTR(3); if (o) *o = 0; return 0;
}
uint32_t shim_GetFlavorFlags(uint32_t *a) {           // (DragRef, ItemRef, FlavorType, FlavorFlags*)
    uint32_t *o = (uint32_t *)PTR(3); if (o) *o = 0; return 0;
}
uint32_t shim_GetFlavorDataSize(uint32_t *a) {        // (DragRef, ItemRef, FlavorType, Size*)
    uint32_t *o = (uint32_t *)PTR(3); if (o) *o = 0; return 0;
}
uint32_t shim_GetFlavorData(uint32_t *a) {            // (DragRef, ItemRef, FlavorType, void*, Size*, off)
    int32_t *sz = (int32_t *)PTR(4); if (sz) *sz = 0; return (uint32_t)-1856; // badDragFlavorErr
}
uint32_t shim_GetScrapFlavorFlags(uint32_t *a) {      // (ScrapRef, ScrapFlavorType, ScrapFlavorFlags*)
    uint32_t *o = (uint32_t *)PTR(2); if (o) *o = 0; return 0;
}
uint32_t shim_AddDragItemFlavor(uint32_t *a)    { (void)a; return 0; }
uint32_t shim_SetDragItemFlavorData(uint32_t *a){ (void)a; return 0; }
uint32_t shim_SetDropLocation(uint32_t *a)      { (void)a; return 0; }

// ---------------- Theme drawing state (removed) ----------------
uint32_t shim_DisposeThemeDrawingState(uint32_t *a) { (void)a; return 0; }
uint32_t shim_SetThemePen(uint32_t *a)              { (void)a; return 0; }
uint32_t shim_GetThemeScrollBarArrowStyle(uint32_t *a) { // (ThemeScrollBarArrowStyle *out)
    int32_t *o = (int32_t *)PTR(0); if (o) *o = 0; return 0;
}

// ---------------- Mouse tracking (classic TrackMouseLocation removed) ----------------
// OSStatus TrackMouseLocation(GrafPtr, Point *outPt, MouseTrackingResult *outResult)
// -> report "mouse released" (0) so a tracking loop terminates immediately.
uint32_t shim_TrackMouseLocation(uint32_t *a) {
    int32_t *pt = (int32_t *)PTR(1); if (pt) *pt = 0;
    int16_t *res = (int16_t *)PTR(2); if (res) *res = 0;  // kMouseTrackingMouseReleased
    return 0;
}
uint32_t shim_TrackMouseLocationWithOptions(uint32_t *a) {
    // (GrafPtr, options, timeout, Point *outPt, UInt32 *outModifiers, MouseTrackingResult*)
    int32_t *pt = (int32_t *)PTR(3); if (pt) *pt = 0;
    uint32_t *mod = (uint32_t *)PTR(4); if (mod) *mod = 0;
    int16_t *res = (int16_t *)PTR(5); if (res) *res = 0;
    return 0;
}
