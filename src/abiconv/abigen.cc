#include <cassert>
#include <iostream>
#include <list>
#include <vector>
#include <fstream>
#include <sstream>
#include <getopt.h>
#include <cstdlib>
#include <algorithm>
#include <clang-c/Index.h>

#include "emit.hh"
#include "util.hh"
#include "abigen.hh"
#include "typeinfo.hh"
#include "typeconv.hh"

bool force_all = false;

/* Generation-time gate (mirrors the translator's MACHO_NULL_TRAP): when
 * ABICONV_GEN_NATIVE_CRUMB is set in abigen's env, every generated shim records
 * a "last native callee entered" breadcrumb (shim addr + i386 caller RA) so a
 * crash handler can name the native function a faulting thread was inside — the
 * durable answer to "native code through 0, low-4GB stack unwalkable" (iPhoto)
 * and a native-call trace for Portal 2's silent launcher exit. Default build is
 * byte-identical (emits nothing). Toggling requires regenerating abiconv.asm. */
static bool gen_native_crumb() {
   static int v = -1;
   if (v < 0) v = std::getenv("ABICONV_GEN_NATIVE_CRUMB") ? 1 : 0;
   return v != 0;
}

template <typename RegIt>
struct param_info {
   RegIt reg_it;
   const RegIt reg_end;
   unsigned fp_idx = 0;
   const unsigned fp_end;

   param_info(RegIt reg_begin, RegIt reg_end, unsigned fp_count):
      reg_it(reg_begin), reg_end(reg_end), fp_idx(0), fp_end(fp_count) {}
};

struct ABIConversion {
   using Cursors = std::list<CXCursor>;
   
   std::string sym;
   CXType function_type;

   static constexpr unsigned max_reg_args = 6;
   static constexpr unsigned max_xmm_args = 8;

   using Regs = std::list<const reg_group *>;
   Regs regs;

   virtual void emit_call(std::ostream& os) const = 0;

   ABIConversion(CXCursor function_decl, const Regs& regs):
      function_type(clang_getCursorType(function_decl)), regs(regs) {
      auto cxsym = clang_getCursorSpelling(function_decl);
      sym = std::string("_") + clang_getCString(cxsym);
      clang_disposeString(cxsym);
      assert(function_type.kind == CXType_FunctionProto);
   }

   ABIConversion(CXType function_type, const std::string& sym, const Regs& regs):
      sym(sym), function_type(function_type), regs(regs) {}

   virtual ~ABIConversion() {}

   /* The name the shim FORWARDS TO natively. Callers bind to the exported
    * global <prefix>sym (which keeps its suffix so classic/modern binds still
    * resolve), but the inner extern/call must target a symbol that STILL EXISTS
    * in modern libSystem. The legacy 10.5-era POSIX conformance-variant aliases
    * (`_usleep$UNIX2003`, `_write$UNIX2003`, `_foo$UNIX2003$NOCANCEL`, ...) were
    * REMOVED from modern macOS: dyld leaves their dynamic-lookup slot NULL, so a
    * shim that inner-calls `<name>$UNIX2003` does `jmpq *NULL` -> rip=0 on first
    * use (Portal 2's libtier0 CalculateCPUFreq -> usleep was the first hit). The
    * bare modern symbol is the conforming one, so strip the dead suffix for the
    * forward target only. Same policy as 86x64.sh's strip-bind,suffix=$UNIX2003
    * (which strips it from TRANSLATED targets' binds); this closes the gap for
    * libabiconv's OWN internal forward. UNIVERSAL: fires on the structural
    * `$UNIX2003`/`$NOCANCEL` conformance suffix, not any app or symbol. */
   std::string native_target() const {
      std::string t = sym;
      /* Repeatedly peel any trailing conformance modifier so a compound
       * `$UNIX2003$NOCANCEL` (either order) collapses to the bare name. */
      bool changed = true;
      while (changed) {
         changed = false;
         for (const std::string suffix : {std::string("$UNIX2003"),
                                          std::string("$NOCANCEL")}) {
            if (t.size() > suffix.size() &&
                t.compare(t.size() - suffix.size(), suffix.size(), suffix) == 0) {
               t.resize(t.size() - suffix.size());
               changed = true;
            }
         }
      }
      return t;
   }

   /* True for conversions whose inner forward is a `call <native_target>` by
    * NAME (FunctionConversion). Only those can be captured by a sibling shim
    * definition; SyscallConversion forwards via `syscall`. */
   virtual bool calls_native() const { return false; }

   /* SHIM-CAPTURE (double-wrap) protection — the Civ IV tolower-family root
    * cause. Every shim is EXPORTED as override_prefix+S = "__"+S, and every
    * consider-set symbol S starts with "_", so the universe of shim names is
    * exactly the names with >= 3 leading underscores. When a shim's inner
    * native_target() itself has >= 3 leading underscores (libc's internal
    * underscore twins: ___tolower/___toupper/___maskrune/___error/...), the
    * name can collide with a DEFINED sibling shim in the final link — from
    * THIS abigen pass, the OTHER abigen pass (modern vs legacy: neither sees
    * the other's shim names, and the tolower capture was exactly cross-pass),
    * or a hand-written "__"-prefixed .asm shim. ld resolves the inner call to
    * that in-image definition instead of the real native function; the sibling
    * shim then re-runs its i386 arg marshalling on a NATIVE 8-byte call frame
    * ([rbp+0xc] = high half of the return address = 0), so e.g. tolower
    * returns 0 for every char.
    *
    * For this structural class the inner call is routed through a data slot
    * (`call qword [rel <slot>]`) statically initialized `dq <target>` — i.e.
    * bound at link time to the SAME definition the direct call would have
    * used, so pre-repair behavior is exactly the old behavior. At load,
    * nslot_repair.c walks the slot table and re-points only slots whose
    * current value structurally fingerprints as a marshalling shim (the
    * `cmp qword [rel __dyld_stub_binder_flag], 0` prologue) at the real
    * native definition (libSystem/CoreFoundation/dlsym, never a libabiconv
    * image). Intentional native impls in libabiconv (e.g. file_shim.c's
    * __srget, which generated shims inner-call ON PURPOSE) don't fingerprint
    * as shims and are left untouched. UNIVERSAL: triggers on the structural
    * ">= 3 leading underscores" name class, not on any symbol list, so it is
    * immune to consider-set growth (the 6369a4a closure expansion) and needs
    * no cross-pass name plumbing. */
   bool needs_native_slot() const {
      if (!calls_native()) { return false; }
      const std::string t = native_target();
      return t.size() >= 3 && t.compare(0, 3, "___") == 0;
   }

   std::string native_slot_label() const {
      return "__abicnslot" + native_target();
   }

   /* Emit the slot backing needs_native_slot() call routing, once per distinct
    * target per output file (dedup via `emitted`; label is file-local so the
    * two abigen passes never collide at link — each file repairs its own).
    * Called only after the shim body was successfully flushed, so a discarded
    * shim never leaves a dangling slot reference, and vice versa NASM resolves
    * the body's forward reference to this label in its second pass. Appends
    * the (name, slot) pair to `order` for the emit_native_slot_table footer. */
   void emit_native_slot(std::ostream& os, Symbols& emitted,
                         std::vector<std::string>& order) const {
      if (!needs_native_slot()) { return; }
      const std::string t = native_target();
      if (!emitted.insert(t).second) { return; }
      order.push_back(t);
      os << "\tsection .data" << std::endl;
      os << "\talign 8" << std::endl;
      os << native_slot_label() << ":" << std::endl;
      os << "\tdq\t" << t << std::endl;
      os << "\tsection .text" << std::endl;
   }

   static CXType handle_type(CXType type) {
      return clang_getCanonicalType(type);
   }

   unsigned argc() const {
      const int tmp = clang_getNumArgTypes(function_type);
      assert(tmp >= 0);
      return tmp;
   }

   static CXTypeKind get_arg_kind(CXTypeKind kind) {
      switch (kind) {
      case CXType_ConstantArray: return CXType_Pointer;
      default: return kind;
      }
   }

   /* A struct passed BY VALUE as an argument. The i386 cdecl ABI pushes it onto
    * the stack as raw contiguous bytes; the x86_64 SysV ABI classifies it into
    * eightbytes (an INTEGER eightbyte -> a GP register, in arg order). Field
    * widths differ between the ABIs (long: 4 vs 8 bytes), so the struct can't be
    * byte-copied: each field is read from its i386 offset and WIDENED into its
    * own x86_64 eightbyte register.
    *
    * Handled (the common + tractable case): a struct of 1-2 plain-`long`-width
    * integer fields, each 4 bytes on i386 and 8 on x86_64, each its own INTEGER
    * eightbyte. That is CFRange{CFIndex,CFIndex}, NSRange{NSUInteger,NSUInteger}
    * and any {long,long}/{ulong,ulong} struct — the range/index-struct family CF
    * and Foundation pass by value (CFStringGetCharacters, CFStringGetBytes,
    * CFDataGetBytes, CFArrayGetValues, -getCharacters:range:, ...). Before this,
    * abigen SKIPPED every such function (get_type_domain threw on CXType_Record),
    * so its bind stayed on the NATIVE callee and the i386-cdecl call reached it
    * with the wrong registers — e.g. CFStringGetCharacters got a 32-bit proxy
    * handle in rdi, native CF then msgSend'd the held string as that object's
    * isa, and libobjc SIGBUS'd realizing a CFString constant as a class.
    *
    * Anything else (FP/SSE fields, sub-eightbyte int packing, pointer/objc fields,
    * `long long`, >2 fields, padding, unions, packed) throws -> abigen skips that
    * one function exactly as before. Returns the ordered field types. */
   static record_decl::FieldTypes integer_byval_struct_fields(CXType record) {
      record_decl decl(record);
      if (decl.cursor.kind != CXCursor_StructDecl || decl.packed) {
         throw std::invalid_argument("byval struct: union/packed not supported");
      }
      const size_t n = decl.field_types.size();
      if (n < 1 || n > 2) {   /* >16 bytes is never register-passed (SysV) */
         throw std::invalid_argument("byval struct: field count not in 1..2");
      }
      for (CXType f : decl.field_types) {
         CXType cf = clang_getCanonicalType(f);
         switch (cf.kind) {
         case CXType_Long: case CXType_ULong: break;  /* CFIndex / NS[U]Integer */
         default:
            throw std::invalid_argument("byval struct: non-long field");
         }
         if (sizeof_type(cf, arch::i386) != 4 || sizeof_type(cf, arch::x86_64) != 8) {
            throw std::invalid_argument("byval struct: field not 4/8 (i386/x86_64)");
         }
      }
      /* natural contiguous layout, no padding/alignment surprises */
      if (sizeof_type(record, arch::i386) != 4 * n ||
          sizeof_type(record, arch::x86_64) != 8 * n) {
         throw std::invalid_argument("byval struct: unexpected padding");
      }
      return decl.field_types;
   }

   /* Detect a CGFloat field. The modern abigen pass parses headers as the host
    * (x86_64), where `CGFloat` is a typedef for `double` and so canonicalizes to
    * `double` — indistinguishable from a genuine `double` after canonicalization.
    * But in the original i386 binary CGFloat was `float` (4 bytes). Detect it by
    * the AS-WRITTEN typedef name on the (non-canonical) field type, so the field
    * is read as a 4-byte float on the i386 side and widened to a double. */
   static bool field_is_cgfloat(CXType field_type /* non-canonical */) {
      CXString s = clang_getTypeSpelling(field_type);
      const bool cg =
         std::string(clang_getCString(s)).find("CGFloat") != std::string::npos;
      clang_disposeString(s);
      return cg;
   }

   /* Recursively flatten a homogeneous floating-point by-value struct into one
    * `widen` flag per scalar field: true = an i386 4-byte float widened to an
    * x86_64 8-byte double (a CGFloat); false = a genuine `double` (8 bytes on
    * both ABIs). Recurses into nested structs (CGRect = {CGPoint, CGSize}). Sets
    * `bad` (it never throws — this runs inside the libclang visitor callback) on
    * any member that is not such an FP field. */
   static void flatten_fp_fields(CXType record, std::vector<bool>& widen, bool& bad) {
      CXCursor decl = clang_getTypeDeclaration(record);
      CXCursor def = clang_getCursorDefinition(decl);
      if (!clang_Cursor_isNull(def)) { decl = def; }
      if (clang_getCursorKind(decl) != CXCursor_StructDecl) { bad = true; return; }
      for_each(decl, [&] (CXCursor c, CXCursor) {
         if (bad) { return CXChildVisit_Break; }
         const CXCursorKind k = clang_getCursorKind(c);
         if (k == CXCursor_FieldDecl) {
            CXType ft = clang_getCursorType(c);              /* non-canonical */
            CXType cft = clang_getCanonicalType(ft);
            switch (cft.kind) {
            case CXType_Record:
               flatten_fp_fields(cft, widen, bad);
               break;
            case CXType_Double:                 /* CGFloat or genuine double */
               widen.push_back(field_is_cgfloat(ft));
               break;
            default:
               /* a genuine 4-byte float (two pack per eightbyte), or any int/
                * pointer/bitfield member: not this homogeneous-8B-eightbyte path */
               bad = true;
               break;
            }
            if (bad) { return CXChildVisit_Break; }
         } else if (k == CXCursor_StructDecl || k == CXCursor_UnionDecl ||
                    clang_isAttribute(k)) {
            /* nested record type declaration / attribute: no layout impact */
         } else {
            bad = true;
            return CXChildVisit_Break;
         }
         return CXChildVisit_Continue;
      });
   }

   /* A by-value struct of homogeneous 8-byte FP eightbytes — the CG/NS geometry
    * family: CGPoint/CGSize {2}, CGRect/NSRect {4}, CGAffineTransform {6}. Each
    * flattened field is one SSE eightbyte on x86_64. SysV classifies a <=16-byte
    * (<=2 field) struct into SSE registers and a >16-byte one into MEMORY (the
    * stack). The i386 caller passed the same struct with 4-byte CGFloat fields,
    * so each field is read at its i386 offset and widened to a double. Returns
    * the per-field widen flags; throws (caller skips the function) for anything
    * else, leaving the previously-skipped set otherwise unchanged. */
   static std::vector<bool> fp_byval_struct_fields(CXType record) {
      std::vector<bool> widen;
      bool bad = false;
      flatten_fp_fields(clang_getCanonicalType(record), widen, bad);
      if (bad || widen.empty()) {
         throw std::invalid_argument("byval struct: not a homogeneous FP struct");
      }
      /* every field is exactly one 8-byte eightbyte on x86_64 (no padding/holes) */
      if (sizeof_type(record, arch::x86_64) != 8 * widen.size()) {
         throw std::invalid_argument("byval struct: unexpected FP layout/padding");
      }
      return widen;
   }

