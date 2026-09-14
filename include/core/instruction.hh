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
      std::size_t dbg_orig_md = 0;                /*!< DBG: original i386 abs disp32 */

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

      void decode();
      static void decode(xed_decoded_inst_t& xedd, const opcode_t& instbuf);
      // void patch_disp(ssize_t disp);
      // void patch_relbr(xed_decoded_inst_t& xedd, opcode_t& instbuf, ssize_t disp) const;

      void parse_handle_relbr();
   };   

}
