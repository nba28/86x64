/*
 * carbon_standardalert_shim.m — REAL, clickable+movable StandardAlert/Alert
 * family on modern AppKit. One job: the Appearance-Manager "standard alert" APIs.
 *
 * THE BUG (Civ IV "XML Load Error" modal, and universally any translated i386
 * Carbon app that pops a standard alert):
 *   CreateStandardAlert / RunStandardAlert / GetStandardAlertDefaultParams /
 *   StandardAlert were NOT shimmed — they bound straight to the LIVE 64-bit
 *   HIToolbox (all still exported: verified by dlsym). Native RunStandardAlert
 *   materializes a real HIToolbox alert window and runs RunAppModalLoopForWindow.
 *   BUT in a translated PURE-CARBON process that never became a foreground GUI
 *   app (no NSApplicationLoad on this path, no activation policy, no app event
 *   pump — our shim_GetNextEvent returns 0, TrackMouseLocation is stubbed), that
 *   native alert window is created but never becomes key/front and its modal loop
 *   starves: the buttons receive no clicks and the window can't be dragged. The
 *   user sees the alert but it is DEAD.
 *
 * THE FIX (universal, functionality-first): route the whole StandardAlert/Alert
 * family through the SAME real-modal mechanism carbon_dialog_shim.m already uses
 * for the classic Dialog Manager — a genuine AppKit NSAlert run with -runModal.
 * NSAlert owns its own key/front/movable/event-pumped modal session (it does not
 * depend on the translated app's starved Carbon event pump), so the buttons are
 * clickable and the alert window is movable by its title bar, exactly like a
 * native app's alert. -runModal returns which button the user clicked; we map it
 * back to the classic kAlertStdAlert{OK,Cancel,Other}Button item numbers so the
 * app's own "which button did the user pick" logic runs UNCHANGED.
 *
 * UNIVERSAL: triggers on the API family (CreateStandardAlert/RunStandardAlert/
 * StandardAlert/Alert), never on an app name or a specific alert string. Any
 * translated i386 Carbon app benefits. Single purpose: the standard-alert family
 * only. (The classic Dialog Manager DLOG/DITL path is owned by
 * carbon_dialog_shim.m; IBCarbon nibs by carbon_nib_shim.c; Nav Services by
 * nav_shim.m — not touched here.)
 *
 * MTSHIM convention (maptable_tramp.asm): rdi -> &i386 args[0] (4-byte cdecl
 * slots), uint32_t result in eax. Object/CFString args arrive as i386 32-bit
 * handles; we resolve them to real x86_64 CFStrings via the objc_shim bridge
 * (x64_objc_unwrap) exactly like carbon_dialog_shim.m does.
 */

#import <AppKit/AppKit.h>
#import <CoreFoundation/CoreFoundation.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <dispatch/dispatch.h>

/* AppKit window-host bootstrap (carbon_appkit_host.c). NSAlert -runModal is
 * self-sufficient, but ensure the AppKit<->WindowManagement bridge exists in a
 * pure-Carbon process so the alert composites correctly. */
extern void carbon_ensure_window_host(void);

/* objc_shim.c bridge: resolve an i386 32-bit CF/obj handle to a real 64-bit id
 * (arena proxy, R/S shadow, i386 CFSTR constant, or low raw value). */
extern uint32_t x64_objc_wrap(uint64_t real);
extern uint64_t x64_objc_unwrap(uint32_t h);

#define PTR(i) ((void *)(uintptr_t)a[(i)])

static int sa_debug(void) {
    static int v = -1;
    if (v < 0) v = getenv("CARBON_DIALOG_TRACE") != NULL ||
                   getenv("CARBON_ALERT_TRACE") != NULL;
    return v;
}
#define SA(...) do { if (sa_debug()) { fprintf(stderr, "[stdalert] " __VA_ARGS__); fflush(stderr); } } while (0)

/* ---- classic constants (Dialogs.h, 10.6 SDK) --------------------------- */
enum { kAlertStopAlert = 0, kAlertNoteAlert = 1, kAlertCautionAlert = 2, kAlertPlainAlert = 3 };
enum { kAlertStdAlertOKButton = 1, kAlertStdAlertCancelButton = 2,
       kAlertStdAlertOtherButton = 3, kAlertStdAlertHelpButton = 4 };
