#include <cassert>
#include <clang-c/Index.h>
#include <cstdint>
#include <map>
#include <sstream>
#include <vector>

#include "emit.hh"
#include "util.hh"
#include "typeinfo.hh"
#include "typeconv.hh"
#include "abigen.hh"

static size_t alignof_record(CXType type, arch a);

namespace {

   namespace {

      template <arch a, typename T_i386, typename T_x86_64>
      auto select_type_func() {
         if constexpr (a == arch::i386) {
               return T_i386();
            }
         if constexpr (a == arch::x86_64) {
               return T_x86_64();
            }
      }


   }

   template <arch a, typename T_i386, typename T_x86_64>
   using select_type = decltype(select_type_func<a, T_i386, T_x86_64>());
   
   template <arch a>
   using ul_t = select_type<a, uint32_t, uint64_t>;

   template <arch a>
   using l_t = select_type<a, int32_t, int64_t>;


}

record_decl::record_decl(CXCursor cursor): cursor(cursor) {
   populate_fields();
}

record_decl::record_decl(CXType type): cursor(clang_getTypeDeclaration(type)) {
   populate_fields();
}

/* See typeconv.hh. Walks the AS-WRITTEN typedef chain of a `long`-canonical
 * type for a "*32"-suffixed name (SInt32/UInt32 and everything built on them).
 * Single source of truth: abigen's byval machinery (byval_field_is_int32) and
 * record_decl::populate_fields both call this. */
bool written_is_fixed32_long(CXType written, CXTypeKind canon_kind) {
   if (canon_kind != CXType_Long && canon_kind != CXType_ULong) {
      return false;
   }
   CXType t = written;
   for (int depth = 0; depth < 8 && t.kind == CXType_Typedef; ++depth) {
      CXCursor d = clang_getTypeDeclaration(t);
      CXString ns = clang_getCursorSpelling(d);
      const char *cs = clang_getCString(ns);
      const std::string name(cs ? cs : "");
      clang_disposeString(ns);
      if (name.size() >= 2 && name.compare(name.size() - 2, 2, "32") == 0) {
         return true;
      }
      t = clang_getTypedefDeclUnderlyingType(d);
   }
   return false;
}

CXType effective_field_type(CXType canonical, CXType written) {
   if (written_is_fixed32_long(written, canonical.kind)) {
      CXType eff = canonical;
      eff.kind = (canonical.kind == CXType_ULong) ? CXType_UInt : CXType_Int;
      return eff;
   }
   return canonical;
}

void record_decl::populate_fields() {
   /* A struct may be FORWARD-DECLARED (opaque, no body) before its full
    * definition appears later in the translation unit — extremely common in
    * framework headers (e.g. `struct FSRef` is forward-declared in CFURL.h,
    * then defined as `{ UInt8 hidden[80]; }` in Files.h). clang_getTypeDeclaration
    * can hand us the forward-decl cursor, which has ZERO child FieldDecls, so we
    * computed sizeof()==0 and emitted an empty deep-copy. A native callee then
    * read/WROTE the real struct size (FSPathMakeRef writes 80 bytes for the FSRef
    * out-param) into our undersized bounce buffer, smashing the shim's saved rbp
    * and return address -> jmp to 0. Always lay out the DEFINITION cursor. */
   CXCursor def = clang_getCursorDefinition(cursor);
   if (!clang_Cursor_isNull(def)) {
      cursor = def;
   }
   for_each(cursor,
            [&] (CXCursor c, CXCursor p) {
               switch (clang_getCursorKind(c)) {
               case CXCursor_FieldDecl:
                  {
                     CXType written = clang_getCursorType(c);
                     /* Store the EFFECTIVE canonical type: a legacy fixed-32
                      * typedef field (SInt32/UInt32/OSType/... under the
                      * -arch i386 parse) is marshalled as Int/UInt (4 bytes on
                      * BOTH sides — the native framework was compiled from the
                      * modern headers where the same typedef is `int`), not
                      * the 8-byte native `long` its canonical kind implies.
                      * This feeds convert_record / sizeof_struct /
                      * alignof_record, so a by-pointer deep-copied record
                      * (AlertStdCFStringAlertParamRec, EventRecord, ...) gets
                      * the true native field offsets. See typeconv.hh. */
                     CXType canon = clang_getCanonicalType(written);
                     CXType type;
                     if (canon.kind == CXType_ConstantArray &&
                         written.kind == CXType_ConstantArray) {
                        /* Keep the WRITTEN array: its element retains the
                         * typedef sugar, so the typedef-aware sizeof_type /
                         * convert recursion applies the fixed-32 correction
                         * PER ELEMENT (MatrixRecord = Fixed[3][3] is 4-byte
                         * native elements, not the canonical-long 8). */
                        type = written;
                     } else {
                        type = effective_field_type(canon, written);
                     }
                     field_types.push_back(type);
                     field_types_written.push_back(written);
                  }
                  break;

               case CXCursor_PackedAttr:
                  packed = true;
                  break;

               case CXCursor_StructDecl:
               case CXCursor_UnionDecl:
                  /* nested anonymous record type declaration: ignored; the
                   * corresponding anonymous FieldDecl carries the layout */
                  break;

               case CXCursor_UnexposedAttr:
                  /* attribute on a field/record (availability, alignment hint,
                   * swift_name, etc.) that libclang doesn't expose. These do
                   * not affect the C field layout we compute, so ignore. */
                  break;

               default:
                  if (getenv("ABIGEN_DEBUG_MEMBERS")) {
                     auto ks = clang_getCursorKindSpelling(clang_getCursorKind(c));
                     std::cerr << "  member kind: " << clang_getCString(ks) << std::endl;
                     clang_disposeString(ks);
                  }
                  /* Any attribute cursor (aligned, objc_boxable, availability,
                   * deprecated, swift_name, ...) is a hint that does NOT change
                   * the C field layout libclang already computed for us — clang
                   * folds alignment into the type's offsets/size. Ignore them
                   * all, like CXCursor_UnexposedAttr above; otherwise a single
                   * `__attribute__((aligned))` on a member (e.g. FSCatalogInfo)
                   * made every function taking that struct unshimmable, so the
                   * i386 caller reached the native function unmarshalled. */
                  if (clang_isAttribute(clang_getCursorKind(c))) {
                     break;
                  }
                  /* This runs inside a libclang visitor callback (C frame),
                   * so we must not throw across it. Record the condition and
                   * let populate_fields() throw after the visit completes
                   * (e.g. C++ member functions, bitfields, or other members
                   * we can't lay out). */
                  unsupported = true;
                  break;
               }
               return CXChildVisit_Continue;
            });
   if (unsupported) {
      throw std::invalid_argument("record_decl: unsupported struct/union member");
   }
}

void conversion::push(std::ostream& os, const RegisterLocation& loc, Location& src, Location& dst) {
   src.push(); dst.push(); data.push();
   emit_inst(os, "push", loc.reg.reg_q);
}

void conversion::pop(std::ostream& os, const RegisterLocation& loc, Location& src, Location& dst) {
   src.pop();  dst.pop();  data.pop();
   emit_inst(os, "pop", loc.reg.reg_q);
}

void conversion::push(std::ostream& os, const SSELocation& loc, Location& src, Location& dst) {
   src.push(); dst.push(); data.push();
   emit_inst(os, "sub", "rsp", 8);
   emit_inst(os, "movsd", "[rsp]", loc.op(reg_width::Q));
}

void conversion::pop(std::ostream& os, const SSELocation& loc, Location& src, Location& dst) {
   src.pop(); dst.pop(); data.pop();
   emit_inst(os, "movsd", loc.op(reg_width::Q), "[rsp]");
   emit_inst(os, "add", "rsp", 8);
}

