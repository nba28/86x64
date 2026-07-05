// quicktime_movie_shim.c — shim for QuickTime Movie Toolbox entry _NewMovieFromDataRef.
//
// WHY THIS IS NEEDED (subtle — NOT a removed symbol): the bundled iLife11
// QuickTime.framework's export TRIE is incomplete — it lists 4500 symbols while its
// symbol table defines 4681 (181 are symtab-only). dyld resolves a modern two-level bind
// via the export TRIE, so Civ IV's now-eager bind to _NewMovieFromDataRef (one of the 181
// trie-missing entries) fails LOAD: "Symbol not found: _NewMovieFromDataRef, Expected in
// QuickTime.framework". (It went eager because the __jump_table undefined-half made formerly
// lazy QuickTime binds load-time; the lazy-bind table is now empty.) Across ALL of Civ IV's
// bundled-framework binds this is the ONLY trie-missing symbol (QuickTime 1, Python 0,
// libcrypto 0) — system frameworks use Apple's complete shared-cache tries.
//
// WHY A libabiconv SHIM (not trie completion): completing the framework trie needs core
// tooling (blocked) and would leave Civ IV bound DIRECTLY to native QuickTime — but all 35
// of Civ IV's QuickTime Movie Toolbox binds are direct-native (bare _X), i.e. a translated
// i386-cdecl call into a native x86_64 function = a latent ABI mismatch that crashes when
// invoked. Routing this entry through libabiconv (static-interpose renames _NewMovieFromDataRef
// -> ___NewMovieFromDataRef at the next translate) makes it RESOLVE at load AND lets us return
// a clean QuickTime error, so the caller takes its "couldn't open movie" path and SKIPS the
// follow-on EnterMovies/StartMovie/... calls (which are the other 34 ABI-mismatched binds) —
// turning an eventual crash into a graceful no-movie.
//
// ⚠This stubs movie LOADING, not fixes it. Making QuickTime movies actually play needs the
// whole Movie Toolbox routed through i386->x86_64 marshalling shims with impls resolved by a
// runtime symtab walk (dlsym can't see trie-missing symbols) — a separate, larger effort;
// flagged to the coordinator. Civ IV reaches it long after the LOAD/static-init wall this
// unblocks (movies only play in the frontend, well past dyld load + C++ static init).
//
// MTSHIM convention: rdi -> &i386 args[0]; OSErr result in eax.

#include <stdint.h>

#define PTR(n) ((void *)(uintptr_t)args[(n)])
// couldNotResolveDataRef (-2000): the canonical "this data reference can't be opened" QuickTime
// error; callers treat the movie as unavailable and move on without it.
#define QT_COULD_NOT_RESOLVE_DATAREF (-2000)

// OSErr NewMovieFromDataRef(Movie *theMovie, short newMovieFlags, short *idOut,
//                           Handle dataRef, OSType dataRefType);
uint32_t shim_NewMovieFromDataRef(uint32_t *args) {
    uint32_t *theMovie = (uint32_t *)PTR(0);   // out Movie -> NULL so caller never derefs it
    if (theMovie) *theMovie = 0;
    return (uint32_t)QT_COULD_NOT_RESOLVE_DATAREF;
}

// OSErr EnterMovies(void) — QuickTime Movie Toolbox initialization. The bundled
// (translated) QuickTime's real EnterMovies runs InitCodecManagerInternal and a
// deep tail of removed-QuickDraw / Component-Manager codec-registration calls
// that fault on modern macOS. Civ IV / Halo only need QuickTime for OPTIONAL
// frontend movies, which the NewMovieFromDataRef stub above already declines.
// Report success WITHOUT running the codec init: the caller believes QuickTime
// is available and proceeds into the game; every movie load then gracefully
// declines. This short-circuits the entire codec-init subsystem (a program that
// genuinely needs QuickTime playback would instead route the Movie Toolbox
// through real marshalling — the larger effort flagged above). Idempotent.
// EnterMoviesOnThread / ExitMovies mirror it (init/teardown no-ops).
uint32_t shim_EnterMovies(uint32_t *args)         { (void)args; return 0; }
uint32_t shim_EnterMoviesOnThread(uint32_t *args) { (void)args; return 0; }
uint32_t shim_ExitMovies(uint32_t *args)          { (void)args; return 0; }
uint32_t shim_ExitMoviesOnThread(uint32_t *args)  { (void)args; return 0; }
