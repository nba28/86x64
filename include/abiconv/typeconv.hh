#pragma once

#include <list>
#include <string>
#include <unordered_set>
#include <set>

#include "loc.hh"
#include "util.hh"
#include "typeinfo.hh"

using Symbols = std::unordered_set<std::string>;

struct memloc;

/* Callback-signature registry: fn-ptr parameter bridging. convert_pointer
 * registers the callback's prototype here and emits a runtime call to
 * x64_cb_wrap(fn32, &descriptor), which binds the i386 callback to a native
 * trampoline (cb_bridge.c / cb_tramp.asm). cb_sig_register throws
 * std::invalid_argument for signatures the runtime marshaller can't express
 * (variadic, struct-by-value args, FP returns, no prototype). cb_sig_emit
 * writes the accumulated descriptor blobs; call it once after all headers. */
unsigned cb_sig_register(CXType fnproto_canonical);
void cb_sig_emit(std::ostream& os);

/* true iff the canonical type is a pointer to an opaque `__CF*` record
 * (CFStringRef, CFURLRef, ...) — marshalled via the proxy-handle bridge */
bool cf_opaque_ptr_type(CXType canonical);

/* true iff `orig` (the AS-WRITTEN, un-canonicalized arg type) is a void*-backed
 * CoreFoundation object ref typedef — CFTypeRef / CFPropertyListRef. These
 * canonicalize to `const void *`, indistinguishable from a plain void*, so they
 * must be detected by typedef name before canonicalization. They carry proxy
 * handles that need the same unwrap as the opaque `*Ref` records. */
bool cf_void_ref_type(CXType orig);

/* true iff `written` (the AS-WRITTEN arg/field type) is a classic Mac OS Memory
 * Manager Handle (or the AEDataStorage / opaque-Ptr* family): a typedef whose
 * canonical type is a pointer-to-pointer (T**) AND whose typedef spelling names
 * it a *Handle. Such a value is an OPAQUE token (a pointer to a relocatable
 * master pointer): a marshalling shim must pass the pointer VALUE, NEVER
 * dereference/deep-copy it. Name+structure gated so a genuine `T** out` param
 * (never spelled *Handle) is still deep-copied as a real out-pointer. */
bool is_opaque_handle_type(CXType written);

/* Legacy fixed-32 typedef correction. The LEGACY -arch i386 header parse
 * canonicalizes the fixed-width Mac typedefs (SInt32/UInt32/OSType/OSStatus/
 * OptionBits/Fixed/FourCharCode/...) to `long`/`unsigned long` — 4 bytes on
 * i386 (correct) but 8 on x86_64 per the canonical kind. The NATIVE framework
 * was built from the MODERN headers, where the SAME typedef is `int` = 4
 * bytes. Detected by walking the AS-WRITTEN typedef chain for a name ending
 * in "32" (every fixed-width Mac typedef chains through SInt32/UInt32, incl.
 * Fixed/Fract/UnsignedFixed) with a `long`-family canonical. A pointer-width
 * `long` typedef (CFIndex/NSInteger/Size) never chains through "*32" and
 * keeps its genuine 4->8 widening. INERT in the modern LP64 parse (there the
 * canonical is already Int/UInt). Hoisted from abigen's byval machinery so
 * the by-POINTER record deep-copy applies the same correction (Civ IV
 * CreateStandardAlert: AlertStdCFStringAlertParamRec.version widened to 8 ->
 * every following field shifted +4 -> HIToolbox read the defaultText
 * CFStringRef across the movable/helpButton bytes = 0xffffffffffff0000). */
bool written_is_fixed32_long(CXType written, CXTypeKind canon_kind);

/* The canonical type a field should be MARSHALLED as: substitutes kind
 * Int/UInt for a legacy fixed-32 `long`, else returns `canonical` unchanged.
 * NOTE: the returned CXType carries the ORIGINAL canonical `data` with only
 * `.kind` swapped — valid for this module's kind-switched layout/emission
 * machinery (sizeof_type, alignof_type, get_type_xxx, convert_int), NOT for
 * clang_* layout queries (they would see the original `long`). Scalar-only,
 * so no such query happens downstream. */