void conversion::convert_int(std::ostream& os, CXTypeKind type_kind, const Location& src,
                             const Location& dst) {
   reg_width from_width = get_type_width(type_kind, from_arch);
   reg_width to_width = get_type_width(type_kind, to_arch);
   
   if (src.kind() == Location::Kind::MEM && dst.kind() == Location::Kind::MEM) {
      const RegisterLocation tmp_reg(rax);
      MemoryLocation tmp_src = dynamic_cast<const MemoryLocation&>(src);
      MemoryLocation tmp_dst = dynamic_cast<const MemoryLocation&>(dst);
      push(os, tmp_reg, tmp_src, tmp_dst);
      convert_int(os, type_kind, tmp_src, tmp_reg);
      convert_int(os, type_kind, tmp_reg, tmp_dst);
      pop(os, tmp_reg, tmp_src, tmp_dst);
   } else if (dst.kind() == Location::Kind::MEM && get_type_signed(type_kind) &&
              from_width < to_width) {
      /* movsx qword <src>, dword <src>
       * mov <dst>, <src>
       */
      emit_inst(os, "movsx", src.op(to_width), src.op(from_width));
      emit_inst(os, "mov", dst.op(to_width), src.op(to_width));
   } else {
      const char *opcode;

      if (from_width < to_width) {
         if (get_type_signed(type_kind)) {
            opcode = "movsx";
         } else if (dst.kind() == Location::Kind::MEM) {
            /* Unsigned widen into MEMORY: the `mov r32` zero-extend trick only
             * clears the upper half when the destination is a REGISTER. Writing
             * dword to an 8-byte stack arg slot leaves its top 4 bytes garbage
             * -> a truncated pointer/unsigned-long arg passed on the stack (the
             * mem->mem case above bounces through rax, so src here is a register
             * already holding the zero-extended value). Store the full width.
             * (iPhoto: CGImageCreate's 9th arg, the `decode` pointer.) */
            opcode = "mov";
            from_width = to_width;
         } else {
            to_width = from_width;
            opcode = "mov";
         }
      } else {
         opcode = "mov";
         from_width = to_width;
      }

      emit_inst(os, opcode, dst.op(to_width), src.op(from_width));
   }
}

void conversion::convert_real(std::ostream& os, CXTypeKind type_kind, const Location& src,
                              const Location& dst) {
   if (src.kind() == Location::Kind::MEM && dst.kind() == Location::Kind::MEM) {
      SSELocation tmp_loc = {0};
      MemoryLocation tmp_src = dynamic_cast<const MemoryLocation&>(src);
      MemoryLocation tmp_dst = dynamic_cast<const MemoryLocation&>(dst);
      push(os, tmp_loc, tmp_src, tmp_dst);
      convert_real(os, type_kind, src, tmp_loc);
      convert_real(os, type_kind, tmp_loc, dst);
      pop(os, tmp_loc, tmp_src, tmp_dst);
   } else { 
      const reg_width to_width = get_type_width(type_kind, to_arch);
      const reg_width from_width = get_type_width(type_kind, from_arch);
      std::stringstream opcode;
      if (to_width == from_width) {
         opcode << "mov" << reg_width_to_sse(to_width);
      } else {
         opcode << "cvt" << reg_width_to_sse(from_width) << "2" << reg_width_to_sse(to_width);
      }
      emit_inst(os, opcode.str(), dst.op(to_width), src.op(from_width));
   }
}

void conversion::convert_void_pointer(std::ostream& os, const Location& src,
                                      const Location& dst) {
   convert_int(os, CXType_Pointer, src, dst);
}

void conversion::convert(std::ostream& os, CXType type, const Location& src, const Location& dst) {
   os << "\t; convert '" << to_string(type) << "'" << std::endl;
   switch (type.kind) {
   case CXType_Invalid:
   case CXType_Unexposed:
      throw std::invalid_argument("invalid type");
      
   case CXType_Void:
      return;
      
   case CXType_Bool:
   case CXType_UChar:
   case CXType_Char_U:
   case CXType_UShort:
   case CXType_UInt:
   case CXType_ULong:
   case CXType_ULongLong:
   case CXType_SChar:
   case CXType_Char_S:
   case CXType_Short:
   case CXType_Int:
   case CXType_Long:
   case CXType_LongLong:
   case CXType_Enum:
      convert_int(os, type.kind, src, dst);
      break;
      
   case CXType_Float:
   case CXType_Double:
   case CXType_LongDouble:
      convert_real(os, type.kind, src, dst);
      return;
      
   case CXType_Pointer:
      convert_pointer(os, clang_getPointeeType(type), src, dst);
      break;
      
   case CXType_BlockPointer:
      // TODO
      convert_int(os, CXType_Pointer, src, dst);
      break;

   case CXType_ObjCObjectPointer:
   case CXType_ObjCId:
   case CXType_ObjCClass:
      convert_objc_ptr(os, src, dst);
      break;

   case CXType_ObjCSel:
      convert_objc_sel(os, src, dst);
      break;

   case CXType_ConstantArray:
      convert_constant_array(os, type,
                             dynamic_cast<const MemoryLocation&>(src),
                             dynamic_cast<const MemoryLocation&>(dst));
      break;
      
   case CXType_Record:
      convert_record(os, type,
                     dynamic_cast<const MemoryLocation&>(src),
                     dynamic_cast<const MemoryLocation&>(dst));
      break;

   case CXType_Typedef:
      /* Sugared constant-array ELEMENT (see populate_fields: a written array
       * is kept so its element retains the typedef name). Convert as the
       * effective canonical — fixed-32 corrected (Fixed -> Int). */
      convert(os, effective_field_type(clang_getCanonicalType(type), type),
              src, dst);
      break;

   case CXType_FunctionProto:
      break;

   default:
      throw std::invalid_argument("conversion::convert: unsupported type kind");
   }
}

void conversion::convert_constant_array(std::ostream& os, CXType array, MemoryLocation src,
                                        MemoryLocation dst) {
   /* convert data */
   const long long arrlen = clang_getArraySize(array);
   assert(arrlen >= 1);
   const CXType elem = clang_getArrayElementType(array);
   const std::string loop = label();
   
   /*   push rcx
    *   push rdi
    *   push rsi
    *   mov ecx, <arrlen>
    *   lea rdi, [<dst>]
    *   lea rsi, [<src>]
    * loop:
    *   <move>
    *   add rdi, <size64>
    *   add rsi, <size32>
    * entry:
    *   dec ecx
    *   jnz loop
    *   pop rsi
    *   pop rdi
    *   pop rcx
    */

   push(os, rcx, src, dst);
   push(os, rdi, src, dst);
   push(os, rsi, src, dst);
   
   emit_inst(os, "mov", "ecx", arrlen);
   emit_inst(os, "lea", "rdi", dst.op());
   emit_inst(os, "lea", "rsi", src.op());

   os << loop << ":";
   {
      MemoryLocation src(rsi, 0);
      MemoryLocation dst(rdi, 0);
      
      convert(os, elem, src, dst);
      emit_inst(os, "add", "rdi", sizeof_type(elem, arch::x86_64));
      emit_inst(os, "add", "rsi", sizeof_type(elem, arch::i386));
      emit_inst(os, "dec", "ecx");
      emit_inst(os, "jnz", loop);
   }

   pop(os, rsi, src, dst);
   pop(os, rdi, src, dst);
   pop(os, rcx, src, dst);
}

