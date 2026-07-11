/* 95_crypto_cdkey_verify.c — regression for the OpenSSL 0.9.7 CD-key/product-key
 * RSA-verify bridge (libabiconv crypto_shim.c / crypto_tramp.asm).
 *
 * WHY: Halo imports a small OpenSSL surface for its product-key check
 * (CRYPTO_set_mem_functions, RIPEMD160*, BN_bin2bn, RSA_new/verify/free). Those
 * symbols resolve to the LIVE native libcrypto.0.9.7 in the shared cache, so
 * abigen left them as raw weak binds -> when Halo called _CRYPTO_set_mem_functions
 * the native 8-byte `ret` over-popped the i386 4-byte cdecl frame -> fused PC
 * EXC_BAD_ACCESS (Halo-2026-07-11-233750.ips), the wall right before the GL
 * renderer. static-interpose now renames the imports to the ___ shim spelling
 * (undefined dynamic_lookup at link, resolved at translate time — the same shape
 * as tests 39/49/79/85), and crypto_shim.c marshals the i386 cdecl frame.
 *
 * This uses ONLY the nine bridged functions and mirrors Halo's exact pattern:
 *   - CRYPTO_set_mem_functions(m,r,f)  -> no-op returning 1 (its 3 args are the
 *     app's OWN i386 allocators; must NOT be handed to native OpenSSL).
 *   - RIPEMD160(d,n,md) one-shot + RIPEMD160_Init/Update/Final streaming: both
 *     yield the published RIPEMD160 digest of "abc".
 *   - RSA_new + BN_bin2bn(n)+BN_bin2bn(e) written into rsa->n (@0x10) and rsa->e
 *     (@0x14) by hand [the i386 0.9.7 RSA struct has n@0x10/e@0x14, matching the
 *     shim's low-4GB shadow], then RSA_verify against an embedded RSA-512 public
 *     key + a precomputed valid PKCS#1 signature over RIPEMD160("abc"): a valid
 *     sig verifies (1) and a tampered hash is rejected (0). RSA_free tears down.
 *
 * A broken shim (wrong no-op, wrong RIPEMD160 marshalling, wrong RSA shadow
 * offset, verify not reaching native) mis-prints or crashes. All buffers live in
 * the translated app's low-4GB space, so the shim's i386 pointer widening is
 * exercised for real. exit 42 == all pass.
 */

#include <openssl/ripemd.h>
#include <openssl/rsa.h>
#include <openssl/bn.h>
#include <string.h>
extern int printf(const char *, ...);
extern void exit(int);

/* NID_ripemd160 (avoids pulling <openssl/objects.h>); RSA_verify type arg. */
#define NID_RIPEMD160 117

/* Precomputed RSA-512 public key (n,e) and a valid PKCS#1v1.5 signature over
 * RIPEMD160("abc"), generated offline and confirmed to verify against the
 * shared-cache libcrypto.0.9.7. Deliberately NON-const (mutable) so they land in
 * __data, exactly like Halo's hardcoded pubkey blobs (RE: __DATA,__data). A
 * `static const` byte array would land in __const, where an unrelated
 * translator interior-const-read quirk shifts the read — a pre-existing issue
 * orthogonal to this crypto bridge, and one the real target never hits (its
 * blobs are __data). */
