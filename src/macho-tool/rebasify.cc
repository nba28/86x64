#include <iostream>
#include <list>
#include <ctime>
#include <cstdio>

#include "rebasify.hh"
#include "core/macho.hh"
#include "core/archive.hh"
#include "core/opcodes.hh"
#include "core/section_blob.hh"
#include "core/instruction.hh"
#include "core/rebase_info.hh"
#include "core/dyldinfo.hh"

int Rebasify::opthandler(int optchar) {
   switch (optchar) {
   case 'h':
      usage(std::cout);
      return 0;

   case 'v':
      verbose = true;
      return 1;
      
   default: abort();
   }
}

void Rebasify::state_info::reset() {
   state = 0;
   vmaddr = 0;
   live_regs.clear();
   frame_index = std::nullopt;
   // text_it = section->content.begin();
}

int Rebasify::work() {
   MachO::MachO *macho = MachO::MachO::Parse(*in_img);
   auto archive32 = dynamic_cast<MachO::Archive<MachO::Bits::M32> *>(macho);
   if (archive32 == nullptr) {
      log("input Mach-O not a 32-bit archive");
      return -1;
   }

   /* ALGORITHM
    * - Pattern-match for 'call 0 \ pop eax'
    * - Remove pattern.
    * - Find all subsequent uses of %eax (before any conditional branches?)
    * ...
    */
   // TODO
   auto text = archive32->section(SECT_TEXT);
   if (text == nullptr) {
      log("missing text section");
      return -1;
   }

   const size_t thunks_seeded = handle_insts(archive32);

   /*
    * Fast path: when no PIC thunks were found, rebasify has nothing to
    * rewrite — its purpose is to find `call 0; pop reg` sites, rewrite
    * them with explicit `mov reg, imm32` plus REBASE_TYPE_TEXT_ABSOLUTE32
    * entries. With zero thunks, the output is semantically identical to
    * the input; the only difference from Build/Emit would be cosmetic
    * file-layout reshuffling (alignment padding, etc.). Skipping
    * Build/Emit and copying bytes directly avoids the long tail of
    * Build/Emit asserts a 17 MB binary like iPhoto trips on (LOOP
    * widening, prefixed Jcc, DylibCommand variants...). When real thunks
    * exist (e.g. binaries that actually rely on PIC), we fall through to
    * the regular Build/Emit path.
    */
   if (thunks_seeded == 0) {
      const size_t sz = in_img->size();
      out_img->copy(0, &in_img->at<uint8_t>(0), sz);
      fprintf(stderr,
              "rebasify: no thunks found — copied %zu bytes input->output, skipping Build/Emit\n",
              sz);
      return 0;
   }

   fprintf(stderr, "rebasify: Build start\n");
   try {
      const std::size_t total = archive32->Build(0);
      fprintf(stderr, "rebasify: Build done, total_size=%zu\n", total);
      archive32->Emit(*out_img);
      fprintf(stderr, "rebasify: Emit done, out_img size=%zu\n", out_img->size());
   } catch (const std::exception& e) {
      fprintf(stderr, "rebasify: BUILD/EMIT THREW (%s): %s\n",
              typeid(e).name(), e.what());
      return -1;
   } catch (...) {
      fprintf(stderr, "rebasify: BUILD/EMIT THREW unknown exception\n");
      return -1;
   }
   return 0;
}

#define MULTIPASS 1

/*
 * Cap on distinct states recorded per vmaddr. The multipass analysis only
 * skips a revisit when the EXACT state was seen before, so a hot vmaddr in
 * a large binary can accumulate an unbounded state list — iPhoto's 17 MB
 * __text drove this to multi-GB RSS without terminating. Once a vmaddr has
 * this many distinct states, further arrivals are treated as visited. Small
 * binaries (photocd, dbRepair) never approach the cap, so their analysis is
 * bit-for-bit unchanged.
 */
#define REBASIFY_STATE_CAP 4

