#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include "parse.hh"
#include "section_blob.hh"
#include "archive.hh"
#include "segment.hh"
#include "instruction.hh"

namespace MachO {


   template <Bits bits>
   Placeholder<bits> *ParseEnv<bits>::add_placeholder(std::size_t vmaddr) {
      if (vmaddr == 0) {
         return nullptr;
      }

      /* A reference that lands at the base vmaddr of the HEADER-bearing segment
       * (the one mapped at file offset 0 — __TEXT) points at the mach_header
       * itself, which has no parsed blob (Archive::Emit produces it directly).
       * Seen for dylibs: rebase entries / __mh_dylib_header / data slots holding
       * the dylib's own load address. Don't create a placeholder — the
       * originating Immediate keeps pointee=nullptr and emits raw M32 bytes;
       * dyld slides the slot on load.
       *
       * IMPORTANT: this must NOT fire for the base of OTHER segments (__DATA,
       * __OBJC, ...). Their first byte is a genuine datum — e.g. an
       * __OBJC,__cls_refs[0] sitting exactly at the segment base. Dropping its
       * placeholder leaves any PIC-anchored load of that datum with a stale
       * i386 displacement (the load reads into the wrong section at runtime —
       * the i386 ObjC class-ref-via-get_pc_thunk bug). So gate on fileoff==0. */
      bool in_any_segment = false;
      for (Segment<bits> *seg : archive.segments()) {
         if (strcmp(seg->segment_command.segname, "__PAGEZERO") == 0) {
            continue;  /* no real content */
         }
         if (seg->segment_command.vmaddr == vmaddr &&
             seg->segment_command.fileoff == 0 &&
             seg->segment_command.filesize > 0) {
            return nullptr;  /* mach_header self-reference */
         }
         /* Check vmaddr is within ANY (non-__PAGEZERO) segment's vmaddr range. */
         if (vmaddr >= seg->segment_command.vmaddr &&
             vmaddr <  seg->segment_command.vmaddr + seg->segment_command.vmsize) {
            in_any_segment = true;
         }
      }

      /* Skip placeholders for vmaddrs that aren't in any real segment. These
       * commonly arise when XED disassembles "instruction" bytes that are
       * actually embedded data (jump tables, constants between functions); the
       * decoded memdisp produces a garbage targetaddr like 0xffffffffba049296
       * (sign-extended-negative junk) or 0x1001290 (inside __PAGEZERO). Was:
       * created placeholders for these, then archive.cc threw "not all
       * placeholders could be placed" downstream because no blob ever showed
       * up at that vmaddr. Now: silently drop — the originating Immediate
       * keeps its raw value and emits verbatim. If the value happens to be
       * an actual valid bind/rebase target after the M32→M64 shift, dyld
       * will catch it via the bind/rebase tables, not via parse-time blob
       * resolution. */
      if (!in_any_segment) {
         return nullptr;
      }

      auto it = placeholders.find(vmaddr);
      if (it == placeholders.end()) {
         Placeholder<bits> *placeholder = Placeholder<bits>::Parse(Location(0, vmaddr), *this);
         placeholders.insert({vmaddr, placeholder});
         return placeholder;
      } else {
         return it->second;
      }
   }

   template <Bits bits>
   void ParseEnv<bits>::do_resolve() {
      offset_resolver.do_resolve();
      vmaddr_resolver.do_resolve();
   }

   template <Bits bits>
   bool ParseEnv<bits>::vmaddr_in_image(std::size_t vmaddr) const {
      for (Segment<bits> *seg : archive.segments()) {
         const char *sn = seg->segment_command.segname;
         const std::size_t snsz = sizeof(seg->segment_command.segname);
         if (std::strncmp(sn, SEG_PAGEZERO, snsz) == 0) { continue; }
         if (std::strncmp(sn, SEG_LINKEDIT, snsz) == 0) { continue; }
         if (seg->contains_vmaddr(vmaddr)) { return true; }
      }
      return false;
   }

   template <Bits bits>
   const Section<bits> *ParseEnv<bits>::section_at(std::size_t vmaddr) const {
      for (Segment<bits> *seg : archive.segments()) {
         for (Section<bits> *sec : seg->sections) {
            if (sec->contains_vmaddr(vmaddr)) { return sec; }
         }
      }
      return nullptr;
   }

   template <Bits bits>
   bool ParseEnv<bits>::fixed_load_image() const {
      return archive.header.filetype == MH_EXECUTE &&
             (archive.header.flags & MH_PIE) == 0;
   }

   template <Bits bits>
   bool ParseEnv<bits>::vmaddr_in_writable_data(std::size_t vmaddr) const {
      for (Segment<bits> *seg : archive.segments()) {
         if ((seg->segment_command.initprot & VM_PROT_WRITE) == 0) { continue; }
         if (std::strncmp(seg->segment_command.segname, SEG_OBJC,
                          sizeof(seg->segment_command.segname)) == 0) { continue; }
         if (seg->contains_vmaddr(vmaddr)) { return true; }
      }
      return false;
   }

   template <Bits bits>
   bool ParseEnv<bits>::vmaddr_in_zerofill(std::size_t vmaddr) const {
      for (Segment<bits> *seg : archive.segments()) {
         if (!seg->contains_vmaddr(vmaddr)) { continue; }
         for (Section<bits> *sec : seg->sections) {
            if (!sec->contains_vmaddr(vmaddr)) { continue; }
            const uint32_t stype = sec->sect.flags & SECTION_TYPE;
            return stype == S_ZEROFILL || stype == S_GB_ZEROFILL ||
                   stype == S_THREAD_LOCAL_ZEROFILL;
         }
         return false; /* in segment but between/outside sections */
      }
      return false;
   }

   template <Bits bits>
   std::size_t ParseEnv<bits>::min_section_vmaddr() const {
      if (min_sect_vmaddr_cache != 0) { return min_sect_vmaddr_cache; }
      std::size_t lo = SIZE_MAX;
      for (Segment<bits> *seg : archive.segments()) {
         if (std::strncmp(seg->segment_command.segname, SEG_PAGEZERO,
                          sizeof(seg->segment_command.segname)) == 0) { continue; }
         if (std::strncmp(seg->segment_command.segname, SEG_LINKEDIT,
                          sizeof(seg->segment_command.segname)) == 0) { continue; }
         for (Section<bits> *sec : seg->sections) {
            if (sec->sect.size == 0) { continue; }
            if ((std::size_t)sec->sect.addr < lo) {
               lo = (std::size_t)sec->sect.addr;
            }
         }
      }
      /* An image with no sections at all: keep the caller's own floor by
       * reporting a value that cannot lower it. */
      min_sect_vmaddr_cache = (lo == SIZE_MAX) ? SIZE_MAX : lo;
      return min_sect_vmaddr_cache;
   }

   template <Bits bits>
   bool ParseEnv<bits>::vmaddr_in_readonly_opaque_data(std::size_t vmaddr) const {
      for (Segment<bits> *seg : archive.segments()) {
         if ((seg->segment_command.initprot & VM_PROT_WRITE) != 0) { continue; }
         if (std::strncmp(seg->segment_command.segname, SEG_PAGEZERO,
                          sizeof(seg->segment_command.segname)) == 0) { continue; }
         if (std::strncmp(seg->segment_command.segname, SEG_LINKEDIT,
                          sizeof(seg->segment_command.segname)) == 0) { continue; }
         if (!seg->contains_vmaddr(vmaddr)) { continue; }
         for (Section<bits> *sec : seg->sections) {
            if (!sec->contains_vmaddr(vmaddr)) { continue; }
            /* opaque data only — mid-instruction offsets don't survive the
             * M32->M64 transform's code rewrite. */
            return (sec->sect.flags &
                    (S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS)) == 0;
         }
         return false;   /* in segment but between/outside sections */
      }
      return false;
   }