static unsigned char PUB_N[64] = {
   0xcf,0xf9,0x69,0x9d,0x6d,0x02,0xdc,0x92,0x0e,0x8b,0x64,0x44,
   0xfa,0x9b,0x1b,0x6f,0x86,0x20,0x85,0x33,0x8c,0xed,0x7c,0xd3,
   0xe8,0x85,0x5a,0xfd,0x82,0x29,0x9e,0xb7,0xba,0x17,0x9d,0x76,
   0x80,0xcb,0xd1,0x13,0x36,0xee,0x18,0xed,0x7f,0x47,0x24,0x6c,
   0x74,0xb0,0x6f,0xbc,0x3a,0x3d,0x64,0x13,0x20,0x6c,0xbd,0x64,
   0x41,0xb3,0xe2,0x07,
};
static unsigned char PUB_E[3] = { 0x01,0x00,0x01 };
static unsigned char SIG[64] = {
   0x97,0xc0,0x4a,0x4c,0x52,0xd4,0x0e,0x13,0xb9,0x73,0x72,0xa6,
   0x9c,0x80,0x10,0x42,0xc8,0x6b,0x8c,0x93,0x1e,0x88,0x7e,0x76,
   0x72,0x4a,0x26,0xb0,0x91,0xac,0x27,0x8b,0x6a,0x30,0x1f,0x8d,
   0x12,0xab,0xb2,0xbb,0x30,0x11,0x47,0xd8,0x8c,0x87,0xc0,0x57,
   0x6c,0x30,0x4c,0x86,0x37,0x49,0x97,0x98,0x86,0xb4,0x5c,0x0e,
   0xa8,0x15,0x9a,0xb3,
};

/* dummy i386 allocator callbacks handed to CRYPTO_set_mem_functions (never
 * actually invoked — the shim is a no-op). */
static void *my_malloc(size_t n){ (void)n; return 0; }
static void *my_realloc(void *p, size_t n){ (void)p; (void)n; return 0; }
static void  my_free(void *p){ (void)p; }

int main(void) {
   int ok = 1;

   /* --- A) CRYPTO_set_mem_functions no-op --- */
   int mem = CRYPTO_set_mem_functions(my_malloc, my_realloc, my_free);
   ok &= (mem == 1);
   printf("A set_mem=%d\n", mem);

   /* --- B) RIPEMD160 one-shot + streaming over "abc" --- (non-const => __data,
    * see the PUB_N note re: the __const interior-read quirk.) */
   static unsigned char abc[3] = { 'a','b','c' };
   static unsigned char want[20] = {
      0x8e,0xb2,0x08,0xf7,0xe0,0x5d,0x98,0x7a,0x9b,0x04,
      0x4a,0x8e,0x98,0xc6,0xb0,0x87,0xf1,0x5a,0x0b,0xfc };
   unsigned char md[20];
   memset(md, 0, sizeof md);
   RIPEMD160(abc, 3, md);
   int oneshot = (memcmp(md, want, 20) == 0);

   RIPEMD160_CTX c;
   unsigned char md2[20];
   RIPEMD160_Init(&c);
   RIPEMD160_Update(&c, abc, 3);
   RIPEMD160_Final(md2, &c);
   int stream = (memcmp(md2, want, 20) == 0);
   ok &= oneshot & stream;
   printf("B ripemd_oneshot=%d ripemd_stream=%d\n", oneshot, stream);

   /* --- C) RSA build-by-hand + verify, mirroring Halo --- */
   RSA *pub = RSA_new();
   pub->n = BN_bin2bn(PUB_N, (int)sizeof PUB_N, 0);   /* Halo: movl %eax,0x10(rsa) */
   pub->e = BN_bin2bn(PUB_E, (int)sizeof PUB_E, 0);   /* Halo: movl %eax,0x14(rsa) */

   int v_good = RSA_verify(NID_RIPEMD160, md, 20, (unsigned char *)SIG,
                           (unsigned)sizeof SIG, pub);
   md[0] ^= 0xff;
   int v_bad = RSA_verify(NID_RIPEMD160, md, 20, (unsigned char *)SIG,
                          (unsigned)sizeof SIG, pub);
   md[0] ^= 0xff;

   RSA_free(pub);

   ok &= (v_good == 1) & (v_bad == 0);
   printf("C verify_good=%d verify_bad=%d\n", v_good, v_bad);

   printf("result=%s\n", ok ? "PASS" : "FAIL");
   exit(ok ? 42 : 1);
   return 0;
}