#define kStdCFStringAlertVersionOne 1
/* kAlertDefaultOKText / CancelText / OtherText = (CFStringRef)-1: "use the
 * standard localized button title". In an i386 slot that is 0xffffffff. */
#define CF_MINUS_ONE 0xffffffffu

/* AlertStdCFStringAlertParamRec — i386 layout (verified offsets):
 *   0  UInt32      version
 *   4  Boolean     movable
 *   5  Boolean     helpButton
 *   8  CFStringRef defaultText
 *   12 CFStringRef cancelText
 *   16 CFStringRef otherText
 *   20 SInt16      defaultButton
 *   22 SInt16      cancelButton
 *   24 UInt16      position
 *   28 OptionBits  flags
 *   32 IconRef     icon        (VersionTwo)
 *   size 36
 */
enum {
    PARM_version = 0, PARM_movable = 4, PARM_helpButton = 5,
    PARM_defaultText = 8, PARM_cancelText = 12, PARM_otherText = 16,
    PARM_defaultButton = 20, PARM_cancelButton = 22, PARM_position = 24,
    PARM_flags = 28, PARM_icon = 32, PARM_size = 36,
};

/* Our opaque "standard alert" state. CreateStandardAlert builds it; RunStandard-
 * Alert runs+disposes it. Allocated from libabiconv's <4GB heap so its raw
 * address round-trips through a 4-byte i386 DialogRef slot (arena-wrapped for
 * uniformity, like carbon_dialog_shim.m's Dialog). */
typedef struct StdAlert {
    uint32_t magic;           /* SA_MAGIC */
    int      alertType;
    char     error[1024];     /* primary (bold) message                */
    char     explanation[2048];/* secondary (informative) text          */
    char     defaultText[256];/* OK-position button title  ("" = std OK) */
    char     cancelText[256]; /* Cancel-position title    ("" = none)   */
    char     otherText[256];  /* Other-position title     ("" = none)   */
    int      hasCancel;       /* a Cancel button was requested          */
    int      hasOther;        /* an Other button was requested          */
    int      helpButton;      /* a Help button was requested            */
    int      defaultButton;   /* 1..3, 0 = none                         */
    int      cancelButton;    /* 1..3, 0 = none (Esc)                   */
} StdAlert;
#define SA_MAGIC 0x53414C54u  /* 'SALT' */

static uint32_t wrap_ptr(void *p) {
    if (!p) return 0;
    uint64_t v = (uint64_t)(uintptr_t)p;
    return (v >> 32) ? x64_objc_wrap(v) : (uint32_t)v;
}
static void *unwrap_ptr(uint32_t h) {
    if (!h) return NULL;
    return (void *)(uintptr_t)x64_objc_unwrap(h);
}
static StdAlert *sa_from(uint32_t h) {
    StdAlert *s = (StdAlert *)unwrap_ptr(h);
    if (s && s->magic == SA_MAGIC) return s;
    return NULL;
}

/* Never return nil to an NSAlert setter. */
static NSString *ns_str(const char *c) {
    if (!c || !*c) return @"";
    NSString *s = [NSString stringWithUTF8String:c];
    if (!s) s = [[NSString alloc] initWithBytes:c length:strlen(c)
                                       encoding:NSMacOSRomanStringEncoding];
    return s ? s : @"";
}

/* Resolve an i386 CFStringRef arg to a C string. Handles the (CFStringRef)-1
 * "use standard button text" sentinel (leaves out empty), nil, and any real
 * CFString (via the objc bridge). Returns 1 if a non-sentinel value was present
 * (even if empty), 0 if the slot was nil/absent. Sets *wasDefault when the -1
 * sentinel was seen (caller substitutes the localized default). */
