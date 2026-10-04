/*
 * 99_dir_handle — opendir's DIR is libc's (above 4GB) and classic i386
 * readdir returns the OLD struct dirent (u32 d_ino, u16 d_reclen, u8 d_type,
 * u8 d_namlen, char d_name[256]); dir_shim.c serves both. Portal 2
 * libmilesx86: readdir on the truncated DIR* died locking DIR->__dd_lock.
 *
 * Exit 42 = "." and ".." seen with sane fields, closedir 0. Kill switch
 * M64_NO_DIR_SHIM=1 (run time): a fault or a wrong exit.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct dirent32 { uint32_t d_ino; uint16_t d_reclen; uint8_t d_type, d_namlen; char d_name[256]; };
extern void *opendir(const char *);
extern struct dirent32 *readdir(void *);
extern int closedir(void *);

int main(void) {
   void *d = opendir("/");
   if (!d) exit(3);
   int dot = 0, dotdot = 0, n = 0;
   struct dirent32 *e;
   while ((e = readdir(d)) != 0 && n < 4096) {
      n++;
      if (e->d_namlen != strlen(e->d_name)) exit(4);
      if (!strcmp(e->d_name, ".")) dot = 1;
      if (!strcmp(e->d_name, "..")) dotdot = 1;
   }
   if (closedir(d) != 0) exit(5);
   exit(dot && dotdot && n > 2 ? 42 : 6);
}
