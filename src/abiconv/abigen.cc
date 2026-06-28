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
            /* by-value struct. An INTEGER struct (CFRange/NSRange: 1-2 longs)
             * takes n INTEGER eightbytes -> n GP regs if they all fit, else the
             * whole struct spills. A homogeneous FP struct takes n SSE eightbytes:
             * <=16 bytes (<=2 fields) -> SSE regs, else MEMORY (stack). Each class
             * follows SysV all-or-nothing. Mirrors the marshalling loop. Throws
             * (neither shape) -> skip fn. */
            bool is_int = true;
            size_t n = 0;
            try {
               n = integer_byval_struct_fields(argtype).size();
            } catch (const std::invalid_argument&) {
               is_int = false;
            }
            if (is_int) {
               if (reg_i + n <= max_reg_args) {
                  reg_i += n;
               } else {
                  size += 8 * n;
               }
               continue;
            }
            n = fp_byval_struct_fields(argtype).size();   /* throws -> skip fn */
            if (8 * n <= 16 && xmm_i + n <= max_xmm_args) {
               xmm_i += n;
            } else {
               size += 8 * n;
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


#if 0
   void emit_function_call(std::ostream& os, Symbols& symbols, const Symbols& ignore_structs) {
      const std::list<const reg_group *> regs {&rdi, &rsi, &rdx, &rcx, &r8, &r9};
      emit(os, symbols, ignore_structs, regs.begin(), regs.end(),
           [] (std::ostream& os, const std::string& sym) {
              emit_inst(os, "call", sym);
           });      
   }

   void emit_system_call(std::ostream& os, Symbols& symbols, const Symbols& ignore_structs) {
      /* NOTE: %rax isn't really a parameter, though treating it as such preserves it during
       * transformation process.
       */
      const std::list<const reg_group *> regs {&rax, &rdi, &rsi, &rdx, &r10, &r8, &r9};
      emit(os, symbols, ignore_structs, regs.begin(), regs.end(),
           [] (std::ostream& os, const std::string& sym) {
              emit_inst(os, "syscall");
           });
   }
#endif

   void emit(std::ostream& final_os, Symbols& symbols, const Symbols& ignore_structs,
             const Symbols& reserved_names) {
      const bool variadic = clang_isFunctionTypeVariadic(function_type);
      const std::string& override_prefix = "__";

      if (variadic) {
         /* skip variadic functions */
         return;
      }

      if (symbols.find(sym) == symbols.end()) {
         return;
      }

      /* Shim-name collision guard. The shim for symbol S is named
       * override_prefix+S (e.g. _tolower -> ___tolower). If this symbol's own
       * linker name equals some OTHER bridged symbol's shim name, emitting it
       * would (a) clash with that shim's label and (b) make this shim's
       * `extern <sym>` reference its own colliding shim instead of the real
       * function. This happens for libc's internal double-underscore twins
       * (e.g. __tolower's name ___tolower == tolower's shim name). Skip this
       * one; the public twin (tolower) is bridged instead. */
      if (reserved_names.find(sym) != reserved_names.end()) {
         std::cerr << "abigen: skipping " << sym
                   << ": linker name collides with another symbol's shim" << std::endl;
         return;
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
            return;
         }
      }
      symbols.erase(sym);
      final_os << os.str();
   }

   void emit_body(std::ostream& os, const Symbols& ignore_structs,
                  const std::string& override_prefix) {
      os << "\tglobal\t" << override_prefix << sym << std::endl;
      os << "\textern\t" << sym << std::endl;

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
      const CXType ret_canon =
         clang_getCanonicalType(clang_getResultType(function_type));
      const size_t sret_size =
         fp_sret ? align_up<size_t>(sizeof_type(ret_canon, arch::x86_64), 16) : 0;
      /* the native return buffer sits just ABOVE the outgoing stack args and the
       * pointer-deep-copy scratch data, at the top of the reserved frame */
      const size_t sret_buf_off = stack_args_size() + stack_data_size();

      emit_inst(os, "sub", "rsp", stack_data_size() + stack_args_size() + sret_size);

      /* transfer arguments */
      param_info info(regs.begin(), regs.end(), 8);
      /* the hidden sret pointer consumes rdi on the native side and occupies the
       * i386 caller's first arg slot, so the real declared args start one i386 slot
       * later and the first INTEGER arg shifts from rdi to rsi. */
      if (fp_sret) { ++info.reg_it; }
      int param_it;
      int param_end = clang_getNumArgTypes(function_type);
      MemoryLocation load_loc(rbp, fp_sret ? 16 : 12);

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

         if (type.kind == CXType_Record) {
            /* by-value INTEGER struct (CFRange/NSRange: 1-2 long fields): marshal
             * each field into its own GP register, widening the i386 4-byte field
             * to the x86_64 8-byte eightbyte; or spill the whole struct to
             * consecutive 8-byte stack slots when the GP regs can't hold it all
             * (SysV all-or-nothing). No post-call copy-back (the arg is by value). */
            bool is_int = true;
            record_decl::FieldTypes fields;
            try {
               fields = integer_byval_struct_fields(type);
            } catch (const std::invalid_argument&) {
               is_int = false;
            }
            if (is_int) {
               const size_t n = fields.size();
               const bool in_regs =
                  static_cast<size_t>(std::distance(info.reg_it, info.reg_end)) >= n;
               int foff = 0;
               for (CXType ftype : fields) {
                  std::unique_ptr<Location> fdst;
                  if (in_regs) {
                     fdst = std::make_unique<RegisterLocation>(**info.reg_it++);
                  } else {
                     fdst = std::make_unique<MemoryLocation>(stack_args);
                     stack_args += 8;
                  }
                  MemoryLocation fsrc = load_loc + foff;
                  to_conv.convert(to_ss, ftype, fsrc, *fdst);
                  foff += 4;              /* i386 field width (long = 4) */
               }
               load_loc += align_up<size_t>(sizeof_type(type, arch::i386), 4); /* 4*n */
               continue;
            }

            /* by-value FLOATING-POINT struct (CGPoint/CGSize/CGRect/NSRect/
             * CGAffineTransform): SSE-class (<=16 bytes) goes in consecutive xmm
             * registers; MEMORY-class (>16 bytes) spills to consecutive 8-byte
             * stack slots. Each i386 field (a 4-byte CGFloat or 8-byte double) is
             * read at its i386 offset and widened to an x86_64 double. No
             * copy-back (the arg is by value). Throws -> skip the whole function. */
            const std::vector<bool> widen = fp_byval_struct_fields(type);
            if (ret_is_record && !fp_sret) {
               throw std::invalid_argument(
                  "fp-struct arg with non-FP/register-class struct return not supported");
            }
            const size_t n = widen.size();
            const bool in_regs = 8 * n <= 16 && (info.fp_idx + n) <= max_xmm_args;
            int i386_off = 0;
            for (bool w : widen) {
               MemoryLocation fsrc = load_loc + i386_off;
               if (in_regs) {
                  SSELocation fdst(info.fp_idx++);
                  emit_fp_widen(to_ss, w, fsrc, fdst);
               } else {
                  MemoryLocation fdst = stack_args;
                  emit_fp_widen(to_ss, w, fsrc, fdst);
                  stack_args += 8;
               }
               i386_off += w ? 4 : 8;     /* i386 field width (CGFloat 4 / double 8) */
            }
            load_loc += align_up<size_t>(static_cast<size_t>(i386_off), 4);
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
         const CXType rcanon =
            clang_getCanonicalType(clang_getResultType(function_type));
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
         } else if (cf_opaque_ptr_type(rcanon)) {
            /* opaque CF refs: a dyld-cache constant (CFSTR, kCF*) would
             * truncate in the i386 caller's eax — wrap only those; low
             * heap refs stay raw (status quo, no arena churn). The CF-arg
             * unwrap (convert_cf_ptr) accepts both forms. */
            emit_inst(os, "mov", "rdi", "rax");
            emit_inst(os, "shr", "rdi", "32");
            emit_inst(os, "jz", ".cfretlow");
            emit_inst(os, "mov", "rdi", "rax");
            emit_inst(os, "call", "_x64_objc_wrap");
            os << ".cfretlow:" << std::endl;
         } else if (rcanon.kind == CXType_Pointer) {
            /* C-string return such as glGetString's const GLubyte ptr (a pointer
             * to char or unsigned char): a >4GB native static string truncates
             * to a wild pointer in the i386 caller's eax and faults when its
             * bytes are read (strlen / initWithUTF8String:). Bounce a high
             * return into a low-4GB copy; a low return (a pointer into a caller
             * buffer, the strchr/strstr case) passes through unchanged. */
            const CXTypeKind pk =
               clang_getCanonicalType(clang_getPointeeType(rcanon)).kind;
            if (pk == CXType_Char_S || pk == CXType_Char_U ||
                pk == CXType_SChar  || pk == CXType_UChar) {
               emit_inst(os, "mov", "rdi", "rax");
               emit_inst(os, "call", "_x64_cstr_ret_low");
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

      // emit_inst(os, "add", "rsp", stack_data_size() + stack_args_size());
      emit_inst(os, "lea", "rsp", "[rbp - 0x10]");
      
      /* cleanup */
      emit_inst(os, "pop", "rsi");
      emit_inst(os, "pop", "rdi");
      emit_inst(os, "leave");

      /* return */
      emit_inst(os, "mov", "r11d", "dword [rsp]");
      emit_inst(os, "add", "rsp", "4");
      emit_inst(os, "jmp", "r11");
      
   }
   
};

struct FunctionConversion: ABIConversion {
   template <typename... Args>
   FunctionConversion(Args&&... args):
      ABIConversion(args..., {&rdi, &rsi, &rdx, &rcx, &r8, &r9}) {}

   virtual void emit_call(std::ostream& os) const override {
      emit_inst(os, "call", sym);
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
                             * name is in here is skipped to avoid label clashes */
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
         conv->emit(os, symbols, ignore_structs, collision_names);
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
         conv->emit(os, symbols, ignore_structs, collision_names);
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
      default:
         return; /* only objc-object + opaque-CF data constants */
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
      for (const std::string& s : data_shadow_syms) {
         const std::string shadow = override_prefix + s; /* ___NSArgumentDomain */
         os << "\tglobal " << shadow << std::endl;
         os << shadow << ": dq 0" << std::endl;
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

   return 0;
}