   template <Bits bits>
   bool ParseEnv<bits>::imm_bounds_relocated_table(std::size_t vmaddr) const {
      /* Find a relocated pointer-immediate base B <= vmaddr (the loop base) and
       * require B and vmaddr to lie in the SAME segment — a table's base and its
       * one-past-the-end sentinel are in the same data segment by construction.
       * relocated_ptr_imms is sorted, so the greatest B <= vmaddr is the nearest
       * candidate base; if it shares vmaddr's segment the immediate is a table
       * bound, otherwise it is an unrelated constant. */
      if (relocated_ptr_imms.empty()) { return false; }
      auto it = relocated_ptr_imms.upper_bound(vmaddr);
      if (it == relocated_ptr_imms.begin()) { return false; }
      --it;                          /* greatest base <= vmaddr */
      const std::size_t base = *it;
      for (Segment<bits> *seg : archive.segments()) {
         if (seg->contains_vmaddr(base)) {
            return seg->contains_vmaddr(vmaddr);
         }
      }
      return false;
   }

   template <Bits bits>
   bool ParseEnv<bits>::vmaddr_in_indexed_table_target(std::size_t vmaddr) const {
      for (Segment<bits> *seg : archive.segments()) {
         const char *sn = seg->segment_command.segname;
         const std::size_t snsz = sizeof(seg->segment_command.segname);
         if (std::strncmp(sn, SEG_PAGEZERO, snsz) == 0) { continue; }
         if (std::strncmp(sn, SEG_LINKEDIT, snsz) == 0) { continue; }
         if (std::strncmp(sn, SEG_OBJC,     snsz) == 0) { continue; }
         if (seg->contains_vmaddr(vmaddr)) { return true; }
      }
      return false;
   }

   template <Bits bits>
   bool ParseEnv<bits>::vmaddr_in_const_section(std::size_t vmaddr) const {
      for (Segment<bits> *seg : archive.segments()) {
         if (!seg->contains_vmaddr(vmaddr)) { continue; }
         for (Section<bits> *sec : seg->sections) {
            if (!sec->contains_vmaddr(vmaddr)) { continue; }
            const std::string n = sec->name();
            /* NARROW string/object-pointer targets only: a 32-bit value aliasing
             * one of these is almost always a real pointer. Deliberately
             * EXCLUDES __text/__const/literals — those are large/varied ranges
             * that integer constants frequently alias, which would mis-relocate
             * a `movl $int, off(%reg)` integer ivar store as a pointer (observed
             * to crash iPhoto's early AppKit init). The constant @"..." path
             * that needs this lives in __cstring/__cfstring; objc receiver
             * pointers live in the class-ref / metadata sections. */
            return n == "__cstring" || n == "__cfstring"
                || n.rfind("__objc_", 0) == 0   /* modern ObjC metadata */
                || n == "__cls_refs" || n == "__message_refs"
                || n == "__cls_meth" || n == "__inst_meth"
                || n == "__class" || n == "__meta_class" || n == "__category"
                || n == "__module_info" || n == "__symbols"
                || n == "__instance_vars" || n == "__class_vars"
                || n == "__protocol" || n == "__string_object";
         }
         return false;   /* in segment but between/outside sections */
      }
      return false;
   }

   template <Bits bits>
   bool ParseEnv<bits>::code_alias_is_constant(std::size_t vmaddr) const {
      /* Gate disarmed for locals-stripped binaries: without symbols for the
       * static functions we cannot discriminate a genuine mid-image code
       * pointer from a constant, so callers keep their legacy permissive
       * heuristics. */
      if (!have_local_text_syms) { return false; }
      /* A function ENTRY (fn-ptr table slot / vtable slot / ObjC1 IMP /
       * callback immediate) — genuine pointer, not a constant. */
      if (func_syms.count(vmaddr) != 0) { return false; }
      for (Segment<bits> *seg : archive.segments()) {
         if (!seg->contains_vmaddr(vmaddr)) { continue; }
         for (Section<bits> *sec : seg->sections) {
            if (!sec->contains_vmaddr(vmaddr)) { continue; }
            /* SECTION-granular: only targets inside a section flagged as
             * containing instructions are gated. __TEXT's DATA sections
             * (__cstring/__const/literals) and all data segments keep the
             * callers' permissive treatment (selector refs, string pointers
             * and switch tables legitimately target unsymboled bytes). */
            return (sec->sect.flags &
                    (S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS)) != 0;
         }
         return false;   /* in segment but between/outside sections */
      }
      return false;
   }

   template <Bits bits>
   bool ParseEnv<bits>::stackarg_imm_is_code_constant(std::size_t vmaddr) const {
      /* Only the disarmed (locals-stripped) path — with symbols present
       * code_alias_is_constant already discriminates. See the declaration in
       * parse.hh for why a stack-arg imm32 aliasing a code section is a
       * constant, not a pointer, for a -no_pie/PIC stripped binary. */
      if (have_local_text_syms) { return false; }
      for (Segment<bits> *seg : archive.segments()) {
         if (!seg->contains_vmaddr(vmaddr)) { continue; }
         for (Section<bits> *sec : seg->sections) {
            if (!sec->contains_vmaddr(vmaddr)) { continue; }
            return (sec->sect.flags &
                    (S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS)) != 0;
         }
         return false;   /* in segment but between/outside sections */
      }
      return false;
   }

   template <Bits bits>
   bool ParseEnv<bits>::cstring_slot_has_string_neighbour(
           const Image& img, std::size_t slot_vmaddr) const {
      static const bool disabled =
         std::getenv("M64_NO_CSTR_NEIGHBOUR") != nullptr;
      if (disabled) { return false; }
      /* file offset of a vmaddr inside a file-backed section, or 0 */
      const auto file_off = [this](std::size_t va, uint32_t *flags) -> std::size_t {
         for (Segment<bits> *seg : archive.segments()) {
            if (!seg->contains_vmaddr(va)) { continue; }
            for (Section<bits> *sec : seg->sections) {
               if (!sec->contains_vmaddr(va)) { continue; }
               const uint32_t st = sec->sect.flags & SECTION_TYPE;
               if (st == S_ZEROFILL || st == S_GB_ZEROFILL) { return 0; }
               if (flags) { *flags = sec->sect.flags; }
               return sec->sect.offset + (va - sec->sect.addr);
            }
            return 0;
         }
         return 0;
      };
      /* The slot's section is the one being parsed, which is not yet listed
       * in its segment — so take it from current_section, not the archive. */
      if (current_section == nullptr) { return false; }
      const auto& cs = current_section->sect;
      const uint32_t self = img.at<uint32_t>(cs.offset + (slot_vmaddr - cs.addr));
      for (const long d : {-8L, -4L, 4L, 8L}) {
         const std::size_t nva = slot_vmaddr + d;
         if (nva < cs.addr || nva + 4 > cs.addr + cs.size) { continue; }  /* same section only */
         const uint32_t v = img.at<uint32_t>(cs.offset + (nva - cs.addr));
         if (v == self) { continue; }   /* a repeated table word vouches for nothing */
         uint32_t tflags = 0;
         const std::size_t to = file_off(v, &tflags);
         if (!to || (tflags & SECTION_TYPE) != S_CSTRING_LITERALS) { continue; }
         if (!cstring_interior_alias(img, v)) { return true; }   /* a START */
      }
      return false;
   }

