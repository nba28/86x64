/*
 * nav_shim.m — GOLDEN functional Navigation Services -> modern AppKit bridge.
 *
 * The classic Carbon Navigation Services file dialogs (NavGetFile / NavPutFile /
 * NavChooseFile / NavChooseFolder / NavChooseObject and the Carbon-only
 * create/run/reply API NavCreate*Dialog + NavDialogRun + NavDialogGetReply) were
 * removed from 64-bit macOS. Translated i386 apps still call them to let the user
 * open/save/choose files. This shim REIMPLEMENTS them for real on top of AppKit's
 * NSOpenPanel / NSSavePanel and marshals the result (FSRefs / file name) back
 * through the i386 caller's frame.
 *
 * Reached from translated i386 code through the ___Nav<Name> trampolines in
 * maptable_tramp.asm (NavServices block): each hands us a pointer `a` to the i386
 * cdecl arg block (4-byte stack slots) and returns our uint32_t (OSErr in eax).
 *
 * THE REPLY.selection MARSHALLING (the crux). abigen's Apple Event shims marshal
 * an AEDesc's `dataHandle` (an AEDataStorage*, a >4GB native heap pointer) across
 * the i386 boundary through libabiconv's object wrap table: the copy-BACK path
 * wraps it with x64_objc_wrap (only if >4GB), and the arg-IN path (e.g. the app's
 * ___AEGetNthPtr(&reply.selection,...)) unwraps it with unwrap_obj_arg — the exact
 * arena inverse. So to hand back a selection the app can read, we build a REAL
 * native AEDescList (AECreateList + AEPutPtr typeFSRef) and store, at the i386
 * reply's packed offsets, the descriptorType (4 bytes verbatim) and
 * x64_objc_wrap(dataHandle). The app's own ___AECountItems / ___AEGetNthPtr then
 * unwrap and read it natively. CFStringRef fields (saveFileName) are wrapped the
 * same way.
 *
 * i386 STRUCT LAYOUT: classic Carbon structs are 2-byte packed on i386 (verified
 * by dumping an i386-compiled offsetof table). NavReplyRecord is 256 bytes with
 * selection.descriptorType@6, dataHandle@10, keyScript@14, fileTranslation@16,
 * reserved1@20, saveFileName@24, saveFileExtHidden@28. We read/write those exact
 * byte offsets rather than casting to a native (differently-laid-out) struct.
 *
 * NavDialogRef is our OWN opaque handle (the app only ever passes it back to us):
 * we wrap our native NavCtx* with x64_objc_wrap so it is a 32-bit-representable
 * handle regardless of where malloc put it, and unwrap on entry.
 *
 * UNIVERSAL, not app-specific: any translated i386 Carbon app that opens/saves/
 * chooses files through Navigation Services benefits. Single purpose (file
 * dialogs only); HIToolbox windows/controls/menus/NIB are owned elsewhere.
 */

#import <AppKit/AppKit.h>
#import <CoreServices/CoreServices.h>   /* AEDesc/AECreateList/AEPutPtr, FSRef, CFURLGetFSRef */
#include <stdint.h>
#include <string.h>
#include <dispatch/dispatch.h>

/* libabiconv object wrap table (objc_shim.c): native ptr <-> 32-bit handle,
 * the SAME table abigen's AE/CF shims use, so our marshalled handles round-trip
 * through the app's own generated AE/CF trampolines. */
extern uint32_t x64_objc_wrap(uint64_t real);
extern uint64_t x64_objc_unwrap(uint32_t h);

/* ---- classic result / action / selector vocabulary (i386 widths) -------- */
enum { navNoErr = 0, navParamErr = -50, navUserCanceledErr = -128 };

enum {                                   /* NavUserAction */
   kNavUserActionNone      = 0,
   kNavUserActionCancel    = 1,
   kNavUserActionOpen      = 2,
   kNavUserActionSaveAs    = 3,
   kNavUserActionChoose    = 4,
   kNavUserActionNewFolder = 5,
};

enum {                                   /* NavCustomControlMessage (subset we honor) */
   kNavCtlGetLocation     = 7,
   kNavCtlSetLocation     = 8,
   kNavCtlGetSelection    = 9,
   kNavCtlSetSelection    = 10,
   kNavCtlShowSelection   = 11,
   kNavCtlCancel          = 15,
   kNavCtlAccept          = 16,
   kNavCtlGetEditFileName = 23,
   kNavCtlSetEditFileName = 24,
   kNavCtlTerminate       = 30,
};

