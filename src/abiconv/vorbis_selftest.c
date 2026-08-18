/*
 * vorbis_selftest.c — DIAGNOSTIC ONLY (task #46).  Call Halo's own translated
 * libVorbis `ov_open_callbacks` directly, on a bitstream we control, and report
 * exactly what it returns.
 *
 * ⚠★DELETE THIS FILE (and its CMakeLists entry) WHEN #46 CLOSES.  It ships
 * inert: nothing runs unless ABICONV_VORBIS_SELFTEST is set in the environment.
 *
 * WHY.  #46 is "44100 Hz music is silent while 22050 Hz SFX play".  It is
 * localised to Halo's statically linked libVorbis: `ov_open_callbacks` (i386
 * 0x2d6988) NEVER succeeds, every sample, every run, on a bitstream that is
 * provably intact (21 pages, page 0 BOS, `\x01vorbis` ident, 0 CRC failures,
 * and ffmpeg decodes it to 5.28 s of real audio).  A whole-image pcmap audit
 * has since ruled out mis-translated INSTRUCTIONS across all 699,396 rows, so
 * the fault is in data, in a bridged libc call, or in runtime state.
 *
 * The previous probes could only WATCH: they saw the failure but not its
 * REASON, and every new question cost another human-driven run.  This one
 * DRIVES the decoder instead, so a single run answers:
 *   - do the four callbacks behave correctly when called in isolation
 *     (in particular SEEK, which takes a 64-bit ogg_int64_t as two stack
 *     words -- the remaining structural suspect);
 *   - what error code ov_open_callbacks actually returns (OV_ENOTVORBIS vs
 *     OV_EBADHEADER vs OV_EREAD point at completely different subsystems);
 *   - how many bytes the decoder consumed before giving up.
 *
 * HOW.  libabiconv already owns the primitive: __86x64_call_i386 lays an i386
 * cdecl word frame on a fresh low-4GB stack and enters translated code
 * (objc_reverse.asm).  We reuse HALO'S OWN callbacks and build a datasource in
 * its documented shape, so nothing here has to bridge native->i386.
 *
 * ADDRESSES ARE RESOLVED FROM __DATA,__86x64_pcmap, NEVER HARDCODED.  A
 * hardcoded translated address is stale the moment anything is retranslated,
 * and would silently call into the middle of some other function.  We look up
 * the i386 addresses (which are frozen -- Apps32 is read-only) and read the
 * translated entry points out of the very instructions Halo uses to install
 * them.  See resolve_targets() for the two shapes and their checks.
 *
 * WHAT IS ASSERTED vs ASSUMED.  Every resolved address is range-checked and
 * shape-checked before use, and the run declines loudly rather than calling a
 * guess.  The OggVorbis_File post-mortem is interpreted ONLY if its first word
 * still equals the datasource we passed (libvorbisfile 1.0 memsets the whole
 * struct in ov_clear on a failed open, so all-zero is the EXPECTED outcome and
 * is reported as such, not as a layout failure).
 */
#include <dlfcn.h>
#include <fcntl.h>
#include <mach-o/dyld.h>
#include <mach-o/getsect.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* objc_reverse.asm: lay `nwords` i386 cdecl args + a return frame on the
 * provided low-4GB stack, enter the translated fn, return edx:eax in rax. */
uint64_t _86x64_call_i386(uint64_t fn, uint64_t nwords, const uint32_t *words,
                          uint64_t lowstack_top);

#define VS_STACK_SZ (1u * 1024u * 1024u)

/* ---- i386 anchors (frozen: Apps32/Halo is read-only) ---------------------
 * Derived by disassembly, cross-checked against pcmap; see the journal entry
 * for 5c52740 and src/86x64/pcmap-diff.py.
 *   0x2468ca  the function that installs the callbacks and calls ov_open
 *   0x2469fe  the `call ov_open_callbacks`
 *   0x246a03  the instruction AFTER that call (has a pcmap row; the call
 *             itself does not, because the translator expands it)
 *   0x2468d1  first mapped instruction of the installer (its prologue has no
 *             row -- prologues become a different blob type)
 */
