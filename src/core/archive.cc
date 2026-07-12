#include <cstdlib>
#include <sstream>
#include <stdexcept>

#include <algorithm>
#include <vector>
#include <map>
#include <cstdint>
#include <mach-o/nlist.h>

#include "archive.hh"
#include "parse.hh"
#include "build.hh"
#include "segment.hh"
#include "types.hh"
#include "section_blob.hh"
#include "instruction.hh" // Instruction (inject_pcmap_section)
#include "section.hh"     // Section::Synthetic / iterate sections
#include "symtab.hh"   // Dysymtab (inject_xrel_section)
#include "dyldinfo.hh" // DyldInfo (classic-image gate)
#include "rebase_info.hh"  // RebaseInfo (synthesize_dyld_info)
#include "export_info.hh"  // ExportInfo / RegularExportNode (synthesize_dyld_info)
#include "lc.hh"           // DylibCommand (synthesize_dyld_info)

namespace MachO {

   template <Bits b>
   Archive<b>::Archive(const Image& img, std::size_t offset):
      header(img.at<mach_header_t<b>>(offset))
   {
      ParseEnv<b> env(*this);
      offset += sizeof(header);
      for (int i = 0; i < header.ncmds; ++i) {
         /* Advance by the LC header's stored cmdsize, not by cmd->size().
          * size() recomputes from the parsed string length and may disagree
          * with the file's cmdsize if a tool (e.g. an in-place install_name
          * patch) shortened a path but kept the same cmdsize via null
          * padding. Trusting the file's cmdsize keeps subsequent LCs at
          * their actual on-disk offsets. */
         const std::size_t lc_start = offset;
         const auto& lc_hdr = img.at<load_command>(lc_start);
         const std::size_t lc_cmdsize = lc_hdr.cmdsize;
         LoadCommand<b> *cmd = LoadCommand<b>::Parse(img, offset, env);
         if (cmd) { load_commands.push_back(cmd); }   /* nullptr = intentionally dropped (e.g. LC_TWOLEVEL_HINTS) */
         offset = lc_start + lc_cmdsize;
      }

      /*
       * Adopt the input binary's existing vmaddr base instead of forcing
       * the default `vmaddr_start<b>`. Picking it from the first non-
       * __PAGEZERO segment preserves the base across multi-step toolchain
       * passes (transform → modify → convert) so internal pointer values
       * baked into __data by the transform step don't get truncated when a
       * later step rebuilds at a different base.
       */
      for (LoadCommand<b> *cmd : load_commands) {
         auto seg = dynamic_cast<Segment<b> *>(cmd);
         if (seg == nullptr) continue;
         if (strcmp(seg->segment_command.segname, SEG_PAGEZERO) == 0) continue;
         vmaddr = seg->segment_command.vmaddr;
         break;
      }

      /*
       * Structural GCC PIC-thunk scan (i386 only). ParseEnv::pic_thunks is
       * otherwise populated BY NAME (`___i686.get_pc_thunk.<r>` nlists, Symtab
       * ctor) so that a `call thunk` whose thunk lives in a DIFFERENT text
       * section (GCC: thunks in __textcoal_nt, callers in __text) still
       * establishes the PIC anchor — the detectors' section-local byte-scans
       * only see their OWN section's blobs. A fully STRIPPED GCC binary (Halo:
       * 0 defined text nlists) defeats the name seed: the cross-section thunk
       * goes unrecognized, no anchor is established, and every anchored
       * `[ebx+disp32]` whose disp aliases a segment falls through to the
       * absolute-table rewrite `lea r11,[rip+target]; op [anchor_base+r11]` —
       * double-counting the base (fault at anchor+target; Halo.dylib+0x1e36).
       *
       * Fix on STRUCTURE, not names: scan every executable text section's raw
       * bytes for the canonical 4-byte thunk body
       *     8b {04,0c,14,1c,2c,34,3c} 24 c3   (mov (%esp),%r32 ; ret)
       * and record entry vmaddr -> GPR encoding. Semantically exact: CALLing
       * any address with this body places the return address in %r32 and
       * returns — i.e. it IS a get_pc_thunk regardless of symbols. Runs after
       * LC construction and before any Parse1 (order-safe: raw image bytes,
       * no parsed content needed). emplace() keeps name-seeded entries first
       * (values agree anyway). M32 only — the idiom is i386 PIC; M64 re-parses
       * (modify/convert) must not re-detect. reg==ESP (0x24) is excluded.
       */
      if constexpr (b == Bits::M32) {
         for (LoadCommand<b> *cmd : load_commands) {
            auto seg = dynamic_cast<Segment<b> *>(cmd);
            if (seg == nullptr) continue;
            if ((seg->segment_command.initprot & VM_PROT_EXECUTE) == 0) continue;
            for (Section<b> *sect : seg->sections) {
               if ((sect->sect.flags & (S_ATTR_PURE_INSTRUCTIONS |
                                        S_ATTR_SOME_INSTRUCTIONS)) == 0) continue;
               const uint32_t stype = sect->sect.flags & SECTION_TYPE;
               if (stype == S_ZEROFILL || stype == S_GB_ZEROFILL) continue;
               if (sect->sect.size < 4 || sect->sect.offset == 0) continue;
               const std::size_t off_end = sect->sect.offset + sect->sect.size - 3;
               for (std::size_t o = sect->sect.offset; o < off_end; ++o) {
                  if (img.at<uint8_t>(o) != 0x8b) continue;
                  const uint8_t modrm = img.at<uint8_t>(o + 1);
                  if ((modrm & 0xC7) != 0x04) continue;      /* mod=00 rm=100 */
                  const uint8_t reg = (modrm >> 3) & 0x07;
                  if (reg == 4) continue;                    /* esp: not a thunk */
                  if (img.at<uint8_t>(o + 2) != 0x24) continue; /* SIB: base=esp */
                  if (img.at<uint8_t>(o + 3) != 0xc3) continue; /* ret */
                  const std::size_t vm =
                     sect->sect.addr + (o - sect->sect.offset);
                  env.pic_thunks.emplace(vm, reg);
               }
            }
         }
      }

      for (LoadCommand<b> *cmd : load_commands) {
         cmd->Parse1(img, env);
      }

      env.do_resolve();

      for (LoadCommand<b> *cmd : load_commands) {
         cmd->Parse2(env);
      }

      /* remaining placeholder should go at end of sections */
      for (Section<b> *section : sections()) {
         auto placeholder_it = env.placeholders.begin();
         while (placeholder_it != env.placeholders.end()) {
            if (section->sect.addr + section->sect.size == placeholder_it->first) {
               section->content.push_back(placeholder_it->second);
               placeholder_it = env.placeholders.erase(placeholder_it);
            } else {
               ++placeholder_it;
            }
         }
      }

      if (!env.placeholders.empty()) {
         /* Stranded placeholders represent addresses the parser thought might
          * be pointers (instruction memdisp targets, nlist values, etc.) but
          * for which no parsed blob exists at that vmaddr. Common origins:
          *   - XED-decoded instruction bytes that are actually data
          *     (jump tables, constants between functions) → garbage memdisp
          *   - Values that land in __LINKEDIT (opaque metadata, no section
          *     blobs)
          *   - Cross-segment addresses computed from translator-emitted code
          *     that no longer correspond to a real blob after layout shift
          * The originating Immediate keeps its raw value (pointee=nullptr)
          * and emits the bytes verbatim. dyld doesn't resolve via parsed
          * blobs; it uses LC_DYLD_INFO bind/rebase, which is unaffected.
          * So stranded placeholders are non-fatal — warn and continue. Was:
          * threw std::logic_error, which blocked modify on iPhoto run23. */
         static const bool verbose = std::getenv("MACHO_PARSE_VERBOSE") != nullptr;
         if (verbose) {
            for (const auto& p : env.placeholders) {
               std::size_t vmaddr = p.first;
               const char *where = "outside any segment";
               for (const Segment<b> *seg : segments()) {
                  if (vmaddr >= seg->segment_command.vmaddr &&
                      vmaddr <  seg->segment_command.vmaddr + seg->segment_command.vmsize) {
                     where = seg->segment_command.segname;
                     break;
                  }
               }
               fprintf(stderr, "stranded placeholder vmaddr=0x%zx (%s)\n", vmaddr, where);
            }
         }
         if (env.placeholders.size() > 0) {
            fprintf(stderr,
                    "warning: %zu stranded placeholder(s) at parse; "
                    "originating Immediate values preserved verbatim "
                    "(set MACHO_PARSE_VERBOSE=1 for per-placeholder addresses).\n",
                    env.placeholders.size());
         }
         env.placeholders.clear();
      }

      env.do_resolve();
      /* Containing-blob fallback for absolute data refs that land mid-blob
       * (exact-key resolve missed). Must run after do_resolve (so exact wins
       * where possible) and after all blobs are registered. */
      env.vmaddr_resolver.do_resolve_containing();
   }