   template <Bits bits>
   bool ParseEnv<bits>::cstring_interior_alias(const Image& img,
                                               std::size_t vmaddr) const {
      for (Segment<bits> *seg : archive.segments()) {
         if (!seg->contains_vmaddr(vmaddr)) { continue; }
         for (Section<bits> *sec : seg->sections) {
            if (!sec->contains_vmaddr(vmaddr)) { continue; }
            /* Only S_CSTRING_LITERALS sections carry the NUL-delimited string
             * invariant this test relies on. Any other target (instructions,
             * __const, ObjC metadata, data) is left to the existing gates. */
            if ((sec->sect.flags & SECTION_TYPE) != S_CSTRING_LITERALS) {
               return false;
            }
            /* A pointer to the section's first string is a valid START. */
            if (vmaddr == sec->sect.addr) { return false; }
            /* String starts are exactly the bytes following a NUL terminator;
             * everything else is a mid-string INTERIOR position. */
            const std::size_t off = sec->sect.offset + (vmaddr - sec->sect.addr);
            if (img.at<uint8_t>(off - 1) == 0) { return false; }  /* start */
            return true;                                          /* interior */
         }
         return false;   /* in segment but between/outside sections */
      }
      return false;
   }

   template <Bits bits>
   void ParseEnv<bits>::build_zf_code_literals(const Image& img) const {
      std::vector<std::pair<std::size_t, std::size_t>> zf;   /* [lo, hi) */
      for (Segment<bits> *seg : archive.segments()) {
         for (Section<bits> *sec : seg->sections) {
            const uint32_t st = sec->sect.flags & SECTION_TYPE;
            if (st == S_ZEROFILL || st == S_GB_ZEROFILL) {
               zf.emplace_back(sec->sect.addr, sec->sect.addr + sec->sect.size);
            }
         }
      }
      if (zf.empty()) { return; }
      for (Segment<bits> *seg : archive.segments()) {
         for (Section<bits> *sec : seg->sections) {
            if (!(sec->sect.flags & (S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS)) ||
                sec->sect.offset == 0 || sec->sect.size < 4) { continue; }
            const std::size_t off = sec->sect.offset;
            for (std::size_t i = 0; i + 4 <= sec->sect.size; ++i) {
               const uint32_t v = img.at<uint32_t>(off + i);
               for (const auto& r : zf) {
                  if (v >= r.first && v < r.second) { zf_code_literals.insert(v); break; }
               }
            }
         }
      }
   }

   template <Bits bits>
   bool ParseEnv<bits>::zerofill_target_unattested(const Image& img, std::size_t vmaddr,
                                                   bool sibling) const {
      static const bool disabled =
         std::getenv("M64_NO_ZEROFILL_TARGET_GATE") != nullptr;
      if (disabled) { return false; }
      for (Segment<bits> *seg : archive.segments()) {
         if (!seg->contains_vmaddr(vmaddr)) { continue; }
         for (Section<bits> *sec : seg->sections) {
            if (!sec->contains_vmaddr(vmaddr)) { continue; }
            const uint32_t stype = sec->sect.flags & SECTION_TYPE;
            /* Zero-fill only (__DATA,__bss and __DATA,__common). Every other
             * data target keeps the callers' existing treatment. */
            if (stype != S_ZEROFILL && stype != S_GB_ZEROFILL) { return false; }
            /* ★POSITIVE-EVIDENCE TEST, the same idiom the code-entry gate uses.
             * A genuine compile-time pointer into zero-fill space targets an
             * OBJECT — either its start or its interior (`&freqstruct[150000]`,
             * the shape guarded by 96_zerofill_common_interior and needed by
             * Halo's own non-lazy slots). Such an object begins at a symbol. So
             * demand a data symbol at or below the target WITHIN THIS SECTION:
             * that symbol is the object the pointer is into.
             * A (small, small) u16 pair that merely aliases zero-fill space has
             * no such anchor — MEASURED on Halo, whose 0x0048021C / 0x005802D0
             * both fall BELOW the lowest __common symbol (0x005B4620), i.e. in
             * the symbol-free region, while the test fixture's interior pointer
             * sits above its array's symbol.
             * func_syms holds every non-stab N_SECT symbol, data included. */
            const auto it = func_syms.upper_bound(vmaddr);   /* first > vmaddr */
            if (it != func_syms.begin()) {
               const std::size_t prev = *std::prev(it);
               if (prev >= sec->sect.addr) { return false; }  /* attested */
            }
            /* The missing anchor is only evidence when the section HAS
             * symbols to be missing (Halo: its targets sit below the lowest
             * __common symbol). A section with none at all — a locals-stripped
             * image (iPhoto's `__data` slot -> &__common+0xac, fed to
             * gettimeofday) — can attest nothing, so the gate disarms, like
             * the func-entry gate does for stripped binaries. Guard
             * zerofill-stripped-ptr; OFF arm M64_ZF_GATE_STRIPPED=1. */
            static const bool gate_stripped =
               std::getenv("M64_ZF_GATE_STRIPPED") != nullptr;
            /* A SIBLING (record-field column evidence) keeps the armed test:
             * there symbol absence is one vote among several exact ones, not
             * the sole verdict on a lone word (record-field-pair's column into
             * a -x fixture's symbol-free zero-fill). */
            if (!gate_stripped && !sibling) {
               const std::size_t end = sec->sect.addr + sec->sect.size;
               const auto first = func_syms.lower_bound(sec->sect.addr);
               if (first == func_syms.end() || *first >= end) { return false; }
            }
            /* The CODE addresses this exact object (`movl %eax, 0x374250`):
             * as good an anchor as a symbol. ★MEASURED on PvZ, locals-stripped
             * with its only __common symbols (Mach notify exports) at the END
             * of the section: a __data table of 1365 pointers to its globals
             * sat in "symbol-free" space and stayed raw i386 (SIGSEGV after
             * "Click to start"); 1259 of them are code literals. Halo's
             * 0x0048021C / 0x005802D0 are not. Guard zerofill-code-literal;
             * OFF arm M64_NO_ZF_CODE_LITERAL=1. */
            static const bool no_code_literal =
               std::getenv("M64_NO_ZF_CODE_LITERAL") != nullptr;
            if (!no_code_literal) {
               if (!zf_code_literals_built) {
                  zf_code_literals_built = true;
                  build_zf_code_literals(img);
               }
               if (zf_code_literals.count(static_cast<uint32_t>(vmaddr))) { return false; }
            }
            return true;   /* no anchoring symbol in this section -> constant */
         }
         return false;   /* in segment but between/outside sections */
      }
      return false;
   }

