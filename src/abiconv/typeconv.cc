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
                     CXType type = clang_getCanonicalType(clang_getCursorType(c));
                     field_types.push_back(type);
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

void conversion::convert_record(std::ostream& os, CXType record, MemoryLocation src,
                                MemoryLocation dst) {
   record_decl decl(record);
   /* Unions can't be converted field-by-field (members overlap; which one is
    * live is unknown). Skip the whole function rather than mislaying it. */
   if (decl.cursor.kind != CXCursor_StructDecl) {
      throw std::invalid_argument("convert_record: union by value not supported");
   }
   for (CXType field_type : decl.field_types) {
      src.align_field(field_type, from_arch);
      dst.align_field(field_type, to_arch);
      
      convert(os, field_type, src, dst);

#if 0
      std::cerr << to_string(record) << "," << to_string(field_type) << "," << src.index
                << "," << dst.index << std::endl;
#endif

      /* update src, dst */
      src += sizeof_type(field_type, from_arch);
      dst += sizeof_type(field_type, to_arch);
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

/* An ObjC object / Class parameter. i386 -> x86_64: the i386 value is either
 * a low-4GB proxy-arena HANDLE (data-symbol shadows, wrapped returns) or an
 * already-usable low pointer (slid legacy CFString constants, registered
 * reverse-class objects) — x64_objc_unwrap resolves handles and passes raw
 * values through. x86_64 -> i386 (deep-copy-back of out-params like
 * NSError**): wrap the real 64-bit object into a handle the bridge unwraps
 * on the next message send (x64_objc_wrap dedupes, so handles are stable
 * per object and equality survives). */
void conversion::convert_objc_ptr(std::ostream& os, const Location& src,
                                  const Location& dst) {
   if (from_arch == arch::i386 && to_arch == arch::x86_64) {
      os << "\t; objc object arg: unwrap handle -> real object" << std::endl;
      emit_runtime_bridge_call(os, "_x64_objc_unwrap", "",
                               src, reg_width::D, dst, reg_width::Q);
   } else {
      os << "\t; objc object copy-back: wrap real object -> handle" << std::endl;
      emit_runtime_bridge_call(os, "_x64_objc_wrap", "",
                               src, reg_width::Q, dst, reg_width::D);
   }
}

/* An opaque CF-ref parameter (CFStringRef etc.). Forward: unwrap proxy-arena
 * handles to the real ref (x64_objc_unwrap passes raw values through).
 * Copy-back: only re-wrap values above 4GB (a dyld-cache constant the i386
 * slot can't hold); low heap refs are stored raw as they always were —
 * wrapping every created ref would churn arena slots for nothing. */
void conversion::convert_cf_ptr(std::ostream& os, const Location& src,
                                const Location& dst) {
   if (from_arch == arch::i386 && to_arch == arch::x86_64) {
      os << "\t; CF ref arg: unwrap handle -> real ref" << std::endl;
      emit_runtime_bridge_call(os, "_x64_objc_unwrap", "",
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

   if (ignore_structs.find(to_string(pointee)) != ignore_structs.end()) {
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
            emit_inst(os, "jz", null_lbl);

            /* convert */
            convert(os, pointee, mem_src, mem_dst);
            emit_inst(os, "jmp", done_lbl);

            os << null_lbl << ":" << std::endl;
            emit_inst(os, "xor", reg_dst.reg.reg_d, reg_dst.reg.reg_d);
            convert_int(os, CXType_Pointer, reg_dst, dst);
            os << done_lbl << ":" << std::endl;
         }
         pop(os, reg_src, src, dst);
      }
      pop(os, reg_dst, src, dst);

   } else {
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
            emit_inst(os, "jz", done_lbl);

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

