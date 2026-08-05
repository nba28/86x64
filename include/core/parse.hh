#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <unordered_map>

#include "loc.hh"
#include "types.hh"
#include "resolve.hh"
#include "region.hh"

namespace MachO {
   
   template <Bits bits> class SectionBlob;
   template <Bits bits> class RelocBlob;
   
   template <typename T>
   class CountResolver {
   public:
      void add(T *pointee) { resolver.add(++id, pointee); }
      template <typename... Args>
      void resolve(Args&&... args) { return resolver.resolve(args...); }
      
      CountResolver(const std::string& name): resolver(name), id(0) {}
      
   private:
      Resolver<std::size_t, T, false> resolver;
      unsigned id;
   };

   template <Bits bits>
   class ParseEnv {
   public:
      Archive<bits>& archive;
      
      Resolver<std::size_t, SectionBlob<bits>, true> vmaddr_resolver;
      Resolver<std::size_t, SectionBlob<bits>, true> offset_resolver;
      Resolver<uint32_t, BindNode<bits, true>, false> lazy_bind_node_resolver;
      std::unordered_map<std::size_t, RelocBlob<bits> *> relocs;
      // Resolver<std::size_t, RelocBlob<bits>> reloc_resolver; /*!< resolves by vmaddr */
      CountResolver<DylibCommand<bits>> dylib_resolver;
      CountResolver<Segment<bits>> segment_resolver;
      CountResolver<Section<bits>> section_resolver;
      Segment<bits> *current_segment = nullptr;
      Section<bits> *current_section = nullptr;
      Regions data_in_code;

      /* vmaddr-to-placeholder map */
      using TodoPlaceholders = std::map<std::size_t, Placeholder<bits> *>;
      TodoPlaceholders placeholders;

      /* i386 PIC relative-offset switch jump-table slots: maps each 4-byte
       * table-entry vmaddr to its dispatch's PIC anchor vmaddr. Populated by
       * Section::DetectJumpTables (a pre-pass before the linear sweep) and
       * consumed by TextParser, which emits a JumpTableEntry blob (instead of
       * decoding the entry bytes as code) for any offset whose vmaddr is a key
       * here. See JumpTableEntry in section_blob.hh. */
      std::map<std::size_t, std::size_t> jump_table_slots;

      /* Classic i386 self-modifying CALL-stub redirect map (`__IMPORT,
       * __jump_table`, section type S_SYMBOL_STUBS + attr
       * S_ATTR_SELF_MODIFYING_CODE). A pre-10.4 i386 image routes its lazy
       * calls through 5-byte stubs that dyld populated in place at load time;
       * the section ships all-`0xf4` (hlt) and modern dyld never fills it, so a
       * translated `call <stub>` jumps into hlt on a non-exec page (Civ IV crt
       * `__start` -> `___keymgr_dwarf2_register_sections` SIGBUS). Keyed by each
       * stub's original i386 vmaddr, valued by the original vmaddr of the
       * DEFINED function the stub stands for (resolved via the indirect symbol
       * table -> nlist n_value). Populated by Dysymtab::lift_jump_table_targets
       * (parse phase, before any section's Parse1) and consumed by the relative-
       * branch handler in Instruction::parse(): a branch whose target is a key
       * here is retargeted to the defined function, bypassing the dead stub.
       * Only DEFINED (N_SECT) stub symbols are entered — the 92% intra-image
       * majority; UNDEFINED external stubs (libstdc++ __cxa_, keymgr) need a
       * synthesized runtime-bound trampoline and are NOT handled here. */
      std::map<std::size_t, std::size_t> jump_table_targets;

      /* Classic i386 self-modifying CALL-stub redirect for the UNDEFINED half.
       * Keyed by each undefined stub's original i386 vmaddr, valued by the
       * synthesized JumpStubBlob (in __TEXT,__jt_tramp) that jumps through a
       * dyld-bound __DATA,__jt_ptrs slot to the real import. Populated by
       * Dysymtab::synthesize_undef_jump_stubs (parse phase). Consumed by the
       * relative-branch handler in Instruction::parse(): a branch whose target
       * is a key here has its brdisp pointed STRAIGHT at the trampoline blob,
       * bypassing add_placeholder() — which positions a placeholder by vmaddr
       * and would otherwise land the branch on the dead __IMPORT stub (the stub
       * vmaddr lives in the still-present __jump_table section). */
      std::map<std::size_t, const SectionBlob<bits> *> jump_table_undef_tramps;

