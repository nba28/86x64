// crypto_shim.c — i386->x86_64 marshalling shims for the small OpenSSL 0.9.7
// surface Halo imports for its product-key (CD-key) RSA-signature check
// (wired through crypto_tramp.asm's CRYPTO_MTSHIM* -> ___<sym> exports).
//
// WHY: these are PRESENT native functions (libcrypto.0.9.7.dylib lives in the
// dyld shared cache; every symbol resolves) but abigen never shimmed them, so
// the translated app's imports were left as raw weak binds straight to the
// native x86_64 libcrypto. The moment Halo called one, the native 8-byte `ret`
// over-popped the i386 4-byte cdecl return frame -> fused PC crash
// (Halo-2026-07-11-233750.ips: EXC_BAD_ACCESS at 0x02086cb00203564a, the crash
// was _CRYPTO_set_mem_functions, the FIRST crypto call = OpenSSL lazy init).
// static-interpose renames the app's `_CRYPTO_set_mem_functions`/`_RSA_*`/... to
// `___...`; these provide them with a correct i386-cdecl-return unwind.
//
// TWO marshalling problems solved here:
//
// 1. CRYPTO_set_mem_functions(malloc_cb, realloc_cb, free_cb): the 3 args are
//    the translated app's OWN i386 allocator functions. Forwarding them to
//    native libcrypto would make native OpenSSL call an i386 function pointer
//    with the x86_64 ABI -> recrash. So this is a NO-OP that returns 1
//    (success): native OpenSSL keeps its default allocators, and because every
//    RSA/BN object below is allocated+freed inside native libcrypto, allocator
//    ownership stays entirely native and consistent. (Semantically transparent:
//    Halo only installed custom allocators for leak-tracking; the crypto still
//    works with the defaults.)
//
// 2. RSA/BIGNUM struct layout: Halo builds its RSA public key BY HAND, writing
//    the BIGNUM* from BN_bin2bn directly into the RSA struct at i386 offsets
//    rsa->n @ 0x10 and rsa->e @ 0x14 (`movl %eax,0x10(%rsa)` /
//    `movl %eax,0x14(%rsa)`). The native x86_64 RSA struct has 8-byte fields, so
//    those offsets are WRONG for native libcrypto. We therefore hand Halo an
//    i386-LAYOUT shadow RSA (a low-4GB buffer whose 0x10/0x14 slots Halo pokes),
//    and BN_bin2bn returns a low-4GB shadow BIGNUM token that carries the native
//    BIGNUM*. At RSA_verify time we read the shadow's n/e tokens, materialize a
//    real native RSA (RSA_new + assign the BIGNUM*s), and call native
//    RSA_verify. Halo treats RSA*/BIGNUM* opaquely apart from those two writes
//    (verified by disassembly), so a shadow is safe.
//
// The RIPEMD160 family takes only data/ctx BUFFERS (all low-4GB stack pointers,
// and the i386 & modern RIPEMD160_CTX are both 96 bytes) so they forward to
// native directly with just pointer widening.
//
// MTSHIM convention: `args` -> &i386 args[0] (4-byte cdecl slots). Returns land
// in eax (crypto_tramp.asm fixes the i386 4-byte-frame unwind).

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <dlfcn.h>

/* Opaque native OpenSSL types — we never dereference them, only pass the
 * pointers native libcrypto handed us back. */
typedef void RSA;
typedef void BIGNUM;

/* Low-4GB scratch allocator provided by the wrapper (weak: only present once
 * the wrapper executable is in the process). Halo's RSA/BIGNUM handles are
 * stored in 32-bit i386 slots and written into 32-bit i386 struct fields, so
 * every shadow we return MUST live below 4GB. */
extern void *_86x64_alloc_low_4gb(size_t) __attribute__((weak));

/* Resolve a native libcrypto entry, caching it. We resolve EXCLUSIVELY from the
 * pinned libcrypto.0.9.7.dylib handle, never RTLD_DEFAULT: modern macOS also
 * ships a much newer libcrypto (3.x) in the shared cache, and RTLD_DEFAULT can
 * bind a symbol (e.g. RSA_free) to the 3.x copy while another (e.g. RSA_new)
 * came from 0.9.7 — mixing two OpenSSL ABIs on the SAME RSA object corrupts it
 * (the 3.x RSA struct layout differs) and aborts in RSA_free. Pinning all
 * resolutions to the one 0.9.7 image (the exact library the app's weak imports
 * name) keeps every RSA/BN/RIPEMD operation on a single consistent ABI, which
 * also justifies rsa_native()'s hard-coded 0.9.7 n@32/e@40 offsets. The image
 * lives in the dyld shared cache; dlopen by versioned name resolves it. */
