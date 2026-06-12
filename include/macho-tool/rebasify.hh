#pragma once

#include <cstdint>
extern "C" {
#include <xed/xed-interface.h>
}
#include "command.hh"
#include "core/segment.hh"
#include "core/section.hh"

struct Rebasify: InOutCommand {
   struct state_info {
      /* 0 - init
       * 1 - seen 'call 0'
       * 2 - seen 'pop eax'
       * 3 - seen 'mov [ebp - idx], reg'
       */
      int state;
      std::size_t vmaddr;

      /*
       * LiveRegs was a std::unordered_set<xed_reg_enum_t>. Each state_info
       * copy (every worklist push in handle_insts) allocated a hash table,
       * which dominated cost on a 17 MB binary. The set never holds more
       * than the 8 i386 GPRs in practice (EAX..EDI), so a uint8_t bitmask
       * is correct and orders of magnitude cheaper — sizeof(state_info)
       * drops, copy is a memcpy, equality is a byte compare, iteration is
       * a bit scan. The same iterator-based API is exposed so the existing
       * insert/find/erase call sites in rebasify.cc don't change.
       */
      struct LiveRegs {
         uint8_t mask = 0;

         static int bit_of(xed_reg_enum_t r) {
            switch (r) {
               case XED_REG_EAX: return 0;
               case XED_REG_ECX: return 1;
               case XED_REG_EDX: return 2;
               case XED_REG_EBX: return 3;
               case XED_REG_ESP: return 4;
               case XED_REG_EBP: return 5;
               case XED_REG_ESI: return 6;
               case XED_REG_EDI: return 7;
               default: return -1;
            }
         }
         static xed_reg_enum_t reg_of(int bit) {
            static const xed_reg_enum_t tab[8] = {
               XED_REG_EAX, XED_REG_ECX, XED_REG_EDX, XED_REG_EBX,
               XED_REG_ESP, XED_REG_EBP, XED_REG_ESI, XED_REG_EDI
            };
            return tab[bit];
         }

         void clear() { mask = 0; }
         bool empty() const { return mask == 0; }
         void insert(xed_reg_enum_t r) {
            int b = bit_of(r);
            if (b >= 0) { mask |= uint8_t(1u << b); }
         }
         bool operator==(const LiveRegs& o) const { return mask == o.mask; }
         bool operator!=(const LiveRegs& o) const { return mask != o.mask; }

         class iterator {
         public:
            LiveRegs *owner = nullptr;
            uint8_t   bit   = 8;       /* 8 == end() */

            iterator() = default;
            iterator(LiveRegs *o, uint8_t b): owner(o), bit(b) {}

            xed_reg_enum_t operator*() const { return reg_of(bit); }
            iterator& operator++() {
               ++bit;
               while (bit < 8 && !(owner->mask & uint8_t(1u << bit))) { ++bit; }
               return *this;
            }
            bool operator==(const iterator& o) const { return bit == o.bit; }
            bool operator!=(const iterator& o) const { return bit != o.bit; }
         };

         iterator begin() {
            uint8_t b = 0;
            while (b < 8 && !(mask & uint8_t(1u << b))) { ++b; }
            return iterator(this, b);
         }
         iterator end() { return iterator(this, 8); }

         iterator find(xed_reg_enum_t r) {
            int b = bit_of(r);
            if (b < 0 || !(mask & uint8_t(1u << b))) { return end(); }
            return iterator(this, uint8_t(b));
         }

         iterator erase(iterator it) {
            mask &= ~uint8_t(1u << it.bit);
            uint8_t b = uint8_t(it.bit + 1);
            while (b < 8 && !(mask & uint8_t(1u << b))) { ++b; }
            return iterator(this, b);
         }
      };

      LiveRegs live_regs;
      std::optional<int> frame_index;

      MachO::Archive<MachO::Bits::M32> *archive = nullptr;
      MachO::Segment<MachO::Bits::M32> *segment = nullptr;
      MachO::Section<MachO::Bits::M32> *section = nullptr;
      typename MachO::Section<MachO::Bits::M32>::Content::iterator text_it;
      
      void reset();
      state_info(MachO::Archive<MachO::Bits::M32> *archive);
      state_info(const state_info& other, const MachO::SectionBlob<MachO::Bits::M32> *target);

      bool operator==(const state_info& other) const;
   };

   struct decode_info {
      typename MachO::Section<MachO::Bits::M32>::Content::iterator text_it;
      xed_reg_enum_t reg0, reg1;
      xed_iform_enum_t iform;
      xed_iclass_enum_t iclass;
      unsigned nmemops;
      xed_reg_enum_t base_reg;
      int64_t memdisp;

      decode_info(const xed_decoded_inst_t& xedd,
                  typename MachO::Section<MachO::Bits::M32>::Content::iterator text_it);
   };

   bool verbose = false;

   virtual const char *optstring() const override { return "hv"; }
   virtual std::vector<option> longopts() const override {
      return {{"help", no_argument, nullptr, 'h'},
              {"verbose", no_argument, nullptr, 'v'},
              {0}};
   }
   virtual int opthandler(int optchar) override;
   virtual std::string optusage() const override { return "[-hv]"; }
   virtual int work() override;
   Rebasify(): InOutCommand("rebasify") {}

   /* Returns the number of PIC thunk sites seeded into the worklist. */
   size_t handle_insts(MachO::Archive<MachO::Bits::M32> *archive) const;
   int handle_inst(MachO::Instruction<MachO::Bits::M32> *inst, state_info& state,
                   const decode_info& info) const;
   int handle_inst_thunk(MachO::Instruction<MachO::Bits::M32> *inst, state_info& state,
                         const decode_info& info) const;
};