/* ---- i386 NavReplyRecord packed offsets (2-byte packing; verified) ------ */
enum {
   RPL_VERSION   = 0,   /* UInt16  */
   RPL_VALID     = 2,   /* Boolean */
   RPL_REPLACING = 3,
   RPL_STATIONERY= 4,
   RPL_XLATE     = 5,
   RPL_SEL_TYPE  = 6,   /* AEDesc.descriptorType (DescType, 4) */
   RPL_SEL_HANDLE= 10,  /* AEDesc.dataHandle      (AEDataStorage*, 4 on i386) */
   RPL_KEYSCRIPT = 14,  /* ScriptCode (SInt16) */
   RPL_FILEXLATE = 16,  /* Handle (4) */
   RPL_RESERVED1 = 20,  /* UInt32 */
   RPL_SAVENAME  = 24,  /* CFStringRef (4) */
   RPL_EXTHIDDEN = 28,  /* Boolean */
   RPL_SIZE      = 256,
};

/* NavGetDefaultDialogCreationOptions / ...DialogOptions zero-fill sizes. */
enum { NAV_CREATION_OPTS_SIZE = 66, NAV_DIALOG_OPTS_SIZE = 2048 };

/* ---- i386 <-> native pointer helpers ------------------------------------ */
static inline void    *i386_ptr(uint32_t v)      { return (void *)(uintptr_t)v; }
static inline uint8_t *reply_base(uint32_t v)    { return (uint8_t *)(uintptr_t)v; }

static void put8 (uint8_t *b, int off, uint8_t v)  { if (b) b[off] = v; }
static void put16(uint8_t *b, int off, uint16_t v) { if (b) memcpy(b + off, &v, 2); }
static void put32(uint8_t *b, int off, uint32_t v) { if (b) memcpy(b + off, &v, 4); }
static uint32_t get32(const uint8_t *b, int off)   { uint32_t v = 0; if (b) memcpy(&v, b + off, 4); return v; }

/* ---- our dialog context (opaque NavDialogRef backing) ------------------- */
typedef enum { NAV_OPEN_FILE, NAV_OPEN_FOLDER, NAV_OPEN_OBJECT, NAV_SAVE } nav_kind;

typedef struct NavCtx {
   uint32_t magic;                 /* 'NvCx' validity */
   nav_kind kind;
   void    *panel;                 /* retained NSOpenPanel/NSSavePanel */
   int32_t  action;                /* kNavUserAction* captured after run */
   int      ran;
   void    *urls;                  /* retained NSArray<NSURL*> (chosen; save: parent dir) */
   void    *saveName;              /* retained NSString (save file name) */
} NavCtx;

#define NAVCTX_MAGIC 0x4E764378u   /* 'NvCx' */

static NavCtx *ctx_from_ref(uint32_t ref) {
   if (!ref) return NULL;
   NavCtx *c = (NavCtx *)(uintptr_t)x64_objc_unwrap(ref);
   if (!c || c->magic != NAVCTX_MAGIC) return NULL;
   return c;
}

/* Run a block synchronously on the main thread (AppKit panels are main-thread
 * only). Nav dialogs are inherently modal calls from the app's main (UI) thread,
 * so the common case runs inline with no dispatch. */
static void run_on_main(void (^blk)(void)) {
   if ([NSThread isMainThread]) { blk(); return; }
   dispatch_sync(dispatch_get_main_queue(), blk);
}

/* Configure an NSOpenPanel/NSSavePanel for a kind + capture nothing yet. */
static id make_panel(nav_kind kind) {
   if (kind == NAV_SAVE) {
      NSSavePanel *p = [NSSavePanel savePanel];
      return [p retain];
   }
   NSOpenPanel *p = [NSOpenPanel openPanel];
   if (kind == NAV_OPEN_FOLDER) {
      [p setCanChooseFiles:NO];
      [p setCanChooseDirectories:YES];
   } else if (kind == NAV_OPEN_OBJECT) {
      [p setCanChooseFiles:YES];
      [p setCanChooseDirectories:YES];
   } else {
      [p setCanChooseFiles:YES];
      [p setCanChooseDirectories:NO];
   }
   [p setAllowsMultipleSelection:NO];
   return [p retain];
}