   template <Bits b>
   Archive<b>::~Archive() {
      for (LoadCommand<b> *lc : load_commands) {
         delete lc;
      }
   }

   template <Bits b>
   Segment<b> *Archive<b>::segment(const std::string& name) {
      for (Segment<b> *seg : segments()) {
         if (name == seg->name()) {
            return seg;
         }
      }
      return nullptr;
   }

   template <Bits b>
   std::size_t Archive<b>::Build(std::size_t offset) {
      /* Divert 4-byte __DATA,__const RTTI/vtable binds out of the (8-byte) dyld
       * bind streams and into the Dysymtab's xrel_entries FIRST, so the
       * inject_xrel_section below picks them up. Without this, dyld's 8-byte
       * write into a 4-byte typeinfo vtable slot clobbers the adjacent __name
       * field (Civ IV boost.python NULL-strcmp). M64-only, no-op if no such
       * binds. */
      divert_narrow_const_binds_to_xrel();

      /* Inject our runtime-bind metadata section (classic external relocs that
       * dyld can't process + the diverted narrow __const binds above) before
       * laying out, so it shares __DATA's layout and its XrelBlob::Emit can read
       * each slot blob's resolved vmaddr. M64-only, no-op when there are no
       * lifted relocs. */
      inject_xrel_section();

      /* Emit the C++ exception PC map + per-function LSDA table (M64 only; inert
       * for binaries with no __eh_frame/LSDA; idempotent). Like inject_xrel
       * these add __DATA sections, so they must run before the segment/section
       * accounting below. */
      inject_pcmap_section();
      inject_ehlsda_section();

      /* Emit the exact runtime abs32-slide site table (M64 only; idempotent —
       * a reparse carries the section as a live re-resolving Abs32Blob). Must
       * also run before the layout accounting below. */
      inject_abs32_section();

      /* Manufacture a modern LC_DYLD_INFO_ONLY for a classic image (opt-in via
       * convert --synthesize-dyld-info). Runs AFTER inject_xrel_section so the
       * 4-byte external-reloc RTTI slots still get their runtime __86x64_xrel
       * binding (that section is bound by libabiconv, not dyld, and covers a
       * disjoint slot set from the 8-byte symbol-pointer binds synthesized
       * here). Adds the DyldInfo load command, so it must precede the ncmds /
       * sizeofcmds accounting below. */
      synthesize_dyld_info();

      /* Enforce the "__LINKEDIT is the last segment" invariant before laying
       * out file offsets. codesign/dyld append the code-signature superblob at
       * the end of __LINKEDIT and require it to cover the whole image, so
       * __LINKEDIT must hold the highest file offset of any segment. dyld's
       * strict validation additionally rejects a binary whose segment
       * load-command order disagrees with its file/vmaddr order ("segment load
       * commands out of order with respect to layout", MachOFile::validSegments
       * / Malformed::segmentOrder). So we move the __LINKEDIT *load command*
       * itself to sit after every other segment -- not merely bump its file
       * offset -- so the Build cursor below and Emit both place it last in
       * load-command, file, and vmaddr order. Without this, a binary that
       * carries a custom segment trailing __LINKEDIT (e.g. Civ IV's Firaxis
       * __OINK, a 93740-byte opaque sectionless payload) is unsignable:
       * codesign reports "main executable failed strict validation".
       *
       * Universal: triggers on the structural property (a non-__LINKEDIT
       * segment laid out after __LINKEDIT), never on an app name. Idempotent --
       * a no-op once __LINKEDIT is already the last segment, so binaries with
       * the normal layout are unaffected (and byte-stable). Moving only the
       * sectionless __LINKEDIT past trailing segments leaves every section's
       * index (n_sect) unchanged, since section IDs are assigned in
       * load-command order and the section-bearing segments keep their relative
       * positions. */
      if (Segment<b> *linkedit = segment(SEG_LINKEDIT)) {
         Segment<b> *last_seg = nullptr;
         for (LoadCommand<b> *lc : load_commands) {
            if (auto *seg = dynamic_cast<Segment<b> *>(lc)) { last_seg = seg; }
         }
         if (last_seg != linkedit) {
            /* unlink __LINKEDIT from its current slot */
            for (auto it = load_commands.begin(); it != load_commands.end(); ++it) {
               if (*it == linkedit) { load_commands.erase(it); break; }
            }
            /* re-insert immediately after the (now) last segment command */
            std::size_t insert_idx = 0;
            for (std::size_t i = 0; i < load_commands.size(); ++i) {
               if (dynamic_cast<Segment<b> *>(load_commands[i])) { insert_idx = i + 1; }
            }
            load_commands.insert(load_commands.begin() + insert_idx, linkedit);
         }
      }

      BuildEnv<b> env(this, Location(offset, vmaddr));
      
      env.allocate(sizeof(header));
      
      /* count number of load commands */
      header.ncmds = load_commands.size();

      /* compute size of commands */
      header.sizeofcmds = 0;
      for (LoadCommand<b> *lc : load_commands) {
         header.sizeofcmds += lc->size();
      }

      env.allocate(header.sizeofcmds);

      /* Header padding between the end of the load commands and the first
       * section. Two sources, in priority order:
       *
       *  1. header_size_target (set by the `change-deps` dependency-path
       *     rewriter): reserve exactly enough padding that the first section
       *     keeps its ORIGINAL file offset, so no code/data vmaddr moves. This
       *     makes a load-command string edit byte-stable everywhere except the
       *     rewritten strings -- required because re-parsing a translated binary
       *     leaves some absolute __data pointers as verbatim values that would
       *     be stale if the layout shifted. Since such a rewrite only ever
       *     shrinks dependency paths (/System/... -> @rpath/...), the freed
       *     load-command bytes are absorbed back into this pad.
       *
       *  2. MACHO_HEADERPAD=N env var (legacy): reserve N bytes of slack so a
       *     later install_name_tool -add_rpath / -change can grow a path without
       *     "load commands do not fit". Apple's linker reserves 32 bytes by
       *     default (1024 with -headerpad_max_install_names). 0 by default to
       *     keep regressions byte-stable with prior outputs. */
      std::size_t headerpad = 0;
      const std::size_t header_used = sizeof(header) + header.sizeofcmds;
      if (header_size_target > 0) {
         if (header_size_target >= header_used) {
            headerpad = header_size_target - header_used;
         } else {
            fprintf(stderr,
                    "warning: rewritten load commands (%zu bytes) exceed the original "
                    "header region (%zu bytes); first section will shift and absolute "
                    "data pointers may go stale\n",
                    header_used, header_size_target);
         }
      } else if (const char *hp_env = std::getenv("MACHO_HEADERPAD")) {
         headerpad = std::strtoull(hp_env, nullptr, 0);
      }
      if (headerpad > 0) {
         env.allocate(headerpad);
      }

      /* assign IDs */
      for (LoadCommand<b> *lc : load_commands) {
         lc->AssignID(env);
      }
      
      /* build each command */
      static const bool build_debug = std::getenv("MACHO_BUILD_DEBUG") != nullptr;
      for (size_t i = 0; i < load_commands.size(); ++i) {
         LoadCommand<b> *lc = load_commands[i];
         if (build_debug) {
            fprintf(stderr,
                    "Build: lc[%zu] cmd=0x%x size=%u\n",
                    i, (unsigned)lc->cmd(), (unsigned)lc->size());
         }
         lc->Build(env);
      }

      total_size = env.loc.offset - offset;
      return total_size;
   }

   /* Insert a synthesized section BEFORE any zerofill section in the segment.
    * Zerofill (__bss/__common) must be the segment's vmaddr TAIL: it occupies
    * no file bytes, so any file-backed section placed after it would break the
    * segment's linear file<->vm correspondence (offset delta != vmaddr delta)
    * AND overlay its file bytes onto the zerofill's mapped page — dyld maps
    * [fileoff, fileoff+filesize) contiguously, so the zerofill range inside
    * that span would read the later section's CONTENT instead of zeros
    * (observed: __86x64_pcmap rows appearing in __bss globals once zerofill
    * stopped advancing the file cursor). Mirrors the __jt_ptrs placement in
    * Dysymtab::synthesize_undef_jump_stubs. */
   template <Bits b>
   static void insert_section_before_zerofill(Segment<b> *seg, Section<b> *sect) {
      auto insert_it = seg->sections.end();
      for (auto it = seg->sections.begin(); it != seg->sections.end(); ++it) {
         const uint32_t st = (*it)->sect.flags & SECTION_TYPE;
         if (st == S_ZEROFILL || st == S_GB_ZEROFILL ||
             st == S_THREAD_LOCAL_ZEROFILL) { insert_it = it; break; }
      }
      seg->sections.insert(insert_it, sect);
   }