   /* ---- generalized SysV struct-by-value ARG marshalling ----
    *
    * The old arg path handled exactly two by-value shapes: 1-2 plain-`long`
    * fields (CFRange/NSRange) and homogeneous-FP CGFloat/double structs (the
    * CG geometry family). EVERYTHING else threw -> the whole function was
    * skipped -> unshimmed -> the i386 cdecl call reached the native callee raw
    * (over-pop / garbage regs). That skipped the classic Carbon Point-by-value
    * family (DragWindow, PtInRgn, FindControlUnderMouse, HandleControlClick,
    * TEClick, ContextualMenuSelect, ...), `struct in_addr` (inet_ntoa),
    * CFUUIDBytes (16 UInt8s), CFGregorianDate (mixed int+double), and
    * NSCreateMapTableWithZone's callback-struct args.
    *
    * General scheme (SysV x86_64 classification, structural — no name checks):
    *   1. classify the record's x86_64 eightbytes: an eightbyte is SSE iff
    *      every field byte overlapping it is float/double, else INTEGER.
    *      <=16 bytes -> register candidate (all-or-nothing); >16 -> MEMORY.
    *   2. stage the x86_64 image of the struct in a scratch slot inside the
    *      shim frame, converting FIELD BY FIELD from the i386 layout (long
    *      4->8 widen, CGFloat float->double widen, alignment re-padding).
    *      Pointer fields are passed as VALUES (an i386 pointer is low-4GB and
    *      hence a valid native pointer; deep-copying would break identity),
    *      fn-ptr fields are bound to native callback trampolines, objc/CF-ref
    *      fields go through the proxy-handle bridge.
    *   3. load each eightbyte into the next GP/XMM arg register, or copy the
    *      staged image to the outgoing stack area (MEMORY / regs exhausted).
    * No post-call copy-back (the arg is by value). */

   struct byval_field { CXType written; CXType canon; };

   static void byval_collect_fields(CXType record_canon, std::vector<byval_field>& out) {
      clang_Type_visitFields(
         record_canon,
         [](CXCursor field, CXClientData data) -> CXVisitorResult {
            auto *v = static_cast<std::vector<byval_field> *>(data);
            CXType w = clang_getCursorType(field);
            v->push_back({w, clang_getCanonicalType(w)});
            return CXVisit_Continue;
         },
         &out);
   }

   struct byval_plan {
      size_t sz32 = 0, sz64 = 0;
      /* per x86_64 eightbyte: {has_int, has_fp} -> SSE iff fp && !int */
      std::vector<bool> eb_int, eb_fp;
      unsigned ebs() const { return eb_int.size(); }
      bool eb_sse(unsigned k) const { return eb_fp[k] && !eb_int[k]; }
      unsigned n_int() const { unsigned n = 0; for (unsigned k = 0; k < ebs(); ++k) if (!eb_sse(k)) ++n; return n; }
      unsigned n_sse() const { unsigned n = 0; for (unsigned k = 0; k < ebs(); ++k) if (eb_sse(k)) ++n; return n; }
   };

   static bool byval_field_is_cgfloat(const byval_field& f) {
      return field_is_cgfloat(f.written) &&
             (f.canon.kind == CXType_Double || f.canon.kind == CXType_Float);
   }

   /* Detect a fixed-width 32-bit Mac typedef (SInt32/UInt32/OSType/FourCharCode/
    * OSStatus/DescType/...) that the LEGACY -arch i386 header parse canonicalizes
    * to `long`/`unsigned long`. On i386 that `long` is 4 bytes (correct), but the
    * NATIVE x86_64 framework was built from the MODERN headers where the SAME
    * typedef is `int` = 4 bytes — NOT the 8-byte `long` the canonical implies. So a
    * by-value struct carrying such a field (HIViewID/ControlID {OSType;SInt32},
    * EventTypeSpec {OSType;UInt32}, ...) is over-sized to 16 bytes and rides TWO GP
    * regs instead of ONE, shifting the following arg (Civ's HIViewFindByID wall:
    * inID's second half landed in rdx, displacing outView). Detected by walking the
    * AS-WRITTEN typedef chain for a name ending in "32" (every one is built on
    * SInt32/UInt32) with a `long`/`unsigned long` canonical. INERT in the modern
    * pass (there the canonical is Int/UInt, already the correct 4 bytes); a
    * pointer-width `long` typedef (CFIndex/NSInteger) never ends in "32", so
    * CFRange/NSRange keep 8 bytes per field. Structural + name gated, never a name. */
   static bool byval_field_is_int32(const byval_field& f) {
      /* Hoisted to typeconv (written_is_fixed32_long) so record_decl's
       * by-POINTER deep-copy path applies the identical correction; this
       * wrapper keeps the byval machinery reading naturally. */
      return written_is_fixed32_long(f.written, f.canon.kind);
   }

   /* x86_64 SIZE / ALIGN of a by-value field, correcting a legacy fixed-32 typedef
    * (byval_field_is_int32) back to its true native 4-byte width; recurses for
    * nested records / arrays. Used everywhere the byval machinery computes the
    * x86_64 layout, so a fixed-32 field consumes 4 bytes, not the canonical 8. */
   /* Element of an ARRAY field, keeping the WRITTEN typedef sugar when the
    * as-written type is itself an array: Fixed[3][3]'s element must stay
    * recognizable as the fixed-32 typedef `Fixed`, or the canonical `long`
    * element would widen to 8 native bytes. */
   static byval_field byval_array_elem(const byval_field& f) {
      const CXType wa =
         (f.written.kind == CXType_ConstantArray) ? f.written : f.canon;
      const CXType we = clang_getArrayElementType(wa);
      return {we, clang_getCanonicalType(we)};
   }

   static size_t byval_field_x64_size(const byval_field& f) {
      if (byval_field_is_int32(f)) { return 4; }
      const CXType c = f.canon;
      if (c.kind == CXType_Record) { return byval_x64_sizeof(c); }
      if (c.kind == CXType_ConstantArray) {
         return static_cast<size_t>(clang_getArraySize(c)) *
                byval_field_x64_size(byval_array_elem(f));
      }
      return sizeof_type(c, arch::x86_64);
   }
   static size_t byval_field_x64_align(const byval_field& f) {
      if (byval_field_is_int32(f)) { return 4; }
      const CXType c = f.canon;
      if (c.kind == CXType_Record) {
         std::vector<byval_field> fs;
         byval_collect_fields(c, fs);
         size_t a = 1;
         for (const byval_field& g : fs) { a = std::max(a, byval_field_x64_align(g)); }
         return a;
      }
      if (c.kind == CXType_ConstantArray) {
         return byval_field_x64_align(byval_array_elem(f));
      }
      return alignof_type(c, arch::x86_64);
   }
   static size_t byval_x64_sizeof(CXType record_canon) {
      std::vector<byval_field> fs;
      byval_collect_fields(record_canon, fs);
      size_t off = 0, salign = 1;
      for (const byval_field& f : fs) {
         const size_t a = byval_field_x64_align(f);
         off = align_up(off, a);
         salign = std::max(salign, a);
         off += byval_field_x64_size(f);
      }
      return align_up(off, salign);
   }

   /* ---- i386 twins of the byval_field_x64_* layout helpers ----
    * Same fixed-32-typedef correction, but i386 widths (long/pointer = 4) and
    * the Darwin i386 rule that no struct MEMBER demands more than 4-byte
    * alignment (a `double` field sits at a 4-aligned offset; alignof_record
    * already encodes this for nested records — mirror it for bare scalars).
    * Used only by byval_layout_identical() below. */
   static size_t byval_field_i386_size(const byval_field& f) {
      if (byval_field_is_int32(f)) { return 4; }
      const CXType c = f.canon;
      if (c.kind == CXType_Record) { return sizeof_type(c, arch::i386); }
      if (c.kind == CXType_ConstantArray) {
         return static_cast<size_t>(clang_getArraySize(c)) *
                byval_field_i386_size(byval_array_elem(f));
      }
      return sizeof_type(c, arch::i386);
   }
   static size_t byval_field_i386_align(const byval_field& f) {
      if (byval_field_is_int32(f)) { return 4; }
      const CXType c = f.canon;
      if (c.kind == CXType_Record) {
         std::vector<byval_field> fs;
         byval_collect_fields(c, fs);
         size_t a = 1;
         for (const byval_field& g : fs) { a = std::max(a, byval_field_i386_align(g)); }
         return a;
      }
      if (c.kind == CXType_ConstantArray) {
         return byval_field_i386_align(byval_array_elem(f));
      }
      return std::min<size_t>(alignof_type(c, arch::i386), 4);
   }

   static size_t byval_field_size(const byval_field& f, arch a) {
      return a == arch::i386 ? byval_field_i386_size(f) : byval_field_x64_size(f);
   }
   static size_t byval_field_align(const byval_field& f, arch a) {
      return a == arch::i386 ? byval_field_i386_align(f) : byval_field_x64_align(f);
   }

   /* Flatten a record to its scalar leaves as (offset, size) pairs under `a`'s
    * layout rules. `bad` is set for any field the comparison cannot model
    * faithfully — most importantly a CGFloat, whose canonical type under the
    * modern parse is `double` (8 bytes) while the real i386 field is a 4-byte
    * float, so a naive i386 walk would silently report the wrong offsets. */
   static void byval_collect_leaves(CXType record_canon, size_t base, arch a,
                                    std::vector<std::pair<size_t, size_t>>& out,
                                    bool& bad) {
      std::vector<byval_field> fs;
      byval_collect_fields(record_canon, fs);
      size_t off = 0;
      for (const byval_field& f : fs) {
         if (byval_field_is_cgfloat(f)) { bad = true; return; }
         off = align_up(off, byval_field_align(f, a));
         const size_t sz = byval_field_size(f, a);
         const CXType c = f.canon;
         if (!byval_field_is_int32(f) && c.kind == CXType_Record) {
            byval_collect_leaves(c, base + off, a, out, bad);
            if (bad) { return; }
         } else if (!byval_field_is_int32(f) && c.kind == CXType_ConstantArray) {
            const byval_field ef = byval_array_elem(f);
            if (byval_field_is_cgfloat(ef)) { bad = true; return; }
            const size_t esz = byval_field_size(ef, a);
            const long long n = clang_getArraySize(c);
            for (long long i = 0; i < n; ++i) {
               if (ef.canon.kind == CXType_Record) {
                  byval_collect_leaves(ef.canon, base + off + i * esz, a, out, bad);
                  if (bad) { return; }
               } else {
                  out.emplace_back(base + off + i * esz, esz);
               }
            }
         } else {
            out.emplace_back(base + off, sz);
         }
         off += sz;
      }
   }

   /* True iff `record_canon` has BYTE-IDENTICAL i386 and x86_64 layouts: every
    * scalar leaf lands at the same offset with the same width, and the total
    * sizes agree. This is the precondition for copying an x86_64 return image
    * VERBATIM into an i386 caller's struct buffer (mem32_reg64_return below).
    * Declining is always safe — the caller falls back to the pre-existing
    * behaviour. Structural: no name is ever consulted. */
   static bool byval_layout_identical(CXType record_canon) {
      std::vector<std::pair<size_t, size_t>> l32, l64;
      bool bad = false;
      try {
         byval_collect_leaves(record_canon, 0, arch::i386, l32, bad);
         if (bad) { return false; }
         byval_collect_leaves(record_canon, 0, arch::x86_64, l64, bad);
         if (bad) { return false; }
      } catch (const std::exception&) {
         return false;
      }
      if (l32.empty() || l32 != l64) { return false; }
      return sizeof_type(record_canon, arch::i386) == byval_x64_sizeof(record_canon);
   }

   static void byval_mark(byval_plan& plan, size_t off, size_t size, bool fp) {
      for (size_t k = off / 8; k <= (off + size - 1) / 8; ++k) {
         if (k >= plan.eb_int.size()) { plan.eb_int.resize(k + 1); plan.eb_fp.resize(k + 1); }
         if (fp) plan.eb_fp[k] = true; else plan.eb_int[k] = true;
      }
   }

   /* classify one (possibly nested) record at x86_64 offset base64. Throws on
    * any shape the flat converter can't marshal -> caller skips the function
    * (exactly the pre-existing failure mode, never a silent wrong emit). */
   static void byval_classify_walk(CXType record_canon, size_t base64, byval_plan& plan) {
      record_decl decl(record_canon);           /* throws on unsupported members */
      if (decl.cursor.kind != CXCursor_StructDecl) {
         throw std::invalid_argument("byval struct: union not supported");
      }
      if (decl.packed) {
         throw std::invalid_argument("byval struct: packed not supported");
      }
      std::vector<byval_field> fields;
      byval_collect_fields(record_canon, fields);
      size_t off = 0;
      for (const byval_field& f : fields) {
         off = align_up(off, byval_field_x64_align(f));
         byval_classify_leaf(f, base64 + off, plan);
         off += byval_field_x64_size(f);
      }
   }

   static void byval_classify_leaf(const byval_field& f, size_t off64, byval_plan& plan) {
      const CXType c = f.canon;
      if (byval_field_is_cgfloat(f)) {
         if (c.kind == CXType_Float) {
            /* legacy (-arch i386) parse: CGFloat canonicalizes to float, so the
             * x86_64 field size/offsets computed from the canon type would be
             * WRONG (native CGFloat is double). Skip — same as before. */
            throw std::invalid_argument("byval struct: CGFloat under i386 parse");
         }
         byval_mark(plan, off64, 8, true);
         return;
      }
      switch (c.kind) {
      case CXType_Float:
      case CXType_Double:
         byval_mark(plan, off64, sizeof_type(c, arch::x86_64), true);
         return;
      case CXType_Bool:
      case CXType_UChar: case CXType_Char_U:
      case CXType_SChar: case CXType_Char_S:
      case CXType_UShort: case CXType_Short:
      case CXType_UInt:  case CXType_Int:  case CXType_Enum:
      case CXType_ULong: case CXType_Long:
      case CXType_ULongLong: case CXType_LongLong:
      case CXType_Pointer: case CXType_BlockPointer:
      case CXType_ObjCObjectPointer: case CXType_ObjCId:
      case CXType_ObjCClass: case CXType_ObjCSel:
         /* byval_field_x64_size corrects a legacy fixed-32 typedef (SInt32/OSType
          * => canonical `long`) to its true 4-byte native width, so it occupies
          * one eightbyte slot, not the two an 8-byte `long` would span. */
         byval_mark(plan, off64, byval_field_x64_size(f), false);
         return;
      case CXType_Record:
         byval_classify_walk(c, off64, plan);
         return;
      case CXType_ConstantArray: {
         const long long n = clang_getArraySize(c);
         const byval_field ef = byval_array_elem(f);   /* keeps typedef sugar */
         const size_t esz = byval_field_x64_size(ef);  /* fixed-32 corrected */
         for (long long i = 0; i < n; ++i) {
            byval_classify_leaf(ef, off64 + i * esz, plan);
         }
         return;
      }
      default:
         throw std::invalid_argument("byval struct: unsupported field kind");
      }
   }

