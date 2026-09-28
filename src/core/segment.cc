#include <cassert>
#include <cstdlib>
#include <sstream>

#include "segment.hh"
#include "util.hh"
#include "transform.hh"
#include "archive.hh"
#include "types.hh"
#include "section_blob.hh"
#include "dyldinfo.hh"
#include "linkedit.hh"
#include "data_in_code.hh"
#include "symtab.hh"


namespace MachO {

   template <Bits bits>
   Segment<bits> *Segment<bits>::Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env) {
      const char *segname = img.at<segment_command_t<bits>>(offset).segname;
      
      if (strcmp(segname, SEG_LINKEDIT) == 0) {
         return Segment_LINKEDIT<bits>::Parse(img, offset, env);
      } else {
         return new Segment(img, offset, env);
      }
   }
   
   
   template <Bits bits>
   Segment<bits>::Segment(const Image& img, std::size_t offset, ParseEnv<bits>& env):
      LoadCommand<bits>(img, offset, env)
   {
      segment_command = img.at<segment_command_t<bits>>(offset);

      env.current_segment = this;
      
      offset += sizeof(segment_command_t<bits>);
      for (int i = 0; i < segment_command.nsects; ++i) {
         Section<bits> *section = Section<bits>::Parse(img, offset, env);
         sections.push_back(section);
         offset += section->size();
      }

      /* Capture the raw file payload of a sectionless segment (no Section owns
       * these bytes). __PAGEZERO and __LINKEDIT are special-cased elsewhere and
       * carry no verbatim payload here, so exclude them. */
      if (segment_command.nsects == 0 && segment_command.filesize > 0 &&
          strcmp(segment_command.segname, SEG_PAGEZERO) != 0 &&
          strcmp(segment_command.segname, SEG_LINKEDIT) != 0) {
         const uint8_t *src =
            &img.template at<uint8_t>(segment_command.fileoff);
         raw_data.assign(src, src + segment_command.filesize);
      }

      env.current_segment = nullptr;
   }

   template <Bits bits>
   Segment<bits>::~Segment() {
      for (Section<bits> *ptr : sections) {
         delete ptr;
      }
   }   

   template <Bits bits>
   std::size_t Segment<bits>::size() const {
      return sizeof(segment_command) + sections.size() * Section<bits>::size();
   }

   template <Bits bits>
   Section<bits> *Segment<bits>::section(const std::string& name) {
      for (Section<bits> *section : sections) {
         if (section->name() == name) {
            return section;
         }
      }
      return nullptr;
   }

   template <Bits bits>
   std::string Segment<bits>::name() const {
      return std::string(segment_command.segname, strnlen(segment_command.segname,
                                                          sizeof(segment_command.segname)));
   }

   template <Bits bits>
   void Segment<bits>::Build(BuildEnv<bits>& env) {
      assert(env.loc.vmaddr % PAGESIZE == 0);
      segment_command.nsects = sections.size();
      segment_command.cmdsize =
         sizeof(segment_command) + Section<bits>::size() * segment_command.nsects;
      
      if (strcmp(segment_command.segname, SEG_PAGEZERO) == 0) {
         Build_PAGEZERO(env);
         return;
      }

      /* set segment start location */
      if (strcmp(segment_command.segname, SEG_TEXT) == 0) {
         /* __TEXT must cover the mach_header + load commands (fileoff=0).
          * vmaddr advances by env.loc.offset so the first section's
          * vmaddr - fileoff matches segment.vmaddr - segment.fileoff (=segment.vmaddr). */
         env.loc.vmaddr = align_up(env.loc.vmaddr, PAGESIZE);
         segment_command.fileoff = 0;
         segment_command.vmaddr = env.loc.vmaddr;
         env.loc.vmaddr += env.loc.offset;
      } else {
         env.newsegment();
         segment_command.fileoff = env.loc.offset;
         segment_command.vmaddr = env.loc.vmaddr;
      }
      
      /* Build the sections. When the segment transitions from file-backed to
       * ZEROFILL (__bss/__common), page-align the vmaddr AND advance the file
       * offset cursor to the same page boundary ONCE, at the first zerofill
       * section. Rationale: the segment's filesize is derived below from the
       * (page-rounded) file-offset cursor. If the first zerofill section began at
       * a non-page-aligned vmaddr immediately after a file-backed section (e.g.
       * Halo's synthesized __86x64_abs32 table ends mid-page right before __bss),
       * that page round-up spilled up to a page of FILE backing INTO the zerofill
       * vmaddr — so __bss/__common bytes were mapped from the file instead of
       * anonymous zero-fill. On Halo that placed a C++ static-init RUN-ONCE GUARD
       * FLAG at __bss[0] on a file page (a latent non-zero-guard hazard). Zerofill
       * sections advance vmaddr but not the offset cursor, so BEFORE any zerofill
       * the two cursors track with a constant segment skew (vmaddr - offset ==
       * segment.vmaddr - segment.fileoff); that equality pinpoints the first
       * zerofill. Aligning both cursors there makes the file-backed extent end
       * EXACTLY on the zerofill page boundary, so every zerofill byte is genuine
       * zero-fill and the page-rounded filesize no longer overlaps it. The offset
       * bump is a hole the emitter zero-pads; no file-backed section moves (they
       * were already laid out). */
      const std::size_t seg_skew = segment_command.vmaddr - segment_command.fileoff;
      bool zf_aligned = false;
      for (Section<bits> *sect : sections) {
         const uint32_t stype = sect->sect.flags & SECTION_TYPE;
         const bool is_zf = (stype == S_ZEROFILL || stype == S_GB_ZEROFILL ||
                             stype == S_THREAD_LOCAL_ZEROFILL);
         if (is_zf && !zf_aligned &&
             (env.loc.vmaddr - env.loc.offset) == seg_skew) {
            /* first zerofill, cursors still in sync -> snap both to a page */
            env.loc.vmaddr = align_up(env.loc.vmaddr, PAGESIZE);
            env.loc.offset = align_up(env.loc.offset, PAGESIZE);
            zf_aligned = true;
         }
         sect->Build(env);
      }

      /* Lay out a sectionless segment's verbatim payload so the offset/vmaddr
       * counters advance and fileoff/filesize remain self-consistent. */
      if (!raw_data.empty()) {
         env.loc.offset += raw_data.size();
         env.loc.vmaddr += raw_data.size();
      }

      /* post-conditions for vmaddr */
      env.loc.vmaddr = align_up(env.loc.vmaddr, PAGESIZE);
      env.loc.offset = align_up(env.loc.offset, PAGESIZE); /* experimental! */

      segment_command.filesize = env.loc.offset - segment_command.fileoff;
      segment_command.vmsize = align_up<size_t>(env.loc.vmaddr - segment_command.vmaddr, PAGESIZE);
   }

   template <Bits bits>
   void Segment_LINKEDIT<bits>::Build(BuildEnv<bits>& env) {
      this->segment_command.cmdsize = sizeof(segment_command_t<bits>);
      this->segment_command.nsects = 0;
      this->segment_command.fileoff = env.loc.offset = align_up(env.loc.offset, PAGESIZE);
      this->segment_command.vmaddr = env.loc.vmaddr = align_up(env.loc.vmaddr, PAGESIZE);
      
      // linkedits = env.archive->template subcommands<LinkeditCommand<bits>>();

      /* LC_DYLD_INFO[_ONLY] */
      auto dyld_info = env.archive->template subcommand<DyldInfo>();
      if (dyld_info) { dyld_info->Build_LINKEDIT(env); }

      /* LC_FUNCTION_STARTS */
      auto function_starts = env.archive->template subcommand<FunctionStarts>();
      if (function_starts) { function_starts->Build_LINKEDIT(env); }

      /* LC_DATA_IN_CODE */
      auto data_in_code = env.archive->template subcommand<DataInCode>();
      if (data_in_code) { data_in_code->Build_LINKEDIT(env); }

      /* Opaque blob linkedit commands placed AFTER data_in_code matches
       * Apple's actual __LINKEDIT order observed in linker outputs. Their
       * dataoff/datasize from i386 input become stale; Build_LINKEDIT recomputes.
       * (Seen on QuickTime, which carries an LC_SEGMENT_SPLIT_INFO.) */
      for (auto opaque : env.archive->template subcommands<OpaqueLinkeditBlob>()) {
         opaque->Build_LINKEDIT(env);
      }

      /* LC_SYMTAB: symbol table */
      auto symtab = env.archive->template subcommand<Symtab>();
      if (symtab) { symtab->Build_LINKEDIT_symtab(env); }

      /* LC_DYSYMTAB */
      auto dysymtab = env.archive->template subcommand<Dysymtab>();
      if (dysymtab) { dysymtab->Build_LINKEDIT(env); }

      /* LC_SYMTAB: string table */
      if (symtab) { symtab->Build_LINKEDIT_strtab(env); }

      /* LC_CODE_SIGNATURE */
      auto code_signature = env.archive->template subcommand<CodeSignature>();
      if (code_signature) { code_signature->Build_LINKEDIT(env); }

      
      for (LinkeditCommand<bits> *linkedit : linkedits) {
         linkedit->Build_LINKEDIT(env);
      }
      
      env.loc.vmaddr = align_up(env.loc.vmaddr, PAGESIZE);

      this->segment_command.filesize = env.loc.offset - this->segment_command.fileoff;
      this->segment_command.vmsize = align_up<std::size_t>(this->segment_command.filesize,
                                                           PAGESIZE);

      /* The Build_LINKEDIT helpers advance env.loc.offset (file) but not
       * env.loc.vmaddr, so leave the vmaddr cursor parked past this segment's
       * VM extent. __LINKEDIT is usually the last segment, so this is normally
       * inert; but when another segment follows it in the load-command list
       * (e.g. a trailing sectionless __OINK), the next segment would otherwise
       * be assigned __LINKEDIT's own vmaddr and ld rejects the VM overlap.
       * Maintain the same post-condition every other Segment::Build upholds. */
      env.loc.vmaddr = align_up<std::size_t>(this->segment_command.vmaddr +
                                             this->segment_command.vmsize, PAGESIZE);
   }

   template <Bits bits>
   void Segment<bits>::Build_PAGEZERO(BuildEnv<bits>& env) {
      segment_command.vmaddr = 0;
      segment_command.vmsize = env.loc.vmaddr;
      segment_command.fileoff = 0;
      segment_command.filesize = 0;
   }

   template <Bits bits>
   void Segment<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<segment_command_t<bits>>(offset) = segment_command;
      offset += sizeof(segment_command_t<bits>);

      for (const Section<bits> *sect : sections) {
         sect->Emit(img, offset);
         offset += sect->size();
      }

      /* Write back a sectionless segment's verbatim payload at its fileoff,
       * then zero-fill the page-alignment tail so the file actually spans the
       * full declared filesize. Otherwise the file ends at the payload's real
       * end while the load command claims a (page-rounded) larger filesize,
       * and ld rejects it with "content extends beyond end of file". */
      if (!raw_data.empty()) {
         img.copy(segment_command.fileoff, raw_data.begin(), raw_data.size());
         const std::size_t declared = segment_command.filesize;
         if (declared > raw_data.size()) {
            img.memset(segment_command.fileoff + raw_data.size(), 0,
                       declared - raw_data.size());
         }
      }

      // fprintf(stderr, "[EMIT] segment={name=%s,fileoff=0x%zx,filesize=0x%zx,vmaddr=0x%zx,vmsize=0x%zx}\n", segment_command.segname, (std::size_t) segment_command.fileoff, (size_t) segment_command.filesize, (size_t) segment_command.vmaddr, (size_t) segment_command.vmsize);
   }

   template <Bits bits>
   Segment<bits>::Segment(const Segment<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
      LoadCommand<bits>(other, env), id(0)
   {
      env(other.segment_command, segment_command);

      /* Carry verbatim sectionless-segment payload across the bitness change.
       * These bytes are opaque data (no instructions / no relocations), so they
       * pass through unchanged. */
      raw_data = other.raw_data;

      for (const auto other_section : other.sections) {
         sections.push_back(other_section->Transform(env));
      }
   }

   template <Bits bits>
   void Segment<bits>::insert(SectionBlob<bits> *blob, const Location& loc, Relation rel) {
      std::size_t locval;
      if (loc.offset) {
         locval = loc.offset;
      } else if (loc.vmaddr) {
         locval = loc.vmaddr;
      } else {
         throw std::invalid_argument("vmaddr and offsets are both 0");
      }

      auto sectloc = [=] (const Section<bits> *section) -> std::size_t {
                        if (loc.offset) {
                           return section->sect.offset;
                        } else {
                           return section->sect.addr;
                        }

                     };
      
      for (Section<bits> *section : sections) {
         std::size_t start = sectloc(section);
         if (start <= locval && locval < start + section->sect.size) {
            blob->segment = this;
            section->insert(blob, loc, rel);
            return;
         }
      }

      throw std::invalid_argument("location not found");
   }

   template <Bits bits>
   std::size_t Segment<bits>::offset_to_vmaddr(std::size_t offset) const {
      for (Section<bits> *section : sections) {
         if (offset >= section->sect.offset &&
             offset < section->sect.offset + section->sect.size) {
            return offset - (std::size_t) section->sect.offset + (std::size_t) section->sect.addr;
         }
      }
      throw std::invalid_argument(std::string("offset ") + std::to_string(offset) +
                                  " not in any section");
   }

   template <Bits bits>
   std::optional<std::size_t> Segment<bits>::try_offset_to_vmaddr(std::size_t offset) const {
      for (Section<bits> *section : sections) {
         if (section->contains_offset(offset)) {
            return offset - section->sect.offset + section->sect.addr;
         }
      }
      return std::nullopt;
   }

   template <Bits bits>
   bool Segment<bits>::contains_vmaddr(std::size_t vmaddr) const {
      return vmaddr >= segment_command.vmaddr &&
         vmaddr < segment_command.vmaddr + segment_command.vmsize;
   }

   template <Bits bits>
   bool Segment<bits>::contains_offset(std::size_t offset) const {
      return offset >= segment_command.fileoff &&
         offset < segment_command.fileoff + segment_command.filesize;
   }

   template <Bits bits>
   void Segment<bits>::AssignID(BuildEnv<bits>& env) {
      id = env.segment_counter();
      for (auto section : sections) {
         section->AssignID(env);
      }
   }

   template <Bits bits>
   void Segment<bits>::Parse2(ParseEnv<bits>& env) {
      env.current_segment = this;
      for (auto section : sections) {
         section->Parse2(env);
      }
      env.current_segment = nullptr;


   }

   template class Segment<Bits::M32>;
   template class Segment<Bits::M64>;

}
