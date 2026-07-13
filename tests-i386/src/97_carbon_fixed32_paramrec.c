/*
 * 97_carbon_fixed32_paramrec — guard for the legacy fixed-32 RECORD-LAYOUT
 * marshalling family + the Carbon/CF "-1 sentinel" round-trip.
 *
 * The legacy (-arch i386, 10.6 SDK) abigen pass canonicalizes the fixed-width
 * Mac typedefs (UInt32/SInt32/OSType/OSStatus/OptionBits/...) to `unsigned
 * long`/`long`. Marshalling a struct FIELD of such a type at the canonical
 * native width (8) shifts every following field by +4 versus the REAL native
 * record, which the modern SDK compiled with the same typedef as `int` (4).
 * Live crash this reproduces: Civ IV CreateStandardAlert — the shim staged
 * AlertStdCFStringAlertParamRec.version as 8 bytes, so HIToolbox read the
 * defaultText CFStringRef at its true pack(2) offset 6 across the
 * movable/helpButton bytes = 0xffffffffffff0000 -> __CF_IS_OBJC EXC_BAD_ACCESS.
 * Fix: written_is_fixed32_long()/effective_field_type() applied in
 * record_decl::populate_fields (typeconv.cc), shared with the byval machinery.
 *
 * GetStandardAlertDefaultParams is the pure-data probe of the SAME record in
 * BOTH directions (deep-copy down, out-param copy-back up), headless-safe:
 *  - version must round-trip 4-byte (pre-fix: reads native qword@0 garbage),
 *  - movable/helpButton must be REAL Booleans at native 4/5 (pre-fix: read
 *    from inside defaultText's 0xff bytes -> 0xff),
 *  - defaultText must come back as the kAlertDefaultOKText sentinel
 *    0xffffffff: native fills (CFStringRef)-1, and the copy-back wrap
 *    (x64_objc_wrap) must return the 32-bit sentinel, not mint an arena
 *    handle; the down-direction unwrap (unwrap_obj_arg) widens it to ~0ULL,
 *  - defaultButton==kAlertStdAlertOKButton pins the tail-offset chain
 *    (pre-fix: read from `position`'s slot -> 0),
 *  - icon (kStdCFStringAlertVersionOne leaves it untouched) pins that an
 *    unwritten pointer field round-trips verbatim through the deep-copy.
 *
 * Carbon/HIToolbox isn't in the i386 sysroot, so the import is an undefined
 * dynamic_lookup resolved at translate time by static-interpose ->
 * libabiconv's legacy-pass ___GetStandardAlertDefaultParams shim.
 */

extern int printf(const char *, ...);
extern void exit(int status);

/* i386 (10.6 SDK) AlertStdCFStringAlertParamRec, #pragma pack(2): 32 bytes. */
#pragma pack(push, 2)
typedef struct {
   unsigned long  version;        /* UInt32   @0  */
   unsigned char  movable;        /* Boolean  @4  */
   unsigned char  helpButton;     /* Boolean  @5  */
   const void    *defaultText;    /* CFStringRef @6  */
   const void    *cancelText;     /* CFStringRef @10 */
   const void    *otherText;      /* CFStringRef @14 */
   short          defaultButton;  /* SInt16   @18 */
   short          cancelButton;   /* SInt16   @20 */
   unsigned short position;       /* UInt16   @22 */
   unsigned long  flags;          /* OptionBits @24 */
   const void    *icon;           /* IconRef  @28 */
} AlertParamRec32;                /* sizeof == 32 */
#pragma pack(pop)

extern short GetStandardAlertDefaultParams(AlertParamRec32 *param,
                                           unsigned long version);

int main(void)
{
   AlertParamRec32 r;
   unsigned char *p = (unsigned char *)&r;
   unsigned i;
   for (i = 0; i < sizeof r; ++i) { p[i] = 0xAA; }

   short err = GetStandardAlertDefaultParams(&r, 1 /* kStdCFStringAlertVersionOne */);

   printf("size=%u\n", (unsigned)sizeof r);
   printf("err=%d version=%lu movable=%u help=%u\n",
          (int)err, r.version, (unsigned)r.movable, (unsigned)r.helpButton);
   printf("defText=0x%lx cancelText=0x%lx otherText=0x%lx\n",
          (unsigned long)r.defaultText, (unsigned long)r.cancelText,
          (unsigned long)r.otherText);
   printf("defBtn=%d cancelBtn=%d pos=%u flags=%lu\n",
          (int)r.defaultButton, (int)r.cancelButton,
          (unsigned)r.position, r.flags);
   printf("icon=0x%lx\n", (unsigned long)r.icon);
   /* -e _main entry (no crt0): returning would ret into garbage; exit like
    * the other dynamic_lookup guards (79/85/95). */
   exit(0);
}