   template <Bits b>
   void Archive<b>::divert_narrow_const_binds_to_xrel() {
      if constexpr (b != Bits::M64) {
         return; /* M32 builds are intermediate; XrelBlob is M64-output only */
      } else {
         DyldInfo<b> *dyld = this->template subcommand<DyldInfo>();
         if (dyld == nullptr) { return; }

         auto *dysymtab = this->template subcommand<Dysymtab>();
         if (dysymtab == nullptr) { return; }

         const bool dbg = std::getenv("MACHO_BUILD_DEBUG") != nullptr;
         std::size_t moved = 0;

         /* A bind's target slot is a NARROW (4-byte) data slot when its resolved
          * blob is an Immediate (Immediate::size()==4 in both M32 and M64 — the
          * i386 __const RTTI/vtable pointer never widens). A dyld BIND_TYPE_
          * POINTER write is 8 bytes wide in x86_64, so binding such a slot writes
          * into the next 4 bytes too. That is only HARMFUL when the next 4-byte
          * field is itself a live pointer we need (the classic case: an i386
          * __class_type_info object {__vtable@0=external bind; __name@4=internal
          * pointer to __ZTS} — the 8-byte vtable bind zeroes __name -> NULL
          * typeid().name() -> boost::python strcmp SIGSEGV, Civ IV). When the
          * following 4 bytes hold an integer (e.g. a global block literal's
          * {isa@0=bind; flags@4=0x50000000 integer}) the wide write is benign and
          * the bind MUST stay in the dyld stream (native 8-byte ObjC isa; test
          * 37_objc_blocks). So we divert a narrow bind ONLY when the immediately-
          * following blob is a live 4-byte pointer that the wide write would
          * clobber.  Structural + universal (adjacency of two 4-byte pointer
          * fields), never an RTTI-name match. SymbolPointer slots
          * (__nl/__la_symbol_ptr) are genuine 8-byte pointers and never match.
          *
          * We test the +4 neighbour by CONTENT ORDER, not by loc.vmaddr: this
          * pass runs at the top of Build BEFORE the layout that assigns each
          * blob's final loc.vmaddr, so those addresses are stale here — but the
          * section's content list is already in address order and an Immediate is
          * always exactly 4 bytes (Immediate::size()), so the blob immediately
          * following `slot` in `content` is exactly the +4 field. */
         auto clobbers_adjacent_pointer = [&] (const SectionBlob<b> *slot) -> bool {
            const Section<b> *sect = slot->section;
            if (sect == nullptr) { return false; }
            const auto& content = sect->content;
            auto it = std::find(content.begin(), content.end(), slot);
            if (it == content.end()) { return false; }
            ++it;
            /* Skip zero-size marker blobs (Placeholder: add_placeholder emits one
             * at a symbol address that lands mid-section — e.g. the __ZTS name
             * symbol at the typeinfo __name field — and it occupies no bytes) to
             * reach the real +4 pointer field. */
            while (it != content.end() && (*it)->size() == 0) { ++it; }
            if (it == content.end()) { return false; }
            const SectionBlob<b> *next = *it;
            if (auto *im = dynamic_cast<const Immediate<b> *>(next)) {
               return im->pointee != nullptr;
            }
            if (dynamic_cast<const SymbolPointer<b> *>(next) != nullptr) { return true; }
            if (dynamic_cast<const JumpTableEntry<b> *>(next) != nullptr) { return true; }
            return false;
         };

         auto divert = [&] (auto& bindees) {
            for (auto it = bindees.begin(); it != bindees.end(); ) {
               auto *node = *it;
               const SectionBlob<b> *slot = node->blob;
               if (slot != nullptr &&
                   dynamic_cast<const Immediate<b> *>(slot) != nullptr &&
                   clobbers_adjacent_pointer(slot)) {
                  typename Dysymtab<b>::XrelEntry e;
                  e.slot = slot;
                  e.name = node->sym;                 /* linker name, leading '_' */
                  e.addend = static_cast<int32_t>(node->addend);
                  e.orig_vmaddr = slot->loc.vmaddr;   /* diagnostic */
                  dysymtab->xrel_entries.push_back(std::move(e));
                  it = bindees.erase(it);
                  ++moved;
               } else {
                  ++it;
               }
            }
         };

         if (dyld->bind      != nullptr) { divert(dyld->bind->bindees); }
         if (dyld->weak_bind != nullptr) { divert(dyld->weak_bind->bindees); }
         if (dyld->lazy_bind != nullptr) { divert(dyld->lazy_bind->bindees); }

         if (dbg) {
            fprintf(stderr, "divert_narrow_const_binds_to_xrel: moved %zu "
                    "4-byte __const bind(s) to xrel_entries\n", moved);
         }
      }
   }

   template <Bits b>
   void Archive<b>::inject_xrel_section() {
      if constexpr (b != Bits::M64) {
         return; /* M32 builds are intermediate; XrelBlob is M64-output only */
      } else {
         /* xrel_entries hold ONLY genuine 4-byte slots by construction: the
          * classic-image external-reloc lift (symtab.cc lift_external_relocs)
          * only runs for LC_DYSYMTAB-only images (4-byte pointer slots), and the
          * modern-image path (divert_narrow_const_binds_to_xrel, run just above)
          * only diverts binds whose target is a 4-byte Immediate. So we no longer
          * gate on the absence of a DyldInfo — a modern non-PIE C++ exec (Civ IV
          * Steam) legitimately carries a DyldInfo AND diverted narrow __const
          * binds, and both need the runtime __86x64_xrel section. An image with
          * no such slots leaves xrel_entries empty and this is a no-op. */
         auto *dysymtab = this->template subcommand<Dysymtab>();
         if (dysymtab == nullptr || dysymtab->xrel_entries.empty()) { return; }

         Segment<b> *data_seg = segment(SEG_DATA);
         if (data_seg == nullptr) { return; }

         /* idempotent: a reparse (modify/convert) of an already-translated dylib
          * carries the section as data but has no lifted relocs — and we never
          * want two. */
         for (Section<b> *s : data_seg->sections) {
            if (s->name() == "__86x64_xrel") { return; }
         }

         auto *blob = XrelBlob<b>::Create();
         std::size_t skipped = 0;
         for (const auto& e : dysymtab->xrel_entries) {
            if (e.slot == nullptr) { ++skipped; continue; } /* slot didn't resolve */
            typename XrelBlob<b>::Ent ent;
            ent.slot = e.slot;
            ent.addend = e.addend;
            ent.name_off = static_cast<uint32_t>(blob->strtab.size()); /* intra-strtab */
            blob->strtab.append(e.name);
            blob->strtab.push_back('\0');
            blob->ents.push_back(ent);
         }
         if (blob->ents.empty()) { delete blob; return; }

         /* Rebase name_off to the section start (header + entry table precede the
          * packed names), now that the entry count (hence header size) is final. */
         const uint32_t hdr = static_cast<uint32_t>(blob->header_size());
         for (auto& ent : blob->ents) { ent.name_off += hdr; }

         auto *sect = Section<b>::Synthetic(SEG_DATA, "__86x64_xrel",
                                            S_REGULAR, /*align=*/2);
         sect->segment = data_seg;
         sect->content.push_back(blob);
         blob->section = sect;
         blob->segment = data_seg;
         insert_section_before_zerofill(data_seg, sect);
         invalidate_segments_cache();

         if (std::getenv("MACHO_BUILD_DEBUG")) {
            fprintf(stderr, "inject_xrel_section: %zu entries (%zu unresolved "
                    "skipped) -> __DATA,__86x64_xrel\n", blob->ents.size(), skipped);
         }
      }
   }

   /* ---- C++ exception PC map + LSDA table (eh_shim.c) ---------------------- */
   namespace {
      struct CieInfo { bool has_aug=false, has_L=false, has_R=false; uint8_t L_enc=0, R_enc=0; };

