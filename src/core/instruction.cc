#include <algorithm>
#include <cassert>
#include <unordered_map>
#include <execinfo.h>
#include <cxxabi.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>

extern "C" {
#include <xed/xed-interface.h>
}

#include "instruction.hh"
#include "image.hh"
#include "build.hh"
#include "parse.hh"
#include "transform.hh"
#include "opcodes.hh"
#include "section.hh"
#include "archive.hh"
#include "segment.hh"

namespace MachO {

   namespace {
      template <Bits bits>
      const xed_state_t dstate_ =
         {select_value(bits, XED_MACHINE_MODE_LEGACY_32, XED_MACHINE_MODE_LONG_64),
          select_value(bits, XED_ADDRESS_WIDTH_32b, XED_ADDRESS_WIDTH_32b)
         };

      typename SectionBlob<Bits::M32>::SectionBlobs push_r32(xed_reg_enum_t r32) {
         /* i386 | push r32
          * -----|---------
          * X86  | lea rsp,[rsp-4]
          *      | mov [rsp], r32
          * NOTE: Shouldn't modify flags (lea doesn't). Was two 16-bit `push ax`,
          * ~45% slower under Rosetta for a push/pop pair (measured). */
         auto lea = new Instruction<Bits::M64>(opcode::lea_rsp_mem_rsp_m4());
         auto mov = new Instruction<Bits::M64>(opcode::mov_mem_rsp_r32(r32));
         return {lea, mov};
      }

      typename SectionBlob<Bits::M32>::SectionBlobs push_imm(uint32_t imm) {
         /* i386 | push imm32
          * -----|-----------
          * X86  | lea rsp,[rsp-4]
          *      | mov dword [rsp], imm
          */
         auto lea = new Instruction<Bits::M64>(opcode::lea_rsp_mem_rsp_m4());
         opcode_t mov_opcode = {0xc7, 0x04, 0x24};
         opcode::push_back_imm<uint32_t>(mov_opcode, imm);
         auto mov = new Instruction<Bits::M64>(mov_opcode);
         return {lea, mov};
      }

      typename SectionBlob<Bits::M32>::SectionBlobs pop_r32(xed_reg_enum_t r32) {
         /* i386 | pop r32
          * -----|--------
          * X86  | mov r32,[rsp]
          *      | lea rsp,[rsp+4]
          * NOTE: Shouldn't modify flags.
          */
         auto mov = new Instruction<Bits::M64>(opcode::mov_r32_mem_rsp(r32));
         auto lea = new Instruction<Bits::M64>(opcode::lea_rsp_mem_rsp_4());
         return {mov, lea};
      }

      /*
       * Width-equivalence guard for byte-identical pass-throughs.
       *
       * Every "8-byte read of a 4-byte slot" bug we've hit (call/jmp/
       * push/pop [mem], 2026-06-11 fix pass) is the same class: an i386
       * instruction whose identical bytes decode in x86_64 with a
       * DIFFERENT memory-access width (explicit operand or implicit
       * stack access — XED models both as memory operands). Instead of
       * finding these one runtime crash at a time, compare the memory-
       * operand widths of the source-mode and dest-mode decodes whenever
       * the transform is about to emit the bytes unchanged, and fail
       * LOUDLY at translate time naming the iform, so the missing
       * explicit rule can be added.
       *
       * MACHO_WIDTH_GUARD=warn  downgrade to a warning (corpus audit)
       * MACHO_WIDTH_GUARD=off   disable
       *
       * Intentional width-divergent pass-throughs (stub-helper push,
       * call via promoted 8-byte dyld slot — native-context code) must
       * NOT route through the guarded default paths; they construct
       * their clones explicitly.
       */
      void check_transform_width(const xed_decoded_inst_t& src_xedd,
                                 const xed_decoded_inst_t& dst_xedd,
                                 const opcode_t& instbuf,
                                 std::size_t vmaddr) {
         const unsigned nsrc =
            xed_decoded_inst_number_of_memory_operands(&src_xedd);
         const unsigned ndst =
            xed_decoded_inst_number_of_memory_operands(&dst_xedd);
         bool mismatch = nsrc != ndst;
         for (unsigned i = 0; !mismatch && i < nsrc; ++i) {
            if (xed_decoded_inst_get_memory_operand_length(&src_xedd, i) !=
                xed_decoded_inst_get_memory_operand_length(&dst_xedd, i)) {
               mismatch = true;
            }
         }
         if (!mismatch) { return; }

         char hex[3 * 24 + 8] = {0};
         const std::size_t n = instbuf.size() < 24 ? instbuf.size() : 24;
         char *p = hex;
         for (std::size_t i = 0; i < n; ++i) {
            p += sprintf(p, "%02x ", instbuf.at(i));
         }
         char widths[128] = {0};
         p = widths;
         for (unsigned i = 0; i < std::max(nsrc, ndst); ++i) {
            p += sprintf(p, "%s mem%u: %u->%u", i ? "," : "", i,
                         i < nsrc ? xed_decoded_inst_get_memory_operand_length(
                                       &src_xedd, i) : 0,
                         i < ndst ? xed_decoded_inst_get_memory_operand_length(
                                       &dst_xedd, i) : 0);
         }
         const char *guard_mode = std::getenv("MACHO_WIDTH_GUARD");
         if (guard_mode && strcmp(guard_mode, "off") == 0) { return; }
         if (guard_mode && strcmp(guard_mode, "warn") == 0) {
            fprintf(stderr,
                    "warning: WIDTH GUARD: byte-identical pass-through of %s "
                    "changes memory-access width (%s) at vmaddr 0x%zx "
                    "(bytes: %s) — needs an explicit Transform rule\n",
                    xed_iform_enum_t2str(
                       xed_decoded_inst_get_iform_enum(&src_xedd)),
                    widths, vmaddr, hex);
            return;
         }
         throw error("WIDTH GUARD: byte-identical pass-through of %s changes "
                     "memory-access width (%s) at vmaddr 0x%zx (bytes: %s) — "
                     "add an explicit Transform rule for this iform "
                     "(MACHO_WIDTH_GUARD=warn to audit)",
                     xed_iform_enum_t2str(
                        xed_decoded_inst_get_iform_enum(&src_xedd)),
                     widths, vmaddr, hex);
      }

      typename SectionBlob<Bits::M32>::SectionBlobs call_op(SectionBlob<Bits::M64> *jmp_inst) {
         /* i386 | call <op>
          * -----|----------
          * X86  | lea r11,[rip+<size>]
          *      | _push r11d
          *      | jmp <op>
          */
         auto lea_inst = new Instruction<Bits::M64>(opcode::lea_r11_mem_rip_disp32());
         auto push_insts = push_r32(XED_REG_R11D);
         auto ret_placeholder = Placeholder<Bits::M64>::Create();
         lea_inst->memidx = 0;
         lea_inst->memdisp = ret_placeholder;
         auto insts = push_insts;
         insts.push_front(lea_inst);
         insts.push_back(jmp_inst);
         insts.push_back(ret_placeholder);
         return insts;
      }

      /*
       * Optional translate-time null-branch trap (env MACHO_NULL_TRAP).
       *
       * Returns `{ test r64,r64 ; jnz +2 ; ud2 }` to splice right before an
       * indirect `jmp <r64>`, so a transfer through a NULL register faults with
       * SIGILL *at the jump site* (rip names the instruction) instead of
       * jumping to address 0 (rip=0, rax=0 — unrecoverable, no frame). The jnz
       * skips the 2-byte ud2 when the register is non-zero; the trap is emitted
       * as three fixed-byte M64 blobs laid out contiguously, so the rel8 +2
       * stays valid through convert.
       *
       * Empty unless MACHO_NULL_TRAP is set in the translator's environment, so
       * production builds are byte-for-byte unchanged. This is a permanent,
       * reusable diagnostic for the recurring "rip=0 / jmp-to-0" crash class
       * (iPhoto callback NULL fn-ptr, Portal 2 null C++ vtable slots): retranslate
       * the suspect binary with MACHO_NULL_TRAP=1 and the SIGILL pins the site.
       */
      typename SectionBlob<Bits::M32>::SectionBlobs null_trap(xed_reg_enum_t r64) {
         typename SectionBlob<Bits::M32>::SectionBlobs out;
         if (!std::getenv("MACHO_NULL_TRAP")) { return out; }
         const unsigned n = (r64 - XED_REG_RAX) % 8;
         uint8_t rex = 0x48;                 /* REX.W */
         if (r64 >= XED_REG_R8 && r64 <= XED_REG_R15) { rex |= 0x05; } /* REX.R|REX.B */
         const uint8_t modrm = 0xC0 | (n << 3) | n;
         out.push_back(new Instruction<Bits::M64>(opcode_t{rex, 0x85, modrm})); /* test r64,r64 */
         out.push_back(new Instruction<Bits::M64>(opcode_t{0x75, 0x02}));       /* jnz +2 */
         out.push_back(new Instruction<Bits::M64>(opcode_t{0x0f, 0x0b}));       /* ud2 */
         return out;
      }

      bool is_legacy_prefix(uint8_t b) {
         switch (b) {
         case 0x66: case 0x67: case 0xF0: case 0xF2: case 0xF3:
         case 0x2E: case 0x36: case 0x3E: case 0x26: case 0x64: case 0x65:
            return true;
         default:
            return false;
         }
      }

      /* Byte layout of an i386 instruction that carries a ModR/M memory
       * operand: [prefixes][opcode][ModR/M [SIB] [disp]][trailing immediate]. */
      struct ModrmLayout {
         std::size_t opcode_idx = 0;   /*!< first byte after the legacy prefixes */
         std::size_t modrm_idx = 0;
         std::size_t operand_end = 0;  /*!< one past the last disp byte */
      };

      bool modrm_layout(const opcode_t& buf, ModrmLayout& out) {
         std::size_t p = 0;
         while (p < buf.size() && is_legacy_prefix(buf[p])) { ++p; }
         out.opcode_idx = p;
         if (p < buf.size() && buf[p] == 0x0F) {
            p += (p + 1 < buf.size() && (buf[p + 1] == 0x38 || buf[p + 1] == 0x3A)) ? 3 : 2;
         } else {
            p += 1;
         }
         if (p >= buf.size()) { return false; }
         out.modrm_idx = p;
         const uint8_t mod = buf[p] >> 6, rm = buf[p] & 7;
         if (mod == 3) { return false; }
         std::size_t end = p + 1;
         uint8_t sib_base = 0;
         if (rm == 4) {
            if (end >= buf.size()) { return false; }
            sib_base = buf[end] & 7;
            ++end;
         }
         if (mod == 1) { end += 1; }
         else if (mod == 2 || (mod == 0 && (rm == 5 || (rm == 4 && sib_base == 5)))) { end += 4; }
         if (end > buf.size()) { return false; }
         out.operand_end = end;
         return true;
      }

      /* i386 computes every effective address mod 2^32. A scaled index that
       * relies on the wrap (a negative/sentinel index) needs the 0x67 prefix in
       * x86_64. Never for a stack-pointer base or index (the widened rsp/rbp
       * must not be truncated; rbp as a BASE is an ordinary register in
       * -fomit-frame-pointer code, guard 99_sib_ebp_base_neg_wrap) nor for a
       * base-only operand, which may hold a native >4GB pointer. */
      bool wants_addr32(xed_reg_enum_t base, xed_reg_enum_t index) {
         static const bool rbp_base_wide =
            std::getenv("M64_SIB_RBP_BASE_WIDE") != nullptr;
         auto stack = [](xed_reg_enum_t r) {
            return r == XED_REG_ESP || r == XED_REG_RSP || r == XED_REG_EBP ||
                   r == XED_REG_RBP || r == XED_REG_EIP || r == XED_REG_RIP;
         };
         auto frame = [](xed_reg_enum_t r) { return r == XED_REG_EBP || r == XED_REG_RBP; };
         if (index == XED_REG_INVALID || stack(index)) { return false; }
         if (stack(base) && (rbp_base_wide || !frame(base))) { return false; }
         return true;
      }

   }