/* True if `written` is a classic Mac OS Memory Manager Handle (or the
 * AEDataStorage / opaque-Ptr* family): a typedef whose canonical type is a
 * pointer-to-pointer (T**) AND whose typedef spelling names it a Handle. The
 * app treats such a value as an OPAQUE token — a pointer to a relocatable
 * master pointer — so a marshalling shim must pass the pointer VALUE (i386
 * 4-byte low-4GB handle -> zero-extended to 8 bytes), NEVER dereference it.
 *
 * WHY (Civ IV 'oapp'-launch crash): AEDesc { DescType descriptorType;
 * AEDataStorage dataHandle; } where AEDataStorage = Ptr* = char**.
 * cb_is_cf_record_ptr does NOT catch it — `char` is a COMPLETE type, unlike the
 * incomplete OpaqueXxx* records that Component instances / GWorld handles use —
 * so convert_record deep-copied the field: convert_pointer(char**) dereferences
 * the handle to read the master char*, and the handle is an opaque token, not a
 * readable data pointer -> EXC_BAD_ACCESS (movl (%r12),%r14d, r12=garbage).
 * Structural + name gated so a genuine `T** out` parameter (never spelled
 * *Handle) is still deep-copied as a real out-pointer. Universal: any i386 app
 * passing a Handle-bearing struct to a native-marshalled Toolbox function
 * (AppleEvent 'oapp'/'odoc', Resource Manager, ...) is served. */
bool is_opaque_handle_type(CXType written) {
   CXType canon = clang_getCanonicalType(written);
   if (canon.kind != CXType_Pointer) { return false; }
   CXType pointee = clang_getCanonicalType(clang_getPointeeType(canon));
   if (pointee.kind != CXType_Pointer) { return false; }   /* must be T** */
   CXType t = written;
   for (int depth = 0; depth < 8 && t.kind == CXType_Typedef; ++depth) {
      CXCursor d = clang_getTypeDeclaration(t);
      CXString ns = clang_getCursorSpelling(d);
      const char *cs = clang_getCString(ns);
      const std::string name(cs ? cs : "");
      clang_disposeString(ns);
      if (name == "Handle" || name == "AEDataStorage" ||
          (name.size() >= 6 && name.compare(name.size() - 6, 6, "Handle") == 0)) {
         return true;
      }
      t = clang_getTypedefDeclUnderlyingType(d);
   }
   return false;
}

void conversion::convert_record(std::ostream& os, CXType record, MemoryLocation src,
                                MemoryLocation dst) {
   record_decl decl(record);
   /* Unions can't be converted field-by-field (members overlap; which one is
    * live is unknown). Skip the whole function rather than mislaying it. */
   if (decl.cursor.kind != CXCursor_StructDecl) {
      throw std::invalid_argument("convert_record: union by value not supported");
   }
   /* #pragma pack / __attribute__((packed)) cap: a packed struct's fields sit at
    * TIGHTER offsets than natural alignment (classic Carbon AppleEvent structs are
    * pack(2): AEDesc is {DescType@0; AEDataStorage dataHandle@+4}, sizeof 12 — the
    * Handle at +4, NOT the natural +8). clang folds the pragma into the record's
    * alignment, so clang_Type_getAlignOf gives the effective pack cap (2 for
    * AEDesc; the natural max-field-align for an unpacked struct, which caps
    * NOTHING since every field's align is already <= it). Building the native
    * x86_64 record with natural alignment writes dataHandle at +8 -> the native
    * AE callee reads it at +4, straddling two fields = a garbage handle pointer
    * (Civ 'oapp' AEGetParamDesc EXC_BAD_ACCESS at addr 0x..._00000008). The SAME
    * cap governs the i386 side (pack is arch-independent), applied after
    * align_field's Darwin-i386 8->4 clamp. */
   const long long rec_align = clang_Type_getAlignOf(record);
   const size_t pack_cap = (rec_align > 0) ? static_cast<size_t>(rec_align) : 0;
   /* field_types (canonical) and field_types_written are populated in lockstep. */
   auto wi = decl.field_types_written.begin();
   for (CXType field_type : decl.field_types) {
      const CXType written =
         (wi != decl.field_types_written.end()) ? *wi : field_type;
      src.align_field(field_type, from_arch, pack_cap);
      dst.align_field(field_type, to_arch, pack_cap);

      if (is_opaque_handle_type(written)) {
         /* Opaque Memory Manager Handle field: marshal the pointer VALUE, do
          * NOT deep-copy/dereference it (see is_opaque_handle_type). */
         os << "\t; opaque Handle field '" << to_string(written)
            << "' -> pointer value (no deep-copy)" << std::endl;
         convert_int(os, CXType_Pointer, src, dst);
      } else {
         convert(os, field_type, src, dst);
      }

#if 0
      std::cerr << to_string(record) << "," << to_string(field_type) << "," << src.index
                << "," << dst.index << std::endl;
#endif

      /* update src, dst */
      src += sizeof_type(field_type, from_arch);
      dst += sizeof_type(field_type, to_arch);
      if (wi != decl.field_types_written.end()) { ++wi; }
   }
}

/* ---- callback-signature registry (fn-ptr parameter bridging) ----
 *
 * A fn-ptr parameter's value is an i386 code address the native API will
 * eventually CALL with the x86_64 ABI. The shim therefore can't pass it (or
 * any deep copy) through: it must substitute a NATIVE trampoline bound to the
 * i386 callback. The marshalling the trampoline must perform is fully
 * determined by the callback's prototype, which we have here from the
 * header — encode it as a descriptor blob the runtime dispatcher
 * (cb_bridge.c) interprets per invocation. Codes must match cb_bridge.c. */

namespace {

   enum cb_arg_code : uint8_t {
      CBA_I32 = 0,   /* int-domain arg, low 32 bits -> 1 word              */
      CBA_I64 = 1,   /* long long -> 2 words (lo, hi)                      */
      CBA_PTR = 2,   /* pointer, truncate (process heap is low-4GB)        */
      CBA_OBJ = 3,   /* objc/CF object ptr: wrap via x64_objc_wrap if high */
      CBA_F32 = 4,   /* float (xmm low 4 bytes) -> 1 word                  */
      CBA_F64 = 5,   /* double -> 2 words                                  */
   };
   enum cb_ret_code : uint32_t {
      CBR_VOID  = 0,
      CBR_I32   = 1, /* eax, zero-extended                                 */
      CBR_PTR   = 2, /* eax, zero-extended                                 */
      CBR_OBJ   = 3, /* eax; arena handle -> unwrap to the real object     */
      CBR_I32SX = 4, /* eax sign-extended (i386 long -> native 64-bit long)*/
      CBR_I64   = 5, /* i386 edx:eax -> native rax (long long: funopen     */
                     /* seekfn/fpos_t, CGDataProvider skipForward/off_t)   */
   };
   constexpr unsigned CB_MAX_ARGS = 16; /* x64_cb_sig.arg_kinds[] capacity */

   struct cb_sig {
      uint32_t ret_kind;
      std::vector<uint8_t> arg_kinds;
   };
   std::vector<cb_sig> cb_sigs;
   std::map<std::string, unsigned> cb_sig_dedupe;

   bool cb_is_cf_record_ptr(CXType pointee_canon) {
      if (pointee_canon.kind != CXType_Record) { return false; }
      /* An OPAQUE (incomplete, forward-declared) record is the universal shape
       * of a CF/CG/IOKit *Ref handle type: CFStringRef = struct __CFString *,
       * CGColorSpaceRef = struct CGColorSpace *, CGContextRef, IOHIDDeviceRef,
       * &c. Such a pointer value is a low-4GB arena handle that must be
       * unwrapped to the real 64-bit ref before crossing into a native API.
       * A COMPLETE struct pointer (CGRect *, NSRect *) is a by-value record and
       * must NOT be treated as a handle. The old "__CF" name match only caught
       * CoreFoundation's own opaque types and missed all of CoreGraphics, so
       * a CGColorSpaceRef reached native CGBitmapContextCreate raw (iPhoto). */
      if (clang_Type_getSizeOf(pointee_canon) == CXTypeLayoutError_Incomplete) {
         return true;
      }
      CXString s = clang_getTypeSpelling(pointee_canon);
      const bool cf = std::string(clang_getCString(s)).find("__CF") != std::string::npos;
      clang_disposeString(s);
      return cf;
   }

