/* 99_bundled_owner_bind — the app's own dylib exports a function whose name is
 * also a Carbon API. Source's vstdlib CreateThreadPool returns a thread-pool
 * object; the Thread Manager's CreateThreadPool(style, count, stack) returns an
 * OSErr. The import is bound to the @loader_path dylib, so the translation must
 * keep it there rather than reroute it into libabiconv's same-named bridge,
 * both in the bind table and in the load-time import repair (guard
 * bundled-owner-bind). The bundled stub returns 0x5150. */
#include <stdlib.h>
extern int CreateThreadPool(void);
int main(void) { exit(CreateThreadPool() == 0x5150 ? 0 : 1); }