      /* Function-symbol vmaddrs, populated from the LC_SYMTAB nlist table
       * (all N_SECT, non-stab entries with n_value != 0).  Used by the linear
       * code sweep (Section::Parse1) to detect when a decoded instruction would
       * SPAN a function boundary (i.e. a known entry point lands inside the
       * decoded range, not at the decode start).  Such a span means the bytes
       * preceding the entry point are inter-function padding (e.g. a 0x00 align
       * byte) that the sweep has mistakenly absorbed into the previous decode.
       * The sweep truncates to a 1-byte DataBlob and retries from the next byte,
       * re-syncing cleanly at the true function boundary.
       *
       * Universal: the check triggers only when a symbol boundary falls
       * STRICTLY INSIDE the decoded instruction (vmaddr < sym < vmaddr+len),
       * which is never true for well-aligned code and exclusively fires on
       * padding bytes before a labelled entry.  Gated on M32 only (no i386 PIC
       * confusion in M64 re-parses). */
      std::set<std::size_t> func_syms;

      /* True iff the LC_SYMTAB contains at least one LOCAL (non-N_EXT) N_SECT
       * symbol whose address lies in an EXECUTABLE segment — i.e. the binary
       * still carries symbols for its static/file-scope functions. This is the
       * confidence signal for DataParser's exec-target pointer gate: when the
       * statics are symboled, EVERY genuine code pointer baked into __DATA/
       * __OBJC (fn-pointer tables, ObjC1 method IMPs) matches a func_syms
       * entry, so a data word aliasing a mid-function text address can be
       * safely rejected as a constant. Locals-stripped binaries leave this
       * false and keep the legacy permissive detection (can't discriminate).
       * Populated in the Symtab ctor alongside func_syms. */
      bool have_local_text_syms = false;

      /* CLASSIC-RELOC AUTHORITATIVE POINTER MAP (M32 classic images): the
       * vmaddr of every slot covered by the LC_DYSYMTAB LOCAL relocation
       * table (locreloff/nlocrel; scattered entries included, PAIR followers
       * skipped). A slidable classic image's genuine absolute INTERNAL
       * pointers ALL carry a local reloc — dyld could not slide them
       * otherwise — so when this map is armed it is EXACT, not heuristic: a
       * 4-aligned data word whose value merely aliases a segment's vmaddr
       * range but has no entry here is an integer/packed constant. (Portal 2
       * engine.dylib: 637/637 in-range heuristic hits inside __TEXT,__const
       * had NO reloc — packed int16 pairs like g_SideVertCorners {1,0} =
       * 0x00010000, ASCII string bytes, floats — and rebasing them corrupted
       * the displacement-map tables: wild SIGBUS store in InitPowerInfo_R.)
       * Armed (have_classic_local_relocs) only for genuinely classic images:
       * nlocrel > 0 AND no LC_DYLD_INFO rebase stream — a hybrid's pointer
       * truth lives in its rebase opcodes instead. Populated by
       * Dysymtab::lift_local_relocs (LC-construction phase, ready before any
       * Section::Parse1); consumed by DataParser's pointer detection.
       * Fixed-address execs carry no relocs at all (nlocrel == 0), stay
       * disarmed, and keep the heuristics + func-entry gate. */
      std::set<std::size_t> local_reloc_addrs;
      bool have_classic_local_relocs = false;

      /* GCC PIC thunks (`___i686.get_pc_thunk.<r>`), keyed by the thunk's
       * entry vmaddr (= its nlist n_value) and valued by the x86 GPR encoding
       * (0=EAX,1=ECX,2=EDX,3=EBX,5=EBP,6=ESI,7=EDI) the thunk loads with the
       * caller's PC.  Populated from the LC_SYMTAB by NAME in the Symtab ctor
       * (first parse phase), so it is GLOBAL and ready before any section's
       * Parse1 runs.  Section::DetectPicAnchoredDisps / DetectJumpTables each
       * additionally byte-scan their own section for unnamed thunks, then
       * seed from this map so a `call ___i686.get_pc_thunk.bx` whose thunk
       * lives in a DIFFERENT text section (Civ IV/GCC put them in
       * __textcoal_nt while callers are in __text) still establishes the PIC
       * anchor.  Without it the anchored `[ebx+disp32]` falls through to the
       * generic absolute-table rewrite, which keeps the anchor base and emits
       * `[anchor + lea(rip+target)]` — double-counting the base (SIGSEGV at
       * anchor+target, Civ IV crt `start`).  The uint8_t value avoids pulling
       * xed into this header; the consumers map it back to xed_reg_enum_t. */
      std::unordered_map<std::size_t, uint8_t> pic_thunks;