   /* True if the record (recursively) contains NO floating-point field, so every
    * SysV eightbyte is INTEGER-class and the whole aggregate travels in GP
    * register(s)/stack — never xmm. A small (<=8B) such struct is thus passed by
    * x86_64 in a SINGLE GP register, byte-identical to a 32/64-bit integer, which
    * is exactly how the callback dispatcher already marshals CBA_I32/CBA_I64.
    * An FP-bearing (possibly SSE) struct would arrive in xmm and is left
    * unsupported (skips to the raw-pointer fallback). */
   bool cb_record_all_int(CXType record_canon) {
      record_decl decl(record_canon);
      for (CXType f : decl.field_types) {
         const CXType c = clang_getCanonicalType(f);
         switch (c.kind) {
         case CXType_Float: case CXType_Double: case CXType_LongDouble:
            return false;
         case CXType_Record:
            if (!cb_record_all_int(c)) { return false; }
            break;
         case CXType_ConstantArray: {
            const CXType el = clang_getCanonicalType(clang_getArrayElementType(c));
            if (el.kind == CXType_Float || el.kind == CXType_Double ||
                el.kind == CXType_LongDouble) { return false; }
            if (el.kind == CXType_Record && !cb_record_all_int(el)) { return false; }
            break;
         }
         default:
            break;
         }
      }
      return true;
   }

   uint8_t cb_arg_code_for(CXType t) {
      t = clang_getCanonicalType(t);
      switch (t.kind) {
      case CXType_Bool:
      case CXType_Char_U:
      case CXType_UChar:
      case CXType_Char_S:
      case CXType_SChar:
      case CXType_UShort:
      case CXType_Short:
      case CXType_UInt:
      case CXType_Int:
      case CXType_Enum:
      /* native long is 64-bit; the i386 callback's long is the low 4 bytes */
      case CXType_ULong:
      case CXType_Long:
         return CBA_I32;
      case CXType_ULongLong:
      case CXType_LongLong:
         return CBA_I64;
      case CXType_Float:
         return CBA_F32;
      case CXType_Double:
         return CBA_F64;
      case CXType_ObjCObjectPointer:
      case CXType_ObjCId:
      case CXType_ObjCClass:
         return CBA_OBJ;
      case CXType_Pointer:
         return cb_is_cf_record_ptr(clang_getCanonicalType(clang_getPointeeType(t)))
            ? CBA_OBJ : CBA_PTR;
      case CXType_BlockPointer:
      case CXType_ConstantArray:
      case CXType_IncompleteArray:
         return CBA_PTR;
      case CXType_Record: {
         /* small by-VALUE struct callback arg (CGScreenUpdateMoveDelta
          * {int32,int32}, classic Carbon Point {short,short}, ...). An
          * all-integer aggregate <=8 bytes is ONE SysV eightbyte, so x86_64
          * passes it in a single GP register byte-identical to a 32/64-bit int,
          * and the i386 callback receives its 1 or 2 by-value words on the stack
          * — exactly the CBA_I32 / CBA_I64 marshalling the dispatcher already
          * performs (read one GP reg, emit 1 word if <=4 bytes else 2). No new
          * descriptor code / dispatcher change. A >8B or FP-bearing (SSE) struct
          * would span multiple regs / arrive in xmm; leave those unsupported. */
         const long long sz = clang_Type_getSizeOf(t);
         if (sz >= 1 && sz <= 8 && cb_record_all_int(t)) {
            return sz <= 4 ? CBA_I32 : CBA_I64;
         }
         throw std::invalid_argument(
            "callback arg type unsupported: " + to_string(t));
      }
      default:
         throw std::invalid_argument("callback arg type unsupported: " + to_string(t));
      }
   }

   uint32_t cb_ret_code_for(CXType t) {
      t = clang_getCanonicalType(t);
      switch (t.kind) {
      case CXType_Void:
         return CBR_VOID;
      case CXType_Bool:
      case CXType_Char_U:
      case CXType_UChar:
      case CXType_Char_S:
      case CXType_SChar:
      case CXType_UShort:
      case CXType_Short:
      case CXType_UInt:
      case CXType_Int:
      case CXType_Enum:
      case CXType_ULong:
         return CBR_I32;
      case CXType_Long: /* CFIndex comparators: native caller reads all of rax */
         return CBR_I32SX;
      case CXType_ULongLong:
      case CXType_LongLong: /* i386 edx:eax -> native rax (funopen fpos_t seekfn,
                             * CGDataProvider off_t skipForward) */
         return CBR_I64;
      case CXType_ObjCObjectPointer:
      case CXType_ObjCId:
      case CXType_ObjCClass:
         return CBR_OBJ;
      case CXType_Pointer:
         return cb_is_cf_record_ptr(clang_getCanonicalType(clang_getPointeeType(t)))
            ? CBR_OBJ : CBR_PTR;
      default:
         /* long long (eax:edx) and x87 FP returns aren't captured by
          * _86x64_call_i386 */
         throw std::invalid_argument("callback return type unsupported: " + to_string(t));
      }
   }

}

/* exported: is this canonical type an opaque-CF-ref pointer (CFStringRef &c)?
 * Used by abigen's return-value wrapping. */
bool cf_opaque_ptr_type(CXType canon) {
   if (canon.kind != CXType_Pointer) { return false; }
   return cb_is_cf_record_ptr(
      clang_getCanonicalType(clang_getPointeeType(canon)));
}

/* exported: CFTypeRef / CFPropertyListRef (void*-backed CF object-ref typedefs)
 * detected by the AS-WRITTEN typedef name, since their canonical type is a
 * bare const void* that can't be told apart from a non-object void* arg. A
 * plain void* (data/context pointer) keeps its raw pass-through. */
bool cf_void_ref_type(CXType orig) {
   CXType canon = clang_getCanonicalType(orig);
   if (canon.kind != CXType_Pointer) { return false; }
   if (clang_getCanonicalType(clang_getPointeeType(canon)).kind != CXType_Void) {
      return false;
   }
   CXString s = clang_getTypeSpelling(orig);
   const std::string name(clang_getCString(s));
   clang_disposeString(s);
   return name.find("CFTypeRef") != std::string::npos ||
          name.find("CFPropertyListRef") != std::string::npos;
}

unsigned cb_sig_register(CXType fnproto) {
   if (fnproto.kind != CXType_FunctionProto) {
      throw std::invalid_argument("callback has no prototype");
   }
   if (clang_isFunctionTypeVariadic(fnproto)) {
      throw std::invalid_argument("variadic callback");
   }
   const int nargs = clang_getNumArgTypes(fnproto);
   if (nargs < 0 || nargs > (int)CB_MAX_ARGS) {
      throw std::invalid_argument("callback has too many args");
   }

   cb_sig sig;
   sig.ret_kind = cb_ret_code_for(clang_getResultType(fnproto));
   for (int i = 0; i < nargs; ++i) {
      sig.arg_kinds.push_back(cb_arg_code_for(clang_getArgType(fnproto, i)));
   }

   std::string key = std::to_string(sig.ret_kind) + ":";
   for (uint8_t k : sig.arg_kinds) { key += (char)('0' + k); }
   auto it = cb_sig_dedupe.find(key);
   if (it != cb_sig_dedupe.end()) { return it->second; }

   const unsigned idx = cb_sigs.size();
   cb_sigs.push_back(std::move(sig));
   cb_sig_dedupe.emplace(std::move(key), idx);
   return idx;
}

