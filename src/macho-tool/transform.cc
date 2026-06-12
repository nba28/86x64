#include <iostream>

#include "transform.hh"
#include "core/archive.hh"

int TransformCommand::opthandler(int optchar) {
   switch (optchar) {
   case 'h': // help
      usage(std::cout);
      return 0;
      
   case 'm': // bits
      switch (std::stoi(optarg)) {
      case 32:
         bits = MachO::Bits::M32;
         break;
      case 64:
         bits = MachO::Bits::M64;
      default:
         throw std::string("bits must be 32 or 64");
      }
      return 1;
      
   default:
      abort();
   }
}

template <MachO::Bits b>
int TransformCommand::workT(MachO::MachO *macho) {
   auto archive = dynamic_cast<MachO::Archive<b> *>(macho);
   if (archive == nullptr) {
      log("transform requires archive of correct bits");
      return -1;
   }
   archive->Build(0);
   auto newarchive = archive->Transform();
   /*
    * Force the transformed M64 binary to live entirely in low 32-bit
    * address space. The i386 → x86_64 translation patches up internal
    * pointers in __data (e.g. `extern char *gptr = &common_var;`) to
    * point at the new (shifted) segment locations — but each slot is
    * still only 4 bytes wide, so the new vmaddr has to fit in 32 bits.
    * The default M64 vmaddr_start is 0x100000000 (4GB PAGEZERO), which
    * would silently truncate every patched pointer.
    *
    * 0x10000000 (256 MB) is well above where dyld places the wrapper
    * executable and leaves room for ASLR slide of 0; the convert step
    * preserves this base and the wrapper disables ASLR at spawn time.
    */
   newarchive->vmaddr = 0x10000000;
   newarchive->Build(0);
   newarchive->Emit(*out_img);
   return 0;
}

int TransformCommand::work() {
   MachO::MachO *macho = MachO::MachO::Parse(*in_img);

   switch (bits ? *bits : macho->bits()) {
   case MachO::Bits::M32: return workT<MachO::Bits::M32>(macho);
   case MachO::Bits::M64: return workT<MachO::Bits::M64>(macho);
   }
}