      uint32_t ehbuf_le32(const std::vector<uint8_t>& b, size_t o) {
         return (uint32_t)b[o] | ((uint32_t)b[o+1]<<8) |
                ((uint32_t)b[o+2]<<16) | ((uint32_t)b[o+3]<<24);
      }
      uint16_t ehbuf_le16(const std::vector<uint8_t>& b, size_t o) {
         return (uint16_t)(b[o] | (b[o+1]<<8));
      }
      uint64_t eh_uleb(const std::vector<uint8_t>& b, size_t& o) {
         uint64_t r=0; int s=0; uint8_t c;
         do { c=b[o++]; r |= (uint64_t)(c&0x7f)<<s; s+=7; } while ((c&0x80) && o<b.size());
         return r;
      }
      int64_t eh_sleb(const std::vector<uint8_t>& b, size_t& o) {
         int64_t r=0; int s=0; uint8_t c;
         do { c=b[o++]; r |= (int64_t)(c&0x7f)<<s; s+=7; } while ((c&0x80) && o<b.size());
         if (s<64 && (c&0x40)) r |= -((int64_t)1<<s);
         return r;
      }
      int eh_enc_size(uint8_t enc) {       /* fixed-size encodings (i386 absptr=4) */
         switch (enc & 0x0f) {
         case 0x00: case 0x03: case 0x0b: return 4;   /* absptr/udata4/sdata4 */
         case 0x02: case 0x0a: return 2;              /* udata2/sdata2 */
         case 0x04: case 0x0c: return 8;              /* udata8/sdata8 */
         default: return -1;                          /* uleb/sleb -> variable */
         }
      }
      /* Decode an EH-encoded value; field_vmaddr = vmaddr of its first byte.
       * Returns false (skip) for omit / indirect / unsupported relativity. */
      bool eh_read_encoded(const std::vector<uint8_t>& b, size_t& o, uint8_t enc,
                           uint32_t field_vmaddr, uint32_t& out) {
         if (enc == 0xff) return false;               /* DW_EH_PE_omit */
         uint32_t raw;
         switch (enc & 0x0f) {
         case 0x00: case 0x03: raw = ehbuf_le32(b,o); o+=4; break;          /* absptr/udata4 */
         case 0x0b: raw = ehbuf_le32(b,o); o+=4; break;                     /* sdata4 (2's-comp) */
         case 0x02: raw = ehbuf_le16(b,o); o+=2; break;                     /* udata2 */
         case 0x0a: raw = (uint32_t)(int16_t)ehbuf_le16(b,o); o+=2; break;  /* sdata2 */
         case 0x01: raw = (uint32_t)eh_uleb(b,o); break;                    /* uleb */
         case 0x09: raw = (uint32_t)eh_sleb(b,o); break;                    /* sleb */
         default: return false;
         }
         if (enc & 0x80) return false;                /* indirect: can't deref here */
         switch (enc & 0x70) {
         case 0x10: out = field_vmaddr + raw; return true;   /* pcrel (raw 2's-comp) */
         case 0x00: out = raw; return true;                  /* absptr */
         default: return false;                              /* datarel/funcrel/etc. */
         }
      }
      /* Does a well-formed GCC LSDA header start at gbuf[off]?  Used to snap the
       * (drifted) FDE LSDA pointer to the real LSDA position. */
      bool eh_lsda_valid_at(const std::vector<uint8_t>& gbuf, size_t off) {
         if (off >= gbuf.size()) return false;
         size_t p = off;
         uint8_t lpenc = gbuf[p++];
         if (lpenc != 0xff && (lpenc & 0x0f) > 0x0c) return false;
         if (lpenc != 0xff) { int s = eh_enc_size(lpenc); if (s < 0) eh_uleb(gbuf, p); else p += s; }
         if (p >= gbuf.size()) return false;
         uint8_t ttenc = gbuf[p++];
         if (ttenc != 0xff) { eh_uleb(gbuf, p); }            /* @TType base offset */
         if (p >= gbuf.size()) return false;
         uint8_t csenc = gbuf[p++];
         if ((csenc & 0x0f) > 0x0c) return false;
         if (p >= gbuf.size()) return false;
         uint64_t cslen = eh_uleb(gbuf, p);
         if (p + cslen > gbuf.size()) return false;          /* table must fit */
         return cslen > 0;
      }
   } // namespace

   template <Bits b>
   void Archive<b>::collect_eh_lsda_pairs(const Archive<opposite<b>>& other) {
      if constexpr (b != Bits::M64) {
         (void)other; return;                         /* M32 output is intermediate */
      } else {
         eh_lsda_pairs.clear();
         const Section<opposite<b>> *ehf = nullptr, *gxt = nullptr;
         for (Segment<opposite<b>> *seg : other.segments()) {
            for (Section<opposite<b>> *s : seg->sections) {
               if (s->name() == "__eh_frame") { ehf = s; }
               else if (s->name() == "__gcc_except_tab") { gxt = s; }
            }
         }
         if (ehf == nullptr) { return; }              /* no exceptions -> inert */

         /* macho-tool's parse re-packs the source sections, so __eh_frame's base
          * lands a few bytes off from the original inter-section layout and its
          * self-relative (pcrel) FDE offsets are uniformly STALE in parse space
          * (the same staleness libabiconv sees at runtime).  We don't trust the
          * raw pcrel target; instead we SNAP each FDE's pc-begin to the nearest
          * real instruction start (a function entry, in the same parse vmaddr
          * space as SectionBlob::orig_vmaddr) and apply that per-FDE correction
          * to the LSDA pointer too (both are pcrel from adjacent __eh_frame
          * fields, so they share the offset).  The region used for the LSDA
          * offsets must be the TRUE function entry (the i386 `push ebp`, which is
          * a func_entry blob): in the source's disk-faithful M32 layout the
          * landing pad is exactly entry+cs_lp, even though the M64 transform
          * later drops the PIC get_pc_thunk and the entry/prologue themselves
          * aren't in the pcmap.  So snap pc-begin to the nearest func_entry. */
         /* (post-Build loc.vmaddr, disk-faithful orig_vmaddr) per func entry.  The
          * FDE pc-begin we parse is in (drifted) post-Build vmaddr space, so we
          * snap it to the nearest entry BY loc.vmaddr, then take that entry's
          * orig_vmaddr (the i386 layout coordinate) as the region. */
         std::vector<std::pair<uint32_t,uint32_t>> func_starts; /* (loc.vmaddr, orig) */
         for (Segment<opposite<b>> *seg : other.segments()) {
            for (Section<opposite<b>> *s : seg->sections) {
               const std::string sn = s->name();
               if (sn != "__text" && sn != "__textcoal_nt" && sn != "__coalesced")
                  continue;
               for (const SectionBlob<opposite<b>> *blob : s->content) {
                  if (blob->func_entry &&
                      dynamic_cast<const Instruction<opposite<b>> *>(blob) != nullptr)
                     func_starts.emplace_back((uint32_t) blob->loc.vmaddr,
                                              (uint32_t) blob->orig_vmaddr);
               }
            }
         }
         std::sort(func_starts.begin(), func_starts.end());

         /* Reconstruct the i386 __eh_frame bytes from its blobs (DataParser emits
          * 4-byte Immediates on aligned slots + DataBlob tails; M32 blobs still
          * hold the original pre-transform values, so the bytes are exact). */
         const uint32_t base = (uint32_t) ehf->loc().vmaddr;
         std::vector<uint8_t> buf;
         bool clean = true;
         for (const SectionBlob<opposite<b>> *blob : ehf->content) {
            const uint32_t v = (uint32_t) blob->loc.vmaddr;
            const size_t off = v - base;
            if (auto *im = dynamic_cast<const Immediate<opposite<b>> *>(blob)) {
               if (off + 4 > buf.size()) { buf.resize(off + 4, 0); }
               const uint32_t val = im->value;
               buf[off+0]=val; buf[off+1]=val>>8; buf[off+2]=val>>16; buf[off+3]=val>>24;
            } else if (auto *db = dynamic_cast<const DataBlob<opposite<b>> *>(blob)) {
               if (off + 1 > buf.size()) { buf.resize(off + 1, 0); }
               buf[off] = db->data;
            } else {
               clean = false; break;                  /* unexpected blob -> bail safely */
            }
         }
         if (!clean) { return; }

         /* Reconstruct the __gcc_except_tab bytes too, so the LSDA pointer can be
          * snapped to a position whose LSDA header actually parses (the FDE
          * pcrel drift means the raw pointer can be a few bytes off). */
         const uint32_t gxt_base = gxt ? (uint32_t) gxt->loc().vmaddr : 0;
         std::vector<uint8_t> gbuf;
         if (gxt) {
            for (const SectionBlob<opposite<b>> *blob : gxt->content) {
               const uint32_t v = (uint32_t) blob->loc.vmaddr;
               const size_t off = v - gxt_base;
               if (auto *im = dynamic_cast<const Immediate<opposite<b>> *>(blob)) {
                  if (off + 4 > gbuf.size()) { gbuf.resize(off + 4, 0); }
                  const uint32_t val = im->value;
                  gbuf[off+0]=val; gbuf[off+1]=val>>8; gbuf[off+2]=val>>16; gbuf[off+3]=val>>24;
               } else if (auto *db = dynamic_cast<const DataBlob<opposite<b>> *>(blob)) {
                  if (off + 1 > gbuf.size()) { gbuf.resize(off + 1, 0); }
                  gbuf[off] = db->data;
               }
            }
         }
         std::map<size_t, CieInfo> cies;
         size_t i = 0;
         while (i + 4 <= buf.size()) {
            const uint32_t len = ehbuf_le32(buf, i);
            if (len == 0) { break; }                  /* terminator */
            const size_t after_len = i + 4;
            const size_t rec_end = after_len + len;
            if (rec_end > buf.size()) { break; }
            const uint32_t id = ehbuf_le32(buf, after_len);
            if (id == 0) {                             /* CIE */
               CieInfo ci;
               size_t p = after_len + 4;
               const uint8_t version = buf[p++];
               std::string aug;
               while (p < rec_end && buf[p] != 0) { aug.push_back((char)buf[p++]); }
               if (p < rec_end) { p++; }               /* skip NUL */
               ci.has_aug = (!aug.empty() && aug[0] == 'z');
               eh_uleb(buf, p);                        /* code alignment factor */
               eh_sleb(buf, p);                        /* data alignment factor */
               if (version == 1) { p++; } else { eh_uleb(buf, p); } /* return reg */
               if (ci.has_aug) {
                  eh_uleb(buf, p);                     /* augmentation data length */
                  for (size_t k = 1; k < aug.size(); k++) {
                     switch (aug[k]) {
                     case 'L': ci.L_enc = buf[p++]; ci.has_L = true; break;
                     case 'R': ci.R_enc = buf[p++]; ci.has_R = true; break;
                     case 'P': { uint8_t pe = buf[p++]; int sz = eh_enc_size(pe);
                                 if (sz < 0) { eh_uleb(buf, p); } else { p += sz; } break; }
                     case 'S': break;                  /* signal frame, no data */
                     default: break;
                     }
                  }
               }
               cies[i] = ci;
            } else {                                   /* FDE */
               const size_t cie_off = (id <= after_len) ? after_len - id : SIZE_MAX;
               auto it = cies.find(cie_off);
               if (it != cies.end() && it->second.has_R) {
                  const CieInfo& ci = it->second;
                  size_t p = after_len + 4;            /* after CIE pointer */
                  uint32_t pcbegin = 0;
                  if (eh_read_encoded(buf, p, ci.R_enc, base + (uint32_t)p, pcbegin)) {
                     int rsz = eh_enc_size(ci.R_enc);  /* skip pc-range */
                     if (rsz < 0) { eh_uleb(buf, p); } else { p += rsz; }
                     if (ci.has_aug) {
                        eh_uleb(buf, p);               /* FDE aug data length */
                        if (ci.has_L && gxt != nullptr) {
                           uint32_t lsda = 0;
                           if (eh_read_encoded(buf, p, ci.L_enc, base + (uint32_t)p, lsda) &&
                               lsda != 0) {
                              /* (1) region = disk-faithful orig of the func_entry
                               * nearest (by post-Build vmaddr) to the drifted
                               * pc-begin — the i386 function start the LSDA
                               * offsets are relative to. */
                              uint32_t region = 0; int64_t bestd = INT64_MAX;
                              for (const auto& fs : func_starts) {
                                 int64_t d = (int64_t)fs.first - (int64_t)pcbegin;
                                 if (d < 0) d = -d;
                                 if (d < bestd) { bestd = d; region = fs.second; }
                              }
                              /* (2) lsda_off = nearest position to the drifted
                               * LSDA pointer whose LSDA header actually parses. */
                              int32_t lsda_off = -1;
                              const int32_t want = (int32_t)(lsda - gxt_base);
                              /* Two passes: first only accept the nearest position
                               * whose @LPStart-encoding is DW_EH_PE_omit (0xff) —
                               * the overwhelmingly common GCC/clang form and a
                               * strong anchor that rejects a false-positive parse a
                               * few bytes inside the previous LSDA's type table;
                               * fall back to any valid header otherwise. */
                              for (int pass = 0; pass < 2 && lsda_off < 0; pass++) {
                                 for (int32_t r = 0; r <= 16 && lsda_off < 0; r++) {
                                    for (int sgn = 0; sgn < 2 && lsda_off < 0; sgn++) {
                                       int32_t cand = want + (sgn ? -r : r);
                                       if (cand < 0 || (size_t)cand >= gbuf.size()) continue;
                                       if (!eh_lsda_valid_at(gbuf, (size_t)cand)) continue;
                                       if (pass == 0 && gbuf[cand] != 0xff) continue;
                                       lsda_off = cand;
                                    }
                                 }
                              }
                              if (bestd <= 96 && lsda_off >= 0) {
                                 eh_lsda_pairs.emplace_back((size_t)region,
                                                            (size_t)(uint32_t)lsda_off);
                              }
                           }
                        }
                     }
                  }
               }
            }
            i = rec_end;
         }
         if (std::getenv("MACHO_BUILD_DEBUG")) {
            fprintf(stderr, "collect_eh_lsda_pairs: %zu FDE LSDA pairs\n",
                    eh_lsda_pairs.size());
            for (const auto& pr : eh_lsda_pairs) {
               fprintf(stderr, "  pc-begin(orig_func)=0x%zx lsda_off=0x%zx\n",
                       pr.first, pr.second);
            }
         }
      }
   }