   static byval_plan byval_classify(CXType record_canon) {
      byval_plan plan;
      plan.sz32 = sizeof_type(record_canon, arch::i386);
      /* corrected x86_64 size: a legacy fixed-32 field is 4 bytes, not 8 (see
       * byval_field_is_int32), so a {OSType;SInt32} record is 8 bytes / ONE
       * eightbyte, not 16 / two — the in-regs vs MEMORY decision below depends
       * on this, as does the outgoing register count. */
      plan.sz64 = byval_x64_sizeof(record_canon);
      if (plan.sz32 == 0 || plan.sz64 == 0) {
         throw std::invalid_argument("byval struct: empty/incomplete");
      }
      byval_classify_walk(record_canon, 0, plan);
      return plan;
   }

   /* stage the x86_64 image of a by-value struct: field-by-field i386 -> x64
    * conversion into `dst` (a scratch slot in the shim frame). `src` (the i386
    * source) and `dst` (the x86_64 image) are advanced BY REFERENCE past the
    * bytes consumed, so a caller can read src.index afterward to learn the TRUE
    * i386 struct size — which differs from sizeof_type(canon, arch::i386) whenever
    * the struct contains a CGFloat: the modern parse canonicalizes CGFloat to an
    * 8-byte double, but the i386 field is a 4-byte float (each such field is read
    * as 4 bytes and widened to 8). Accumulating the per-field advances (via the
    * align_field walk, which already applies i386 4-byte alignment) is the only
    * correct i386 size for a CGFloat-bearing struct. */
   static void byval_flat_convert(conversion& conv, std::ostream& os, CXType record_canon,
                                  MemoryLocation& src, MemoryLocation& dst) {
      std::vector<byval_field> fields;
      byval_collect_fields(record_canon, fields);
      for (const byval_field& f : fields) {
         byval_flat_field(conv, os, f, src, dst);
      }
   }

   static void byval_flat_field(conversion& conv, std::ostream& os, const byval_field& f,
                                MemoryLocation& src, MemoryLocation& dst) {
      const CXType c = f.canon;
      /* CGFloat: i386 4-byte float at src -> x86_64 8-byte double at dst */
      if (byval_field_is_cgfloat(f)) {
         src.align_field(c, arch::i386);
         /* align dst as a DOUBLE (the canon float would 4-align it) */
         CXType dbl = c; dbl.kind = CXType_Double;
         dst.align_field(dbl, arch::x86_64);
         emit_fp_widen(os, true, src, dst);
         src += 4;
         dst += 8;
         return;
      }
      /* legacy fixed-32 typedef (SInt32/OSType => canonical `long`): 4 bytes on
       * BOTH the i386 source and the native x86_64 image (the modern SDK types it
       * `int`), NOT the 8-byte `long` the canonical implies. Copy the 4 bytes and
       * advance both cursors by 4 so the eightbyte layout matches native. */
      if (byval_field_is_int32(f)) {
         CXType i32 = c; i32.kind = CXType_Int;   /* force 4-byte align on both */
         src.align_field(i32, arch::i386);
         dst.align_field(i32, arch::x86_64);
         conv.convert_int(os, c.kind == CXType_ULong ? CXType_UInt : CXType_Int,
                          src, dst);
         src += 4;
         dst += 4;
         return;
      }
      src.align_field(c, arch::i386);
      dst.align_field(c, arch::x86_64);
      switch (c.kind) {
      case CXType_Float:
      case CXType_Double:
         conv.convert_real(os, c.kind, src, dst);
         break;
      case CXType_Bool:
      case CXType_UChar: case CXType_Char_U:
      case CXType_SChar: case CXType_Char_S:
      case CXType_UShort: case CXType_Short:
      case CXType_UInt:  case CXType_Int:  case CXType_Enum:
      case CXType_ULong: case CXType_Long:
      case CXType_ULongLong: case CXType_LongLong:
         conv.convert_int(os, c.kind, src, dst);
         break;
      case CXType_ObjCObjectPointer: case CXType_ObjCId: case CXType_ObjCClass:
         conv.convert_objc_ptr(os, src, dst);
         break;
      case CXType_ObjCSel:
         conv.convert_objc_sel(os, src, dst);
         break;
      case CXType_Pointer: {
         const CXType pointee = clang_getCanonicalType(clang_getPointeeType(c));
         if (pointee.kind == CXType_FunctionProto ||
             pointee.kind == CXType_FunctionNoProto) {
            /* callback field (NSMapTableKeyCallBacks etc.): bind to a native
             * trampoline exactly like a top-level fn-ptr arg */
            conv.convert_fnptr(os, pointee, src, dst);
         } else if (cf_opaque_ptr_type(c)) {
            /* opaque CF-ref field: may carry a proxy-arena handle */
            conv.convert_cf_ptr(os, src, dst);
         } else {
            /* data-pointer field of a BY-VALUE struct: pass the pointer VALUE
             * (low-4GB, valid natively); deep-copying would break identity */
            conv.convert_int(os, CXType_Pointer, src, dst);
         }
         break;
      }
      case CXType_BlockPointer:
         conv.convert_int(os, CXType_Pointer, src, dst);
         break;
      case CXType_Record:
         /* recurse with src/dst by reference: the nested fields advance both by
          * their REAL i386 / x86_64 sizes (a nested CGFloat is 4 bytes on i386,
          * 8 on x86_64). Return early to skip the tail sizeof_type advance below,
          * which would over-count a CGFloat-bearing nested struct's i386 size. */
         byval_flat_convert(conv, os, c, src, dst);
         return;
      case CXType_ConstantArray: {
         const long long n = clang_getArraySize(c);
         const byval_field ef = byval_array_elem(f);   /* keeps typedef sugar */
         for (long long i = 0; i < n; ++i) {
            byval_flat_field(conv, os, ef, src, dst);
         }
         /* element loop already advanced src/dst; skip the tail advance */
         return;
      }
      default:
         throw std::invalid_argument("byval struct: unsupported field kind");
      }
      src += sizeof_type(c, arch::i386);
      dst += sizeof_type(c, arch::x86_64);
   }

   /* Detect a by-value struct RETURN that BOTH ABIs return through a hidden
    * caller-allocated pointer (the MEMORY class), restricted to the homogeneous-
    * FP geometry family so each field can be widened/narrowed exactly like the
    * FP-struct ARG path. x86_64 SysV returns a >16-byte aggregate via a hidden
    * pointer in rdi (the sret register) and hands it back in rax; the i386 cdecl
    * ABI returns any struct larger than the 8-byte eax:edx pair the same way (the
    * buffer address is the implicit FIRST stack arg, returned in eax). So
    * CGRect (16B i386 / 32B x86_64) and CGAffineTransform (24B / 48B) qualify,
    * while CGPoint/CGSize (8B / 16B, register-returned on both) do NOT. Returns
    * true and fills *widen with the per-field flags (true = a CGFloat that is a
    * 4-byte float on i386 and an 8-byte double on x86_64; false = a genuine 8-byte
    * double); returns false for anything else (register-returned, integer/mixed,
    * non-homogeneous, padded), leaving the caller's existing skip path in force.
    * Triggers on the STRUCTURAL return shape, never a function name. */
   bool fp_sret_return(std::vector<bool> *widen = nullptr) const {
      const CXType ret = clang_getCanonicalType(clang_getResultType(function_type));
      if (ret.kind != CXType_Record) { return false; }
      std::vector<bool> w;
      try {
         w = fp_byval_struct_fields(ret);   /* homogeneous-FP only, else throws */
      } catch (const std::invalid_argument&) {
         return false;
      }
      /* MEMORY-return on BOTH ABIs: x86_64 >16 bytes (rdi sret, not xmm0:xmm1)
       * AND i386 >8 bytes (hidden pointer, not eax:edx). */
      if (sizeof_type(ret, arch::x86_64) <= 16 || sizeof_type(ret, arch::i386) <= 8) {
         return false;
      }
      if (widen) { *widen = std::move(w); }
      return true;
   }

   /* Detect a by-value struct RETURN that x86_64 returns in SSE REGISTERS
    * (xmm0:xmm1) but i386 returns in the INTEGER pair eax:edx — the
    * CGPoint/CGSize/NSPoint/NSSize CGFloat-pair family. Empirically (verified by
    * disassembling clang -arch i386): i386 Darwin returns a struct with >=2
    * fields and total size <= 8 bytes in eax:edx (raw bytes), while a struct
    * with a SINGLE float/double field is returned in st0 (the scalar path). So
    * this fires ONLY for the multi-field homogeneous-FP register-return case:
    *   - homogeneous FP (fp_byval_struct_fields succeeds), >= 2 fields;
    *   - x86_64 <= 16 bytes  (register-returned in xmm0:xmm1, NOT the >16 sret
    *     handled by fp_sret_return);
    *   - REAL i386 size = sum(widen? 4 : 8) <= 8  (register-returned in eax:edx,
    *     NOT the i386 hidden-pointer memory return).
    * Under the modern (x86_64-host) parse CGFloat canonicalizes to double so the
    * naive sizeof_type(ret, i386) would give 16 for CGPoint; the widen flags
    * (field_is_cgfloat) recover the true 4-byte-per-field i386 layout. Without
    * this, abigen emits NO return conversion: the native callee leaves the point
    * in xmm0:xmm1 and the i386 caller reads eax:edx = garbage (CGContextGet-
    * TextPosition, CGPointApplyAffineTransform, CGContextConvertPointToUserSpace,
    * CGLayerGetSize, ...). Triggers on the STRUCTURAL return shape, never a name. */
   bool fp_reg_return(std::vector<bool> *widen = nullptr) const {
      const CXType ret = clang_getCanonicalType(clang_getResultType(function_type));
      if (ret.kind != CXType_Record) { return false; }
      std::vector<bool> w;
      try {
         w = fp_byval_struct_fields(ret);   /* homogeneous-FP only, else throws */
      } catch (const std::invalid_argument&) {
         return false;
      }
      if (w.size() < 2) { return false; }    /* single FP field -> st0, not eax:edx */
      if (sizeof_type(ret, arch::x86_64) > 16) { return false; }  /* xmm0:xmm1 only */
      size_t i386_bytes = 0;
      for (bool cg : w) { i386_bytes += cg ? 4 : 8; }
      if (i386_bytes > 8) { return false; }  /* i386 eax:edx only (else memory) */
      if (widen) { *widen = std::move(w); }
      return true;
   }

   /* Detect a by-value struct RETURN that BOTH ABIs return in INTEGER registers:
    * i386 in eax:edx (any struct <= 8 bytes, no hidden sret pointer, no arg
    * shift), x86_64 in rax(:rdx) (an all-INTEGER-eightbyte aggregate <= 16 bytes).
    * These need at most a trivial fix-up and NO sret buffer — the AbsoluteTime /
    * Duration / Nanoseconds timing family (UnsignedWide {UInt32,UInt32}), and the
    * NSRange / CFRange {long,long} family. *hi_split is set when the x86_64 value
    * is a SINGLE eightbyte (in rax) but the i386 struct spans > 4 bytes, so the
    * high dword must be copied rax[63:32] -> edx (i386 returns bytes[4:8] in edx);
    * a 2-eightbyte return already lands in rax:rdx = i386 eax:edx with no fix-up,
    * and a <=4-byte return is just eax. Mutually exclusive with fp_sret/fp_reg
    * (those carry an SSE eightbyte or an i386 hidden pointer). Structural, never a
    * name. Returns false (skip) for unions/packed and any SSE-bearing return. */
   bool int_reg_struct_return(bool *hi_split = nullptr) const {
      const CXType ret = clang_getCanonicalType(clang_getResultType(function_type));
      if (ret.kind != CXType_Record) { return false; }
      byval_plan plan;
      try {
         plan = byval_classify(ret);          /* throws on union/packed -> skip */
      } catch (const std::invalid_argument&) {
         return false;
      }
      if (plan.sz64 > 16) { return false; }    /* x86_64 MEMORY -> real sret path */
      if (sizeof_type(ret, arch::i386) > 8) { return false; } /* i386 hidden ptr   */
      for (unsigned k = 0; k < plan.ebs(); ++k) {
         if (plan.eb_sse(k)) { return false; } /* any SSE eightbyte -> not int-reg */
      }
      if (hi_split) {
         *hi_split = (plan.ebs() == 1) && (sizeof_type(ret, arch::i386) > 4);
      }
      return true;
   }

   /* Kill-switch for the 4th struct-return classifier below: M64_NO_ABIGEN_SRET_GAP=1
    * restores the pre-fix behaviour (no hidden-sret slot for the MEMORY-on-i386 /
    * REGISTER-on-x86_64 family), so the tests-i386 guard can A/B the fix rather
    * than pass inertly. Read once. */
   static bool sret_gap_disabled() {
      static const bool off = getenv("M64_NO_ABIGEN_SRET_GAP") != nullptr;
      return off;
   }

   /* ★ 4th struct-return classifier: the return is MEMORY on i386 but REGISTER
    * on x86_64 — the shape none of the three above claim, and the one that made
    * every declared arg read 4 bytes low.
    *
    *   i386 cdecl: sizeof > 8 => the caller allocates the buffer and passes its
    *     address as the IMPLICIT FIRST STACK ARG; the value comes back in eax;
    *     and (verified from `clang -arch i386 -O1 -S` codegen) the CALLEE POPS
    *     IT — `retl $4`. Our generated bridge IS that i386 callee, so its
    *     declared args start one 4-byte slot later ([rbp+16], not [rbp+12]) and
    *     its epilogue must pop 8, not 4.
    *   x86_64 SysV: sz64 <= 16 => returned in REGISTERS (rax/rdx and/or
    *     xmm0/xmm1 per eightbyte class). NO hidden pointer, so rdi is NOT
    *     consumed and the native INTEGER args do NOT shift.
    *
    * Members measured with ABIGEN_SRET_GAP_TRACE=1 (5 in the modern pass, 0 in
    * the legacy pass), spanning all three x86_64 return-register shapes:
    *   all-INTEGER 2 eightbytes  lldiv, CFUUIDGetUUIDBytes           rax:rdx
    *   homogeneous-FP 2 eightbytes  __sincos_stret, __sincospi_stret xmm0:xmm1
    *   MIXED INTEGER+SSE  CFAbsoluteTimeGetGregorianDate             rax + xmm0
    * which is why the emitter must store PER EIGHTBYTE off plan.eb_sse(k).
    *
    * Mutual exclusion with the other three, made explicit here rather than
    * relying on the emitter's ordering:
    *   - fp_sret_return  needs x86_64 > 16   -> excluded by plan.sz64 <= 16;
    *   - int_reg_struct_return needs i386 <= 8 -> excluded by sz32 > 8;
    *   - fp_reg_return  CAN overlap: under the modern parse CGFloat
    *     canonicalizes to `double`, so a CGPoint reads as sizeof_type(i386)==16
    *     even though its REAL i386 size is 8 (eax:edx, no hidden pointer). It is
    *     rejected twice over — explicitly, and by byval_layout_identical(),
    *     which declines any record carrying a CGFloat.
    *
    * byval_layout_identical() is the extra precondition that lets the emitter
    * copy the x86_64 return image VERBATIM into the i386 caller's buffer; a
    * record whose two layouts differ would need a field-by-field x86_64->i386
    * conversion (byval_flat_convert only runs i386->x86_64) and is left on the
    * pre-existing path. Triggers on the STRUCTURAL return shape, never a name. */
   bool mem32_reg64_return(byval_plan *out = nullptr) const {
      if (sret_gap_disabled()) { return false; }
      const CXType ret = clang_getCanonicalType(clang_getResultType(function_type));
      if (ret.kind != CXType_Record) { return false; }
      byval_plan plan;
      try {
         plan = byval_classify(ret);          /* throws on union/packed -> skip */
      } catch (const std::invalid_argument&) {
         return false;
      }
      if (plan.sz64 > 16) { return false; }                    /* x86_64 MEMORY  */
      if (sizeof_type(ret, arch::i386) <= 8) { return false; } /* i386 eax:edx   */
      if (fp_sret_return() || fp_reg_return()) { return false; }
      if (!byval_layout_identical(ret)) { return false; }
      if (out) { *out = std::move(plan); }
      return true;
   }