#define I386_INSTALLER_FIRST  0x002468d1u
#define I386_AFTER_OVOPEN     0x00246a03u

/* ov_callbacks slot offsets in the outgoing i386 frame, from the disassembly:
 *   mov [esp+0x10], read   mov [esp+0x14], seek
 *   mov [esp+0x18], close  mov [esp+0x1c], tell                              */
#define CB_SLOT_READ  0x10
#define CB_SLOT_SEEK  0x14
#define CB_SLOT_CLOSE 0x18
#define CB_SLOT_TELL  0x1c

/* Halo's Ogg datasource, decoded from the callbacks themselves (read cb
 * 0x246b3a, seek 0x246a5a, tell 0x246a3a, close 0x246a1e). 16 bytes. */
struct halo_ds {
   uint32_t pos;     /* +0x00 current offset                                 */
   uint32_t base;    /* +0x04 i386 pointer to the bitstream (close() zeroes)  */
   uint32_t len;     /* +0x08 total length                                    */
   uint8_t  eof;     /* +0x0c sticky short-read flag                          */
   uint8_t  pad[3];
};

/* libvorbisfile return codes (vorbis/codec.h). */
static const char *ov_err(int32_t r) {
   switch (r) {
      case 0:    return "OK";
      case -1:   return "OV_FALSE";
      case -2:   return "OV_EOF";
      case -3:   return "OV_HOLE";
      case -128: return "OV_EREAD (a read callback failed)";
      case -129: return "OV_EFAULT (internal logic fault / NULL)";
      case -130: return "OV_EIMPL";
      case -131: return "OV_EINVAL";
      case -132: return "OV_ENOTVORBIS (no vorbis ident found in the stream)";
      case -133: return "OV_EBADHEADER (a header packet was malformed)";
      case -134: return "OV_EVERSION (bitstream version mismatch)";
      case -135: return "OV_ENOTAUDIO";
      case -136: return "OV_EBADPACKET";
      case -137: return "OV_EBADLINK";
      case -138: return "OV_ENOSEEK";
      default:   return "(unknown)";
   }
}

#define VS(...) do { fprintf(stderr, "[vorbis] " __VA_ARGS__); \
                     fflush(stderr); } while (0)

/* ---- pcmap ------------------------------------------------------------- */
struct pcmap_row { int32_t trans_off; uint32_t orig; };

static const struct pcmap_row *g_rows;
static uint32_t                g_nrows;
static const uint8_t          *g_text;      /* loaded __text base            */
static uint64_t                g_text_vm;   /* its address as an integer     */
static uint64_t                g_text_sz;

/* The translated image is the one carrying __DATA,__86x64_pcmap. Identified
 * STRUCTURALLY (it has the section) rather than by filename, so this keeps
 * working if the bundle layout changes; the name is only used in the report. */
static const char *pcmap_load(void) {
   if (g_rows) { return NULL; }
   const uint32_t n = _dyld_image_count();
   for (uint32_t i = 0; i < n; i++) {
      const struct mach_header *mh = _dyld_get_image_header(i);
      if (!mh || mh->magic != MH_MAGIC_64) { continue; }
      const struct mach_header_64 *m64 = (const struct mach_header_64 *)mh;
      unsigned long psz = 0, tsz = 0;
      const uint8_t *pc = getsectiondata(m64, "__DATA", "__86x64_pcmap", &psz);
      if (!pc) { pc = getsectiondata(m64, "__TEXT", "__86x64_pcmap", &psz); }
      if (!pc || psz < 8) { continue; }
      const uint8_t *tx = getsectiondata(m64, "__TEXT", "__text", &tsz);
      if (!tx) { continue; }
      const uint32_t magic = *(const uint32_t *)pc;
      const uint32_t count = *(const uint32_t *)(pc + 4);
      if (magic != 0x366d6370u) { continue; }      /* "pcm6" */
      if ((uint64_t)count * 8 + 8 > (uint64_t)psz) { continue; }
      g_rows = (const struct pcmap_row *)(pc + 8);
      g_nrows = count;
      g_text = tx;
      g_text_vm = (uint64_t)(uintptr_t)tx;
      g_text_sz = (uint64_t)tsz;
      VS("image %s: %u pcmap rows, __text %p..%p\n",
         _dyld_get_image_name(i) ? _dyld_get_image_name(i) : "?",
         count, (const void *)tx, (const void *)(tx + tsz));
      return NULL;
   }
   return "no loaded image carries __DATA,__86x64_pcmap";
}

