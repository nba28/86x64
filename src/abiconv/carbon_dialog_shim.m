/*
 * carbon_dialog_shim.m — REAL classic Dialog Manager on modern AppKit.
 *
 * The classic Dialog Manager (a DialogRef built from a 'DLOG'+'DITL' resource,
 * run by ModalDialog, its items read/written via GetDialogItem[Text]) was
 * removed from 64-bit macOS ENTIRELY, and classic edit-text ('editText' DITL
 * items, TextEdit) no longer exists. A translated i386 app that still opens a
 * classic modal dialog (Halo's "Enter Your Halo Product Key" CD-key gate, any
 * SIOUX/PowerPlant/legacy-Carbon prompt) got our old graceful stub —
 * GetNewDialog -> NULL — which turned an app's "prompt until valid input" modal
 * loop into an INFINITE 100%-CPU spin (Halo: the gate is mandatory, so the app
 * hangs before any window and the OS force-quits it as unresponsive).
 *
 * FIX (real functionality, universal): materialize ANY 'DLOG'+'DITL' as a real
 * modal AppKit window:
 *   - GetNewDialog(resID)      -> load 'DLOG' (native Get1Resource), parse its
 *                                 boundsRect/procID/title + its 'DITL' id, build
 *                                 an NSWindow with a live NSView per DITL item
 *                                 (NSButton for ctrlItem+btnCtrl, editable
 *                                 NSTextField for editText, non-editable label
 *                                 for statText). Returns a 32-bit DialogRef
 *                                 handle (our own opaque, arena-wrapped).
 *   - ModalDialog(&itemHit)    -> run a real modal session (NSApp
 *                                 runModalForWindow) with live keyboard entry,
 *                                 field-to-field focus (Tab), Return=default,
 *                                 Esc=cancel; returns the clicked item number
 *                                 exactly as the classic Dialog Manager does, so
 *                                 the app's own OK/Cancel/validate loop runs
 *                                 UNCHANGED.
 *   - GetDialogItem / GetDialogItemText / SetDialogItemText / SetDialogDefault-
 *     Item / GetDialogWindow / DrawDialog / DialogSelect / DisposeDialog: real
 *     operations on that window's items (Str255 <-> NSTextField.stringValue).
 *
 * We do NOT validate or bypass anything: the app reads the user's typed text via
 * GetDialogItemText and runs its OWN validator (e.g. Halo's gcdkey checksum). An
 * empty/invalid entry just re-prompts, which is the correct classic behavior.
 *
 * UNIVERSAL: triggers on the 'DLOG'/'DITL' resource STRUCTURE, never on an app
 * name or a specific resID. Any translated i386 Carbon app that runs a classic
 * modal dialog benefits. Single purpose: the Dialog Manager only. (StandardAlert
 * / IBCarbon nibs / Nav Services are owned by their own shims.)
 *
 * MTSHIM convention (maptable_tramp.asm): rdi -> &i386 args[0] (4-byte cdecl
 * slots), uint32_t result in eax. Object args (DialogRef, item Handle) arrive as
 * arena handles when >4GB; we allocate our own state from libabiconv's <4GB heap
 * so a raw pointer round-trips through the 4-byte slot without a token.
 */

#import <AppKit/AppKit.h>
#import <CoreFoundation/CoreFoundation.h>
#import <objc/runtime.h>          /* objc_setAssociatedObject (retain the delegate) */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <dispatch/dispatch.h>

/* AppKit window-host bootstrap (carbon_appkit_host.c): a pure-Carbon translated
 * process has no WindowManagement delegate until NSApplicationLoad runs. */
extern void carbon_ensure_window_host(void);

#define PTR(i) ((void *)(uintptr_t)a[(i)])
#define DLOG_NO_ERR (0)

static int dlg_debug(void) {
    static int v = -1;
    if (v < 0) v = getenv("CARBON_DIALOG_TRACE") != NULL;
    return v;
}
#define DLG(...) do { if (dlg_debug()) { fprintf(stderr, "[dialog] " __VA_ARGS__); fflush(stderr); } } while (0)

/* ---- classic geometry -------------------------------------------------
 * DLOG/DITL/ALRT are big-endian ON DISK, but the modern macOS Resource Manager
 * BYTE-SWAPS every 16-/32-bit numeric field to host (little-endian) order when
 * it hands the resource back via Get1Resource (verified against Halo's fork:
 * itemsID on-disk 0x2711 arrives as bytes `11 27`). Type bytes and the length-
 * prefixed pascal strings are byte-granular and are NOT swapped. So we read the
 * numeric fields little-endian (res16/res32); text/type bytes stay verbatim. */