   /* Re-encode this i386 instruction's memory operand for x86_64.
    *
    * Every rewrite that rebuilds an instruction around the SAME memory operand
    * (call/jmp/push/pop [mem], `op [mem],$ptr`, the generic PIC and absolute-
    * table forms) goes through here, so the operand's relocation is decided in
    * one place:
    *
    *   PIC-anchored `disp(%anchor)`      -> [rip+target]
    *   PIC-anchored `disp(%anchor,i,s)`  -> lea r11,[rip+target]; [r11+i*s]
    *   absolute `[disp32]`               -> [rip+target]
    *   absolute `disp32(%base)`          -> lea r11,[rip+target]; [base+r11]
    *   absolute `disp32(,i,s)` / `disp32(%b,i,s)` -> kept absolute; the disp is
    *     rebased at Emit and listed in __86x64_abs32 for the runtime slide
    *   register-relative, no relocation -> bytes kept, 0x67 per wants_addr32
    *
    * The anchor register is dead in x86_64 (it holds the translated return
    * address, not the i386 anchor), so a PIC operand must never keep it. */
   template <Bits bits>
   bool Instruction<bits>::lower_mem(TransformEnv<bits>& env, LoweredMem& out) const {
      ModrmLayout L;
      if (!modrm_layout(instbuf, L)) { return false; }
      for (std::size_t i = 0; i < L.opcode_idx; ++i) {
         if (instbuf[i] == 0x67) { return false; }   /* 16-bit addressing */
      }
      const uint8_t modrm = instbuf[L.modrm_idx];
      const uint8_t rm = modrm & 7;
      const bool has_sib = rm == 4;
      const uint8_t sib = has_sib ? instbuf[L.modrm_idx + 1] : 0;
      const xed_operand_values_t *ops = xed_decoded_inst_operands_const(&xedd);
      const xed_reg_enum_t base = xed_decoded_inst_get_base_reg(ops, memidx);
      const xed_reg_enum_t index = xed_decoded_inst_get_index_reg(ops, memidx);

      auto lea_r11 = [&]() {
         auto *lea = new Instruction<opposite<bits>>(opcode::lea_r11_mem_rip_disp32());
         lea->memidx = 0;
         env.resolve(memdisp, &lea->memdisp);
         lea->memdisp_offset = memdisp_offset;
         out.pre.push_back(lea);
      };
      auto rip_rel = [&]() {
         out.operand = {0x05, 0, 0, 0, 0};
         out.bind = LoweredMem::RIP;
      };

      static const bool no_pic_lowering =
         std::getenv("M64_NO_PIC_OPERAND_LOWERING") != nullptr;   /* guard OFF arm */
      if (memdisp && pic_anchored && !no_pic_lowering) {
         uint8_t idx = (sib >> 3) & 7, scale = sib >> 6;
         if (pic_anchor_in_index) { idx = sib & 7; scale = 0; }
         if (!has_sib || idx == 4) {
            rip_rel();
         } else {
            lea_r11();
            out.operand = {0x04, (uint8_t)((scale << 6) | (idx << 3) | 0x03)};
            out.rex = 0x01;                            /* REX.B: base r11 */
            out.addr32 = idx != 5;
         }
      } else if (memdisp && base == XED_REG_INVALID && index == XED_REG_INVALID) {
         rip_rel();
      } else if (base == XED_REG_INVALID && index == XED_REG_INVALID && !has_sib) {
         /* unbound i386 [disp32]: the same ModR/M is [rip+disp32] in x86_64;
          * the SIB no-base/no-index form keeps it the absolute i386 address */
         out.operand = {0x04, 0x25};
         out.operand.insert(out.operand.end(), instbuf.begin() + L.modrm_idx + 1,
                            instbuf.begin() + L.operand_end);
      } else if (memdisp && index == XED_REG_INVALID) {
         const uint8_t b = has_sib ? (sib & 7) : rm;
         lea_r11();
         if (b == 5) {   /* SIB base 101 at mod=00 means "no base": use disp8 0 */
            out.operand = {0x44, (uint8_t)(0x18 | b), 0x00};
         } else {
            out.operand = {0x04, (uint8_t)(0x18 | b)};
         }
         out.rex = 0x02;                               /* REX.X: index r11 */
      } else {
         out.operand.assign(instbuf.begin() + L.modrm_idx,
                            instbuf.begin() + L.operand_end);
         out.operand[0] = (uint8_t)(modrm & 0xC7);
         out.bind = memdisp ? LoweredMem::ABS : LoweredMem::NONE;
         out.addr32 = wants_addr32(base, index);
      }
      out.prefixes.assign(instbuf.begin(), instbuf.begin() + L.opcode_idx);
      out.opcode.assign(instbuf.begin() + L.opcode_idx, instbuf.begin() + L.modrm_idx);
      out.reg = (modrm >> 3) & 7;
      out.trailing.assign(instbuf.begin() + L.operand_end, instbuf.end());
      return true;
   }

   /* Assemble `[0x67][prefixes][REX] opcode ModR/M(reg) operand trailing` from a
    * lowered operand and bind its displacement. `reg` may name r8..r15. */
   template <Bits bits>
   typename SectionBlob<bits>::SectionBlobs
   Instruction<bits>::emit_mem(TransformEnv<bits>& env, const LoweredMem& m,
                               const opcode_t& opcode, uint8_t reg,
                               const opcode_t& trailing) const {
      opcode_t buf;
      if (m.addr32) { buf.push_back(0x67); }
      buf.insert(buf.end(), m.prefixes.begin(), m.prefixes.end());
      const uint8_t rex = m.rex | (reg & 8 ? 0x04 : 0);
      if (rex) { buf.push_back(0x40 | rex); }
      buf.insert(buf.end(), opcode.begin(), opcode.end());
      buf.push_back((uint8_t)(m.operand[0] | ((reg & 7) << 3)));
      buf.insert(buf.end(), m.operand.begin() + 1, m.operand.end());
      buf.insert(buf.end(), trailing.begin(), trailing.end());
      auto *inst = new Instruction<opposite<bits>>(buf);
      inst->memidx = 0;
      if (m.bind != LoweredMem::NONE) {
         env.resolve(memdisp, &inst->memdisp);
         inst->memdisp_offset = memdisp_offset;
         inst->memdisp_absolute = m.bind == LoweredMem::ABS;
      }
      typename SectionBlob<bits>::SectionBlobs insts = m.pre;
      insts.push_back(inst);
      return insts;
   }

   template <Bits bits>
   const xed_state_t& Instruction<bits>::dstate() { return dstate_<bits>; }

   /* ---- Is a 32-bit literal an address? ------------------------------------
    *
    * An i386 image with no relocation for a site (every fixed-load executable)
    * gives no record of which literals are addresses, so each operand shape
    * below decides from the value and the image. Two rules hold throughout:
    *
    *  - The verdict must be a PURE FUNCTION of value + image, never of parse
    *    order or of which iform carries the literal. Otherwise the two sides of
    *    a compare (`mov $X,%edx` ... `cmp $X,%edx`) classify differently and a
    *    test that held on i386 silently fails (Civ IV's 0xffff static-init
    *    priority skipped a TU's constructors).
    *  - A value aliasing an INSTRUCTIONS section is an integer unless there is
    *    positive function-ENTRY evidence at it (a symbol, or an entry shape).
    *    Small integers alias a large __text constantly.
    */

   /* An imm32 that aliases code with no entry evidence is a constant. */
   template <Bits bits>
   static bool imm32_code_alias_is_constant(const Image& img, ParseEnv<bits>& env,
                                            uint32_t imm_val) {
      if (!env.code_alias_is_constant(imm_val) &&
          !env.stackarg_imm_is_code_constant(imm_val)) {
         return false;
      }
      return !env.code_target_has_entry_evidence(img, (std::size_t) imm_val);
   }

   /* An i386 value that can name a pointer operand: past the first page and
    * in the low 2GB. The floor stays 0x1000 even for a base-0 dylib (DataParser
    * uses min(0x1000, lowest section) for data words): in instruction operands,
    * small integer immediates vastly outnumber pointers into the first page. */
   static inline bool ptr32_range(std::size_t v) { return v >= 0x1000 && v < 0x80000000U; }

   /* The same rule for the disp32 of a BASE-register operand, where disp is
    * far more often a struct offset or extent than a table address (Civ IV
    * `lea 0x27ec(%ecx),%edx`, a free-list terminator). A load/store at
    * disp(%base) never indexes data from a function's entry, so for anything
    * but `lea` (which can form a function pointer) any code alias is an
    * integer (PvZ `movl %edx,0x558c(%eax)`). Guard memdisp_code_alias_test.sh;
    * M64_MEMDISP_ACCESS_ENTRY_RULE=1 applies the entry rule to accesses too. */
   template <Bits bits>
   static bool memdisp_code_alias_is_constant(const Image& img, ParseEnv<bits>& env,
                                              std::size_t disp, bool is_lea) {
      static const bool disabled =
         std::getenv("M64_NO_MEMDISP_CODE_ALIAS_GATE") != nullptr;
      static const bool access_entry_rule =
         std::getenv("M64_MEMDISP_ACCESS_ENTRY_RULE") != nullptr;
      if (disabled || bits != Bits::M32) { return false; }
      if (!ptr32_range(disp)) { return false; }
      if (!is_lea && !access_entry_rule && env.vmaddr_in_instructions_sect(disp)) {
         return true;
      }
      return imm32_code_alias_is_constant(img, env, (uint32_t) disp);
   }

   /* A code address stored into (or compared against) a struct field is a
    * callback install `obj->cb = &f`. Evidence: a symbol at the value, or an
    * entry shape that nothing falls through into (stripped images carry no
    * symbols for their own functions: Halo 0 of 207, Civ IV 0 of 517). A
    * page-multiple value is refused: `obj->cap = 0x100000` and a function that
    * genuinely starts at 0x100000 look identical, and corrupting a live size
    * costs more than missing an install (iPhoto). Guards 99_fnptr_field_call,
    * 99_code_alias_imm_falsereloc. */
   template <Bits bits>
   static bool field_store_code_target_is_fnptr(const Image& img,
                                                ParseEnv<bits>& env,
                                                uint32_t value) {
      if (!ptr32_range(value)) { return false; }
      if (!env.vmaddr_in_instructions_sect(value)) { return false; }
      if (env.func_syms.count(value) != 0) { return true; }
      if ((value & 0xfffU) == 0) { return false; }
      return env.code_target_has_entry_evidence(img, (std::size_t) value) &&
             env.code_target_is_function_start(img, (std::size_t) value);
   }