/* orig i386 vmaddr -> loaded translated address, or 0. Rows are sorted by
 * trans_off, not by orig, so this is a linear scan; it runs a handful of
 * times, once. */
static uint64_t pcmap_trans(uint32_t orig) {
   for (uint32_t i = 0; i < g_nrows; i++) {
      if (g_rows[i].orig == orig) {
         return g_text_vm + (int64_t)g_rows[i].trans_off;
      }
   }
   return 0;
}

/* ---- resolved targets --------------------------------------------------- */
static uint64_t g_ovopen, g_read, g_seek, g_close, g_tell;

/*
 * Two shapes, both read out of the instructions Halo itself executes:
 *
 * (a) the CALL. The translator expands `call X` into
 *        lea r11,[rip+ret] ; push ax ; push ax ; mov [rsp],r11d ; jmp X
 *     so the five bytes immediately before the return site are `e9 rel32`,
 *     and the callee is (return site) + rel32.
 *
 * (b) the four CALLBACK INSTALLS. Each `movl $cb,<slot>` becomes
 *        4c 8d 1d <disp32>      lea r11,[rip+disp32]
 *        44 89 5c 24 <imm8>     mov [rsp+imm8], r11d
 *     so the target is (address after the lea) + disp32, and the imm8 says
 *     WHICH callback it is. Reading the slot number instead of assuming an
 *     order means a compiler reordering cannot silently swap two callbacks.
 *
 * The installer has TWO arms (streaming vs one-shot) that install the same
 * four pointers; we scan both and require them to agree, which is a free
 * cross-check on the whole decode.
 */
static const char *resolve_targets(void) {
   const uint64_t after = pcmap_trans(I386_AFTER_OVOPEN);
   const uint64_t first = pcmap_trans(I386_INSTALLER_FIRST);
   if (!after)  { return "no pcmap row for the instruction after the ov_open call"; }
   if (!first)  { return "no pcmap row for the installer's first instruction"; }
   if (after <= first || after - first > 0x2000) {
      return "installer span is implausible - the anchors no longer match";
   }
   const uint8_t *jmp = (const uint8_t *)(uintptr_t)(after - 5);
   if (*jmp != 0xe9) { return "no `jmp rel32` before the ov_open return site"; }
   g_ovopen = after + (int64_t)*(const int32_t *)(jmp + 1);
   if (g_ovopen < g_text_vm || g_ovopen >= g_text_vm + g_text_sz) {
      return "resolved ov_open_callbacks is outside __text";
   }

   int nfound = 0, disagree = 0;
   for (const uint8_t *p = (const uint8_t *)(uintptr_t)first;
        p + 12 <= (const uint8_t *)(uintptr_t)after; p++) {
      if (!(p[0] == 0x4c && p[1] == 0x8d && p[2] == 0x1d)) { continue; }
      if (!(p[7] == 0x44 && p[8] == 0x89 && p[9] == 0x5c && p[10] == 0x24)) {
         continue;   /* a lea r11 that does not feed an [rsp+imm8] store */
      }
      const uint64_t tgt = (uint64_t)(uintptr_t)(p + 7)
                         + (int64_t)*(const int32_t *)(p + 3);
      uint64_t *slot = NULL;
      switch (p[11]) {
         case CB_SLOT_READ:  slot = &g_read;  break;
         case CB_SLOT_SEEK:  slot = &g_seek;  break;
         case CB_SLOT_CLOSE: slot = &g_close; break;
         case CB_SLOT_TELL:  slot = &g_tell;  break;
         default: continue;
      }
      if (*slot == 0) { *slot = tgt; nfound++; }
      else if (*slot != tgt) { disagree++; }
   }
   if (nfound != 4) { return "did not find all four callback installs"; }
   if (disagree) { return "the installer's two arms disagree on a callback"; }
   const uint64_t all[4] = { g_read, g_seek, g_close, g_tell };
   for (int i = 0; i < 4; i++) {
      if (all[i] < g_text_vm || all[i] >= g_text_vm + g_text_sz) {
         return "a resolved callback is outside __text";
      }
   }
   return NULL;
}

