#include <stdio.h>
#include <dlfcn.h>
int main(void){
  void *carbon = dlopen("/System/Library/Frameworks/Carbon.framework/Carbon", RTLD_LAZY);
  void *agl    = dlopen("/System/Library/Frameworks/AGL.framework/AGL", RTLD_LAZY);
  void *ogl    = dlopen("/System/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_LAZY);
  printf("Carbon=%p AGL=%p OpenGL=%p\n\n", carbon, agl, ogl);
  const char *syms[] = {"GetWindowPort","GetWindowRef","HIViewGetRoot","HIWindowGetCGWindowID",
                        "aglChoosePixelFormat","aglCreateContext","aglSetDrawable","aglSetWindowRef",
                        "aglSetCurrentContext","aglSwapBuffers","aglUpdateContext","aglSetFullScreen",
                        "aglSetInteger","aglGetError","aglDescribeRenderer","aglQueryRendererInfo", 0};
  for (int i=0; syms[i]; i++)
    printf("  %-24s -> %p\n", syms[i], dlsym(RTLD_DEFAULT, syms[i]));
  return 0;
}