   /* Marshal one i386 FP field (a 4-byte CGFloat or an 8-byte double) at `src` to
    * an x86_64 double at `dst` (an xmm register or an 8-byte stack slot), widening
    * float->double when `widen`. A MEMORY (stack) destination bounces through
    * xmm15 — never an argument register, and untouched by the conversion machinery
    * and the runtime-bridge helpers (which save only xmm0-7). */
   static void emit_fp_widen(std::ostream& os, bool widen, const MemoryLocation& src,
                             const Location& dst) {
      if (dst.kind() == Location::Kind::SSE) {
         if (widen) {
            emit_inst(os, "cvtss2sd", dst.op(reg_width::Q), src.op(reg_width::D));
         } else {
            emit_inst(os, "movsd", dst.op(reg_width::Q), src.op(reg_width::Q));
         }
      } else {
         if (widen) {
            emit_inst(os, "cvtss2sd", "xmm15", src.op(reg_width::D));
         } else {
            emit_inst(os, "movsd", "xmm15", src.op(reg_width::Q));
         }
         emit_inst(os, "movsd", dst.op(reg_width::Q), "xmm15");
      }
   }

   /* assumes stack is 16-byte aligned */
   size_t stack_args_size() const {
      size_t size = 0;
      /* a homogeneous-FP MEMORY struct return (CGRect/CGAffineTransform) passes
       * its hidden sret pointer in rdi, so the first GP register is taken and any
       * INTEGER args shift down one — fewer fit in registers, more may spill. */
      unsigned reg_i = fp_sret_return() ? 1 : 0;
      unsigned xmm_i = 0;
      
      for (unsigned argi = 0; argi < argc(); ++argi) {
         const CXType argtype = clang_getCanonicalType(clang_getArgType(function_type, argi));
         if (argtype.kind == CXType_Record) {
            /* by-value struct: general SysV eightbyte classification. Register
             * candidate (<=16B) is all-or-nothing per class; MEMORY (>16B or
             * regs exhausted) spills the whole staged image to 8-byte slots.
             * Mirrors the marshalling loop. Throws -> skip fn. */
            const byval_plan plan = byval_classify(argtype);   /* throws -> skip */
            const bool in_regs = plan.sz64 <= 16 &&
               reg_i + plan.n_int() <= max_reg_args &&
               xmm_i + plan.n_sse() <= max_xmm_args;
            if (in_regs) {
               reg_i += plan.n_int();
               xmm_i += plan.n_sse();
            } else {
               size += align_up<size_t>(plan.sz64, 8);
            }
            continue;
         }
         switch (get_type_domain(get_arg_kind(argtype.kind))) {
         case type_domain::INT:
            if (reg_i < max_reg_args) {
               ++reg_i;
            } else {
               size += std::max<size_t>(sizeof_type(argtype, arch::x86_64), 8);
            }
            break;
         case type_domain::REAL:
            if (xmm_i < max_xmm_args) {
               ++xmm_i;
            } else {
               size += std::max<size_t>(sizeof_type(argtype, arch::x86_64), 8);
            }
            break;
         default: abort();
         }
         
      }
      
      return align_up<size_t>(size, 16);
   }

   size_t stack_data_size() const {
      size_t size = 0;

      for (unsigned argi = 0; argi < argc(); ++argi) {
         CXType argtype = clang_getCanonicalType(clang_getArgType(function_type, argi));

         std::optional<CXType> size_type;
         switch (argtype.kind) {
         case CXType_Pointer:
            size_type = clang_getPointeeType(argtype);
            break;
         case CXType_ConstantArray:
            size_type = argtype;
            break;
         default:
            break;
         }

         if (size_type) {
            align_up(size, alignof_type(*size_type, arch::x86_64));
            size += sizeof_type(*size_type, arch::x86_64);
         }
      }

      return align_up<size_t>(size, 16);
   }



   /* True if `type` (a canonical arg type) is a `va_list`. A va_list is
    * `__builtin_va_list` = `struct __va_list_tag[1]` on x86_64, so as a parameter
    * it appears as a ConstantArray of (or a decayed Pointer to) a Record whose
    * spelling names it __va_list_tag. Detected structurally, never by function
    * name; used to skip functions abigen cannot marshal (see the arg loop). */
   static bool is_va_list_arg(CXType type) {
      CXType inner;
      if (type.kind == CXType_ConstantArray) {
         inner = clang_getCanonicalType(clang_getArrayElementType(type));
      } else if (type.kind == CXType_Pointer) {
         inner = clang_getCanonicalType(clang_getPointeeType(type));
      } else {
         return false;
      }
      if (inner.kind != CXType_Record) { return false; }
      CXString s = clang_getTypeSpelling(inner);
      const bool va =
         std::string(clang_getCString(s)).find("__va_list_tag") != std::string::npos;
      clang_disposeString(s);
      return va;
   }

   void emit(std::ostream& final_os, Symbols& symbols, const Symbols& ignore_structs,
             const Symbols& reserved_names,
             Symbols& emitted_slots, std::vector<std::string>& slot_order) {
      const bool variadic = clang_isFunctionTypeVariadic(function_type);
      const std::string& override_prefix = "__";

      if (variadic) {
         /* skip variadic functions */
         return;
      }

      if (symbols.find(sym) == symbols.end()) {
         return;
      }

      /* Shim-name collision note. The shim for symbol S is named
       * override_prefix+S (e.g. _tolower -> ___tolower). When this symbol's
       * own linker name equals some OTHER bridged symbol's shim name (libc's
       * internal underscore twins: __tolower's name ___tolower == tolower's
       * shim name), this shim's inner `call <sym>` would bind to that sibling
       * shim instead of the real function (the double-wrap). We used to SKIP
       * the twin here — WRONG for an IMPORTED twin: the app's redirected
       * import ("__"+twin) was then left with no definition (or, worse, the
       * twin fell through to the OTHER abigen pass, which emitted it with the
       * capture intact — the Civ IV tolower()==0 "XML Load Error" root cause).
       * Emitting is safe: shim naming is injective (no label clash is
       * possible), and the inner call of every capturable target is routed
       * through a load-time-repaired native slot (see needs_native_slot). */
      if (reserved_names.find(sym) != reserved_names.end()) {
         std::cerr << "abigen: note: " << sym
                   << " collides with another symbol's shim name; "
                   << "emitting with a capture-proof native slot" << std::endl;
      }

      /* Build the trampoline into a local buffer first. The conversion
       * machinery throws std::invalid_argument on signatures it cannot handle
       * (e.g. struct-by-value args). Catch it and SKIP this one function
       * rather than aborting the whole generation run or leaving half-emitted
       * asm in the output. This keeps abigen robust when fed large framework
       * umbrella headers. The symbol is only consumed (erased) on success. */
      std::ostringstream os;
      if (getenv("ABIGEN_TRACE")) {
         std::cerr << "abigen: emitting " << sym << std::endl;
      }
      try {
         emit_body(os, ignore_structs, override_prefix);
      } catch (const std::exception& e) {
         std::cerr << "abigen: skipping " << sym << ": " << e.what() << std::endl;
         return;
      }
      /* Per-shim size cap. A struct-pointer arg/return whose i386 and x86_64
       * layouts differ is deep-copied field-by-field; for a large, deeply nested
       * struct (e.g. Python's PyThreadState / PyInterpreterState / PyCodeObject,
       * or a struct holding a big inline ConstantArray) this recursion unrolls to
       * tens of thousands of asm lines per shim and blows the .asm up to >100MB
       * (unassemblable). Such functions are almost always internals the i386 app
       * does not call across the boundary anyway; skip any shim over the cap (it
       * over-pops exactly as it did before — no regression) rather than emit a
       * pathological body. Default 1200 lines; override with ABIGEN_MAX_SHIM_LINES
       * (0 disables the cap). */
      {
         static const long cap = [] {
            const char *e = getenv("ABIGEN_MAX_SHIM_LINES");
            return e ? std::strtol(e, nullptr, 10) : 1200;
         }();
         if (cap > 0) {
            const std::string body = os.str();
            const long nlines = std::count(body.begin(), body.end(), '\n');
            if (nlines > cap) {
               std::cerr << "abigen: skipping " << sym << ": shim too large ("
                         << nlines << " lines > " << cap << " cap)" << std::endl;
               return;
            }
            symbols.erase(sym);
            final_os << body;
            emit_native_slot(final_os, emitted_slots, slot_order);
            return;
         }
      }
      symbols.erase(sym);
      final_os << os.str();
      emit_native_slot(final_os, emitted_slots, slot_order);
   }