      /* Absolute pointer-immediate VALUES relocated during the M32 parse: the
       * base address of a fixed-load-address `mov/add/push $&data` idiom that
       * the instruction-immediate heuristic moved to the translated layout.
       * Recorded so a later `cmp reg, $&data_end` that BOUNDS a pointer loop
       * (its immediate = base + table_size) can be relocated by the same delta.
       * If only the base moves and the sentinel keeps its raw i386 value, the
       * loop iterator starts at the slid base and never reaches the stale
       * sentinel -> runs off the end of the table (Halo static-init table walk:
       * `mov $tbl,%ebx; loop: ...; cmp $tbl_end,%ebx; jne loop` calls a virtual
       * method through each entry, overruns into a NULL slot -> `jmp *0`).
       * Populated in address order by the linear sweep, so a base at a lower
       * vmaddr than the loop's cmp is present when the cmp is parsed. */
      std::set<std::size_t> relocated_ptr_imms;

      /* True iff `vmaddr` sits at or above a relocated pointer-immediate base
       * (relocated_ptr_imms) that lies in the SAME segment — i.e. it plausibly
       * bounds a table whose base was already relocated. Gates the CMP-immediate
       * relocation (see the instruction.cc CMP_*_IMMz case) so that only a
       * genuine table-end sentinel moves, never a bare loop-count constant that
       * merely aliases a data vmaddr. */
      bool imm_bounds_relocated_table(std::size_t vmaddr) const;

      Placeholder<bits> *add_placeholder(std::size_t vmaddr);
      void do_resolve();

      /* True iff `vmaddr` lands in a program-writable data segment (the
       * `__DATA` segment family: VM_PROT_WRITE set, not the ObjC fragile-ABI
       * `__OBJC` metadata segment which is also writable but whose blobs are
       * parsed structurally). Gates the mid-blob containing-fallback on the
       * LOAD/pointer paths: in opaque `__DATA`, a mid-blob offset into the
       * nearest containing DataBlob is meaningful; in `__OBJC` the "nearest
       * containing blob" guess corrupts category/method metadata (regressed
       * +[NSObject isLogEnabled] legacy category lookup). */
      bool vmaddr_in_writable_data(std::size_t vmaddr) const;

      /* True iff `vmaddr` lands in any real image segment EXCEPT __PAGEZERO,
       * __LINKEDIT and the ObjC fragile-ABI `__OBJC` metadata segment. Unlike
       * vmaddr_in_writable_data this ALSO admits read-only const/literal
       * segments (`__TEXT` const tables). Gates the mid-blob containing-fallback
       * for an INDEXED memory operand (`[disp32 + index*scale]`), whose disp32
       * is unambiguously a dereferenced table base (never an integer constant),
       * so a mid-blob offset into the containing const/data blob is always
       * meaningful — only __OBJC's structurally-parsed metadata must be spared
       * (see vmaddr_in_writable_data). */
      bool vmaddr_in_indexed_table_target(std::size_t vmaddr) const;

      /* True iff `vmaddr` lands in a READ-ONLY, NON-CODE section (no
       * S_ATTR_PURE_INSTRUCTIONS/S_ATTR_SOME_INSTRUCTIONS): `__TEXT,__const`,
       * `__TEXT,__cstring`, literal sections, etc. Companion to
       * vmaddr_in_writable_data for the pointer-VALUE mid-blob fallbacks
       * (Immediate / NonLazySymbolPointer): once the caller has decided a
       * value IS a pointer, an INTERIOR (non-blob-start) address into opaque
       * read-only data must relocate exactly like the aligned/blob-start case
       * — leaving it raw guarantees a stale pre-slide vmaddr in the output
       * (Quinn Preferences: `movl $_pieceMatrix2,(%esp)` with _pieceMatrix2 at
       * 2 mod 4 in `__TEXT,__const` stayed 0xb3a36 -> EXC_BAD_ACCESS in the
       * native callee; its 4-aligned siblings relocated fine). Read-only
       * segments are never `__OBJC` (writable), and instruction sections are
       * excluded because a mid-instruction byte offset is meaningless after
       * the M32->M64 transform rewrites the code bytes. */
      bool vmaddr_in_readonly_opaque_data(std::size_t vmaddr) const;

