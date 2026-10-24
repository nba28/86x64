/*
 * 99_environ_low — _NSGetEnviron() from i386 code. Natively it returns
 * &environ, a char ** of native strings above 4GB; the i386 caller truncated
 * it (Civ IV's Python 2.6 builds os.environ this way at Init Python).
 *
 * Run with M64_ENV_PROBE=ok in the environment. Exit 42 = that entry found
 * through *_NSGetEnviron(). Kill switch M64_NO_ENVIRON_SHIM=1 (run time):
 * the truncated native pointer, a fault or a wrong exit.
 */
#include <stdlib.h>
#include <string.h>

extern char ***_NSGetEnviron(void);

int main(void) {
   char **env = *_NSGetEnviron();
   if (!env) exit(3);
   for (int i = 0; env[i] && i < 100000; ++i)
      if (!strcmp(env[i], "M64_ENV_PROBE=ok")) exit(42);
   exit(4);
}