/* ---- calling translated code ------------------------------------------- */
static uint64_t g_stack_top;

static int32_t call32(uint64_t fn, int nwords, const uint32_t *w) {
   return (int32_t)(uint32_t)_86x64_call_i386(fn, (uint64_t)nwords, w,
                                              g_stack_top);
}

/* Everything translated code touches must be addressable in 32 bits. Our
 * malloc is libabiconv's low-4GB heap, so this holds by construction -- but it
 * is asserted rather than assumed, because a silent truncation here would look
 * exactly like a decoder bug. */
static int lo32(const void *p, const char *what) {
   if ((uint64_t)(uintptr_t)p >> 32) {
      VS("FATAL: %s allocated ABOVE 4GB (%p) - cannot pass to translated code\n",
         what, p);
      return 0;
   }
   return 1;
}

static void dump_ds(const char *tag, const struct halo_ds *ds) {
   VS("  %-8s pos=%u base=0x%08x len=%u eof=%u\n",
      tag, ds->pos, ds->base, ds->len, ds->eof);
}

/* ---- the run ------------------------------------------------------------ */

/* Part 1: the four callbacks, in isolation. If one of these is wrong, part 2's
 * error code is a consequence and not a clue. */
static void probe_callbacks(uint8_t *buf, uint32_t len, struct halo_ds *ds) {
   VS("--- part 1: callbacks called directly ---\n");
   memset(ds, 0, sizeof *ds);
   ds->base = (uint32_t)(uintptr_t)buf;
   ds->len  = len;
   const uint32_t ds32 = (uint32_t)(uintptr_t)ds;
   uint32_t w[4];

   w[0] = ds32;
   int32_t t0 = call32(g_tell, 1, w);
   VS("  tell(ds)            -> %d   %s\n", t0, t0 == 0 ? "OK" : "⚠expected 0");

   uint8_t *rb = (uint8_t *)malloc(64);
   if (!rb || !lo32(rb, "read buffer")) { return; }
   memset(rb, 0, 64);
   w[0] = (uint32_t)(uintptr_t)rb; w[1] = 1; w[2] = 4; w[3] = ds32;
   int32_t nr = call32(g_read, 4, w);
   VS("  read(buf,1,4,ds)    -> %d   got '%c%c%c%c'   %s\n", nr,
      rb[0] ? rb[0] : '?', rb[1] ? rb[1] : '?', rb[2] ? rb[2] : '?',
      rb[3] ? rb[3] : '?',
      (nr == 4 && memcmp(rb, "OggS", 4) == 0) ? "OK" : "⚠the READ callback is wrong");
   free(rb);

   /* _ov_open1 calls seek(f,0,SEEK_CUR) before parsing a single header, and
    * ogg_int64_t arrives as TWO stack words. */
   w[0] = ds32; w[1] = 0; w[2] = 0; w[3] = 1;
   int32_t sc = call32(g_seek, 4, w);
   VS("  seek(ds,0,SEEK_CUR) -> %d   %s\n", sc,
      sc == 0 ? "OK (seekable)" : "⚠non-zero: the stream would be marked UNSEEKABLE");

   w[0] = ds32; w[1] = 0; w[2] = 0; w[3] = 2;
   int32_t se = call32(g_seek, 4, w);
   w[0] = ds32;
   int32_t te = call32(g_tell, 1, w);
   VS("  seek(ds,0,SEEK_END) -> %d   then tell -> %d (len=%u)   %s\n",
      se, te, len, (te == (int32_t)len) ? "OK" : "⚠END/tell disagree with the length");

   w[0] = ds32; w[1] = 0; w[2] = 0; w[3] = 0;
   VS("  seek(ds,0,SEEK_SET) -> %d\n", call32(g_seek, 4, w));

   /* A 64-bit offset that does not fit in 32 bits: if the high word is dropped
    * anywhere in the bridge this is accepted instead of refused. */
   w[0] = ds32; w[1] = 0; w[2] = 1; w[3] = 0;
   int32_t sh = call32(g_seek, 4, w);
   VS("  seek(ds,2^32,SET)   -> %d   %s\n", sh,
      sh != 0 ? "OK (refused, so the HIGH word survives)"
              : "⚠ACCEPTED - the 64-bit offset's high word is being LOST");
}