Rebasify::decode_info::decode_info(const xed_decoded_inst_t& xedd,
                                   typename MachO::Section<MachO::Bits::M32>::Content::iterator it):
   text_it(it),
   reg0(xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0)),
   reg1(xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG1)),
   iform(xed_decoded_inst_get_iform_enum(&xedd)),
   iclass(xed_decoded_inst_get_iclass(&xedd)),
   nmemops(xed_decoded_inst_number_of_memory_operands(&xedd)),
   base_reg(xed_decoded_inst_get_base_reg(&xedd, 0)),
   memdisp(xed_decoded_inst_get_memory_displacement(&xedd, 0)) {}

size_t Rebasify::handle_insts(MachO::Archive<MachO::Bits::M32> *archive) const {
   std::list<state_info> states;
#if MULTIPASS == 0
   std::unordered_set<size_t> visited_vmaddrs;
#else
   std::unordered_map<size_t, std::list<state_info>> visited_vmaddrs; /* also captures state */
#endif

   /*
    * Seed the worklist with one starting state per PIC thunk site instead
    * of one state at __text begin walking everything in state-0. The
    * pre-thunk state-0 walk did literally nothing useful (the switch in
    * handle_inst case 0 returns immediately) but ate ~99% of analysis time
    * — for iPhoto's 13 MB __text that was hours of trivial worklist churn.
    *
    * A thunk is `call $+5` (e8 00 00 00 00) followed immediately by a
    * `pop reg`; after the pop the popped register holds the PIC base.
    * We start each seed at state=2 with that register live, pointed at
    * the instruction AFTER the pop. The state machine then explores
    * forward exactly as before, finds the function's address-computation
    * sites, and emits the rebase rewrites. handle_inst still recognises
    * call_0 inside the explored body so nested thunks (rare) still work.
    */
   const MachO::opcode_t call_0({0xe8, 0x00, 0x00, 0x00, 0x00});
   auto *text_section = archive->section(SECT_TEXT);
   size_t thunks_seeded = 0;
   if (text_section != nullptr) {
      for (auto it = text_section->content.begin();
           it != text_section->content.end(); ++it) {
         auto *inst = dynamic_cast<MachO::Instruction<MachO::Bits::M32> *>(*it);
         if (!inst || inst->instbuf != call_0) { continue; }
         auto next_it = std::next(it);
         if (next_it == text_section->content.end()) { continue; }
         auto *pop_inst =
            dynamic_cast<MachO::Instruction<MachO::Bits::M32> *>(*next_it);
         if (!pop_inst) { continue; }
         if (xed_decoded_inst_get_iform_enum(&pop_inst->xedd) !=
             XED_IFORM_POP_GPRv_58) { continue; }
         xed_reg_enum_t reg =
            xed_decoded_inst_get_reg(&pop_inst->xedd, XED_OPERAND_REG0);

         state_info s(archive);
         s.text_it = std::next(next_it);
         s.state = 2;
         s.vmaddr = pop_inst->loc.vmaddr;
         s.live_regs.insert(reg);
         states.push_back(s);
         ++thunks_seeded;
      }
   }
   fprintf(stderr, "rebasify: seeded %zu PIC thunk site(s)\n", thunks_seeded);

   /*
    * Progress logging: print every 1M worklist iterations on stderr so
    * long runs are not silently grinding. With the per-thunk seeding above
    * total iterations on iPhoto drop ~2 orders of magnitude.
    */
   const size_t kProgressEvery = 1000000;
   size_t iter_count = 0;
   const time_t start_time = time(nullptr);

   while (!states.empty()) {
      ++iter_count;
      if ((iter_count % kProgressEvery) == 0) {
         std::size_t cur_vmaddr = 0;
         if (!states.empty() && states.front().text_it !=
             states.front().section->content.end()) {
            cur_vmaddr = (**states.front().text_it).loc.vmaddr;
         }
         fprintf(stderr,
                 "rebasify: iter=%zuM elapsed=%lds worklist=%zu visited=%zu vmaddr=0x%zx\n",
                 iter_count / 1000000,
                 (long)(time(nullptr) - start_time),
                 states.size(), visited_vmaddrs.size(), cur_vmaddr);
      }
      state_info state = states.front();
      states.pop_front();

      /* check if state has already been visited or has reached end of section */
#if MULTIPASS == 0
      if (state.text_it == state.section->content.end() ||
          visited_vmaddrs.find((**state.text_it).loc.vmaddr) != visited_vmaddrs.end()) {
         continue;
      }
#else
      if (state.text_it == state.section->content.end()) {
         continue;
      }
      const auto visited_it = visited_vmaddrs.find((**state.text_it).loc.vmaddr);
      if (visited_it != visited_vmaddrs.end()) {
         auto& visited_states = visited_it->second;
         if (visited_states.size() >= REBASIFY_STATE_CAP ||
             std::find(visited_states.begin(), visited_states.end(), state)
                != visited_states.end())
            {
               continue;
            }
      }
#endif

      if (verbose) {
         fprintf(stderr, "[REBASIFY] 0x%zx\n", (**state.text_it).loc.vmaddr);
      }

      MachO::Instruction<MachO::Bits::M32> *inst =
         dynamic_cast<MachO::Instruction<MachO::Bits::M32> *>(*state.text_it);
      
      if (inst) {
         /* add to visited list */
         visited_vmaddrs[inst->loc.vmaddr].push_back(state);
         
         decode_info decode(inst->xedd, state.text_it);
         
         switch (decode.iclass) {
         case XED_ICLASS_JB: 
         case XED_ICLASS_JBE: 
         case XED_ICLASS_JL: 
         case XED_ICLASS_JLE: 
         case XED_ICLASS_JNB: 
         case XED_ICLASS_JNBE: 
         case XED_ICLASS_JNL: 
         case XED_ICLASS_JNLE: 
         case XED_ICLASS_JNO: 
         case XED_ICLASS_JNP: 
         case XED_ICLASS_JNS: 
         case XED_ICLASS_JNZ: 
         case XED_ICLASS_JO: 
         case XED_ICLASS_JP: 
         case XED_ICLASS_JS: 
         case XED_ICLASS_JZ:
            /* split into two states */
            states.emplace_front(state, inst->brdisp);
            ++state.text_it;
            break;

         case XED_ICLASS_JMP:
            {
               /* add next instruction to be processed last in case it's not reachable thru static
                * analysis */
               state_info state2 = state;
               ++state2.text_it;
               states.push_back(state2);

               /* move state */
               state = state_info(state, inst->brdisp);
            }
            break;

         default:
            handle_inst(inst, state, decode);
            ++state.text_it;
            break;
         }

      } else {
         ++state.text_it;
      }

      states.push_front(state);
   }
   return thunks_seeded;
}