void cb_sig_emit(std::ostream& os) {
   os << "\n\tsegment .data" << std::endl;
   os << "\t; callback-signature descriptors (struct x64_cb_sig, cb_bridge.c)"
      << std::endl;
   for (unsigned i = 0; i < cb_sigs.size(); ++i) {
      const cb_sig& sig = cb_sigs[i];
      os << "_x64_cbsig_" << i << ":" << std::endl;
      os << "\tdd " << sig.arg_kinds.size() << std::endl;
      os << "\tdd " << sig.ret_kind << std::endl;
      os << "\tdb ";
      for (unsigned k = 0; k < CB_MAX_ARGS; ++k) {
         if (k) { os << ", "; }
         os << (k < sig.arg_kinds.size() ? (unsigned)sig.arg_kinds[k] : 0u);
      }
      os << std::endl;
   }
}

/* Emit a mid-conversion-stream call to a one-argument libabiconv runtime
 * helper: r14 <- src; rdi <- r14 (+ optional rsi operand); r14 <- callee(...);
 * dst <- r14. Conversion streams run with x86_64 arg registers (and, inside a
 * record deep copy, the r11/r12 pointee bases) holding live values: save
 * every caller-saved GP reg + xmm0-7 around the call, keep values in r13/r14
 * (callee-saved, unused by the conversion machinery), and only touch
 * rsp-relative src/dst OUTSIDE the pushed region (src is read into r14
 * before the pushes; dst is written after the pops). */
void conversion::emit_runtime_bridge_call(std::ostream& os, const char *callee,
                                          const std::string& rsi_operand,
                                          const Location& src, reg_width src_width,
                                          const Location& dst, reg_width dst_width) {
   emit_inst(os, "mov", src_width == reg_width::Q ? "r14" : "r14d",
             src.op(src_width));

   static const char *saves[] = {"rax", "rcx", "rdx", "rsi", "rdi",
                                 "r8", "r9", "r10", "r11", "r12"};
   for (const char *r : saves) { emit_inst(os, "push", r); }
   emit_inst(os, "sub", "rsp", 64);
   for (int k = 0; k < 8; ++k) {
      emit_inst(os, "movsd", "[rsp + " + std::to_string(k * 8) + "]",
                "xmm" + std::to_string(k));
   }

   emit_inst(os, "mov", "rdi", "r14");
   if (!rsi_operand.empty()) {
      emit_inst(os, "lea", "rsi", rsi_operand);
   }
   emit_inst(os, "mov", "r13", "rsp");
   emit_inst(os, "and", "rsp", "~0xf");
   emit_inst(os, "call", callee);
   emit_inst(os, "mov", "rsp", "r13");
   emit_inst(os, "mov", "r14", "rax");

   for (int k = 0; k < 8; ++k) {
      emit_inst(os, "movsd", "xmm" + std::to_string(k),
                "[rsp + " + std::to_string(k * 8) + "]");
   }
   emit_inst(os, "add", "rsp", 64);
   for (int i = sizeof(saves) / sizeof(saves[0]); i-- > 0; ) {
      emit_inst(os, "pop", saves[i]);
   }

   emit_inst(os, "mov", dst.op(dst_width),
             dst_width == reg_width::Q ? "r14" : "r14d");
}

/* A fn-ptr parameter (i386 -> x86_64 direction): substitute a native
 * trampoline bound to the i386 callback via the runtime x64_cb_wrap. */
void conversion::convert_fnptr(std::ostream& os, CXType pointee, const Location& src,
                               const Location& dst) {
   if (!(from_arch == arch::i386 && to_arch == arch::x86_64)) {
      /* copy-back direction: a callback pointer has no reverse conversion;
       * leave the i386 caller's value untouched. */
      return;
   }

   unsigned idx;
   try {
      idx = cb_sig_register(pointee);
   } catch (const std::exception& e) {
      std::cerr << "abigen: fn-ptr arg '" << to_string(pointee) << "': " << e.what()
                << "; passing raw value" << std::endl;
      /* keep the emitted symbol set identical to pre-callback-bridge builds
       * (already-interposed binaries reference this shim by name) */
      convert_int(os, CXType_Pointer, src, dst);
      return;
   }

   os << "\t; wrap i386 fn ptr in native callback trampoline (sig " << idx << ")"
      << std::endl;
   emit_runtime_bridge_call(os, "_x64_cb_wrap",
                            "[rel _x64_cbsig_" + std::to_string(idx) + "]",
                            src, reg_width::D, dst, reg_width::Q);
}

/* An ObjC object / Class parameter. i386 -> x86_64: the i386 value can be a
 * low-4GB proxy-arena HANDLE (data-symbol shadows, wrapped returns), an i386
 * legacy object/class, a shadow, an i386 block, OR an i386 CFConstantString
 * @"..." constant whose isa is a wrapped proxy handle. Resolve them all with
 * the SAME full resolver the objc_msgSend forward bridge uses for `@`/`#` args
 * (_86x64_unwrap_obj_arg / unwrap_obj_arg) — the arena-only weak x64_objc_unwrap
 * left non-arena values (e.g. a constant @"...") RAW, so native code read the
 * wrapped-handle isa as a Class -> "unknown class 0x800xxxxx". x86_64 -> i386
 * (deep-copy-back of out-params like NSError**): wrap the real 64-bit object
 * into a handle the bridge unwraps on the next message send (x64_objc_wrap
 * dedupes, so handles are stable per object and equality survives). */
void conversion::convert_objc_ptr(std::ostream& os, const Location& src,
                                  const Location& dst) {
   if (from_arch == arch::i386 && to_arch == arch::x86_64) {
      os << "\t; objc object arg: resolve handle/legacy/cfstr/shadow -> real object"
         << std::endl;
      os << "\textern __86x64_unwrap_obj_arg" << std::endl;
      emit_runtime_bridge_call(os, "__86x64_unwrap_obj_arg", "",
                               src, reg_width::D, dst, reg_width::Q);
   } else {
      os << "\t; objc object copy-back: wrap real object -> handle" << std::endl;
      emit_runtime_bridge_call(os, "_x64_objc_wrap", "",
                               src, reg_width::Q, dst, reg_width::D);
   }
}

/* An opaque CF-ref parameter (CFStringRef etc.). Forward: resolve the i386 ref
 * to its real native ref with the FULL object resolver (_86x64_unwrap_obj_arg)
 * — proxy-arena handles, AND i386/x86_64 CFConstantString @"..." constants
 * whose isa is a wrapped proxy handle (the data-shadow of
 * ___CFConstantStringClassReference). The old arena-only x64_objc_unwrap passed
 * such a constant RAW into native CF, which read the wrapped handle as the
 * object's Class -> "unknown class 0x800xxxxx" / __CF_IS_OBJC __builtin_trap
 * (Halo CFURLCreateCopyAppendingPathComponent, Civ IV CFStringCreateCopy).
 * i386_cfstr_to_real keys on the constant's flags+exact-strlen layout, never
 * the isa, so a genuine low native ref still passes through unchanged.
 * Copy-back: only re-wrap values above 4GB (a dyld-cache constant the i386
 * slot can't hold); low heap refs are stored raw as they always were —
 * wrapping every created ref would churn arena slots for nothing. */