/* Run the panel modally, capture action + result URLs/name into the ctx. */
static void run_ctx(NavCtx *c) {
   if (!c || c->ran) return;
   run_on_main(^{
      @autoreleasepool {
         if (NSApp) [NSApp activateIgnoringOtherApps:YES];
         NSInteger resp = [(NSSavePanel *)c->panel runModal];
         if (resp == NSModalResponseOK) {
            if (c->kind == NAV_SAVE) {
               NSURL *u = [(NSSavePanel *)c->panel URL];
               c->saveName = [[u lastPathComponent] retain];
               NSURL *dir = [u URLByDeletingLastPathComponent];
               c->urls = [(dir ? @[dir] : @[]) retain];
               c->action = kNavUserActionSaveAs;
            } else {
               NSArray *us = [(NSOpenPanel *)c->panel URLs];
               c->urls = [(us ? us : @[]) retain];
               c->action = (c->kind == NAV_OPEN_FILE) ? kNavUserActionOpen
                                                      : kNavUserActionChoose;
            }
         } else {
            c->action = kNavUserActionCancel;
         }
      }
   });
   c->ran = 1;
}

/* Build a native typeFSRef AEDescList from an array of NSURLs; store its
 * descriptorType + wrapped dataHandle into the i386 reply.selection. Returns the
 * item count actually stored. */
static int fill_selection(uint8_t *rb, NSArray *urls) {
   AEDescList list; int n = 0;
   if (AECreateList(NULL, 0, false, &list) != noErr) {
      put32(rb, RPL_SEL_TYPE, 0);
      put32(rb, RPL_SEL_HANDLE, 0);
      return 0;
   }
   for (NSURL *u in urls) {
      FSRef ref;
      if (u && CFURLGetFSRef((CFURLRef)u, &ref)) {
         if (AEPutPtr(&list, 0, typeFSRef, &ref, sizeof(FSRef)) == noErr) n++;
      }
   }
   put32(rb, RPL_SEL_TYPE, (uint32_t)list.descriptorType);
   uint64_t h = (uint64_t)(uintptr_t)list.dataHandle;
   put32(rb, RPL_SEL_HANDLE, h ? x64_objc_wrap(h) : 0);
   return n;
}

/* Populate an i386 NavReplyRecord from a run ctx. */
static void fill_reply(NavCtx *c, uint32_t reply32) {
   uint8_t *rb = reply_base(reply32);
   if (!rb) return;
   memset(rb, 0, RPL_SIZE);
   put16(rb, RPL_VERSION, 2);           /* v2: saveFileName + extHidden present */
   int cancelled = (c->action == kNavUserActionCancel || c->action == kNavUserActionNone);
   if (cancelled) { put8(rb, RPL_VALID, 0); return; }

   NSArray *urls = (NSArray *)c->urls;
   int n = fill_selection(rb, urls);
   put8(rb, RPL_VALID, (n > 0 || c->kind == NAV_SAVE) ? 1 : 0);

   if (c->kind == NAV_SAVE && c->saveName) {
      CFStringRef s = (CFStringRef)c->saveName;   /* keep alive; app copies it */
      uint64_t sp = (uint64_t)(uintptr_t)s;
      put32(rb, RPL_SAVENAME, sp ? x64_objc_wrap(sp) : 0);
      put8(rb, RPL_EXTHIDDEN, 0);
   }
}

static void dispose_ctx(NavCtx *c) {
   if (!c) return;
   run_on_main(^{
      if (c->panel)    [(id)c->panel release];
      if (c->urls)     [(id)c->urls release];
      if (c->saveName) [(id)c->saveName release];
   });
   c->magic = 0;
   free(c);
}

/* ======================================================================== *
 *  Create / run / reply API (the modern Carbon-only path iPhoto uses)      *
 * ======================================================================== */

static uint32_t create_dialog(nav_kind kind, uint32_t out_dialog32) {
   NavCtx *c = (NavCtx *)calloc(1, sizeof(NavCtx));
   if (!c) return (uint32_t)navParamErr;
   c->magic = NAVCTX_MAGIC;
   c->kind  = kind;
   run_on_main(^{ c->panel = make_panel(kind); });
   uint32_t ref = x64_objc_wrap((uint64_t)(uintptr_t)c);
   put32(i386_ptr(out_dialog32), 0, ref);
   return navNoErr;
}

/* NavCreatePutFileDialog(options, fileType, fileCreator, eventProc, cbUD, *out) */
uint32_t shim_NavCreatePutFileDialog(const uint32_t *a) {
   return create_dialog(NAV_SAVE, a[5]);
}
/* NavCreateGetFileDialog(options, typeList, eventProc, previewProc, filterProc, cbUD, *out) */
uint32_t shim_NavCreateGetFileDialog(const uint32_t *a) {
   return create_dialog(NAV_OPEN_FILE, a[6]);
}
/* NavCreateChooseFolderDialog(options, eventProc, filterProc, cbUD, *out) */
uint32_t shim_NavCreateChooseFolderDialog(const uint32_t *a) {
   return create_dialog(NAV_OPEN_FOLDER, a[4]);
}
/* NavCreateChooseFileDialog(options, typeList, eventProc, previewProc, filterProc, cbUD, *out) */
uint32_t shim_NavCreateChooseFileDialog(const uint32_t *a) {
   return create_dialog(NAV_OPEN_FILE, a[6]);
}
/* NavCreateChooseObjectDialog(options, eventProc, previewProc, filterProc, cbUD, *out) */
uint32_t shim_NavCreateChooseObjectDialog(const uint32_t *a) {
   return create_dialog(NAV_OPEN_OBJECT, a[5]);
}