/* Part 2: ov_open_callbacks on one bitstream. Returns its result. */
static int32_t probe_open(const char *label, uint8_t *buf, uint32_t len,
                          struct halo_ds *ds) {
   memset(ds, 0, sizeof *ds);
   ds->base = (uint32_t)(uintptr_t)buf;
   ds->len  = len;
   const uint32_t ds32 = (uint32_t)(uintptr_t)ds;

   /* sizeof(OggVorbis_File) is 0x2c0, derived BY CONSTRUCTION: Halo's two vf
    * slots sit at state+8 and state+0x2c8 with the success flag at state+0x588,
    * so they are adjacent and 0x2c0 apart. Over-allocate anyway - it costs
    * nothing, and a struct bigger than we think would corrupt the heap. */
   uint32_t *vf = (uint32_t *)malloc(4096);
   if (!vf || !lo32(vf, "OggVorbis_File")) { return 1; }
   memset(vf, 0, 4096);

   uint32_t a[8];
   a[0] = ds32; a[1] = (uint32_t)(uintptr_t)vf; a[2] = 0; a[3] = 0;
   a[4] = (uint32_t)g_read; a[5] = (uint32_t)g_seek;
   a[6] = (uint32_t)g_close; a[7] = (uint32_t)g_tell;

   const int32_t rc = call32(g_ovopen, 8, a);
   VS("  %-10s ov_open -> %-5d %s\n", label, rc, ov_err(rc));
   VS("             consumed %u of %u bytes (eof=%u)%s\n",
      ds->pos, len, ds->eof,
      ds->pos == 0 ? "  <-- NOTHING read: it failed before the first read" : "");
   if (len > 0 && ds->pos > 8500) {
      VS("             ⚠that is %u chunk(s) of 8500; a correct decoder needs the "
         "headers only\n", (ds->pos + 8499) / 8500);
   }
   if (ds->base == 0) {
      VS("             the CLOSE callback ran (it zeroes base) -> the failure "
         "was past ogg_sync_init\n");
   }
   int allzero = 1;
   for (int i = 0; i < 24; i++) { if (vf[i]) { allzero = 0; break; } }
   if (rc != 0 && allzero) {
      VS("             OggVorbis_File all zero -> ov_clear() ran (expected)\n");
   } else if (vf[0] == ds32) {
      VS("             vf (layout CORROBORATED by word0==datasource): "
         "seekable=%u offset=%u:%u end=%u:%u\n", vf[1], vf[3], vf[2], vf[5], vf[4]);
      VS("             oy: data=0x%08x storage=%d fill=%d returned=%d "
         "unsynced=%d headerbytes=%d bodybytes=%d\n",
         vf[6], (int32_t)vf[7], (int32_t)vf[8], (int32_t)vf[9],
         (int32_t)vf[10], (int32_t)vf[11], (int32_t)vf[12]);
   } else if (!allzero) {
      VS("             vf word0=0x%08x != datasource 0x%08x - layout NOT "
         "corroborated, raw only\n", vf[0], ds32);
   }
   free(vf);
   return rc;
}

/* Load one file into low-4GB memory. Returns NULL and explains on failure. */
static uint8_t *load_low(const char *path, uint32_t *out_len) {
   int fd = open(path, O_RDONLY);
   if (fd < 0) { VS("  %s: cannot open - SKIPPED\n", path); return NULL; }
   off_t sz = lseek(fd, 0, SEEK_END);
   lseek(fd, 0, SEEK_SET);
   if (sz <= 0 || sz > 64 * 1024 * 1024) {
      VS("  %s: implausible size %lld - SKIPPED\n", path, (long long)sz);
      close(fd); return NULL;
   }
   uint8_t *b = (uint8_t *)malloc((size_t)sz);
   if (!b || !lo32(b, "bitstream buffer")) { close(fd); return NULL; }
   if (read(fd, b, (size_t)sz) != (ssize_t)sz) {
      VS("  %s: short read - SKIPPED\n", path); close(fd); free(b); return NULL;
   }
   close(fd);
   *out_len = (uint32_t)sz;
   return b;
}