void conversion::convert_cf_ptr(std::ostream& os, const Location& src,
                                const Location& dst) {
   if (from_arch == arch::i386 && to_arch == arch::x86_64) {
      os << "\t; CF ref arg: resolve handle/cfstr-constant -> real ref" << std::endl;
      os << "\textern __86x64_unwrap_obj_arg" << std::endl;
      emit_runtime_bridge_call(os, "__86x64_unwrap_obj_arg", "",
                               src, reg_width::D, dst, reg_width::Q);
   } else {
      os << "\t; CF ref copy-back: wrap only if >4GB" << std::endl;
      const std::string low_lbl = label();
      emit_inst(os, "mov", "r14", src.op(reg_width::Q));
      emit_inst(os, "mov", "r13", "r14");
      emit_inst(os, "shr", "r13", "32");
      emit_inst(os, "jz", low_lbl);
      emit_runtime_bridge_call(os, "_x64_objc_wrap", "",
                               src, reg_width::Q, dst, reg_width::D);
      const std::string done_lbl = label();
      emit_inst(os, "jmp", done_lbl);
      os << low_lbl << ":" << std::endl;
      emit_inst(os, "mov", dst.op(reg_width::D), "r14d");
      os << done_lbl << ":" << std::endl;
   }
}

/* A SEL parameter: i386 SELs are low selector-name pointers; register them
 * with the modern runtime going in, intern a stable low name copy coming
 * back (objc_shim.c x64_objc_sel_unwrap/_wrap). */
void conversion::convert_objc_sel(std::ostream& os, const Location& src,
                                  const Location& dst) {
   if (from_arch == arch::i386 && to_arch == arch::x86_64) {
      os << "\t; objc SEL arg: register legacy name -> real SEL" << std::endl;
      emit_runtime_bridge_call(os, "_x64_objc_sel_unwrap", "",
                               src, reg_width::D, dst, reg_width::Q);
   } else {
      os << "\t; objc SEL copy-back: real SEL -> low name ptr" << std::endl;
      emit_runtime_bridge_call(os, "_x64_objc_sel_wrap", "",
                               src, reg_width::Q, dst, reg_width::D);
   }
}

/* A pointer whose pointee is a fixed-width scalar has IDENTICAL element layout
 * in i386 and x86_64, so the pointer needs no deep copy — it can be passed
 * straight through (the i386 pointer is always low-4GB, hence a valid native
 * pointer; native code reads/writes the caller's buffer directly, in or out).
 * EXCLUDED: long/unsigned long (4 vs 8 bytes) and long double (12 vs 16) widen
 * across the ABIs and still need the bounce-buffer conversion, as do structs,
 * pointers, and arrays of any of those. */
static bool pointee_is_passthrough_scalar(CXType canon) {
   switch (canon.kind) {
   case CXType_Bool:
   case CXType_UChar:  case CXType_Char_U:
   case CXType_SChar:  case CXType_Char_S:
   case CXType_UShort: case CXType_Short:
   case CXType_UInt:   case CXType_Int:   case CXType_Enum:
   case CXType_Float:  case CXType_Double:
   case CXType_ULongLong: case CXType_LongLong:
      return true;
   default:
      return false;
   }
}

/* A pointer whose pointee is a RECORD with IDENTICAL memory layout under both
 * ABIs can likewise pass straight through. Layout is identical iff every leaf
 * field (recursing through nested records, unions and fixed-size arrays) is a
 * fixed-width scalar of size <= 4 (bool/char/short/int/enum/float): those have
 * the same size AND natural alignment (<= 4) on both ABIs, so every field
 * offset and the total size coincide. 8-byte scalars (double/long long) are
 * same-SIZE but i386 packs them 4-aligned inside structs where x86_64 uses
 * 8-aligned -> offsets can differ -> excluded. long/pointer fields change size
 * entirely -> excluded (those keep the bounce-buffer deep copy).
 *
 * This matters beyond economy: the deep copy stages exactly ONE element, so an
 * ARRAY arg is truncated to its first entry (InstallEventHandler's
 * `const EventTypeSpec inList[inNumTypes]` registered stack garbage beyond
 * entry 0), and its post-call copy-back writes through the caller's pointer,
 * SIGBUSing when the data is read-only (Halo keeps that list in
 * __TEXT,__const -> KERN_PROTECTION_FAILURE on the copy-back store). The
 * classic Carbon surface passes small all-scalar structs (Point, Rect,
 * EventTypeSpec, RGBColor, EventRecord) by pointer pervasively. */
static bool record_is_layout_identical(CXType canon);

static bool field_type_is_layout_identical(CXType t) {
   const CXType canon = clang_getCanonicalType(t);
   switch (canon.kind) {
   case CXType_Bool:
   case CXType_UChar:  case CXType_Char_U:
   case CXType_SChar:  case CXType_Char_S:
   case CXType_UShort: case CXType_Short:
   case CXType_UInt:   case CXType_Int:   case CXType_Enum:
   case CXType_Float:
      return true;
   case CXType_ConstantArray:
      return field_type_is_layout_identical(clang_getElementType(canon));
   case CXType_Record:
      return record_is_layout_identical(canon);
   default:
      return false;
   }
}

static bool record_is_layout_identical(CXType canon) {
   /* incomplete/opaque records (objc runtime structs, CF __CFString, forward
    * decls) and empty structs report size <= 0: not passthrough material */
   if (clang_Type_getSizeOf(canon) <= 0) {
      return false;
   }
   bool ok = true;
   clang_Type_visitFields(
      canon,
      [](CXCursor field, CXClientData data) -> CXVisitorResult {
         bool *okp = static_cast<bool *>(data);
         if (!field_type_is_layout_identical(clang_getCursorType(field))) {
            *okp = false;
            return CXVisit_Break;
         }
         return CXVisit_Continue;
      },
      &ok);
   return ok;
}