/* NavDialogRun(NavDialogRef) */
uint32_t shim_NavDialogRun(const uint32_t *a) {
   NavCtx *c = ctx_from_ref(a[0]);
   if (!c) return (uint32_t)navParamErr;
   run_ctx(c);
   return navNoErr;
}

/* NavDialogGetUserAction(NavDialogRef) -> NavUserAction */
uint32_t shim_NavDialogGetUserAction(const uint32_t *a) {
   NavCtx *c = ctx_from_ref(a[0]);
   return c ? (uint32_t)c->action : kNavUserActionNone;
}

/* NavDialogGetReply(NavDialogRef, NavReplyRecord *outReply) */
uint32_t shim_NavDialogGetReply(const uint32_t *a) {
   NavCtx *c = ctx_from_ref(a[0]);
   if (!c || !a[1]) return (uint32_t)navParamErr;
   fill_reply(c, a[1]);
   return navNoErr;
}

/* NavDialogGetSaveFileName(NavDialogRef) -> CFStringRef */
uint32_t shim_NavDialogGetSaveFileName(const uint32_t *a) {
   NavCtx *c = ctx_from_ref(a[0]);
   if (!c) return 0;
   __block uint32_t r = 0;
   run_on_main(^{
      if (c->kind == NAV_SAVE && c->panel) {
         NSString *nm = [(NSSavePanel *)c->panel nameFieldStringValue];
         if (nm) r = x64_objc_wrap((uint64_t)(uintptr_t)nm);
      } else if (c->saveName) {
         r = x64_objc_wrap((uint64_t)(uintptr_t)c->saveName);
      }
   });
   return r;
}

/* NavDialogSetSaveFileName(NavDialogRef, CFStringRef name) */
uint32_t shim_NavDialogSetSaveFileName(const uint32_t *a) {
   NavCtx *c = ctx_from_ref(a[0]);
   if (!c) return (uint32_t)navParamErr;
   NSString *nm = (NSString *)(uintptr_t)x64_objc_unwrap(a[1]);
   run_on_main(^{
      if (nm && c->kind == NAV_SAVE && c->panel)
         [(NSSavePanel *)c->panel setNameFieldStringValue:nm];
   });
   return navNoErr;
}

/* NavDialogGetWindow(NavDialogRef) -> WindowRef.
 * We have an NSWindow, not a Carbon WindowRef; handing back a wrapped NSWindow
 * would make the caller invoke Carbon WindowRef APIs on a non-WindowRef and
 * crash. Return NULL — callers use it only for optional customization/parenting,
 * which they must tolerate skipping (Halo's single use). */
uint32_t shim_NavDialogGetWindow(const uint32_t *a) {
   (void)a; return 0;
}

/* NavDialogDispose(NavDialogRef) */
uint32_t shim_NavDialogDispose(const uint32_t *a) {
   NavCtx *c = ctx_from_ref(a[0]);
   if (c) dispose_ctx(c);
   return navNoErr;
}

/* NavCustomControl(NavDialogRef, selector, parms) — honor the benign selectors;
 * ignore layout/preview tweaks. Returns noErr so callers proceed. */
uint32_t shim_NavCustomControl(const uint32_t *a) {
   NavCtx *c = ctx_from_ref(a[0]);
   uint32_t sel = a[1];
   if (!c) return (uint32_t)navParamErr;
   switch (sel) {
      case kNavCtlSetEditFileName: {
         NSString *nm = (NSString *)(uintptr_t)x64_objc_unwrap(a[2]);
         run_on_main(^{
            if (nm && c->kind == NAV_SAVE && c->panel)
               [(NSSavePanel *)c->panel setNameFieldStringValue:nm];
         });
         return navNoErr;
      }
      default:
         return navNoErr;   /* location/selection/preview tweaks: safely ignored */
   }
}

/* ======================================================================== *
 *  Classic blocking API (fills the reply directly)                         *
 * ======================================================================== */