/* `paths` is a colon-separated list. The FIRST is the control (the bitstream
 * Halo itself hands the decoder) and is the only one part 1 runs on; the rest
 * are single-variable VARIANTS whose whole purpose is to differ from it in one
 * respect, so what matters is how their result differs from the control's. */
static void run_selftest(const char *paths) {
   const char *err = pcmap_load();
   if (err) { VS("DECLINED: %s\n", err); return; }
   if ((err = resolve_targets()) != NULL) {
      VS("DECLINED: %s\n", err);
      VS("  (nothing was called; this is a resolution failure, NOT a result)\n");
      return;
   }
   VS("resolved: ov_open=%#llx read=%#llx seek=%#llx close=%#llx tell=%#llx\n",
      (unsigned long long)g_ovopen, (unsigned long long)g_read,
      (unsigned long long)g_seek, (unsigned long long)g_close,
      (unsigned long long)g_tell);

   void *stk = malloc(VS_STACK_SZ);
   if (!stk || !lo32(stk, "i386 stack")) { return; }
   g_stack_top = ((uint64_t)(uintptr_t)stk + VS_STACK_SZ) & ~0xfULL;

   struct halo_ds *ds = (struct halo_ds *)malloc(sizeof *ds);
   if (!ds || !lo32(ds, "datasource")) { free(stk); return; }

   char list[2048];
   snprintf(list, sizeof list, "%s", paths);
   int first = 1, n = 0;
   char *save = NULL;
   for (char *tok = strtok_r(list, ":", &save); tok;
        tok = strtok_r(NULL, ":", &save)) {
      uint32_t len = 0;
      uint8_t *buf = load_low(tok, &len);
      if (!buf) { continue; }
      const char *base = strrchr(tok, '/');
      base = base ? base + 1 : tok;
      VS("bitstream %s: %u bytes, page0 header_type=0x%02x%s\n",
         base, len, len > 5 ? buf[5] : 0,
         (len > 5 && (buf[5] & 0x02)) ? "  (BOS - a legal stream start)"
                                      : "  ⚠BOS NOT SET");
      if (first) {
         probe_callbacks(buf, len, ds);
         VS("--- part 2: ov_open_callbacks, control then variants ---\n");
      }
      /* the control runs twice: a differing pair would mean the result is not
       * deterministic, and every comparison below would be meaningless */
      probe_open(base, buf, len, ds);
      if (first) { probe_open("(repeat)", buf, len, ds); first = 0; }
      free(buf);
      n++;
   }
   if (n == 0) { VS("no bitstream could be loaded - nothing was measured\n"); }
   VS("--- done ---\n");
   free(ds); free(stk);
}

static void *selftest_thread(void *arg) {
   const char *path = (const char *)arg;
   const char *ds = getenv("ABICONV_VORBIS_SELFTEST_DELAY");
   unsigned delay = ds ? (unsigned)strtoul(ds, NULL, 10) : 12u;
   if (delay > 600) { delay = 600; }
   /* Wait for Halo to finish loading: the probe needs Halo.dylib mapped and
    * the low-4GB heap live. It does NOT need the sound system to work, which
    * is the point -- the silent voice never enqueues, so hanging this off the
    * audio path would make it depend on the very thing under investigation. */
   sleep(delay);
   VS("=== SELFTEST (diagnostic, task #46) after %us ===\n", delay);
   run_selftest(path);
   return NULL;
}

__attribute__((constructor))
static void vorbis_selftest_init(void) {
   const char *p = getenv("ABICONV_VORBIS_SELFTEST");
   if (!p || !*p) { return; }         /* ships inert */
   static char path[2048];
   snprintf(path, sizeof path, "%s", p);
   pthread_t th;
   if (pthread_create(&th, NULL, selftest_thread, path) == 0) {
      pthread_detach(th);
   }
}