   /* RECORD-FIELD (neighbour/stride) gate. See the header for the Halo #35
    * measurement and for why the conditions are as demanding as they are. */
   template <Bits bits>
   bool ParseEnv<bits>::record_field_neighbours_are_integers(
           const Image& img, std::size_t slot_vmaddr) const {
      static const bool disabled =
         std::getenv("M64_NO_RECORD_FIELD_GATE") != nullptr;
      if (disabled) { return false; }

      /* ★"Is this SIBLING a pointer?" — and the naive form of that question is a
       * TRAP, which cost me a full build+retranslate cycle to see. Asking merely
       * "does the value land in a mapped section" answers YES for every sibling
       * here: Halo's column holds 0x00180000 / 0x00240000 / 0x00300000, all of
       * which alias __TEXT,__text. In a SMALL image every valid address has a
       * small high half, so a (small,small) pair is byte-identical to a real
       * address — the same reason no value-based test can classify the candidate
       * itself. A gate built on that test vetoes instantly and does nothing.
       *
       * The question that actually discriminates is what the PASS DECIDED: those
       * siblings were left as constants (0x00030002 / 0x00070002 / 0x00060002 /
       * 0x00050001 all land STRICTLY INSIDE a decoded instruction; 0x003c0000
       * was rejected as unattested zero-fill), and only 0x00380000 slipped
       * through because it lands in __DATA,__data where no gate existed. So run
       * the existing discriminators on the sibling and treat "a gate calls it a
       * constant" as integer evidence. Never recurses into this gate.
       *
       * ★★ONLY THE EXACT, STRUCTURAL DISCRIMINATORS MAY BE USED HERE, and this
       * is MEASURED, not a matter of taste. The first version also accepted
       * `code_alias_lacks_entry_evidence` / `code_alias_is_constant` as
       * declassifying a sibling — and those are precisely the heuristics that a
       * switch JUMP TABLE is deliberately exempted from, because its entries
       * target basic-block heads which have no symbol and no prologue. So every
       * entry of every jump table declassified every other entry, the gate fired
       * on the whole table, and the ON-vs-OFF blast radius was 8721 words on
       * Halo alone — 7889 of them in __TEXT,__const, i.e. real code pointers
       * demoted wholesale (a contiguous run 0x79da, 0x78fc, 0x7908, 0x7914,
       * 0x7920 ... left holding i386 addresses). With only the exact tests the
       * radius is the two defective Halo records and nothing else, because a
       * jump-table entry IS an instruction boundary, so `code_interior_alias`
       * refuses to declassify it and `pointer_sibling` vetoes — which is exactly
       * the separation between a record array and a jump table that this gate
       * exists to draw. */
      const auto sibling_is_pointerish = [this, &img](uint32_t v) -> bool {
         if (v == 0) { return false; }              /* neutral, handled by caller */
         bool in_section = false, in_exec = false;
         for (Segment<bits> *seg : archive.segments()) {
            if (!seg->contains_vmaddr(v)) { continue; }
            for (Section<bits> *sec : seg->sections) {
               if (!sec->contains_vmaddr(v)) { continue; }
               in_section = true;
               in_exec = (sec->sect.flags & S_ATTR_PURE_INSTRUCTIONS) ||
                         (sec->sect.flags & S_ATTR_SOME_INSTRUCTIONS);
               break;
            }
            break;
         }
         if (!in_section) { return false; }         /* addresses nothing: integer */
         if (zerofill_target_unattested(img, v, true)) { return false; }
         if (cstring_interior_alias(img, v)) { return false; }
         if (in_exec && code_interior_alias(v)) { return false; }
         return true;                               /* nothing declassifies it */
      };

      for (Segment<bits> *seg : archive.segments()) {
         if (!seg->contains_vmaddr(slot_vmaddr)) { continue; }
         for (Section<bits> *sec : seg->sections) {
            if (!sec->contains_vmaddr(slot_vmaddr)) { continue; }
            /* Needs file content to read siblings, so zero-fill is out (and is
             * already handled by zerofill_target_unattested). */
            if (sec->sect.offset == 0) { return false; }
            const uint32_t stype = sec->sect.flags & SECTION_TYPE;
            /* Sections that are BY DEFINITION arrays of pointers must never be
             * reclassified by a neighbour argument. */
            if (stype == S_LAZY_SYMBOL_POINTERS ||
                stype == S_NON_LAZY_SYMBOL_POINTERS ||
                stype == S_MOD_INIT_FUNC_POINTERS ||
                stype == S_MOD_TERM_FUNC_POINTERS ||
                stype == S_LITERAL_POINTERS) {
               return false;
            }

            const std::size_t lo = sec->sect.addr;
            const std::size_t hi = sec->sect.addr + sec->sect.size;

            /* Record strides. 4 is EXCLUDED on purpose: at stride 4 the
             * "siblings" are the adjacent FIELDS of a single struct, not the
             * same field of sibling records, so a lone pointer between two int
             * members would be misread as an integer. */
            static const std::size_t strides[] =
               { 8, 12, 16, 20, 24, 28, 32, 36, 40, 48, 56, 64 };
            /* ★★ONE HYPOTHESIS ONLY. The first version tried all twelve strides
             * and accepted if ANY of them looked integer-ish — twelve chances to
             * be wrong, and in a 110KB __const section a run of six unrelated
             * non-pointer words at some stride is not a coincidence, it is a
             * certainty. MEASURED on Halo: the switch jump table at 0x341540
             * (0x00018dd6, one of a dense cluster 0x18da8..0x18dd6) is correctly
             * VETOED at stride 8 — `pointer_sibling=1`, its true neighbours are
             * code pointers — and was then wrongly ACCEPTED at stride 64, where
             * the "siblings" are 0x7fffffff sentinels belonging to a completely
             * different table. That one bug accounted for most of a 1264-word
             * blast radius.
             *
             * A record array has exactly ONE stride, so testing many is fishing.
             * Take the TIGHTEST hypothesis the data supports — the smallest
             * stride that has a complete sibling set — and let its verdict be
             * final. Failing to fire on a genuine stride-16 array whose stride-8
             * view contains a pointer is the safe direction to be wrong in. */
            for (std::size_t stride : strides) {
               int present = 0, aliasing_int = 0, pointer_siblings = 0;
               for (int k = -3; k <= 3; ++k) {
                  if (k == 0) { continue; }
                  const long long a =
                     (long long)slot_vmaddr + (long long)k * (long long)stride;
                  if (a < (long long)lo || a + 4 > (long long)hi) { continue; }
                  ++present;
                  const std::size_t off =
                     sec->sect.offset + ((std::size_t)a - sec->sect.addr);
                  const uint32_t w = img.template at<uint32_t>(off);
                  /* ★★ONLY AN ADDRESS-SHAPED SIBLING IS EVIDENCE. This is the
                   * "same FIELD of neighbouring RECORDS" requirement done
                   * properly, and the stride-4 exclusion is only a special case
                   * of it. We do not know the true record stride, so the stride
                   * under test may be a DIVISOR of it — and then this set is the
                   * neighbouring FIELDS of one record, which proves nothing
                   * about our field.
                   *
                   * A sibling that ALIASES a mapped section and was nonetheless
                   * proven constant is evidence: it says "this column holds
                   * values that look like addresses but are not". A sibling that
                   * is 0, a small count, or a float addresses nothing, so it
                   * cannot be the same field as an address-shaped candidate —
                   * it is a sign the stride is wrong. Those are NEUTRAL, exactly
                   * like the zeros the original rule already excluded.
                   *
                   * ★MEASURED on Quinn, which is precisely the divisor shape: an
                   * array of 32-byte records {CFStringRef, 0, 1, float, float,
                   * float, float, ptr} at 0xb4300, 0xb4320, 0xb4340 ... At
                   * stride 8 the siblings of the CFStringRef field are that
                   * record's OWN 0 / 1 / 0x41300000 / 0x40800000 — three nonzero
                   * "integers", exactly meeting the old bar — so the gate fired
                   * and demoted 7 genuine CFString pointers into
                   * __DATA,__cfstring. Requiring address-shaped evidence drops
                   * that to zero while Halo's column (0x00050001, 0x00070002,
                   * 0x00060002 — all aliasing __TEXT,__text, all proven
                   * mid-instruction) still supplies four. */
                  if (w == 0) { continue; }            /* neutral, never evidence */
                  bool in_section = false;
                  for (Segment<bits> *s2 : archive.segments()) {
                     if (!s2->contains_vmaddr(w)) { continue; }
                     for (Section<bits> *c2 : s2->sections) {
                        if (c2->contains_vmaddr(w)) { in_section = true; break; }
                     }
                     break;
                  }
                  if (!in_section) { continue; }       /* not address-shaped */
                  if (sibling_is_pointerish(w)) { ++pointer_siblings; continue; }
                  ++aliasing_int;
               }
               /* Demand the FULL sibling set: a partial one is what a one-off
                * struct near a section edge looks like. */
               if (present < 6) { continue; }   /* not a usable hypothesis yet */
               /* ★PROVEN-INTEGER MAJORITY, not a single-sibling veto. A jump
                * table's siblings are all basic-block heads, never
                * mid-instruction (aliasing_int stays 0), and a real pointer
                * column's siblings are pointers, so >=3 exact mid-instruction /
                * unattested siblings cannot be either; a packed integer that
                * merely lands on an instruction boundary (~1 in 3 odds per
                * word) must not veto them. MEASURED on PvZ's static zlib
                * `lenfix` ({op,bits,val} words, 0x003c0800 ...): 4 siblings
                * mid-instruction, 0x000c0800 on a boundary -> 58 codes
                * "relocated", every inflate of a fixed-Huffman block corrupt
                * ("invalid distance too far back"). OFF arm:
                * M64_RECFIELD_STRICT_VETO=1. */
               static const bool strict_veto =
                  std::getenv("M64_RECFIELD_STRICT_VETO") != nullptr;
               if (strict_veto) { return pointer_siblings == 0 && aliasing_int >= 3; }
               return aliasing_int >= 3 && aliasing_int > pointer_siblings;
            }
            return false;
         }
         return false;
      }
      return false;
   }