   void emit_body(std::ostream& os, const Symbols& ignore_structs,
                  const std::string& override_prefix) {
      os << "\tglobal\t" << override_prefix << sym << std::endl;
      os << "\textern\t" << native_target() << std::endl;

      os << override_prefix << sym << ":" << std::endl;

      /* check if freshly bound */
      emit_inst(os, "cmp", "qword [rel __dyld_stub_binder_flag]", "0");
      emit_inst(os, "je", ".l1");

      /* fix stack */
      emit_inst(os, "mov", "rsp", "qword [rel __dyld_stub_binder_flag]");
      emit_inst(os, "add", "rsp", "16");
      emit_inst(os, "mov", "qword [rel __dyld_stub_binder_flag]", "0");

      os << ".l1:" << std::endl;
      
      /* save registers */
      emit_inst(os, "push", "rbp");
      emit_inst(os, "mov", "rbp", "rsp");
      emit_inst(os, "push", "rdi");
      emit_inst(os, "push", "rsi");

      /* align stack */
      emit_inst(os, "and", "rsp", "~0xf");

      /* native-callee breadcrumb (gen-gated, diagnostic). Here rsp is
       * 16-aligned, rbp valid ([rbp+8]=i386 caller RA), rdi/rsi already saved,
       * and arg regs not yet marshalled (args load from [rbp+12]) — so rdi/rsi
       * and the call clobber are all safe, and rsp is restored by the call. */
      if (gen_native_crumb()) {
         emit_inst(os, "lea", "rdi", "[rel " + override_prefix + sym + "]");
         emit_inst(os, "mov", "rsi", "qword [rbp+8]");
         emit_inst(os, "call", "_abiconv_native_crumb");
      }

      /* make space on stack */
      MemoryLocation stack_args(rsp, 0);
      // MemoryLocation stack_data(rsp, stack_args_size());
      unsigned label = 0;
      conversion to_conv(true, arch::i386, arch::x86_64, {rsp, static_cast<int>(stack_args_size())},
                         label, ignore_structs);
      conversion from_conv(false, arch::x86_64, arch::i386,
                           {rsp, static_cast<int>(stack_args_size())}, label, ignore_structs);
      
      /* By-value struct RETURN handling (the CG/NS geometry MEMORY-class family).
       * x86_64 SysV returns CGRect (32B) / CGAffineTransform (48B) via a hidden
       * pointer in rdi (the sret register) and i386 cdecl returns the same family
       * via a caller-allocated hidden pointer whose address is the implicit FIRST
       * i386 arg (handed back in eax). We reserve a native return buffer at the top
       * of this frame, point rdi at it, run the call, then NARROW each x86_64
       * double back into the i386 caller's 4-byte CGFloat field. fp_sret gates
       * every piece; ret_widen carries the per-field flags (true = CGFloat
       * float<->double, false = genuine 8-byte double). */
      std::vector<bool> ret_widen;
      const bool fp_sret = fp_sret_return(&ret_widen);
      /* SSE-register FP-pair return (CGPoint/CGSize/NSPoint/NSSize): x86_64
       * returns it in xmm0:xmm1 but i386 wants it in eax:edx. Mutually exclusive
       * with fp_sret (that is the >16B MEMORY family). */
      std::vector<bool> ret_reg_widen;
      const bool fp_reg = !fp_sret && fp_reg_return(&ret_reg_widen);
      /* All-INTEGER register-class struct return (AbsoluteTime/UnsignedWide,
       * NSRange/CFRange): i386 eax:edx, x86_64 rax(:rdx). No sret buffer, no arg
       * shift; at most a rax[63:32]->edx high-dword split (int_ret_hi_split). */
      bool int_ret_hi_split = false;
      const bool int_reg_ret =
         !fp_sret && !fp_reg && int_reg_struct_return(&int_ret_hi_split);
      /* ★ MEMORY-on-i386 / REGISTER-on-x86_64 struct return (mem32_reg64_return:
       * lldiv, CFUUIDGetUUIDBytes, __sincos_stret, __sincospi_stret,
       * CFAbsoluteTimeGetGregorianDate). The i386 caller allocated the buffer and
       * passed its address as the implicit FIRST stack arg, so the declared args
       * shift to [rbp+16] and the epilogue must pop 8 — but the NATIVE side hands
       * the value back in registers, so rdi is NOT an sret pointer and the native
       * arg registers do NOT shift (no ++info.reg_it, and stack_args_size() keeps
       * reg_i at 0). mrs_plan carries the per-eightbyte INT/SSE classes. */
      byval_plan mrs_plan;
      const bool mem_reg_sret =
         !fp_sret && !fp_reg && !int_reg_ret && mem32_reg64_return(&mrs_plan);
      const CXType ret_canon =
         clang_getCanonicalType(clang_getResultType(function_type));
      /* BLAST-RADIUS DIAGNOSTIC (ABIGEN_SRET_GAP_TRACE=1, inert otherwise).
       * Report every record return that NONE of the three classifiers above
       * claimed. The known gap is the family that is MEMORY on i386 (>8 bytes,
       * so the callee takes a hidden sret pointer as its implicit first stack
       * arg) but REGISTER on x86_64 (<=16 bytes) and not homogeneous-FP: for
       * those we emit no hidden-sret slot, so every declared argument is read 4
       * bytes low and the return conversion is wrong. Counting them is what
       * decides whether this is a footnote or systematic. */
      if (ret_canon.kind == CXType_Record && !fp_sret && !fp_reg && !int_reg_ret &&
          !mem_reg_sret && getenv("ABIGEN_SRET_GAP_TRACE")) {
         const size_t sz32 = sizeof_type(ret_canon, arch::i386);
         bool classified = true;
         size_t sz64 = 0;
         try {
            sz64 = byval_classify(ret_canon).sz64;
         } catch (const std::invalid_argument&) {
            classified = false;      /* union/packed: byval_classify declines it */
         }
         const bool gap = classified && sz32 > 8 && sz64 <= 16;
         std::cerr << "[abigen-sret-gap] " << (gap ? "GAP  " : "other")
                   << " i386=" << sz32 << " x86_64=";
         if (classified) { std::cerr << sz64; } else { std::cerr << "declined"; }
         std::cerr << " " << sym << "\n";
      }
      /* Native return buffer. fp_sret needs one because rdi must point at it;
       * mem_reg_sret needs one because the value arrives in REGISTERS and must be
       * captured to memory IMMEDIATELY after the call (the out-param copy-backs
       * and the return-wrap block that follow can clobber rax/rdx/xmm0/xmm1)
       * before it is copied, byte-exactly, into the i386 caller's buffer. */
      const size_t sret_size =
         fp_sret ? align_up<size_t>(sizeof_type(ret_canon, arch::x86_64), 16)
                 : (mem_reg_sret ? align_up<size_t>(mrs_plan.sz64, 16) : 0);
      /* the native return buffer sits just ABOVE the outgoing stack args and the
       * pointer-deep-copy scratch data, at the top of the reserved frame */
      const size_t sret_buf_off = stack_args_size() + stack_data_size();

      /* scratch slots where by-value struct args stage their x86_64 image
       * (byval_flat_convert), above the outgoing args + pointer-copy data +
       * sret buffer. One 16-aligned slot per record arg, consumed in order. */
      size_t byval_scratch = 0;
      for (unsigned argi = 0; argi < argc(); ++argi) {
         const CXType t = clang_getCanonicalType(clang_getArgType(function_type, argi));
         if (t.kind == CXType_Record) {
            byval_scratch += align_up<size_t>(byval_classify(t).sz64, 16);
         }
      }
      size_t byval_off = stack_args_size() + stack_data_size() + sret_size;

      emit_inst(os, "sub", "rsp",
                stack_data_size() + stack_args_size() + sret_size + byval_scratch);

      /* transfer arguments */
      param_info info(regs.begin(), regs.end(), 8);
      /* the hidden sret pointer consumes rdi on the native side and occupies the
       * i386 caller's first arg slot, so the real declared args start one i386 slot
       * later and the first INTEGER arg shifts from rdi to rsi. */
      if (fp_sret) { ++info.reg_it; }
      /* ★ mem_reg_sret does NOT advance info.reg_it. The two halves of "hidden
       * sret pointer" are independent and only fp_sret has BOTH: the i386 side
       * has one (so load_loc shifts by 4 below), but the x86_64 side returns the
       * aggregate in registers and therefore consumes NO rdi. Advancing reg_it
       * here would push the first INTEGER arg into rsi and hand the native callee
       * garbage in rdi — e.g. CFAbsoluteTimeGetGregorianDate's CFTimeZoneRef would
       * land in rsi. Confirmed by the same reasoning in stack_args_size(), which
       * likewise starts reg_i at 0 for this class. */
      int param_it;
      int param_end = clang_getNumArgTypes(function_type);
      /* i386 arg shift: the hidden sret buffer pointer occupies the FIRST i386
       * stack slot ([rbp+12]), so declared args start one 4-byte slot later.
       * Matches `clang -arch i386` codegen, where a struct-returning callee reads
       * the buffer at 8(%ebp) and its first declared arg at 12(%ebp). */
      MemoryLocation load_loc(rbp, (fp_sret || mem_reg_sret) ? 16 : 12);

      std::stringstream to_ss;
      std::stringstream from_ss;

      /* A by-value struct RETURN that we do NOT recognize as a homogeneous-FP
       * MEMORY sret (fp_sret) is still unmarshalled, so guard against pairing such
       * a return with a newly-marshalled FP-struct ARG below (skip that combo). */
      const bool ret_is_record = ret_canon.kind == CXType_Record;

      for (param_it = 0;
           param_it != param_end;
           ++param_it) {

         CXType orig_type = clang_getArgType(function_type, param_it);
         CXType type = handle_type(orig_type);

         if (is_va_list_arg(type)) {
            /* A va_list arg is fundamentally un-marshallable by abigen: the i386
             * va_list is a plain char* pointing straight at the stack varargs,
             * whereas x86_64's is a __va_list_tag register-save-area struct — and
             * translating it needs the arg TYPES, which only the callee's format
             * string yields at runtime. The generic Pointer/ConstantArray path
             * would deep-copy the i386 char* AS a __va_list_tag (16 garbage bytes)
             * -> native va_arg derefs a fused overflow_arg_area -> SIGSEGV. Skip
             * the function so no actively-broken shim is emitted; a correct v*
             * function needs a per-function hand-shim that walks the i386 va_list
             * from its format (printf-conv.cc, CFStringCreateWithFormatAndArguments
             * in objc_shim.c). Universal: triggers on the __va_list_tag structure. */
            throw std::invalid_argument(
               "va_list arg not marshallable (i386 char* vs x86_64 __va_list_tag);"
               " needs a hand-shim");
         }

         if (type.kind == CXType_Record) {
            /* by-value struct: general SysV eightbyte classification (see the
             * byval_* helpers above). Stage the x86_64 image in this arg's
             * scratch slot, then load each eightbyte into the next GP/XMM arg
             * register — or copy the image to the outgoing stack area when
             * MEMORY-class / registers exhausted (all-or-nothing). No post-call
             * copy-back (the arg is by value). Throws -> skip the function
             * (exactly the old failure mode for still-unsupported shapes:
             * unions, packed, bitfields, CGFloat-under-i386-parse). */
            const byval_plan plan = byval_classify(type);
            if (ret_is_record && !fp_sret && !fp_reg && !int_reg_ret && !mem_reg_sret) {
               /* An UNRECOGNIZED record return leaves the i386 hidden sret pointer
                * / arg offsets unmodelled -> pairing it with a byval arg would
                * marshal every arg from the wrong slot. But a REGISTER-class
                * return (fp_reg = CGPoint/CGSize in xmm0:xmm1; int_reg_ret =
                * AbsoluteTime/NSRange/CFRange in eax:edx) has NO i386 hidden
                * pointer and NO arg shift, so the default modelling is already
                * correct and the return fix-up is emitted after the call. Those
                * combos (CGPointApplyAffineTransform, CGContextConvertPointTo*,
                * NSIntersectionRange, CFDataFind, AddDurationToAbsolute, ...) are
                * now marshalled; only a genuine sret-shaped return we didn't
                * detect (fp_sret) stays conservatively skipped. */
               throw std::invalid_argument(
                  "byval struct arg with unhandled struct return not supported");
            }
            const bool in_regs = plan.sz64 <= 16 &&
               static_cast<size_t>(std::distance(info.reg_it, info.reg_end)) >= plan.n_int() &&
               (info.fp_idx + plan.n_sse()) <= info.fp_end;
            const size_t slot = byval_off;
            byval_off += align_up<size_t>(plan.sz64, 16);
            to_ss << "\t; stage by-value struct '" << to_string(type)
                  << "' x86_64 image at [rsp+" << slot << "]" << std::endl;
            /* stage into local src/dst copies; byval_flat_convert advances them by
             * reference, so bsrc.index - load_loc.index is the TRUE i386 struct
             * size (CGFloat-aware) used to step load_loc to the next i386 arg. */
            MemoryLocation bsrc = load_loc;
            MemoryLocation bdst(rsp, static_cast<int>(slot));
            byval_flat_convert(to_conv, to_ss, type, bsrc, bdst);
            const size_t real_i386 = static_cast<size_t>(bsrc.index - load_loc.index);
            if (in_regs) {
               for (unsigned k = 0; k < plan.ebs(); ++k) {
                  MemoryLocation eb(rsp, static_cast<int>(slot + 8 * k));
                  if (plan.eb_sse(k)) {
                     SSELocation fdst(info.fp_idx++);
                     emit_inst(to_ss, "movsd", fdst.op(reg_width::Q), eb.op(reg_width::Q));
                  } else {
                     emit_inst(to_ss, "mov", (*info.reg_it++)->reg_q, eb.op(reg_width::Q));
                  }
               }
            } else {
               /* copy the staged image to consecutive 8-byte outgoing slots.
                * r11 is free between conversions (the machinery push/pops it). */
               for (size_t o = 0; o < align_up<size_t>(plan.sz64, 8); o += 8) {
                  MemoryLocation eb(rsp, static_cast<int>(slot + o));
                  emit_inst(to_ss, "mov", "r11", eb.op(reg_width::Q));
                  emit_inst(to_ss, "mov", stack_args.op(reg_width::Q), "r11");
                  stack_args += 8;
               }
            }
            /* i386 stack args are 4-byte granular; step past this struct's REAL
             * i386 size (NOT plan.sz32 = sizeof_type(canon,i386), which counts a
             * CGFloat as an 8-byte double and would mis-locate every later arg). */
            load_loc += align_up<size_t>(real_i386, 4);
            continue;
         }

         /* Scalar CGFloat arg. The modern (x86_64-host) parse canonicalizes
          * CGFloat to `double`, so the generic REAL path below would read an
          * 8-byte double from the i386 stack — but the i386 ABI passed CGFloat as
          * a 4-byte `float`, mis-reading this arg AND shifting every later arg.
          * Detect it by the AS-WRITTEN typedef name (like the struct-field path)
          * and widen the 4-byte float to an 8-byte double (cvtss2sd) into the next
          * xmm register (or an 8-byte stack slot). No copy-back (by value). This is
          * the same structural i386-float -> x86_64-double fix as the FP-struct
          * fields, applied to a bare scalar — universal across every CGFloat-taking
          * C function (CGAffineTransformMakeScale/Rotation, CGColorCreateGenericGray,
          * ...). stack_args_size() already sizes a REAL scalar identically (the
          * x86_64 side is one xmm or one 8-byte slot either way), so the outgoing
          * layout is unchanged; only the i386 read width (4 vs 8) differs. */
         if (type.kind == CXType_Double && field_is_cgfloat(orig_type)) {
            if (info.fp_idx != info.fp_end) {
               SSELocation fdst(info.fp_idx++);
               emit_fp_widen(to_ss, true, load_loc, fdst);
            } else {
               MemoryLocation fdst = stack_args;
               emit_fp_widen(to_ss, true, load_loc, fdst);
               stack_args += 8;
            }
            load_loc += 4;             /* i386 CGFloat width (float) */
            continue;
         }

         std::unique_ptr<Location> src;
         std::unique_ptr<Location> dst;
         switch (get_type_domain(get_arg_kind(type.kind))) {
         case type_domain::INT:
            if (info.reg_it != info.reg_end) {
               dst = std::make_unique<RegisterLocation>(**info.reg_it++);
            } else {
               dst = std::make_unique<MemoryLocation>(stack_args);
            }
            break;
            
         case type_domain::REAL:
            if (info.fp_idx != info.fp_end) {
               dst = std::make_unique<SSELocation>(info.fp_idx++);
            } else {
               dst = std::make_unique<MemoryLocation>(stack_args);
            }
         }

         CXTypeKind type_kind;
         if (cf_void_ref_type(orig_type)) {
            /* CFTypeRef / CFPropertyListRef value: a void*-backed CF object
             * ref that carries a proxy handle (e.g. CFDateFormatterSetProperty's
             * value arg — iWeb passed a raw handle, CF then msgSend'd its garbage
             * isa). Unwrap going in; no copy-back, same as the opaque *Ref args. */
            type_kind = CXType_Pointer;
            to_conv.convert_cf_ptr(to_ss, load_loc, *dst);
         } else
         if (is_opaque_handle_type(orig_type)) {
            /* Bare opaque Memory Manager Handle ARG (Handle / AEDataStorage /
             * *Handle: a T** whose typedef spelling names it a Handle). The app
             * treats it as an OPAQUE token — a pointer to a relocatable master
             * pointer — so marshal the pointer VALUE (i386 4-byte low-4GB handle
             * -> zero-extended to 8 bytes), NEVER dereference/deep-copy it. The
             * generic Pointer path below would convert_pointer(T**) = read the
             * master pointer through the handle (movl (%rN),... on an opaque
             * token) -> EXC_BAD_ACCESS. This is the top-level-ARG twin of the
             * convert_record opaque-Handle FIELD case (is_opaque_handle_type):
             * GetDialogItemText/SetDialogItemText/HLock/HUnlock/DisposeHandle,
             * the Resource Manager, AppleEvent AEDesc handles, ... No copy-back
             * (the handle value is unchanged; re-writing it would clobber the
             * i386 caller's slot). Detected on orig_type because canonicalization
             * to `char **` loses the *Handle typedef spelling. */
            type_kind = CXType_Pointer;
            to_ss << "\t; opaque Handle arg '" << to_string(orig_type)
                  << "' -> pointer value (no deep-copy)" << std::endl;
            to_conv.convert_int(to_ss, CXType_Pointer, load_loc, *dst);
         } else
         switch (type.kind) {
         case CXType_ConstantArray:
            type_kind = CXType_Pointer;
            to_conv.convert_pointer(to_ss, type, load_loc, *dst);
            from_conv.convert_pointer(from_ss, type, *dst, load_loc);
            break;

         /* objc object/Class/SEL by-value args: no post-call copy-back — it
          * would only re-wrap the (unchanged) value into a fresh proxy-arena
          * handle per distinct object, growing the arena for nothing. The
          * reverse wrap still runs for deep-copied pointees (NSError **). */
         case CXType_ObjCObjectPointer:
         case CXType_ObjCId:
         case CXType_ObjCClass:
         case CXType_ObjCSel:
            type_kind = type.kind;
            to_conv.convert(to_ss, type, load_loc, *dst);
            break;

         default:
            type_kind = type.kind;
            to_conv.convert(to_ss, type, load_loc, *dst);
            /* CF-ref by-value args: same no-copy-back rule as the objc
             * kinds — the arg registers are clobbered after the call, and
             * the conditional re-wrap would mint arena handles from that
             * garbage on every call */
            if (!cf_opaque_ptr_type(type)) {
               from_conv.convert(from_ss, type, *dst, load_loc);
            }
            break;
         }

         load_loc += align_up<size_t>(sizeof_type(type_kind, arch::i386), 4);
         if (dst->kind() == Location::Kind::MEM) {
            stack_args += align_up<size_t>(sizeof_type(type, arch::x86_64), 8);
         }

      }

      /* convert from i386 to x86_64 */
      os << to_ss.str();

      /* sret: point rdi at the native return buffer. Done after arg marshalling
       * (no declared arg targets rdi — we skipped it above) and right before the
       * call, so nothing clobbers it. */
      if (fp_sret) {
         emit_inst(os, "lea", "rdi",
                   "[rsp + " + std::to_string(sret_buf_off) + "]");
      }

      /* call */
      emit_call(os);

      /* ★ mem_reg_sret CAPTURE, PER EIGHTBYTE, immediately after the call.
       * The native callee returned the aggregate in registers, assigned in SysV
       * order: INTEGER eightbytes take rax then rdx, SSE eightbytes take xmm0
       * then xmm1, INDEPENDENTLY — so a MIXED {int, double} return
       * (CFAbsoluteTimeGetGregorianDate) is rax + xmm0, an all-INTEGER return
       * (lldiv_t, CFUUIDBytes) is rax:rdx, and a homogeneous-FP return
       * (__sincos_stret) is xmm0:xmm1. A single movsd/mov cannot serve all three,
       * hence plan.eb_sse(k) per eightbyte with its OWN counter.
       * Captured here, before the from_ss out-param copy-backs and the return-wrap
       * block, both of which may call into the runtime bridge and clobber
       * rax/rdx/xmm0-7. The copy into the i386 caller's buffer happens in the tail
       * block, where rcx/r11 are free. */
      if (mem_reg_sret) {
         os << "\t; capture x86_64 register-class struct return -> native buffer"
            << std::endl;
         static const char *const int_ret[] = {"rax", "rdx"};
         unsigned int_i = 0, sse_i = 0;
         for (unsigned k = 0; k < mrs_plan.ebs(); ++k) {
            const std::string slot =
               "qword [rsp + " + std::to_string(sret_buf_off + 8 * k) + "]";
            if (mrs_plan.eb_sse(k)) {
               emit_inst(os, "movsd", slot, "xmm" + std::to_string(sse_i++));
            } else {
               emit_inst(os, "mov", slot, int_ret[int_i++]);
            }
         }
      }

      /* Scalar float/double return: the native x86_64 callee returns the value
       * in xmm0, but the i386 caller (cdecl) expects it on the x87 stack (st0).
       * Bounce xmm0 -> st0 through the red zone ([rsp-8]; no further call on a
       * float/double-return path, and captured here before any out-param
       * copy-back could clobber xmm0). long double already returns in st0 on
       * BOTH ABIs (no conversion). Universal: every float/double-returning C
       * function (libm floorf/ceilf/sqrtf/..., CGFloat getters, CFAbsoluteTime,
       * ...) needs this; without it the i386 caller reads garbage from st0
       * (and an UNSHIMMED libm fn's native 8-byte ret over-pops the i386 frame
       * -> fused PC). */
      {
         const CXType rret =
            clang_getCanonicalType(clang_getResultType(function_type));
         if (rret.kind == CXType_Float) {
            emit_inst(os, "movss", "[rsp - 8]", "xmm0");
            emit_inst(os, "fld", "dword [rsp - 8]");
         } else if (rret.kind == CXType_Double) {
            emit_inst(os, "movsd", "[rsp - 8]", "xmm0");
            emit_inst(os, "fld", "qword [rsp - 8]");
         }
      }

      /* convert from x86_64 to i386 */
      os << from_ss.str();

      /* Wrap an ObjC-object return value into a 32-bit proxy handle. A C
       * function declared to return an ObjC object (NSString*, id, NSArray*,
       * ...) — e.g. NSHomeDirectory(), NSSearchPath..., NSFullUserName() —
       * returns a real 64-bit Foundation pointer. The i386 caller reads only
       * the low 4 bytes (eax), truncating it to a wild pointer that crashes the
       * moment the object is retained/messaged (e.g. inserted into a real
       * NSDictionary). x64_objc_wrap mints a low-4GB handle that the
       * objc_msgSend bridge unwraps back to the real object. rsp is still the
       * 16-aligned call frame here, so the call is ABI-safe; rdi was saved at
       * [rbp-8]. CF `^struct` returns are left untouched (no regression). */
      {
         const CXType rorig = clang_getResultType(function_type);
         const CXType rcanon = clang_getCanonicalType(rorig);
         /* Conditional CF/opaque-pointer return wrap: a native CF/object ref
          * that lives above 4GB (the CF heap, dyld-cache constants) TRUNCATES
          * in the i386 caller's 4-byte eax. Mint a low-4GB proxy handle for it
          * (the CF-arg unwrap, convert_cf_ptr, restores the real ref on the
          * next call); a genuine low-4GB return (a data/context pointer, an
          * i386 buffer) has a zero high half and passes through unchanged, so
          * this is regression-safe. Shared by the named-CF-ref and raw-void*
          * return cases below. The `.cfretlow` local label is unique per shim
          * because at most one mutually-exclusive branch emits it. */
         auto emit_cf_cond_ret_wrap = [&os]() {
            emit_inst(os, "mov", "rdi", "rax");
            emit_inst(os, "shr", "rdi", "32");
            emit_inst(os, "jz", ".cfretlow");
            emit_inst(os, "mov", "rdi", "rax");
            emit_inst(os, "call", "_x64_objc_wrap");
            os << ".cfretlow:" << std::endl;
         };
         /* libclang models SEL as Pointer-to-ObjCSel (see typeconv
          * convert_pointer); catch both shapes */
         const bool ret_is_sel =
            rcanon.kind == CXType_ObjCSel ||
            (rcanon.kind == CXType_Pointer &&
             clang_getCanonicalType(clang_getPointeeType(rcanon)).kind
                == CXType_ObjCSel);
         if (rcanon.kind == CXType_ObjCObjectPointer ||
             rcanon.kind == CXType_ObjCId ||
             rcanon.kind == CXType_ObjCClass) {
            /* Class returns too (NSClassFromString): native Class objects
             * live in the dyld shared cache, far above 4GB */
            emit_inst(os, "mov", "rdi", "rax");
            emit_inst(os, "call", "_x64_objc_wrap");
         } else if (ret_is_sel) {
            /* SEL returns (NSSelectorFromString): intern a stable low-4GB
             * selector-name pointer the i386 caller can store and re-message */
            emit_inst(os, "mov", "rdi", "rax");
            emit_inst(os, "call", "_x64_objc_sel_wrap");
         } else if (cf_opaque_ptr_type(rcanon) || cf_void_ref_type(rorig)) {
            /* Opaque CF refs (CFStringRef = `struct __CFString *`, ...) AND the
             * void*-backed CFTypeRef / CFPropertyListRef typedefs (CFRetain,
             * CFBundleGetValueForInfoDictionaryKey, ...). The latter is matched
             * by the as-written typedef name (cf_void_ref_type) since its
             * canonical type is a bare const void* indistinguishable from a
             * non-object void* — so it is recognised here, off rorig, not the
             * generic void* return case below. A dyld-cache/CF-heap ref above
             * 4GB would truncate in the i386 caller's eax — wrap only those; low
             * heap refs stay raw (status quo, no arena churn). The CF-arg
             * unwrap (convert_cf_ptr) accepts both forms. */
            emit_cf_cond_ret_wrap();
         } else if (rcanon.kind == CXType_Pointer) {
            const CXTypeKind pk =
               clang_getCanonicalType(clang_getPointeeType(rcanon)).kind;
            if (pk == CXType_Char_S || pk == CXType_Char_U ||
                pk == CXType_SChar  || pk == CXType_UChar) {
               /* C-string return such as glGetString's const GLubyte ptr (a
                * pointer to char or unsigned char): a >4GB native static string
                * truncates to a wild pointer in the i386 caller's eax and faults
                * when its bytes are read (strlen / initWithUTF8String:). Bounce
                * a high return into a low-4GB copy; a low return (a pointer into
                * a caller buffer, the strchr/strstr case) passes through. */
               emit_inst(os, "mov", "rdi", "rax");
               emit_inst(os, "call", "_x64_cstr_ret_low");
            } else if (pk == CXType_Void) {
               /* raw `const void*` / `void*` return: the CF-collection accessors
                * CFArrayGetValueAtIndex / CFDictionaryGetValue / CFSetGetValue
                * (and CFBundleGetValueForInfoDictionaryKey) hand back the stored
                * element as a bare void*. When that element is a native CF/object
                * ref above 4GB it truncates in the i386 caller's eax and the next
                * CFGetTypeID / CFRetain derefs the wild low-32 pointer (Halo's
                * libabiconv `__CFGetTypeID` EXC_BAD_ACCESS wall). Conditionally
                * wrap it exactly like the named-CFTypeRef case so the >4GB ref
                * round-trips as a proxy handle (the CF-arg unwrap restores it on
                * the next call); a genuine low data/context void* passes through
                * unchanged, so this is regression-safe. */
               emit_cf_cond_ret_wrap();
            }
         } else if (rcanon.kind == CXType_LongLong ||
                    rcanon.kind == CXType_ULongLong) {
            /* 64-bit integer return: the i386 cdecl ABI returns it in edx:eax
             * (eax=low, edx=high), but the x86_64 callee returned the whole
             * value in rax. eax already aliases rax's low 32 bits; set edx to
             * the high 32 so the i386 caller reads the full value. Without this
             * a uint64-returning function (mach_absolute_time, mach_continuous_
             * time, AudioGetCurrentHostTime, ...) leaves garbage in the high
             * half — exactly the split that halo_shim's MTSHIM64 used to do by
             * hand, now done universally for every 64-bit-int-returning shim. */
            emit_inst(os, "mov", "rdx", "rax");
            emit_inst(os, "shr", "rdx", "32");
         }
      }

      /* MEMORY-class FP-struct return (sret): the native callee wrote its
       * widen.size() doubles into our return buffer (its address was passed in
       * rdi); the i386 caller wants the corresponding 4-byte CGFloat fields in
       * ITS buffer (the hidden first arg, still untouched at [rbp+12]) and that
       * pointer back in eax. Narrow each double -> float (cvtsd2ss) into the i386
       * buffer; a genuine 8-byte double field is copied verbatim. rcx and xmm0 are
       * caller-saved and dead after the call, so both are free scratch here. */
      if (fp_sret) {
         os << "\t; narrow x86_64 FP-struct sret (doubles) -> i386 CGFloat buffer"
            << std::endl;
         emit_inst(os, "mov", "ecx", "dword [rbp + 12]");  /* i386 sret ptr (low-4GB) */
         size_t x64_off = 0;
         int i386_off = 0;
         for (bool w : ret_widen) {
            const std::string fsrc =
               "qword [rsp + " + std::to_string(sret_buf_off + x64_off) + "]";
            if (w) {
               emit_inst(os, "cvtsd2ss", "xmm0", fsrc);
               emit_inst(os, "movss",
                         "dword [rcx + " + std::to_string(i386_off) + "]", "xmm0");
               i386_off += 4;        /* i386 CGFloat width (float) */
            } else {
               emit_inst(os, "movsd", "xmm0", fsrc);
               emit_inst(os, "movsd",
                         "qword [rcx + " + std::to_string(i386_off) + "]", "xmm0");
               i386_off += 8;        /* genuine double width */
            }
            x64_off += 8;            /* one x86_64 eightbyte per field */
         }
         emit_inst(os, "mov", "eax", "dword [rbp + 12]");  /* return i386 sret ptr */
      }

      /* SSE-register FP-pair return (CGPoint/CGSize/NSPoint/NSSize): the native
       * callee returned the two doubles in xmm0:xmm1; the i386 caller wants the
       * two 4-byte CGFloat fields in eax:edx (field0->eax, field1->edx). Narrow
       * each double -> float (cvtsd2ss) and move it into the integer pair. Placed
       * last: xmm0/xmm1 survive the from_ss out-param copy-backs and the return-
       * wrap block (both save/restore xmm0-7 around any runtime-bridge call), and
       * eax/edx are caller-saved and dead here. Both fields are 4-byte on i386
       * (fp_reg_return only fires when the total i386 size is <= 8 with >= 2
       * fields => every field 4 bytes), so both are cvtsd2ss narrows. */
      if (fp_reg) {
         os << "\t; narrow x86_64 SSE-pair record return (xmm0:xmm1) -> i386 eax:edx"
            << std::endl;
         emit_inst(os, "cvtsd2ss", "xmm0", "xmm0");
         emit_inst(os, "movd", "eax", "xmm0");
         emit_inst(os, "cvtsd2ss", "xmm1", "xmm1");
         emit_inst(os, "movd", "edx", "xmm1");
      }

      /* All-INTEGER register-class struct return (UnsignedWide/AbsoluteTime): the
       * native callee packed the <=8-byte aggregate into a SINGLE eightbyte in
       * rax, but the i386 caller reads an 8-byte struct as eax:edx — so the high
       * dword must be copied rax[63:32] -> edx (i386 returns bytes[4:8] there).
       * eax already holds bytes[0:4]. A 2-eightbyte return (NSRange/CFRange) is
       * already in rax:rdx = i386 eax:edx and needs no fix-up (hi_split false);
       * a <=4-byte return is just eax. rdx is caller-saved and dead here. */
      if (int_reg_ret && int_ret_hi_split) {
         os << "\t; split x86_64 single-eightbyte int struct return rax -> i386 eax:edx"
            << std::endl;
         emit_inst(os, "mov", "rdx", "rax");
         emit_inst(os, "shr", "rdx", "32");
      }

      /* ★ mem_reg_sret STORE-BACK: copy the captured x86_64 return image into the
       * i386 caller's hidden buffer (its address is still untouched at [rbp+12],
       * a low-4GB i386 pointer) and hand that pointer back in eax, which is what
       * the i386 cdecl ABI promises a struct-returning callee leaves there.
       * mem32_reg64_return only fires when byval_layout_identical() holds, so the
       * two layouts are byte-identical and this is a straight sz-byte copy — no
       * field-by-field narrowing (that is fp_sret's CGFloat job). rcx and r11 are
       * caller-saved and dead here; rdi/rsi are still saved at [rbp-8]/[rbp-16].
       * The copy is chunked 8/4/2/1 so a non-multiple-of-8 aggregate never writes
       * past the end of the caller's buffer. */
      if (mem_reg_sret) {
         const size_t sz = mrs_plan.sz64;
         os << "\t; copy x86_64 register-class struct return -> i386 sret buffer ("
            << sz << " bytes)" << std::endl;
         emit_inst(os, "mov", "ecx", "dword [rbp + 12]");   /* i386 sret ptr */
         size_t o = 0;
         while (o < sz) {
            size_t chunk = 8;
            while (chunk > 1 && o + chunk > sz) { chunk >>= 1; }
            const char *sfx = chunk == 8 ? "qword" : chunk == 4 ? "dword"
                                         : chunk == 2 ? "word" : "byte";
            const char *tmp = chunk == 8 ? "r11" : chunk == 4 ? "r11d"
                                         : chunk == 2 ? "r11w" : "r11b";
            emit_inst(os, "mov", tmp,
                      std::string(sfx) + " [rsp + " +
                      std::to_string(sret_buf_off + o) + "]");
            emit_inst(os, "mov",
                      std::string(sfx) + " [rcx + " + std::to_string(o) + "]", tmp);
            o += chunk;
         }
         emit_inst(os, "mov", "eax", "dword [rbp + 12]");   /* return it in eax */
      }

      // emit_inst(os, "add", "rsp", stack_data_size() + stack_args_size());
      emit_inst(os, "lea", "rsp", "[rbp - 0x10]");
      
      /* cleanup */
      emit_inst(os, "pop", "rsi");
      emit_inst(os, "pop", "rdi");
      emit_inst(os, "leave");

      /* return. This shim IS the i386 callee, so it obeys i386 cdecl: pop the
       * 4-byte return address... and, for a struct return passed through a hidden
       * caller-allocated buffer, ALSO the 4-byte hidden pointer — `retl $4`, the
       * callee-pops rule verified from real `clang -arch i386 -O1 -S` codegen
       * (the caller's own `addl` after the call already excludes those 4 bytes).
       * Popping only 4 leaves the i386 caller's esp 4 bytes off PER CALL, which
       * corrupts far from the call site rather than at it.
       *
       * BOTH hidden-pointer families are covered:
       *   mem_reg_sret  MEMORY on i386 / REGISTER on x86_64  (guard 89)
       *   fp_sret       homogeneous-FP, MEMORY on BOTH ABIs — the
       *                 CGAffineTransform / CGRect / NSRect geometry family
       * fp_sret already reads its args from the SHIFTED slots ([rbp+16], and the
       * first INTEGER arg moved off rdi), i.e. it already models the hidden
       * pointer as present on the i386 stack — so popping only 4 was simply
       * inconsistent with its own arg handling. It went unnoticed because a
       * 4-byte-per-call leak changes no returned VALUE: 50_cgaffine_sret
       * exercises this exact family and passes on values alone. Guard
       * 90_fp_sret_stack_balance measures the caller's esp instead and was RED
       * (exit 1 of 15: value ok, all three esp checks failed).
       * Kill-switch M64_NO_FP_SRET_POP8=1 restores the old 4-byte pop so that
       * guard can A/B this rather than pass inertly. */
      const bool pop_hidden_ptr =
         mem_reg_sret || (fp_sret && !getenv("M64_NO_FP_SRET_POP8"));
      emit_inst(os, "mov", "r11d", "dword [rsp]");
      emit_inst(os, "add", "rsp", pop_hidden_ptr ? "8" : "4");
      emit_inst(os, "jmp", "r11");
      
   }
   
};

