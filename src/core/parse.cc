#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <execinfo.h>
#include <unistd.h>
#include "parse.hh"
#include "section_blob.hh"
#include "archive.hh"
#include "segment.hh"
#include "instruction.hh"

namespace MachO {

#if 0
   template <Bits bits>
   void ParseEnv<bits>::add(std::size_t vmaddr, const SectionBlob<bits> *pointee) {
      /* check for any unresolved pointers in todo map */
      auto todo_it = todo_map.find(vmaddr);
      if (todo_it != todo_map.end()) {
         for (const SectionBlob<bits> **pointer : todo_it->second) {
            *pointer = pointee;
         }
         todo_map.erase(todo_it);
      }

      /* add to addr map */
      addr_map.insert({vmaddr, pointee});
   }

   template <Bits bits>
   void ParseEnv<bits>::resolve(std::size_t vmaddr, const SectionBlob<bits> **pointer) {
      /* check if in addr map */
      auto addr_it = addr_map.find(vmaddr);
      if (addr_it != addr_map.end()) {
         *pointer = addr_it->second;
      } else {
         todo_map[vmaddr].push_back(pointer);
      }
   }

   template <Bits bits>
   void ParseEnv<bits>::add(const DylibCommand<bits> *dylib) {
      dylibs.insert({++dylib_id, dylib});
      auto todo_it = todo_dylibs.find(dylib_id);
      if (todo_it != todo_dylibs.end()) {
         for (const DylibCommand<bits> **ptr : todo_it->second) {
            *ptr = dylib;
         }
         todo_dylibs.erase(todo_it);
      }
   }

   template <Bits bits>
   void ParseEnv<bits>::resolve(std::size_t index, const DylibCommand<bits> **dylib) {
      auto it = dylibs.find(index);
      if (it == dylibs.end()) {
         todo_dylibs[index].push_back(dylib);
      } else {
         *dylib = it->second;
      }
   }
#endif

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

      static const char *trace_str = std::getenv("MACHO_TRACE_PLACEHOLDER");
      static const std::size_t trace_vmaddr = trace_str ? std::strtoull(trace_str, nullptr, 0) : 0;
      if (trace_vmaddr && vmaddr == trace_vmaddr) {
         fprintf(stderr, "add_placeholder(0x%zx) called; backtrace:\n", vmaddr);
         void *bt[20]; int n = backtrace(bt, 20);
         backtrace_symbols_fd(bt, n, STDERR_FILENO);
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
   bool ParseEnv<bits>::zerofill_target_unattested(std::size_t vmaddr) const {
      static const bool disabled =
         std::getenv("M64_NO_ZEROFILL_TARGET_GATE") != nullptr;
      if (disabled) { return false; }
      /* An exact nlist hit is positive evidence that an object really starts
       * here, so honour it. func_syms holds EVERY non-stab N_SECT symbol
       * (populated in the Symtab ctor), data symbols included. */
      if (func_syms.count(vmaddr) != 0) { return false; }
      for (Segment<bits> *seg : archive.segments()) {
         if (!seg->contains_vmaddr(vmaddr)) { continue; }
         for (Section<bits> *sec : seg->sections) {
            if (!sec->contains_vmaddr(vmaddr)) { continue; }
            const uint32_t stype = sec->sect.flags & SECTION_TYPE;
            /* Zero-fill only (__DATA,__bss and __DATA,__common). Every other
             * data target keeps the callers' existing treatment. */
            return stype == S_ZEROFILL || stype == S_GB_ZEROFILL;
         }
         return false;   /* in segment but between/outside sections */
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