int Rebasify::handle_inst(MachO::Instruction<MachO::Bits::M32> *inst, state_info& state,
                           const decode_info& info) const {
   const MachO::opcode_t call_0({0xe8, 0x00, 0x00, 0x00, 0x00});
   // const MachO::opcode_t pop({0x58});

   if (inst->instbuf == call_0) {
      if (verbose) {
         fprintf(stderr, "[REBASIFY] 0x%zx call 0x0\n", inst->loc.vmaddr);
      }
      state.state = 1;
      return 0;
   }
   
   switch (state.state) {
   case 0: /* init */
      return 0;
      
   case 1: /* seen call */
      if (info.iform == XED_IFORM_POP_GPRv_58) {
         state.state = 2;
         xed_reg_enum_t live_reg = xed_decoded_inst_get_reg(&inst->xedd, XED_OPERAND_REG0);
         state.live_regs.insert(live_reg);
         state.vmaddr = inst->loc.vmaddr;
         if (verbose) {
            fprintf(stderr, "[REBASIFY] 0x%zx thunk assigned to register %s\n",
                    inst->loc.vmaddr, xed_reg_enum_t2str(live_reg));
         }
      }
      return 0;

   case 2:
      if (info.iclass == XED_ICLASS_CALL_NEAR) {
         for (auto live_reg_it = state.live_regs.begin(); live_reg_it != state.live_regs.end(); ) {
            /* check if it's a preserved register */
            switch (*live_reg_it) {
            case XED_REG_EAX:
            case XED_REG_ECX:
            case XED_REG_EDX:
               if (verbose) {
                  fprintf(stderr, "[REBASIFY] 0x%zx register %s destroyed by call\n",
                          inst->loc.vmaddr, xed_reg_enum_t2str(*live_reg_it));
               }
               live_reg_it = state.live_regs.erase(live_reg_it);
               break;
            case XED_REG_EBX:
            case XED_REG_EDI:
            case XED_REG_ESI:
               ++live_reg_it;
               break;
            default: abort();
            }
         }
      } else {
         /* look for frame store */
         if ((info.iform == XED_IFORM_MOV_MEMv_OrAX || info.iform == XED_IFORM_MOV_MEMv_GPRv) &&
             info.base_reg == XED_REG_EBP) {
            for (xed_reg_enum_t live_reg : state.live_regs) {
               if (live_reg == info.reg0) {
                  /* frame store */
                  state.frame_index = info.memdisp;
                  if (verbose) {
                     fprintf(stderr, "[REBASIFY] 0x%zx frame store from %s to index %d\n",
                             inst->loc.vmaddr, xed_reg_enum_t2str(live_reg), *state.frame_index);
                  }
                  return 0;
               }
            }

            if (state.frame_index && info.memdisp == *state.frame_index) {
               if (verbose) {
                  fprintf(stderr, "[REBASIFY] 0x%zx overwrote frame store at index %d\n",
                          inst->loc.vmaddr, *state.frame_index);
               }
               state.frame_index = std::nullopt;
               return 0;
            }
         }

         /* look for frame load */
         if ((info.iform == XED_IFORM_MOV_GPRv_MEMv || info.iform == XED_IFORM_MOV_OrAX_MEMv) &&
             info.base_reg == XED_REG_EBP && info.memdisp == state.frame_index) {
            state.live_regs.insert(info.reg0);
            if (verbose) {
               fprintf(stderr, "[REBASIFY] 0x%zx frame load from index %d to %s\n",
                       inst->loc.vmaddr, *state.frame_index, xed_reg_enum_t2str(info.reg0));
            }
            return 0;
         }
         
         /* look for alias */
         if (info.iform == XED_IFORM_MOV_GPRv_GPRv_89 &&
             state.live_regs.find(info.reg1) != state.live_regs.end()) {
            state.live_regs.insert(info.reg0);
            if (verbose) {
               fprintf(stderr, "[REBASIFY] 0x%zx alias %s\n", inst->loc.vmaddr,
                       xed_reg_enum_t2str(info.reg0));
            }
            return 0;
         }
         
         /* otherwise handle inst thunk */
         handle_inst_thunk(inst, state, info);
      }
      return 0;

   default: abort();
   }
}