struct FunctionConversion: ABIConversion {
   template <typename... Args>
   FunctionConversion(Args&&... args):
      ABIConversion(args..., {&rdi, &rsi, &rdx, &rcx, &r8, &r9}) {}

   virtual bool calls_native() const override { return true; }

   virtual void emit_call(std::ostream& os) const override {
      if (needs_native_slot()) {
         /* capture-proof indirect call through the load-time-repaired slot;
          * see needs_native_slot() */
         emit_inst(os, "call", "qword [rel " + native_slot_label() + "]");
      } else {
         emit_inst(os, "call", native_target());
      }
   }
};

struct SyscallConversion: ABIConversion {
   template <typename... Args>
   SyscallConversion(Args&&... args):
      ABIConversion(args..., {&rax, &rdi, &rsi, &rdx, &r10, &r8, &r9}) {}

   virtual void emit_call(std::ostream& os) const override {
      emit_inst(os, "syscall");
   }
};

struct ABIGenerator {
   CXIndex index = clang_createIndex(0, 0);
   std::ostream& os;
   Symbols symbols;
   Symbols ignore_structs;
   Symbols collision_names; /* shim names (override_prefix+sym) reserved across
                             * the whole consider set; a symbol whose own linker
                             * name is in here gets a note (its inner call is
                             * capture-proofed via a native slot; see
                             * needs_native_slot) */
   Symbols emitted_slots;   /* native-slot dedup: targets with a slot emitted */
   std::vector<std::string> slot_order; /* slot emission order for the footer */
   enum class ABI {FUNCTION, SYSCALL} abi;
   bool force_all;
   /* Secondary (legacy) pass: this .asm is assembled into libabiconv ALONGSIDE
    * the primary abiconv.asm. The singleton data-shadow runtime tables
    * (_x64_data_shadows / _x64_data_shadows_count, consumed once by
    * x64_init_data_shadows in objc_shim.c) are emitted only by the primary pass;
    * a second definition would be a duplicate-symbol link error. So when set we
    * suppress data-shadow collection/emission entirely (function shims only).
    * Function shims (global ___sym) never collide because the legacy consider
    * set excludes everything the primary pass already shims. See
    * the shimdb legacy expansion. */
   bool secondary_pass = false;
   std::vector<std::string> clang_args;  /* extra args passed to libclang */
   /* External ObjC-object DATA constants (e.g. NSString* const NSArgumentDomain)
    * in the consider set. abigen emits a low-4GB shadow variable + a runtime
    * table for each; see emit_data_shadows(). Stored WITH the leading
    * underscore (e.g. "_NSArgumentDomain"), like function `sym`. */
   std::vector<std::string> data_shadow_syms;
   /* Symbols forced to be OBJECT data-shadows (info=0) WITHOUT a header decl.
    * Used for external data-pointer constants from frameworks we have no
    * headers for — most importantly an app's bundled PRIVATE frameworks
    * (RedRock, iLifeSlideshow, ...). Those export NSString or CF object
    * constants the translated i386 binary reads via the truncating
    * `movl slot,%reg; movl (%reg),%reg` double-deref, exactly like the
    * header-typed NS / CF constants, but they never enter the consider set
    * (it is built from SYSTEM framework exports) so handle_var_decl never
    * shadows them. Seeded generically from a binary's own un-shadowed data
    * imports (see _extract_data_shadows.sh). Stored WITH leading underscore. */
   std::vector<std::string> forced_object_shadows;
   /* Per-shadow kind/size, parallel to data_shadow_syms. 0 = ObjC-object /
    * opaque-CF pointer constant (runtime wraps the >4GB value as a handle).
    * 1/2/4/8 = SCALAR data constant (double/float/int/...): the runtime copies
    * that many value bytes into the shadow verbatim. A scalar extern is read
    * `movl slot,%reg` (-> &var) then dereferenced ONCE; binding the real 64-bit
    * &var truncates (NSAppKitVersionNumber: 0x7ff8_12f60850 -> 0x12f60850 ->
    * fault). The shadow is a low-4GB value copy so the single-deref reads it. */
   std::vector<unsigned> data_shadow_info;