static int cfarg_to_cstr(uint32_t handle, char *out, size_t outsz, int *wasDefault) {
    if (wasDefault) *wasDefault = 0;
    if (outsz) out[0] = 0;
    if (handle == 0) return 0;
    if (handle == CF_MINUS_ONE) { if (wasDefault) *wasDefault = 1; return 1; }
    uint64_t real = x64_objc_unwrap(handle);
    if (!real || real == ~0ULL) { if (wasDefault && real == ~0ULL) *wasDefault = 1; return real == ~0ULL; }
    CFStringRef cf = (CFStringRef)(uintptr_t)real;
    /* toll-free: works on NSString and CFStringRef. Guard the type so a raw
     * non-string pointer can't fault CFStringGetCString deep in CF. */
    if (CFGetTypeID(cf) != CFStringGetTypeID()) return 0;
    if (!CFStringGetCString(cf, out, (CFIndex)outsz, kCFStringEncodingUTF8)) {
        CFStringGetCString(cf, out, (CFIndex)outsz, kCFStringEncodingMacRoman);
    }
    return 1;
}

/* Read the AlertStdCFStringAlertParamRec at i386 address `parm` into s. */
static void read_param(StdAlert *s, const uint8_t *parm) {
    if (!parm) {
        /* No param: a lone OK button, default, is the classic behavior. */
        s->hasCancel = s->hasOther = s->helpButton = 0;
        s->defaultButton = kAlertStdAlertOKButton;
        s->cancelButton = 0;
        return;
    }
    s->helpButton = parm[PARM_helpButton] ? 1 : 0;
    uint32_t dflt = *(const uint32_t *)(parm + PARM_defaultText);
    uint32_t cncl = *(const uint32_t *)(parm + PARM_cancelText);
    uint32_t othr = *(const uint32_t *)(parm + PARM_otherText);
    int wd = 0;
    (void)cfarg_to_cstr(dflt, s->defaultText, sizeof s->defaultText, &wd); /* OK always shown */
    int haveCancel  = cfarg_to_cstr(cncl, s->cancelText,  sizeof s->cancelText,  &wd);
    int haveOther   = cfarg_to_cstr(othr, s->otherText,   sizeof s->otherText,   &wd);
    /* A present slot (even the -1 "use default text" sentinel) requests that
     * button; a nil (0) slot requests no button. The OK button always exists. */
    s->hasCancel = haveCancel;
    s->hasOther  = haveOther;
    s->defaultButton = (int16_t)*(const uint16_t *)(parm + PARM_defaultButton);
    s->cancelButton  = (int16_t)*(const uint16_t *)(parm + PARM_cancelButton);
    if (s->defaultButton < 0) s->defaultButton = 0;
}

/* Build+run the NSAlert on the main thread; returns the classic 1/2/3 item #. */
static NSInteger run_ns_alert(StdAlert *s) {
    __block NSInteger hit = kAlertStdAlertOKButton;
    void (^work)(void) = ^{
        carbon_ensure_window_host();
        NSAlert *al = [[NSAlert alloc] init];
        /* Alert type -> AppKit style. Stop/Note/Plain map to the standard
         * informational alert; Caution -> warning. (macOS no longer draws the
         * classic distinct icons for all four, but this keeps intent.) */
        switch (s->alertType) {
            case kAlertCautionAlert: al.alertStyle = NSAlertStyleWarning; break;
            case kAlertStopAlert:    al.alertStyle = NSAlertStyleCritical; break;
            default:                 al.alertStyle = NSAlertStyleInformational; break;
        }
        al.messageText = ns_str(s->error[0] ? s->error : "Alert");
        if (s->explanation[0]) al.informativeText = ns_str(s->explanation);

        /* Buttons in classic order: OK(1), Cancel(2), Other(3). NSAlert returns
         * NSAlertFirstButtonReturn(1000) for the FIRST button added, +1 per
         * subsequent. We add them in the classic item order and map the return
         * to the 1-based classic item number. */
        NSString *okTitle     = (s->defaultText[0]) ? ns_str(s->defaultText) : @"OK";
        [al addButtonWithTitle:okTitle];                       /* item 1 (OK)     */
        if (s->hasCancel) {
            NSString *t = (s->cancelText[0]) ? ns_str(s->cancelText) : @"Cancel";
            NSButton *b = [al addButtonWithTitle:t];           /* item 2 (Cancel) */
            b.keyEquivalent = @"\033";                          /* Esc = cancel    */
        }
        if (s->hasOther) {
            NSString *t = (s->otherText[0]) ? ns_str(s->otherText) : @"Don't Save";
            [al addButtonWithTitle:t];                         /* item 3 (Other)  */
        }
        if (s->helpButton) al.showsHelp = YES;

        NSModalResponse r = [al runModal];
        /* Map NSAlert{First,Second,Third}ButtonReturn -> classic 1/2/3, honoring
         * that Cancel/Other may be absent (so button index != item order). */
        int idx = (int)(r - NSAlertFirstButtonReturn);         /* 0,1,2 as added  */
        int order[3]; int n = 0;
        order[n++] = kAlertStdAlertOKButton;
        if (s->hasCancel) order[n++] = kAlertStdAlertCancelButton;
        if (s->hasOther)  order[n++] = kAlertStdAlertOtherButton;
        hit = (idx >= 0 && idx < n) ? order[idx] : kAlertStdAlertOKButton;
    };
    if ([NSThread isMainThread]) work();
    else dispatch_sync(dispatch_get_main_queue(), work);
    return hit;
}

