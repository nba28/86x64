/*
 * 99_scandir_ino64 — scandir$INODE64 + alphasort$INODE64 (what a 64-bit-inode
 * i386 build imports): the namelist and its entries are low copies of the
 * native dirent, the INODE64 layout (d_name at +0x15). The generated bridge
 * left the 4-byte namelist slot NULL under a nonzero count (Portal
 * filesystem_stdio FillDataStruct -> read of namelist[0] at 0).
 *
 * Exit 42 = sorted, select applied, fields sane, free() of every entry works.
 * Kill switch M64_NO_DIR_SHIM=1 (run time): a fault or a wrong exit.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct dirent64 {
   uint64_t d_ino, d_seekoff;
   uint16_t d_reclen, d_namlen;
   uint8_t d_type;
   char d_name[1024];
} __attribute__((packed));
_Static_assert(__builtin_offsetof(struct dirent64, d_name) == 0x15, "INODE64 dirent");

extern int scandir64(const char *, struct dirent64 ***, int (*)(const struct dirent64 *),
                     int (*)(const void *, const void *)) __asm__("_scandir$INODE64");
extern int alphasort64(const void *, const void *) __asm__("_alphasort$INODE64");

static int calls;
static int no_dot(const struct dirent64 *e) { calls++; return e->d_name[0] != '.'; }

int main(void) {
   struct dirent64 **list = 0;
   int n = scandir64("/", &list, no_dot, alphasort64);
   if (n < 3 || !list) exit(3);
   if (calls < n) exit(4);
   for (int i = 0; i < n; i++) {
      if (list[i]->d_name[0] == '.') exit(5);
      if (list[i]->d_namlen != strlen(list[i]->d_name)) exit(6);
      if (i && strcmp(list[i - 1]->d_name, list[i]->d_name) > 0) exit(7);
      free(list[i]);
   }
   free(list);
   exit(42);
}