      /* True iff `vmaddr` lands inside a ZEROFILL section (S_ZEROFILL /
       * S_GB_ZEROFILL / S_THREAD_LOCAL_ZEROFILL — __DATA,__bss/__common).
       * Zerofill sections legitimately span megabytes of vmaddr space while
       * occupying zero file bytes, so an integer constant/offset is far more
       * likely to ALIAS their range than any file-backed section's. Gates the
       * DetectPicAnchoredDisps cancellation of heuristic pointer-immediates:
       * inside PIC-anchored code the compiler never emits absolute-address
       * immediates (globals are reached anchor-relative), so an immediate that
       * aliases a zerofill span there is an anchor/base-relative OFFSET (e.g.
       * `addl $0x124f80, %edx` computing &array[i]) and relocating it would
       * corrupt the pointer arithmetic (Halo-class zerofill repro). */
      bool vmaddr_in_zerofill(std::size_t vmaddr) const;

      /* True iff vmaddr falls in a constant/string/code section that is a
       * high-confidence pointer target (__cstring/__cfstring/__const/__text/
       * __objc* etc.). Used to disambiguate a `mov [reg+disp], imm32` whose
       * imm32 might be a baked-in absolute pointer vs. an integer constant. */
      bool vmaddr_in_const_section(std::size_t vmaddr) const;

      /* CODE-target FUNCTION-ENTRY gate (see section.cc DataParser + the
       * instruction-immediate heuristics in instruction.cc). True iff `vmaddr`
       * merely ALIASES a mid-function address inside an
       * S_ATTR_(PURE|SOME)_INSTRUCTIONS section — i.e. the binary keeps its
       * local text symbols (have_local_text_syms), NO func_syms nlist exists
       * exactly at `vmaddr`, and `vmaddr` lands in an instructions-flagged
       * section. A genuine code pointer (fn-ptr table slot, vtable slot, ObjC1
       * IMP, callback immediate) targets a function ENTRY and carries a symbol;
       * a value with no symbol that lands mid-function is an integer constant
       * (Quinn's {4,4}=0x00040004 piece size, Civ IV's `mov $0xffff,%edx`
       * static-init priority) and must NOT be relocated. Returns false when
       * the gate is disarmed (locals-stripped binary) or the target is not
       * instructions-flagged (__cstring/__TEXT,__const/data stay permissive:
       * callers keep their own heuristics for those). */
      bool code_alias_is_constant(std::size_t vmaddr) const;

      /* STRIPPED-BINARY stack-arg fallback (used only by instruction.cc's
       * esp/ebp `movl $imm,N(%esp/%ebp)` heuristic). code_alias_is_constant is
       * deliberately disarmed for locals-stripped images — without symbols it
       * cannot tell a genuine mid-image code pointer from a constant. But a
       * STACK-ARG imm32 that aliases an instructions-flagged section is a
       * code-aliasing integer constant, never a data pointer: genuine data/
       * string args target __cstring/__const/__data, and a -no_pie/PIC image
       * (no rebase metadata) has no absolute code-pointer immediates at all, so
       * this has no false-negatives there. True iff !have_local_text_syms AND
       * `vmaddr` lands in an S_ATTR_(PURE|SOME)_INSTRUCTIONS section. (Halo's
       * CPU-speed check: the divisor 1000000 = 0xF4240 aliased i386 __TEXT ->
       * mis-relocated + load-slid to 49557824 -> MHz 2400 read as 48 -> a false
       * "insufficient CPU" renderer nag. Regression: 74_stripped_stackarg_const.) */
      bool stackarg_imm_is_code_constant(std::size_t vmaddr) const;