void conversion::convert_pointer(std::ostream& os, CXType pointee, const Location& src_,
                                 const Location& dst_) {
   /* a pointer-to-function is NOT data to deep-copy (the old path handed the
    * native API a pointer to an uninitialized dead stack temp — the 22nd
    * blocker); bind it to a native callback trampoline instead */
   const CXType pointee_canon = clang_getCanonicalType(pointee);
   if (pointee_canon.kind == CXType_FunctionProto ||
       pointee_canon.kind == CXType_FunctionNoProto) {
      convert_fnptr(os, pointee_canon, src_, dst_);
      return;
   }

   /* libclang models a SEL parameter as Pointer-to-ObjCSel (canonical
    * spelling "SEL *"): the Pointer level IS the SEL value itself, not an
    * out-param. Deep-copying would dereference the selector name. */
   if (pointee_canon.kind == CXType_ObjCSel) {
      convert_objc_sel(os, src_, dst_);
      return;
   }

   /* In pure-C contexts SEL/id/Class canonicalize to pointers to the opaque
    * runtime records `struct objc_selector|objc_object|objc_class`.
    * Deep-copying those would dereference a selector-name/isa as struct
    * data; route them to the objc marshalling instead. */
   if (pointee_canon.kind == CXType_Record) {
      CXString ps = clang_getTypeSpelling(pointee_canon);
      const std::string spelling(clang_getCString(ps));
      clang_disposeString(ps);
      if (spelling.find("objc_selector") != std::string::npos) {
         convert_objc_sel(os, src_, dst_);
         return;
      }
      if (spelling.find("objc_object") != std::string::npos ||
          spelling.find("objc_class") != std::string::npos) {
         convert_objc_ptr(os, src_, dst_);
         return;
      }
   }

   /* Opaque CF refs (CFStringRef = `struct __CFString *`...): the i386 value
    * may be a proxy-arena HANDLE (data-symbol shadows, wrapped returns) —
    * passing it raw into a native CF API trips __CF_IS_OBJC and aborts.
    * Unwrap going in (raw low values pass through); on copy-back wrap only
    * values above 4GB (dyld-cache constants) — low heap refs stay raw,
    * exactly the pre-existing behavior. */
   if (cb_is_cf_record_ptr(pointee_canon)) {
      convert_cf_ptr(os, src_, dst_);
      return;
   }

   /* Pointer to a fixed-width scalar (char/short/int/float/double/long long/
    * enum/bool): no layout difference between the ABIs, so pass the i386 pointer
    * straight through instead of bouncing the pointee through a scratch buffer.
    * The old deep copy was both lossy — it copied only ONE element, mangling
    * array/buffer args like CFStringCreateWithBytes's `const UInt8 *bytes` or
    * CFDataGetBytes's out buffer — and unsafe: the post-call copy-BACK writes
    * through the pointer, faulting (SIGBUS) when it targets read-only memory such
    * as a constant string's character data. */
   if (pointee_is_passthrough_scalar(pointee_canon)) {
      convert_int(os, CXType_Pointer, src_, dst_);
      return;
   }

   /* Layout-identical record pointee: same rationale and emit as the scalar
    * passthrough above (see record_is_layout_identical). The opaque objc/CF
    * record shapes were already routed away above; those are incomplete types
    * the helper rejects anyway. */
   if (pointee_canon.kind == CXType_Record &&
       record_is_layout_identical(pointee_canon)) {
      convert_int(os, CXType_Pointer, src_, dst_);
      return;
   }

   if (ignore_structs.find(to_string(pointee)) != ignore_structs.end()) {
      /* A `void *` arg is opaque, so it may carry a PROXY-ARENA HANDLE: an i386
       * object the bridge wrapped into a 32-bit low-4GB handle (CFTypeRef stored
       * into a CF collection as `const void *`, e.g. CFDictionaryAddValue's key/
       * value). If the raw handle passes through to native CF, CF later derefs it
       * as an object (CFGetTypeID -> objc_msgSend(handle,_cfTypeID) reads
       * arena[slot] as the isa) and crashes. Route void* through x64_objc_unwrap:
       * the arena region is EXCLUSIVE to handles, so a genuine pointer (truncated
       * native ptr, i386 buffer, CFAllocatorRef) is never in range and passes
       * through untouched. Forward direction only (i386->x86_64 args); a native
       * return must not be "unwrapped". Other ignore_structs pointees (char*,
       * DIR*, Python opaque handles — never arena handles) keep plain passthrough. */
      const std::string ps = to_string(pointee);
      if (from_arch == arch::i386 && to_arch == arch::x86_64 &&
          ps.find("void") != std::string::npos) {
         emit_runtime_bridge_call(os, "_x64_objc_unwrap", "",
                                  src_, reg_width::D, dst_, reg_width::Q);
         return;
      }
      convert_int(os, CXType_Pointer, src_, dst_);
      return;
   }

   /* Break cycles in self-referential types and bound deep object graphs:
    * if we're already deep-converting this pointee type higher on the
    * recursion stack (e.g. struct QElem whose qLink is a QElem*), or we've
    * descended too far, marshal the pointer as an opaque value rather than
    * recursively deep-copying — which would never terminate. */
   const std::string pointee_key = to_string(clang_getCanonicalType(pointee));
   if (active_pointees.count(pointee_key) || active_pointees.size() >= max_pointer_depth) {
      convert_int(os, CXType_Pointer, src_, dst_);
      return;
   }
   active_pointees.insert(pointee_key);
   struct pointee_guard {
      std::set<std::string>& set;
      const std::string& key;
      ~pointee_guard() { set.erase(key); }
   } guard{active_pointees, pointee_key};

   std::unique_ptr<Location> srcp(src_.copy());
   std::unique_ptr<Location> dstp(dst_.copy());
   Location& src = *srcp;
   Location& dst = *dstp;

   if (allocate) {
      /* allocate new pointer to stack data into reg_dst and mem_dst */
      RegisterLocation reg_dst(r11);
      MemoryLocation mem_dst(r11, 0);
      push(os, reg_dst, src, dst);
      {
         data.align(pointee, to_arch);
         emit_inst(os, "lea", "r11", data.op());
         data += sizeof_type(pointee, to_arch);

         /* Hand the bounce buffer to the native callee at FULL 64-bit width.
          * r11 holds a genuine native stack address (lea r11,[rsp+..]) which on
          * macOS lives above 4GB; convert_int(Pointer,..) would marshal it with
          * the i386 pointer width (`mov edi,r11d`) and TRUNCATE the high half,
          * so the callee writes its out-param to a low garbage address while our
          * buffer stays uninitialised. (Portal 2 CThreadLocalBase:
          * pthread_key_create then reads the key back as 0 -> garbage TLS index.)
          * dst is always the x86_64 side here (allocate => i386->x86_64). */
         emit_inst(os, "mov", dst.op(reg_width::Q), reg_dst.reg.reg_q);

         /* load src pointer into reg_src */
         RegisterLocation reg_src(r12);
         MemoryLocation mem_src(r12, 0);
         push(os, reg_src, src, dst);
         {
            convert_int(os, CXType_Pointer, src, reg_src);

            /* A NULL pointer argument must marshal to NULL, not to a pointer
             * into the freshly-reserved (uninitialised) scratch buffer:
             * dereferencing it to deep-copy the pointee would fault (the i386
             * caller legitimately passes NULL for optional args), and a callee
             * that tests `arg == NULL` must still observe NULL. Guard the deep
             * copy on a non-null source; on NULL, store a null pointer to dst. */
            const std::string null_lbl = label();
            const std::string done_lbl = label();
            emit_inst(os, "test", reg_src.reg.reg_q, reg_src.reg.reg_q);
            /* force `near` (rel32): this jumps over the recursive convert()
             * block whose size varies with the pointee, and a rel8/rel32 flip
             * across NASM's optimization passes cumulatively shifts later shim
             * offsets enough to trip NASM 3.01 multi-pass non-convergence
             * ("label changed during code generation"). Fixed-size jumps remove
             * the only forward-ref size variability so NASM converges. */
            emit_inst(os, "jz", "near " + null_lbl);

            /* convert */
            convert(os, pointee, mem_src, mem_dst);
            emit_inst(os, "jmp", "near " + done_lbl);

            os << null_lbl << ":" << std::endl;
            /* Store NULL to dst. reg_src (r12) is already 0 here -- we reached
             * null_lbl via `jz` after `test reg_src` -- and, unlike reg_dst (r11),
             * it is NOT the base register of the `dst` memory operand. The old code
             * did `xor reg_dst` first, zeroing r11 = dst's base, so the store wrote
             * through [0 + field_off] -> SIGSEGV at a near-null addr (e.g.
             * pthread_create(&t, NULL, fn, arg): the NULL attr faulted at 0x8).
             * Emitted for EVERY nullable nested pointer/struct field (487 sites in
             * libabiconv) -> a broad, data-dependent latent crash. */
            convert_int(os, CXType_Pointer, reg_src, dst);
            os << done_lbl << ":" << std::endl;
         }
         pop(os, reg_src, src, dst);
      }
      pop(os, reg_dst, src, dst);

   } else {
      /* Post-call copy-back pass (!allocate). A CONST pointee has nothing to
       * copy back — the callee contracts not to modify it — and writing
       * through the caller's pointer FAULTS when the pointed-to data lives in
       * read-only memory: a `static const` table lands in __TEXT,__const, so
       * the copy-back store trips SIGBUS KERN_PROTECTION_FAILURE (Halo's
       * `static const EventTypeSpec` list passed to InstallEventHandler; any
       * `const struct *` arg backed by constant data). Emit nothing, but still
       * advance the bounce-buffer cursor exactly as the copy-back emit would,
       * keeping this pass's staging offsets in lockstep with the forward
       * pass for every argument that follows. */
      if (clang_isConstQualifiedType(pointee) ||
          clang_isConstQualifiedType(clang_getCanonicalType(pointee))) {
         data.align(pointee, from_arch);
         data += sizeof_type(pointee, from_arch);
         return;
      }
      /* let dst pointer be */
      RegisterLocation reg_src(r12);
      MemoryLocation mem_src(r12, 0);
      push(os, reg_src, src, dst);
      {
         RegisterLocation reg_dst(r11);
         MemoryLocation mem_dst(r11, 0);
         push(os, reg_dst, src, dst);
         {
            data.align(pointee, from_arch);
            emit_inst(os, "lea", "r12", data.op());
            data += sizeof_type(pointee, from_arch);

            convert_int(os, CXType_Pointer, dst, reg_dst);

            /* NULL pointer arg: nothing to copy back, and dereferencing the
             * null destination would fault (mirror the allocate path). */
            const std::string done_lbl = label();
            emit_inst(os, "test", reg_dst.reg.reg_q, reg_dst.reg.reg_q);
            emit_inst(os, "jz", "near " + done_lbl);   /* near: see convert note above */

            /* convert underlying data */
            convert(os, pointee, mem_src, mem_dst);
            os << done_lbl << ":" << std::endl;
         }
         pop(os, reg_dst, src, dst);
      }
      pop(os, reg_src, src, dst);
   }
}