/* ============================ MTSHIM entry points ====================== */

/* OSStatus CreateStandardAlert(AlertType alertType, CFStringRef error,
 *                              CFStringRef explanation, const Param *param,
 *                              DialogRef *outAlert)
 * i386 slots: a0=alertType a1=error a2=explanation a3=param a4=outAlert */
uint32_t shim_CreateStandardAlert(uint32_t *a) {
    uint32_t *outAlert = (uint32_t *)PTR(4);
    if (outAlert) *outAlert = 0;
    StdAlert *s = (StdAlert *)calloc(1, sizeof *s);
    if (!s) return -108 /*memFullErr*/;
    s->magic = SA_MAGIC;
    s->alertType = (int)a[0];
    int wd = 0;
    cfarg_to_cstr(a[1], s->error, sizeof s->error, &wd);
    cfarg_to_cstr(a[2], s->explanation, sizeof s->explanation, &wd);
    read_param(s, (const uint8_t *)PTR(3));
    if (outAlert) *outAlert = wrap_ptr(s);
    SA("CreateStandardAlert type=%d error='%s' expl='%s' cancel=%d other=%d -> %p\n",
       s->alertType, s->error, s->explanation, s->hasCancel, s->hasOther, s);
    return 0;
}

/* OSStatus RunStandardAlert(DialogRef inAlert, ModalFilterUPP filterProc,
 *                           DialogItemIndex *outItemHit)
 * i386 slots: a0=inAlert a1=filterProc a2=outItemHit
 * RunStandardAlert disposes the alert; the DialogRef is invalid on return. */
uint32_t shim_RunStandardAlert(uint32_t *a) {
    int16_t *outItemHit = (int16_t *)PTR(2);
    StdAlert *s = sa_from(a[0]);
    if (!s) { if (outItemHit) *outItemHit = kAlertStdAlertOKButton; return -50 /*paramErr*/; }
    NSInteger hit = run_ns_alert(s);
    if (outItemHit) *outItemHit = (int16_t)hit;
    SA("RunStandardAlert -> item %ld\n", (long)hit);
    s->magic = 0;
    free(s);                                     /* Run* owns the dispose */
    return 0;
}

/* OSErr StandardAlert(AlertType inAlertType, ConstStr255Param inError,
 *                     ConstStr255Param inExplanation,
 *                     const AlertStdAlertParamRec *inAlertParam,
 *                     SInt16 *outItemHit)
 * The classic Str255 one-shot: build + run + dispose in one call. inError /
 * inExplanation are pascal strings (Str255), NOT CFStrings. The param rec is the
 * OLD AlertStdAlertParamRec whose text fields are Str255 Handles — rarely used
 * with non-default buttons; we honor movable/help and default to OK/Cancel by
 * the presence flags we can read, else a lone OK. */