static uint32_t blocking(nav_kind kind, uint32_t reply32) {
   NavCtx c; memset(&c, 0, sizeof c);
   c.magic = NAVCTX_MAGIC; c.kind = kind;
   NavCtx *cp = &c;                       /* blocks capture the pointer, not the struct */
   run_on_main(^{ cp->panel = make_panel(kind); });
   run_ctx(cp);
   fill_reply(cp, reply32);
   run_on_main(^{
      if (cp->panel)    [(id)cp->panel release];
      if (cp->urls)     [(id)cp->urls release];
      if (cp->saveName) [(id)cp->saveName release];
   });
   return navNoErr;
}

/* NavGetFile(defaultLocation, NavReplyRecord*, options, evt, prev, filter, typeList, cbUD) */
uint32_t shim_NavGetFile(const uint32_t *a)      { return blocking(NAV_OPEN_FILE,   a[1]); }
/* NavChooseFile(...) same reply slot as NavGetFile */
uint32_t shim_NavChooseFile(const uint32_t *a)   { return blocking(NAV_OPEN_FILE,   a[1]); }
/* NavChooseFolder(defaultLocation, NavReplyRecord*, options, evt, filter, cbUD) */
uint32_t shim_NavChooseFolder(const uint32_t *a) { return blocking(NAV_OPEN_FOLDER, a[1]); }
/* NavChooseObject(defaultLocation, NavReplyRecord*, options, evt, filter, cbUD) */
uint32_t shim_NavChooseObject(const uint32_t *a) { return blocking(NAV_OPEN_OBJECT, a[1]); }
/* NavPutFile(defaultLocation, NavReplyRecord*, options, evt, fileType, fileCreator, cbUD) */
uint32_t shim_NavPutFile(const uint32_t *a)      { return blocking(NAV_SAVE,        a[1]); }

/* ======================================================================== *
 *  Reply lifecycle + defaults + availability                              *
 * ======================================================================== */

/* NavDisposeReply(NavReplyRecord*) — release the selection AEDescList (via native
 * AEDisposeDesc on the unwrapped handle) and the wrapped saveFileName. */
uint32_t shim_NavDisposeReply(const uint32_t *a) {
   uint8_t *rb = reply_base(a[0]);
   if (!rb) return navNoErr;
   uint32_t selType = get32(rb, RPL_SEL_TYPE);
   uint32_t selH    = get32(rb, RPL_SEL_HANDLE);
   if (selH) {
      AEDesc d; d.descriptorType = (DescType)selType;
      d.dataHandle = (AEDataStorage)(uintptr_t)x64_objc_unwrap(selH);
      if (d.dataHandle) AEDisposeDesc(&d);
      put32(rb, RPL_SEL_HANDLE, 0);
   }
   uint32_t nm = get32(rb, RPL_SAVENAME);
   if (nm) {
      NSString *s = (NSString *)(uintptr_t)x64_objc_unwrap(nm);
      if (s) [s release];
      put32(rb, RPL_SAVENAME, 0);
   }
   put8(rb, RPL_VALID, 0);
   return navNoErr;
}

/* NavCompleteSave(NavReplyRecord*, howToTranslate) — NSSavePanel already applied
 * extension policy; nothing left to finalize. */
uint32_t shim_NavCompleteSave(const uint32_t *a) { (void)a; return navNoErr; }

/* NavGetDefaultDialogCreationOptions(NavDialogCreationOptions*) */
uint32_t shim_NavGetDefaultDialogCreationOptions(const uint32_t *a) {
   uint8_t *p = (uint8_t *)i386_ptr(a[0]);
   if (!p) return (uint32_t)navParamErr;
   memset(p, 0, NAV_CREATION_OPTS_SIZE);
   put16(p, 0, 0);                       /* version = kNavDialogCreationOptionsVersion */
   return navNoErr;
}

/* NavGetDefaultDialogOptions(NavDialogOptions*) */
uint32_t shim_NavGetDefaultDialogOptions(const uint32_t *a) {
   uint8_t *p = (uint8_t *)i386_ptr(a[0]);
   if (!p) return (uint32_t)navParamErr;
   memset(p, 0, NAV_DIALOG_OPTS_SIZE);
   put16(p, 0, 0);                       /* version = kNavDialogOptionsVersion */
   return navNoErr;
}

/* NavServicesAvailable() -> Boolean, NavServicesCanRun() -> Boolean */
uint32_t shim_NavServicesAvailable(const uint32_t *a) { (void)a; return 1; }
uint32_t shim_NavServicesCanRun(const uint32_t *a)    { (void)a; return 1; }

/* NavLoad()/NavUnload() — no library to (un)load; report success. */
uint32_t shim_NavLoad(const uint32_t *a)   { (void)a; return navNoErr; }
uint32_t shim_NavUnload(const uint32_t *a) { (void)a; return navNoErr; }