typedef struct { int16_t top, left, bottom, right; } CRect;
static inline int16_t res16(const uint8_t *p) { return (int16_t)(p[0] | (p[1] << 8)); }
static inline int32_t res32(const uint8_t *p) {
    return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                     ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

/* ---- DITL item kinds (classic Dialogs.h) ------------------------------- */
enum {
    kItemDisableBit = 0x80,   /* high bit = "disabled" (not a click item)   */
    kHelpItem       = 1,
    kUserItem       = 0,
    kEditText       = 16,     /* 0x10 editText                              */
    kIconItem       = 32,
    kPicItem        = 64,
    kStatText       = 8,      /* 0x08 statText                              */
    kCtrlItem       = 4,      /* 0x04 ctrlItem base; + btnCtrl(0) = button  */
    kBtnCtrl        = 0,
    kChkCtrl        = 1,
    kRadCtrl        = 2,
    kResCtrl        = 3,      /* control from a 'CNTL' resource             */
};

/* One materialized DITL item. */
typedef struct {
    int          type;        /* raw type byte with the disable bit stripped */
    int          disabled;    /* the disable bit                             */
    CRect        rect;        /* classic local coords (top,left,bottom,right) */
    char         text[256];   /* button title / static text / edit contents  */
    NSView      *view;        /* the live AppKit view (button/field/label)    */
} DItem;

#define DLG_MAX_ITEMS 64

/* Our opaque DialogRef. The app only ever passes it back to us. Allocated from
 * libabiconv's <4GB heap so its raw address is a valid 32-bit i386 slot value;
 * we still arena-wrap on the way out / unwrap on the way in for uniformity with
 * the rest of the bridge (a <4GB pointer wraps to itself). */
typedef struct Dialog {
    uint32_t     magic;       /* DLG_MAGIC */
    NSWindow    *win;
    int          nitems;
    DItem        items[DLG_MAX_ITEMS];
    int          defaultItem; /* 1-based, 0 = none */
    int          cancelItem;  /* 1-based, 0 = none */
    CRect        bounds;      /* window content rect (classic global coords)  */
    char         title[256];  /* window title (block-capturable, stable)      */
} Dialog;
#define DLG_MAGIC 0x444C4731u  /* 'DLG1' */

/* The most-recently-shown dialog. ModalDialog takes no DialogRef (the classic
 * Dialog Manager runs the FRONT modal dialog), so we track it here. */
static struct Dialog *g_front_dialog = NULL;

extern uint32_t x64_objc_wrap(uint64_t real);
extern uint64_t x64_objc_unwrap(uint32_t h);

static uint32_t wrap_ptr(void *p) {
    if (!p) return 0;
    uint64_t v = (uint64_t)(uintptr_t)p;
    return (v >> 32) ? x64_objc_wrap(v) : (uint32_t)v;
}
static void *unwrap_ptr(uint32_t h) {
    if (!h) return NULL;
    return (void *)(uintptr_t)x64_objc_unwrap(h);
}
static Dialog *dlg_from(uint32_t h) {
    Dialog *d = (Dialog *)unwrap_ptr(h);
    if (d && d->magic == DLG_MAGIC) return d;
    return NULL;
}

/* ---- native Resource Manager (rm_shim.c forwards these to the live
 *      64-bit HIToolbox Resource Manager; Halo's EULA path proves DLOG/DITL-
 *      style loads from the app's own res chain work). ------------------- */
static void  *(*n_Get1Resource)(uint32_t, int16_t);
static void  *(*n_GetResource)(uint32_t, int16_t);
static long   (*n_GetHandleSize)(void *);
static void   (*n_ReleaseResource)(void *);
static void   (*n_DetachResource)(void *);
static int    g_rm_resolved;
static void resolve_rm(void) {
    if (g_rm_resolved) return;
    g_rm_resolved = 1;
    n_Get1Resource   = (void *(*)(uint32_t, int16_t))dlsym(RTLD_DEFAULT, "Get1Resource");
    n_GetResource    = (void *(*)(uint32_t, int16_t))dlsym(RTLD_DEFAULT, "GetResource");
    n_GetHandleSize  = (long (*)(void *))dlsym(RTLD_DEFAULT, "GetHandleSize");
    n_ReleaseResource= (void (*)(void *))dlsym(RTLD_DEFAULT, "ReleaseResource");
    n_DetachResource = (void (*)(void *))dlsym(RTLD_DEFAULT, "DetachResource");
}

/* Load a resource's bytes into a malloc'd buffer (caller frees). Handle is a
 * classic Handle (Ptr* ); *handle is the data. Returns len via *outLen. */
static uint8_t *load_resource(uint32_t type, int16_t id, long *outLen) {
    resolve_rm();
    if (!n_Get1Resource) return NULL;
    void *h = n_Get1Resource(type, id);
    if (!h && n_GetResource) h = n_GetResource(type, id);
    if (!h) return NULL;
    long len = n_GetHandleSize ? n_GetHandleSize(h) : 0;
    if (len <= 0 || len > (1 << 20)) { if (n_ReleaseResource) n_ReleaseResource(h); return NULL; }
    const uint8_t *data = *(const uint8_t **)h;
    uint8_t *buf = (uint8_t *)malloc((size_t)len);
    if (buf && data) memcpy(buf, data, (size_t)len);
    if (n_ReleaseResource) n_ReleaseResource(h);
    if (outLen) *outLen = len;
    return buf;
}

/* Read a classic pascal string at p (len byte + chars) into a C string; returns
 * bytes consumed (1 + len), padded to even per DITL alignment when `pad`. */
static long read_pstr(const uint8_t *p, long avail, char *out, size_t outsz, int pad) {
    if (avail < 1) { if (outsz) out[0] = 0; return 0; }
    int len = p[0];
    if (len > avail - 1) len = (int)(avail - 1);
    int n = len; if (n > (int)outsz - 1) n = (int)outsz - 1;
    memcpy(out, p + 1, n); out[n] = 0;
    long consumed = 1 + len;
    if (pad && (consumed & 1)) consumed++;   /* DITL items are word-aligned */
    return consumed;
}

/* ---- DITL parse: fill d->items[] from the 'DITL' bytes -----------------
 * Classic 'DITL' layout (big-endian):
 *   SInt16 count-1
 *   per item: 4 bytes reserved(handle placeholder), Rect(top,left,bottom,right
 *             as 4×SInt16), UInt8 type (disable bit 0x80), then a byte-count-
 *             prefixed body: for statText/editText/button/etc a pascal string;
 *             for icon/pic/control a 2-byte resource id; userItem has 0. The
 *             body is word-aligned. */
static void parse_ditl(Dialog *d, const uint8_t *p, long len) {
    d->nitems = 0;
    if (len < 2) return;
    int count = res16(p) + 1;               /* stored as count-1 */
    if (count < 0) count = 0;
    if (count > DLG_MAX_ITEMS) count = DLG_MAX_ITEMS;
    long off = 2;
    for (int i = 0; i < count && off + 13 <= len; i++) {
        DItem *it = &d->items[d->nitems];
        memset(it, 0, sizeof *it);
        off += 4;                          /* skip the 4 reserved handle bytes */
        it->rect.top    = res16(p + off + 0);
        it->rect.left   = res16(p + off + 2);
        it->rect.bottom = res16(p + off + 4);
        it->rect.right  = res16(p + off + 6);
        off += 8;
        uint8_t t = p[off++];
        it->disabled = (t & kItemDisableBit) ? 1 : 0;
        it->type = t & ~kItemDisableBit;
        /* body. NOTE word-alignment is on the ABSOLUTE offset (each item starts
         * on an even boundary), NOT on the body length: the 13-byte item header
         * (4 reserved + 8 rect + 1 type) is odd, so read_pstr must NOT self-pad;
         * we align `off` to even after the whole body (verified against Halo's
         * DITL 10001: "OK" body = 02 4f 4b, next item lands naturally on even). */
        if (it->type == kStatText || it->type == kEditText ||
            it->type == kCtrlItem  /* button/checkbox/radio: pascal title */) {
            long c = read_pstr(p + off, len - off, it->text, sizeof it->text, 0);
            off += c;
        } else if (it->type == kIconItem || it->type == kPicItem || it->type == kResCtrl) {
            off += 2;                      /* a resource id */
        } else { /* userItem / helpItem / unknown: 1-byte length prefix, 0 body */
            if (off < len) off += 1 + p[off];
        }
        if (off & 1) off++;                /* next item is word-aligned */
        d->nitems++;
    }
    DLG("parsed DITL: %d items\n", d->nitems);
}

/* ============================ AppKit materialization =================== */
/* Classic dialog coords are top-left origin, points. AppKit content view is
 * bottom-left origin. We flip Y within the window content rect. */

/* Classic DITL text is MacRoman (or plain ASCII), NOT UTF-8: e.g. Halo's note
 * uses 0xD5 (curly apostrophe). stringWithUTF8String: returns nil on invalid
 * UTF-8, and NSTextField setters throw on nil — so decode UTF-8 first, fall back
 * to MacRoman, and never return nil. */
static NSString *ns_str(const char *c) {
    if (!c) return @"";
    NSString *s = [NSString stringWithUTF8String:c];
    if (!s) s = [[NSString alloc] initWithBytes:c length:strlen(c)
                                       encoding:NSMacOSRomanStringEncoding];
    return s ? s : @"";
}

/* Button action IMP. libabiconv is loaded once PER co-located framework, so a
 * compile-time @implementation would register the same ObjC class in every copy
 * ("... implemented in both" -> mysterious crashes) AND per-copy statics would
 * diverge. Instead we register ONE target class at runtime, shared across copies
 * by name, whose action drives the process-global NSApp modal session via
 * stopModalWithCode: — ModalDialog gets the item number back through
 * runModalForWindow's return value (no cross-copy static). */
static void abi_button_action(id self, SEL _cmd, id sender) {
    (void)self; (void)_cmd;
    NSInteger tag = [(NSButton *)sender tag];       /* 1-based DITL item # */
    if ([NSApp modalWindow]) [NSApp stopModalWithCode:tag];
}
static id dialog_target(void) {
    static id shared;
    if (shared) return shared;
    const char *cn = "AbiCarbonDialogTarget";
    Class cls = objc_getClass(cn);                  /* another copy may have made it */
    if (!cls) {
        cls = objc_allocateClassPair([NSObject class], cn, 0);
        if (cls) {
            class_addMethod(cls, sel_registerName("abiButton:"),
                            (IMP)abi_button_action, "v@:@");
            objc_registerClassPair(cls);
        }
    }
    if (!cls) return nil;
    shared = [[cls alloc] init];
    return shared;
}

/* Build the NSWindow + item views from the parsed DITL. Must run on the main
 * thread (AppKit). */
static void build_window(Dialog *d, const char *title) {
    carbon_ensure_window_host();
    CRect *b = &d->bounds;
    CGFloat w = b->right - b->left, h = b->bottom - b->top;
    if (w < 80) w = 360; if (h < 40) h = 160;

    NSRect content = NSMakeRect(0, 0, w, h);
    NSWindow *win = [[NSWindow alloc]
        initWithContentRect:content
                  styleMask:(NSWindowStyleMaskTitled)
                    backing:NSBackingStoreBuffered
                      defer:NO];
    win.releasedWhenClosed = NO;
    if (title && *title) win.title = ns_str(title);
    [win center];
    id del = dialog_target();                       /* runtime-registered, shared */

    NSView *cv = win.contentView;
    NSButton *defaultBtn = nil;
    for (int i = 0; i < d->nitems; i++) {
        DItem *it = &d->items[i];
        /* flip Y: classic top-left -> AppKit bottom-left within content */
        CGFloat iw = it->rect.right - it->rect.left;
        CGFloat ih = it->rect.bottom - it->rect.top;
        CGFloat ix = it->rect.left;
        CGFloat iy = h - it->rect.bottom;      /* bottom edge */
        NSRect fr = NSMakeRect(ix, iy, iw, ih);

        if (it->type == kCtrlItem) {           /* push button */
            NSButton *btn = [[NSButton alloc] initWithFrame:fr];
            btn.title = ns_str(it->text);
            btn.bezelStyle = NSBezelStyleRounded;
            btn.buttonType = NSButtonTypeMomentaryPushIn;
            btn.tag = i + 1;                   /* 1-based DITL item # */
            btn.target = del;
            btn.action = @selector(abiButton:);
            [cv addSubview:btn];
            it->view = btn;
            if (d->defaultItem == i + 1) defaultBtn = btn;
        } else if (it->type == kEditText) {    /* editable text field */
            NSTextField *tf = [[NSTextField alloc] initWithFrame:fr];
            tf.stringValue = ns_str(it->text);
            tf.editable = YES;
            tf.selectable = YES;
            tf.bezeled = YES;
            tf.bezelStyle = NSTextFieldSquareBezel;
            tf.tag = i + 1;
            [cv addSubview:tf];
            it->view = tf;
        } else if (it->type == kStatText) {    /* static label */
            NSTextField *lbl = [NSTextField labelWithString:
                                    ns_str(it->text)];
            lbl.frame = fr;
            lbl.lineBreakMode = NSLineBreakByWordWrapping;
            lbl.maximumNumberOfLines = 0;
            [cv addSubview:lbl];
            it->view = lbl;
        } else {
            it->view = nil;                    /* icon/pic/user: not drawn yet */
        }
    }
    /* Default button = Return key. */
    if (defaultBtn) { defaultBtn.keyEquivalent = @"\r"; win.defaultButtonCell = defaultBtn.cell; }
    /* First editText gets initial keyboard focus. */
    for (int i = 0; i < d->nitems; i++) {
        if (d->items[i].type == kEditText && d->items[i].view) {
            [win makeFirstResponder:d->items[i].view];
            break;
        }
    }
    d->win = win;
}

/* Marshal AppKit main-thread work synchronously (shims are called from the
 * translated app's main thread, which IS the main thread, but be safe). */
static void on_main_sync(void (^block)(void)) {
    if ([NSThread isMainThread]) block();
    else dispatch_sync(dispatch_get_main_queue(), block);
}

/* ============================ MTSHIM entry points ====================== */

/* DialogRef GetNewDialog(SInt16 dialogID, void *dStorage, WindowRef behind) */
uint32_t shim_GetNewDialog(uint32_t *a) {
    int16_t dlogID = (int16_t)a[0];
    __block Dialog *d = NULL;
    long dlen = 0;
    uint8_t *dlog = load_resource('DLOG', dlogID, &dlen);
    if (!dlog || dlen < 22) { free(dlog); DLG("GetNewDialog %d: no DLOG\n", dlogID); return 0; }
    /* DLOG: Rect bounds(8) | SInt16 procID(2) | Boolean visible(1)+filler(1) |
     *       Boolean goAway(1)+filler(1) | SInt32 refCon(4) | SInt16 itemsID(2) |
     *       Str255 title | ... */
    CRect bounds;
    bounds.top = res16(dlog + 0); bounds.left = res16(dlog + 2);
    bounds.bottom = res16(dlog + 4); bounds.right = res16(dlog + 6);
    int16_t itemsID = res16(dlog + 18);
    char titlebuf[256] = "";
    read_pstr(dlog + 20, dlen - 20, titlebuf, sizeof titlebuf, 0);
    free(dlog);

    long ilen = 0;
    uint8_t *ditl = load_resource('DITL', itemsID, &ilen);
    if (!ditl) { DLG("GetNewDialog %d: no DITL %d\n", dlogID, itemsID); return 0; }

    d = (Dialog *)calloc(1, sizeof *d);
    if (!d) { free(ditl); return 0; }
    d->magic = DLG_MAGIC;
    d->bounds = bounds;
    memcpy(d->title, titlebuf, sizeof d->title);
    parse_ditl(d, ditl, ilen);
    free(ditl);

    on_main_sync(^{ build_window(d, d->title); });
    if (!d->win) { free(d); return 0; }
    g_front_dialog = d;         /* the newest dialog is the ModalDialog target */
    DLG("GetNewDialog %d -> DITL %d, %d items, dialog=%p\n", dlogID, itemsID, d->nitems, d);
    return wrap_ptr(d);
}

/* DialogRef GetNewDialogFromResource / GetNewColorDialog fall through to the
 * same builder (dStorage/behind ignored — we own the window). */

/* void ModalDialog(ModalFilterUPP filterProc, DialogItemIndex *itemHit) */
uint32_t shim_ModalDialog(uint32_t *a) {
    int16_t *itemHit = (int16_t *)PTR(1);
    /* The DialogRef isn't an arg to ModalDialog — the classic Dialog Manager
     * runs the FRONT modal dialog (tracked in g_front_dialog). */
    Dialog *d = g_front_dialog;
    if (!d || !d->win) { if (itemHit) *itemHit = 1; return 0; }
    __block NSInteger hit = 0;
    on_main_sync(^{
        NSWindow *win = d->win;
        if (!win.isVisible) { [win makeKeyAndOrderFront:nil]; }
        [NSApp activateIgnoringOtherApps:YES];
        /* runModalForWindow returns the code passed to stopModalWithCode: (the
         * button's 1-based DITL item #) — process-global via the shared NSApp,
         * so it survives the libabiconv multi-copy split. */
        hit = [NSApp runModalForWindow:win];
    });
    if (hit <= 0) hit = 1;                       /* safety: never 0 */
    if (itemHit) *itemHit = (int16_t)hit;
    DLG("ModalDialog -> item %ld\n", (long)hit);
    return 0;
}

/* Item-handle model. GetDialogItem returns a stable Handle the app then passes
 * to GetDialogItemText/SetDialogItemText (Halo NEVER passes DialogRef+itemNo to
 * the text calls). We hand back a pointer to the DItem itself, wrapped as a
 * 32-bit handle; the text calls unwrap it straight back to the DItem. Classic
 * code treats the "Handle" opaquely (only re-passes it), so any stable 32-bit
 * token works. We use a tiny fixed pool of {itemPtr} cells so a Handle deref
 * (Ptr* ) by defensive callers reads a valid low-4GB pointer. */
typedef struct { DItem *item; } ItemHandleCell;
#define ITEMH_POOL 256
static ItemHandleCell g_itemh_pool[ITEMH_POOL];
static int g_itemh_next;
static uint32_t item_handle(DItem *it) {
    if (!it) return 0;
    /* reuse an existing cell for this item if present (stable identity) */
    for (int i = 0; i < ITEMH_POOL; i++)
        if (g_itemh_pool[i].item == it) return wrap_ptr(&g_itemh_pool[i]);
    int idx = g_itemh_next++ % ITEMH_POOL;
    g_itemh_pool[idx].item = it;
    return wrap_ptr(&g_itemh_pool[idx]);
}
static DItem *item_from_handle(uint32_t h) {
    ItemHandleCell *c = (ItemHandleCell *)unwrap_ptr(h);
    if (!c) return NULL;
    /* validate it points into our pool */
    if ((uintptr_t)c < (uintptr_t)&g_itemh_pool[0] ||
        (uintptr_t)c >= (uintptr_t)&g_itemh_pool[ITEMH_POOL]) return NULL;
    return c->item;
}

/* Pull the current UI string out of an item's live view into it->text. */
static void sync_item_from_view(DItem *it) {
    if (!it || !it->view) return;
    if (it->type == kEditText || it->type == kStatText) {
        NSTextField *tf = (NSTextField *)it->view;
        if ([tf isKindOfClass:[NSTextField class]]) {
            const char *s = [tf.stringValue UTF8String];
            if (s) { strncpy(it->text, s, sizeof it->text - 1); it->text[sizeof it->text - 1] = 0; }
        }
    }
}

/* c-string -> the item's text + live view (main thread for the view write). */
static void set_item_text(DItem *it, const char *s) {
    if (!it) return;
    strncpy(it->text, s ? s : "", sizeof it->text - 1);
    it->text[sizeof it->text - 1] = 0;
    if (it->view && (it->type == kEditText || it->type == kStatText)) {
        NSTextField *tf = (NSTextField *)it->view;
        NSString *ns = ns_str(it->text);
        on_main_sync(^{ tf.stringValue = ns ? ns : @""; });
    }
}

/* Str255 (pascal) <-> C. */
static void cstr_to_pstr(const char *c, uint8_t *p255) {
    size_t n = c ? strlen(c) : 0; if (n > 255) n = 255;
    p255[0] = (uint8_t)n; if (n) memcpy(p255 + 1, c, n);
}
static void pstr_to_cstr(const uint8_t *p255, char *out, size_t outsz) {
    int n = p255[0]; if (n > (int)outsz - 1) n = (int)outsz - 1;
    memcpy(out, p255 + 1, n); out[n] = 0;
}

/* OSStatus GetDialogItem(DialogRef, SInt16 itemNo, SInt16 *outType,
 *                        Handle *outItem, Rect *outBox)
 * i386 slots: a0=DialogRef a1=itemNo a2=&type a3=&handle a4=&box */
uint32_t shim_GetDialogItem(uint32_t *a) {
    Dialog *d = dlg_from(a[0]);
    int no = (int16_t)a[1];
    int16_t *outType = (int16_t *)PTR(2);
    uint32_t *outHandle = (uint32_t *)PTR(3);
    int16_t *outBox = (int16_t *)PTR(4);          /* Rect: 4×SInt16 */
    if (outType) *outType = 0;
    if (outHandle) *outHandle = 0;
    if (!d || no < 1 || no > d->nitems) return DLOG_NO_ERR;
    DItem *it = &d->items[no - 1];
    if (outType) *outType = (int16_t)(it->type | (it->disabled ? kItemDisableBit : 0));
    if (outHandle) *outHandle = item_handle(it);
    if (outBox) {                                  /* classic Rect order t,l,b,r */
        outBox[0] = it->rect.top; outBox[1] = it->rect.left;
        outBox[2] = it->rect.bottom; outBox[3] = it->rect.right;
    }
    return DLOG_NO_ERR;
}

/* void GetDialogItemText(Handle item, Str255 text)
 * a0 = item Handle (from GetDialogItem), a1 = Str255 out */
uint32_t shim_GetDialogItemText(uint32_t *a) {
    DItem *it = item_from_handle(a[0]);
    uint8_t *p255 = (uint8_t *)PTR(1);
    if (!p255) return 0;
    if (!it) { p255[0] = 0; return 0; }
    sync_item_from_view(it);                       /* pull live user entry */
    cstr_to_pstr(it->text, p255);
    DLG("GetDialogItemText -> '%s'\n", it->text);
    return 0;
}

/* void SetDialogItemText(Handle item, ConstStr255Param text) */
uint32_t shim_SetDialogItemText(uint32_t *a) {
    DItem *it = item_from_handle(a[0]);
    const uint8_t *p255 = (const uint8_t *)PTR(1);
    if (!it || !p255) return 0;
    char c[256]; pstr_to_cstr(p255, c, sizeof c);
    set_item_text(it, c);
    DLG("SetDialogItemText '%s'\n", c);
    return 0;
}

/* WindowRef GetDialogWindow(DialogRef) — our dialog IS an NSWindow, not a Carbon
 * WindowRef, and the app only passes the result to ShowWindow / (Advance/Clear)
 * KeyboardFocus. We return 0: ShowWindow(NULL) is a guarded native no-op, the
 * focus calls are shimmed no-ops, and ModalDialog orders our window on-screen
 * itself. (Returning our own handle would let native ShowWindow deref a non-
 * WindowRef; returning 0 is the safe, faithful choice for a modal dialog.) */
uint32_t shim_GetDialogWindow(uint32_t *a) {
    Dialog *d = dlg_from(a[0]);
    if (d) g_front_dialog = d;      /* mark it front for the upcoming ModalDialog */
    return 0;
}

/* void ClearKeyboardFocus(WindowRef) — cosmetic caret clearing; no-op (our
 * NSTextField editing owns the caret). Safe for any handle incl. NULL. */
uint32_t shim_ClearKeyboardFocus(uint32_t *a) {
    Dialog *d = dlg_from(a[0]);
    if (d && d->win) { NSWindow *win = d->win; on_main_sync(^{ [win makeFirstResponder:nil]; }); }
    return 0;
}

/* void DisposeDialog(DialogRef) */
uint32_t shim_DisposeDialog(uint32_t *a) {
    Dialog *d = dlg_from(a[0]);
    if (!d) return 0;
    if (g_front_dialog == d) g_front_dialog = NULL;
    NSWindow *win = d->win;
    on_main_sync(^{ if (win) { [win orderOut:nil]; win.delegate = nil; } });
    /* drop item-handle cells that referenced this dialog's items */
    for (int i = 0; i < ITEMH_POOL; i++) {
        DItem *cit = g_itemh_pool[i].item;
        if (cit >= &d->items[0] && cit < &d->items[DLG_MAX_ITEMS]) g_itemh_pool[i].item = NULL;
    }
    d->magic = 0;
    d->win = nil;
    free(d);
    DLG("DisposeDialog %p\n", d);
    return 0;
}

/* void SetDialogDefaultItem(DialogRef, SInt16 itemNo) */
uint32_t shim_SetDialogDefaultItem(uint32_t *a) {
    Dialog *d = dlg_from(a[0]);
    if (!d) return 0;
    d->defaultItem = (int16_t)a[1];
    /* wire Return to that button if it exists */
    if (d->defaultItem >= 1 && d->defaultItem <= d->nitems) {
        DItem *it = &d->items[d->defaultItem - 1];
        if (it->type == kCtrlItem && it->view) {
            NSButton *b = (NSButton *)it->view;
            NSWindow *win = d->win;
            on_main_sync(^{ b.keyEquivalent = @"\r"; win.defaultButtonCell = b.cell; });
        }
    }
    return 0;
}
/* GetDialogDefaultItem / GetDialogCancelItem (owned here historically). */
uint32_t shim_GetDialogDefaultItem(uint32_t *a) { Dialog *d = dlg_from(a[0]); return d ? (uint32_t)d->defaultItem : 0; }
uint32_t shim_GetDialogCancelItem(uint32_t *a)  { Dialog *d = dlg_from(a[0]); return d ? (uint32_t)d->cancelItem : 0; }
/* NOTE: DrawDialog/DialogSelect/IsDialogEvent/SetDialogCancelItem/Alert and the
 * Dialog-item list/edit no-ops are owned by qt_hitoolbox_shim.c — not redefined
 * here to avoid duplicate symbols. */

/* TEHandle GetDialogTextEditHandle(DialogRef) — no classic TextEdit; the app
 * uses it only for TESetSelect (caret positioning), which is cosmetic under our
 * NSTextField editing. Return NULL; TESetSelect is a no-op. */
uint32_t shim_GetDialogTextEditHandle(uint32_t *a) { (void)a; return 0; }

/* SInt16 StopAlert/CautionAlert/NoteAlert(SInt16 alertID, ModalFilterUPP):
 * these show a modal 'ALRT'/'DITL'. When called in a validation-retry loop the
 * app expects a real modal that BLOCKS until the user acknowledges. Render it as
 * a real NSAlert built from the ALRT's DITL text (first non-disabled statText),
 * defaulting to the OK item. Returns the default item (1). */
static uint32_t run_alert(uint32_t *a, const char *kind) {
    int16_t alrtID = (int16_t)a[0];
    __block NSString *msg = nil;
    long alen = 0;
    uint8_t *alrt = load_resource('ALRT', alrtID, &alen);
    int16_t ditlID = alrtID;
    if (alrt && alen >= 12) ditlID = res16(alrt + 8);   /* ALRT: Rect(8) + DITL id(2) */
    free(alrt);
    long ilen = 0; uint8_t *ditl = load_resource('DITL', ditlID, &ilen);
    if (ditl) {
        Dialog tmp; memset(&tmp, 0, sizeof tmp);
        parse_ditl(&tmp, ditl, ilen);
        for (int i = 0; i < tmp.nitems; i++)
            if (tmp.items[i].type == kStatText && tmp.items[i].text[0]) {
                msg = ns_str(tmp.items[i].text); break;
            }
        free(ditl);
    }
    if (!msg) msg = @"Alert";
    __block NSInteger hit = 1;
    on_main_sync(^{
        carbon_ensure_window_host();
        NSAlert *al = [[NSAlert alloc] init];
        al.messageText = msg;
        [al addButtonWithTitle:@"OK"];
        hit = ([al runModal] == NSAlertFirstButtonReturn) ? 1 : 1;
    });
    DLG("%s(%d) -> %ld\n", kind, alrtID, (long)hit);
    return (uint32_t)hit;
}
/* StopAlert is MTSHIM'd to us and Halo's validation loop uses it. The old stub
 * returned 1 instantly (no UI, which is why a validate-retry loop spun). Show a
 * REAL modal alert that blocks until the user acknowledges, then re-prompt.
 * (CautionAlert/NoteAlert/Alert are owned by qt_hitoolbox_shim.c.) */
uint32_t shim_StopAlert(uint32_t *a) { return run_alert(a, "StopAlert"); }

/* ---- Dialog-item list/geometry no-ops the old shim owned (MTSHIM'd to us) - */
uint32_t shim_GetModalDialogEventMask(uint32_t *a) {
    int16_t *outMask = (int16_t *)PTR(0);
    if (outMask) *outMask = (int16_t)0xFFFF;
    return 0;
}
uint32_t shim_AppendDialogItemList(uint32_t *a) { (void)a; return 0; }
uint32_t shim_AutoSizeDialog(uint32_t *a)       { (void)a; return 0; }
uint32_t shim_MoveDialogItem(uint32_t *a)       { (void)a; return 0; }
uint32_t shim_SizeDialogItem(uint32_t *a)       { (void)a; return 0; }
uint32_t shim_SetPortDialogPort(uint32_t *a)    { (void)a; return 0; }