/* SIZEOF */


static size_t sizeof_record(CXType type, arch a);

size_t sizeof_type(CXType type, arch a) {
   switch (type.kind) {
   case CXType_Bool:
   case CXType_Char_U:
   case CXType_UChar:
   case CXType_Char_S:
   case CXType_SChar:
      return 1;
   case CXType_UShort:
   case CXType_Short:
      return 2;
   case CXType_UInt:
   case CXType_Int:
   case CXType_Enum:
      return 4;
   case CXType_ULong:
   case CXType_Long:
      switch (a) {
      case arch::i386: return 4;
      case arch::x86_64: return 8;
      default: abort();
      }
   case CXType_ULongLong:
   case CXType_LongLong:
      return 8;
   case CXType_Float:
      return sizeof(float);
   case CXType_Double:
      return sizeof(double);
   case CXType_LongDouble:
      return sizeof(long double);
   case CXType_Pointer:
   case CXType_BlockPointer:
   case CXType_ObjCObjectPointer:
   case CXType_ObjCId:
   case CXType_ObjCClass:
   case CXType_ObjCSel:
      switch (a) {
      case arch::i386: return 4;
      case arch::x86_64: return 8;
      default: abort();
      }
   case CXType_ConstantArray:
      return sizeof_type(clang_getArrayElementType(type), a) * clang_getArraySize(type);
   case CXType_Record:
      return sizeof_record(type, a);

   case CXType_Typedef:
      /* A WRITTEN (sugared) type reaches the layout machinery only via the
       * constant-array element path (populate_fields keeps a written array so
       * its element retains the typedef sugar). Apply the fixed-32 correction
       * and recurse on the effective canonical (Fixed -> 4-byte native Int,
       * not the canonical-long 8). */
      return sizeof_type(effective_field_type(clang_getCanonicalType(type), type), a);

   case CXType_FunctionProto:
      // TODO: May need to address this case in the future.
      return sizeof_type(CXType_Pointer, a);

   case CXType_Void:
      return 0;

   case CXType_IncompleteArray:
      throw std::invalid_argument("sizeof_type: incomplete array");

   default:
      throw std::invalid_argument("sizeof_type: unsupported type kind");
   }
}

size_t sizeof_type(CXTypeKind type_kind, arch a) {
   switch (type_kind) {
   case CXType_Bool:
   case CXType_Char_U:
   case CXType_UChar:
   case CXType_Char_S:
   case CXType_SChar:
      return 1;
   case CXType_UShort:
   case CXType_Short:
      return 2;
   case CXType_UInt:
   case CXType_Int:
   case CXType_Enum:
      return 4;
   case CXType_ULong:
   case CXType_Long:
      switch (a) {
      case arch::i386: return 4;
      case arch::x86_64: return 8;
      default: abort();
      }
   case CXType_ULongLong:
   case CXType_LongLong:
      return 8;
   case CXType_Float:
      return sizeof(float);
   case CXType_Double:
      return sizeof(double);
   case CXType_LongDouble:
      return sizeof(long double);
   case CXType_Pointer:
   case CXType_BlockPointer:
   case CXType_ObjCObjectPointer:
   case CXType_ObjCId:
   case CXType_ObjCClass:
   case CXType_ObjCSel:
      switch (a) {
      case arch::i386: return 4;
      case arch::x86_64: return 8;
      default: abort();
      }
   default:
      throw std::invalid_argument("sizeof_type(kind): unsupported type kind");
   }
}

static size_t sizeof_union(CXType type, arch a);
static size_t sizeof_struct(CXType type, arch a);
static size_t sizeof_record(CXType type, arch a) {
   switch (clang_getCursorKind(clang_getTypeDeclaration(type))) {
   case CXCursor_UnionDecl:
      return sizeof_union(type, a);
   case CXCursor_StructDecl:
      return sizeof_struct(type, a);
   default:
      throw std::invalid_argument("sizeof_record: not a struct/union");
   }
}

static size_t sizeof_union(CXType type, arch a) {
   record_decl decl(type);
   assert(!decl.packed);
   
   size_t size = 0;

   for (CXType field_type : decl.field_types) {
      const size_t field_size = sizeof_type(field_type, a);
      size = std::max(size, field_size);
   }

   const size_t align = alignof_record(type, a);
   size = align_up(size, align);

   return size;
}

static size_t sizeof_struct(CXType type, arch a) {
   record_decl decl(type);
   size_t size = 0;

   for (CXType field_type : decl.field_types) {
      const size_t field_size = sizeof_type(field_type, a);
      if (!decl.packed) {
         const size_t field_align = alignof_type(field_type, a);
         size = align_up(size, field_align) + field_size;
      } else {
         /* packed record (e.g. Carbon `#pragma pack` structs such as
          * HFSUniStr255 {UInt16 length; UniChar unicode[255];}): no inter-field
          * padding. This branch previously did NOTHING, so EVERY packed struct
          * sized as 0. A by-pointer packed-struct arg (FSGetDataForkName's
          * HFSUniStr255* out-param) then reserved 0 scratch bytes in the shim
          * frame (`sub rsp, 0`), and the native callee's struct write (512 zero
          * bytes for an empty data-fork name) overflowed UP the stack, zeroing
          * the shim's saved return address -> the i386-style epilogue did
          * `jmp r11` with r11=0 -> rip=0. Accumulate field sizes (matches the
          * conversion code's own field-by-field, padding-free scratch layout). */
         size += field_size;
      }
   }

   if (!decl.packed) {
      const size_t align = alignof_type(type, a);
      size = align_up(size, align);
   }

   return size;
}

int sizeof_type_archcmp(CXType type) {
   return sizeof_type(type, arch::i386) - sizeof_type(type, arch::x86_64);
}

/* ALIGNOF */

static size_t alignof_record(CXType type, arch a) {
   record_decl decl(type);

   if (decl.packed) {
      return 1;
   } else {
      size_t align = 1;
      for (CXType field_type : decl.field_types) {
         size_t field_align = alignof_type(field_type, a);

         // NOTE: This seems inconsistent, but it appears how clang works.
         if (field_align == 8 && a == arch::i386) {
            field_align = 4;
         }
         
         align = std::max(align, field_align);
      }
      return align;
   }
}

size_t alignof_type(CXType type, arch a) {
   switch (type.kind) {
   case CXType_Record:
      return alignof_record(type, a);
   case CXType_ConstantArray:
      return alignof_type(clang_getElementType(type), a);
   default:
      return std::max<size_t>(sizeof_type(type, a), 1);
   }
}