   template <Bits b>
   void Archive<b>::inject_pcmap_section() {
      if constexpr (b != Bits::M64) {
         return;
      } else {
         /* Gate: only translated images that carry exception data. Keeps the
          * section out of every non-C++ binary so the suite is unperturbed. */
         bool has_eh = false;
         Section<b> *text = nullptr;
         Segment<b> *data_seg = segment(SEG_DATA);
         for (Segment<b> *seg : segments()) {
            for (Section<b> *s : seg->sections) {
               const std::string n = s->name();
               if (n == "__gcc_except_tab" || n == "__eh_frame") { has_eh = true; }
               else if (n == "__text" && text == nullptr) { text = s; }
               else if (n == "__86x64_pcmap") { return; } /* idempotent */
            }
         }
         if (!has_eh || text == nullptr || data_seg == nullptr) { return; }

         auto *blob = PcmapBlob<b>::Create();
         blob->anchor = text;
         for (Segment<b> *seg : segments()) {
            for (Section<b> *s : seg->sections) {
               for (SectionBlob<b> *sb : s->content) {
                  if (sb->orig_vmaddr == 0) { continue; }
                  if (dynamic_cast<const Instruction<b> *>(sb) == nullptr) { continue; }
                  typename PcmapBlob<b>::Ent ent;
                  ent.trans = sb;
                  ent.orig = (uint32_t) sb->orig_vmaddr;
                  blob->ents.push_back(ent);
               }
            }
         }
         if (blob->ents.empty()) { delete blob; return; }

         auto *sect = Section<b>::Synthetic(SEG_DATA, "__86x64_pcmap",
                                            S_REGULAR, /*align=*/2);
         sect->segment = data_seg;
         sect->content.push_back(blob);
         blob->section = sect;
         blob->segment = data_seg;
         insert_section_before_zerofill(data_seg, sect);
         invalidate_segments_cache();
         if (std::getenv("MACHO_BUILD_DEBUG")) {
            fprintf(stderr, "inject_pcmap_section: %zu instructions -> "
                    "__DATA,__86x64_pcmap\n", blob->ents.size());
         }
      }
   }