static void *crypto_dlsym(const char *name)
{
   static void *h = NULL;
   if (!h) h = dlopen("/usr/lib/libcrypto.0.9.7.dylib", RTLD_LAZY | RTLD_GLOBAL);
   if (!h) return NULL;
   return dlsym(h, name);
}
#define NDL(fn, ret, argl) \
   static ret (*fn) argl = NULL; \
   if (!fn) fn = (ret (*) argl)crypto_dlsym(#fn)

/* --- i386-layout shadow structs (low-4GB) --------------------------------- */

/* Shadow BIGNUM: Halo never dereferences the BIGNUM* it gets from BN_bin2bn —
 * it only stores it (into rsa->n / rsa->e) and hands it back to us implicitly
 * via the RSA at RSA_verify. So the shadow just needs to carry the real native
 * BIGNUM*. A small tagged low-4GB record does that. */
typedef struct {
   uint32_t tag;         /* CRYPTO_BN_TAG */
   uint32_t pad;
   BIGNUM  *native;      /* real native BIGNUM* (8 bytes; its ADDRESS is high but
                            THIS record's address is low-4GB) */
} bn_shadow_t;
#define CRYPTO_BN_TAG 0x424E5348u   /* "BNSH" */

/* Shadow RSA: an i386-layout RSA public key. Halo writes:
 *   rsa->n @ offset 0x10  (a shadow BIGNUM* token, low-4GB)
 *   rsa->e @ offset 0x14  (a shadow BIGNUM* token, low-4GB)
 * We keep a tag + a materialized native RSA* built lazily at verify time. The
 * layout below places n/e at exactly 0x10/0x14 so Halo's raw stores land in the
 * right slots. Everything before 0x10 is opaque padding Halo may zero (it does
 * `RSA_new` then only touches 0x10/0x14 — confirmed by disasm). */
typedef struct {
   uint32_t tag;         /* 0x00  CRYPTO_RSA_TAG */
   uint32_t reserved0;   /* 0x04 */
   RSA     *native;      /* 0x08  materialized native RSA* (built at verify) */
   uint32_t n_tok;       /* 0x10  <- Halo writes rsa->n here */
   uint32_t e_tok;       /* 0x14  <- Halo writes rsa->e here */
} rsa_shadow_t;
#define CRYPTO_RSA_TAG 0x52534148u  /* "RSAH" */

/* --- CRYPTO_set_mem_functions: NO-OP, return success ---------------------- */

/* int CRYPTO_set_mem_functions(void*(*m)(size_t), void*(*r)(void*,size_t),
 *                              void(*f)(void*)); i386 args a[0..2] = the app's
 * OWN i386 allocator callbacks. Do NOT forward (native OpenSSL would call an
 * i386 fn-ptr with the x86_64 ABI). Return 1 = installed; native OpenSSL keeps
 * its defaults, which is transparent for the crypto that follows. */
uint32_t shim_CRYPTO_set_mem_functions(uint32_t *a)
{
   (void)a;
   return 1;
}

/* --- RIPEMD160 family: forward to native (buffers only) ------------------- */

/* void RIPEMD160_Init(RIPEMD160_CTX *c); ctx is a low-4GB stack buffer, and the
 * i386 & modern RIPEMD160_CTX are both 96 bytes, so forward the pointer. */
uint32_t shim_RIPEMD160_Init(uint32_t *a)
{
   NDL(RIPEMD160_Init, int, (void *));
   if (RIPEMD160_Init) RIPEMD160_Init((void *)(uintptr_t)a[0]);
   return 1;
}

/* int RIPEMD160_Update(RIPEMD160_CTX *c, const void *data, size_t len); */
uint32_t shim_RIPEMD160_Update(uint32_t *a)
{
   NDL(RIPEMD160_Update, int, (void *, const void *, size_t));
   if (RIPEMD160_Update)
      return (uint32_t)RIPEMD160_Update((void *)(uintptr_t)a[0],
                                        (const void *)(uintptr_t)a[1],
                                        (size_t)a[2]);
   return 1;
}

/* int RIPEMD160_Final(unsigned char *md, RIPEMD160_CTX *c); i386 arg order is
 * (md, ctx) — a[0]=md, a[1]=ctx. */
uint32_t shim_RIPEMD160_Final(uint32_t *a)
{
   NDL(RIPEMD160_Final, int, (unsigned char *, void *));
   if (RIPEMD160_Final)
      return (uint32_t)RIPEMD160_Final((unsigned char *)(uintptr_t)a[0],
                                       (void *)(uintptr_t)a[1]);
   return 1;
}

/* unsigned char *RIPEMD160(const unsigned char *d, unsigned long n,
 *                          unsigned char *md); one-shot. Returns md (low-4GB). */
uint32_t shim_RIPEMD160(uint32_t *a)
{
   NDL(RIPEMD160, unsigned char *, (const unsigned char *, size_t, unsigned char *));
   if (RIPEMD160)
      RIPEMD160((const unsigned char *)(uintptr_t)a[0], (size_t)a[1],
                (unsigned char *)(uintptr_t)a[2]);
   return a[2];   /* return the md buffer (i386 caller expects the ptr in eax) */
}

/* --- BIGNUM: BN_bin2bn -> low-4GB shadow carrying a native BIGNUM ---------- */

/* BIGNUM *BN_bin2bn(const unsigned char *s, int len, BIGNUM *ret); Halo always
 * passes ret=NULL (a[2]==0). Forward to native with the low-4GB data buffer,
 * wrap the native BIGNUM* in a low-4GB shadow, and return the shadow token. */
uint32_t shim_BN_bin2bn(uint32_t *a)
{
   NDL(BN_bin2bn, BIGNUM *, (const unsigned char *, int, BIGNUM *));
   if (!BN_bin2bn || !_86x64_alloc_low_4gb) return 0;
   BIGNUM *nb = BN_bin2bn((const unsigned char *)(uintptr_t)a[0], (int)a[1], NULL);
   if (!nb) return 0;
   bn_shadow_t *sh = (bn_shadow_t *)_86x64_alloc_low_4gb(sizeof(bn_shadow_t));
   if (!sh) return 0;
   sh->tag = CRYPTO_BN_TAG;
   sh->pad = 0;
   sh->native = nb;
   return (uint32_t)(uintptr_t)sh;   /* low-4GB shadow token */
}

static BIGNUM *bn_native(uint32_t tok)
{
   if (!tok) return NULL;
   bn_shadow_t *sh = (bn_shadow_t *)(uintptr_t)tok;
   if (sh->tag != CRYPTO_BN_TAG) return NULL;
   return sh->native;
}

/* --- RSA: shadow key + native verify -------------------------------------- */

/* RSA *RSA_new(void); return an i386-layout shadow whose 0x10/0x14 slots Halo
 * pokes with n/e BIGNUM tokens. */
uint32_t shim_RSA_new(uint32_t *a)
{
   (void)a;
   if (!_86x64_alloc_low_4gb) return 0;
   rsa_shadow_t *sh = (rsa_shadow_t *)_86x64_alloc_low_4gb(sizeof(rsa_shadow_t));
   if (!sh) return 0;
   memset(sh, 0, sizeof(*sh));
   sh->tag = CRYPTO_RSA_TAG;
   return (uint32_t)(uintptr_t)sh;
}

/* Materialize (once) a native RSA from the shadow's n/e tokens. OpenSSL 0.9.7's
 * shared-cache libcrypto exposes the RSA struct with n/e as plain BIGNUM*
 * fields. We set them by native struct offset (x86_64 0.9.7 RSA:
 * pad(int,4)+version(long,@8)+meth(ptr,@16)+engine(ptr,@24)+ n @32 + e @40).
 * Guarded by the tag; only reached from RSA_verify. */
static RSA *rsa_native(rsa_shadow_t *sh)
{
   if (sh->native) return sh->native;
   NDL(RSA_new, RSA *, (void));
   if (!RSA_new) return NULL;
   RSA *r = RSA_new();
   if (!r) return NULL;
   /* x86_64 OpenSSL 0.9.7 RSA: n @ offset 32, e @ offset 40 (see comment). */
   BIGNUM **np = (BIGNUM **)((char *)r + 32);
   BIGNUM **ep = (BIGNUM **)((char *)r + 40);
   *np = bn_native(sh->n_tok);
   *ep = bn_native(sh->e_tok);
   sh->native = r;
   return r;
}

/* int RSA_verify(int type, const unsigned char *m, unsigned int m_len,
 *                unsigned char *sigbuf, unsigned int siglen, RSA *rsa);
 * i386 args a[0..5]. Rebuild/resolve the native RSA from the shadow, forward. */
uint32_t shim_RSA_verify(uint32_t *a)
{
   NDL(RSA_verify, int, (int, const unsigned char *, unsigned int,
                         const unsigned char *, unsigned int, RSA *));
   if (!RSA_verify) return 0;
   uint32_t rsa_tok = a[5];
   if (!rsa_tok) return 0;
   rsa_shadow_t *sh = (rsa_shadow_t *)(uintptr_t)rsa_tok;
   if (sh->tag != CRYPTO_RSA_TAG) return 0;
   RSA *r = rsa_native(sh);
   if (!r) return 0;
   return (uint32_t)RSA_verify((int)a[0],
                               (const unsigned char *)(uintptr_t)a[1],
                               (unsigned int)a[2],
                               (const unsigned char *)(uintptr_t)a[3],
                               (unsigned int)a[4], r);
}

/* void RSA_free(RSA *rsa); free the native RSA (which owns the assigned n/e
 * BIGNUMs via RSA_free's internal BN_free) and drop the shadow (the low-4GB
 * scratch is a bump allocator with no free; leaking a handful of ~32-byte
 * shadows over the CD-key check is harmless). */
uint32_t shim_RSA_free(uint32_t *a)
{
   uint32_t rsa_tok = a[0];
   if (!rsa_tok) return 0;
   rsa_shadow_t *sh = (rsa_shadow_t *)(uintptr_t)rsa_tok;
   if (sh->tag != CRYPTO_RSA_TAG) return 0;
   if (sh->native) {
      NDL(RSA_free, void, (RSA *));
      if (RSA_free) RSA_free(sh->native);
      sh->native = NULL;
   }
   sh->tag = 0;   /* poison so a double-free/verify after free is caught */
   return 0;
}
