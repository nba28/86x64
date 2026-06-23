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
          * X86  | push ax
          *      | push ax
          *      | mov [rsp], r32
          * NOTE: Shouldn't modify flags.
          */
         auto push1 = new Instruction<Bits::M64>(opcode::push_ax());
         auto push2 = new Instruction<Bits::M64>(opcode::push_ax());
         auto mov = new Instruction<Bits::M64>(opcode::mov_mem_rsp_r32(r32));
         return {push1, push2, mov};
      }

      typename SectionBlob<Bits::M32>::SectionBlobs push_imm(uint32_t imm) {
         /* i386 | push imm32
          * -----|-----------
          * X86  | push ax
          *      | push ax
          *      | mov dword [rsp], imm
          */
         auto push1 = new Instruction<Bits::M64>(opcode::push_ax());
         auto push2 = new Instruction<Bits::M64>(opcode::push_ax());
         opcode_t mov_opcode = {0xc7, 0x04, 0x24};
         opcode::push_back_imm<uint32_t>(mov_opcode, imm);
         auto mov = new Instruction<Bits::M64>(mov_opcode);
         return {push1, push2, mov};
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
      
   }

   template <Bits bits>
   const xed_state_t& Instruction<bits>::dstate() { return dstate_<bits>; }

   template <Bits bits>
   bool Instruction<bits>::CanDecode(const Image& img, const Location& loc) {
      xed_decoded_inst_t xedd;
      xed_decoded_inst_zero_set_mode(&xedd, &dstate());
      xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_INVALID);
      return xed_decode(&xedd, &img.at<uint8_t>(loc.offset),
                        img.size() - loc.offset) == XED_ERROR_NONE;
   }

   template <Bits bits>
   Instruction<bits>::Instruction(const Image& img, const Location& loc, ParseEnv<bits>& env,
                                  bool add_to_map):
      SectionBlob<bits>(loc, env, add_to_map), memdisp(nullptr), imm(nullptr), brdisp(nullptr)
   {
      xed_error_enum_t err;

      xed_decoded_inst_zero_set_mode(&xedd, &dstate());
      xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_INVALID);

      if ((err = xed_decode(&xedd, &img.at<uint8_t>(loc.offset), img.size() - loc.offset)) !=
          XED_ERROR_NONE) {
         throw error("%s: offset 0x%x: xed_decode: %s", __FUNCTION__, loc.offset,
                     xed_error_enum_t2str(err));
      }
      
      instbuf = std::vector<uint8_t>(&img.at<uint8_t>(loc.offset),
                                     &img.at<uint8_t>(loc.offset + xed_decoded_inst_get_length(&xedd)));

      /* special transformations */
      // parse_handle_relbr();

      const std::size_t refaddr = loc.vmaddr + xed_decoded_inst_get_length(&xedd);
      xed_operand_values_t *operands = xed_decoded_inst_operands(&xedd);

      /* Check for relocations */
      std::unordered_map<xed_iform_enum_t, std::size_t> reloc_index_map =
         {{XED_IFORM_CALL_NEAR_RELBRz, 1},
          {XED_IFORM_JMP_RELBRz, 1},
          {XED_IFORM_JZ_RELBRz, 2},
          {XED_IFORM_JNZ_RELBRz, 2},
          {XED_IFORM_JL_RELBRz, 2},
          {XED_IFORM_JLE_RELBRz, 2},
          {XED_IFORM_JNL_RELBRz, 2},
          {XED_IFORM_JNLE_RELBRz, 2},
          {XED_IFORM_JNBE_RELBRz, 2},
          {XED_IFORM_JBE_RELBRz, 2},
          {XED_IFORM_JB_RELBRz, 2},
          {XED_IFORM_JNB_RELBRz, 2},
          /* sign / parity / overflow conditionals — same 2-byte (0F 8x)
           * opcode layout as the rest, so the rel32 starts at offset 2.
           * Rare to carry a reloc in a fully-linked image, but completes
           * the Jcc-RELBRz family so none is silently missed. */
          {XED_IFORM_JS_RELBRz, 2},
          {XED_IFORM_JNS_RELBRz, 2},
          {XED_IFORM_JP_RELBRz, 2},
          {XED_IFORM_JNP_RELBRz, 2},
          {XED_IFORM_JO_RELBRz, 2},
          {XED_IFORM_JNO_RELBRz, 2},
          {XED_IFORM_LEA_GPRv_AGEN, 3},
         };

      auto reloc_index_map_it = reloc_index_map.find(xed_decoded_inst_get_iform_enum(&xedd));
      if (reloc_index_map_it != reloc_index_map.end()) {
         /* check if there is a relocation entry at this address */
         auto relocs_it = env.relocs.find(loc.vmaddr + reloc_index_map_it->second);
         if (relocs_it != env.relocs.end()) {
            reloc = relocs_it->second;
            env.relocs.erase(relocs_it);
            return;
         }
      }
      
      /* Memory Accesses */
      const unsigned int nops = xed_decoded_inst_noperands(&xedd);
      if (xed_operand_values_has_memory_displacement(operands)) {
         for (unsigned i = 0; i < nops; ++i) {
            xed_reg_enum_t basereg  = xed_decoded_inst_get_base_reg(operands,  i);
            xed_reg_enum_t indexreg = xed_decoded_inst_get_index_reg(operands, i);
            if (basereg == select_value(bits, XED_REG_EIP, XED_REG_RIP) &&
                indexreg == XED_REG_INVALID)
               {
                  if (memdisp) {
                     /* Two rip/eip-relative memory operands on one
                      * instruction — we only track a single memdisp. Throw
                      * a located, catchable error instead of a raw abort()
                      * so the driver can report which input tripped it. */
                     throw error("%s: duplicate rip-relative memdisp at "
                                 "offset 0x%jx, vmaddr 0x%jx",
                                 __FUNCTION__, (uintmax_t) loc.offset,
                                 (uintmax_t) loc.vmaddr);
                  }

                  memidx = i;

                  /* get memory displacement & reference address */
                  const ssize_t memdisp = xed_decoded_inst_get_memory_displacement(operands, i);
                  const std::size_t targetaddr = refaddr + memdisp;

                  /* resolve pointer */
                  this->memdisp = env.add_placeholder(targetaddr);
                  // env.vmaddr_resolver.resolve(targetaddr, &this->memdisp);
                  /* mid-blob fallback for M64 [rip+disp] re-parse: if targetaddr
                   * lands inside a multi-byte blob, prefer the containing blob +
                   * offset over the stranded mid-section placeholder so the ref
                   * survives the modify/convert layout shift. override=true.
                   * Gated to writable __DATA: the containing-blob guess is only
                   * valid for opaque program data; in __OBJC it corrupts the
                   * fragile-ABI metadata (see ParseEnv::vmaddr_in_writable_data). */
                  if (env.vmaddr_in_writable_data(targetaddr)) {
                     env.vmaddr_resolver.resolve_containing(
                        targetaddr,
                        (const SectionBlob<bits> **) &this->memdisp,
                        &this->memdisp_offset, /*override=*/true);
                  }

#if 0
                  char pbuf[32];
                  xed_print_info_t pinfo;
                  xed_init_print_info(&pinfo);
                  pinfo.blen = 32;
                  pinfo.buf = pbuf;
                  pinfo.p = &xedd;
                  pinfo.runtime_address = loc.vmaddr;
                  xed_format_generic(&pinfo);
                  fprintf(stderr, "%s\n", pbuf);
#endif
               } else if (basereg == XED_REG_INVALID && indexreg == XED_REG_INVALID &&
                          xed_decoded_inst_get_memory_displacement_width(operands, i) ==
                          sizeof(uint32_t)) {
               /*
                * Absolute `[disp32]` memory operand (i386 mod=00 r/m=101).
                * `mov [abs32], imm32` (`c7 05 disp32 imm32`) has a trailing
                * imm32, so the disp32 is NOT the last 4 instruction bytes:
                * capture the disp32 destination in `memdisp` and the imm32
                * in `imm`. Plain loads/stores have no trailing immediate
                * and keep the disp32 in `imm` as before.
                *
                * Guard: only do the split when the destination lands in a
                * section holding program-writable data. Linear-sweep
                * disassembly occasionally misdecodes data/padding as
                * `c7 05 ...`; such bogus instructions point their disp32
                * at dyld-managed symbol-pointer/stub tables. Resolving
                * those strands a placeholder mid-table and breaks a later
                * re-parse, so fall back to plain simple-pointer handling.
                */
               const bool has_any_imm =
                  xed_operand_values_has_immediate(operands);
               const unsigned imm_w = has_any_imm
                  ? xed_decoded_inst_get_immediate_width_bits(operands) : 0;
               const bool has_imm32 = has_any_imm && imm_w == 32;
               /* `cmpb $imm8,[abs32]` / `movb $imm8,[abs32]` (and imm16 forms)
                * place the disp32 BEFORE the trailing small immediate, so it is
                * NOT the last 4 instruction bytes. The simple-pointer fallback
                * below assumes disp32==last-4-bytes and would mis-read it,
                * leaving the absolute address UNRELOCATED — emitted verbatim as
                * a rip-relative disp it points outside the dylib (observed:
                * `movb $1, 0xf6761d(%rip)` writing the raw i386 __DATA addr ->
                * EXC_BAD_ACCESS). Treat ANY trailing immediate as "disp32 is the
                * memory operand" so it gets relocated like the imm32 case. */
               const bool has_small_imm =
                  has_any_imm && (imm_w == 8 || imm_w == 16);
               bool dest_is_data = false;
               ssize_t md = 0;
               /* DBG_SMALLIMM extended: capture matched seg/sect/flags */
               char dbg_segname[17] = {};
               char dbg_sectname[17] = {};
               uint32_t dbg_flags = 0;
               bool dbg_matched = false;
               if (has_imm32 || has_small_imm) {
                  md = xed_decoded_inst_get_memory_displacement(operands, i);
                  dbg_orig_md = (std::size_t) md;
                  for (auto *seg : env.archive.segments()) {
                     for (auto *sect : seg->sections) {
                        if (!sect->contains_vmaddr((std::size_t) md)) continue;
                        const uint32_t stype = sect->sect.flags & SECTION_TYPE;
                        dest_is_data = (stype == S_REGULAR || stype == S_ZEROFILL
                                        || stype == S_GB_ZEROFILL);
                        /* capture for DBG_SMALLIMM */
                        std::strncpy(dbg_segname, seg->segment_command.segname,
                                     sizeof(dbg_segname) - 1);
                        std::strncpy(dbg_sectname, sect->sect.sectname,
                                     sizeof(dbg_sectname) - 1);
                        dbg_flags = sect->sect.flags;
                        dbg_matched = true;
                        break;
                     }
                     if (dest_is_data) break;
                  }
               }
               if (has_imm32 && dest_is_data) {
                  memidx = i;
                  /*
                   * i386 `[disp32]` is ABSOLUTE addressing — the disp32
                   * is the address itself. Mark memdisp_absolute so an
                   * M32 re-emit (the `rebasify` pass) writes the address
                   * back verbatim instead of a rip-relative delta. The
                   * x86_64 transform re-derives this flag from the new
                   * decode (mod=00 r/m=101 becomes rip-relative there).
                   */
                  memdisp_absolute = true;
                  dbg_orig_md = (std::size_t) md;
                  env.vmaddr_resolver.resolve((std::size_t) md,
                                              (const SectionBlob<bits> **) &this->memdisp);
                  /* mid-blob fallback: if md lands inside a multi-byte blob
                   * (exact-key resolve misses), attach to the containing blob
                   * + offset so the store still relocates correctly. */
                  env.vmaddr_resolver.resolve_containing(
                     (std::size_t) md,
                     (const SectionBlob<bits> **) &this->memdisp,
                     &this->memdisp_offset);
                  /* probe whether the trailing imm32 is itself a pointer */
                  const std::size_t imm_idx = instbuf.size() - sizeof(uint32_t);
                  const uint32_t imm_val =
                     img.template at<uint32_t>(loc.offset + imm_idx);
                  bool imm_is_ptr = false;
                  if (imm_val >= 0x1000 && imm_val < 0x80000000U) {
                     for (auto *seg : env.archive.segments()) {
                        std::string name(seg->segment_command.segname,
                                         strnlen(seg->segment_command.segname,
                                                 sizeof(seg->segment_command.segname)));
                        if (name == SEG_PAGEZERO || name == SEG_LINKEDIT) continue;
                        if (seg->contains_vmaddr(imm_val)) { imm_is_ptr = true; break; }
                     }
                  }
                  imm = Immediate<bits>::Parse(img, loc + imm_idx, env, imm_is_ptr);
               } else if (has_small_imm && dest_is_data) {
                  /* Relocate the absolute disp32 (the memory operand). The
                   * trailing imm8/imm16 is a scalar that stays verbatim in
                   * instbuf; only the disp is patched (M32 absolute -> M64
                   * rip-relative, re-derived per-instruction at Emit). */
                  memidx = i;
                  memdisp_absolute = true;
                  dbg_orig_md = (std::size_t) md;
                  env.vmaddr_resolver.resolve((std::size_t) md,
                                              (const SectionBlob<bits> **) &this->memdisp);
                  /* mid-blob fallback (see has_imm32 branch above): a
                   * `movb/cmpb $imm8,[abs32]` whose abs32 is inside a multi-byte
                   * __data blob misses exact-key resolve; attach to the
                   * containing blob + offset. This is the iPhoto 8th-blocker. */
                  env.vmaddr_resolver.resolve_containing(
                     (std::size_t) md,
                     (const SectionBlob<bits> **) &this->memdisp,
                     &this->memdisp_offset);
                  /* DBG_SMALLIMM: diagnostic for the movb $imm8,[abs32] mis-relocation bug.
                   * Prints md (the i386 abs32 address), matched seg/sect/flags,
                   * PURE_INSTRUCTIONS/SOME_INSTRUCTIONS attribute bits,
                   * the resolved memdisp pointer, and (if resolved) which section
                   * it landed in + its vmaddr. Also prints the trailing imm8/imm16
                   * value and XED iform for identification. */
                  if (std::getenv("DBG_SMALLIMM")) {
                     /* trailing small immediate: last imm_w/8 bytes of instbuf */
                     uint32_t trail_imm = 0;
                     if (imm_w == 8 && !instbuf.empty())
                        trail_imm = instbuf.back();
                     else if (imm_w == 16 && instbuf.size() >= 2)
                        trail_imm = (uint32_t)instbuf[instbuf.size()-2] |
                                    ((uint32_t)instbuf[instbuf.size()-1] << 8);
                     const xed_iform_enum_t iform_dbg =
                        xed_decoded_inst_get_iform_enum(&xedd);
                     std::fprintf(stderr,
                        "[smallimm] instr_vmaddr=0x%zx md=0x%zx imm_w=%u trail_imm=0x%x"
                        " iform=%s matched_seg=%.16s matched_sect=%.16s"
                        " flags=0x%08x PURE_INST=%d SOME_INST=%d dest_is_data=%d"
                        " memdisp=%p",
                        (std::size_t)loc.vmaddr,
                        (std::size_t)md,
                        imm_w,
                        trail_imm,
                        xed_iform_enum_t2str(iform_dbg),
                        dbg_matched ? dbg_segname : "(none)",
                        dbg_matched ? dbg_sectname : "(none)",
                        dbg_flags,
                        (dbg_flags & S_ATTR_PURE_INSTRUCTIONS) ? 1 : 0,
                        (dbg_flags & S_ATTR_SOME_INSTRUCTIONS) ? 1 : 0,
                        dest_is_data ? 1 : 0,
                        (const void *)this->memdisp);
                     if (this->memdisp && this->memdisp->section) {
                        std::fprintf(stderr,
                           " resolved_sect=%.16s resolved_seg=%.16s resolved_vmaddr=0x%zx",
                           this->memdisp->section->sect.sectname,
                           (this->memdisp->section->segment
                              ? this->memdisp->section->segment->segment_command.segname
                              : "(null)"),
                           (std::size_t)this->memdisp->loc.vmaddr);
                     } else if (this->memdisp) {
                        std::fprintf(stderr,
                           " (memdisp non-null but section==nullptr) resolved_vmaddr=0x%zx",
                           (std::size_t)this->memdisp->loc.vmaddr);
                     } else {
                        std::fprintf(stderr, " (deferred/unresolved)");
                     }
                     std::fprintf(stderr, " obj=%p &memdisp=%p\n",
                                  (const void *)this, (const void *)&this->memdisp);
                  }
               } else if (!has_small_imm) {
                  /* simple pointer: disp32 is the last 4 bytes */
                  const std::size_t idx = instbuf.size() - sizeof(uint32_t);
                  dbg_orig_md = img.template at<uint32_t>(loc.offset + idx);
                  imm = Immediate<bits>::Parse(img, loc + idx, env, true);
               }
            } else if (basereg == XED_REG_INVALID && indexreg != XED_REG_INVALID &&
                       xed_decoded_inst_get_memory_displacement_width(operands, i) ==
                       sizeof(uint32_t)) {
               /*
                * `[disp32 + index*scale]` — jump-table or indexed-array
                * addressing. The disp32 is an absolute pointer to a table
                * base. In 64-bit mode the disp32 is sign-extended, so for
                * the same code to work after transform we have to rewrite
                * disp32 to the new vmaddr of the table.
                *
                * Skip when the instruction also carries a trailing
                * 32-bit immediate (e.g. `MOV_MEMv_IMMz` =
                * `mov [disp32+idx*4], imm32`). xed_patch_disp doesn't
                * handle that combo cleanly — the runtime patcher in
                * wrapper_setup.c will rewrite the disp32 if its value
                * lands in our dylib's vmaddr range.
                */
               const xed_iform_enum_t iform = xed_decoded_inst_get_iform_enum(&xedd);
               const bool has_trailing_imm32 =
                  xed_operand_values_has_immediate(operands)
                  && xed_decoded_inst_get_immediate_width_bits(operands) == 32;
               (void)iform; /* keep for future selective handling */

               const ssize_t disp = xed_decoded_inst_get_memory_displacement(operands, i);
               if (!has_trailing_imm32
                   && disp >= 0x1000 && (std::size_t)disp < 0x80000000U
                   && !memdisp) {
                  memidx = i;
                  memdisp_absolute = true;
                  env.vmaddr_resolver.resolve((std::size_t)disp,
                                              (const SectionBlob<bits> **)&this->memdisp);
                  /* mid-blob fallback: an absolute `[disp32+idx]` whose disp32
                   * lands inside a multi-byte blob misses exact-key resolve and
                   * ships a stale disp into read-only __TEXT (the 9th-blocker
                   * null-memdisp class). Attach to the containing blob + offset.
                   * Gated to writable __DATA (excludes __OBJC) like the other
                   * containing fallbacks — see ParseEnv::vmaddr_in_writable_data. */
                  if (env.vmaddr_in_writable_data((std::size_t)disp)) {
                     env.vmaddr_resolver.resolve_containing(
                        (std::size_t)disp,
                        (const SectionBlob<bits> **)&this->memdisp,
                        &this->memdisp_offset);
                  }
               }
            } else if (basereg != XED_REG_INVALID &&
                       basereg != select_value(bits, XED_REG_EIP, XED_REG_RIP) &&
                       basereg != select_value(bits, XED_REG_ESP, XED_REG_RSP) &&
                       basereg != select_value(bits, XED_REG_EBP, XED_REG_RBP) &&
                       indexreg == XED_REG_INVALID &&
                       xed_decoded_inst_get_memory_displacement_width(operands, i) ==
                       sizeof(uint32_t)) {
               /*
                * `[base + disp32]` absolute table addressing (i386
                * mod=10, e.g. photocd's `movl %edi, 0x11260(%edx)`).
                * The compiler indexes a fixed global table: `base`
                * holds the element offset and disp32 is the table's
                * absolute vmaddr. Our transform shifts that table, so
                * the disp32 has to be relocated — capture it in
                * `memdisp` so the transform can rewrite it.
                *
                * Guard: only when disp32 lands inside a real
                * (non-pagezero/linkedit) segment; otherwise this is an
                * ordinary `[reg+offset]` struct-field access whose
                * displacement must be emitted verbatim. esp/ebp bases
                * are excluded outright — a global table is never
                * indexed through the frame/stack pointer.
                */
               const ssize_t disp =
                  xed_decoded_inst_get_memory_displacement(operands, i);
               bool disp_in_seg = false;
               if (disp >= 0x1000 && (std::size_t) disp < 0x80000000U) {
                  for (auto *seg : env.archive.segments()) {
                     std::string name(seg->segment_command.segname,
                                      strnlen(seg->segment_command.segname,
                                              sizeof(seg->segment_command.segname)));
                     if (name == SEG_PAGEZERO || name == SEG_LINKEDIT) continue;
                     if (seg->contains_vmaddr((std::size_t) disp)) {
                        disp_in_seg = true;
                        break;
                     }
                  }
               }
               if (disp_in_seg && !memdisp) {
                  memidx = i;
                  memdisp_absolute = true;
                  env.vmaddr_resolver.resolve((std::size_t) disp,
                                              (const SectionBlob<bits> **) &this->memdisp);
               }
            }
         }
      }
      
      /* Relative Branches */
      if (xed_operand_values_has_branch_displacement(operands)) {
         const ssize_t brdisp = xed_decoded_inst_get_branch_displacement(operands);
         const size_t targetaddr = refaddr + brdisp;

         /*
          * Only resolve the target if it lands inside a real segment.
          * Linear-sweep disassembly misdecodes data interleaved in __text
          * as branch instructions; their computed targets fall outside any
          * segment and would strand an unplaceable placeholder, which is
          * fatal in Archive::Build ("not all placeholders could be
          * placed"). brdisp==nullptr is already a handled state — Emit
          * (xed_patch_brdisp) and the i386->x86_64 copy ctor both guard on
          * it — so leaving it null keeps the original displacement bytes.
          */
         bool target_in_seg = false;
         for (auto *seg : env.archive.segments()) {
            std::string name(seg->segment_command.segname,
                             strnlen(seg->segment_command.segname,
                                     sizeof(seg->segment_command.segname)));
            if (name == SEG_PAGEZERO || name == SEG_LINKEDIT) { continue; }
            if (seg->contains_vmaddr(targetaddr)) {
               target_in_seg = true;
               break;
            }
         }
         if (target_in_seg) {
            this->brdisp = env.add_placeholder(targetaddr);
         }
      }

      /* Check for other immediates */
      switch (xed_decoded_inst_get_iform_enum(&xedd)) {
      case XED_IFORM_PUSH_IMMz: /* push imm32 */
      case XED_IFORM_MOV_GPRv_IMMv:
         {
            assert(imm == nullptr);
            const std::size_t imm_idx = instbuf.size() - sizeof(uint32_t);

            /*
             * `mov reg, imm32` / `push imm32` where imm32 is an ABSOLUTE
             * data/code address. A fixed-load-address i386 image (no rebase
             * info — `rebase_size==0`, common for non-PIE main executables)
             * references its own globals by bare absolute immediate
             * (`mov $0xf4c500, %ebx; ...; mov (%ebx)`) relying on its
             * preferred base, with NO relocation to mark the site. Once
             * translated into a dylib (which slides, and whose sections move
             * as __cfstring/etc. expand), that raw immediate points at the
             * stale i386 vmaddr -> EXC_BAD_ACCESS. Detect the case the same
             * way the absolute-memory-operand paths above do — imm lands
             * inside a real (non-pagezero/linkedit) segment's vmaddr range —
             * and mark it a pointer so it gets a placeholder + relocation to
             * the translated layout. (Integer constants that happen to fall
             * in a segment's vmaddr range are the inherent false-positive
             * risk of this heuristic, already accepted for memory operands.)
             */
            bool imm_is_ptr = false;
            /* GATE: only a FIXED-load-address image (non-PIE MH_EXECUTE) ever
             * references its own globals/code by a bare absolute immediate. A
             * dylib (MH_DYLIB) and a PIE executable are position-independent —
             * they reach their data via PIC (get_pc_thunk / rip-relative), never
             * a hardcoded absolute immediate. Applying this heuristic to them
             * mis-relocates ordinary integer constants that merely alias a
             * vmaddr: e.g. libtier0's `mov $0x3400,%eax` (a loop count) became
             * `lea eax,[rip+disp]`, so the array-zeroing loop ran on a giant
             * pointer and stomped past __DATA into __LINKEDIT (SIGBUS in
             * GetGlobalLoggingSystem_Internal). Non-PIE execs (Portal 2's
             * portal2_osx, the original use case) keep the heuristic. */
            const bool fixed_load_addr =
               env.archive.header.filetype == MH_EXECUTE &&
               (env.archive.header.flags & MH_PIE) == 0;
            /* Only a full 32-bit immediate can hold a pointer. `*_IMMv`/`IMMz`
             * also cover the 16-bit-operand forms (`66`-prefixed, e.g.
             * `mov di, imm16`); there imm_idx would mis-read into the opcode
             * bytes and the M32->M64 transform (lea r32) would reject the
             * 16-bit dest reg. Skip those — a 16-bit immediate is never a ptr. */
            if (fixed_load_addr &&
                xed_decoded_inst_get_immediate_width_bits(operands) == 32) {
               const uint32_t imm_val =
                  img.template at<uint32_t>(loc.offset + imm_idx);
               if (imm_val >= 0x1000 && imm_val < 0x80000000U) {
                  for (auto *seg : env.archive.segments()) {
                     std::string name(seg->segment_command.segname,
                                      strnlen(seg->segment_command.segname,
                                              sizeof(seg->segment_command.segname)));
                     if (name == SEG_PAGEZERO || name == SEG_LINKEDIT) continue;
                     if (seg->contains_vmaddr(imm_val)) { imm_is_ptr = true; break; }
                  }
               }
            }
            imm = Immediate<bits>::Parse(img, loc + imm_idx, env, imm_is_ptr);
         }
         break;
      default:
         break;
      }

      /*
       * `mov [esp/rsp + small_disp], imm32` (c7 04/44/84 24 imm32) and
       * `mov [ebp + disp], imm32` (c7 45/85 imm32) — i386's canonical way
       * to set up a stack-passed call arg (e.g. printf format string).
       * If imm32 lands inside a real segment we treat it as a pointer
       * so the transform rewrites it to lea+store against the new vmaddr.
       *
       * Validated by tests-i386/01_hello, 03_stack_imm_args.
       * Caveat: value-range guessing has false positives — an integer
       * constant that happens to alias a vmaddr will be mis-relocated.
       * If we hit one, narrow the guard (e.g. require __cstring / __const
       * / __text destination) rather than removing the heuristic, since
       * 01_hello and every printf("format", ...) caller depends on it.
       */
      if (imm == nullptr && instbuf.size() >= 7 && instbuf.at(0) == 0xc7) {
         const uint8_t modrm = instbuf.at(1);
         const uint8_t mod = (modrm >> 6) & 0x3;
         const uint8_t rm  = modrm & 0x07;
         std::size_t imm_off = 0;
         if ((modrm & 0x38) == 0 && rm == 0x04
             && instbuf.size() >= 3 && instbuf.at(2) == 0x24) {
            if (mod == 0 && instbuf.size() == 7)       imm_off = 3;
            else if (mod == 1 && instbuf.size() == 8)  imm_off = 4;
            else if (mod == 2 && instbuf.size() == 11) imm_off = 7;
         } else if ((modrm & 0x38) == 0 && rm == 0x05) {
            if (mod == 1 && instbuf.size() == 7)       imm_off = 3;
            else if (mod == 2 && instbuf.size() == 10) imm_off = 6;
         }
         if (imm_off > 0) {
            const uint32_t value =
               img.template at<uint32_t>(loc.offset + imm_off);
            if (value >= 0x1000 && value < 0x80000000U) {
               bool in_seg = false;
               for (auto *seg : env.archive.segments()) {
                  std::string name(
                     seg->segment_command.segname,
                     strnlen(seg->segment_command.segname,
                             sizeof(seg->segment_command.segname)));
                  if (name == SEG_PAGEZERO || name == SEG_LINKEDIT) continue;
                  if (seg->contains_vmaddr(value)) { in_seg = true; break; }
               }
               if (in_seg) {
                  imm = Immediate<bits>::Parse(img, loc + imm_off, env, true);
               }
            }
         }
      }

      /* `mov [reg+disp], imm32` for a GENERAL register base (rax/rcx/rdx/rbx/
       * rsi/rdi — not esp/ebp, handled above) where imm32 is an absolute data
       * pointer the i386 image baked in, e.g. `movl $&__cfstring, 0x7c(%edi)`
       * (c7 47 7c imm32) storing a constant NSString into an ivar. The
       * esp/ebp heuristic above accepts ANY segment because stack-arg setup
       * is overwhelmingly string/pointer args; a struct-field store through an
       * arbitrary base, however, is just as often an integer ivar, so to avoid
       * mis-relocating integer constants that merely alias a vmaddr we require
       * the value to land in a CONSTANT/STRING section (cstring, cfstring,
       * const, objc metadata, text) -- a high-confidence pointer target. The
       * MOV_MEMv_IMMz transform rewrites this to a slide-correct lea+store. */
      if (imm == nullptr && instbuf.size() >= 6 && instbuf.at(0) == 0xc7
          && (instbuf.at(1) & 0x38) == 0          /* 0xc7 /0 = MOV r/m32, imm32 */
          && (instbuf.at(1) >> 6) != 3) {         /* memory destination */
         const uint8_t rm = instbuf.at(1) & 0x07;
         if (rm != 0x04 && rm != 0x05               /* esp/ebp handled above */
             && xed_operand_values_has_immediate(operands)
             && xed_decoded_inst_get_immediate_width_bits(operands) == 32) {
            const std::size_t imm_off = instbuf.size() - sizeof(uint32_t);
            const uint32_t value = img.template at<uint32_t>(loc.offset + imm_off);
            bool ptr_target = false;
            if (value >= 0x1000 && value < 0x80000000U) {
               ptr_target = env.vmaddr_in_const_section(value);
               /* Pointer-table (C++ vtable / fn-ptr dispatch array) install,
                * e.g. `movl $vtable, (%eax)` (c7 00 imm32). The imm lands in
                * __DATA,__const, which vmaddr_in_const_section deliberately
                * EXCLUDES (integer constants frequently alias the __const
                * range). Discriminate by DOUBLE-INDIRECTION: a vtable/table
                * address points to a word that is ITSELF an in-image pointer
                * (the first table entry), whereas an integer constant aliasing
                * __const points at scalar bytes. This relocates the legitimate
                * vtable store without the integer-constant false positives the
                * __const exclusion guards against. (iPhoto's C++ frameworks
                * install vtables via this exact `movl $vtable,(%reg)` form ->
                * un-relocated stale i386 vtable ptr -> EXC_BAD_ACCESS.) */
               if (!ptr_target && (value & 3) == 0) {
                  for (auto *seg : env.archive.segments()) {
                     const auto &sc = seg->segment_command;
                     std::string sn(sc.segname,
                                    strnlen(sc.segname, sizeof(sc.segname)));
                     if (sn == SEG_PAGEZERO || sn == SEG_LINKEDIT) continue;
                     if ((sc.initprot & VM_PROT_EXECUTE) != 0) continue; /* code */
                     if (!seg->contains_vmaddr(value)) continue;
                     const std::size_t toff = value - sc.vmaddr + sc.fileoff;
                     if (toff + 4 > sc.fileoff + sc.filesize) break; /* zerofill */
                     if (toff + 4 > img.size()) break;
                     const uint32_t tword = img.template at<uint32_t>(toff);
                     if (tword < 0x1000 || tword >= 0x80000000U) break;
                     for (auto *s2 : env.archive.segments()) {
                        const auto &s2c = s2->segment_command;
                        std::string s2n(s2c.segname,
                                        strnlen(s2c.segname, sizeof(s2c.segname)));
                        if (s2n == SEG_PAGEZERO || s2n == SEG_LINKEDIT) continue;
                        if (s2->contains_vmaddr(tword)) { ptr_target = true; break; }
                     }
                     break;
                  }
               }
            }
            if (ptr_target) {
               imm = Immediate<bits>::Parse(img, loc + imm_off, env, true);
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
         /* DBG_BADRELOC: catch the 8th-blocker bug at ground truth. A data
          * store/ref whose resolved memdisp lands in a section flagged as
          * containing instructions (__text) is the mis-relocation: writing
          * an i386 __DATA/__bss flag address that resolved into code. Keyed on
          * the resolved blob (NOT a code vmaddr), so it is immune to the
          * Build cross-phase vmaddr remap that derailed prior diagnosis. */
         if (std::getenv("DBG_BADRELOC")) {
            char segn[17] = {0}, secn[17] = {0};
            if (memdisp->section) {
               std::memcpy(segn, memdisp->section->sect.segname, 16);
               std::memcpy(secn, memdisp->section->sect.sectname, 16);
            } else {
               std::strcpy(segn, "(nosect)");
            }
            const bool in_text = memdisp->section &&
               std::strncmp(memdisp->section->sect.segname, SEG_TEXT, 16) == 0;
            if (in_text || !memdisp->section) {
               std::fprintf(stderr,
                  "[badreloc] iform=%s orig_md=0x%zx m64_vmaddr=0x%zx "
                  "target=0x%zx -> %s,%s abs=%d pic=%d\n",
                  xed_iform_enum_t2str(xed_decoded_inst_get_iform_enum(&xedd)),
                  dbg_orig_md, (std::size_t)this->loc.vmaddr,
                  (std::size_t)memdisp->loc.vmaddr, segn, secn,
                  (int)memdisp_absolute, (int)pic_anchored);
            }
         }
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
         xed_enc_displacement_t enc; // = {disp, width_bits};
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
#if 1
         const unsigned width_bits = xed_decoded_inst_get_branch_displacement_width_bits(&xedd);
         xed_encoder_operand_t enc;
         enc.u.brdisp = disp;
         enc.width_bits = width_bits;
         /* xed_patch_relbr was renamed to xed_patch_brdisp in newer xed. */
         if (!xed_patch_brdisp(&xedd, &*instbuf.begin(), enc)) {
            throw error("%s: xed_patch_brdisp: failed to patch instruction at offset 0x%zx, " \
                        "vmaddr 0x%zx\n", __FUNCTION__, this->loc.offset, this->loc.vmaddr);
         }
#else
         patch_relbr(xedd, instbuf, disp);
#endif
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
         assert(bits == Bits::M32);

         /* add dummy immediate */
         env.template add<Immediate>(imm, nullptr);

         switch (xed_decoded_inst_get_iform_enum(&xedd)) {
         case XED_IFORM_PUSH_IMMz:
            {
               /* i386 | push abs32_pointer  (68 imm32, 5 bytes)
                * -----|----------------------
                * stub  | lea r11,[rip+disp32]  (compute new ptr)
                *       | push r11              (8-byte push for dyld binder)
                * other | lea r11,[rip+disp32]  (compute new ptr)
                *       | push_r32(r11d)        (4-byte push of low 32)
                *
                * Stub sections (`__symbol_stub`/`__stub_helper`) push an
                * 8-byte arg into the lazy-bind ABI. Other sections (the
                * compiler emitting `push &global_func` for a 4-byte
                * caller-pushed arg) need a 4-byte push instead, otherwise
                * the callee sees its stack frame misaligned. The low 32
                * bits of r11 carry the full pointer because the M64
                * archive is placed at vmaddr < 4GB.
                */
               auto lea_inst = new Instruction<opposite<bits>>(opcode::lea_r11_mem_rip_disp32());
               lea_inst->memidx = 0;
               env.resolve(imm->pointee, &lea_inst->memdisp);
               lea_inst->memdisp_offset = imm->pointee_offset;

               const std::string sect = this->section->name();
               if (sect == SECT_SYMBOL_STUB || sect == SECT_STUB_HELPER) {
                  auto push_inst = new Instruction<opposite<bits>>(opcode::push_r11());
                  return {lea_inst, push_inst};
               }
               /* Non-stub: 4-byte push via the push_r32 widening helper.
                * Gated on bits==M32 because push_r32 returns M64-blob
                * lists; the surrounding `if (imm && imm->pointee)` block
                * has an assert(bits == M32) so this is unreachable for
                * the M64 instantiation but still needs to type-check. */
               if constexpr (bits == Bits::M32) {
                  auto push_insts = push_r32(XED_REG_R11D);
                  push_insts.push_front(lea_inst);
                  return push_insts;
               } else {
                  throw error("PUSH_IMMz non-stub in M64 transform unreachable");
               }
            }

         case XED_IFORM_MOV_GPRv_IMMv:
            {
               /* i386 | mov r32, abs32
                * -----|---------------
                * X86  | lea r32, [rip+disp32]
                *
                * Only reached when the imm is a pointer (via the
                * `imm->pointee` guard at the top of Transform). For 16-bit
                * variants (`0x66 BX+r imm16`) the immediate is a 16-bit
                * value that can't carry a 32-bit pointer, so this code
                * path doesn't make sense — bail. lea_r32_mem_rip_disp32
                * asserts r32 is in [EAX, EDI]; pre-guard so we throw a
                * descriptive error instead of crashing on the assert.
                */
               const auto r32 = xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0);
               if (r32 < XED_REG_EAX || r32 > XED_REG_EDI) {
                  throw error("%s: MOV_GPRv_IMMv at vmaddr 0x%zx: reg0=%s "
                              "out of [EAX,EDI] (likely 16-bit imm with "
                              "pointer marking)",
                              __FUNCTION__, this->loc.vmaddr,
                              xed_reg_enum_t2str(r32));
               }
               auto lea_inst = new Instruction<opposite<bits>>(opcode::lea_r32_mem_rip_disp32(r32));
               lea_inst->memidx = 0; // TODO: is this right
               env.resolve(imm->pointee, &lea_inst->memdisp);
               lea_inst->memdisp_offset = imm->pointee_offset;
               return {lea_inst};
            }

         /*
          * `mov eax, [abs32]` short form (opcode 0xa1). The general form
          * `MOV_GPRv_MEMv` with absolute addressing is the longer encoding
          * — both get mapped to the same x86_64 sequence
          * `mov r32, [rip+disp32]`.
          *
          * Note: at i386 decode time only the eax flavor (REG0=EAX) is
          * possible for the OrAX variant, but we read the register out of
          * xedd to keep the code symmetric with MOV_GPRv_MEMv if we add it.
          */
         case XED_IFORM_MOV_OrAX_MEMv:
         case XED_IFORM_MOV_GPRv_MEMv:
            {
               auto r32 = xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0);
               /* 16-bit GPR form (`0x66 0xA1 disp32` for AX, or
                * `0x66 0x8B /r [abs32]` for general r16): emit the
                * x86_64 rip-relative long form with 0x66 prefix.
                * Bytes: `66 8B 05+r<<3 disp32` (7 bytes). */
               if (r32 >= XED_REG_AX && r32 <= XED_REG_DI) {
                  const uint8_t r = (uint8_t)(r32 - XED_REG_AX);
                  auto mov = new Instruction<opposite<bits>>(
                     opcode_t({0x66, 0x8B,
                               (uint8_t)(0x05 | (r << 3)),
                               0x00, 0x00, 0x00, 0x00}));
                  mov->memidx = 0;
                  env.resolve(imm->pointee, &mov->memdisp);
                  mov->memdisp_offset = imm->pointee_offset;
                  return {mov};
               }
               if (r32 < XED_REG_EAX || r32 > XED_REG_EDI) {
                  /* fall through to the generic error below for anything we
                   * can't currently encode (r8d-r15d would only matter for
                   * a x86_64 source we're transforming — not our case). */
                  throw error("%s: MOV from abs32 with reg %s unsupported",
                              __FUNCTION__, xed_reg_enum_t2str(r32));
               }
               auto mov = new Instruction<opposite<bits>>(opcode::mov_r32_mem_rip_disp32(r32));
               mov->memidx = 0;
               env.resolve(imm->pointee, &mov->memdisp);
               mov->memdisp_offset = imm->pointee_offset;
               return {mov};
            }

         /*
          * 8-bit moffs short forms — `mov al, [abs32]` (0xA0) and
          * `mov [abs32], al` (0xA2). Same problem as the 32-bit forms
          * below: i386 uses a 4-byte abs address while x86_64's bare
          * 0xA0/A2 expects an 8-byte moffs64, so the raw bytes don't
          * survive a default copy-ctor decode (BUFFER_TOO_SHORT).
          * Rewrite to the general `8A/88 /r mod=00 r/m=101` rip-relative
          * form (reg=AL=000): `8A 05 disp32` / `88 05 disp32` — 6 bytes.
          */
         case XED_IFORM_MOV_AL_MEMb:
            {
               auto mov = new Instruction<opposite<bits>>(
                  opcode_t({0x8A, 0x05, 0x00, 0x00, 0x00, 0x00}));
               mov->memidx = 0;
               env.resolve(imm->pointee, &mov->memdisp);
               mov->memdisp_offset = imm->pointee_offset;
               return {mov};
            }

         case XED_IFORM_MOV_MEMb_AL:
            {
               auto mov = new Instruction<opposite<bits>>(
                  opcode_t({0x88, 0x05, 0x00, 0x00, 0x00, 0x00}));
               mov->memidx = 0;
               env.resolve(imm->pointee, &mov->memdisp);
               mov->memdisp_offset = imm->pointee_offset;
               return {mov};
            }

         /* Inverse: `mov [abs32], eax` short form (0xa3) / general form. */
         case XED_IFORM_MOV_MEMv_OrAX:
         case XED_IFORM_MOV_MEMv_GPRv:
            {
               auto r32 = xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0);
               /* 16-bit form: `66 89 05+r<<3 disp32` (7 bytes). */
               if (r32 >= XED_REG_AX && r32 <= XED_REG_DI) {
                  const uint8_t r = (uint8_t)(r32 - XED_REG_AX);
                  auto mov = new Instruction<opposite<bits>>(
                     opcode_t({0x66, 0x89,
                               (uint8_t)(0x05 | (r << 3)),
                               0x00, 0x00, 0x00, 0x00}));
                  mov->memidx = 0;
                  env.resolve(imm->pointee, &mov->memdisp);
                  mov->memdisp_offset = imm->pointee_offset;
                  return {mov};
               }
               if (r32 < XED_REG_EAX || r32 > XED_REG_EDI) {
                  throw error("%s: MOV to abs32 from reg %s unsupported",
                              __FUNCTION__, xed_reg_enum_t2str(r32));
               }
               auto mov = new Instruction<opposite<bits>>(opcode::mov_mem_rip_disp32_r32(r32));
               mov->memidx = 0;
               env.resolve(imm->pointee, &mov->memdisp);
               mov->memdisp_offset = imm->pointee_offset;
               return {mov};
            }

         case XED_IFORM_MOV_MEMv_IMMz:
            {
               /* `mov mem, imm32` where imm32 is a pointer. Two dest shapes:
                *
                * (a) REGISTER-BASE dest, e.g. `movl $&cfstring, 0x7c(%rdi)`
                *     (c7 47 7c imm32) — no memdisp. A plain immediate store
                *     can't hold the SLID runtime address (it's a 32-bit field
                *     baked in __text; the dylib slides), so emit a slide-correct
                *     sequence:
                *        lea r11, [rip+disp32]      ; r11 = pointee's new vmaddr
                *        mov  [reg+disp], r11d       ; store low 32 (dylib < 4GB)
                *     r11 is the translator's scratch (no i386 reg maps to it).
                *     This is PRECISE — only instructions the parser marked as
                *     pointer-bearing are rewritten, unlike a blind __text scan.
                *
                * (b) ABS32 dest `mov [abs32], imm32` (c7 05 disp32 imm32) — the
                *     parser put the dest in memdisp and the imm32 in imm. Keep
                *     the rip-relative form and let the wrapper's runtime __text
                *     patcher slide the imm (a STANDALONE Immediate avoids a
                *     bogus dyld rebase for the embedded 32-bit slot). */
               if (!this->memdisp) {
                  auto *lea_inst = new Instruction<opposite<bits>>(
                     opcode::lea_r11_mem_rip_disp32());
                  lea_inst->memidx = 0;
                  env.resolve(imm->pointee, &lea_inst->memdisp);
                  lea_inst->memdisp_offset = imm->pointee_offset;

                  /* mov [mem], r11d : REX.R + 0x89 + modrm(reg=r11) + sib/disp.
                   * Reuse the original c7 /0 ModR/M + any SIB/disp bytes
                   * (everything after the ModR/M except the trailing imm32);
                   * swap the reg field to r11 (low 3 = 011) and add REX.R. */
                  std::vector<uint8_t> mb;
                  mb.push_back(0x44);                                   /* REX.R */
                  mb.push_back(0x89);                                   /* MOV r/m32,r32 */
                  mb.push_back((uint8_t)((instbuf.at(1) & 0xC7) | (0x3 << 3)));
                  for (std::size_t bi = 2; bi + sizeof(uint32_t) < instbuf.size(); ++bi) {
                     mb.push_back(instbuf.at(bi));
                  }
                  auto *mov_inst = new Instruction<opposite<bits>>(opcode_t(mb));
                  return {lea_inst, mov_inst};
               }

               auto *clone = new Instruction<opposite<bits>>(instbuf);
               clone->memidx = memidx;
               env.resolve(this->memdisp, &clone->memdisp);
               auto *m64imm = Immediate<opposite<bits>>::Create(0);
               env.resolve(imm->pointee, &m64imm->pointee);
               m64imm->pointee_offset = imm->pointee_offset;
               clone->imm = m64imm;
               return {clone};
            }

         case XED_IFORM_JMP_MEMv:
            {
               /* i386 | jmp [abs32]    (FF 25 disp32, 6 bytes)
                * -----|-----------------
                * X86  | jmp [rip+disp32]  (FF 25 disp32, 6 bytes)
                *
                * Same encoding in both modes; only the addressing-mode
                * interpretation changes — BUT the x86_64 form reads an
                * 8-byte slot where the i386 form read 4 bytes. Mirror
                * CALL_NEAR_MEMv's slot-width split:
                *
                *  (a) 4-byte target slot (S_REGULAR function-pointer
                *      table, S_LITERAL_POINTERS — e.g. a compiler-
                *      emitted indirect jump through a global fnptr in
                *      `__text`; iPhoto trips this from a non-stub
                *      site): the byte-identical form would join two
                *      adjacent 4-byte slots into one bogus address.
                *      Split into:
                *
                *        mov eax, [rip+disp32]   ; 4-byte load, zero-ext
                *        jmp rax
                *
                *  (b) 8-byte slot (S_LAZY_SYMBOL_POINTERS,
                *      S_NON_LAZY_SYMBOL_POINTERS — dyld-managed and
                *      promoted to ptr-sized by our transform; the
                *      `__symbol_stub`/`__stub_helper` indirect jumps):
                *      keep the byte-identical `jmp [rip+disp32]`.
                */
               if constexpr (bits == Bits::M32) {
                  bool target_is_8byte = false;
                  if (imm->pointee && imm->pointee->section) {
                     const uint32_t stype =
                        imm->pointee->section->sect.flags & SECTION_TYPE;
                     if (stype == S_LAZY_SYMBOL_POINTERS ||
                         stype == S_NON_LAZY_SYMBOL_POINTERS) {
                        target_is_8byte = true;
                     }
                  }
                  if (!target_is_8byte) {
                     auto mov_inst = new Instruction<Bits::M64>(
                        opcode::mov_r32_mem_rip_disp32(XED_REG_EAX));
                     mov_inst->memidx = 0;
                     env.resolve(imm->pointee, &mov_inst->memdisp);
                     mov_inst->memdisp_offset = imm->pointee_offset;
                     auto jmp_rax = new Instruction<Bits::M64>(
                        opcode::jmp_r64(XED_REG_RAX));
                     return {mov_inst, jmp_rax};
                  }
               }
               auto jmp_inst = new Instruction<opposite<bits>>(opcode::jmp_mem_rip_disp32());
               jmp_inst->memidx = 0;
               env.resolve(imm->pointee, &jmp_inst->memdisp);
               jmp_inst->memdisp_offset = imm->pointee_offset;
               return {jmp_inst};
            }

         case XED_IFORM_CALL_NEAR_MEMv:
            {
               /* i386 `call [abs32]` (FF 15 disp32). i386 reads 4 bytes
                * from [abs32] and calls that address.
                *
                * Two cases depending on the target slot in M64:
                *
                *  (a) 4-byte slot (S_REGULAR user-defined function-pointer
                *      table, S_LITERAL_POINTERS): the byte-identical
                *      x86_64 form would read 8 bytes, joining two
                *      adjacent slots into one bogus address. We split:
                *
                *        mov eax, [rip+disp32]   ; 4-byte load, zero-ext
                *        <call_op>(jmp rax)
                *
                *  (b) 8-byte slot (S_LAZY_SYMBOL_POINTERS,
                *      S_NON_LAZY_SYMBOL_POINTERS — dyld-managed and
                *      promoted to ptr-sized by our transform): the
                *      byte-identical `call [rip+disp32]` works
                *      correctly. Falls through to the default rule.
                *
                * Targets with S_REGULAR sections in __DATA that the
                * compiler emitted with i386-stride 4 are also 4-byte;
                * we detect via the pointee's section flags.
                *
                * Gated on bits==M32 because call_op returns M64-blob
                * lists; the surrounding `if (imm && imm->pointee)` block
                * asserts bits == M32 at entry. */
               if constexpr (bits == Bits::M32) {
                  /* Decide based on pointee's section: 8-byte slots in
                   * M64 (S_LAZY_SYMBOL_POINTERS, S_NON_LAZY_SYMBOL_POINTERS)
                   * fall through to default. */
                  bool target_is_8byte = false;
                  if (imm->pointee && imm->pointee->section) {
                     const uint32_t stype =
                        imm->pointee->section->sect.flags & SECTION_TYPE;
                     if (stype == S_LAZY_SYMBOL_POINTERS ||
                         stype == S_NON_LAZY_SYMBOL_POINTERS) {
                        target_is_8byte = true;
                     }
                  }
                  if (target_is_8byte) {
                     /* Byte-identical `call [rip+disp32]` reading the
                      * promoted 8-byte dyld slot. The target is a NATIVE
                      * function (libabiconv shim / dyld_stub_binder
                      * protocol) that returns with a native 8-byte ret,
                      * so the 8-byte return-address push is intentional.
                      * Constructed explicitly to stay EXEMPT from the
                      * width guard in the default rules. */
                     auto *clone = new Instruction<Bits::M64>(instbuf);
                     clone->memidx = 0;
                     env.resolve(imm->pointee, &clone->memdisp);
                     clone->memdisp_offset = imm->pointee_offset;
                     return {clone};
                  }

                  auto mov_inst = new Instruction<Bits::M64>(
                     opcode::mov_r32_mem_rip_disp32(XED_REG_EAX));
                  mov_inst->memidx = 0;
                  env.resolve(imm->pointee, &mov_inst->memdisp);
                  mov_inst->memdisp_offset = imm->pointee_offset;

                  auto jmp_inst = new Instruction<Bits::M64>(
                     opcode::jmp_r64(XED_REG_RAX));
                  auto insts = call_op(jmp_inst);
                  auto it = insts.end();
                  --it; --it;
                  insts.insert(it, mov_inst);
                  return insts;
               } else {
                  throw error("CALL_NEAR_MEMv in M64 transform unreachable");
               }
            }

         case XED_IFORM_PUSH_MEMv:
            {
               /* i386 `push dword [abs32]` (FF 35 disp32). The
                * byte-identical x86_64 form pushes a QWORD: it reads
                * 8 bytes (joining the 4-byte slot with its neighbor)
                * AND decrements rsp by 8, shifting every later cdecl
                * argument (found via tests-i386/17: printf args after
                * the push were off by one slot). Split into a 4-byte
                * load + 4-byte push:
                *
                *   mov r11d, [rip+disp32]
                *   <push_r32(r11d)>
                *
                * r11d as scratch: i386 code can't use r8-r15, and eax
                * may be live across a push. */
               if constexpr (bits == Bits::M32) {
                  auto mov_inst = new Instruction<Bits::M64>(
                     opcode::mov_r32_mem_rip_disp32(XED_REG_R11D));
                  mov_inst->memidx = 0;
                  env.resolve(imm->pointee, &mov_inst->memdisp);
                  mov_inst->memdisp_offset = imm->pointee_offset;
                  typename SectionBlob<Bits::M32>::SectionBlobs insts;
                  insts.push_back(mov_inst);
                  insts.splice(insts.end(), push_r32(XED_REG_R11D));
                  return insts;
               } else {
                  throw error("PUSH_MEMv in M64 transform unreachable");
               }
            }

         case XED_IFORM_POP_MEMv:
            {
               /* i386 `pop dword [abs32]` (8F 05 disp32) pops 4 bytes
                * into the slot; byte-identical M64 pops 8 (wrong rsp
                * adjust + 8-byte store). Rewrite:
                *
                *   <pop_r32(r11d)>           ; mov r11d,[rsp]; lea rsp,[rsp+4]
                *   mov [rip+disp32], r11d
                */
               if constexpr (bits == Bits::M32) {
                  auto store_inst = new Instruction<Bits::M64>(
                     opcode::mov_mem_rip_disp32_r32(XED_REG_R11D));
                  store_inst->memidx = 0;
                  env.resolve(imm->pointee, &store_inst->memdisp);
                  store_inst->memdisp_offset = imm->pointee_offset;
                  auto insts = pop_r32(XED_REG_R11D);
                  insts.push_back(store_inst);
                  return insts;
               } else {
                  throw error("POP_MEMv in M64 transform unreachable");
               }
            }

         default:
            {
               /*
                * Pre-probe: does the i386 instbuf decode as x86_64? If
                * not, this is an iform we don't have an explicit case
                * for and the bytes won't survive `new Instruction(...)`.
                * Almost always misdecoded data that happens to carry an
                * imm that looked like a pointer at parse time — see the
                * MOV_AL_MEMb / MOV_MEMb_AL cases above for the
                * counterexample family (which now have real translations).
                * For everything else, substitute NOPs of equal byte count;
                * preserves section layout without crashing the translator.
                */
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
                  /* Drop the imm — our NOPs already fill the buffer; we
                   * also can't honor the pointee since the opcode form
                   * is gone. */
                  return {clone};
               }

               /* Byte-identical pass-through: require width equivalence
                * between the two decodes (probe_xedd is the dest-mode
                * decode computed above). */
               check_transform_width(xedd, probe_xedd, instbuf,
                                     this->loc.vmaddr);

               auto *clone = new Instruction<opposite<bits>>(instbuf);
               clone->memidx = 0;

               /*
                * Two flavors hit this default:
                *
                *  1. `[abs32]`-form memory operand (i386 mod=00 r/m=101).
                *     The ModR/M byte means rip-relative in x86_64, so the
                *     bytes already encode the right opcode/operand
                *     structure — we just need to recompute disp32 against
                *     the new RIP via `memdisp`.
                *
                *  2. instruction with a literal imm32 that happens to be
                *     a pointer (e.g. `mov [rsp+N], <cstring_addr>` from
                *     a parser branch like the c7-prefix MOV_MEMv_IMMz
                *     handler). Here the bytes don't encode a memdisp at
                *     all — patching memdisp would corrupt the instruction.
                *     Keep `imm` on the clone instead so Emit writes the
                *     resolved pointee value over the trailing 4 bytes.
                *
                * Distinguish via XED's memory-displacement width on the
                * decoded clone: nonzero ⇒ flavor 1, zero ⇒ flavor 2.
                */
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
#if 0
            const auto reg1 = xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG1);
            const auto base_reg = xed_decoded_inst_get_base_reg(&xedd, 0);
            const auto index_reg = xed_decoded_inst_get_index_reg(&xedd, 0);