int Rebasify::handle_inst_thunk(MachO::Instruction<MachO::Bits::M32> *inst, state_info& state,
                                 const decode_info& info) const {
   /* custom rules with duplicate operands  */
   for (auto live_reg_it = state.live_regs.begin(); live_reg_it != state.live_regs.end();
        ++live_reg_it) {
      if (info.reg0 == *live_reg_it && info.reg1 == *live_reg_it) {
         switch (info.iclass) {
         case XED_ICLASS_XOR:
            state.live_regs.erase(live_reg_it);
            return 0;
            
         default:
            break;
         }
      }
   }
   
   /* check if reads from or writes to eax */
   switch (xed_decoded_inst_get_iclass(&inst->xedd)) {
   case XED_ICLASS_RET_NEAR:
   case XED_ICLASS_RET_FAR:
      state.reset();
      return 0;

   default:
      {
         std::size_t target = state.vmaddr;
         if (state.live_regs.find(info.base_reg) != state.live_regs.end()) {
            target += info.memdisp;
         }

         typename state_info::LiveRegs::iterator live_reg_it;
         if ((live_reg_it = state.live_regs.find(info.base_reg)) != state.live_regs.end() ||
             (live_reg_it = state.live_regs.find(info.reg1))     != state.live_regs.end()) {
            if (verbose) {
               fprintf(stderr, "[REBASIFY] 0x%zx ref dst=0x%zx\n", inst->loc.vmaddr,
                       target);
            }
            
            /*
             * Resolve the destination blob FIRST. find_blob throws when
             * `target` lands mid-blob or outside the section — common on a
             * 17 MB binary where linear-sweep disassembly misdecodes data
             * interleaved in __text and the surrounding state-machine
             * accumulates a bogus `target`. Skip the PIC rewrite for those
             * sites; the original i386 code stays intact and the
             * transform's regular instruction-translation paths handle it.
             */
            MachO::SectionBlob<MachO::Bits::M32> *pointee = nullptr;
            try {
               pointee = state.archive->template
                  find_blob<MachO::SectionBlob>(target);
            } catch (const std::invalid_argument& e) {
               if (verbose) {
                  fprintf(stderr,
                          "[REBASIFY] 0x%zx skip PIC rewrite: %s\n",
                          inst->loc.vmaddr, e.what());
               }
               break;
            }
            if (pointee == nullptr) {
               log("unable to find destination blob in rebasify operation at vmaddr 0x%zx",
                   inst->loc.vmaddr);
               return -1;
            }

            auto mov_inst = new MachO::Instruction<MachO::Bits::M32>
               (MachO::opcode::mov_r32_imm32(*live_reg_it));
            auto mov_inst_imm = MachO::Immediate<MachO::Bits::M32>::Create(target);
            mov_inst->segment = mov_inst_imm->segment = state.segment;
            mov_inst->section = mov_inst_imm->section = state.section;
            mov_inst_imm->pointee = pointee;
                     
            mov_inst->imm = mov_inst_imm;
            mov_inst->loc.vmaddr = (*info.text_it)->loc.vmaddr;

            state.section->content.insert(info.text_it, mov_inst);
                     
            /* adjust displacement if necessary */
            if (info.base_reg == *live_reg_it) {
               const unsigned dispbits =
                  xed_decoded_inst_get_memory_displacement_width_bits(&inst->xedd, 0);
               if (dispbits != 0) {
                  const xed_enc_displacement_t encdisp = {0, dispbits};
                  if (!xed_patch_disp(&inst->xedd, inst->instbuf.data(), encdisp)) {
                     log("error while patching displacement");
                     return -1;
                  }
               }
            }
                     
            /* add to rebase info */
            auto rebase_node = MachO::RebaseNode<MachO::Bits::M32>::
               Create(REBASE_TYPE_TEXT_ABSOLUTE32);
            rebase_node->blob = mov_inst_imm;
            auto dyld_info = state.archive->template subcommand<MachO::DyldInfo>();
            if (dyld_info == nullptr) {
               log("missing dyld info");
               return -1;
            }
            auto rebase_info = dyld_info->rebase;
            rebase_info->rebasees.push_back(rebase_node);
         }
         break;
      }
   }

   /* check if eax is destroyed */
   typename state_info::LiveRegs::iterator live_reg_it;
   if ((live_reg_it = state.live_regs.find(info.reg0)) != state.live_regs.end()) {
      switch (xed_decoded_inst_get_iclass(&inst->xedd)) {
      case XED_ICLASS_MOV:
      case XED_ICLASS_XOR:
      case XED_ICLASS_LEA:
         if (verbose) {
            fprintf(stderr, "[REBASIFY] 0x%zx register %s destroyed by instruction\n",
                    inst->loc.vmaddr, xed_reg_enum_t2str(*live_reg_it));
         }
         state.live_regs.erase(live_reg_it);
         return 0;

      default:
         break;
      }
   }

   return 0;
}

Rebasify::state_info::state_info(MachO::Archive<MachO::Bits::M32> *archive):
   archive(archive), segment(archive->segment(SEG_TEXT)), section(archive->section(SECT_TEXT)),
   text_it(section->content.begin())
{
   reset();
}

Rebasify::state_info::state_info(const state_info& other,
                                 const MachO::SectionBlob<MachO::Bits::M32> *target):
   state(other.state), vmaddr(other.vmaddr), live_regs(other.live_regs),
   frame_index(other.frame_index), archive(other.archive), segment(other.segment),
   section(other.section),
   text_it(std::find(section->content.begin(), section->content.end(), target))
{
}

bool Rebasify::state_info::operator==(const Rebasify::state_info& other) const {
   return
      state == other.state &&
      vmaddr == other.vmaddr &&
      live_regs == other.live_regs &&
      frame_index == other.frame_index;
}
