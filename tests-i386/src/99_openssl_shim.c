/* 99_openssl_shim.c — libcrypto 0.9.7 calls from a legacy app.
 * SHA1("abc") and HMAC-SHA1 against their published test vectors, and
 * RAND_pseudo_bytes filling a buffer. Exit 42 = all correct. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct env_md_st EVP_MD;
extern unsigned char *SHA1(const unsigned char *d, unsigned long n, unsigned char *md);
extern const EVP_MD *EVP_sha1(void);
extern unsigned char *HMAC(const EVP_MD *evp, const void *key, int key_len,
                           const unsigned char *d, int n,
                           unsigned char *md, unsigned int *md_len);
extern int RAND_pseudo_bytes(unsigned char *buf, int num);

static void hex(const unsigned char *p, int n, char *out) {
   for (int i = 0; i < n; ++i) sprintf(out + 2 * i, "%02x", p[i]);
}

int main(void) {
   unsigned char md[20]; char h[41];
   SHA1((const unsigned char *)"abc", 3, md); hex(md, 20, h);
   int sha_ok = !strcmp(h, "a9993e364706816aba3e25717850c26c9cd0d89d");
   unsigned int len = 0;   /* RFC 2202 test case 2 */
   HMAC(EVP_sha1(), "Jefe", 4, (const unsigned char *)"what do ya want for nothing?", 28, md, &len);
   hex(md, 20, h);
   int hmac_ok = len == 20 && !strcmp(h, "effcdf6ae5eb2fa2d27416d5f184df9c259a7c79");
   unsigned char buf[20] = {0}, zero[20] = {0};
   int r = RAND_pseudo_bytes(buf, 20);
   int rand_ok = r == 1 && memcmp(buf, zero, 20) != 0;
   printf("sha1=%d hmac=%d rand=%d\n", sha_ok, hmac_ok, rand_ok);
   exit(sha_ok && hmac_ok && rand_ok ? 42 : 1);
}
