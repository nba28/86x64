/*
 * vdsp_legacy_shim.c — the pre-`vDSP_` FFT names (create_fftsetup,
 * destroy_fftsetup, fft_zip) that 10.4-era vecLib exported and modern
 * Accelerate no longer does. BASS audio (Plants vs. Zombies' libbass.dylib)
 * binds them non-lazily; unresolved, its static init jumped to NULL.
 *
 * Same arguments and semantics as the vDSP_-prefixed calls, so each forwards.
 * i386 cdecl args arrive as 4-byte slots (maptable_tramp.asm MTSHIM).
 * The native FFTSetup is a >4GB pointer, so the app gets a low-4GB cell
 * holding it (libabiconv's malloc is the low heap). DSPSplitComplex is two
 * i386 4-byte pointers; widen it for the native call.
 */
#include <stdint.h>
#include <stdlib.h>
#include <Accelerate/Accelerate.h>

uint32_t shim_create_fftsetup(const uint32_t *a) {
   FFTSetup s = vDSP_create_fftsetup((vDSP_Length)a[0], (FFTRadix)(int32_t)a[1]);
   if (!s) { return 0; }
   FFTSetup *cell = malloc(sizeof *cell);
   if (!cell) { vDSP_destroy_fftsetup(s); return 0; }
   *cell = s;
   return (uint32_t)(uintptr_t)cell;
}

uint32_t shim_destroy_fftsetup(const uint32_t *a) {
   FFTSetup *cell = (FFTSetup *)(uintptr_t)a[0];
   if (cell) { vDSP_destroy_fftsetup(*cell); free(cell); }
   return 0;
}

uint32_t shim_fft_zip(const uint32_t *a) {
   FFTSetup *cell = (FFTSetup *)(uintptr_t)a[0];
   const uint32_t *sc = (const uint32_t *)(uintptr_t)a[1];   /* i386 DSPSplitComplex */
   if (!cell || !sc) { return 0; }
   DSPSplitComplex io = { (float *)(uintptr_t)sc[0], (float *)(uintptr_t)sc[1] };
   vDSP_fft_zip(*cell, &io, (vDSP_Stride)(int32_t)a[2], (vDSP_Length)a[3],
                (FFTDirection)(int32_t)a[4]);
   return 0;
}
