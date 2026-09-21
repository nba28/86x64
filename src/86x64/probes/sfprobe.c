#include <stdio.h>
#include <dlfcn.h>
int main(void){
  void *h = dlopen("/usr/lib/libSystem.B.dylib", RTLD_LAZY);
  void *sf = dlsym(h, "__sF");
  printf("stdin =%p\nstdout=%p\nstderr=%p\n", (void*)stdin,(void*)stdout,(void*)stderr);
  printf("dlsym __sF = %p  (native sizeof(FILE)=%zu)\n", sf, sizeof(FILE));
  if (sf) printf("&__sF[0]=%p &__sF[1]=%p &__sF[2]=%p\n",
                 sf, (char*)sf+sizeof(FILE), (char*)sf+2*sizeof(FILE));
  FILE *f = fopen("/etc/hosts","r");
  printf("fopen -> %p\n", (void*)f);
  return 0;
}
