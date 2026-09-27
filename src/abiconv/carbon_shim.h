/*
 * carbon_shim.h — shared internals for the Carbon/QuickTime reimplementation
 * shims (carbon_memory.c, carbon_component.c, quicktime_image.c).
 *
 * These shims reimplement classic Mac OS / Carbon / QuickTime APIs that are
 * DEAD on modern macOS (the Component Manager, QuickDraw GWorlds, the
 * QuickTime GraphicsImporter, the Memory Manager) on top of the surviving
 * modern frameworks (ImageIO, CoreGraphics). They are reached from translated
 * i386 code through the ___<Name> trampolines in maptable_tramp.asm: each
 * trampoline hands the C impl a pointer to the i386 cdecl arg block (4-byte
 * stack slots) in `a` and returns the impl's uint32_t result in eax.
 *
 * Everything we hand back to the i386 caller (Component/ComponentInstance,
 * Handle/Ptr, GWorldPtr, PixMapHandle, ImageDescriptionHandle) MUST be a
 * 32-bit value, so we allocate it from libabiconv's malloc — whose heap lives
 * entirely below 4 GB (see malloc_shim.c), i.e. always 32-bit representable. No token table is needed: the real pointer round-trips
 * losslessly through the i386 4-byte slot.
 *
 * UNIVERSAL, not iPhoto-specific: any i386 Carbon/QuickTime app that opens a
 * graphics importer, allocates Handles, or draws into a GWorld benefits.
 */
#ifndef ABICONV_CARBON_SHIM_H
#define ABICONV_CARBON_SHIM_H

#include <stdint.h>
#include <stddef.h>

/* ---- classic Mac result/type vocabulary (i386 widths) ------------------ */
typedef int32_t  cm_result;     /* ComponentResult / OSErr-in-a-long */
typedef uint32_t cm_ostype;     /* OSType / FourCharCode (a 4-byte code) */

enum {
   cmNoErr            =     0,   /* noErr */
   cmParamErr         =   -50,   /* paramErr */
   cmMemFullErr       =  -108,   /* memFullErr */
   cmBadComponentType = -2003,   /* badComponentInstance/Selector family */
   cmCantOpenErr      = -2004,
};

/* Well-known component types (FourCC). 'grip' is what iPhoto opens to decode
 * still images; 'grex' exports them. */
#define kFourCC(a,b,c,d) \
   ((cm_ostype)(((uint32_t)(uint8_t)(a) << 24) | ((uint32_t)(uint8_t)(b) << 16) | \
                ((uint32_t)(uint8_t)(c) <<  8) |  (uint32_t)(uint8_t)(d)))

#define kGraphicsImporterType   kFourCC('g','r','i','p')
#define kGraphicsExporterType    kFourCC('g','r','e','x')
#define kSoundDecompressorType   kFourCC('s','d','e','c')

/* ---- i386 <-> native pointer helpers ----------------------------------- */
/* An i386 pointer arg is a low-4GB address valid in this process. */
static inline void *i386_ptr(uint32_t v) { return (void *)(uintptr_t)v; }
/* A native low-4GB pointer narrowed back to the i386 caller's 4-byte slot. */
static inline uint32_t to_i386(const void *p) { return (uint32_t)(uintptr_t)p; }

/* Read/write a 4-byte value through an i386 pointer (NULL-safe write). */
static inline void put_u32(uint32_t i386p, uint32_t val) {
   if (i386p) *(uint32_t *)(uintptr_t)i386p = val;
}
static inline uint32_t get_u32(uint32_t i386p) {
   return i386p ? *(uint32_t *)(uintptr_t)i386p : 0;
}

/* ---- Component Manager backend registration (carbon_component.c) -------- */
/* A live component instance — opaque; the Component Manager owns its layout
 * (validity magic, the subtype it was opened for, live-set linkage). Backends
 * reach their per-instance state through the storage accessors below. */
typedef struct cm_instance cm_instance;

/* A backend's open hook sets its per-instance storage via cm_inst_set_storage;
 * close releases it. Both run on the native side (x86_64). Return cmNoErr or a
 * negative ComponentResult. */
typedef cm_result (*cm_open_fn)(cm_instance *inst);
typedef void      (*cm_close_fn)(cm_instance *inst);

/* Register a component backend at init time (constructor). subtype/manuf may
 * be 0 to match ANY requested subtype/manufacturer. `name` labels it for
 * GetComponentInfo. */
void cm_register_backend(cm_ostype type, cm_ostype subtype, cm_ostype manuf,
                         cm_open_fn open, cm_close_fn close, const char *name);

/* Resolve an i386 ComponentInstance handle to its live instance, or NULL if it
 * is not one of ours (validated against the live set — never derefs a bogus
 * handle). */
cm_instance *cm_inst_from_i386(uint32_t h);

/* Backend per-instance storage + the subtype the instance was opened for. */
void     *cm_inst_storage(cm_instance *inst);
void      cm_inst_set_storage(cm_instance *inst, void *storage);
cm_ostype cm_inst_subtype(cm_instance *inst);

/* Open the default component for (type, subtype); returns the i386
 * ComponentInstance handle (0 on failure). Used by the higher-level openers
 * such as GetGraphicsImporterForDataRef/ForFile. */
uint32_t cm_open_default(cm_ostype type, cm_ostype subtype);

/* ---- Memory Manager (carbon_memory.c), reused by the other shims -------- */
/* Allocate a classic Handle (a 4-byte master pointer -> low-4GB block of
 * `size` bytes). Returns the i386 Handle value, or 0. carbon_memory tracks the
 * exact size for GetHandleSize. */
uint32_t cm_new_handle(uint32_t size, int clear);
/* The block a Handle points at (native pointer) and its size. */
void    *cm_handle_block(uint32_t h);
uint32_t cm_handle_size(uint32_t h);
void     cm_dispose_handle(uint32_t h);
/* Ptr blocks carry the same header as Handle blocks (exact GetPtrSize), so
 * every Ptr alloc/free must go through this pair, never bare malloc/free. */
uint32_t cm_new_ptr(uint32_t size, int clear);
uint32_t cm_ptr_size(uint32_t p);
void     cm_dispose_ptr(uint32_t p);

#endif /* ABICONV_CARBON_SHIM_H */
