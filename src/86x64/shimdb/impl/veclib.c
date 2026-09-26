/* shimdb curated implementations: the pre-`vDSP_` vecLib FFT names.
 *
 * 10.4-era vecLib exported create_fftsetup / destroy_fftsetup / fft_zip (and
 * friends) alongside the vDSP_-prefixed names; modern Accelerate keeps only
 * the vDSP_ ones. Same arguments, same semantics — forward. (BASS audio in
 * Plants vs. Zombies binds these. Translated i386 callers reach them through

 */
#include <Accelerate/Accelerate.h>

FFTSetup create_fftsetup(vDSP_Length log2n, FFTRadix radix) {
    return vDSP_create_fftsetup(log2n, radix);
}
void destroy_fftsetup(FFTSetup setup) {
    vDSP_destroy_fftsetup(setup);
}
void fft_zip(FFTSetup setup, const DSPSplitComplex *io, vDSP_Stride stride,
             vDSP_Length log2n, FFTDirection dir) {
    vDSP_fft_zip(setup, io, stride, log2n, dir);
}