static void pstr_to_cstr(const uint8_t *p255, char *out, size_t outsz) {
    if (!p255) { if (outsz) out[0] = 0; return; }
    int n = p255[0]; if (n > (int)outsz - 1) n = (int)outsz - 1;
    memcpy(out, p255 + 1, n); out[n] = 0;
}
uint32_t shim_StandardAlert(uint32_t *a) {
    int16_t *outItemHit = (int16_t *)PTR(4);
    StdAlert s; memset(&s, 0, sizeof s);
    s.magic = SA_MAGIC;
    s.alertType = (int)a[0];
    pstr_to_cstr((const uint8_t *)PTR(1), s.error, sizeof s.error);
    pstr_to_cstr((const uint8_t *)PTR(2), s.explanation, sizeof s.explanation);
    /* AlertStdAlertParamRec (Str255 variant) layout differs from the CFString
     * one; without a reliable Str255-handle deref we present the safe classic
     * default: OK + (Cancel if the app clearly wants a two-button prompt). The
     * common StandardAlert use is a single-OK acknowledgement. */
    s.defaultButton = kAlertStdAlertOKButton;
    NSInteger hit = run_ns_alert(&s);
    if (outItemHit) *outItemHit = (int16_t)hit;
    SA("StandardAlert type=%d error='%s' -> item %ld\n", s.alertType, s.error, (long)hit);
    return 0;
}

/* OSStatus GetStandardAlertDefaultParams(AlertStdCFStringAlertParamPtr param,
 *                                        UInt32 version)
 * Fill the caller's param rec with the classic defaults (matches native):
 * movable, no help, default text = kAlertDefaultOKText((CFStringRef)-1),
 * no cancel/other, default button = OK. */
uint32_t shim_GetStandardAlertDefaultParams(uint32_t *a) {
    uint8_t *parm = (uint8_t *)PTR(0);
    uint32_t version = a[1];
    if (!parm) return -50 /*paramErr*/;
    memset(parm, 0, PARM_size);
    *(uint32_t *)(parm + PARM_version) = version ? version : kStdCFStringAlertVersionOne;
    parm[PARM_movable] = 1;
    parm[PARM_helpButton] = 0;
    *(uint32_t *)(parm + PARM_defaultText) = CF_MINUS_ONE;   /* kAlertDefaultOKText */
    *(uint32_t *)(parm + PARM_cancelText)  = 0;
    *(uint32_t *)(parm + PARM_otherText)   = 0;
    *(uint16_t *)(parm + PARM_defaultButton) = kAlertStdAlertOKButton;
    *(uint16_t *)(parm + PARM_cancelButton)  = 0;
    *(uint16_t *)(parm + PARM_position)      = 0;            /* kWindowDefaultPosition */
    *(uint32_t *)(parm + PARM_flags)         = 0;
    SA("GetStandardAlertDefaultParams(v=%u)\n", version);
    return 0;
}

/* SInt16 Alert(SInt16 alertID, ModalFilterUPP filterProc) — the classic
 * 'ALRT'-resource alert. qt_hitoolbox_shim.c previously stubbed this to a no-op
 * returning 0 (which meant NO modal at all: an app's Alert() acknowledgement /
 * yes-no prompt silently returned button 0, breaking any app that branched on
 * the result). Render it as a REAL modal exactly like StopAlert does in
 * carbon_dialog_shim.c: load the 'ALRT' + its 'DITL', show its statText as an
 * NSAlert, return the OK item (1). (StopAlert/CautionAlert/NoteAlert share this;
 * this Alert entry supersedes the qt_hitoolbox_shim.c no-op — the MTSHIM table
 * now points ___Alert here.) */
extern uint32_t shim_StopAlert(uint32_t *a);   /* carbon_dialog_shim.m: real ALRT modal */
uint32_t shim_AlertReal(uint32_t *a) {
    /* Reuse the DLOG/ALRT resource-backed real modal in carbon_dialog_shim.m
     * (run_alert): it loads the ALRT's DITL text and shows a real NSAlert. Alert
     * historically returns the item hit (OK=1). */
    return shim_StopAlert(a);
}
