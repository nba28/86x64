#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <sys/stat.h>
#include <sys/mman.h>

#include "image.hh"
#include "util.hh"
#include "types.hh"

namespace MachO {

   Image::Image(const char *path, int mode) {
      int mode2 = 0;
      if ((mode & O_CREAT)) {
         mode2 = 0776;
      }
      
      if ((fd = open(path, mode, mode2)) < 0) {
         throw cerror(std::string("open: ") + path);
      }

      struct stat st;
      if (fstat(fd, &st) < 0) {
         close(fd);
         throw cerror("fstat");
      }
      filesize = st.st_size;
      mapsize = std::max<std::size_t>(filesize, getpagesize());

      if ((mode & O_RDWR)) {
         prot = PROT_READ | PROT_WRITE;
      } else {
         prot = PROT_READ;
      }
      
      /* In write mode the file may be shorter than mapsize (e.g. newly created
       * with O_CREAT, st_size=0). Writes to the mmap region past EOF SIGBUS
       * on macOS — extend the file to mapsize up front so writes are valid
       * for the whole mapping. Was implicit before because grow() ftruncate'd
       * per-byte; that's now batched, so we have to do it once here. */
      if ((mode & O_RDWR) && filesize < mapsize) {
         if (ftruncate(fd, mapsize) < 0) {
            close(fd);
            throw cerror("ftruncate-init");
         }
      }

      if ((img = mmap(NULL, mapsize, prot, MAP_SHARED, fd, 0)) == MAP_FAILED) {
         close(fd);
         throw cerror("mmap");
      }
   }
   
   Image::~Image() {
      munmap(img, mapsize);
      ftruncate(fd, filesize);
      close(fd);
   }

   void Image::resize(std::size_t newsize) {
      if (munmap(img, mapsize) < 0) {
         img = NULL;
         throw cerror("munmap");
      }

      if (ftruncate(fd, newsize) < 0) {
         throw cerror("ftruncate");
      }

      mapsize = newsize;
      
      if ((img = mmap(NULL, mapsize, prot, MAP_SHARED, fd, 0)) == MAP_FAILED) {
         close(fd);
         throw cerror("mmap");
      }

   }

   void Image::grow(std::size_t size) {
      /* Big-binary perf: the original `grow()` ftruncate'd the file on every
       * byte beyond filesize. On iPhoto-scale (23MB) the Emit phase makes
       * millions of small writes, multiplying into millions of ftruncate
       * syscalls + APFS journal updates → modify subcommand hung 1h41min+ at
       * 100% CPU on iPhoto, completed in seconds on dbRepair/photocd.
       *
       * Fix: only ftruncate when expanding the mmap envelope (already doubles
       * via resize()). Track filesize as the logical high-water mark in
       * memory; persist it via ftruncate-on-destroy in ~Image() (already does
       * `ftruncate(fd, filesize)` on close). Within the current mmap envelope,
       * writes don't need ftruncate because the kernel-mapped region already
       * covers up to mapsize bytes of the file — they're zero-initialized
       * pages until written. */
      if (size > filesize) {
         filesize = size;
         if (filesize > mapsize) {
            resize(std::max<std::size_t>(filesize, mapsize * 2));
         }
      }
   }

   void Image::memset(std::size_t offset, int c, std::size_t bytes) {
      grow(offset + bytes);
      ::memset((char *) img + offset, c, bytes);
   }
   
}