      /* CSTRING-INTERIOR ALIAS gate (see section.cc DataParser). True iff
       * `vmaddr` lands in the INTERIOR of an S_CSTRING_LITERALS section — i.e.
       * inside a NUL-terminated C string but NOT at a string START (the section
       * base, or the byte immediately after a NUL terminator). A genuine baked
       * `char *` data pointer always targets a string START (deduplicated string
       * literals are referenced at their first byte); a value landing mid-string
       * is an integer/byte-table CONSTANT that merely aliases the cstring vmaddr
       * range. Reloc-less, locals-stripped fixed-address execs disarm every
       * other discriminator (code_alias_is_constant / classic-reloc gate), so
       * without this test such a constant is falsely rebased. (Civ IV STEAM:
       * GCompactDeclInfoNodeArray's byte-classification table `00 01 00 01…` =
       * the word 0x01000100 aliases a __cstring interior "…eWidgetType, int
       * iData1, i…"; rebasing it to a translated __text address corrupted the
       * table so table[c&0xf] returned a wild index -> node[8+idx*4] read a NULL
       * name pointer -> SIGSEGV in Find/LowerBoundSearch via x64_cb_dispatch.)
       * Needs the image to read the preceding byte; returns false for any
       * non-cstring target so callers gate only this exact false-positive. */
      bool cstring_interior_alias(const Image& img, std::size_t vmaddr) const;

      /* ZERO-FILL TARGET gate (see section.cc DataParser). True iff `vmaddr`
       * lands in a ZERO-FILL section (S_ZEROFILL / S_GB_ZEROFILL, i.e.
       * __DATA,__bss and __DATA,__common) and NO nlist symbol sits exactly
       * there.
       *
       * WHY. Pointer detection in a reloc-less fixed-address i386 exec is a
       * heuristic over 4-byte values, and a zero-fill target is the WEAKEST
       * evidence class there is: the pointee has no file content to inspect, the
       * image carries no relocation naming the slot, and in a locals-stripped
       * image there is no symbol either. Nothing can corroborate it. Meanwhile a
       * false positive is not inert — it rewrites a live integer.
       *
       * ★MEASURED (Halo CE, 2026-08-04): the tag-class descriptor records hold
       * three u16 fields at +0xa/+0xc/+0xe. In the 'scen' and 'lifi' records the
       * pair at +0xc spells 0x0048021C and 0x005802D0, both of which land in
       * __DATA,__common — so both were "rebased", turning (540, 72) into
       * (27356, 4272) and (720, 88) into (27536, 4288). The 'bipd' record
       * survived only because its pair spells 0x00780234, which is ABOVE the
       * image. The enclosing loop processes exactly the two corrupted records,
       * so Halo died every run: `base + 564` became `base - 9508`, landing in
       * the tag block's name strings, and the deref took a wild address.
       *
       * Same family as the memdisp-code-alias gate: a small-integer PAIR whose
       * bytes happen to spell a plausible address is not a pointer. Because the
       * image is small, EVERY valid address has a small high half — which is
       * precisely what makes (small, small) u16 pairs look like addresses, and
       * why no value-based test can separate them.
       *
       * Deliberately NARROW: zero-fill targets only, M32 only, and only when the
       * image has no local reloc table to speak authoritatively. An exact symbol
       * hit is honoured as positive evidence and passes. Measured population in
       * Halo's __DATA,__data: 42 of ~5000 detected words.
       * Kill switch: M64_NO_ZEROFILL_TARGET_GATE. */
      bool zerofill_target_unattested(std::size_t vmaddr) const;

      /* CODE-INTERIOR ALIAS gate (see section.cc DataParser). True iff `vmaddr`
       * lands STRICTLY INSIDE a decoded instruction of an
       * S_ATTR_(PURE|SOME)_INSTRUCTIONS section — i.e. the section has already
       * been parsed into blobs, but NO blob starts exactly at `vmaddr`.
       *
       * A genuine pointer into code ALWAYS targets an instruction BOUNDARY: a
       * function entry (fn-ptr table / vtable / ObjC1 IMP), or at worst a
       * basic-block head (switch table). Nothing can target the middle of an
       * instruction — it is not a valid execution address. So a data word whose
       * value lands mid-instruction is an integer/string CONSTANT that merely
       * aliases the code vmaddr range, and rebasing it corrupts it.
       *
       * This is the discriminator of LAST RESORT and the only one that arms for
       * a locals-stripped, reloc-less, fixed-address i386 exec, where
       * code_alias_is_constant (needs func_syms), the classic-reloc gate (needs
       * a local reloc table) and cstring_interior_alias (cstring targets only)
       * are ALL disarmed. Unlike those it needs no symbols, no relocs and no
       * string invariant — only the instruction decode macho-tool already
       * performs. (Civ IV STEAM: the 4-byte __DATA,__data string constant
       * " ._" = 0x005F2E20 — the strtok delimiter set of the engine's
       * hierarchical name-registry lookup — aliased a MID-INSTRUCTION i386
       * __text address and was falsely rebased to a translated __text address
       * 0x10A9546D, whose bytes are "mT\xa9\x10". strtok then split the path
       * "Game" on 'm' into "Ga"/"e"; "Ga" is not a child of the root node, so
       * the walk kept a NULL node and the next component dereferenced it at
       * +0x88 -> SIGSEGV during the translated static initializers.)
       *
       * SELF-DISARMING: returns false when the target section holds no parsed
       * blobs yet (parse order put __DATA before that code section), so an
       * unparsed section can never be mistaken for "all interior". Returns
       * false for any non-instructions target — callers keep their own gates
       * for __cstring / __TEXT,__const / data. Env kill-switch
       * M64_NO_CODE_INTERIOR_GATE=1 disarms it (A/B regression harness). */
      bool code_interior_alias(std::size_t vmaddr) const;