   template <Bits bits>
   bool ParseEnv<bits>::packed_pair_family(const Image& img,
                                           std::size_t slot_vmaddr,
                                           uint32_t value) const {
      static const bool disabled =
         std::getenv("M64_NO_PACKED_FAMILY") != nullptr;
      if (disabled) { return false; }
      const uint32_t lo = value & 0xffffu;
      /* lo==0 is how genuine 64K-aligned addresses look; never evidence. */
      if (lo == 0) { return false; }
      const auto addresses_something = [this](uint32_t v) {
         for (Segment<bits> *seg : archive.segments()) {
            if (!seg->contains_vmaddr(v)) { continue; }
            for (Section<bits> *sec : seg->sections) {
               if (sec->contains_vmaddr(v)) { return true; }
            }
            return false;
         }
         return false;
      };
      for (Segment<bits> *seg : archive.segments()) {
         if (!seg->contains_vmaddr(slot_vmaddr)) { continue; }
         for (Section<bits> *sec : seg->sections) {
            if (!sec->contains_vmaddr(slot_vmaddr)) { continue; }
            if (sec->sect.offset == 0) { return false; }
            const std::size_t lo_a = sec->sect.addr, hi_a = sec->sect.addr + sec->sect.size;
            int family = 0, homeless = 0, pairs = 0, pairs_homeless = 0, near = 0;
            uint32_t his[17]; int nhi = 0;
            /* A u16 PAIR: nonzero high half, small low half. A real pointer's
             * low half is ~uniform, so <1/16 of a pointer column looks like
             * this; a u16 array ({16,16} {33,49} ...) is nothing else. */
            const auto is_pair = [](uint32_t w) {
               /* both halves small and nonzero: a float (0x3f800000) or a
                * 64K-aligned value is not a u16 pair (PvZ __data records of
                * {char *, float...} otherwise demoted the string pointer). */
               const uint32_t h = w >> 16, l = w & 0xffffu;
               return h != 0 && h < 0x1000u && l != 0 && l < 0x1000u;
            };
            /* ponytail: fixed windows (+-32 family, +-8 pairs); widen only if a
             * measured table needs it (wider windows sample unrelated data). */
            for (long k = -32; k <= 32; ++k) {
               const long long a = (long long)slot_vmaddr + 4 * k;
               if (k == 0 || a < (long long)lo_a || a + 4 > (long long)hi_a) { continue; }
               const uint32_t w = img.template at<uint32_t>(
                  sec->sect.offset + ((std::size_t)a - sec->sect.addr));
               if (k >= -8 && k <= 8) { ++near; }
               if (k >= -8 && k <= 8 && is_pair(w)) {
                  ++pairs;
                  if (!addresses_something(w)) { ++pairs_homeless; }
                  if (std::find(his, his + nhi, w >> 16) == his + nhi) { his[nhi++] = w >> 16; }
               }
               if ((w & 0xffffu) != lo || (w >> 16) == (value >> 16)) { continue; }
               ++family;
               if (!addresses_something(w)) { ++homeless; }
            }
            if (family >= 3 && homeless >= 1) { return true; }
            /* A word aimed at the START of a C string is a string pointer. */
            for (Segment<bits> *s2 : archive.segments()) {
               if (!s2->contains_vmaddr(value)) { continue; }
               for (Section<bits> *c2 : s2->sections) {
                  if (c2->contains_vmaddr(value) &&
                      (c2->sect.flags & SECTION_TYPE) == S_CSTRING_LITERALS &&
                      !cstring_interior_alias(img, value)) {
                     return false;
                  }
               }
               break;
            }
            /* u16-ARRAY run (PvZ zlib lext/dbase: 0x00100010 = {16,16}). */
            static const bool no_pairs = std::getenv("M64_NO_U16_PAIR_RUN") != nullptr;
            /* Dense runs need no homeless member: >=75% of the (up to 16)
             * neighbours pair-shaped with >=3 distinct high halves is ~1e-6 for
             * a pointer column even at a section edge, and pointers into one
             * 64K-aligned buffer share a single high half. */
            return !no_pairs && is_pair(value) && pairs >= 6 &&
                   (pairs_homeless >= 1 || (4 * pairs >= 3 * near && nhi >= 3));
         }
         return false;
      }
      return false;
   }

   template <Bits bits>
   bool ParseEnv<bits>::code_interior_alias(std::size_t vmaddr) const {
      static const bool disabled =
         std::getenv("M64_NO_CODE_INTERIOR_GATE") != nullptr;
      if (disabled) { return false; }
      for (Segment<bits> *seg : archive.segments()) {
         if (!seg->contains_vmaddr(vmaddr)) { continue; }
         for (Section<bits> *sec : seg->sections) {
            if (!sec->contains_vmaddr(vmaddr)) { continue; }
            /* SECTION-granular, like code_alias_is_constant: only sections that
             * actually hold decoded instructions carry the boundary invariant.
             * __TEXT's DATA sections (__cstring/__const/literals) and every data
             * section keep the callers' permissive treatment. */
            if ((sec->sect.flags &
                 (S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS)) == 0) {
               return false;
            }
            /* SELF-DISARM: the gate is only meaningful once this section has
             * been parsed into blobs. Sections are parsed in load-command order
             * (__TEXT before __DATA in every real image), but never assume it —
             * an empty blob range would otherwise declare every address
             * "interior" and reject genuine code pointers wholesale. */
            const std::size_t lo = sec->sect.addr;
            const std::size_t hi = lo + sec->sect.size;
            const auto& found = vmaddr_resolver.found;
            auto it = found.lower_bound(lo);
            if (it == found.end() || it->first >= hi) { return false; }
            /* Every SectionBlob registers its own start vmaddr (SectionBlob
             * ctor). Two ways to be a non-boundary:
             *   - no blob starts here  -> strictly inside a decoded instruction
             *   - a blob starts here but it is NOT an Instruction -> the linear
             *     sweep could not decode these bytes (or they are inter-function
             *     padding re-synced by the func_syms boundary rule) and emitted
             *     raw DataBlobs. Undecodable bytes are not a legal branch target
             *     either, and Immediate's exact-key resolve DOES attach to such a
             *     DataBlob — which is precisely how Civ IV's " ._" got rebased
             *     (its i386 address 0x5F2E20 sits 8 bytes into the 11-byte span
             *     starting at the instruction 0x5F2E18, on a DataBlob).
             * Only a real decoded Instruction is a genuine code target. */
            auto hit = found.find(vmaddr);
            if (hit == found.end()) { return true; }
            return dynamic_cast<const Instruction<bits> *>(hit->second) == nullptr;
         }
         return false;   /* in segment but between/outside sections */
      }
      return false;
   }