CXType effective_field_type(CXType canonical, CXType written);

size_t sizeof_type(CXType type, arch a);
size_t sizeof_type(CXTypeKind type_kind, arch a);

/* compare sizes between architectures */
int sizeof_type_archcmp(CXType type);

size_t alignof_type(CXType type, arch a);

/**
 * Emit code to convert values between architectures at runtime.
 */
void convert_type(std::ostream& os, CXType type, arch a, Location& src, Location& dst,
                  Location& data);

struct record_decl {
   using FieldTypes = std::list<CXType>;

   CXCursor cursor;
   FieldTypes field_types;
   /* The AS-WRITTEN (non-canonicalized) type of each field, in lockstep with
    * field_types. Canonicalization erases the typedef spelling, which some
    * marshalling decisions need — notably recognizing a classic Memory Manager
    * Handle field (AEDesc::dataHandle is AEDataStorage = Ptr* = char**), an
    * OPAQUE token that must be marshalled as a pointer value, not deep-copied
    * (a deep-copy dereferences the handle -> reads garbage). */
   FieldTypes field_types_written;
   bool packed = false;
   bool unsupported = false; /* set if an unexpected member cursor was seen */

   record_decl(CXCursor cursor);
   record_decl(CXType type);

private:
   void populate_fields();
};


class conversion {
public:
   bool allocate; /*!< whether to allocate new data or copy existing */
   arch from_arch;
   arch to_arch;
   MemoryLocation data;
   const Symbols& ignore_structs;

   std::string label() { return std::string(".") + std::to_string(label_++); }

   void convert(std::ostream& os, CXType type, const Location& src, const Location& dst);
   
   void convert_int(std::ostream& os, CXTypeKind type_kind, const Location& src,
                    const Location& dst);
   void convert_real(std::ostream& os, CXTypeKind type_kind, const Location& src,
                     const Location& dst);
   void convert_void_pointer(std::ostream& os, const Location& src,
                             const Location& dst);
   void convert_constant_array(std::ostream& os, CXType array, MemoryLocation src,
                               MemoryLocation dst);
   void convert_pointer(std::ostream& os, CXType pointee, const Location& src, const Location& dst);
   void convert_fnptr(std::ostream& os, CXType pointee, const Location& src, const Location& dst);
   void convert_objc_ptr(std::ostream& os, const Location& src, const Location& dst);
   void convert_cf_ptr(std::ostream& os, const Location& src, const Location& dst);
   void convert_objc_sel(std::ostream& os, const Location& src, const Location& dst);
   void emit_runtime_bridge_call(std::ostream& os, const char *callee,
                                 const std::string& rsi_operand,
                                 const Location& src, reg_width src_width,
                                 const Location& dst, reg_width dst_width);
   void convert_record(std::ostream& os, CXType record, MemoryLocation src, MemoryLocation dst);
   
   conversion(bool allocate, arch from_arch, arch to_arch, const MemoryLocation& data,
              unsigned& label, const Symbols& ignore_structs):
      allocate(allocate), from_arch(from_arch), to_arch(to_arch), data(data),
      ignore_structs(ignore_structs), label_(label) {}
   
   /* Canonical spellings of struct/union pointee types currently being
    * deep-converted on the recursion stack. Used by convert_pointer to break
    * cycles in self-referential types (e.g. linked-list nodes like QElem
    * whose field points back to QElem); without this, deep-copying the
    * pointee recurses forever. Also bounds excessively deep object graphs. */
   std::set<std::string> active_pointees;
   static constexpr size_t max_pointer_depth = 16;

private:
   unsigned& label_;

   void push(std::ostream& os, const RegisterLocation& loc, Location& src, Location& dst);
   void pop(std::ostream& os, const RegisterLocation& loc, Location& src, Location& dst);
   void push(std::ostream& os, const SSELocation& loc, Location& src, Location& dst);
   void pop(std::ostream& os, const SSELocation& loc, Location& src, Location& dst);
};
