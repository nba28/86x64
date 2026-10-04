/* 99_export_zerofill — an exported ZERO-FILL (__common) global must be exported
 * at its own address. Export values are address offsets from the image base;
 * the translator wrote the blob's FILE offset, which for zero-fill is the
 * section start, so every __common export aliased one address. Portal 2
 * libtier0: g_VProfCurrentProfile / g_ClockSpeed / ... all exported as
 * __common+0 -> server read a zero page as the profiler -> NULL node crash.
 * dlsym resolves through the export trie, so it sees exactly what an importer
 * binds. Exit 42 = both globals found at their own address. Kill switch
 * M64_NO_EXPORT_VMADDR=1 (translate time). Built with -fcommon. */
#include <dlfcn.h>
#include <stdlib.h>

int g_zf_a;
int g_zf_b;

int main(void) {
   g_zf_a = 41;
   g_zf_b = 42;
   int *pa = dlsym(RTLD_DEFAULT, "g_zf_a");
   int *pb = dlsym(RTLD_DEFAULT, "g_zf_b");
   exit(pa == &g_zf_a && pb == &g_zf_b && *pa == 41 && *pb == 42 ? 42 : 1);
}
