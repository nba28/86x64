// quicktime_movie_shim.c — QuickTime Movie Toolbox init/teardown no-ops.
//
// The REAL Movie Toolbox playback surface (NewMovieFromDataRef, StartMovie,
// MoviesTask, GetMovieBox, ... -> AVFoundation) now lives in the companion
// quicktime_movie_bridge.m. This file keeps only the init/teardown entries that
// must succeed WITHOUT running the classic codec/component registration.
//
// EnterMovies is the QuickTime Movie Toolbox initialization. The bundled
// (translated) QuickTime's real EnterMovies runs InitCodecManagerInternal and a
// deep tail of removed-QuickDraw / Component-Manager codec-registration calls
// that fault on modern macOS. With AVFoundation owning decode (bridge), that
// classic codec init is dead — so EnterMovies/InitCodecManager report success
// without running it, and the app proceeds to open + play movies via the bridge.
//
// MTSHIM convention: rdi -> &i386 args[0]; OSErr result in eax.

#include <stdint.h>

// OSErr EnterMovies(void) — QuickTime Movie Toolbox initialization. The bundled
// (translated) QuickTime's real EnterMovies runs InitCodecManagerInternal and a
// deep tail of removed-QuickDraw / Component-Manager codec-registration calls
// that fault on modern macOS. AVFoundation (quicktime_movie_bridge.m) owns decode,
// so that classic codec init is dead. Report success WITHOUT running it: the app
// believes QuickTime is available and proceeds to open + play movies through the
// bridge. This short-circuits the entire classic codec-init subsystem. Idempotent.
// EnterMoviesOnThread / ExitMovies mirror it (init/teardown no-ops).
uint32_t shim_EnterMovies(uint32_t *args)         { (void)args; return 0; }
uint32_t shim_EnterMoviesOnThread(uint32_t *args) { (void)args; return 0; }
uint32_t shim_ExitMovies(uint32_t *args)          { (void)args; return 0; }
uint32_t shim_ExitMoviesOnThread(uint32_t *args)  { (void)args; return 0; }
