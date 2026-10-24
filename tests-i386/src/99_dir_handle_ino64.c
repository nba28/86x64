/*
 * 99_dir_handle_ino64 — the $INODE64 twin of 99_dir_handle: code built with
 * 64-bit inodes imports opendir$INODE64$UNIX2003 / readdir$INODE64 /
 * closedir$UNIX2003. The dirent layout matches natively, but libc's DIR and
 * the dirent it returns live above 4GB (Civ IV's Python 2.6 import scan died
 * in readdir on the truncated DIR*).
 *
 * Exit 42 = "." and ".." seen with sane fields, closedir 0. Kill switch
 * M64_NO_DIR_SHIM=1 (run time): a fault or a wrong exit.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct dirent64 { uint64_t d_ino, d_seekoff; uint16_t d_reclen, d_namlen; uint8_t d_type; char d_name[1024]; };
extern void *opendir64(const char *) __asm("_opendir$INODE64$UNIX2003");
extern struct dirent64 *readdir64(void *) __asm("_readdir$INODE64");
extern int closedir64(void *) __asm("_closedir$UNIX2003");

int main(void) {
   void *d = opendir64("/");
   if (!d) exit(3);
   int dot = 0, dotdot = 0, n = 0;
   struct dirent64 *e;
   while ((e = readdir64(d)) != 0 && n < 4096) {
      n++;
      if (e->d_namlen != strlen(e->d_name)) exit(4);
      if (!strcmp(e->d_name, ".")) dot = 1;
      if (!strcmp(e->d_name, "..")) dotdot = 1;
   }
   if (closedir64(d) != 0) exit(5);
   exit(dot && dotdot && n > 2 ? 42 : 6);
}
