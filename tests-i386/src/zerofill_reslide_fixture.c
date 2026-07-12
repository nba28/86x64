/* zerofill_reslide_fixture.c — fixture for zerofill_reslide_test.sh.
 *
 * Guards the "double-slide of a runtime pointer in a zerofill section" fix
 * (objc_slide.c slide_data_fnptrs: skip S_ZEROFILL / S_GB_ZEROFILL /
 * S_THREAD_LOCAL_ZEROFILL).
 *
 * ROOT CAUSE (Civ IV write-into-__TEXT at exit): slide_data_fnptrs slides
 * every 4-byte word in a WRITABLE segment whose value lands in the image's
 * pre-slide vmaddr window. That predicate is only sound BEFORE the program
 * runs. A __bss/__common (zerofill) section has NO file bytes, so a
 * translate-time-relocated static pointer can NEVER live there — every word is
 * written at RUNTIME (a heap pointer, an int, a live field). When a bundle
 * carries >1 libabiconv copy (multicopy deploy: Contents/MacOS + inside a
 * bundled translated framework), the SECOND copy's add-image callback re-scans
 * every image with its own private processed-set. By then the image's vacated
 * preferred-vmaddr window has been recycled by the low-4GB heap, so a
 * legitimate runtime pointer sitting in a zerofill slot is indistinguishable
 * from an unslid static slot -> it gets a SECOND slide and turns into a wild
 * address (Civ: GTokenizer::FreeAllBuffers()'s __common buffer-pointer array
 * double-slid; at exit the per-buffer teardown wrote through the corrupted
 * pointer into r-x __TEXT -> EXC_BAD_ACCESS).
 *
 * WHY A DIRECT-CALL FIXTURE: the double-slide only bites when the image loads
 * at a NONZERO slide (so an in-window runtime value gets a second +slide). The
 * tests-i386 harness always places translated no_pie images at their preferred
 * base -> real slide == 0 -> slide_data_fnptrs early-returns and an end-to-end
 * reproduction is impossible here. So (mirroring 33_dyld_section_patch.c, which
 * asserts the structural fix when the live crt path can't be recreated) we
 * drive the exact scan directly via the test-only export
 * _86x64_test_slide_data_fnptrs, over a hand-built in-memory Mach-O header with
 * a chosen nonzero slide. We lay out TWO writable sections holding the SAME
 * in-window pointer value: one S_REGULAR (a legitimate scan target -> must be
 * slid) and one S_ZEROFILL (a runtime slot -> must be skipped). The fix is
 * proven iff the regular slot is slid and the zerofill slot is left untouched. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mach-o/loader.h>

/* Test-only entry exported by objc_slide.c. This fixture is built NATIVE
 * x86_64 and linked against the SHIPPING objc_slide.c.o object file, so we
 * call it as an ordinary extern (no i386 translation, no full-libabiconv load
 * — its arena constructor would abort without the wrapper's low-4GB region). */
extern void _86x64_test_slide_data_fnptrs(const struct mach_header_64 *mh64,
                                          long slide,
                                          uint64_t text_lo, uint64_t text_hi,
                                          uint64_t vmaddr_lo, uint64_t vmaddr_hi);

/* objc_slide.c.o references these abiconv-internal symbols from OTHER functions
 * that our single call (_86x64_test_slide_data_fnptrs -> slide_data_fnptrs,
 * which touches only libc) never reaches. Provide inert definitions so the
 * link resolves; they must never actually run. */
void _86x64_cxx_typeinfo_init(void) {}
void _86x64_dyld_func_lookup(void) {}
void _86x64_dyld_noop(void) {}
void _86x64_import_repair(void) {}
void _86x64_objc_index_legacy_classes(void) {}
void _86x64_objc_register_classes(void) {}
void abiconv_call_init(void) {}
void abiconv_init_trampoline(void) {}
unsigned x64_cb_wrap(unsigned long long r) { (void)r; return 0; }
unsigned x64_objc_wrap(unsigned long long r) { (void)r; return 0; }
unsigned x64_value_data_shadow(unsigned long long r) { (void)r; return 0; }

/* A minimal in-memory image: mach_header_64 + one __DATA LC_SEGMENT_64 with two
 * sections (regular + zerofill). slide_data_fnptrs walks load commands from
 * (mh64 + 1) and, for each writable segment section, reads/writes 4-byte words
 * at (sect->addr + slide). We point both sections' addr at our own buffers by
 * choosing sect->addr = (uintptr_t)buf - slide, so (addr + slide) == buf. */
struct img {
   struct mach_header_64 mh;
   struct segment_command_64 seg;
   struct section_64 sect_regular;
   struct section_64 sect_zerofill;
};

int main(void) {
   /* Chosen nonzero slide and a pre-slide vmaddr window. */
   const long SLIDE = -0x1000000;            /* large negative, like real loads */
   const uint64_t VLO = 0x10000000ULL;       /* pre-slide window [VLO, VHI)      */
   const uint64_t VHI = 0x13000000ULL;
   /* An in-window pointer value (4-aligned data pointer) — models a live
    * runtime pointer whose value fell back into the vacated window. */
   const uint32_t WINDOW_VAL = 0x10201240u;  /* in [VLO,VHI), 4-aligned          */

   uint32_t regular_slot  = WINDOW_VAL;      /* file-backed S_REGULAR word       */
   uint32_t zerofill_slot = WINDOW_VAL;      /* runtime S_ZEROFILL word          */

   struct img im;
   memset(&im, 0, sizeof im);
   im.mh.magic = MH_MAGIC_64;
   im.mh.filetype = MH_DYLIB;
   im.mh.ncmds = 1;
   im.mh.sizeofcmds = sizeof(struct segment_command_64)
                    + 2 * sizeof(struct section_64);

   im.seg.cmd = LC_SEGMENT_64;
   im.seg.cmdsize = im.mh.sizeofcmds;
   memcpy(im.seg.segname, "__DATA", 6);
   im.seg.initprot = VM_PROT_READ | VM_PROT_WRITE;   /* writable -> scanned      */
   im.seg.maxprot  = VM_PROT_READ | VM_PROT_WRITE;
   im.seg.nsects = 2;

   /* sect->addr chosen so (addr + SLIDE) lands on our stack slot. */
   memcpy(im.sect_regular.sectname, "__data", 6);
   memcpy(im.sect_regular.segname,  "__DATA", 6);
   im.sect_regular.addr  = (uint64_t)(uintptr_t)&regular_slot - (uint64_t)SLIDE;
   im.sect_regular.size  = 4;
   im.sect_regular.flags = S_REGULAR;

   memcpy(im.sect_zerofill.sectname, "__common", 8);
   memcpy(im.sect_zerofill.segname,  "__DATA", 6);
   im.sect_zerofill.addr  = (uint64_t)(uintptr_t)&zerofill_slot - (uint64_t)SLIDE;
   im.sect_zerofill.size  = 4;
   im.sect_zerofill.flags = S_ZEROFILL;

   _86x64_test_slide_data_fnptrs(&im.mh, SLIDE, /*text_lo*/0, /*text_hi*/0,
                                 VLO, VHI);

   /* The regular slot is a legitimate scan target: it must be slid. */
   printf("regular slot: %s\n",
          regular_slot == (uint32_t)(WINDOW_VAL + (uint32_t)SLIDE)
             ? "slid" : "untouched");
   /* The zerofill slot is a runtime value: it must be left verbatim. */
   printf("zerofill slot: %s\n",
          zerofill_slot == WINDOW_VAL ? "preserved" : "double-slid");

   exit(0);
   return 0;
}