   /* True iff `vmaddr` lands inside a section carrying decoded instructions.
    * The "does this value even alias code" half of the cohesion test, kept
    * separate from the boundary question that code_interior_alias answers. */
   template <Bits bits>
   bool ParseEnv<bits>::vmaddr_in_instructions_sect(std::size_t vmaddr) const {
      for (Segment<bits> *seg : archive.segments()) {
         if (!seg->contains_vmaddr(vmaddr)) { continue; }
         for (Section<bits> *sec : seg->sections) {
            if (!sec->contains_vmaddr(vmaddr)) { continue; }
            return (sec->sect.flags &
                    (S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS)) != 0;
         }
         return false;
      }
      return false;
   }

   template <Bits bits>
   bool ParseEnv<bits>::code_alias_run_contradicted(const Image& img,
                                                    const Location& loc,
                                                    std::size_t value) const {
      static const bool disabled = std::getenv("M64_NO_JT_COHESION") != nullptr;
      if (disabled) { return false; }
      if (current_section == nullptr) { return false; }
      /* Only meaningful for a value that aliases code at a real boundary — the
       * exact case every other gate waves through. */
      if (!vmaddr_in_instructions_sect(value) || code_interior_alias(value)) {
         return false;
      }
      /* LAST-RESORT ONLY. Never speak about a value that carries POSITIVE
       * function-entry evidence (an nlist at the address, an i386 `55 89 e5`
       * prologue, or a C++ adjustor-thunk entry shape): that is a genuine code
       * pointer whatever its neighbours look like, and a vtable slot or
       * fn-ptr-table entry that happens to sit next to an integer must not be
       * demoted. This confines the gate to exactly the blind spot it was
       * written for — a code-aliasing word with NO positive evidence, which is
       * all __TEXT,__const leaves behind once the entry gates are excluded. */
      if (code_target_has_entry_evidence(img, value)) { return false; }
      const auto& cs = current_section->sect;
      const std::size_t sect_lo = cs.addr;
      const std::size_t sect_hi = cs.addr + cs.size;

      /* ★RUN-WIDE, not just the two immediate neighbours — and that widening is
       * the whole gate, because the ±4 form is STRUCTURALLY BLIND to a packed
       * u16-pair table. Such a table is a contiguous run of code-aliasing words
       * in which each word lands mid-instruction or on a boundary essentially
       * at random, so a typical entry has one contradicting AND one
       * corroborating neighbour and the `!corroborated` veto fires every time.
       *
       * MEASURED on Halo CE, the vertex-STRIDE table at i386 0x34e280 in
       * __TEXT,__const — 20 u16 strides indexed by vertex format, i.e. 10
       * words, delimited by 1.0f floats below and zeros above:
       *     0x34e280 (56,32)  BOUNDARY   0x34e290 (24,16)  interior
       *     0x34e284 (20, 8)  BOUNDARY   0x34e294 (16,20)  interior
       *     0x34e288 (68,32)  interior   0x34e298 (32, 8)  interior
       *     0x34e28c (24,36)  interior   0x34e29c (32,32)  BOUNDARY
       *                                  0x34e2a0 (36,28)  BOUNDARY
       *                                  0x34e2a4 (32,40)  interior
       * Six of the ten are proven mid-instruction, yet ALL FOUR boundary words
       * are corroborated by a boundary neighbour, so the ±4 test cannot fire on
       * any of them. Two were then rebased — (20,8) -> 0x1009e312 and (32,32)
       * -> 0x102a780f — so format 15's stride 32 read back as 4138. The vertex
       * walk stepped 4138 bytes per vertex through the menu geometry and every
       * animated 3D object in the background smeared. (The two survivors were
       * saved by the record-field gate below, which is positional and happens
       * to see this shape from a different angle; it is not reliable here.)
       *
       * The run-wide rule is EXACT, not a threshold: a switch jump table is a
       * run of branch targets, and a branch target is an instruction boundary
       * by construction, so a genuine table contains ZERO interior entries. One
       * proven interior anywhere in the run therefore falsifies the entire run
       * — a table cannot contradict itself, so this can never demote a real
       * one however its entries are arranged.
       *
       * ★ONLY code_interior_alias MAY VOTE, for the reason the record-field
       * gate's header records at length: the entry-evidence heuristics are ones
       * jump tables are DELIBERATELY exempt from (their targets are basic-block
       * heads with no symbol and no prologue), so letting those vote makes every
       * table entry declassify every other and the blast radius was 8721 words.
       * A mid-instruction address, by contrast, is not a legal branch target on
       * any control path — that is arithmetic about the decode, not evidence
       * about a symbol.
       *
       * ★THE MEASURE IS THE ALL-BOUNDARY BLOCK, NOT THE WHOLE RUN. A first cut
       * that fired on "any proven interior anywhere in the surrounding run"
       * DEMOTED THREE GENUINE JUMP TABLES on Halo (64, 56 and 44 entries at
       * 0x34a5c8 / 0x357b60 / 0x34406c, every single target a boundary and all
       * of them clustered inside one function) — because a wide window happily
       * runs off the end of a real table into whatever packed data abuts it.
       * The object boundary has to be found, not assumed.
       *
       * So walk outward through BOUNDARY words only and stop at the first word
       * that is not one. That block is the candidate object, and the two shapes
       * separate by an enormous margin — measured on Halo:
       *     stride-table words   block =   2   .............B[B]iiiiiBBi......
       *     jump tables          block = 278, 56, 44   BBBBBBBB[B]BBBBBBBB
       * A switch table IS its all-boundary block, so it is long and its ends
       * are the ends of the object. A packed u16 pair that merely happens to
       * land on a boundary sits in a tiny block wedged between words that are
       * PROVEN mid-instruction — and a mid-instruction word cannot be part of
       * any table, so it is a real object edge, not a coincidence.
       *
       * Two conditions, both required:
       *   - the block is terminated by a proven INTERIOR word on at least one
       *     side. Terminating on a float/zero/section edge is not evidence: that
       *     is exactly how a genuine table ends too.
       *   - the block is shorter than any switch table a compiler would emit in
       *     preference to a compare chain. The observed margin is 2 vs 44, so
       *     this threshold is nowhere near either population. */
      static const bool narrow =
         std::getenv("M64_JT_COHESION_NEIGHBOURS_ONLY") != nullptr;
      static const char *blkenv = std::getenv("M64_JT_COHESION_MAX_BLOCK");
      const int max_block = blkenv ? std::atoi(blkenv) : 3;
      const int window = 512;   /* cap: keeps the walk O(1) */

      if (narrow) {   /* legacy +-4 form, kept for A/B */
         bool contradicted = false, corroborated = false;
         for (int delta : {-4, +4}) {
            const std::size_t nb_vm =
               (std::size_t)((std::ptrdiff_t)loc.vmaddr + delta);
            if (nb_vm < sect_lo || nb_vm + 4 > sect_hi) { continue; }
            const std::size_t nb_off =
               (std::size_t)((std::ptrdiff_t)loc.offset + delta);
            const std::size_t nb = (std::size_t)img.at<uint32_t>(nb_off);
            if (!vmaddr_in_instructions_sect(nb)) { continue; }
            if (code_interior_alias(nb)) { contradicted = true; }
            else { corroborated = true; }
         }
         return contradicted && !corroborated;
      }

      int block = 1;                    /* the slot itself is a boundary */
      bool interior_edge = false;
      for (int dir : {-1, +1}) {
         for (int step = 1; step <= window; ++step) {
            const std::ptrdiff_t delta = (std::ptrdiff_t)dir * step * 4;
            const std::size_t nb_vm =
               (std::size_t)((std::ptrdiff_t)loc.vmaddr + delta);
            /* Stay strictly inside the section the slot lives in: a word beyond
             * it belongs to a different object and says nothing about this
             * run. The section edge is NOT interior evidence. */
            if (nb_vm < sect_lo || nb_vm + 4 > sect_hi) { break; }
            const std::size_t nb_off =
               (std::size_t)((std::ptrdiff_t)loc.offset + delta);
            const std::size_t nb = (std::size_t)img.at<uint32_t>(nb_off);
            /* ★QWORD HIGH HALF — NEUTRAL, NOT AN OBJECT EDGE. An M32 image has
             * no 8-byte pointers, so an 8-aligned array of 64-bit INTEGERS is
             * laid out as {value_lo, value_hi} pairs and its small entries put a
             * ZERO in every odd word. Those zeros are not "another object": they
             * are the other half of this one. The walk used to stop dead on the
             * first of them, so every such array's low halves were judged with
             * block == 1 and interior_edge == false — the gate could not see the
             * proven mid-instruction siblings sitting two words away, and any
             * entry whose value happened to land on an instruction BOUNDARY was
             * rebased into a translated code address.
             *
             * ★MEASURED (Quinn, 2026-09-23): the 32-entry u64 knapsack vector at
             * __TEXT,__const 0xb2c60 (the highscore file's obfuscation weights).
             * Entries 7 (0x29a5) and 9 (0xbd3c) are exact i386 instruction
             * boundaries and were rewritten to 0x10001ee7 / 0x10011d2a, while
             * their stride-8 siblings 0x73b5 / 0x1ee7f / 0x425b6 land
             * mid-instruction and were correctly left alone. Encrypting with two
             * inflated weights made 63 of the 102 stored 8-byte records decrypt
             * to garbage, so the saved NSArchiver stream came back with its
             * "streamtyped" signature mangled and every highscore was lost on
             * restart.
             *
             * EXACT, not a threshold: the skipped word must be zero AND sit at
             * offset 4 of an 8-aligned slot AND the candidate itself must be
             * 8-aligned — i.e. literally the high half of the same qword column.
             * A 4-byte jump table cannot present that shape in its interior (a
             * zero entry is not a branch target), and at its END the downward
             * walk counts its real entries and exceeds max_block first.
             * Kill switch M64_NO_QWORD_HIGH_SKIP=1. */
            static const bool no_qword_skip =
               std::getenv("M64_NO_QWORD_HIGH_SKIP") != nullptr;
            if (!no_qword_skip && nb == 0 &&
                (loc.vmaddr & 7) == 0 && (nb_vm & 7) == 4) {
               continue;                  /* high half of a u64 -> neutral */
            }
            if (!vmaddr_in_instructions_sect(nb)) { break; }  /* object edge */
            if (code_interior_alias(nb)) {
               interior_edge = true;    /* a PROVEN non-target ends the block */
               break;
            }
            ++block;                    /* still inside the all-boundary block */
            if (block > max_block) { return false; }   /* too long: a table */
         }
      }
      return interior_edge && block <= max_block;
   }

