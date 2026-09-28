/* dptr_table_fixture.c — fixture for dptr_table_test.sh.
 *
 * Guards objc_slide.c slide_data_fnptrs's EXACT-TABLE path: when an image
 * carries __DATA,__86x64_dptr (Archive::inject_dptr_section), exactly the
 * listed 4-byte DATA slots are slid and nothing is guessed by value.
 *
 * The value scan it replaces gets two things wrong (measured on the audit
 * corpus): it slides an integer constant that happens to alias the image's
 * pre-slide window (0x10000000, 0x10101008, ...), and it skips a real pointer
 * to UNALIGNED data (a char * into __data) because its data-pointer rule
 * demands 4-byte alignment.
 *
 * Same direct-call shape as zerofill_reslide_fixture.c (the harness cannot
 * load a translated image at a nonzero slide): a hand-built Mach-O header whose
 * __data holds {aligned pointer, window-aliasing constant, unaligned pointer}
 * and, unless DPTR_OFF is set, a __86x64_dptr table listing the two pointers.
 * DPTR_OFF=1 omits the table, reproducing the scan's verdicts (the OFF arm). */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mach-o/loader.h>
#include <sys/mman.h>

extern void _86x64_test_slide_data_fnptrs(const struct mach_header_64 *mh64,
                                          long slide,
                                          uint64_t text_lo, uint64_t text_hi,
                                          uint64_t vmaddr_lo, uint64_t vmaddr_hi);

/* Inert definitions for objc_slide.c.o externs the tested path never reaches
 * (see zerofill_reslide_fixture.c). */
void _86x64_cxx_typeinfo_init(void) {}
void _86x64_dyld_func_lookup(void) {}
void _86x64_dyld_noop(void) {}
void _86x64_import_repair(void) {}
void _86x64_objc_index_legacy_classes(void) {}
void _86x64_objc_register_classes(void) {}
int  _86x64_objc_shared_claim_image(const void *mh) { (void)mh; return 1; }
void abiconv_call_init(void) {}
void abiconv_init_trampoline(void) {}
unsigned x64_cb_wrap(unsigned long long r) { (void)r; return 0; }
unsigned x64_objc_wrap(unsigned long long r) { (void)r; return 0; }
unsigned x64_value_data_shadow(unsigned long long r) { (void)r; return 0; }

struct img {
   struct mach_header_64 mh;
   struct segment_command_64 seg;
   struct section_64 data;
   struct section_64 dptr;
};

int main(void) {
   const long SLIDE = -0x1000000;
   const uint64_t VLO = 0x10000000ULL, VHI = 0x13000000ULL;
   const uint32_t PTR = 0x10201240u;   /* aligned pointer into the image     */
   const uint32_t CONST = 0x10101008u; /* integer that aliases the window    */
   const uint32_t UPTR = 0x102012abu;  /* pointer to unaligned data          */
   const int off = getenv("DPTR_OFF") != NULL;

   /* Both sections live in one buffer, mapped low: table entries are u32. */
   uint32_t *buf = mmap((void *)0x30000000, 4096, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANON, -1, 0);
   if (buf == MAP_FAILED) { puts("fixture: mmap failed"); return 2; }
   buf[0] = PTR; buf[1] = CONST; buf[2] = UPTR;
   /* Pre-slide vmaddr of buf[i] is &buf[i] - SLIDE. */
   const uint64_t base = (uint64_t)(uintptr_t)buf - (uint64_t)SLIDE;
   buf[3] = 0x72747064u;               /* "dptr" */
   buf[4] = 2;
   buf[5] = (uint32_t)(base + 0);      /* the aligned pointer slot          */
   buf[6] = (uint32_t)(base + 8);      /* the unaligned-data pointer slot   */
   if (base >> 32) { puts("fixture: buffer above 4GB, cannot model"); return 2; }

   struct img im;
   memset(&im, 0, sizeof im);
   im.mh.magic = MH_MAGIC_64;
   im.mh.filetype = MH_DYLIB;
   im.mh.ncmds = 1;
   im.seg.cmd = LC_SEGMENT_64;
   im.seg.nsects = off ? 1 : 2;
   im.mh.sizeofcmds = im.seg.cmdsize = sizeof(struct segment_command_64)
                    + im.seg.nsects * sizeof(struct section_64);
   memcpy(im.seg.segname, "__DATA", 6);
   im.seg.initprot = im.seg.maxprot = VM_PROT_READ | VM_PROT_WRITE;

   memcpy(im.data.sectname, "__data", 6);
   memcpy(im.data.segname, "__DATA", 6);
   im.data.addr = base;
   im.data.size = 12;
   memcpy(im.dptr.sectname, "__86x64_dptr", 12);
   memcpy(im.dptr.segname, "__DATA", 6);
   im.dptr.addr = base + 12;
   im.dptr.size = 16;

   _86x64_test_slide_data_fnptrs(&im.mh, SLIDE, 0, 0, VLO, VHI);

   printf("pointer: %s\n", buf[0] == PTR + (uint32_t)SLIDE ? "slid" : "unslid");
   printf("constant: %s\n", buf[1] == CONST ? "preserved" : "slid");
   printf("unaligned pointer: %s\n",
          buf[2] == UPTR + (uint32_t)SLIDE ? "slid" : "unslid");
   return 0;
}