   template <Bits b>
   void Archive<b>::inject_abs32_section() {
      if constexpr (b != Bits::M64) {
         return; /* M32 builds are intermediate; the table describes M64 fields */
      } else {
         Segment<b> *data_seg = segment(SEG_DATA);
         if (data_seg == nullptr) { return; }

         /* Idempotent: a reparse (modify/convert) of an already-translated
          * dylib carries the section as a live Abs32Blob (section.cc lifts it)
          * whose Parse re-resolved every field to the current blobs — Emit
          * re-emits it correctly; never inject a second copy. */
         for (Segment<b> *seg : segments()) {
            for (Section<b> *s : seg->sections) {
               if (s->name() == "__86x64_abs32") { return; }
            }
         }

         auto *blob = Abs32Blob<b>::Create();
         for (Segment<b> *seg : segments()) {
            const std::string segname(
               seg->segment_command.segname,
               strnlen(seg->segment_command.segname,
                       sizeof(seg->segment_command.segname)));
            if (segname != SEG_TEXT) { continue; }
            for (Section<b> *s : seg->sections) {
               for (SectionBlob<b> *sb : s->content) {
                  if (auto *inst = dynamic_cast<Instruction<b> *>(sb)) {
                     /* A kept absolute `[disp32(,idx,scale)]` memory operand:
                      * Emit writes memdisp->loc.vmaddr (+offset) as an
                      * absolute 32-bit displacement the runtime must slide.
                      * (rip-relative forms re-derive memdisp_absolute=false
                      * at parse/transform and need no slide.) */
                     if (inst->memdisp == nullptr || !inst->memdisp_absolute) {
                        continue;
                     }
                     const unsigned disp_bits =
                        xed_decoded_inst_get_memory_displacement_width_bits(
                           &inst->xedd, inst->memidx);
                     if (disp_bits != 32) { continue; }
                     /* disp32 sits at the instruction tail, before any trailing
                      * immediate: prefixes|opcode|modrm|sib|disp32|imm. */
                     const xed_operand_values_t *ops =
                        xed_decoded_inst_operands(&inst->xedd);
                     std::size_t immbytes = 0;
                     if (xed_operand_values_has_immediate(ops)) {
                        immbytes =
                           xed_decoded_inst_get_immediate_width_bits(&inst->xedd)
                           / 8;
                     }
                     if (inst->instbuf.size() < immbytes + 4) { continue; }
                     typename Abs32Blob<b>::Ent ent;
                     ent.blob = inst;
                     ent.off = inst->instbuf.size() - immbytes - 4;
                     blob->ents.push_back(ent);
                  } else if (auto *im = dynamic_cast<Immediate<b> *>(sb)) {
                     /* A 4-byte absolute pointer slot in a __TEXT data section
                      * (switch jump tables in __TEXT,__const): Emit writes
                      * pointee->loc.vmaddr — pre-slide, and dyld rejects
                      * 4-byte local relocs, so the runtime must slide it.
                      * (Immediates with no pointee are integer constants;
                      * JumpTableEntry blobs emit anchor-relative deltas that
                      * need no slide — both excluded.) */
                     if (im->pointee == nullptr) { continue; }
                     typename Abs32Blob<b>::Ent ent;
                     ent.blob = im;
                     ent.off = 0;
                     blob->ents.push_back(ent);
                  }
               }
            }
         }
         /* Emit the table EVEN WHEN EMPTY for our own translated output (the
          * image links libabiconv — the pipeline inserts that load before
          * convert): an empty site list is valid ground truth that tells the
          * runtime "nothing to slide — do NOT fall back to the byte-pattern
          * scan", whose value-window heuristic would still slide integer
          * constants that alias the image span (guard 95_abs32_imm_const).
          * For anything that does NOT link libabiconv (native binaries passed
          * through `macho-tool modify` in unrelated flows), keep the old
          * behavior: only add the section when it has content. */
         if (blob->ents.empty()) {
            bool links_abiconv = false;
            for (const DylibCommand<b> *dc :
                    this->template subcommands<DylibCommand>()) {
               if (dc->dylib_cmd.cmd == LC_LOAD_DYLIB &&
                   dc->name.find("libabiconv") != std::string::npos) {
                  links_abiconv = true;
                  break;
               }
            }
            if (!links_abiconv) { delete blob; return; }
         }

         auto *sect = Section<b>::Synthetic(SEG_DATA, "__86x64_abs32",
                                            S_REGULAR, /*align=*/2);
         sect->segment = data_seg;
         sect->content.push_back(blob);
         blob->section = sect;
         blob->segment = data_seg;
         insert_section_before_zerofill(data_seg, sect);
         invalidate_segments_cache();
         if (std::getenv("MACHO_BUILD_DEBUG")) {
            fprintf(stderr, "inject_abs32_section: %zu abs32 site(s) -> "
                    "__DATA,__86x64_abs32\n", blob->ents.size());
         }
      }
   }

   template <Bits b>
   void Archive<b>::inject_ehlsda_section() {
      if constexpr (b != Bits::M64) {
         return;
      } else {
         if (eh_lsda_pairs.empty()) { return; }
         Section<b> *text = nullptr;
         Segment<b> *data_seg = segment(SEG_DATA);
         for (Segment<b> *seg : segments()) {
            for (Section<b> *s : seg->sections) {
               const std::string n = s->name();
               if (n == "__text" && text == nullptr) { text = s; }
               else if (n == "__86x64_ehlsda") { return; }  /* idempotent */
            }
         }
         if (text == nullptr || data_seg == nullptr) { return; }

         /* original i386 vmaddr -> translated instruction blob (sorted) */
         std::map<size_t, const SectionBlob<b> *> by_orig;
         for (Segment<b> *seg : segments()) {
            for (Section<b> *s : seg->sections) {
               for (SectionBlob<b> *sb : s->content) {
                  if (sb->orig_vmaddr == 0) { continue; }
                  if (dynamic_cast<const Instruction<b> *>(sb) == nullptr) { continue; }
                  by_orig.emplace(sb->orig_vmaddr, sb); /* first (entry) wins */
               }
            }
         }

         auto *blob = EhlsdaBlob<b>::Create();
         blob->text_anchor = text;
         for (const auto& pr : eh_lsda_pairs) {
            /* orig_func (region) is the TRUE entry, whose i386 prologue + PIC
             * get_pc_thunk the M64 transform drops, so it usually isn't itself in
             * the pcmap; use the first MAPPED instruction at/after it to identify
             * the translated function. */
            auto it = by_orig.lower_bound(pr.first);
            if (it == by_orig.end()) { continue; }
            typename EhlsdaBlob<b>::Ent ent;
            ent.func = it->second;
            ent.orig_func = (uint32_t) pr.first;
            ent.lsda_off = (int32_t) pr.second;
            blob->ents.push_back(ent);
         }
         if (blob->ents.empty()) { delete blob; return; }

         auto *sect = Section<b>::Synthetic(SEG_DATA, "__86x64_ehlsda",
                                            S_REGULAR, /*align=*/2);
         sect->segment = data_seg;
         sect->content.push_back(blob);
         blob->section = sect;
         blob->segment = data_seg;
         insert_section_before_zerofill(data_seg, sect);
         invalidate_segments_cache();
         if (std::getenv("MACHO_BUILD_DEBUG")) {
            fprintf(stderr, "inject_ehlsda_section: %zu functions -> "
                    "__DATA,__86x64_ehlsda\n", blob->ents.size());
         }
      }
   }

