/*
 * 99_scandir_sort — classic i386 scandir + alphasort: the namelist and its
 * entries are i386 struct dirents in low memory, select/compar are called with
 * pointers the i386 code can dereference. Portal 2 filesystem_stdio
 * FS_FindNextFile: native scandir handed alphasort truncated 64-bit dirent
 * pointers -> SIGSEGV.
 *
 * Exit 42 = sorted, select applied, fields sane, free() of every entry works.
 * Kill switch M64_NO_DIR_SHIM=1 (run time): a fault or a wrong exit.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct dirent32 { uint32_t d_ino; uint16_t d_reclen; uint8_t d_type, d_namlen; char d_name[256]; };
extern int scandir(const char *, struct dirent32 ***, int (*)(const struct dirent32 *),
                   int (*)(const void *, const void *));
extern int alphasort(const void *, const void *);

static int calls;
static int no_dot(const struct dirent32 *e) { calls++; return e->d_name[0] != '.'; }

int main(void) {
   struct dirent32 **list = 0;
   int n = scandir("/", &list, no_dot, alphasort);
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
