extern "C" {
#include <cassert>
#include <xed/xed-interface.h>
}

#include "stub_helper.hh"
#include "instruction.hh"
#include "dyldinfo.hh"
#include "transform.hh"
#include "build.hh"
#include "parse.hh"
#include "xed-util.hh"

namespace MachO {

   template <Bits bits>
   StubHelperBlob<bits>::StubHelperBlob(const Image& img, const Location& loc_,
                                        ParseEnv<bits>& env): SectionBlob<bits>(loc_, env, false) {
      /* verify signatures match */
      assert(can_parse(img, loc_, env));

      Location loc = loc_;

      /* parse 'push imm32' */
      push_inst = Instruction<bits>::Parse(img, loc, env);
      loc += push_inst->size();
      jmp_inst = Instruction<bits>::Parse(img, loc, env);

      /* resolve bindee */
      env.lazy_bind_node_resolver.resolve(push_inst->imm->value, &bindee);
   }

   template <Bits bits>
   bool StubHelperBlob<bits>::can_parse(const Image& img, const Location& loc_, ParseEnv<bits>& env)
   {
      Location loc = loc_;
      xed_decoded_inst_t push_xedd, jmp_xedd;
      if (!xed::decode<bits>(img, loc.offset, push_xedd) ||
          xed_decoded_inst_get_iform_enum(&push_xedd) != XED_IFORM_PUSH_IMMz) {
         return false;
      }
      loc += xed_decoded_inst_get_length(&push_xedd);

      if (!xed::decode<bits>(img, loc.offset, jmp_xedd)) {
         return false;
      }
      const auto jmp_iform = xed_decoded_inst_get_iform_enum(&jmp_xedd);
      if (jmp_iform != XED_IFORM_JMP_RELBRz && jmp_iform != XED_IFORM_JMP_RELBRd) {
         return false;
      }
      
      return true;
   }
   

   template <Bits bits>
   void StubHelperBlob<bits>::Emit(Image& img, std::size_t offset) const {
      /*
       * Guard against a null bindee. The lazy_bind_node_resolver
       * matches the push imm32 (a lazy-bind-info offset) to a
       * LazyBindNode parsed from the lazy_bind blob; if the imm
       * doesn't correspond to any node — possible when the binary's
       * stub_helper has stubs past the end of the lazy_bind blob, or
       * when the parser's pointer-shape heuristic mis-marked the imm
       * with a pointee that overwrote it — bindee stays null. With
       * the original `push_inst->imm->value = bindee->index;` line
       * that null-derefs SIGSEGVs mid-Emit, leaves the output file
       * truncated, and breaks every segment header past this point.
       * Fall back to the original imm value (whatever the parser set)
       * with a warning so we can spot it in the log.
       */
      if (bindee == nullptr) {
         fprintf(stderr,
                 "warning: StubHelperBlob::Emit at vmaddr 0x%zx: "
                 "bindee is null (lazy_bind_index %u unresolved); "
                 "preserving original push imm value 0x%x\n",
                 (size_t)this->loc.vmaddr,
                 (unsigned)(push_inst->imm ? push_inst->imm->value : 0),
                 push_inst->imm ? push_inst->imm->value : 0);
      } else {
         /* adjust push immediate */
         push_inst->imm->value = bindee->index;
      }
      /*
       * Clear any pointee the parser might have wrongly attached. The
       * push imm32 here is the lazy-bind-info offset (a literal), but
       * the Instruction parser's pointer-shape heuristic can mis-mark
       * it when the offset value happens to fall in a real segment's
       * vmaddr range (common when lazy_bind is tens of KB). Without
       * this, Immediate::Emit prefers pointee->loc.vmaddr over the
       * `value = bindee->index` we just set, and the dyld stub binder
       * receives a bogus offset.
       */
      if (push_inst->imm) push_inst->imm->pointee = nullptr;

      /* emit insturctions */
      push_inst->Emit(img, offset);
      offset += push_inst->size();
      jmp_inst->Emit(img, offset);
      offset += jmp_inst->size();
   }

   template <Bits bits>
   void StubHelperBlob<bits>::Build(BuildEnv<bits>& env) {
      // SectionBlob<bits>::Build(env); -- this does allocation!
      push_inst->Build(env);
      jmp_inst->Build(env);
   }

   template <Bits bits>
   std::size_t StubHelperBlob<bits>::size() const {
      return push_inst->size() + jmp_inst->size();
   }

   template <Bits bits>
   StubHelperBlob<bits>::StubHelperBlob(const StubHelperBlob<opposite<bits>>& other,
                                        TransformEnv<opposite<bits>>& env):
      SectionBlob<bits>(other, env), push_inst(other.push_inst->Transform_one(env)),
      jmp_inst(other.jmp_inst->Transform_one(env))
   {
      env.template resolve<LazyBindNode>(other.bindee, &bindee);
   }

   template class StubHelperBlob<Bits::M32>;
   template class StubHelperBlob<Bits::M64>;

}