   template <Bits b>
   void Archive<b>::synthesize_dyld_info() {
      if constexpr (b != Bits::M64) {
         return; /* M32 builds are intermediate; we only emit modern output */
      } else {
         if (!synthesize_dyld_info_enabled) { return; }

         /* A modern image already carries a real LC_DYLD_INFO stream. Normally
          * there is nothing to do. But a NON-PIE i386 EXECUTABLE (MH_PIE clear,
          * e.g. Civ IV Steam) links with an EMPTY rebase table: as a fixed-base
          * executable its internal __nl_symbol_ptr / __mod_init_func pointers are
          * baked absolute and never slide, so the linker emits zero rebases. When
          * we convert that executable into a DYLIB, dyld ALWAYS applies a slide,
          * and every un-rebased absolute internal pointer is left pointing at the
          * original (pre-slide) vmaddr -> EXC_BAD_ACCESS on first deref (observed:
          * a CFBundleGetFunctionPointerForName plugin-loader path reading a stale
          * __nl_symbol_ptr -> movl (%rdi),%eax with rdi = original vmaddr).
          *
          * So: if a DyldInfo exists but its rebase table is EMPTY, run ONLY the
          * rebase-synthesis pass and merge the synthesized REBASE opcodes into the
          * existing stream (its binds/exports are already correct). A modern PIE
          * input has a populated rebase table and is left untouched. This is the
          * modern-path analogue of the classic (LC_DYSYMTAB-only) synthesis below;
          * both derive the internal sliding-pointer set from the indirect symbol
          * table's INDIRECT_SYMBOL_LOCAL markers (nlocrel/nextrel are 0 here). */
         DyldInfo<b> *existing_dyld = this->template subcommand<DyldInfo>();
         const bool rebase_only = (existing_dyld != nullptr);
         if (rebase_only && !existing_dyld->rebase->rebasees.empty()) { return; }

         auto *symtab   = this->template subcommand<Symtab>();
         auto *dysymtab = this->template subcommand<Dysymtab>();
         if (symtab == nullptr || dysymtab == nullptr) { return; }

         const bool dbg = std::getenv("MACHO_BUILD_DEBUG") != nullptr;

         /* (1) ordinal -> DylibCommand. dyld's two-level library ordinal counts
          * every dylib-loading command in load-command order, 1-based (matching
          * BuildEnv::dylib_counter and classic_symbind). */
         std::vector<const DylibCommand<b> *> ord_dylibs;
         for (LoadCommand<b> *lc : load_commands) {
            auto *dc = dynamic_cast<DylibCommand<b> *>(lc);
            if (dc == nullptr) { continue; }
            switch (dc->cmd()) {
            case LC_LOAD_DYLIB: case LC_LOAD_WEAK_DYLIB: case LC_REEXPORT_DYLIB:
            case LC_LOAD_UPWARD_DYLIB: case LC_LAZY_LOAD_DYLIB:
               ord_dylibs.push_back(dc); break;
            default: break; /* LC_ID_DYLIB et al. don't get an ordinal */
            }
         }

         /* (2) symbol-table index -> Nlist*. The multiset iteration order is the
          * Build/Emit order and is exactly the index space the indirect symbol
          * table entries reference (convert.cc re-indexes the indirect table
          * into this same order after its symtab edits). */
         std::vector<const Nlist<b> *> syms_by_index(symtab->syms.begin(),
                                                     symtab->syms.end());

         /* In rebase_only mode we append to the EXISTING DyldInfo's rebase stream
          * and leave its binds/weak/lazy/export untouched (they are already the
          * image's correct modern streams). The scratch bind/weak/lazy/export
          * objects below are then only populated by the classic path and never
          * installed. */
         auto *rebase    = rebase_only ? existing_dyld->rebase
                                       : RebaseInfo<b>::Create();
         auto *bind      = BindInfo<b, false>::Create();
         auto *weak_bind = BindInfo<b, false>::Create(/*weak=*/true);
         auto *lazy_bind = BindInfo<b, true>::Create();
         auto *export_info = ExportInfo<b>::Create();

         auto add_rebase = [&] (const SectionBlob<b> *blob) {
            auto *rn = RebaseNode<b>::Create(REBASE_TYPE_POINTER);
            rn->blob = blob;
            rebase->rebasees.push_back(rn);
         };

         /* A symbol-pointer slot holds a sliding internal pointer only when its
          * blob resolved to an internal pointee; a null-valued LOCAL slot must
          * NOT be rebased (dyld would add the slide to 0). */
         auto sp_has_pointee = [] (const SectionBlob<b> *blob) -> bool {
            if (auto *n = dynamic_cast<const NonLazySymbolPointer<b> *>(blob)) {
               return n->pointee != nullptr;
            }
            if (auto *l = dynamic_cast<const LazySymbolPointer<b> *>(blob)) {
               return l->pointee != nullptr;
            }
            return false;
         };

         std::size_t n_bind = 0, n_lazy = 0, n_rebase = 0, n_export = 0, n_weak = 0;

         /* (3) Symbol-pointer sections (S_{NON_,}LAZY_SYMBOL_POINTERS): each slot
          * maps through the indirect symbol table to a symbol or a local/abs
          * sentinel. External undef -> BIND (eager: we route lazy imports
          * through the non-lazy bind stream too, which is always correct and
          * avoids depending on the translated stub_helper / dyld_stub_binder
          * path; true LAZY_BIND is a future optimization). Defined symbol or
          * LOCAL sentinel -> REBASE (internal sliding pointer). These slots are
          * 8 bytes wide in the M64 output, so a modern 8-byte bind/rebase is
          * size-correct. */
         for (Segment<b> *seg : segments()) {
            for (Section<b> *sect : seg->sections) {
               const uint32_t stype = sect->sect.flags & SECTION_TYPE;
               if (stype != S_NON_LAZY_SYMBOL_POINTERS &&
                   stype != S_LAZY_SYMBOL_POINTERS) { continue; }
               const uint32_t reserved1 = sect->sect.reserved1;

               uint32_t slot = 0;
               for (SectionBlob<b> *blob : sect->content) {
                  if (dynamic_cast<SymbolPointer<b> *>(blob) == nullptr) {
                     continue; /* skip trailing placeholders etc. */
                  }
                  const uint32_t isym_idx = reserved1 + slot;
                  ++slot;
                  if (isym_idx >= dysymtab->indirectsyms.size()) { continue; }
                  const uint32_t isym = dysymtab->indirectsyms[isym_idx];

                  if (isym & (INDIRECT_SYMBOL_LOCAL | INDIRECT_SYMBOL_ABS)) {
                     /* LOCAL = baked internal pointer (slides) -> rebase, but
                      * only if it actually holds an internal pointer (skip
                      * null-valued slots). ABS = absolute (never slides). */
                     if ((isym & INDIRECT_SYMBOL_ABS) == 0 && sp_has_pointee(blob)) {
                        add_rebase(blob); ++n_rebase;
                     }
                     continue;
                  }
                  if (isym >= syms_by_index.size()) { continue; }
                  const Nlist<b> *nl = syms_by_index[isym];
                  if (nl->string == nullptr) { continue; }

                  /* rebase_only: the image's imports already live in the existing
                   * bind stream; only the internal sliding pointers (defined-symbol
                   * slots -> the `else` below) need a synthesized rebase. Skip the
                   * UNDEF-bind emission entirely. */
                  if (rebase_only && nl->kind() == Nlist<b>::Kind::UNDEF) { continue; }

                  if (nl->kind() == Nlist<b>::Kind::UNDEF) {
                     const uint8_t ord = GET_LIBRARY_ORDINAL(nl->nlist.n_desc);
                     int8_t special = 0;
                     const DylibCommand<b> *dylib = nullptr;
                     if (ord == DYNAMIC_LOOKUP_ORDINAL) {
                        special = BIND_SPECIAL_DYLIB_FLAT_LOOKUP;       /* -2 */
                     } else if (ord == EXECUTABLE_ORDINAL) {
                        special = BIND_SPECIAL_DYLIB_MAIN_EXECUTABLE;   /* -1 */
                     } else if (ord >= 1 && ord <= ord_dylibs.size()) {
                        dylib = ord_dylibs[ord - 1];
                     } else {
                        /* SELF (0) can't be represented in this codebase's
                         * dylib_special sentinel scheme, and an unknown ordinal
                         * has no dylib; skip rather than emit a corrupt bind. */
                        continue;
                     }
                     uint8_t flags = 0;
                     if (nl->nlist.n_desc & N_WEAK_REF) {
                        flags |= BIND_SYMBOL_FLAGS_WEAK_IMPORT;
                     }
                     bind->bindees.push_back(
                        BindNode<b, false>::Create(BIND_TYPE_POINTER, 0, dylib, special,
                                                   nl->string->str, flags, blob));
                     ++n_bind;

                     /* Weak-coalesced external (N_WEAK_REF): the C++ runtime
                      * weak externals — operator new/delete (__Znwm/__Znam/
                      * __ZdlPv/__ZdaPv) and weakly-referenced template/vtable/
                      * RTTI symbols. A native i386 linker records each of these
                      * in BOTH the regular bind table AND the weak_bind table,
                      * targeting the SAME symbol-pointer slot (the modern path
                      * preserves this; see static-interpose's weak rewrite and
                      * tests-i386 29_weak_new_delete). Mirror it here so the
                      * synthesized classic->modern stream is byte-faithful to a
                      * native linker's __DATA weak-coalesced binds.
                      *
                      * A weak bind carries no dylib ordinal — it is an implicit
                      * BIND_SPECIAL_DYLIB_WEAK_LOOKUP (-3) flat coalesced lookup
                      * (BindNode `weak` mode emits no SET_DYLIB). It targets the
                      * symbol by the name now in the nlist, which on the deployed
                      * path is the post-static-interpose shim name (____X bound
                      * to libabiconv): the coalesced lookup then resolves to
                      * libabiconv's single definition — never re-fusing the slot
                      * to a native libstdc++ operator new entered with an i386
                      * frame (the pointer-fusion failure mode 1e685f1 fixed for
                      * the modern path). This adds no new runtime risk over the
                      * regular bind already emitted above (same target name),
                      * and is a no-op for images with no weak externals.
                      *
                      * The coalescing marker on an UNDEF nlist is N_REF_TO_WEAK
                      * (0x0080) — distinct from N_WEAK_REF (0x0040, a weak IMPORT
                      * that may resolve to NULL, handled by the regular bind's
                      * BIND_SYMBOL_FLAGS_WEAK_IMPORT above). N_REF_TO_WEAK shares
                      * its bit value with N_WEAK_DEF but means "reference to a
                      * weak symbol" in undef context — exactly what the linker
                      * sets on operator new/delete and weak template/RTTI refs.
                      * (Verified on Source-engine i386 C++ dylibs: __Znwm et al.
                      * carry n_desc 0x0180 = ordinal 1 | N_REF_TO_WEAK.) */
                     if (nl->nlist.n_desc & N_REF_TO_WEAK) {
                        weak_bind->bindees.push_back(
                           BindNode<b, false>::Create(BIND_TYPE_POINTER, 0, nullptr,
                                                      /*dylib_special=*/0, nl->string->str,
                                                      /*flags=*/0, blob, /*weak=*/true));
                        ++n_weak;
                     }
                  } else {
                     /* Defined symbol referenced by a non-lazy pointer = an
                      * internal sliding pointer. */
                     add_rebase(blob); ++n_rebase;
                  }
               }
            }
         }

         /* (4) Absolute pointer arrays outside the symbol-pointer sections:
          * __mod_init_func / __mod_term_func (NonLazySymbolPointer blobs with a
          * resolved internal pointee). Mirrors Dysymtab::regenerate_local_relocs
          * but emits REBASE opcodes instead of a classic local-reloc table. Only
          * 8-byte NonLazySymbolPointer slots are rebased; 4-byte Immediate
          * pointers stay un-rebased (the translated dylib loads at a pinned base
          * with slide 0, so their baked values are already correct — same as the
          * classic path; an 8-byte rebase would clobber the neighbouring 4-byte
          * slot). */
         for (Segment<b> *seg : segments()) {
            const std::string sn = seg->name();
            if (sn == SEG_PAGEZERO || sn == SEG_LINKEDIT) { continue; }
            for (Section<b> *sect : seg->sections) {
               const uint32_t stype = sect->sect.flags & SECTION_TYPE;
               if (stype == S_NON_LAZY_SYMBOL_POINTERS ||
                   stype == S_LAZY_SYMBOL_POINTERS) { continue; } /* handled in (3) */
               for (SectionBlob<b> *blob : sect->content) {
                  auto *nlp = dynamic_cast<NonLazySymbolPointer<b> *>(blob);
                  if (nlp == nullptr || nlp->pointee == nullptr) { continue; }
                  add_rebase(blob); ++n_rebase;
               }
            }
         }

         /* (5) Exports: every defined external (N_SECT | N_EXT) symbol. N_ABS
          * externals have no `value` placeholder (Nlist parse only placeholders
          * N_SECT symbols) and are skipped — which also avoids re-exporting GCC
          * C++ `.eh` FDE markers (N_ABS, value 0) that dyld can't satisfy. Weak
          * definitions carry EXPORT_SYMBOL_FLAGS_WEAK_DEFINITION.
          *
          * rebase_only: the existing DyldInfo already carries the image's real
          * export trie; skip export synthesis (and the DyldInfo::Create/insert
          * below — the synthesized rebase nodes were appended in-place to the
          * existing stream via the `rebase` alias). */
         if (rebase_only) {
            if (dbg) {
               fprintf(stderr,
                       "synthesize_dyld_info: rebase_only merged %zu rebase into "
                       "existing LC_DYLD_INFO\n", n_rebase);
            }
            return;
         }

         for (const Nlist<b> *nl : symtab->syms) {
            if (nl->kind() != Nlist<b>::Kind::EXT) { continue; }
            if (nl->string == nullptr || nl->string->str.empty()) { continue; }
            if (Nlist<b>::is_header_symbol(nl->string->str)) { continue; }
            if (nl->value == nullptr) { continue; } /* N_ABS / no address */
            std::size_t eflags = EXPORT_SYMBOL_FLAGS_KIND_REGULAR;
            if (nl->nlist.n_desc & N_WEAK_DEF) {
               eflags |= EXPORT_SYMBOL_FLAGS_WEAK_DEFINITION;
            }
            auto *node = RegularExportNode<b>::Create(eflags, nl->value);
            export_info->trie.insert(nl->string->str, node);
            ++n_export;
         }

         auto *dyld = DyldInfo<b>::Create(rebase, bind, weak_bind, lazy_bind, export_info);

         /* Insert the LC_DYLD_INFO_ONLY immediately before LC_SYMTAB (its
          * conventional position, right after the segment commands). */
         auto pos = std::find(load_commands.begin(), load_commands.end(),
                              static_cast<LoadCommand<b> *>(symtab));
         load_commands.insert(pos, dyld);

         if (dbg) {
            fprintf(stderr,
                    "synthesize_dyld_info: %zu bind, %zu weak, %zu lazy, %zu rebase, "
                    "%zu export\n", n_bind, n_weak, n_lazy, n_rebase, n_export);
         }
      }
   }

