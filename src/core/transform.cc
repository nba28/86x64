#include <cstdlib>
#include "transform.hh"

namespace MachO {

   template <Bits b1, Bits b2>
   void TransformEnv<b1, b2>::operator()(const mach_header_t<b1>& h1, mach_header_t<b2>& h2) const {
         if constexpr (b2 == Bits::M32) {
            h2.magic = MH_MAGIC;
            h2.cputype = CPU_TYPE_I386;
            h2.cpusubtype = CPU_SUBTYPE_I386_ALL;
         } else {
            h2.magic = MH_MAGIC_64;
            h2.cputype = CPU_TYPE_X86_64;
            h2.cpusubtype = CPU_SUBTYPE_X86_64_ALL;
            h2.reserved = 0;
         }

         h2.filetype = h1.filetype;
         h2.ncmds = 0; /* build-time */
         h2.sizeofcmds = 0; /* build-time */
         h2.flags = h1.flags;

         /* ── A TRANSLATED IMAGE MUST NOT JOIN THE WEAK-DEF COALESCING POOL ──
          * MH_WEAK_DEFINES advertises "my weak definitions are candidates for
          * process-wide C++ coalescing". dyld resolves another image's
          * <weak-def-coalesce> bind by searching the images that carry this bit,
          * in load order, and taking the first definition. That is fine in an
          * all-i386 process -- every candidate has the same ABI -- but a
          * translated image lives alongside NATIVE frameworks, so the bit lets
          * i386 code satisfy an x86_64 caller's bind. It is a silent ABI
          * violation across the whole process.
          *
          * ★MEASURED on Portal 2 2026-09-13. Every translated Source module
          * exports its own __ZdlPv (operator delete) in __TEXT,__text -- the
          * pristine i386 originals do too, so this is faithful translation, not
          * a translator defect. AGXMetalG16X calls operator delete through a
          * <weak-def-coalesce> bind, and dyld bound the DRIVER's slot to
          * launcher.dylib's TRANSLATED definition. The i386 epilogue's 4-byte
          * `pop %ebp` zeroes the high half of rbp, so the driver's very next
          * frame-pointer store (`movq %rax,-0x9d0(%rbp)` at AGXMetalG16X
          * +0x3f7e59, right after it calls applyWorkaroundForAppList from
          * gatherDeviceOptions) wrote into unmapped low memory. It presented for
          * a long time as "Portal 2 dies in renderer init": the victim was the
          * GPU driver, the culprit was us. Read the driver's OWN got slot to see
          * it -- dlsym(RTLD_DEFAULT,"_ZdlPv") answers the flat search order
          * instead and reports libc++abi even in the failing run.
          *
          * DIRECTIONALITY is why clearing the bit is the right fix rather than
          * hiding the symbols: a translated image that IMPORTS operator
          * new/delete binds `libabiconv/____ZdlPv`, an abigen bridge, as an
          * ordinary TWO-LEVEL bind -- unaffected by this bit -- and its internal
          * calls to its own definition are direct, not binds. Measured across
          * Portal 2's 42 modules: ZERO <weak-def-coalesce> binds anywhere in the
          * translated tree, i.e. translated images only ever SUPPLIED
          * coalescing, never consumed it. Making the definitions private-extern
          * instead WOULD break things -- libtier0/libsteam_api/engine import
          * these with no local definition and must keep reaching the i386
          * bridge.
          *
          * Universal: triggers on the structural fact that we are emitting a
          * translated image into a process that also contains native code, never
          * on an app or symbol name. M64_KEEP_WEAK_DEFINES=1 restores the old
          * behaviour for bisection. */
         if constexpr (b2 == Bits::M64) {
            static const bool keep_weak_defines =
               std::getenv("M64_KEEP_WEAK_DEFINES") != nullptr;
            if (!keep_weak_defines) { h2.flags &= ~(uint32_t) MH_WEAK_DEFINES; }
         }
      }

   template <Bits b1, Bits b2>
   void TransformEnv<b1, b2>::operator()(const segment_command_t<b1>& h1,
                                       segment_command_t<b2>& h2) const {
         h2.cmd = b2 == Bits::M32 ? LC_SEGMENT : LC_SEGMENT_64;
         h2.cmdsize = 0; /* build-time */
         memcpy(h2.segname, h1.segname, sizeof(h2.segname));
         h2.vmaddr = h1.vmaddr;
         h2.vmsize = h1.vmsize;
         h2.fileoff = h1.fileoff;
         h2.filesize = h1.filesize;
         h2.maxprot = h1.maxprot;
         h2.initprot = h1.initprot;
         h2.nsects = 0; /* build-time */
         h2.flags = h1.flags;
      }

   template <Bits b1, Bits b2>
   void TransformEnv<b1, b2>::operator()(const nlist_t<b1>& n1, nlist_t<b2>& n2) const {
      n2.n_un.n_strx = 0; /* build-time */
         n2.n_type = n1.n_type;
         n2.n_sect = 0; /* build-time */
         n2.n_desc = n1.n_desc;
         n2.n_value = 0; /* build-time */
   }

   template <Bits b1, Bits b2>
   void TransformEnv<b1, b2>::operator()(const section_t<b1>& s1, section_t<b2>& s2) const {
         memset(&s2, 0, sizeof(s2));
         memcpy(s2.sectname, s1.sectname, sizeof(s2.sectname));
         memcpy(s2.segname, s1.segname, sizeof(s2.segname));
         s2.addr = 0; /* build-time */
         s2.size = 0; /* build-time */
         s2.offset = 0; /* build-time */
         s2.align = s1.align;
         s2.reloff = s1.reloff;
         s2.nreloc = s1.nreloc;
         s2.flags = s1.flags & ~ (uint32_t) S_ATTR_LOC_RELOC;
         s2.reserved1 = s1.reserved1;
         s2.reserved2 = s1.reserved2;
      }

   template class TransformEnv<Bits::M32, Bits::M64>;
   template class TransformEnv<Bits::M64, Bits::M32>;
   
}