   ABIGenerator(std::ostream& os, ABI abi): os(os), abi(abi) {}

   /* Build the reserved shim-name set from the final consider set. Must be
    * called after symbols/ignore are loaded and before any header is handled
    * (emit() consumes `symbols`, so compute from the full set up front). */
   void compute_collision_names() {
      const std::string override_prefix = "__";
      for (const std::string& s : symbols) {
         collision_names.insert(override_prefix + s);
      }
   }

   /* Footer: the runtime repair table for the capture-proof native slots
    * (see ABIConversion::needs_native_slot). One {name, slot} pair per
    * distinct slot-routed target. The table symbol is FIXED per pass
    * (__abiconv_nslot_tab0 = primary/modern, __abiconv_nslot_tab1 =
    * secondary/legacy) so nslot_repair.c can reference both directly; it
    * carries weak zero definitions that the strong tables here override, so
    * a link missing either .asm (e.g. no 10.6 SDK -> no legacy pass) still
    * works. Always emitted (possibly with count 0) for determinism. */
   void emit_native_slot_table() {
      const std::string tab = secondary_pass ? "__abiconv_nslot_tab1"
                                             : "__abiconv_nslot_tab0";
      os << "\tsection .data" << std::endl;
      for (size_t i = 0; i < slot_order.size(); ++i) {
         os << "__abicnslotname" << i << ":" << std::endl;
         os << "\tdb\t'" << slot_order[i] << "', 0" << std::endl;
      }
      os << "\talign 8" << std::endl;
      os << "\tglobal " << tab << std::endl;
      os << tab << ":" << std::endl;
      for (size_t i = 0; i < slot_order.size(); ++i) {
         os << "\tdq\t__abicnslotname" << i
            << ", __abicnslot" << slot_order[i] << std::endl;
      }
      os << "\tglobal " << tab << "_n" << std::endl;
      os << tab << "_n:" << std::endl;
      os << "\tdq\t" << slot_order.size() << std::endl;
      os << "\tsection .text" << std::endl;
   }

   ~ABIGenerator() {
      clang_disposeIndex(index);
   }

   void emit_header() {
      os << "\tsegment .text" << std::endl;
      os << "\textern __dyld_stub_binder_flag" << std::endl;
      /* objc-object return wrapping (see emit_body): a C function returning an
       * ObjC object hands back a 64-bit pointer that would truncate to the i386
       * caller's 4-byte eax. Wrap it into a low-4GB proxy handle the objc bridge
       * unwraps on the next message send. */
      os << "\textern _x64_objc_wrap" << std::endl;
      /* fn-ptr parameter bridging (typeconv convert_fnptr): binds an i386
       * callback to a native trampoline at shim time. cb_bridge.c. */
      os << "\textern _x64_cb_wrap" << std::endl;
      /* objc object/Class/SEL parameter marshalling (typeconv
       * convert_objc_ptr / convert_objc_sel; objc_shim.c) */
      os << "\textern _x64_objc_unwrap" << std::endl;
      os << "\textern _x64_objc_sel_unwrap" << std::endl;
      os << "\textern _x64_objc_sel_wrap" << std::endl;
      /* C-string return bounce (>4GB native string -> low-4GB copy; objc_shim.c) */
      os << "\textern _x64_cstr_ret_low" << std::endl;
      /* native-callee breadcrumb writer (objc_shim.c); gated, diagnostic. */
      if (gen_native_crumb())
         os << "\textern _abiconv_native_crumb" << std::endl;
   }

   void handle_file(const std::string& path) {
      std::vector<const char *> argv;
      argv.reserve(clang_args.size());
      for (const std::string& s : clang_args) {
         argv.push_back(s.c_str());
      }
      CXTranslationUnit unit = clang_parseTranslationUnit(
         index, path.c_str(),
         argv.empty() ? nullptr : argv.data(),
         static_cast<int>(argv.size()),
         nullptr, 0, CXTranslationUnit_None);
      if (unit == nullptr) {
         std::cerr << "Unable to parse translation unit. Quitting." << std::endl;
         exit(-1);
      }

      /* Surface parse diagnostics so missing-include problems aren't silent. */
      const unsigned ndiag = clang_getNumDiagnostics(unit);
      unsigned errors = 0;
      for (unsigned i = 0; i < ndiag; ++i) {
         CXDiagnostic d = clang_getDiagnostic(unit, i);
         if (clang_getDiagnosticSeverity(d) >= CXDiagnostic_Error) {
            CXString s = clang_formatDiagnostic(d, clang_defaultDiagnosticDisplayOptions());
            std::cerr << "abigen: " << clang_getCString(s) << std::endl;
            clang_disposeString(s);
            ++errors;
         }
         clang_disposeDiagnostic(d);
      }
      if (errors > 0) {
         std::cerr << "abigen: " << errors << " parse error(s) in " << path << std::endl;
      }

      for_each(unit, [&] (CXCursor c, CXCursor p) { return handle_cursor(c, p); });

      clang_disposeTranslationUnit(unit);
   }

   CXChildVisitResult handle_cursor(CXCursor c, CXCursor p) {
      switch (clang_getCursorKind(c)) {
      case CXCursor_FunctionDecl:
         handle_function_decl(c);
         break;
      case CXCursor_VarDecl:
         handle_var_decl(c);
         break;
      case CXCursor_AsmLabelAttr:
         handle_asm_label_attr(c, p);
         break;
      default:
         break;
      }
      return CXChildVisit_Recurse;
   }

   void handle_function_decl(CXCursor c) {
      const CXType t = clang_getCursorType(c);
      switch (t.kind) {
      case CXType_BlockPointer:
         return;
      default:
         break;
      }

      /* Only proper prototyped functions can be marshalled. K&R-style
       * declarations (CXType_FunctionNoProto), common in legacy framework
       * headers, have no argument-type info; constructing an ABIConversion
       * for them would trip the FunctionProto assertion and abort the whole
       * run. Skip them. This runs inside a libclang visitor, so any
       * exception from emit() must be caught here, not allowed to unwind
       * across the C callback frame. */
      if (t.kind != CXType_FunctionProto) {
         return;
      }

      try {
         std::unique_ptr<ABIConversion> conv(make_conv(c));
         conv->emit(os, symbols, ignore_structs, collision_names,
                    emitted_slots, slot_order);
      } catch (const std::exception& e) {
         std::cerr << "abigen: skipping function: " << e.what() << std::endl;
      }
   }

   void handle_asm_label_attr(CXCursor c, CXCursor p) {
      const std::string sym = to_string(c);
      if (sym.find('$') == std::string::npos) {
         return; /* this is something else */
      }
      if (clang_getCursorType(p).kind != CXType_FunctionProto) {
         return;
      }
      try {
         std::unique_ptr<ABIConversion> conv(make_conv(clang_getCursorType(p), sym));
         conv->emit(os, symbols, ignore_structs, collision_names,
                    emitted_slots, slot_order);
      } catch (const std::exception& e) {
         std::cerr << "abigen: skipping " << sym << ": " << e.what() << std::endl;
      }
   }

   /* External ObjC-object data constants. An i386 reference to e.g.
    * `NSString * const NSArgumentDomain` is a non-lazy symbol pointer the
    * linker fills with the symbol's 64-bit address; the i386 code then does
    * `movl slot,%reg; movl (%reg),%reg` (a double-deref: slot -> &var -> value).
    * Both the 64-bit &var AND the 64-bit object value overflow i386's 4-byte
    * loads, so we can't bind the real symbol. Instead we record the symbol and
    * emit (in emit_data_shadows) a low-4GB shadow variable ___SYM whose value
    * is a 32-bit proxy HANDLE wrapping the real object; static-interpose
    * redirects the binary's non-lazy bind for _SYM to ___SYM so the double-deref
    * yields the handle, which the objc bridge unwraps. We only shadow ObjC
    * object pointers (id, NSString *, Class), whose value is always an objc
    * object; a scalar/struct global mis-wrapped this way could corrupt it. */
   /* true if every field (recursively) is a 1/2/4-byte integer, an array of
    * such, or a nested record of such: same size and offsets on both ABIs. */
   static bool abi_neutral_type(CXType t) {
      t = clang_getCanonicalType(t);
      switch (t.kind) {
      case CXType_Bool: case CXType_Char_U: case CXType_UChar:
      case CXType_Char_S: case CXType_SChar: case CXType_Short:
      case CXType_UShort: case CXType_Int: case CXType_UInt:
         return true;
      case CXType_ConstantArray:
         return abi_neutral_type(clang_getArrayElementType(t));
      case CXType_Record:
         return abi_neutral_record(t);
      default:
         return false;
      }
   }
   static bool abi_neutral_record(CXType t) {
      bool ok = true;
      clang_Type_visitFields(t, [](CXCursor f, CXClientData d) {
         bool *okp = static_cast<bool *>(d);
         if (!abi_neutral_type(clang_getCursorType(f))) {
            *okp = false;
            return CXVisit_Break;
         }
         return CXVisit_Continue;
      }, &ok);
      return ok;
   }

