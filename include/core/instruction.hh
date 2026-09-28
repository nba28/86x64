#pragma once

#include <vector>

extern "C" {
#include <xed/xed-interface.h>
}

#include "types.hh"
#include "section_blob.hh"
#include "opcodes.hh"

namespace MachO {

   template <Bits bits>
   class Instruction: public SectionBlob<bits> {
   public:
      static const xed_state_t& dstate();
      
      std::vector<uint8_t> instbuf;
      xed_decoded_inst_t xedd;
      unsigned memidx;
      const SectionBlob<bits> *memdisp = nullptr; /*!< memory displacement pointee */
      bool memdisp_absolute = false;              /*!< true for `[disp32+idx*N]`,
                                                      false for rip-relative */
      std::size_t memdisp_offset = 0;             /*!< intra-blob byte offset when
                                                      memdisp came from a
                                                      containing-blob fallback
                                                      (mid-blob data store) */
      bool pic_anchored = false;                  /*!< true if memdisp came from
                                                      PIC anchor tracking
                                                      (target = anchor + disp).
                                                      Transform emits rip-rel,
                                                      bypassing base register. */
      bool pic_anchor_in_index = false;           /*!< SIB form whose ANCHOR is
                                                      the INDEX register, not the
                                                      base (`lea [live+anchor+
                                                      disp32]`, scale 1). The
                                                      transform must then keep the
                                                      BASE as the live index and
                                                      put the resolved target in
                                                      the SIB base. */
      Immediate<bits> *imm = nullptr;
      const SectionBlob<bits> *brdisp = nullptr;  /*!< branch displacement pointee */

      RelocBlob<bits> *reloc = nullptr; /*!< relocation pointee (owned) */
      
      virtual std::size_t size() const override { return instbuf.size(); }
      virtual void Emit(Image& img, std::size_t offset) const override;
      virtual void Build(BuildEnv<bits>& env) override;
      
      static Instruction<bits> *Parse(const Image& img, const Location& loc, ParseEnv<bits>& env,
                                      bool add_to_map = true) {
         return new Instruction(img, loc, env, add_to_map);
      }

      /*
       * Non-throwing pre-flight: returns true if the bytes at `loc` decode as
       * a valid instruction in our mode. Callers (currently Section's text
       * parser) use this to detect jump tables / data interleaved with code,
       * which are common in old release-build binaries that lack
       * LC_FUNCTION_STARTS / LC_DATA_IN_CODE metadata. Doing this *before*
       * constructing the Instruction avoids the SectionBlob ctor's resolver
       * registration, which would otherwise strand a half-built blob at
       * `loc.vmaddr` and trip later asserts.
       */
      static bool CanDecode(const Image& img, const Location& loc);
      
      template <typename It>
      Instruction(It begin, It end):
         SectionBlob<bits>(), instbuf(begin, end), memdisp(nullptr), brdisp(nullptr) {
         decode();
      }

      Instruction(const opcode_t& opcode): SectionBlob<bits>(), instbuf(opcode) {
         decode();
      }

      /* sometimes needs to return multiple instructions */
      virtual typename SectionBlob<bits>::SectionBlobs Transform(TransformEnv<bits>& env)
         const override;

      virtual Instruction<opposite<bits>> *Transform_one(TransformEnv<bits>& env) const override {
         return new Instruction<opposite<bits>>(*this, env);
      }

   private:
      Instruction(const Image& img, const Location& loc, ParseEnv<bits>& env, bool add_to_map);
      Instruction(const Instruction<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);
      template <Bits> friend class Instruction;

      /* An i386 memory operand re-encoded for x86_64 (see lower_mem). */
      struct LoweredMem {
         typename SectionBlob<bits>::SectionBlobs pre; /*!< runs first (sets r11) */
         opcode_t prefixes;   /*!< the source's legacy prefixes */
         opcode_t opcode;     /*!< the source's opcode bytes */
         uint8_t reg = 0;     /*!< the source's ModR/M reg field */
         opcode_t operand;    /*!< ModR/M (reg=0) [SIB] [disp] */
         opcode_t trailing;   /*!< the source's immediate bytes */
         uint8_t rex = 0;     /*!< REX.X/REX.B the operand needs */
         bool addr32 = false;
         enum { NONE, RIP, ABS } bind = NONE;
      };
      bool lower_mem(TransformEnv<bits>& env, LoweredMem& out) const;
      typename SectionBlob<bits>::SectionBlobs
      emit_mem(TransformEnv<bits>& env, const LoweredMem& m, const opcode_t& opcode,
               uint8_t reg, const opcode_t& trailing) const;

      void decode();
      static void decode(xed_decoded_inst_t& xedd, const opcode_t& instbuf);

      void parse_handle_relbr();
   };   

}