   template <Bits bits>
   bool ParseEnv<bits>::code_target_has_entry_evidence(const Image& img,
                                                       std::size_t vmaddr) const {
      /* An nlist AT the address. Globals survive `strip -x`, and a genuine
       * DATA-resident code pointer (C++ vtable slot, fn-ptr table entry, ObjC1
       * IMP) targets a DEFINED function, so this hits for the overwhelming
       * majority of real pointers even in a stripped image. */
      if (func_syms.count(vmaddr) != 0) { return true; }
      /* Entry SHAPES, for the functions a locals-strip left unsymboled. */
      for (Segment<bits> *seg : archive.segments()) {
         if (!seg->contains_vmaddr(vmaddr)) { continue; }
         const std::size_t fo = vmaddr - seg->segment_command.vmaddr
                              + seg->segment_command.fileoff;
         /* (a) the standard i386 frame-setup prologue. */
         if (fo + 3 <= img.size() &&
             img.template at<uint8_t>(fo)     == 0x55 &&   /* push %ebp     */
             img.template at<uint8_t>(fo + 1) == 0x89 &&   /* mov %esp,%ebp */
             img.template at<uint8_t>(fo + 2) == 0xe5) {
            return true;
         }
         /* GCC may schedule an instruction between the two halves of the
          * frame setup: PvZ's vtable targets `push %ebp; mov $3,%edx;
          * mov %esp,%ebp` (a regparm arg). Accept `mov %esp,%ebp` within the
          * next 12 bytes after the push. Guard prologue-sched-entry; OFF arm
          * M64_STRICT_PROLOGUE=1. */
         static const bool strict_prologue =
            std::getenv("M64_STRICT_PROLOGUE") != nullptr;
         if (!strict_prologue && fo + 14 <= img.size() &&
             img.template at<uint8_t>(fo) == 0x55) {
            for (std::size_t k = fo + 1; k < fo + 13; ++k) {
               if (img.template at<uint8_t>(k) == 0x89 &&
                   img.template at<uint8_t>(k + 1) == 0xe5) { return true; }
            }
         }
         /* (b) an i386 C++ ABI ADJUSTOR THUNK entry: adjust the `this` pointer
          * in place on the stack, then tail-`jmp` to the real override —
          *    83 /0|/5 44|6c 24 <disp8> <imm8>          add|sub $imm8, disp8(%esp)
          *    81 /0|/5 44|6c 24 <disp8> <imm32>         add|sub $imm32,disp8(%esp)
          * immediately followed by E9 rel32 / EB rel8.
          * (SIB 0x24 = base %esp, no index; disp8 is 4 for a normal `this`,
          * 8 when a hidden struct-return pointer precedes it — both occur.)
          *
          * These ARE function entries: GCC emits one per multiple-inheritance
          * or covariant-return override and stores it in the VTABLE. But a
          * thunk is a compiler-generated LOCAL symbol (gone after `strip -x`)
          * and it has no frame setup, so without this shape a locals-stripped
          * C++ image would have thousands of genuine vtable slots demoted to
          * "constants" and left holding i386 addresses. MEASURED on Civ IV:
          * 7883 of the 9196 words the ENTRY gate demoted were thunk targets. */
         static const bool no_thunk =
            std::getenv("M64_NO_THUNK_ENTRY_EVIDENCE") != nullptr;
         if (!no_thunk && fo + 12 <= img.size()) {
            const uint8_t op    = img.template at<uint8_t>(fo);
            const uint8_t modrm = img.template at<uint8_t>(fo + 1);
            const uint8_t sib   = img.template at<uint8_t>(fo + 2);
            if ((op == 0x83 || op == 0x81) &&
                (modrm == 0x44 || modrm == 0x6c) &&   /* /0 add, /5 sub, [esp+d8] */
                sib == 0x24) {
               const std::size_t jmp_off = fo + (op == 0x83 ? 5 : 8);
               const uint8_t jmp = img.template at<uint8_t>(jmp_off);
               if (jmp == 0xe9 || jmp == 0xeb) { return true; }
            }
         }
         break;
      }
      return false;
   }