   template <Bits bits>
   bool Instruction<bits>::CanDecode(const Image& img, const Location& loc) {
      xed_decoded_inst_t xedd;
      xed_decoded_inst_zero_set_mode(&xedd, &dstate());
      xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_INVALID);
      return xed_decode(&xedd, &img.at<uint8_t>(loc.offset),
                        img.size() - loc.offset) == XED_ERROR_NONE;
   }

   /* True iff the word at `value` (in file-backed, non-code data) is itself an
    * in-image address: the first slot of a vtable or fn-ptr table. An integer
    * that merely aliases __DATA,__const points at scalar bytes instead. */
   template <Bits bits>
   static bool points_at_pointer(const Image& img, ParseEnv<bits>& env, uint32_t value) {
      for (Segment<bits> *seg : env.archive.segments()) {
         const auto& sc = seg->segment_command;
         const std::string sn(sc.segname, strnlen(sc.segname, sizeof(sc.segname)));
         if (sn == SEG_PAGEZERO || sn == SEG_LINKEDIT) { continue; }
         if ((sc.initprot & VM_PROT_EXECUTE) != 0) { continue; }
         if (!seg->contains_vmaddr(value)) { continue; }
         const std::size_t toff = value - sc.vmaddr + sc.fileoff;
         if (toff + 4 > sc.fileoff + sc.filesize || toff + 4 > img.size()) { return false; }
         const uint32_t tword = img.template at<uint32_t>(toff);
         return ptr32_range(tword) && env.vmaddr_in_image(tword);
      }
      return false;
   }

   /* Offset of the rel32/disp32 a classic relocation may cover, or 0. */
   static std::size_t reloc_field_offset(xed_iform_enum_t iform) {
      switch (iform) {
      case XED_IFORM_CALL_NEAR_RELBRz:
      case XED_IFORM_JMP_RELBRz:
         return 1;
      case XED_IFORM_JZ_RELBRz:  case XED_IFORM_JNZ_RELBRz:
      case XED_IFORM_JL_RELBRz:  case XED_IFORM_JLE_RELBRz:
      case XED_IFORM_JNL_RELBRz: case XED_IFORM_JNLE_RELBRz:
      case XED_IFORM_JNBE_RELBRz: case XED_IFORM_JBE_RELBRz:
      case XED_IFORM_JB_RELBRz:  case XED_IFORM_JNB_RELBRz:
      case XED_IFORM_JS_RELBRz:  case XED_IFORM_JNS_RELBRz:
      case XED_IFORM_JP_RELBRz:  case XED_IFORM_JNP_RELBRz:
      case XED_IFORM_JO_RELBRz:  case XED_IFORM_JNO_RELBRz:
         return 2;
      case XED_IFORM_LEA_GPRv_AGEN:
         return 3;
      default:
         return 0;
      }
   }

   template <Bits bits>
   Instruction<bits>::Instruction(const Image& img, const Location& loc, ParseEnv<bits>& env,
                                  bool add_to_map):
      SectionBlob<bits>(loc, env, add_to_map), memdisp(nullptr), imm(nullptr), brdisp(nullptr)
   {
      xed_decoded_inst_zero_set_mode(&xedd, &dstate());
      xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_INVALID);
      const xed_error_enum_t err =
         xed_decode(&xedd, &img.at<uint8_t>(loc.offset), img.size() - loc.offset);
      if (err != XED_ERROR_NONE) {
         throw error("%s: offset 0x%x: xed_decode: %s", __FUNCTION__, loc.offset,
                     xed_error_enum_t2str(err));
      }
      instbuf = std::vector<uint8_t>(&img.at<uint8_t>(loc.offset),
                                     &img.at<uint8_t>(loc.offset + xed_decoded_inst_get_length(&xedd)));

      const std::size_t refaddr = loc.vmaddr + xed_decoded_inst_get_length(&xedd);
      xed_operand_values_t *operands = xed_decoded_inst_operands(&xedd);
      const xed_iclass_enum_t iclass = xed_decoded_inst_get_iclass(&xedd);
      const bool fixed_load = env.fixed_load_image();

      /* A value that could name something in this image. */
      auto image_addr = [&](std::size_t v) {
         return ptr32_range(v) && env.vmaddr_in_image(v);
      };
      /* Bind this->memdisp to the table at `addr`; an address inside a blob
       * (a zerofill extent, a packed const table) resolves to blob + offset. */
      auto capture_table = [&](unsigned i, std::size_t addr) {
         memidx = i;
         memdisp_absolute = true;
         env.vmaddr_resolver.resolve(addr, (const SectionBlob<bits> **) &this->memdisp);
         if (env.vmaddr_in_indexed_table_target(addr)) {
            env.vmaddr_resolver.resolve_containing(
               addr, (const SectionBlob<bits> **) &this->memdisp, &this->memdisp_offset);
         }
      };
      auto parse_imm = [&](std::size_t off, bool is_ptr) {
         imm = Immediate<bits>::Parse(img, loc + off, env, is_ptr);
         imm->heuristic = true;   /* a value-alias guess; see Immediate::heuristic */
      };

      if (const std::size_t roff = reloc_field_offset(xed_decoded_inst_get_iform_enum(&xedd))) {
         auto relocs_it = env.relocs.find(loc.vmaddr + roff);
         if (relocs_it != env.relocs.end()) {
            reloc = relocs_it->second;
            env.relocs.erase(relocs_it);
            return;
         }
      }

      /* ---- memory operands ---- */
      const unsigned int nops = xed_decoded_inst_noperands(&xedd);
      if (xed_operand_values_has_memory_displacement(operands)) {
         for (unsigned i = 0; i < nops; ++i) {
            const xed_reg_enum_t basereg  = xed_decoded_inst_get_base_reg(operands,  i);
            const xed_reg_enum_t indexreg = xed_decoded_inst_get_index_reg(operands, i);
            const bool disp32 =
               xed_decoded_inst_get_memory_displacement_width(operands, i) == sizeof(uint32_t);
            const ssize_t disp = xed_decoded_inst_get_memory_displacement(operands, i);

            if (basereg == select_value(bits, XED_REG_EIP, XED_REG_RIP) &&
                indexreg == XED_REG_INVALID) {
               /* [rip+disp] (the M64 re-parse). */
               if (memdisp) {
                  throw error("%s: duplicate rip-relative memdisp at offset 0x%jx, "
                              "vmaddr 0x%jx", __FUNCTION__, (uintmax_t) loc.offset,
                              (uintmax_t) loc.vmaddr);
               }
               memidx = i;
               const std::size_t targetaddr = refaddr + disp;
               this->memdisp = env.add_placeholder(targetaddr);
               /* An interior target of opaque data binds to blob + offset, so it
                * survives re-layout; a mid-blob placeholder would drift to the
                * next blob (Quinn `movswl _pieceSize1+2`). __OBJC and code stay
                * exact: their interior offsets don't survive the rewrite. */
               if (env.vmaddr_in_writable_data(targetaddr) ||
                   env.vmaddr_in_readonly_opaque_data(targetaddr)) {
                  env.vmaddr_resolver.resolve_containing(
                     targetaddr, (const SectionBlob<bits> **) &this->memdisp,
                     &this->memdisp_offset, /*override=*/true);
               }

            } else if (basereg == XED_REG_INVALID && indexreg == XED_REG_INVALID && disp32) {
               /* Absolute [disp32] (i386 mod=00 r/m=101, rip-relative in x86_64).
                * With a trailing immediate the disp is NOT the last 4 bytes:
                * capture it as memdisp and the immediate separately — but only
                * when it names data; linear sweep sometimes decodes junk as
                * `c7 05 ...` aimed at dyld's stub tables. Literal pools are data
                * by section type (Halo's `cmpss xmm,[abs32]` float constants;
                * guard literal_abs_disp_test.sh). */
               const bool has_any_imm = xed_operand_values_has_immediate(operands);
               const unsigned imm_w = has_any_imm
                  ? xed_decoded_inst_get_immediate_width_bits(operands) : 0;
               const bool has_imm32 = has_any_imm && imm_w == 32;
               const bool has_small_imm = has_any_imm && (imm_w == 8 || imm_w == 16);
               bool dest_is_data = false;
               if (has_imm32 || has_small_imm) {
                  static const bool literal_abs_off =
                     std::getenv("M64_NO_LITERAL_ABS_DISP") != nullptr;
                  if (const Section<bits> *sect = env.section_at((std::size_t) disp)) {
                     const uint32_t stype = sect->sect.flags & SECTION_TYPE;
                     const bool pool =
                        stype == S_CSTRING_LITERALS || stype == S_4BYTE_LITERALS ||
                        stype == S_8BYTE_LITERALS || stype == S_16BYTE_LITERALS ||
                        stype == S_LITERAL_POINTERS;
                     /* dyld slots are data too (Halo `cmpl $func, nl_ptr`) */
                     const bool slot = stype == S_NON_LAZY_SYMBOL_POINTERS ||
                                       stype == S_LAZY_SYMBOL_POINTERS;
                     dest_is_data = stype == S_REGULAR || stype == S_ZEROFILL ||
                                    stype == S_GB_ZEROFILL || slot ||
                                    (pool && !literal_abs_off);
                  }
               }
               if (has_imm32 && dest_is_data) {
                  memidx = i;
                  memdisp_absolute = true;
                  env.vmaddr_resolver.resolve((std::size_t) disp,
                                              (const SectionBlob<bits> **) &this->memdisp);
                  env.vmaddr_resolver.resolve_containing(
                     (std::size_t) disp, (const SectionBlob<bits> **) &this->memdisp,
                     &this->memdisp_offset);
                  const std::size_t imm_idx = instbuf.size() - sizeof(uint32_t);
                  const uint32_t imm_val = img.template at<uint32_t>(loc.offset + imm_idx);
                  bool imm_is_ptr = image_addr(imm_val);
                  if (imm_is_ptr && bits == Bits::M32 &&
                      imm32_code_alias_is_constant(img, env, imm_val)) {
                     imm_is_ptr = false;
                  }
                  /* `cmp/test [abs32], $X`: X is a comparison value or mask far
                   * more often than an address (Civ IV `cmpl $0x100308,_version`).
                   * Keep a pointer only with a symbol at X, or — CMP only — the
                   * exact store-side discriminator (aligned, writable data,
                   * fixed-load image), so a pointer-identity test classifies
                   * like the store it tests (guards 99_cmp_abs32_imm_notptr,
                   * 99_cmp_mem_ptr_imm). */
                  if (imm_is_ptr && bits == Bits::M32 &&
                      (iclass == XED_ICLASS_CMP || iclass == XED_ICLASS_TEST)) {
                     const bool cmp_data_identity =
                        iclass == XED_ICLASS_CMP && fixed_load && (imm_val & 3) == 0 &&
                        env.vmaddr_in_writable_data(imm_val);
                     if (env.func_syms.count(imm_val) == 0 && !cmp_data_identity) {
                        imm_is_ptr = false;
                     }
                  }
                  parse_imm(imm_idx, imm_is_ptr);
               } else if (has_small_imm && dest_is_data) {
                  memidx = i;
                  memdisp_absolute = true;
                  env.vmaddr_resolver.resolve((std::size_t) disp,
                                              (const SectionBlob<bits> **) &this->memdisp);
                  env.vmaddr_resolver.resolve_containing(
                     (std::size_t) disp, (const SectionBlob<bits> **) &this->memdisp,
                     &this->memdisp_offset);
               } else if (!has_any_imm) {
                  /* plain load/store: the disp32 is the last 4 bytes (with an
                   * immediate those are the immediate, not the address) */
                  imm = Immediate<bits>::Parse(img, loc + (instbuf.size() - sizeof(uint32_t)),
                                               env, true);
               }

            } else if (basereg == XED_REG_INVALID && indexreg != XED_REG_INVALID && disp32) {
               /* [disp32 + idx*scale]: a dereferenced table base, never an
                * integer — except under `lea`, which computes `C + 4*idx`
                * arithmetic. A PIC image never names an address with a literal,
                * so there it is always the arithmetic (Portal 2 libtogl
                * `lea 0x88e4(,%eax,4)` = GL_STATIC_DRAW + 4*bool). */
               const bool lea_const = iclass == XED_ICLASS_LEA && !fixed_load;
               if (!lea_const && ptr32_range((std::size_t) disp) && !memdisp) {
                  capture_table(i, (std::size_t) disp);
               }

            } else if (fixed_load && disp32 &&
                       basereg != XED_REG_INVALID &&
                       basereg != select_value(bits, XED_REG_EIP, XED_REG_RIP) &&
                       basereg != select_value(bits, XED_REG_ESP, XED_REG_RSP) &&
                       basereg != select_value(bits, XED_REG_EBP, XED_REG_RBP)) {
               /* disp32(%base[,%idx,s]) in a fixed-load image: the disp may be
                * a global table's absolute address (photocd `movl %edi,
                * 0x11260(%edx)`; guard 89_abs_base_index_disp). PIC code uses
                * this shape only for struct fields, and a frame/stack base
                * never indexes a global. DetectPicAnchoredDisps (2d) cancels it
                * inside PIC-anchored functions. */
               if (image_addr((std::size_t) disp) &&
                   !memdisp_code_alias_is_constant(img, env, (std::size_t) disp,
                                                   iclass == XED_ICLASS_LEA) &&
                   !memdisp) {
                  capture_table(i, (std::size_t) disp);
               }
            }
         }
      }

      /* ---- relative branches ---- */
      if (xed_operand_values_has_branch_displacement(operands)) {
         std::size_t targetaddr = refaddr + xed_decoded_inst_get_branch_displacement(operands);
         /* A classic `__IMPORT,__jump_table` stub is dead on modern dyld: a
          * branch to a DEFINED stub goes straight to its function, one to an
          * UNDEFINED stub to its synthesized __jt_tramp trampoline (bound
          * directly: the dead stub still owns its vmaddr, so a placeholder
          * would land on it). See Dysymtab::lift_jump_table_targets. */
         auto jtt = env.jump_table_targets.find(targetaddr);
         if (jtt != env.jump_table_targets.end()) {
            targetaddr = jtt->second;
         }
         auto jtu = env.jump_table_undef_tramps.find(targetaddr);
         if (jtu != env.jump_table_undef_tramps.end()) {
            this->brdisp = jtu->second;
         } else if (env.vmaddr_in_image(targetaddr)) {
            /* data misdecoded as a branch points nowhere; its bytes stay put */
            this->brdisp = env.add_placeholder(targetaddr);
         }
      }

      /* ---- immediates ---- */
      switch (xed_decoded_inst_get_iform_enum(&xedd)) {
      /* `push $X` / `mov $X,%reg` / `add $X,%reg`: a fixed-load image names its
       * own globals and code by literal address (`mov $0xf4c500,%ebx`), with no
       * relocation to mark it. Only full 32-bit immediates can be addresses. */
      case XED_IFORM_PUSH_IMMz:
      case XED_IFORM_MOV_GPRv_IMMv:
      case XED_IFORM_ADD_GPRv_IMMz:
      case XED_IFORM_ADD_OrAX_IMMz:
         {
            assert(imm == nullptr);
            const std::size_t imm_idx = instbuf.size() - sizeof(uint32_t);
            bool imm_is_ptr = false;
            if (fixed_load && xed_decoded_inst_get_immediate_width_bits(operands) == 32) {
               const uint32_t imm_val = img.template at<uint32_t>(loc.offset + imm_idx);
               imm_is_ptr = image_addr(imm_val) &&
                  !(bits == Bits::M32 && imm32_code_alias_is_constant(img, env, imm_val));
               /* a later loop-bounding `cmp $&table_end,%reg` moves with it */
               if (imm_is_ptr && bits == Bits::M32) {
                  env.relocated_ptr_imms.insert(imm_val);
               }
            }
            parse_imm(imm_idx, imm_is_ptr);
         }
         break;

      /* `cmp $X,%reg`: a pointer only as the end sentinel of a table whose base
       * was relocated (Halo static-init table walk), or under the store-side
       * discriminator so an identity compare classifies like the store
       * regardless of parse order (guard 99_cmp_reg_ptr_imm_order). */
      case XED_IFORM_CMP_GPRv_IMMz:
      case XED_IFORM_CMP_OrAX_IMMz:
         {
            assert(imm == nullptr);
            const std::size_t imm_idx = instbuf.size() - sizeof(uint32_t);
            bool imm_is_ptr = false;
            if (fixed_load && bits == Bits::M32 &&
                xed_decoded_inst_get_immediate_width_bits(operands) == 32) {
               const uint32_t imm_val = img.template at<uint32_t>(loc.offset + imm_idx);
               imm_is_ptr = ptr32_range(imm_val) &&
                  !imm32_code_alias_is_constant(img, env, imm_val) &&
                  (env.imm_bounds_relocated_table(imm_val) ||
                   ((imm_val & 3) == 0 && env.vmaddr_in_writable_data(imm_val)));
            }
            parse_imm(imm_idx, imm_is_ptr);
         }
         break;
      default:
         break;
      }

      /* `movl $X, disp(%esp|%ebp)` — i386 stack-argument setup (printf format
       * strings, callbacks). Any in-image value counts, except a code alias
       * with no entry evidence (guards 01_hello, 03_stack_imm_args,
       * 74_stripped_stackarg_const). */
      if (imm == nullptr && instbuf.size() >= 7 && instbuf.at(0) == 0xc7) {
         const uint8_t modrm = instbuf.at(1);
         const uint8_t mod = (modrm >> 6) & 0x3;
         const uint8_t rm  = modrm & 0x07;
         std::size_t imm_off = 0;
         if ((modrm & 0x38) == 0 && rm == 0x04 && instbuf.at(2) == 0x24) {
            if (mod == 0 && instbuf.size() == 7)       imm_off = 3;
            else if (mod == 1 && instbuf.size() == 8)  imm_off = 4;
            else if (mod == 2 && instbuf.size() == 11) imm_off = 7;
         } else if ((modrm & 0x38) == 0 && rm == 0x05) {
            if (mod == 1 && instbuf.size() == 7)       imm_off = 3;
            else if (mod == 2 && instbuf.size() == 10) imm_off = 6;
         }
         if (imm_off > 0) {
            const uint32_t value = img.template at<uint32_t>(loc.offset + imm_off);
            if (image_addr(value) &&
                !(bits == Bits::M32 && imm32_code_alias_is_constant(img, env, value))) {
               parse_imm(imm_off, true);
            }
         }
      }

      /* `movl $X, disp(%reg[,%idx,s])` through a general base: a field store,
       * as often an integer as a pointer. X is a pointer if it names a string/
       * ObjC object section, a pointer table (its first word is itself an
       * in-image address: `movl $vtable,(%eax)`), writable data in a
       * fixed-load image (4-aligned; guards 99_zerofill_target_imm_store,
       * 99_sib_ptr_imm_store), or a function entry. The no-base
       * `disp32(,%idx,s)` shape belongs to the absolute-table capture. */
      if (imm == nullptr && instbuf.size() >= 6 && instbuf.at(0) == 0xc7
          && (instbuf.at(1) & 0x38) == 0          /* c7 /0 = mov r/m32, imm32 */
          && (instbuf.at(1) >> 6) != 3) {         /* memory destination */
         const uint8_t mod = instbuf.at(1) >> 6;
         const uint8_t rm = instbuf.at(1) & 0x07;
         bool genbase = rm != 0x04 && rm != 0x05;   /* esp/ebp: stack-arg arm */
         if (rm == 0x04 && instbuf.size() >= 7) {
            genbase = !(mod == 0 && (instbuf.at(2) & 0x07) == 0x05);
         }
         if (genbase && xed_operand_values_has_immediate(operands)
             && xed_decoded_inst_get_immediate_width_bits(operands) == 32) {
            const std::size_t imm_off = instbuf.size() - sizeof(uint32_t);
            const uint32_t value = img.template at<uint32_t>(loc.offset + imm_off);
            bool ptr_target = false;
            if (ptr32_range(value)) {
               ptr_target = env.vmaddr_in_const_section(value);
               if (!ptr_target && (value & 3) == 0) {
                  ptr_target = points_at_pointer(img, env, value);
               }
               if (!ptr_target && (value & 3) == 0 && fixed_load &&
                   env.vmaddr_in_writable_data(value)) {
                  ptr_target = true;
               }
               if (!ptr_target && bits == Bits::M32 &&
                   field_store_code_target_is_fnptr(img, env, value)) {
                  ptr_target = true;
               }
            }
            if (ptr_target) {
               parse_imm(imm_off, true);
            }
         }
      }

      /* `cmp/add/sub $X, disp(%reg)` (81 /7, /0, /5): the compare and
       * offset<->pointer siblings of the field store above, under its strict
       * data discriminator so both sides of an identity test agree (guards
       * 99_cmp_mem_ptr_imm, 99_alu_mem_ptr_imm). CMP also admits a function
       * entry (`cmpl $_handler, field`). A mask or bit-op against a pointer is
       * meaningless, so AND/OR/XOR/TEST never capture. */
      if (imm == nullptr && this->memdisp == nullptr && bits == Bits::M32
          && instbuf.size() >= 6 && instbuf.at(0) == 0x81
          && ((instbuf.at(1) & 0x38) == 0x38      /* CMP */
              || (instbuf.at(1) & 0x38) == 0x00   /* ADD */
              || (instbuf.at(1) & 0x38) == 0x28)  /* SUB */
          && (instbuf.at(1) >> 6) != 3) {
         const uint8_t mod = instbuf.at(1) >> 6;
         const uint8_t rm  = instbuf.at(1) & 0x07;
         bool reg_base = !(mod == 0 && rm == 5);
         if (mod == 0 && rm == 4 && instbuf.size() >= 3 && (instbuf.at(2) & 0x07) == 5) {
            reg_base = false;
         }
         if (reg_base
             && xed_operand_values_has_immediate(operands)
             && xed_decoded_inst_get_immediate_width_bits(operands) == 32) {
            const std::size_t imm_off = instbuf.size() - sizeof(uint32_t);
            const uint32_t value = img.template at<uint32_t>(loc.offset + imm_off);
            bool cap = false;
            if (ptr32_range(value)) {
               cap = (value & 3) == 0 && fixed_load && env.vmaddr_in_writable_data(value);
               if (!cap && (instbuf.at(1) & 0x38) == 0x38) {
                  cap = field_store_code_target_is_fnptr(img, env, value);
               }
            }
            if (cap) {
               parse_imm(imm_off, true);
            }
         }
      }
   }

   template <Bits bits>
   void Instruction<bits>::Emit(Image& img, std::size_t offset) const {
      /* patch instruction */
      xed_decoded_inst_t xedd = this->xedd;
      std::vector<uint8_t> instbuf = this->instbuf;

      if (memdisp) {
         const unsigned width_bits =
            xed_decoded_inst_get_memory_displacement_width_bits(&xedd, memidx);
         /*
          * Two flavors:
          *   - rip-relative (`[rip+disp]`): disp = target - rip_after_instr
          *   - absolute    (`[disp32+idx*N]`): disp = target (sign-extended
          *     to 64-bit by the CPU at decode time)
          */
         const std::size_t disp =
            memdisp_absolute
            ? memdisp->loc.vmaddr + memdisp_offset
            : memdisp->loc.vmaddr + memdisp_offset
                 - (ssize_t) (this->loc.vmaddr + size());
         xed_enc_displacement_t enc;
         enc.displacement = disp;
         enc.displacement_bits = width_bits;
         if (!xed_patch_disp(&xedd, &*instbuf.begin(), enc)) {
            throw error("%s: xed_patch_disp: failed to patch instruction at offset 0x%zx, " \
                        "vmaddr 0x%zx, iform %s\n", __FUNCTION__, this->loc.offset,
                        this->loc.vmaddr, xed_iform_enum_t2str(xed_decoded_inst_get_iform_enum(&xedd)));
         }
      }
      
      if (brdisp) {
         const ssize_t disp = brdisp->loc.vmaddr - (ssize_t) (this->loc.vmaddr + size());
         const unsigned width_bits = xed_decoded_inst_get_branch_displacement_width_bits(&xedd);
         xed_encoder_operand_t enc;
         enc.u.brdisp = disp;
         enc.width_bits = width_bits;
         if (!xed_patch_brdisp(&xedd, &*instbuf.begin(), enc)) {
            throw error("%s: xed_patch_brdisp: failed to patch instruction at offset 0x%zx, " \
                        "vmaddr 0x%zx, iform %s, width %u, disp %zd, target 0x%zx, orig 0x%zx\n",
                        __FUNCTION__, this->loc.offset, this->loc.vmaddr,
                        xed_iform_enum_t2str(xed_decoded_inst_get_iform_enum(&xedd)),
                        width_bits, disp, (std::size_t)brdisp->loc.vmaddr,
                        (std::size_t)this->orig_vmaddr);
         }
      }

      /* emit instruction bytes */
      img.copy(offset, &*instbuf.begin(), instbuf.size());

      if (imm) {
         imm->Emit(img, offset + instbuf.size() - imm->size());
      }
   }

   template <Bits bits>
   void Instruction<bits>::Build(BuildEnv<bits>& env) {
      parse_handle_relbr();
      
      SectionBlob<bits>::Build(env);
      if (imm) {
         BuildEnv immenv(env.archive, env.loc - imm->size());
         imm->Build(immenv);
      }
   }

   template <Bits bits>
   typename SectionBlob<bits>::SectionBlobs Instruction<bits>::Transform(TransformEnv<bits>& env)
      const
   {
      assert(bits == Bits::M32);

      if (imm && imm->pointee) {
         /* The immediate names something in this image. i386 carried the
          * address as a literal; x86_64 computes it rip-relative so it tracks
          * both the new layout and the load slide. The translated image lives
          * below 4GB, so the low 32 bits of a computed address are the whole
          * pointer. */
         env.template add<Immediate>(imm, nullptr);

         /* A new instruction whose rip-relative disp32 binds to the pointee. */
         auto rip_to_pointee = [&](const opcode_t& bytes) {
            auto *inst = new Instruction<opposite<bits>>(bytes);
            inst->memidx = 0;
            env.resolve(imm->pointee, &inst->memdisp);
            inst->memdisp_offset = imm->pointee_offset;
            return inst;
         };
         /* reg0 of a `op r32, $ptr` form; a 16-bit form cannot hold a pointer */
         auto reg32 = [&](const char *what) {
            const auto r32 = xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0);
            if (r32 < XED_REG_EAX || r32 > XED_REG_EDI) {
               throw error("%s: %s at vmaddr 0x%zx: reg0=%s is not a 32-bit GPR",
                           __FUNCTION__, what, this->loc.vmaddr, xed_reg_enum_t2str(r32));
            }
            return r32;
         };

         switch (xed_decoded_inst_get_iform_enum(&xedd)) {
         case XED_IFORM_PUSH_IMMz:
            {
               /* lea r11,[rip+ptr]; push. A stub pushes the lazy-bind argument
                * for native dyld_stub_binder (8 bytes); anything else is an
                * i386 argument (4 bytes). */
               auto lea_inst = rip_to_pointee(opcode::lea_r11_mem_rip_disp32());
               const std::string sect = this->section->name();
               if (sect == SECT_SYMBOL_STUB || sect == SECT_STUB_HELPER) {
                  return {lea_inst, new Instruction<opposite<bits>>(opcode::push_r11())};
               }
               if constexpr (bits == Bits::M32) {
                  auto insts = push_r32(XED_REG_R11D);
                  insts.push_front(lea_inst);
                  return insts;
               } else {
                  throw error("PUSH_IMMz in M64 transform unreachable");
               }
            }

         case XED_IFORM_MOV_GPRv_IMMv:   /* lea r32,[rip+ptr] */
            return {rip_to_pointee(opcode::lea_r32_mem_rip_disp32(reg32("MOV imm-ptr")))};

         case XED_IFORM_ADD_GPRv_IMMz:   /* lea r11,[rip+ptr]; add r32,r11d */
         case XED_IFORM_ADD_OrAX_IMMz:
            {
               const auto r32 = reg32("ADD imm-ptr");
               return {rip_to_pointee(opcode::lea_r11_mem_rip_disp32()),
                       new Instruction<opposite<bits>>(opcode::add_r32_r11d(r32))};
            }

         case XED_IFORM_CMP_GPRv_IMMz:   /* lea r11,[rip+ptr]; cmp r32,r11d */
         case XED_IFORM_CMP_OrAX_IMMz:
            {
               const auto r32 = reg32("CMP imm-ptr");
               return {rip_to_pointee(opcode::lea_r11_mem_rip_disp32()),
                       new Instruction<opposite<bits>>(opcode::cmp_r32_r11d(r32))};
            }

         /* mov between a register and [abs32]. The short moffs forms (A0-A3)
          * take an 8-byte address in x86_64, so all of them become the ModR/M
          * rip-relative form `66? 8A/8B/88/89 /r [rip+disp32]`. */
         case XED_IFORM_MOV_OrAX_MEMv:
         case XED_IFORM_MOV_GPRv_MEMv:
         case XED_IFORM_MOV_MEMv_OrAX:
         case XED_IFORM_MOV_MEMv_GPRv:
            {
               const bool load = xed_decoded_inst_get_iform_enum(&xedd) == XED_IFORM_MOV_OrAX_MEMv ||
                                 xed_decoded_inst_get_iform_enum(&xedd) == XED_IFORM_MOV_GPRv_MEMv;
               const auto r = xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0);
               opcode_t bytes;
               if (r >= XED_REG_AX && r <= XED_REG_DI) {
                  bytes = {0x66, (uint8_t)(load ? 0x8B : 0x89),
                           (uint8_t)(0x05 | ((r - XED_REG_AX) << 3)), 0, 0, 0, 0};
               } else if (r >= XED_REG_EAX && r <= XED_REG_EDI) {
                  bytes = load ? opcode::mov_r32_mem_rip_disp32(r)
                               : opcode::mov_mem_rip_disp32_r32(r);
               } else {
                  throw error("%s: MOV [abs32] with reg %s unsupported",
                              __FUNCTION__, xed_reg_enum_t2str(r));
               }
               return {rip_to_pointee(bytes)};
            }
         case XED_IFORM_MOV_AL_MEMb:
            return {rip_to_pointee(opcode_t{0x8A, 0x05, 0, 0, 0, 0})};
         case XED_IFORM_MOV_MEMb_AL:
            return {rip_to_pointee(opcode_t{0x88, 0x05, 0, 0, 0, 0})};

         /* `<op> r/m32, $ptr` for the whole immediate group (C7 /0 mov,
          * 81 /0../7, F7 /0-1 test): compute the relocated pointer in a
          * scratch register and re-encode the operation in its MR form
          * (op [mem], r32) against the same destination. lea leaves EFLAGS
          * alone, so the flag-setting members stay exact. No pointer
          * immediate survives in __text, so every later M64 re-parse
          * re-resolves both references (guards 86_selfref_imm_store,
          * selfref_imm_stage_shift_test.sh, 87_alu_absdest_imm_ptr). */
         case XED_IFORM_ADD_MEMv_IMMz:
         case XED_IFORM_OR_MEMv_IMMz:
         case XED_IFORM_ADC_MEMv_IMMz:
         case XED_IFORM_SBB_MEMv_IMMz:
         case XED_IFORM_AND_MEMv_IMMz:
         case XED_IFORM_SUB_MEMv_IMMz:
         case XED_IFORM_XOR_MEMv_IMMz:
         case XED_IFORM_CMP_MEMv_IMMz:
         case XED_IFORM_TEST_MEMv_IMMz_F7r0:
         case XED_IFORM_TEST_MEMv_IMMz_F7r1:
         case XED_IFORM_MOV_MEMv_IMMz:
            {
               uint8_t mr_op;
               switch (xed_decoded_inst_get_iform_enum(&xedd)) {
               case XED_IFORM_ADD_MEMv_IMMz: mr_op = 0x01; break;
               case XED_IFORM_OR_MEMv_IMMz:  mr_op = 0x09; break;
               case XED_IFORM_ADC_MEMv_IMMz: mr_op = 0x11; break;
               case XED_IFORM_SBB_MEMv_IMMz: mr_op = 0x19; break;
               case XED_IFORM_AND_MEMv_IMMz: mr_op = 0x21; break;
               case XED_IFORM_SUB_MEMv_IMMz: mr_op = 0x29; break;
               case XED_IFORM_XOR_MEMv_IMMz: mr_op = 0x31; break;
               case XED_IFORM_CMP_MEMv_IMMz: mr_op = 0x39; break;
               case XED_IFORM_TEST_MEMv_IMMz_F7r0:
               case XED_IFORM_TEST_MEMv_IMMz_F7r1: mr_op = 0x85; break;
               default:                      mr_op = 0x89; break;   /* MOV */
               }
               LoweredMem m;
               if (lower_mem(env, m)) {
                  /* the value goes in r10 when the operand itself needs r11 */
                  const bool r10 = !m.pre.empty();
                  auto *lea = new Instruction<opposite<bits>>(
                     r10 ? opcode_t{0x4C, 0x8D, 0x15, 0, 0, 0, 0}
                         : opcode::lea_r11_mem_rip_disp32());
                  lea->memidx = 0;
                  env.resolve(imm->pointee, &lea->memdisp);
                  lea->memdisp_offset = imm->pointee_offset;
                  auto insts = emit_mem(env, m, opcode_t{mr_op}, r10 ? 10 : 11, {});
                  insts.push_front(lea);
                  return insts;
               }
               /* Unparseable operand: clone, resolving the destination and the
                * pointer immediate separately (translate-time-correct only). */
               auto *clone = new Instruction<opposite<bits>>(instbuf);
               clone->memidx = 0;
               if (this->memdisp) {
                  env.resolve(this->memdisp, &clone->memdisp);
                  clone->memdisp_offset = this->memdisp_offset;
               }
               clone->imm = imm->Transform_one(env);
               return {clone};
            }

         /* `jmp *[abs32]` / `call *[abs32]`: a promoted 8-byte dyld slot
          * (lazy/non-lazy symbol pointers) keeps the byte-identical rip-relative
          * form; the target is native and returns with an 8-byte ret. Any other
          * slot is 4 bytes: load it into a register no i386 code owns (r11 for
          * jmp; r10 for call, whose return-address push needs r11). */
         case XED_IFORM_JMP_MEMv:
         case XED_IFORM_CALL_NEAR_MEMv:
            if constexpr (bits == Bits::M32) {
               const bool is_call =
                  xed_decoded_inst_get_iform_enum(&xedd) == XED_IFORM_CALL_NEAR_MEMv;
               bool slot_8byte = false;
               if (imm->pointee->section) {
                  const uint32_t stype = imm->pointee->section->sect.flags & SECTION_TYPE;
                  slot_8byte = stype == S_LAZY_SYMBOL_POINTERS ||
                               stype == S_NON_LAZY_SYMBOL_POINTERS;
               }
               if (slot_8byte) {
                  auto *clone = new Instruction<Bits::M64>(instbuf);   /* width guard exempt */
                  clone->memidx = 0;
                  env.resolve(imm->pointee, &clone->memdisp);
                  clone->memdisp_offset = imm->pointee_offset;
                  return {clone};
               }
               const xed_reg_enum_t tgt = is_call ? XED_REG_R10 : XED_REG_R11;
               auto mov_inst = new Instruction<Bits::M64>(
                  opcode::mov_r32_mem_rip_disp32(is_call ? XED_REG_R10D : XED_REG_R11D));
               mov_inst->memidx = 0;
               env.resolve(imm->pointee, &mov_inst->memdisp);
               mov_inst->memdisp_offset = imm->pointee_offset;
               auto jmp_inst = new Instruction<Bits::M64>(opcode::jmp_r64(tgt));
               if (!is_call) { return {mov_inst, jmp_inst}; }
               auto insts = call_op(jmp_inst);
               insts.insert(std::prev(insts.end(), 2), mov_inst);
               return insts;
            } else {
               throw error("CALL/JMP_MEMv in M64 transform unreachable");
            }

         /* push/pop dword [abs32]: 4-byte slot, 4-byte stack step */
         case XED_IFORM_PUSH_MEMv:
            if constexpr (bits == Bits::M32) {
               typename SectionBlob<Bits::M32>::SectionBlobs insts =
                  {rip_to_pointee(opcode::mov_r32_mem_rip_disp32(XED_REG_R11D))};
               insts.splice(insts.end(), push_r32(XED_REG_R11D));
               return insts;
            } else {
               throw error("PUSH_MEMv in M64 transform unreachable");
            }
         case XED_IFORM_POP_MEMv:
            if constexpr (bits == Bits::M32) {
               auto insts = pop_r32(XED_REG_R11D);
               insts.push_back(rip_to_pointee(opcode::mov_mem_rip_disp32_r32(XED_REG_R11D)));
               return insts;
            } else {
               throw error("POP_MEMv in M64 transform unreachable");
            }

         default:
            {
               /* Bytes that do not decode in x86_64 carrying a "pointer" are
                * misdecoded data: keep the layout with NOPs. */
               xed_decoded_inst_t probe_xedd;
               xed_decoded_inst_zero_set_mode(
                  &probe_xedd, &Instruction<opposite<bits>>::dstate());
               xed_decoded_inst_set_input_chip(&probe_xedd, XED_CHIP_INVALID);
               const bool m64_ok =
                  xed_decode(&probe_xedd, instbuf.data(), instbuf.size())
                  == XED_ERROR_NONE;
               if (!m64_ok) {
                  char hex[3 * 24 + 8] = {0};
                  size_t n = instbuf.size() < 24 ? instbuf.size() : 24;
                  char *p = hex;
                  for (size_t i = 0; i < n; ++i)
                     p += sprintf(p, "%02x ", instbuf.at(i));
                  fprintf(stderr,
                          "warning: M%d->M%d Transform default-rule: "
                          "instbuf doesn't decode in x86_64, substituting "
                          "%zu NOP(s) (bytes: %s) at vmaddr 0x%zx, "
                          "iform=%s\n",
                          bits == Bits::M32 ? 32 : 64,
                          bits == Bits::M32 ? 64 : 32,
                          instbuf.size(), hex, this->loc.vmaddr,
                          xed_iform_enum_t2str(
                             xed_decoded_inst_get_iform_enum(&xedd)));
                  opcode_t nops(instbuf.size(), (uint8_t)0x90);
                  auto *clone = new Instruction<opposite<bits>>(nops);
                  clone->memidx = 0;
                  return {clone};
               }

               check_transform_width(xedd, probe_xedd, instbuf,
                                     this->loc.vmaddr);

               auto *clone = new Instruction<opposite<bits>>(instbuf);
               clone->memidx = 0;

               /* Byte-identical copy; the pointer lives in one of:
                *  - a captured [abs32] destination AND a trailing imm32 (e.g.
                *    `imul r32,[abs32],imm32`): bind each from its own source;
                *  - the [abs32] operand itself (the parser kept the disp32 as
                *    `imm`): rebind it as the rip-relative disp;
                *  - a trailing imm32 with no memory disp: re-emit the value. */
               if (this->memdisp) {
                  clone->memidx = 0;
                  env.resolve(this->memdisp, &clone->memdisp);
                  clone->memdisp_offset = this->memdisp_offset;
                  clone->imm = imm->Transform_one(env);
                  return {clone};
               }
               const xed_decoded_inst_t* clone_xedd = &clone->xedd;
               const xed_operand_values_t* clone_ops =
                  xed_decoded_inst_operands_const(clone_xedd);
               const unsigned disp_width =
                  xed_operand_values_has_memory_displacement(clone_ops)
                  ? xed_decoded_inst_get_memory_displacement_width(clone_ops, 0)
                  : 0;
               if (disp_width == sizeof(uint32_t)) {
                  env.resolve(imm->pointee, &clone->memdisp);
                  clone->memdisp_offset = imm->pointee_offset;
               } else {
                  clone->imm = imm->Transform_one(env);
               }
               return {clone};
            }
         }

      }

      if constexpr (bits == Bits::M32) {
            const auto reg0 = xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0);
            
            /*
             * Effective operand width for the iform-rules switch.
             * `PUSH r/v` / `POP r/v` / `CALL r/v` use the short opcode form
             * `0x50+r` / `0x58+r` / `FF /2`, whose operand size is normally
             * 32-bit in i386 mode but can be 16-bit with a 0x66 prefix
             * (`PUSH AX`, `POP AX`, `CALL AX`). We have to dispatch on the
             * decoded width because XED uses a single iform for both sizes.
             */
            const unsigned effective_width =
               xed_decoded_inst_get_operand_width(&xedd);

            /* iform rules */
            switch (xed_decoded_inst_get_iform_enum(&xedd)) {
            case XED_IFORM_PUSH_GPRv_50: // push r16 / push r32
            case XED_IFORM_PUSH_GPRv_FFr6: // long form: FF /6 mod=11
               {
                  /* 16-bit form: `0x66 0x50+r` (`push ax` etc) decodes
                   * identically in x86_64 (RSP -= 2, [RSP] = r16). Pass
                   * through unchanged via the default-rule copy ctor.
                   * 32-bit form needs the push_ax + mov-mem-rsp widening
                   * trick (x86_64 has no 4-byte push). The rare long
                   * encoding `FF /6` with a register operand (e.g.
                   * `ff f5` = push ebp; found by the width guard in
                   * iPhoto) is the same operation — same rewrite. */
                  if (effective_width == 16) { break; }
                  return push_r32(reg0);
               }

            case XED_IFORM_POP_GPRv_58:
            case XED_IFORM_POP_GPRv_8F: // long form: 8F /0 mod=11
               {
                  /* 16-bit form: `0x66 0x58+r` decodes identically in
                   * x86_64. 32-bit form needs the mov-mem-rsp + lea-rsp+4
                   * widening trick. The rare long encoding `8F /0` with a
                   * register operand is the same operation (sibling of
                   * PUSH_GPRv_FFr6). */
                  if (effective_width == 16) { break; }
                  return pop_r32(reg0);
               }

            case XED_IFORM_CALL_NEAR_GPRv: // call r32 (or r16 with 0x66)
               {
                  /* `call r16` (`0x66 FF /2`) calls a 16-bit absolute
                   * address — unsupported by our 32-bit return-address
                   * widening (call_op pushes a 4-byte return address).
                   * Almost never appears in real code; treat as bogus and
                   * bail with a clear error. */
                  if (effective_width != 32) {
                     throw error("%s: CALL_NEAR_GPRv at vmaddr 0x%zx: "
                                 "unsupported operand width %u (reg0=%s)",
                                 __FUNCTION__, this->loc.vmaddr,
                                 effective_width,
                                 xed_reg_enum_t2str(reg0));
                  }
                  const xed_reg_enum_t tgt = opcode::r32_to_r64(reg0);
                  auto jmp_inst = new Instruction<Bits::M64>(opcode::jmp_r64(tgt));
                  auto insts = call_op(jmp_inst);
                  /* trap a `call reg` through a NULL register (env-gated). */
                  auto trap = null_trap(tgt);
                  if (!trap.empty()) {
                     auto it = insts.end(); --it; --it;  /* before jmp_inst */
                     insts.splice(it, trap);
                  }
                  return insts;
               }

            /* `call/jmp dword [mem]` read a 4-byte slot; the byte-identical
             * x86_64 forms read 8. Load the target with a 4-byte mov first.
             * i386 evaluates the operand before `call` pushes, so the load
             * precedes the return-address push (guard 99_call_mem_esp). The
             * target goes in r11 (jmp) or r10 (call), never an i386 register:
             * a switch index stays live into the case bodies (guard
             * 99_jmptbl_index_live) and a register argument into the callee
             * (guard 99_call_mem_eax_arg). A bare
             * `[disp32]` with nothing to relocate is a promoted 8-byte dyld
             * slot and stays byte-identical. */
            case XED_IFORM_CALL_NEAR_MEMv:
            case XED_IFORM_JMP_MEMv:
               {
                  const xed_operand_values_t *mops = xed_decoded_inst_operands_const(&xedd);
                  if (!memdisp &&
                      xed_decoded_inst_get_base_reg(mops, 0) == XED_REG_INVALID &&
                      xed_decoded_inst_get_index_reg(mops, 0) == XED_REG_INVALID) {
                     break;
                  }
                  LoweredMem m;
                  if (!lower_mem(env, m)) { break; }
                  const bool is_call =
                     xed_decoded_inst_get_iform_enum(&xedd) == XED_IFORM_CALL_NEAR_MEMv;
                  const xed_reg_enum_t tgt = is_call ? XED_REG_R10 : XED_REG_R11;
                  auto load = emit_mem(env, m, opcode_t{0x8B}, is_call ? 10 : 11, {});
                  auto jmp_inst = new Instruction<Bits::M64>(opcode::jmp_r64(tgt));
                  if (!is_call) {
                     load.splice(load.end(), null_trap(tgt));
                     load.push_back(jmp_inst);
                     return load;
                  }
                  auto insts = call_op(jmp_inst);   /* lea, push (2), jmp, ret */
                  auto at_jmp = std::prev(insts.end(), 2);
                  static const bool load_after_push =
                     std::getenv("M64_CALL_MEM_LOAD_AFTER_PUSH") != nullptr;
                  insts.splice(load_after_push ? at_jmp : insts.begin(), load);
                  insts.splice(at_jmp, null_trap(tgt));
                  return insts;
               }

            case XED_IFORM_JMP_GPRv: // jmp r32 — tail call / computed jump
               {
                  /* The unguarded sibling of `call reg` / `jmp [mem]`: a
                   * register-indirect `jmp` whose target register is 0 lands
                   * at rip=0 with NO other signal (no pushed return addr, no
                   * faulting load) — indistinguishable from a wild jump. This
                   * is the one indirect-transfer form MACHO_NULL_TRAP did not
                   * cover, so a translated `jmp reg`-through-0 (an ObjC/C++
                   * tail-call dispatch through a null fn-ptr) presents only as
                   * the bare rip=0 the iPhoto worker-thread crash shows.
                   * ENTIRELY env-gated: the default build `break`s to the
                   * generic copy (byte-identical `jmp r64`); only with
                   * MACHO_NULL_TRAP=1 do we prepend `test reg,reg; jnz +2; ud2`
                   * so a jump-through-0 SIGILLs AT the dispatch site. */
                  if (effective_width == 32 && std::getenv("MACHO_NULL_TRAP")) {
                     const xed_reg_enum_t tgt = opcode::r32_to_r64(reg0);
                     auto trap = null_trap(tgt);
                     trap.push_back(
                        new Instruction<Bits::M64>(opcode::jmp_r64(tgt)));
                     return trap;
                  }
                  break;
               }

            case XED_IFORM_CALL_NEAR_RELBRz:
            case XED_IFORM_CALL_NEAR_RELBRd:
               {
                  /* `call rel32` (E8 cd). RELBRz is the i386 default; RELBRd
                   * is the fixed-doubleword variant — same E8+rel32 layout,
                   * so both lower to a 32-bit relative branch wrapped in
                   * call_op (which restores the i386 4-byte ret-addr push). */
                  auto jmp_inst = new Instruction<Bits::M64>({0xe9, 0x00, 0x00, 0x00, 0x00});
                  env.resolve(memdisp, &jmp_inst->memdisp);
                  env.resolve(brdisp, &jmp_inst->brdisp);
                  return call_op(jmp_inst);
               }
               
            case XED_IFORM_RET_NEAR:
               {
                  /* i386 | ret
                   * -----|-----
                   * X86  | _pop32 r11d
                   *      | jmp r11d
                   */
                  auto insts = pop_r32(XED_REG_R11D);
                  auto jmp_inst = new Instruction<opposite<bits>>(opcode::jmp_r64(XED_REG_R11));
                  /* trap a `ret` to a NULL return address (env-gated): after the
                   * 4-byte pop into r11, before `jmp r11`. */
                  insts.splice(insts.end(), null_trap(XED_REG_R11));
                  insts.push_back(jmp_inst);
                  return insts;
               }

            case XED_IFORM_RET_NEAR_IMMw:
               {
                  /* i386 `ret imm16` (C2 iw): pop the 4-byte return
                   * address, then release imm16 MORE stack bytes —
                   * callee-cleanup returns: stdcall, and the i386
                   * struct-return convention where the callee pops the
                   * hidden sret pointer with `ret $4` (very common).
                   * The byte-identical M64 form pops 8+imm16, jumping to
                   * the return address joined with the adjacent slot.
                   * Found by the width guard (28 sites in iPhoto).
                   * Mirrors RET_NEAR:
                   *
                   *   mov r11d, [rsp]          ; 4-byte return address
                   *   lea rsp, [rsp+4+imm16]   ; i386-width pop + cleanup
                   *   jmp r11
                   */
                  const uint32_t extra = (uint32_t)
                     xed_decoded_inst_get_unsigned_immediate(&xedd);
                  const uint32_t total = 4 + extra;
                  auto mov_inst = new Instruction<Bits::M64>(
                     opcode::mov_r32_mem_rsp(XED_REG_R11D));
                  opcode_t lea_buf;
                  if (total <= 0x7f) {
                     /* lea rsp, [rsp+disp8] */
                     lea_buf = {0x48, 0x8d, 0x64, 0x24, (uint8_t)total};
                  } else {
                     /* lea rsp, [rsp+disp32] (imm16 can reach 0xffff) */
                     lea_buf = {0x48, 0x8d, 0xa4, 0x24,
                                (uint8_t)(total & 0xff),
                                (uint8_t)((total >> 8) & 0xff),
                                (uint8_t)((total >> 16) & 0xff),
                                (uint8_t)((total >> 24) & 0xff)};
                  }
                  auto lea_inst = new Instruction<Bits::M64>(lea_buf);
                  auto jmp_inst = new Instruction<Bits::M64>(
                     opcode::jmp_r64(XED_REG_R11));
                  /* trap a `ret imm` to a NULL return address (env-gated). */
                  auto trap = null_trap(XED_REG_R11);
                  if (trap.empty()) { return {mov_inst, lea_inst, jmp_inst}; }
                  trap.push_front(lea_inst);
                  trap.push_front(mov_inst);
                  trap.push_back(jmp_inst);
                  return trap;
               }

            case XED_IFORM_ENTER_IMMw_IMMb:
               {
                  /* i386 `enter imm16, 0` = push ebp (4 BYTES); mov
                   * ebp, esp; sub esp, imm16. The byte-identical M64
                   * form pushes 8 bytes. Rarely compiler-emitted, but
                   * cheap to translate correctly (found by the width
                   * guard in RedRock + MobileMe). Expand to the same
                   * sequence the regular prologue translation produces:
                   *
                   *   <push_r32(ebp)>      ; 4-byte push
                   *   mov rbp, rsp         ; stack regs are 64-bit-widened
                   *   sub rsp, imm16       ; (REX.W, matching the
                   *                        ;  stack-reg widening rule)
                   *
                   * Nonzero nesting level is unsupported — no compiler
                   * emits it; bail loudly if it appears. */
                  const uint32_t frame_size = (uint32_t)
                     xed_decoded_inst_get_unsigned_immediate(&xedd);
                  const uint8_t nesting = (uint8_t)
                     xed_decoded_inst_get_second_immediate(&xedd);
                  if (nesting != 0) {
                     /* No compiler emits nonzero nesting levels — every
                      * observed instance (RedRock nesting=48, MobileMe
                      * nesting=142) is linear-sweep misdecoded data.
                      * NOP-substitute like the other junk iforms. */
                     fprintf(stderr,
                             "warning: M32->M64 Transform: NOP-substituting "
                             "ENTER with nesting %u at vmaddr 0x%zx "
                             "(misdecoded data)\n",
                             nesting, this->loc.vmaddr);
                     opcode_t nops(instbuf.size(), (uint8_t)0x90);
                     return {new Instruction<opposite<bits>>(nops)};
                  }
                  auto insts = push_r32(XED_REG_EBP);
                  auto mov_inst = new Instruction<Bits::M64>(
                     opcode_t{0x48, 0x89, 0xe5});   /* mov rbp, rsp */
                  insts.push_back(mov_inst);
                  if (frame_size != 0) {
                     opcode_t sub_buf = {0x48, 0x81, 0xec,
                                         (uint8_t)(frame_size & 0xff),
                                         (uint8_t)((frame_size >> 8) & 0xff),
                                         0x00, 0x00};   /* sub rsp, imm32 */
                     insts.push_back(new Instruction<Bits::M64>(sub_buf));
                  }
                  return insts;
               }

            case XED_IFORM_SGDT_MEMs:
            case XED_IFORM_SIDT_MEMs:
            case XED_IFORM_ARPL_MEMw_GPR16:
               {
                  /* sgdt/sidt store 6 bytes in i386 but 10 in x86_64;
                   * arpl (0x63 /r) is a protected-mode segment
                   * instruction that re-decodes as MOVSXD in x86_64.
                   * None have a legitimate use in translated app code —
                   * in practice these are linear-sweep misdecoded data
                   * (iPhoto: 2 sgdt + 1 arpl sites, each surrounded by
                   * other junk) or VM-detection tricks. Substitute
                   * equal-length NOPs, mirroring the policy for bytes
                   * that don't decode in x86_64 at all. */
                  fprintf(stderr,
                          "warning: M32->M64 Transform: NOP-substituting %s "
                          "at vmaddr 0x%zx (misdecoded data / privileged junk)\n",
                          xed_iform_enum_t2str(
                             xed_decoded_inst_get_iform_enum(&xedd)),
                          this->loc.vmaddr);
                  opcode_t nops(instbuf.size(), (uint8_t)0x90);
                  return {new Instruction<opposite<bits>>(nops)};
               }
               
            case XED_IFORM_INC_GPRv_40:
            case XED_IFORM_DEC_GPRv_48:
               {
                  /* i386 | inc/dec r16-or-r32  (short form: optional 0x66
                   *      |                      operand-size prefix +
                   *      |                      single-byte 0x40+r / 0x48+r)
                   * -----|------------
                   * X86  | inc/dec r16-or-r32 (FF /0 / FF /1, two bytes:
                   *      |                     FF and C0+r / C8+r,
                   *      |                     preserving 0x66 prefix)
                   *
                   * In x86_64, 0x40..0x4F are REX prefixes, so the i386
                   * short form has no x86_64 equivalent — we must rewrite
                   * to the FF /0 (INC) or FF /1 (DEC) two-byte form.
                   *
                   * Reading reg from XED is fragile here: for
                   * 0x66-prefixed (16-bit operand) iPhoto trips this with
                   * reg0=AX (XED's AX value is 32 lower than EAX in this
                   * build), and `0xc8 | (uint8_t)(AX - EAX) == 0xe8`
                   * produces `FF E8` which decodes as `JMP far m16:32`
                   * with a register operand — invalid. Instead, take the
                   * register number directly from the low 3 bits of the
                   * 0x40+r / 0x48+r opcode byte itself, and preserve any
                   * 0x66 prefix verbatim.
                   */
                  const xed_iform_enum_t iform =
                     xed_decoded_inst_get_iform_enum(&xedd);
                  /* Find the opcode byte (after any legacy prefixes). For
                   * this iform XED reports an instbuf of length 1 (`0x48+r`)
                   * or 2 (`0x66, 0x48+r`); be defensive and scan. */
                  std::size_t op_idx = 0;
                  while (op_idx < instbuf.size()) {
                     const uint8_t b = instbuf.at(op_idx);
                     if (b == 0x66 || b == 0x67 || b == 0xF0 || b == 0xF2 ||
                         b == 0xF3 || b == 0x2E || b == 0x36 || b == 0x3E ||
                         b == 0x26 || b == 0x64 || b == 0x65) {
                        ++op_idx;
                        continue;
                     }
                     break;
                  }
                  if (op_idx >= instbuf.size()) {
                     throw error("%s: %s at vmaddr 0x%zx: no opcode byte",
                                 __FUNCTION__, xed_iform_enum_t2str(iform),
                                 this->loc.vmaddr);
                  }
                  const uint8_t op = instbuf.at(op_idx);
                  if ((op & 0xF8) != (iform == XED_IFORM_INC_GPRv_40
                                      ? 0x40 : 0x48)) {
                     throw error("%s: %s at vmaddr 0x%zx: unexpected opcode "
                                 "byte 0x%02x", __FUNCTION__,
                                 xed_iform_enum_t2str(iform),
                                 this->loc.vmaddr, op);
                  }
                  const uint8_t r = op & 0x07;
                  const uint8_t base = iform == XED_IFORM_INC_GPRv_40
                                       ? 0xc0 : 0xc8;
                  /* Build new instbuf: prefixes verbatim, then FF, modrm. */
                  opcode_t buf;
                  for (std::size_t i = 0; i < op_idx; ++i) {
                     buf.push_back(instbuf.at(i));
                  }
                  buf.push_back(0xff);
                  buf.push_back((uint8_t)(base | r));
                  return {new Instruction<opposite<bits>>(buf)};
               }

            /* `push/pop dword [mem]` move 4 bytes; the x86_64 forms move 8.
             * push: load into r11d, then the 4-byte push (the operand is
             * evaluated before %esp moves). pop: the 4-byte pop, then the store
             * (an %esp-relative destination is computed AFTER the increment).
             * The popped value goes through r10d when the operand needs r11.
             * The 16-bit forms move 2 bytes in both modes and copy verbatim. */
            case XED_IFORM_PUSH_MEMv:
            case XED_IFORM_POP_MEMv:
               {
                  if (effective_width == 16) { break; }
                  LoweredMem m;
                  if (!lower_mem(env, m)) { break; }
                  if (xed_decoded_inst_get_iform_enum(&xedd) == XED_IFORM_PUSH_MEMv) {
                     auto insts = emit_mem(env, m, opcode_t{0x8B}, 11, {});
                     insts.splice(insts.end(), push_r32(XED_REG_R11D));
                     return insts;
                  }
                  const bool r10 = !m.pre.empty();
                  auto insts = pop_r32(r10 ? XED_REG_R10D : XED_REG_R11D);
                  insts.splice(insts.end(), emit_mem(env, m, opcode_t{0x89}, r10 ? 10 : 11, {}));
                  return insts;
               }

            case XED_IFORM_PUSHFD:
               {
                  /* i386 `pushfd` (9C) pushes 4 bytes of EFLAGS; the same
                   * byte in x86_64 (pushfq) pushes 8. Rewrite:
                   *
                   *   pushfq             ; 8-byte EFLAGS image at [rsp]
                   *   pop r11            ; r11 = EFLAGS, rsp restored
                   *   <push_r32(r11d)>   ; 4-byte slot, flags untouched
                   *
                   * (pop/push/mov/lea don't modify EFLAGS.) The 16-bit
                   * form `66 9C` decodes as the separate PUSHF iform and
                   * passes through byte-identical (2-byte push in both
                   * modes). */
                  auto pushfq_inst = new Instruction<Bits::M64>(opcode_t{0x9c});
                  auto pop_r11 = new Instruction<Bits::M64>(opcode_t{0x41, 0x5b});
                  typename SectionBlob<Bits::M32>::SectionBlobs insts
                     {pushfq_inst, pop_r11};
                  insts.splice(insts.end(), push_r32(XED_REG_R11D));
                  return insts;
               }

            case XED_IFORM_POPFD:
               {
                  /* i386 `popfd` (9D) pops 4 bytes into EFLAGS; x86_64
                   * popfq pops 8. Rewrite:
                   *
                   *   mov r11d, [rsp]    ; 4-byte EFLAGS image (zero-ext)
                   *   lea rsp, [rsp+4]   ; i386-width pop, flags untouched
                   *   push r11           ; 8-byte image below freed slot
                   *   popfq              ; load EFLAGS (upper 32 zero =
                   *                      ;  reserved bits, fine)
                   */
                  auto mov_inst = new Instruction<Bits::M64>(
                     opcode::mov_r32_mem_rsp(XED_REG_R11D));
                  auto lea_inst = new Instruction<Bits::M64>(
                     opcode::lea_rsp_mem_rsp_4());
                  auto push_r11 = new Instruction<Bits::M64>(opcode_t{0x41, 0x53});
                  auto popfq_inst = new Instruction<Bits::M64>(opcode_t{0x9d});
                  return {mov_inst, lea_inst, push_r11, popfq_inst};
               }

            case XED_IFORM_PUSH_IMMb:
            case XED_IFORM_PUSH_IMMz:
               {
                  /* 16-bit operand-size form (`0x66 0x68 imm16` / `0x66 0x6A
                   * imm8`) pushes 2 bytes; the encoding is identical in
                   * x86_64 (operand-size prefix overrides default 64-bit
                   * push). Pass through via the default copy-ctor rule.
                   * Only widen for the 32-bit form, where i386 pushes 4
                   * bytes and x86_64's bare `push` would push 8. */
                  if (effective_width == 16) { break; }
                  if (xed_decoded_inst_get_immediate_is_signed(&xedd)) {
                     return push_imm(xed_decoded_inst_get_signed_immediate(&xedd));
                  } else {
                     return push_imm(xed_decoded_inst_get_unsigned_immediate(&xedd));
                  }
               }

            case XED_IFORM_LEAVE:
               {
                  /* i386 | leave             ; ESP=EBP; EBP=pop32() (4-byte pop)
                   * -----|---------
                   * X86  | mov rsp, rbp     ; 0x48 0x89 0xEC — 64-bit mov
                   *      | mov ebp, [rsp]   ; load 4-byte saved frame ptr
                   *      | lea rsp, [rsp+4] ; pop 4 bytes
                   *
                   * Was `0x89 0xEC` (32-bit mov esp, ebp) — that zero-extends
                   * rsp, truncating dyld's high-address stack to a low garbage
                   * address whenever the function runs without the wrapper exec's
                   * low-stack setup (every C++ static init in every translated
                   * framework). The 64-bit form restores rsp's high half from
                   * rbp's high half (set by the widened prologue `mov rsp, rbp`),
                   * preserving the stack-pointer invariant. The subsequent 4-byte
                   * pop of ebp keeps the i386 4-byte stack-slot convention.
                   *
                   * NOTE: bare x86_64 `leave` (0xc9) pops 8 bytes instead of 4,
                   * which would consume the next-pushed return-address bytes
                   * into the high half of rbp. We can't use it.
                   */
                  auto mov_inst = new Instruction<Bits::M64>(opcode_t({0x48, 0x89, 0xEC}));
                  auto pop_insts = pop_r32(XED_REG_EBP);
                  auto insts = pop_insts;
                  insts.push_front(mov_inst);
                  return insts;
               }

            default: break;
            }

            /* Any other iform keeps its operation; only a relocated memory
             * operand (PIC-anchored, or an absolute `disp32(%base)` table)
             * needs re-encoding. Absolute `disp32(,i,s)`/`disp32(%b,i,s)`
             * stay byte-identical below with an absolute disp. */
            if (memdisp) {
               const xed_operand_values_t *mops = xed_decoded_inst_operands_const(&xedd);
               const bool base_only =
                  xed_decoded_inst_get_base_reg(mops, memidx) != XED_REG_INVALID &&
                  xed_decoded_inst_get_index_reg(mops, memidx) == XED_REG_INVALID;
               LoweredMem m;
               if ((pic_anchored || base_only) && lower_mem(env, m)) {
                  return emit_mem(env, m, m.opcode, m.reg, m.trailing);
               }
            }

            /* Stack-pointer widening: if the i386 instruction operates on
             * %esp or %ebp as a register operand with NO memory operand,
             * widen to 64-bit by prepending REX.W (0x48). Otherwise the
             * 32-bit op zero-extends the destination, truncating the
             * dyld-provided high-address stack pointer to a garbage low
             * address. Surfaces in C++ static initializers of translated
             * frameworks (no wrapper exec to fix up the entry stack).
             * Limited to reg-only ops to avoid disturbing memidx tracking. */
            {
               const auto reg0w = xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0);
               const auto reg1w = xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG1);
               const bool touches_stack_reg =
                  (reg0w == XED_REG_ESP || reg0w == XED_REG_EBP ||
                   reg1w == XED_REG_ESP || reg1w == XED_REG_EBP);
               const unsigned n_mem_ops =
                  xed_decoded_inst_number_of_memory_operands(&xedd);
               if (touches_stack_reg && n_mem_ops == 0) {
                  /* Skip if already has a REX prefix (0x40..0x4F). */
                  std::size_t prefix_end = 0;
                  while (prefix_end < instbuf.size() &&
                         (instbuf[prefix_end] == 0x66 ||
                          instbuf[prefix_end] == 0x67 ||
                          instbuf[prefix_end] == 0xF0 ||
                          instbuf[prefix_end] == 0xF2 ||
                          instbuf[prefix_end] == 0xF3)) {
                     ++prefix_end;
                  }
                  const bool already_rex = prefix_end < instbuf.size() &&
                     (instbuf[prefix_end] & 0xF0) == 0x40;
                  /*
                   * Skip widening for the `mov reg32, imm32` opcode range
                   * 0xB8..0xBF. With REX.W, those become `mov reg64, imm64`
                   * and require an 8-byte immediate — but our instbuf only
                   * has the original 4-byte imm32, so xed_decode reports
                   * BUFFER_TOO_SHORT for `48 bX <imm32>` (6 bytes when 10
                   * are needed). The unwidened form `mov ebp, imm32`
                   * zero-extends to rbp in x86_64 — which loses any
                   * dyld-passed high bits in rbp, but iWeb's translated
                   * code that hits this pattern is loading a literal
                   * constant where high bits weren't expected to survive.
                   * Surfaces on iWeb translation; iPhoto/Tessera have
                   * different patterns and don't hit this.
                   */
                  const bool is_mov_reg_imm32 =
                     prefix_end < instbuf.size() &&
                     (instbuf[prefix_end] & 0xF8) == 0xB8;
                  if (!already_rex && !is_mov_reg_imm32) {
                     opcode_t widened = instbuf;
                     widened.insert(widened.begin() + prefix_end, 0x48);
                     return {new Instruction<opposite<bits>>(widened)};
                  }
               }
            }

            /* default rule */
            /* Byte-identical pass-through (cross-bits copy ctor): require
             * width equivalence. Probe-decode in the destination mode; if
             * the bytes don't decode there, the copy ctor's own
             * NOP-substitution / bail logic owns that case. */
            {
               xed_decoded_inst_t guard_xedd;
               xed_decoded_inst_zero_set_mode(
                  &guard_xedd, &Instruction<opposite<bits>>::dstate());
               xed_decoded_inst_set_input_chip(&guard_xedd, XED_CHIP_INVALID);
               if (xed_decode(&guard_xedd, instbuf.data(), instbuf.size())
                   == XED_ERROR_NONE) {
                  check_transform_width(xedd, guard_xedd, instbuf,
                                        this->loc.vmaddr);
               }
            }
            return {new Instruction<opposite<bits>>(*this, env)};

         } else {
         abort();
      }
      
   }

   template <Bits bits>
   Instruction<bits>::Instruction(const Instruction<opposite<bits>>& other,
                                  TransformEnv<opposite<bits>>& env):
      SectionBlob<bits>(other, env), instbuf(other.instbuf), memidx(other.memidx),
      memdisp(nullptr), memdisp_absolute(other.memdisp_absolute),
      memdisp_offset(other.memdisp_offset),
      imm(nullptr), brdisp(nullptr)
   {
      if (other.memdisp) {
         env.resolve(other.memdisp, &memdisp);
      }

      /*
       * Originally `assert(!(other.imm && other.imm->pointee))`. The
       * assumed invariant — that Instruction::Transform's imm-pointee
       * dispatch handles every imm-with-pointee case before falling to
       * the M32 default rule's copy-ctor — breaks for instructions
       * reached via Transform_one rather than the main Transform method.
       * Specifically StubHelperBlob's transform copies its push_inst
       * via Transform_one. That push_inst's imm is the lazy-bind-info
       * offset, mis-classified as a pointer when the offset happens to
       * land in an in-binary segment's vmaddr range (iPhoto's 59 KB
       * lazy_bind makes this common). Just let the imm transform
       * normally — the StubHelperBlob's Emit overrides imm->value with
       * bindee->index and clears pointee, so the emitted bytes are
       * still the bind-info offset rather than the resolved-pointee
       * vmaddr.
       */

      if (other.imm) {
         imm = other.imm->Transform_one(env);
      }
      
      /* The brdisp resolve is DEFERRED (the Resolver writes &brdisp when the
       * target blob is transformed, possibly after this ctor returns), so it
       * is registered only once we know the instruction survives. A far
       * call/jmp NOP-substituted below would otherwise get its brdisp written
       * back after being cleared, and Emit would try to patch a branch
       * displacement into a NOP (Portal 2 libcef.dylib). */
      bool nop_substituted = false;

      xed_decoded_inst_zero_set_mode(&xedd, &dstate());
      xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_INVALID);

      xed_error_enum_t err;
      if ((err = xed_decode(&xedd, instbuf.data(), instbuf.size())) != XED_ERROR_NONE) {
         char hex[3 * 24 + 8] = {0};
         size_t n = instbuf.size() < 24 ? instbuf.size() : 24;
         char *p = hex;
         for (size_t i = 0; i < n; ++i) p += sprintf(p, "%02x ", instbuf.at(i));

         /*
          * The i386 bytes don't decode in x86_64 — almost always because
          * the source is an i386-only opcode (PUSH/POP ES/CS/SS/DS at
          * 0x06/0x0E/0x16/0x1E and 0x07/0x17/0x1F; PUSHA/POPA at 0x60/0x61;
          * BOUND at 0x62; INTO at 0xCE; DAA/DAS/AAA/AAS/AAM/AAD at
          * 0x27/0x2F/0x37/0x3F/0xD4/0xD5; far call/jmp at 0x9A/0xEA; etc.).
          * These opcodes were removed in 64-bit mode.
          *
          * Almost every such site is data that the linear-sweep
          * disassembler misdecoded as code (jump-table entries, embedded
          * literals, alignment fillers). Real executable code paths
          * containing these opcodes would crash on either architecture.
          *
          * If the source had no operands (memdisp/brdisp/imm all null),
          * substitute NOPs of equal byte count — preserves section layout
          * without crashing the translator. If the source had operands,
          * the instruction was non-trivial; bail loudly so we can spot it.
          */
         /*
          * NOP-substitute when there is no live operand that would need
          * to be re-emitted: no memdisp / brdisp, and either no imm at
          * all or an imm that didn't resolve to a real pointee. The
          * "imm without pointee" case is the i386 short-form
          * `mov [abs32], eax` (0xA3) and friends, where the parser
          * speculatively created an Immediate but the disp32 lands
          * outside any segment — strong signal it's misdecoded data.
          */
         const bool imm_unresolved =
            (other.imm == nullptr) ||
            (other.imm->pointee == nullptr);
         const bool no_live_operands =
            (other.memdisp == nullptr) &&
            (other.brdisp == nullptr) &&
            imm_unresolved;
         /* Far call/jmp (ptr16:32, opcode 0x9A/0xEA) has no flat-64-bit form,
          * and its segmented far pointer is meaningless in the flat model — it
          * is ALWAYS misdecoded data-in-code here (a real far transfer can't
          * work in a flat i386 dylib either). Unlike the no-operand junk
          * opcodes the parser gave it a brdisp/imm for the far target, so it
          * misses the no_live_operands gate; NOP-substitute it anyway and drop
          * the bogus operand so Emit writes pure NOPs. (Portal 2 libcef.dylib.) */
         const bool far_transfer =
            !instbuf.empty() &&
            (instbuf.front() == 0xEA || instbuf.front() == 0x9A);
         if ((no_live_operands || far_transfer) && !instbuf.empty()) {
            fprintf(stderr,
                    "warning: M%d->M%d copyctor: substituting %zu NOP(s) "
                    "for untranslatable bytes (%s) at src vmaddr 0x%zx\n",
                    bits == Bits::M32 ? 64 : 32,
                    bits == Bits::M32 ? 32 : 64,
                    instbuf.size(), hex, other.loc.vmaddr);
            std::fill(instbuf.begin(), instbuf.end(), (uint8_t)0x90);
            /* Clear any imm/brdisp the ctor body set from other.imm/brdisp —
             * our NOPs already fill the entire instbuf, so Emit must not try
             * to overwrite trailing bytes with an operand value (the far
             * transfer's bogus pointer in particular). */
            this->imm = nullptr;
            nop_substituted = true;
            /* Re-decode: a buffer of 0x90 NOPs decodes to a 1-byte NOP
             * (xedd reflects only the first byte); the remaining bytes
             * are emitted verbatim via instbuf at Emit time, since
             * Instruction::size() returns instbuf.size(). */
            xed_decoded_inst_zero_set_mode(&xedd, &dstate());
            xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_INVALID);
            if (xed_decode(&xedd, instbuf.data(), instbuf.size())
                != XED_ERROR_NONE) {
               /* Should never happen — 0x90 always decodes. */
               throw error("Instruction<m%d> NOP substitution decode failed "
                           "(src vmaddr 0x%zx)",
                           bits == Bits::M32 ? 32 : 64, other.loc.vmaddr);
            }
            /* Fall through to the memdisp_absolute re-derivation below
             * (it's a no-op since other.memdisp is null). */
         } else {
            throw error("Instruction<m%d>(M%d->M%d copyctor): xed_decode: %s (bytes: %s, size %zu, src vmaddr 0x%zx)\n",
                        bits == Bits::M32 ? 32 : 64,
                        bits == Bits::M32 ? 64 : 32,
                        bits == Bits::M32 ? 32 : 64,
                        xed_error_enum_t2str(err), hex, instbuf.size(),
                        other.loc.vmaddr);
         }
      }

      if (other.brdisp && !nop_substituted) {
         env.resolve(other.brdisp, &brdisp);
      }

      /* i386 effective-address wrap: add 0x67 so a scaled-index operand
       * computes its EA mod 2^32 as on i386 (see wants_addr32). */
      if (bits == Bits::M64) {
         const xed_operand_values_t *aops = xed_decoded_inst_operands(&xedd);
         const unsigned nmem =
            xed_decoded_inst_number_of_memory_operands(&xedd);
         /* NOP/WIDENOP never touch memory (growing an alignment nop would
          * shift LSDA offsets) and LEA's 32-bit result already wraps. */
         const xed_category_enum_t cat = xed_decoded_inst_get_category(&xedd);
         const bool non_deref =
            cat == XED_CATEGORY_NOP || cat == XED_CATEGORY_WIDENOP ||
            xed_decoded_inst_get_iclass(&xedd) == XED_ICLASS_LEA;
         /* LOOP/LOOPE/LOOPNE/JECXZ count in the address-size register: RCX
          * in x86_64, ECX (wrapping at 2^32) only with 0x67. */
         const xed_iclass_enum_t ic = xed_decoded_inst_get_iclass(&xedd);
         bool want_addr32 = ic == XED_ICLASS_LOOP || ic == XED_ICLASS_LOOPE ||
                            ic == XED_ICLASS_LOOPNE || ic == XED_ICLASS_JRCXZ;
         for (unsigned i = 0; i < nmem && !want_addr32 && !non_deref; ++i) {
            want_addr32 = wants_addr32(xed_decoded_inst_get_base_reg(aops, i),
                                       xed_decoded_inst_get_index_reg(aops, i));
         }
         /* ...unless the source already carries one */
         if (want_addr32) {
            for (uint8_t b : instbuf) {
               if (!is_legacy_prefix(b)) { break; }
               if (b == 0x67) { want_addr32 = false; break; }
            }
         }
         if (want_addr32) {
            /* Insert 0x67 ahead of all bytes (no REX exists on i386
             * pass-through, and legacy-prefix order is unconstrained), then
             * commit only if it re-decodes to one instruction consuming the
             * whole buffer — keeps `xedd` in sync with `instbuf` for the
             * displacement patch in Emit. */
            opcode_t trial = instbuf;
            trial.insert(trial.begin(), 0x67);
            xed_decoded_inst_t trial_xedd;
            xed_decoded_inst_zero_set_mode(&trial_xedd, &dstate());
            xed_decoded_inst_set_input_chip(&trial_xedd, XED_CHIP_INVALID);
            if (xed_decode(&trial_xedd, trial.data(), trial.size())
                   == XED_ERROR_NONE &&
                xed_decoded_inst_get_length(&trial_xedd) == trial.size()) {
               instbuf.swap(trial);
               xedd = trial_xedd;
            }
         }
      }

      /*
       * Re-derive memdisp_absolute from the freshly decoded (transformed)
       * instruction rather than trusting the source's flag. The i386→x86_64
       * transform can change a memory operand's addressing mode: i386
       * `[disp32]` (absolute, mod=00 r/m=101) becomes x86_64 `[rip+disp32]`
       * (rip-relative). Copying the source flag would leave such an operand
       * marked absolute and make Emit patch a rip-relative slot with an
       * absolute address. `[disp32 + idx*scale]` stays SIB-absolute and is
       * correctly re-detected as non-rip here.
       */
      if (other.memdisp) {
         const xed_operand_values_t *ops = xed_decoded_inst_operands(&xedd);
         memdisp_absolute =
            xed_decoded_inst_get_base_reg(ops, memidx) != XED_REG_RIP;
      }
   }

   template <Bits bits>
   void Instruction<bits>::decode(xed_decoded_inst_t& xedd, const opcode_t& instbuf) {
      xed_error_enum_t err;
      xed_decoded_inst_zero_set_mode(&xedd, &dstate());
      xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_INVALID);
      if ((err = xed_decode(&xedd, instbuf.data(), instbuf.size())) != XED_ERROR_NONE) {
         char hex[3 * 24 + 8] = {0};
         size_t n = instbuf.size() < 24 ? instbuf.size() : 24;
         char *p = hex;
         for (size_t i = 0; i < n; ++i) p += sprintf(p, "%02x ", instbuf.at(i));
         /* one-shot backtrace to stderr so we can identify the call site
          * before the throw unwinds. */
         void *frames[32];
         int nframes = backtrace(frames, 32);
         fprintf(stderr,
                 "decode[m%d] xed_decode FAIL: %s (bytes: %s, size %zu)\n"
                 "  backtrace (%d frames):\n",
                 bits == Bits::M32 ? 32 : 64,
                 xed_error_enum_t2str(err), hex, instbuf.size(), nframes);
         backtrace_symbols_fd(frames, nframes, 2);
         fflush(stderr);
         throw error("%s[m%d]: xed_decode: %s (bytes: %s, size %zu)\n",
                     __FUNCTION__, bits == Bits::M32 ? 32 : 64,
                     xed_error_enum_t2str(err), hex, instbuf.size());
      }
   }

   template <Bits bits>
   void Instruction<bits>::decode() {
      decode(xedd, instbuf);
   }

   /* NOTE: Requires that instruction has already been decode()'ed. */
   template <Bits bits>
   void Instruction<bits>::parse_handle_relbr() {
      const unsigned width_bits = xed_decoded_inst_get_branch_displacement_width_bits(&xedd);

      switch (width_bits) {
      case 8:
         break;
      case 0:
      case 32:
         return;
      default:
         /* 16-bit relbr (e.g. some old 16-bit-mode forms) — leave as-is.
          * Build doesn't try to widen these; Instruction::Emit's
          * xed_patch_brdisp re-patches with the resolved target's
          * actual displacement. Was abort(); that crashed iPhoto. */
         return;
      }

      const int8_t relbr = instbuf.at(1);
      const uint8_t relbru = relbr;
      
      switch (xed_decoded_inst_get_iform_enum(&xedd)) {
      case XED_IFORM_JB_RELBRb:
      case XED_IFORM_JBE_RELBRb:
      case XED_IFORM_JL_RELBRb:
      case XED_IFORM_JLE_RELBRb:
      case XED_IFORM_JNB_RELBRb:
      case XED_IFORM_JNBE_RELBRb:
      case XED_IFORM_JNL_RELBRb:
      case XED_IFORM_JNLE_RELBRb:
      case XED_IFORM_JNO_RELBRb:
      case XED_IFORM_JNP_RELBRb:
      case XED_IFORM_JNS_RELBRb:
      case XED_IFORM_JNZ_RELBRb:
      case XED_IFORM_JO_RELBRb:
      case XED_IFORM_JP_RELBRb:
      case XED_IFORM_JS_RELBRb:
      case XED_IFORM_JZ_RELBRb:
         {
            /*
             * XED reports a Jcc 8-bit relbr iform but instbuf[0] isn't a
             * 0x7X opcode — usually a Jcc with a branch-hint prefix
             * (2E/3E) or a misdecode of data interleaved in __text. The
             * widening below assumes instbuf[0] is the bare 0x7X opcode
             * and instbuf[1] is the displacement; with a prefix both
             * positions shift, so blindly rewriting produces a bogus
             * displacement. Bail out and leave the instruction as-is —
             * Instruction::Emit's xed_patch_brdisp re-patches against
             * the resolved target. Was assert(), which crashed iPhoto.
             */
            if ((instbuf.at(0) & 0xf0) != 0x70) { break; }
            const uint8_t byte = (instbuf.at(0) & 0x0f) | 0x80;
            instbuf = {0x0f, byte, relbru, 0x00, 0x00, 0x00};
         }
         break;

      case XED_IFORM_JMP_RELBRb:
         /* Same prefix-sensitive concern as above (instbuf[0]==0xEB for
          * bare JMP rel8; with a prefix this would corrupt the disp). */
         if (instbuf.at(0) != 0xEB) { break; }
         instbuf = {0xe9, relbru, 0x00, 0x00, 0x00};
         break;

      /*
       * 8-bit-only branches with no 32-bit relbr form. We can't widen
       * them in-place (the ISA has no 32-bit encoding); leaving them
       * as-is is correct as long as the section didn't grow past
       * ±127 around them. Instruction::Emit re-patches the brdisp
       * through xed_patch_brdisp, which will succeed for unchanged
       * targets and throw a clearer "failed to patch" error if a
       * cascading insertion did push the target out of range — far
       * better than aborting unconditionally. iPhoto's __text trips
       * this routinely; rebasify never inserts in this code path
       * (it only inserts on PIC-thunk rewrite, of which iPhoto has
       * zero), so the widening attempt is wasted anyway.
       */
      case XED_IFORM_LOOP_RELBRb:
      case XED_IFORM_LOOPE_RELBRb:
      case XED_IFORM_LOOPNE_RELBRb:
      case XED_IFORM_JCXZ_RELBRb:
      case XED_IFORM_JECXZ_RELBRb:
         break;

      default:
         /* Unknown 8-bit relbr iform — same policy: leave as-is and
          * let Emit's xed_patch_brdisp re-patch. */
         break;
      }

      /* re-decode after modifications to buffer */
      decode();
   }
   

   template class Instruction<Bits::M32>;
   template class Instruction<Bits::M64>;   
   
}