#endif
            
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
                  auto jmp_inst = new Instruction<Bits::M64>
                     (opcode::jmp_r64(opcode::r32_to_r64(reg0)));
                  return call_op(jmp_inst);
               }

            case XED_IFORM_CALL_NEAR_MEMv:
               {
                  /*
                   * `call dword [mem]` (i386 `FF /2`) reads 4 bytes from
                   * memory and calls that address. The byte-identical
                   * x86_64 encoding reads 8 bytes, joining two adjacent
                   * 4-byte slots into one bogus 64-bit address. Split:
                   *
                   *     mov eax, [mem]        ; 4-byte load (zero-ext rax)
                   *     <call_op>(jmp rax)    ; i386 4-byte ret-addr push
                   *
                   * `FF /2` -> `8B /0`: opcode 0xFF→0x8B, ModR/M reg field
                   * 010→000 (eax). Keep mod+rm so the addressing form
                   * (disp32, SIB, base+disp, …) survives untouched.
                   *
                   * The `[disp32]` form (no-base, no-index) goes through
                   * the imm->pointee path in the first switch — the
                   * parser tags it with a pointee imm. memdisp is set by
                   * the `[disp32+idx*scale]` parser (line 266+) and must
                   * be resolved to the translated pointee.
                   *
                   * REGISTER-RELATIVE forms — `[base+disp]` (e.g. a
                   * function pointer in a stack slot `call [ebp-0x1c]` or
                   * a struct field `call [eax+0x10]`) and
                   * `[base+idx*scale+disp]` — have NO memdisp/pointee
                   * (the address is computed at runtime from registers,
                   * nothing to relocate) but STILL read a 4-byte i386
                   * slot. They must be narrowed too, otherwise the
                   * default rule emits a byte-identical `callq *qword
                   * [mem]` that reads 8 bytes and joins the real 32-bit
                   * pointer with adjacent garbage (observed: photocd
                   * `call *-0x1c(%rbp)` -> rip=0x<garbage>_<realptr>).
                   * For these the addressing bytes are byte-identical in
                   * M64 (the i386 base reg maps to its 64-bit name), so
                   * we copy the operand untouched and skip resolution.
                   */
                  const xed_operand_values_t *call_ops =
                     xed_decoded_inst_operands_const(&xedd);
                  const bool reg_relative =
                     xed_decoded_inst_get_base_reg(call_ops, 0) != XED_REG_INVALID ||
                     xed_decoded_inst_get_index_reg(call_ops, 0) != XED_REG_INVALID;
                  if (!memdisp && !reg_relative) { break; }
                  auto mov_buf = instbuf;
                  std::size_t op_idx = 0;
                  while (op_idx < mov_buf.size() &&
                         (mov_buf[op_idx] == 0x66 || mov_buf[op_idx] == 0x67)) {
                     ++op_idx;
                  }
                  const uint8_t modrm = mov_buf.at(op_idx + 1);
                  if (((modrm >> 3) & 0x07) != 0x02) {
                     throw error("CALL_NEAR_MEMv with unexpected ModR/M 0x%02x at vmaddr 0x%zx",
                                 modrm, this->loc.vmaddr);
                  }
                  mov_buf[op_idx] = 0x8B;
                  mov_buf[op_idx + 1] = modrm & 0xC7;     /* reg -> eax */
                  auto mov_inst = new Instruction<Bits::M64>(mov_buf);
                  mov_inst->memidx = 0;
                  /* mod=00 rm=101 in M32 = `[disp32]` (absolute); in M64
                   * the same bytes decode as `[rip+disp32]`. We want the
                   * disp resolved rip-relative so the load hits the
                   * translated pointee at runtime. That's the default
                   * (memdisp_absolute=false). For the SIB-no-base form
                   * (rm=100, SIB base=101) it stays absolute — match what
                   * JMP_MEMv does. */
                  if ((modrm & 0xC7) == 0x04) {
                     /* SIB present: check for no-base (SIB base=101 with
                      * mod=00). */
                     const uint8_t sib = mov_buf.at(op_idx + 2);
                     if ((sib & 0x07) == 0x05) {
                        mov_inst->memdisp_absolute = true;
                     }
                  }
                  if (memdisp) {
                     env.resolve(memdisp, &mov_inst->memdisp);
                  }
                  /* Preserve brdisp if any (rare for indirect call but the
                   * parser may still have set it via the reloc table). */
                  if (brdisp) {
                     env.resolve(brdisp, &mov_inst->brdisp);
                  }

                  auto jmp_inst =
                     new Instruction<Bits::M64>(opcode::jmp_r64(XED_REG_RAX));
                  auto insts = call_op(jmp_inst);
                  /* call_op layout: lea, push_r32(r11d) (3 insts), jmp_inst,
                   * ret_placeholder. Splice mov_inst right before jmp_inst. */
                  auto it = insts.end();
                  --it; --it;
                  insts.insert(it, mov_inst);
                  return insts;
               }

            case XED_IFORM_JMP_MEMv:
               {
                  /*
                   * `jmp dword [disp32 + idx*4]` (i386) — used by switch
                   * dispatch with a 4-byte function-pointer jump table.
                   *
                   * In 64-bit mode `jmp r/m64` reads 8 bytes, joining two
                   * adjacent 4-byte table entries into one bogus 64-bit
                   * address. We split this into:
                   *
                   *     mov  eax, [disp32 + idx*4]   ; 32-bit load, zero-ext
                   *     jmpq rax                     ; 64-bit indirect jmp
                   *
                   * Using rax as scratch overwrites the index (if it was
                   * also rax), but the original `jmpq *mem` doesn't preserve
                   * regs across the jump either.
                   *
                   * Triggers for EVERY register-relative form: jump tables
                   * `[disp32 + idx*scale]`, function pointers in struct
                   * fields or stack slots `[base + disp]`, and
                   * `[base + idx*scale + disp]` — all of them read a 4-byte
                   * i386 slot that the byte-identical M64 form would read
                   * as 8 bytes (same family as CALL_NEAR_MEMv above).
                   * Bare `[disp32]` (no base, no index) still falls through:
                   * those are dyld stub/non-lazy-pointer slots widened to
                   * 8 bytes by the transform, handled via the imm->pointee
                   * path in the first switch or the default rule.
                   */
                  const auto* operands = xed_decoded_inst_operands_const(&xedd);
                  const xed_reg_enum_t basereg  =
                     xed_decoded_inst_get_base_reg(operands, 0);
                  const xed_reg_enum_t indexreg =
                     xed_decoded_inst_get_index_reg(operands, 0);
                  if (basereg == XED_REG_INVALID && indexreg == XED_REG_INVALID) {
                     break;
                  }

                  /* Build `mov eax, [SAME mem operand]` from the instbuf:
                   * `FF /4` -> `8B /0`, keeping mod+rm so the addressing
                   * form (SIB, base+disp, ...) survives untouched. */
                  auto mov_buf = instbuf;
                  /* Strip 0x66/0x67 prefix bytes if present so byte 0 is
                   * the opcode. */
                  size_t op_idx = 0;
                  while (op_idx < mov_buf.size() && (mov_buf[op_idx] == 0x66
                                                     || mov_buf[op_idx] == 0x67)) {
                     ++op_idx;
                  }
                  const uint8_t modrm = mov_buf.at(op_idx + 1);
                  if (((modrm >> 3) & 0x07) != 0x04) {
                     throw error("JMP_MEMv with unexpected ModR/M 0x%02x at vmaddr 0x%zx",
                                 modrm, this->loc.vmaddr);
                  }
                  mov_buf[op_idx] = 0x8B;             /* mov r32, r/m32 */
                  mov_buf[op_idx + 1] = modrm & 0xC7; /* reg -> eax */
                  auto mov_inst = new Instruction<Bits::M64>(mov_buf);
                  mov_inst->memidx = 0;
                  /* SIB no-base form (mod=00 rm=100, SIB base=101) is an
                   * absolute disp32 in M64 too — keep it absolute (the
                   * wrapper's runtime __text patcher applies the slide),
                   * matching CALL_NEAR_MEMv. Other forms keep their
                   * register-relative disp untouched. */
                  if ((modrm & 0xC7) == 0x04) {
                     const uint8_t sib = mov_buf.at(op_idx + 2);
                     if ((sib & 0x07) == 0x05) {
                        mov_inst->memdisp_absolute = true;
                     }
                  }
                  if (memdisp) {
                     env.resolve(memdisp, &mov_inst->memdisp);
                  }

                  auto jmp_inst = new Instruction<Bits::M64>(
                     opcode::jmp_r64(XED_REG_RAX));
                  return {mov_inst, jmp_inst};
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
                  return {mov_inst, lea_inst, jmp_inst};
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

            case XED_IFORM_PUSH_MEMv:
               {
                  /*
                   * `push [r/m]` → split into `mov eax, [r/m]; push eax`.
                   * Was: assert(mov_byte bits) — crashed on any PUSH_MEMv
                   * with a legacy prefix (0x66/0x67/segment override),
                   * which iPhoto's 13 MB __text definitely contains.
                   * Detect prefixes, locate ModR/M, sanity-check that
                   * reg=110 (/6 = PUSH); throw cleanly if anything is
                   * outside what this rewrite supports — `error` is a
                   * std::runtime_error subclass and is catchable up the
                   * stack, unlike abort()/assert().
                   */
                  opcode_t mov_buf = instbuf;
                  std::size_t modrm_idx = 1;
                  while (modrm_idx < mov_buf.size()) {
                     const uint8_t b = mov_buf.at(modrm_idx - 1);
                     if (b == 0x66 || b == 0x67 || b == 0x26 || b == 0x2E ||
                         b == 0x36 || b == 0x3E || b == 0x64 || b == 0x65 ||
                         b == 0xF0 || b == 0xF2 || b == 0xF3) {
                        ++modrm_idx;
                        continue;
                     }
                     break;
                  }
                  if (modrm_idx >= mov_buf.size()) {
                     throw error("PUSH_MEMv: ModR/M past end at vmaddr 0x%zx",
                                 this->loc.vmaddr);
                  }
                  uint8_t mov_byte = mov_buf.at(modrm_idx);
                  /* Convert PUSH /6 (reg=110) → MOV /3 (reg=011) and prepend
                   * REX.R (0x44), making the destination R11D — the register
                   * push_r32(XED_REG_R11D) below actually pushes. (A previous
                   * version encoded reg=100+REX.R = R12D, so the loaded value
                   * was discarded and stale R11 got pushed instead.) The REX
                   * byte must immediately precede the opcode, after any
                   * legacy prefixes. */
                  if (((mov_byte >> 3) & 0x07) != 0x06) {
                     throw error("PUSH_MEMv with unexpected ModR/M 0x%02x at vmaddr 0x%zx",
                                 mov_byte, this->loc.vmaddr);
                  }
                  mov_buf.at(modrm_idx - 1) = 0x8b;   /* opcode: mov r32, r/m32 */
                  mov_byte = (mov_byte & ~(uint8_t)(0x07 << 3)) | (0x03 << 3);
                  mov_buf.at(modrm_idx) = mov_byte;
                  mov_buf.insert(mov_buf.begin() + (modrm_idx - 1), 0x44);
                  typename SectionBlob<Bits::M32>::SectionBlobs insts;
                  auto mov_inst = new Instruction<opposite<bits>>(mov_buf);
                  insts.push_back(mov_inst);
                  insts.splice(insts.end(), push_r32(XED_REG_R11D));
                  return insts;
               }

            case XED_IFORM_POP_MEMv:
               {
                  /*
                   * `pop dword [r/m]` (i386 8F /0) pops 4 bytes into the
                   * memory slot. The byte-identical x86_64 form pops 8:
                   * wrong rsp adjustment AND an 8-byte store smearing the
                   * neighboring word. Rewrite:
                   *
                   *   mov r11d, [rsp]    ; 4-byte load of TOS
                   *   lea rsp, [rsp+4]   ; i386-width pop
                   *   mov [r/m], r11d    ; original addressing bytes
                   *
                   * The store runs AFTER the rsp increment, matching
                   * Intel POP semantics for ESP-based destinations.
                   * `8F /0` -> `89 /3 + REX.R` (mov r/m32, r11d), keeping
                   * mod+rm so the addressing form survives untouched.
                   * 16-bit form (66 8F /0) pops 2 bytes in both modes —
                   * fall through to the byte-identical default rule.
                   * The `[abs32]` pointee-tagged form is handled in the
                   * first switch.
                   */
                  if (effective_width == 16) { break; }
                  opcode_t store_buf = instbuf;
                  std::size_t modrm_idx = 1;
                  while (modrm_idx < store_buf.size()) {
                     const uint8_t b = store_buf.at(modrm_idx - 1);
                     if (b == 0x66 || b == 0x67 || b == 0x26 || b == 0x2E ||
                         b == 0x36 || b == 0x3E || b == 0x64 || b == 0x65 ||
                         b == 0xF0 || b == 0xF2 || b == 0xF3) {
                        ++modrm_idx;
                        continue;
                     }
                     break;
                  }
                  if (modrm_idx >= store_buf.size()) {
                     throw error("POP_MEMv: ModR/M past end at vmaddr 0x%zx",
                                 this->loc.vmaddr);
                  }
                  uint8_t store_byte = store_buf.at(modrm_idx);
                  if (((store_byte >> 3) & 0x07) != 0x00) {
                     throw error("POP_MEMv with unexpected ModR/M 0x%02x at vmaddr 0x%zx",
                                 store_byte, this->loc.vmaddr);
                  }
                  store_buf.at(modrm_idx - 1) = 0x89;   /* mov r/m32, r32 */
                  store_buf.at(modrm_idx) = (uint8_t)(store_byte | (0x03 << 3));
                  store_buf.insert(store_buf.begin() + (modrm_idx - 1), 0x44);
                  auto store_inst = new Instruction<opposite<bits>>(store_buf);
                  auto insts = pop_r32(XED_REG_R11D);
                  insts.push_back(store_inst);
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

            /*
             * PIC-anchored [base + disp32] (Tessera root cause):
             * Parser's DetectPicAnchoredDisps tagged this with
             * memdisp = SectionBlob at (anchor_vmaddr + disp). Rewrite
             * the load/store as rip-relative, dropping the base reg
             * entirely. Works for any ModR/M shape `mod∈{01,10} r/m=base`
             * with no SIB: keep prefixes + opcode + reg field, change
             * mod=00 r/m=101 (rip-relative), replace disp with a 4-byte
             * placeholder resolved at emit. The base register is no
             * longer read by the rewritten instruction — it still gets
             * loaded by the `pop` upstream (low 32 of anchor vmaddr) but
             * is now dead w.r.t. memory addressing.
             */
            if (pic_anchored && memdisp) {
               /* Locate ModR/M after legacy prefixes. Reject 0x67
                * (addr-size override) and 0x0F-prefixed multi-byte
                * opcodes — the rewrite assumes a 1-byte opcode and a
                * normal ModR/M layout. */
               std::size_t p = 0;
               bool have_66 = false;
               while (p < instbuf.size()) {
                  const uint8_t b = instbuf.at(p);
                  if (b == 0x66) { have_66 = true; ++p; continue; }
                  if (b == 0x67) {
                     throw error("%s: pic_anchored with 0x67 prefix at "
                                 "vmaddr 0x%zx", __FUNCTION__,
                                 this->loc.vmaddr);
                  }
                  if (b == 0xF0 || b == 0xF2 || b == 0xF3 ||
                      b == 0x2E || b == 0x36 || b == 0x3E ||
                      b == 0x26 || b == 0x64 || b == 0x65) {
                     ++p; continue;
                  }
                  break;
               }
               if (p >= instbuf.size()) {
                  goto pic_anchor_fallthrough;
               }
               /* Opcode may be multi-byte: 0x0F xx (SSE: movups/movss/
                * movsd/...) or 0x0F 0x38/0x3A xx. clang's i386 codegen
                * loads float struct literals through the PIC anchor with
                * SSE (`movups 0x1057(%eax), %xmm0`), so these must take
                * the rip-relative rewrite too — the fallthrough rewrite
                * below ADDS the base reg (anchor) as an index and reads
                * anchor+target garbage. */
               std::size_t opcode_len = 1;
               if (instbuf.at(p) == 0x0F) {
                  opcode_len = (p + 1 < instbuf.size() &&
                                (instbuf.at(p + 1) == 0x38 ||
                                 instbuf.at(p + 1) == 0x3A)) ? 3 : 2;
               }
               if (p + opcode_len > instbuf.size()) {
                  goto pic_anchor_fallthrough;
               }
               const std::size_t opcode_idx = p;
               p += opcode_len; /* p -> ModR/M */
               if (p >= instbuf.size()) {
                  throw error("%s: pic_anchored: ModR/M past end at "
                              "vmaddr 0x%zx", __FUNCTION__,
                              this->loc.vmaddr);
               }
               const std::size_t modrm_idx = p;
               const uint8_t modrm = instbuf.at(modrm_idx);
               const uint8_t reg_field = modrm & 0x38;
               const uint8_t rm        = modrm & 0x07;
               const uint8_t mod       = modrm & 0xC0;
               const bool has_sib = (rm == 0x04);
               if (has_sib) {
                  /* [base + idx*scale + disp] — bypassing base would
                   * lose the index. Not handled; fall back to existing
                   * rewrite (which preserves base+r11 addressing). */
                  goto pic_anchor_fallthrough;
               }
               /* Compute original disp size to find any trailing imm. */
               std::size_t disp_bytes = 0;
               if (mod == 0x40) { disp_bytes = 1; }
               else if (mod == 0x80) { disp_bytes = 4; }
               else if (mod == 0x00 && rm == 0x05) { disp_bytes = 4; }
               /* else mod==0x00 with rm!=0x05: no disp (shouldn't happen
                * because parser only attaches memdisp when disp width is
                * 4; mod==0x00 rm!=0x05 means no displacement). */
               const std::size_t after =
                  modrm_idx + 1 + disp_bytes;
               const std::size_t imm_bytes =
                  instbuf.size() > after ? instbuf.size() - after : 0;

               /* Build rewritten instbuf: prefixes + opcode + new ModR/M
                * (mod=00 r/m=101, reg preserved) + disp32 placeholder +
                * trailing imm verbatim. */
               opcode_t buf;
               for (std::size_t j = 0; j < opcode_idx + opcode_len; ++j) {
                  buf.push_back(instbuf.at(j));
               }
               buf.push_back((uint8_t)(0x05 | reg_field));
               buf.push_back(0x00);
               buf.push_back(0x00);
               buf.push_back(0x00);
               buf.push_back(0x00);
               for (std::size_t j = instbuf.size() - imm_bytes;
                    j < instbuf.size(); ++j) {
                  buf.push_back(instbuf.at(j));
               }

               auto *new_inst = new Instruction<opposite<bits>>(buf);
               new_inst->memidx = 0;
               new_inst->memdisp_absolute = false; /* rip-relative */
               env.resolve(memdisp, &new_inst->memdisp);
               return {new_inst};
            }
         pic_anchor_fallthrough:

            /*
             * `[base + disp32]` absolute table access (e.g. i386
             * `movl %edi, 0x11260(%edx)`). The parser captured the
             * table's absolute address in `memdisp`. A bare disp32 is
             * not slide-correct once the translated dylib moves, so
             * rewrite:
             *
             *   i386 | <op> [base + disp32]
             *   -----|------------------------------
             *   X86  | lea  r11, [rip + disp32]   ; r11 = table base
             *        | <op> [base + r11*1]        ; index addressing
             *
             * rip-relative `lea` tracks the ASLR slide automatically,
             * so — unlike the `[disp32+idx*scale]` jump-table form —
             * this needs no wrapper-side runtime __text patching.
             */
            if (memdisp
                && xed_decoded_inst_get_iform_enum(&xedd) != XED_IFORM_JMP_MEMv) {
               const xed_operand_values_t* mops =
                  xed_decoded_inst_operands_const(&xedd);
               const xed_reg_enum_t br =
                  xed_decoded_inst_get_base_reg(mops, memidx);
               const xed_reg_enum_t ir =
                  xed_decoded_inst_get_index_reg(mops, memidx);
               if (br != XED_REG_INVALID && ir == XED_REG_INVALID) {
                  /*
                   * Locate the ModR/M byte by skipping legacy prefixes.
                   *
                   * 0x66 (operand-size override): preserve verbatim on
                   * the rewritten main instruction — it controls reg
                   * width, not addressing, so the [base+r11*1] rewrite
                   * still applies. The lea-r11 doesn't need 0x66.
                   *
                   * 0x67 (address-size override): in i386 this switches
                   * addressing to addr16:disp16, but the parser only
                   * captures memdisp when memory_displacement_width == 4,
                   * so 0x67 should not reach here. Bail loudly if it
                   * does — the rewrite math below assumes a 4-byte disp.
                   */
                  bool have_66 = false;
                  std::size_t p = 0;
                  while (p < instbuf.size()) {
                     const uint8_t b = instbuf.at(p);
                     if (b == 0x66) {
                        have_66 = true;
                        ++p;
                        continue;
                     }
                     if (b == 0x67) {
                        throw error("%s: [base+disp32] with addr-size "
                                    "prefix at vmaddr 0x%zx", __FUNCTION__,
                                    this->loc.vmaddr);
                     }
                     if (b == 0xF0 || b == 0xF2 || b == 0xF3 || b == 0x2E ||
                         b == 0x36 || b == 0x3E || b == 0x26 || b == 0x64 ||
                         b == 0x65) {
                        ++p;
                        continue;
                     }
                     break;
                  }
                  const std::size_t opcode_start = p;
                  bool is_3byte_opcode = false;
                  if (instbuf.at(p) == 0x0F) {
                     ++p;
                     if (instbuf.at(p) == 0x38 || instbuf.at(p) == 0x3A) {
                        /*
                         * 3-byte opcode (SSE3/SSSE3/SSE4). Skip the
                         * explicit lea+r11 rewrite — it'd require
                         * preserving 3-byte opcode in `buf` and the
                         * existing math assumes 1-byte opcode after
                         * prefixes. Fall through to the M32 default
                         * rule (line ~1331): copy ctor reuses the
                         * bytes (`[base+disp32]` decodes the same way
                         * in x86_64 memory addressing) and resolves
                         * memdisp absolute. Slide-correctness isn't
                         * an issue since the wrapper disables ASLR.
                         */
                        is_3byte_opcode = true;
                     }
                  }
                  if (is_3byte_opcode) {
                     /* exit the [base+disp32] rewrite without returning
                      * — control falls through to the default rule. */
                     goto base_disp32_skip;
                  }
                  ++p;                                   /* p -> ModR/M */
                  const std::size_t modrm_idx = p;
                  const uint8_t modrm = instbuf.at(modrm_idx);
                  const uint8_t reg = (modrm >> 3) & 0x7;
                  const uint8_t rm  = modrm & 0x7;
                  const bool has_sib = (rm == 0x4);
                  const uint8_t base = has_sib
                     ? (uint8_t)(instbuf.at(modrm_idx + 1) & 0x7)
                     : rm;
                  /* trailing immediate bytes, e.g. mov [base+disp32], imm32 */
                  const std::size_t after =
                     modrm_idx + 1 + (has_sib ? 1u : 0u) + 4;
                  const std::size_t imm_bytes =
                     instbuf.size() > after ? instbuf.size() - after : 0;

                  /* lea r11, [rip+disp32] — table base, slide-correct */
                  auto* lea_inst = new Instruction<opposite<bits>>
                     (opcode::lea_r11_mem_rip_disp32());
                  lea_inst->memidx = 0;
                  env.resolve(memdisp, &lea_inst->memdisp);

                  /* rebuild the operation with [base + r11*1] addressing */
                  opcode_t buf;
                  for (std::size_t j = opcode_start; j < modrm_idx; ++j) {
                     buf.push_back(instbuf.at(j));
                  }
                  const uint8_t new_sib = 0x18 | base;   /* scale 1, idx r11 */
                  if (base == 0x5) {
                     /* SIB base=101 has no base meaning at mod=00 — use
                      * mod=01 with an explicit zero disp8 instead. */
                     buf.push_back((uint8_t)(0x40 | (reg << 3) | 0x04));
                     buf.push_back(new_sib);
                     buf.push_back(0x00);
                  } else {
                     buf.push_back((uint8_t)((reg << 3) | 0x04));
                     buf.push_back(new_sib);
                  }
                  for (std::size_t j = instbuf.size() - imm_bytes;
                       j < instbuf.size(); ++j) {
                     buf.push_back(instbuf.at(j));
                  }
                  /* REX.X (0x42) — index field's high bit. Must come
                   * AFTER any legacy prefix (0x66) per x86_64 ISA. */
                  buf.insert(buf.begin(), 0x42);
                  if (have_66) {
                     buf.insert(buf.begin(), 0x66);
                  }
                  auto* main_inst = new Instruction<opposite<bits>>(buf);
                  return {lea_inst, main_inst};
               }
            }

         base_disp32_skip:
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
            /* DBG_SMALLIMM: post-do_resolve diagnostic for the movb $imm8,[abs32] bug.
             * At Transform time do_resolve() has already run, so this->memdisp
             * reflects the ACTUAL resolved value (vs. deferred-null at parse time).
             * Also prints the trailing imm8 value and resolved NEW (x64 build) vmaddr
             * when available (set during Build phase — may be 0 if Build not yet run). */
            if (std::getenv("DBG_SMALLIMM") && memdisp_absolute && !imm) {
               /* trailing imm8: last byte of instbuf (true for c6 05 movb form) */
               const uint8_t trail_imm8 =
                  instbuf.empty() ? 0 : instbuf.back();
               std::fprintf(stderr,
                  "[smallimm-transform] i386_vmaddr=0x%zx trail_imm8=0x%02x memdisp=%p",
                  (std::size_t)this->loc.vmaddr,
                  (unsigned)trail_imm8,
                  (const void *)this->memdisp);
               if (this->memdisp && this->memdisp->section) {
                  /* loc.vmaddr at transform time = i386 (old) vmaddr of the blob.
                   * The x64 (new) vmaddr is assigned during Build, which runs after
                   * Transform, so it is typically still 0 here. We print both. */
                  std::fprintf(stderr,
                     " resolved_sect=%.16s resolved_seg=%.16s"
                     " resolved_old_vmaddr=0x%zx resolved_new_vmaddr=0x%zx",
                     this->memdisp->section->sect.sectname,
                     (this->memdisp->section->segment
                        ? this->memdisp->section->segment->segment_command.segname
                        : "(null)"),
                     (std::size_t)this->memdisp->loc.vmaddr,
                     (std::size_t)this->memdisp->loc.vmaddr);
               } else if (this->memdisp) {
                  std::fprintf(stderr,
                     " (section==nullptr) resolved_vmaddr=0x%zx",
                     (std::size_t)this->memdisp->loc.vmaddr);
               } else {
                  std::fprintf(stderr, " (still null after do_resolve)");
               }
               std::fprintf(stderr, "\n");
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
      imm(nullptr), brdisp(nullptr), dbg_orig_md(other.dbg_orig_md)
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
      
      if (other.brdisp) {
         env.resolve(other.brdisp, &brdisp);
      }

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
            this->brdisp = nullptr;
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
