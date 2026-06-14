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