      /* POSITIVE function-ENTRY evidence at `vmaddr`, without needing local
       * symbols. True iff either
       *   - an nlist symbol sits exactly AT `vmaddr` (func_syms; GLOBAL text
       *     symbols survive `strip -x`, so this still works for a locals-
       *     stripped image), or
       *   - the standard i386 frame-setup prologue `55 89 e5` (push %ebp;
       *     mov %esp,%ebp) is at `vmaddr`, or
       *   - an i386 C++ ABI ADJUSTOR THUNK entry is at `vmaddr`:
       *     `add|sub $imm, disp8(%esp)` (83/81 44|6c 24 ..) immediately
       *     followed by a `jmp` (E9/EB). One is emitted per multiple-
       *     inheritance / covariant-return override and STORED IN A VTABLE, yet
       *     it is a local symbol (stripped) with no frame setup — so without
       *     this shape a locals-stripped C++ image loses thousands of genuine
       *     vtable slots to the ENTRY gate. Kill-switch for A/B:
       *     M64_NO_THUNK_ENTRY_EVIDENCE=1.
       * This is the same positive-evidence test instruction.cc applies to
       * code-aliasing imm32s (imm32_code_alias_is_constant), lifted here so the
       * __DATA pointer-detection path shares ONE definition of "this address is
       * a function entry" instead of duplicating it. */
      bool code_target_has_entry_evidence(const Image& img,
                                          std::size_t vmaddr) const;

      /* CODE-ENTRY gate for __DATA pointer detection (see section.cc
       * DataParser). True iff `vmaddr` lands in an
       * S_ATTR_(PURE|SOME)_INSTRUCTIONS section but carries NO function-entry
       * evidence — i.e. it is a mid-function address that a data word merely
       * ALIASES, not a genuine code pointer.
       *
       * Armed ONLY for locals-STRIPPED images (!have_local_text_syms). When
       * locals survive, code_alias_is_constant already discriminates with the
       * stronger "must have an nlist" rule and this adds nothing; the two are
       * exact complements, so symboled binaries see no behavior change.
       *
       * WHY this and not the instruction-BOUNDARY test: a genuine pointer into
       * code targets a function ENTRY, but being an instruction boundary is far
       * weaker than being an entry — Civ IV's " ._" = 0x005F2E20 IS a valid
       * boundary (`sub $0x18,%esp`, 3 bytes into the function that starts at
       * 0x5F2E1D with `55 89 e5`), so code_interior_alias cannot see it. The
       * entry test does: no nlist, no prologue -> constant.
       *
       * NOT applied to __TEXT,__const: switch jump tables legitimately target
       * mid-function BASIC-BLOCK heads, which have neither symbol nor prologue
       * (same exclusion code_alias_is_constant makes). Env kill-switch
       * M64_NO_CODE_ENTRY_GATE=1 disarms it (A/B regression harness). */
      bool code_alias_lacks_entry_evidence(const Image& img,
                                           std::size_t vmaddr) const;


      ParseEnv(Archive<bits>& archive):
         archive(archive),
         vmaddr_resolver("ParseEnv::vmaddr_resolver"), offset_resolver("ParseEnv::offset_resolver"),
         lazy_bind_node_resolver("ParseEnv::lazy_bind_node_resolver"),
         dylib_resolver("ParseEnv::dylib_resolver"), segment_resolver("ParseEnv::segment_resolver"),
         section_resolver("ParseEnv::section_resolver"),
         current_segment(nullptr), current_section(nullptr) {}
      
   private:
      
   };
   
}