   /* Length of a canonical NOP encoding at file offset `fo`, else 0. Covers the
    * fillers a compiler/assembler emits for inter-function alignment:
    *   (66)* 90                       xchg ax,ax / nop
    *   (66)* 0f 1f /0 [sib] [disp]     the multi-byte NOP family
    *   8d 76 00 / 8d 74 26 00 / 8d b4 26 00 00 00 00   older lea-based fillers
    * ★The multi-byte forms MATTER: `0f 1f 80 00 00 00 00` and
    * `66 0f 1f 84 00 00 00 00 00` both END in 0x00, so a naive "is the previous
    * byte a filler?" test misses them. MEASURED on iPhoto, that miss split ONE
    * eight-slot handler table (installs into +0x8..+0x24 at 0x412d44..0x412d78)
    * into 2 admitted and 6 refused purely by which NOP encoding preceded each
    * target — arbitrary, and 32 of 42 genuine targets image-wide were lost. */
   static std::size_t canonical_nop_len(const Image& img, std::size_t fo) {
      const std::size_t n = img.size();
      std::size_t p = fo, pre = 0;
      while (p < n && img.at<uint8_t>(p) == 0x66 && pre < 4) { ++pre; ++p; }
      if (p < n && img.at<uint8_t>(p) == 0x90) { return pre + 1; }
      if (p + 2 < n && img.at<uint8_t>(p) == 0x0f && img.at<uint8_t>(p + 1) == 0x1f) {
         const uint8_t modrm = img.at<uint8_t>(p + 2);
         const uint8_t mod = modrm >> 6, rm = modrm & 0x07;
         std::size_t len = pre + 3;
         if (rm == 4) { len += 1; }          /* SIB */
         if (mod == 1) { len += 1; }         /* disp8  */
         else if (mod == 2) { len += 4; }    /* disp32 */
         return len;
      }
      if (pre != 0) { return 0; }
      if (fo + 2 < n && img.at<uint8_t>(fo) == 0x8d &&
          img.at<uint8_t>(fo + 1) == 0x76 && img.at<uint8_t>(fo + 2) == 0x00) {
         return 3;
      }
      if (fo + 3 < n && img.at<uint8_t>(fo) == 0x8d &&
          img.at<uint8_t>(fo + 1) == 0x74 && img.at<uint8_t>(fo + 2) == 0x26 &&
          img.at<uint8_t>(fo + 3) == 0x00) {
         return 4;
      }
      if (fo + 6 < n && img.at<uint8_t>(fo) == 0x8d &&
          img.at<uint8_t>(fo + 1) == 0xb4 && img.at<uint8_t>(fo + 2) == 0x26 &&
          img.at<uint8_t>(fo + 3) == 0x00 && img.at<uint8_t>(fo + 4) == 0x00 &&
          img.at<uint8_t>(fo + 5) == 0x00 && img.at<uint8_t>(fo + 6) == 0x00) {
         return 7;
      }
      return 0;
   }

   /* True iff a chain of canonical NOPs tiles exactly up to `fo` — i.e. `fo` is
    * preceded by genuine alignment PADDING, not by data or by the tail of a real
    * instruction. Bounded to a 15-byte window (the x86 max instruction length),
    * so this is O(1). A run of ZERO bytes is NOT a NOP chain (0x00 decodes as
    * `add %al,(%eax)`), which is exactly what keeps the negative guards red. */
   static bool nop_padding_ends_at(const Image& img, std::size_t fo) {
      for (std::size_t back = 1; back <= 15; ++back) {
         if (back > fo) { break; }
         std::size_t p = fo - back;
         bool ok = true;
         while (p < fo) {
            const std::size_t len = canonical_nop_len(img, p);
            if (len == 0) { ok = false; break; }
            p += len;
         }
         if (ok && p == fo) { return true; }
      }
      return false;
   }

   /* See parse.hh. Structural "nothing falls through into this address" test —
    * the half code_target_has_entry_evidence cannot supply, since that one only
    * looks at the bytes AT the target. Pure function of value + image bytes;
    * MUST NOT consult vmaddr_resolver (parse-order dependence). */
   template <Bits bits>
   bool ParseEnv<bits>::code_target_is_function_start(const Image& img,
                                                      std::size_t vmaddr) const {
      static const bool disabled =
         std::getenv("M64_NO_FUNCTION_START_EVIDENCE") != nullptr;
      if (disabled) { return true; }
      for (Segment<bits> *seg : archive.segments()) {
         if (!seg->contains_vmaddr(vmaddr)) { continue; }
         const std::size_t fo = vmaddr - seg->segment_command.vmaddr
                              + seg->segment_command.fileoff;
         /* Need 5 bytes of lookback for the longest suffix (`jmp rel32`). */
         if (fo < 5 || fo > img.size()) { return false; }
         const uint8_t b1 = img.template at<uint8_t>(fo - 1);
         const uint8_t b2 = img.template at<uint8_t>(fo - 2);
         const uint8_t b3 = img.template at<uint8_t>(fo - 3);
         const uint8_t b5 = img.template at<uint8_t>(fo - 5);
         /* (a) the preceding function RETURNED, or single-byte filler. */
         if (b1 == 0xc3 || b1 == 0xcb ||          /* ret / retf              */
             b1 == 0x90 || b1 == 0xcc) {          /* nop / int3 alignment    */
            return true;
         }
         /* (b) `ret imm16` (C2 iw). */
         if (b3 == 0xc2) { return true; }
         /* (c) tail jump: `jmp rel8` (EB cb) / `jmp rel32` (E9 cd). */
         if (b2 == 0xeb || b5 == 0xe9) { return true; }
         /* (d) `ud2` (0F 0B) — a no-return call's trap tail. */
         if (b2 == 0x0f && b1 == 0x0b) { return true; }
         /* (e) indirect tail jump `jmp r/m32`, register form (FF E0..E7). */
         if (b2 == 0xff && b1 >= 0xe0 && b1 <= 0xe7) { return true; }
         /* (f) MULTI-BYTE NOP alignment padding (many such encodings end in
          * 0x00, so they cannot be recognised from the last byte alone). */
         return nop_padding_ends_at(img, fo);
      }
      return false;
   }

   template <Bits bits>
   bool ParseEnv<bits>::code_alias_lacks_entry_evidence(const Image& img,
                                                        std::size_t vmaddr) const {
      static const bool disabled =
         std::getenv("M64_NO_CODE_ENTRY_GATE") != nullptr;
      if (disabled) { return false; }
      /* Exact complement of code_alias_is_constant: that gate owns the
       * locals-symboled case, this one the locals-stripped case. */
      if (have_local_text_syms) { return false; }
      for (Segment<bits> *seg : archive.segments()) {
         if (!seg->contains_vmaddr(vmaddr)) { continue; }
         for (Section<bits> *sec : seg->sections) {
            if (!sec->contains_vmaddr(vmaddr)) { continue; }
            if ((sec->sect.flags &
                 (S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS)) == 0) {
               return false;   /* not an instructions section */
            }
            return !code_target_has_entry_evidence(img, vmaddr);
         }
         return false;   /* in segment but between/outside sections */
      }
      return false;
   }

   template class ParseEnv<Bits::M32>;
   template class ParseEnv<Bits::M64>;

}
