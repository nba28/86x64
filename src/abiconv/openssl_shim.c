/*
 * openssl_shim.c — the OpenSSL 0.9.x libcrypto calls legacy apps imported from
 * /usr/lib/libcrypto.0.9.7.dylib, reimplemented on CommonCrypto.
 *
 * macOS no longer ships a usable system libcrypto (the file is gone or aborts
 * on load), so a weak-imported libcrypto call was a raw cross-ABI jump: Quinn's
 * server builds its password-challenge nonce with RAND_pseudo_bytes(buf, 20)
 * while answering REQ_SERVER_INFO, and the native `ret` popped 8 bytes off the
 * i386 caller's 4-byte return slot -> fused PC -> crash. Quinn also uses
 * SHA1 and HMAC(EVP_sha1(), ...) for its challenge/response and highscore HMAC.
 *
 * Covered: SHA1, EVP_sha1 (+ EVP_md5 for HMAC-MD5), HMAC, RAND_pseudo_bytes,
 * RAND_bytes. Called through MTSHIM trampolines (maptable_tramp.asm):
 * `a` points at the i386 cdecl argument words; the return value goes in eax.
 * All pointers are the app's own low-4GB buffers; the static fallbacks for a
 * NULL `md` live in libabiconv's (low) __DATA, as OpenSSL's did.
 */
#include <stdint.h>
#include <stdlib.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonHMAC.h>

/* EVP_MD handles are opaque to the app: hand out the address of a tag. */
static const uint32_t g_evp_sha1 = 1, g_evp_md5 = 2;

#define P(x) ((void *)(uintptr_t)(x))

/* unsigned char *SHA1(const unsigned char *d, size_t n, unsigned char *md) */
uint32_t shim_SHA1(const uint32_t *a) {
   static unsigned char s_md[CC_SHA1_DIGEST_LENGTH];
   unsigned char *md = a[2] ? P(a[2]) : s_md;
   CC_SHA1(P(a[0]), (CC_LONG)a[1], md);
   return (uint32_t)(uintptr_t)md;
}

/* const EVP_MD *EVP_sha1(void) / EVP_md5(void) */
uint32_t shim_EVP_sha1(const uint32_t *a) { (void)a; return (uint32_t)(uintptr_t)&g_evp_sha1; }
uint32_t shim_EVP_md5(const uint32_t *a)  { (void)a; return (uint32_t)(uintptr_t)&g_evp_md5; }

/* unsigned char *HMAC(const EVP_MD *evp_md, const void *key, int key_len,
 *                     const unsigned char *d, int n,
 *                     unsigned char *md, unsigned int *md_len) */
uint32_t shim_HMAC(const uint32_t *a) {
   static unsigned char s_md[CC_SHA1_DIGEST_LENGTH];
   CCHmacAlgorithm alg;
   unsigned len;
   if (a[0] == (uint32_t)(uintptr_t)&g_evp_sha1)     { alg = kCCHmacAlgSHA1; len = CC_SHA1_DIGEST_LENGTH; }
   else if (a[0] == (uint32_t)(uintptr_t)&g_evp_md5) { alg = kCCHmacAlgMD5;  len = CC_MD5_DIGEST_LENGTH; }
   else { return 0; }                          /* unknown digest: OpenSSL fails */
   unsigned char *md = a[5] ? P(a[5]) : s_md;
   CCHmac(alg, P(a[1]), (size_t)(int32_t)a[2], P(a[3]), (size_t)(int32_t)a[4], md);
   if (a[6]) { *(uint32_t *)P(a[6]) = len; }
   return (uint32_t)(uintptr_t)md;
}

/* int RAND_pseudo_bytes(unsigned char *buf, int num) / RAND_bytes(...) */
uint32_t shim_RAND_pseudo_bytes(const uint32_t *a) {
   if ((int32_t)a[1] > 0) { arc4random_buf(P(a[0]), (size_t)a[1]); }
   return 1;
}
uint32_t shim_RAND_bytes(const uint32_t *a) { return shim_RAND_pseudo_bytes(a); }