   void handle_var_decl(CXCursor c) {
      /* secondary pass emits no data shadows (see secondary_pass) */
      if (secondary_pass) { return; }
      const enum CX_StorageClass sc = clang_Cursor_getStorageClass(c);
      if (sc != CX_SC_None && sc != CX_SC_Extern) {
         return; /* static / register / etc. — not an imported global */
      }
      const CXType canon = clang_getCanonicalType(clang_getCursorType(c));
      unsigned info = 0;   /* 0 = object/CF handle-wrap; else scalar byte size */
      switch (canon.kind) {
      case CXType_ObjCObjectPointer:
      case CXType_ObjCId:
      case CXType_ObjCClass:
         break;
      /* Scalar data constants (e.g. `double NSAppKitVersionNumber`). The i386
       * code loads &var via the non-lazy ptr then dereferences once; the real
       * 64-bit &var truncates on the 4-byte load. Shadow a low-4GB COPY of the
       * value (filled by x64_init_data_shadows). Only fixed-width scalars whose
       * value fits the 8-byte shadow slot; long double (16B) is excluded. */
      case CXType_Bool:
      case CXType_Char_U: case CXType_UChar:
      case CXType_Char_S: case CXType_SChar:
      case CXType_Short:  case CXType_UShort:
      case CXType_Int:    case CXType_UInt:
      case CXType_Long:   case CXType_ULong:
      case CXType_LongLong: case CXType_ULongLong:
      case CXType_Float:  case CXType_Double: {
         const long long sz = clang_Type_getSizeOf(canon);
         if (sz < 1 || sz > 8) { return; }
         info = (unsigned)sz;
         break;
      }
      case CXType_Pointer: {
         /* Opaque CoreFoundation refs (CFStringRef = `struct __CFString *`,
          * etc.) are 64-bit pointers to objects that live high in the dyld
          * region, so an i386 `movl slot,%reg; movl (%reg),%reg` double-deref
          * of such a data constant (e.g. kCFRunLoopDefaultMode) truncates and
          * faults — exactly like the objc case. Shadow them too, keyed on the
          * CF opaque-struct naming convention (`__CF*`). The runtime ctor
          * x64_init_data_shadows() wraps the >4GB value as a handle; toll-free
          * CF objects unwrap fine through the objc bridge. We do NOT shadow
          * void / char / scalar globals (mis-wrapping could corrupt them).
          * NOTE: a pure-C consumer of a CF string constant (e.g. the
          * CFRunLoopRunInMode mode arg) would get a handle, not a real
          * CFString -- acceptable for now (it was a hard crash before); the
          * proper fix is a low-4GB value-equal CFString copy. */
         const CXType pointee = clang_getCanonicalType(clang_getPointeeType(canon));
         if (pointee.kind != CXType_Record) { return; }
         CXString ps = clang_getTypeSpelling(pointee);
         const bool is_cf =
            std::string(clang_getCString(ps)).find("__CF") != std::string::npos;
         clang_disposeString(ps);
         if (!is_cf) { return; }
         break;
      }
      case CXType_Record: {
         /* Small struct-VALUE data constants (e.g. `const HIViewID
          * kHIViewWindowContentID` = {OSType signature; SInt32 id}, 8B). The
          * i386 code loads &var via the non-lazy ptr then reads its fields
          * (`movl slot,%reg; movl (%reg),%f0; movl 0x4(%reg),%f1`); the real
          * 64-bit &var truncates on the 4-byte load and the field read faults
          * (Civ EULADialog: kHIViewWindowContentID -> HIViewFindByID). The bits
          * ARE the value, so shadow a low-4GB COPY (x64_init_data_shadows
          * memcpy's `info` bytes, exactly like the scalar case). Only records
          * that fit the 8-byte shadow slot — larger toolbox structs would need
          * a wider shadow (none seen yet). REGRESSION-SAFE: such a global is
          * already broken-on-read (its high &var truncates), so no working path
          * is lost. Universal: triggers on "small record-value data constant",
          * not an app name. */
         const long long sz = clang_Type_getSizeOf(canon);
         /* Wider records too (e.g. `const struct in6_addr in6addr_any`, 16B),
          * but only when their layout is IDENTICAL on i386 and x86_64 —
          * 1/2/4-byte integer fields, arrays and nested records of those —
          * so the native bytes ARE the i386 value. (Quinn's AsyncSocket copied
          * 16 garbage bytes through a truncated &in6addr_any, the IPv6 bind
          * failed and the server never started.) */
         if (sz < 1 || sz > 64 || (sz > 8 && !abi_neutral_record(canon))) { return; }
         info = (unsigned)sz;
         break;
      }
      default:
         return; /* only objc-object + opaque-CF + small-record data constants */
      }
      CXString cxsym = clang_getCursorSpelling(c);
      const std::string sym = std::string("_") + clang_getCString(cxsym);
      clang_disposeString(cxsym);

      if (symbols.find(sym) == symbols.end()) {
         return; /* not in the consider set */
      }
      symbols.erase(sym); /* consume so a re-declaration isn't shadowed twice */
      if (getenv("ABIGEN_TRACE")) {
         std::cerr << "abigen: data shadow " << sym
                   << (info ? " (scalar " : " (object")
                   << (info ? std::to_string(info) + "B)" : ")") << std::endl;
      }
      data_shadow_syms.push_back(sym);
      data_shadow_info.push_back(info);
   }

   /* Append forced object data-shadows (header-less; see forced_object_shadows)
    * to the shadow set, skipping any a header already shadowed so the emitted
    * `__<sym>: dq 0` label is not duplicated (which would fail to assemble).
    * Run after all headers are processed and before emit_data_shadows. */
   void finalize_forced_shadows() {
      Symbols already(data_shadow_syms.begin(), data_shadow_syms.end());
      for (const std::string& sym : forced_object_shadows) {
         if (already.count(sym)) { continue; }
         already.insert(sym);
         data_shadow_syms.push_back(sym);
         data_shadow_info.push_back(0);   /* object / CF handle-wrap */
         if (getenv("ABIGEN_TRACE")) {
            std::cerr << "abigen: forced object data shadow " << sym << std::endl;
         }
      }

      /* Compiler-runtime SCALAR data symbols that no public header declares, so
       * handle_var_decl never sees them. The chief one is `___stack_chk_guard`:
       * stack-protected i386 functions read the canary via `movl slot,%reg;
       * movl (%reg),%reg`, where the slot is dyld-bound to libSystem's 64-bit
       * &__stack_chk_guard (high, in the shared cache). The i386 4-byte load
       * truncates that address (e.g. 0x00007ff8`4a89b350 -> 0x4a89b350) and the
       * second deref faults. Shadow it like any scalar extern: a low-4GB COPY of
       * the canary bytes that x64_init_data_shadows() fills via dlsym, so the
       * slot -> &shadow (low, fits) -> stable canary value. The exit-time
       * re-read hits the same shadow, so the consistency check still passes.
       * Generic: every i386 binary built with -fstack-protector imports this. */
      static const struct { const char *sym; unsigned size; } scalars[] = {
         { "___stack_chk_guard", 8 },
      };
      for (const auto& s : scalars) {
         if (already.count(s.sym)) { continue; }
         already.insert(s.sym);
         data_shadow_syms.push_back(s.sym);
         data_shadow_info.push_back(s.size);
         if (getenv("ABIGEN_TRACE")) {
            std::cerr << "abigen: forced scalar data shadow " << s.sym
                      << " (" << s.size << "B)" << std::endl;
         }
      }
   }

   /* Emit the data-shadow storage + the runtime table consumed by
    * x64_init_data_shadows() in objc_shim.c. Always emits the table/count
    * symbols (possibly empty) so the C side links. */
   void emit_data_shadows() {
      /* secondary pass: skip the whole table (including the singleton globals
       * _x64_data_shadows / _x64_data_shadows_count) so it does not collide with
       * the primary pass's. See secondary_pass. */
      if (secondary_pass) { return; }
      const std::string override_prefix = "__";
      os << "\n\tsegment .data" << std::endl;
      for (std::size_t i = 0; i < data_shadow_syms.size(); ++i) {
         const std::string shadow = override_prefix + data_shadow_syms[i];
         os << "\tglobal " << shadow << std::endl;
         os << "\talign 8" << std::endl;
         /* value slot sized to the datum (>= 8: object/CF handles, scalars) */
         const unsigned w = data_shadow_info[i] > 8 ? (data_shadow_info[i] + 7) / 8 : 1;
         os << shadow << ": times " << w << " dq 0" << std::endl;
      }
      std::size_t idx = 0;
      for (const std::string& s : data_shadow_syms) {
         /* dlsym wants the name without the leading underscore */
         os << "_x64_dsn_" << idx << ": db \"" << s.substr(1) << "\", 0"
            << std::endl;
         ++idx;
      }
      os << "\tglobal _x64_data_shadows" << std::endl;
      os << "_x64_data_shadows:" << std::endl;
      idx = 0;
      for (const std::string& s : data_shadow_syms) {
         const std::string shadow = override_prefix + s;
         /* triple: &shadow, name, info (0=object/CF wrap, else scalar size) */
         os << "\tdq " << shadow << std::endl;
         os << "\tdq _x64_dsn_" << idx << std::endl;
         os << "\tdq " << data_shadow_info[idx] << std::endl;
         ++idx;
      }
      os << "\tglobal _x64_data_shadows_count" << std::endl;
      os << "_x64_data_shadows_count: dq " << data_shadow_syms.size()
         << std::endl;
   }

   template <typename... Args>
   ABIConversion *make_conv(Args&&... args) const {
      switch (abi) {
      case ABI::FUNCTION:
         return new FunctionConversion(args...);
      case ABI::SYSCALL:
         return new SyscallConversion(args...);
      default: abort();
      }
   }
};

template <typename Op>
void parse_syms(std::istream& is, Op op) {
   std::string tmp;
   while (is >> tmp) {
      op(tmp);
   }
}

template <typename Op>
void parse_syms(const char *path, Op op) {
   std::ifstream is;
   is.open(path);
   parse_syms(is, op);
}

template <typename Op>
void parse_lines(const char *path, Op op) {
   std::ifstream is;
   is.open(path);
   std::string line;
   while (getline(is, line)) {
      op(line);
   }
}

int main(int argc, char *argv[]) {
   auto usage = [=] (FILE *f) {
                   const char *usage =
                      "usage: %s [option...] header...\n"               \
                      "Options:\n"                                      \
                      "  -h              print help dialog\n"           \
                      "  -o <path>       output asm file path (if omitted, defaults to stdin\n" \
                      "  -s <symfile>    file containing symbols to consider\n" \
                      "  -i <ignorefile> file containing symbols to ignore\n" \
                      "  -r <structfile> file containing struct names to not convert\n" \
                      "  --data-shadow-file <file>  symbols (one per line, leading\n" \
                      "                  underscore) to force as OBJECT data-shadows\n" \
                      "                  with no header decl (private-framework consts)\n" \
                      "  -c              use system call ABI\n"         \
                      "  -X <arg>        extra arg to forward to libclang (may repeat)\n" \
                      "  --isysroot <path>  shorthand for -X -isysroot -X <path>\n" \
                      "";
                   fprintf(f, usage, argv[0]);
                };

   const char *outpath = nullptr;
   const char *sympath = nullptr;
   const char *symignorepath = nullptr;
   const char *structpath = nullptr;
   const char *datashadowpath = nullptr;
   ABIGenerator::ABI abi = ABIGenerator::ABI::FUNCTION;
   std::vector<std::string> clang_args;
   const char *optstring = "ho:s:i:r:cX:";
   enum { OPT_ISYSROOT = 1000, OPT_DATASHADOW = 1001, OPT_SECONDARY = 1002 };
   const struct option longopts[] = {{"help", no_argument, nullptr, 'h'},
                                     {"output", required_argument, nullptr, 'o'},
                                     {"symfile", required_argument, nullptr, 's'},
                                     {"ignorefile", required_argument, nullptr, 'i'},
                                     {"structfile", required_argument, nullptr, 'r'},
                                     {"syscall", no_argument, nullptr, 'c'},
                                     {"clang-arg", required_argument, nullptr, 'X'},
                                     {"isysroot", required_argument, nullptr, OPT_ISYSROOT},
                                     {"data-shadow-file", required_argument, nullptr, OPT_DATASHADOW},
                                     {"secondary-pass", no_argument, nullptr, OPT_SECONDARY},
                                     {0}
   };

   bool secondary_pass = false;
   int optchar;
   while ((optchar = getopt_long(argc, argv, optstring, longopts, nullptr)) >= 0) {
      switch (optchar) {
      case 'h':
         usage(stdout);
         return 0;
      case 'o':
         outpath = optarg;
         break;
      case 's':
         sympath = optarg;
         break;
      case 'i':
         symignorepath = optarg;
         break;
      case 'r':
         structpath = optarg;
         break;
      case 'c':
         abi = ABIGenerator::ABI::SYSCALL;
         break;
      case 'X':
         clang_args.emplace_back(optarg);
         break;
      case OPT_ISYSROOT:
         clang_args.emplace_back("-isysroot");
         clang_args.emplace_back(optarg);
         break;
      case OPT_DATASHADOW:
         datashadowpath = optarg;
         break;
      case OPT_SECONDARY:
         secondary_pass = true;
         break;
      case '?':
         usage(stderr);
         return 1;
      }
   }

   std::ofstream of;
   if (outpath) {
      of.open(outpath);
   }
   std::ostream& os = outpath ? of : std::cout;

   ABIGenerator abigen(os, abi);
   abigen.clang_args = std::move(clang_args);
   abigen.secondary_pass = secondary_pass;

   if (sympath) {
      parse_syms(sympath, [&] (const std::string& s) { abigen.symbols.insert(s); });
   } else {
      parse_syms(std::cin, [&] (const std::string& s) { abigen.symbols.insert(s); });
   }
   
   if (symignorepath) {
      parse_syms(symignorepath, [&] (const std::string& s) { abigen.symbols.erase(s); });
   }

   if (structpath) {
      parse_lines(structpath, [&] (const std::string& s) { abigen.ignore_structs.insert(s); });
   }

   /* Header-less forced object data-shadows (one symbol per line, with the
    * leading underscore). Comment lines (#) and blanks are skipped. */
   if (datashadowpath) {
      parse_lines(datashadowpath, [&] (const std::string& line) {
         std::size_t a = line.find_first_not_of(" \t\r\n");
         if (a == std::string::npos || line[a] == '#') { return; }
         std::size_t b = line.find_last_not_of(" \t\r\n");
         abigen.forced_object_shadows.push_back(line.substr(a, b - a + 1));
      });
   }

   /* must run after the consider set is finalized (load + ignore) and before
    * any emit, which mutates abigen.symbols */
   abigen.compute_collision_names();

   abigen.emit_header();
   
   /* handle each header */
   for (int i = optind; i < argc; ++i) {
      abigen.handle_file(argv[i]);
   }

   /* append header-less forced object shadows, then emit external ObjC-object
    * data-constant shadows + their runtime table */
   abigen.finalize_forced_shadows();
   abigen.emit_data_shadows();

   /* emit the callback-signature descriptors registered by convert_fnptr */
   cb_sig_emit(os);

   /* emit the capture-proof native-slot repair table (see needs_native_slot) */
   abigen.emit_native_slot_table();

   return 0;
}