   template <Bits b>
   void Archive<b>::Emit(Image& img) const {
      /* emit header */
      img.at<mach_header_t<b>>(0) = header;

      /* emit load commands */
      static const bool emit_debug = std::getenv("MACHO_EMIT_DEBUG") != nullptr;
      std::size_t offset = sizeof(header);
      std::size_t i = 0;
      for (LoadCommand<b> *lc : load_commands) {
         if (emit_debug) {
            fprintf(stderr,
                    "Archive::Emit lc[%zu] cmd=0x%x size=%u @ offset=0x%zx\n",
                    i, (unsigned)lc->cmd(), (unsigned)lc->size(), offset);
         }
         lc->Emit(img, offset);
         offset += lc->size();
         ++i;
      }
      if (emit_debug) {
         fprintf(stderr, "Archive::Emit completed all %zu LCs\n", load_commands.size());
      }
   }

   template <Bits b>
   Archive<b>::Archive(const Archive<opposite<b>>& other, TransformEnv<opposite<b>>& env)
   {
      env(other.header, header);
      for (const auto lc : other.load_commands) {
         /* Drop LC_SEGMENT_SPLIT_INFO, LC_DYLIB_CODE_SIGN_DRS, and
          * LC_LINKER_OPTIMIZATION_HINT from the translated output. These are
          * hints for Apple's static linker / dyld shared cache builder, not
          * required for runtime image loading. install_name_tool has strict
          * ordering checks for them that depend on absolute file offsets, and
          * our M32→M64 layout shifts (especially dyld_info data growth) push
          * them off the expected positions — install_name_tool then refuses
          * to mutate with "X data out of place". Dropping them silences the
          * error, lets later -change/-add_rpath calls succeed, and costs
          * nothing at runtime since dyld doesn't consult these blobs. */
         const uint32_t cmd = lc->cmd();
         if (cmd == LC_SEGMENT_SPLIT_INFO ||
             cmd == LC_DYLIB_CODE_SIGN_DRS ||
             cmd == LC_LINKER_OPTIMIZATION_HINT) {
            continue;
         }
         load_commands.push_back(lc->Transform(env));
      }
      /* Read the source __eh_frame (i386 addresses still valid) so Build can emit
       * the per-EH-function LSDA table for the C++ exception unwinder. */
      collect_eh_lsda_pairs(other);
   }

   template <Bits b>
   void Archive<b>::insert(SectionBlob<b> *blob, const Location& loc, Relation rel) {
      macho_addr_t<b> segment_command_t<b>::*segloc;
      macho_addr_t<b> segment_command_t<b>::*segsize;
      std::size_t locval;
      if (loc.offset) {
         segloc = &segment_command_t<b>::fileoff;
         segsize = &segment_command_t<b>::filesize;
         locval = loc.offset;
      } else if (loc.vmaddr) {
         segloc = &segment_command_t<b>::vmaddr;
         segsize = &segment_command_t<b>::vmsize;
         locval = loc.vmaddr;
      } else {
         throw std::invalid_argument("location offset and vmaddr are both 0");
      }

      for (Segment<b> *segment : segments()) {
         std::size_t segloc_ = segment->segment_command.*segloc;
         if (locval >= segloc_ && locval < segloc_ + segment->segment_command.*segsize) {
            segment->insert(blob, loc, rel);
            return;
         }
      }

      throw std::invalid_argument("location not in any segment");
   }

   template <Bits b>
   std::size_t Archive<b>::offset_to_vmaddr(std::size_t offset) const {
      for (Segment<b> *segment : segments()) {
         if (offset >= segment->segment_command.fileoff &&
             offset < segment->segment_command.fileoff + segment->segment_command.filesize) {
            return segment->offset_to_vmaddr(offset);
         }
      }
      throw std::invalid_argument(std::string("offset" ) + std::to_string(offset) +
                                  " not in any segment");
   }

   template <Bits b>
   std::optional<std::size_t> Archive<b>::try_offset_to_vmaddr(std::size_t offset) const {
      for (Segment<b> *segment : segments()) {
         if (segment->contains_offset(offset)) {
            return segment->try_offset_to_vmaddr(offset);
         }
      }
      return std::nullopt;
   }

   AbstractArchive *AbstractArchive::Parse(const Image& img, std::size_t offset) {
      uint32_t magic = img.at<uint32_t>(offset);
      switch (magic) {
      case MH_CIGAM:
      case MH_CIGAM_64:
         throw std::invalid_argument("archive has opposite endianness");

      case MH_MAGIC:
         return Archive<Bits::M32>::Parse(img, offset);
         
      case MH_MAGIC_64:
         return Archive<Bits::M64>::Parse(img, offset);

      default:
         {
            std::stringstream ss;
            ss << "invalid magic number 0x" << std::hex << magic;
            throw std::invalid_argument(ss.str());
         }
      }
   }

   template <Bits b>
   void Archive<b>::remove_commands(uint32_t cmd) {
      /* erase-friendly loop: erase returns the iterator to the next element,
       * which we use directly instead of ++it'ing past it. The prior version
       * `it = erase(it); ++it;` skipped every second consecutive match. */
      for (auto it = load_commands.begin(); it != load_commands.end(); ) {
         if ((*it)->cmd() == cmd) {
            it = load_commands.erase(it);
         } else {
            ++it;
         }
      }
   }

   template <Bits b>
   std::vector<Section<b> *> Archive<b>::sections() const {
      std::vector<Section<b> *> acc;
      for (const auto segment : segments()) {
         acc.insert(acc.end(), segment->sections.begin(), segment->sections.end());
      }
      return acc;
   }

   template <Bits b>
   Section<b> *Archive<b>::section(uint8_t index) const {
      if (index == NO_SECT) {
         return nullptr;
      } else {
         return sections().at(index - 1);
      }
   }

   template <Bits b>
   Section<b> *Archive<b>::section(const std::string& name) const {
      for (auto section : sections()) {
         if (section->name() == name) {
            return section;
         }
      }
      return nullptr;
   }

   template class Archive<Bits::M32>;
   template class Archive<Bits::M64>;

}
