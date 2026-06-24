/* shimdb curated implementations: classic Mac OS Memory Manager block moves.
 *
 * Removed from modern macOS, but every one is a real, simple, universal
 * primitive -- a return-0 auto-stub silently drops the copy/clear and corrupts
 * downstream state (this is the canonical "shimgen broke a framework" bug).
 * A curator writes the true semantics here ONCE; shimgen links only the ones a
 * given app actually binds into that app's shim and dead-strips the rest.
 */
#include <string.h>

typedef long Size;   /* classic 32/64-bit byte count */

/* BlockMoveData(srcPtr, destPtr, byteCount) -- overlap-safe copy. */
void BlockMoveData(const void *srcPtr, void *destPtr, Size byteCount) {
    if (srcPtr && destPtr && byteCount > 0)
        memmove(destPtr, srcPtr, (size_t)byteCount);
}
void BlockMove(const void *srcPtr, void *destPtr, Size byteCount) {
    if (srcPtr && destPtr && byteCount > 0)
        memmove(destPtr, srcPtr, (size_t)byteCount);
}
/* "Uncached" variants: the cache hint is meaningless on x86_64; same copy. */
void BlockMoveDataUncached(const void *srcPtr, void *destPtr, Size byteCount) {
    if (srcPtr && destPtr && byteCount > 0)
        memmove(destPtr, srcPtr, (size_t)byteCount);
}
void BlockMoveUncached(const void *srcPtr, void *destPtr, Size byteCount) {
    if (srcPtr && destPtr && byteCount > 0)
        memmove(destPtr, srcPtr, (size_t)byteCount);
}
/* BlockZero(destPtr, byteCount) / BlockZeroData -- clear a range. */
void BlockZero(void *destPtr, Size byteCount) {
    if (destPtr && byteCount > 0) memset(destPtr, 0, (size_t)byteCount);
}
void BlockZeroData(void *destPtr, Size byteCount) {
    if (destPtr && byteCount > 0) memset(destPtr, 0, (size_t)byteCount);
}
