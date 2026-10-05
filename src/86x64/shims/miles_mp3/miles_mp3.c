/*
 * miles_mp3.c — an i386 stand-in for RAD Miles' MP3 decoder plugin.
 *
 * Miles (libMilesX86.dylib) decodes MP3 only through the "ASI codec" provider
 * in mssmp3.asi, which Mac builds ship as a Windows PE32 DLL and load with
 * Miles' own PE loader. A translated (x86_64) process cannot run that i386
 * code, so without it every MP3 stream is silently unplayable: Portal 2's
 * voice lines (sound/vo/..., MP3 behind a .wav name) were mute while the PCM
 * effects played.
 *
 * This provider is built for i386 against the game's own libMilesX86 and
 * translated with the rest of the tree, so Miles calls it like any other
 * translated code. A constructor registers it as a static provider
 * (RIB_load_static_provider_library) before AIL_startup runs. The interface
 * reproduces mssmp3.asi's RIB table (names, tokens, subtypes) and its entry
 * points' contracts, read from the original's disassembly; decoding is
 * minimp3 (CC0, LICENSE.minimp3).
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_SIMD
#include "minimp3.h"

typedef int32_t S32;
typedef uint32_t U32;

typedef void *(*MSS_ALLOC)(U32 size, U32 user, const char *file, U32 line);
typedef void (*MSS_FREE)(void *ptr, U32 user, const char *file, U32 line);
typedef S32 (*ASI_FETCH)(U32 user, void *dest, S32 bytes, S32 offset);   /* offset -1: sequential */

typedef struct { U32 type; const char *name; const void *token; U32 subtype; } RIB_ENTRY;   /* i386: 16 bytes */
typedef S32 (*RIB_REGISTER)(U32 provider, const char *iface, S32 n, const RIB_ENTRY *e);
typedef S32 (*RIB_UNREGISTER)(U32 provider, const char *iface, S32 n, const RIB_ENTRY *e);
typedef S32 (*RIB_MAIN)(U32 provider, S32 up, void *alloc, RIB_REGISTER reg, RIB_UNREGISTER unreg);
extern U32 RIB_load_static_provider_library(RIB_MAIN main, const char *name);

enum { RIB_FUNCTION = 0, RIB_PROPERTY = 1 };

enum { ASI_NOERR = 0, ASI_ALREADY_STARTED = 2, ASI_NOT_INIT = 8 };

#define IN_CAP 16384

typedef struct {
   MSS_ALLOC alloc;
   MSS_FREE free_;
   U32 user;
   ASI_FETCH fetch;
   U32 total;                 /* input size in bytes (0: unknown) */
   S32 fetch_at;              /* next fetch offset; -1 = sequential */
   U32 in_pos;                /* input bytes consumed ("Position") */
   S32 eof;
   mp3dec_t dec;
   int in_len;
   uint8_t in[IN_CAP];
   int pcm_bytes, pcm_off;    /* decoded but not yet returned */
   int16_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
   int rate, channels, bitrate_kbps, layer, version;
   uint64_t emitted;          /* samples returned ("Emitted samples") */
   S32 req[3];                /* requested rate / bit width / channels (stored, not applied) */
} stream;

static const char *g_err;
static int g_started;

static void fill(stream *s) {
   if (s->eof || s->in_len == IN_CAP) { return; }
   const S32 n = s->fetch(s->user, s->in + s->in_len, IN_CAP - s->in_len, s->fetch_at);
   s->fetch_at = -1;
   if (n <= 0) { s->eof = 1; return; }
   s->in_len += n;
}

/* Decode the next frame into s->pcm. 0 at end of stream. */
static int next_frame(stream *s) {
   for (;;) {
      if (s->in_len < IN_CAP / 2) { fill(s); }
      if (s->in_len == 0) { return 0; }
      mp3dec_frame_info_t info;
      const int samples = mp3dec_decode_frame(&s->dec, s->in, s->in_len, s->pcm, &info);
      if (info.frame_bytes == 0) {          /* no frame in what is buffered */
         if (s->eof) { s->in_len = 0; return 0; }
         if (s->in_len == IN_CAP) { s->in_len = 0; }   /* a full buffer of garbage: drop it */
         fill(s);
         continue;
      }
      s->in_len -= info.frame_bytes;
      memmove(s->in, s->in + info.frame_bytes, s->in_len);
      s->in_pos += info.frame_bytes;
      if (samples == 0) { continue; }       /* skipped (ID3, free format, sync) */
      s->rate = info.hz;
      s->channels = info.channels;
      s->bitrate_kbps = info.bitrate_kbps;
      s->layer = info.layer;
      s->version = info.hz >= 32000 ? 1 : 2;
      s->pcm_bytes = samples * info.channels * 2;
      s->pcm_off = 0;
      return 1;
   }
}

static void reset(stream *s) {
   mp3dec_init(&s->dec);
   s->in_len = 0;
   s->pcm_bytes = s->pcm_off = 0;
   s->eof = 0;
}

/* ---- ASI codec ---------------------------------------------------------- */

static S32 provider_property(S32 index, void *before, const void *nv, void *after) {
   (void)nv; (void)after;
   if (!before) { return 0; }
   switch (index) {
   case -100: *(const char **)before = "MSS MPEG Layer 3 Audio Decoder"; return 1;   /* Name */
   case -101: *(U32 *)before = 0x130; return 1;                                      /* Version */
   case 0:                                                                           /* Input file types */
   case 1: *(const char **)before = "MPEG Layer 3 audio files\0*.MP3\0"; return 1;   /* Editor-supported */
   case 2: *(U32 *)before = 0x55; return 1;                                          /* Input wave tag */
   case 3: *(const char **)before = "Raw PCM files\0*.RAW\0"; return 1;              /* Output file types */
   case 4: *(U32 *)before = 0x800; return 1;                                         /* Maximum frame size */
   default: return 0;
   }
}

static S32 asi_startup(void) {
   if (g_started++) { g_err = "Already started"; return ASI_ALREADY_STARTED; }
   g_err = NULL;
   return ASI_NOERR;
}

static const char *asi_error(void) { return g_err; }

static S32 asi_shutdown(void) {
   if (g_started == 0) { g_err = "Not initialized"; return ASI_NOT_INIT; }
   --g_started;
   return ASI_NOERR;
}

/* ---- ASI stream --------------------------------------------------------- */

static U32 stream_open(MSS_ALLOC alloc, MSS_FREE free_, U32 user, ASI_FETCH fetch, U32 total) {
   stream *s = alloc(sizeof *s, user, __FILE__, __LINE__);
   if (!s) { g_err = "Out of memory"; return 0; }
   memset(s, 0, sizeof *s);
   s->alloc = alloc; s->free_ = free_; s->user = user; s->fetch = fetch; s->total = total;
   s->fetch_at = -1;
   s->req[0] = s->req[1] = s->req[2] = -1;
   reset(s);
   if (!next_frame(s)) {                    /* the format must be known after open */
      g_err = "MPEG audio header not found or is badly formatted";
      free_(s, user, __FILE__, __LINE__);
      return 0;
   }
   return (U32)(uintptr_t)s;
}

static S32 stream_process(U32 h, void *buffer, S32 size) {
   stream *s = (stream *)(uintptr_t)h;
   uint8_t *out = buffer;
   S32 done = 0;
   while (done < size) {
      if (s->pcm_off == s->pcm_bytes && !next_frame(s)) { break; }
      int n = s->pcm_bytes - s->pcm_off;
      if (n > size - done) { n = size - done; }
      memcpy(out + done, (uint8_t *)s->pcm + s->pcm_off, n);
      s->pcm_off += n;
      done += n;
      s->emitted += (uint64_t)n / (2u * (unsigned)s->channels);
   }
   if (done < size) { memset(out + done, 0, size - done); }   /* as mssmp3: silence past the end */
   return done;
}

/* -2: restart the sample count and keep decoding; any other offset (or -1,
 * "continue") drops buffered data and resumes fetching at that offset. */
static S32 stream_seek(U32 h, S32 offset) {
   stream *s = (stream *)(uintptr_t)h;
   s->emitted = 0;
   if (offset == -2) { s->fetch_at = -1; return ASI_NOERR; }
   reset(s);
   s->fetch_at = offset;
   if (offset >= 0) { s->in_pos = (U32)offset; }
   return ASI_NOERR;
}

static S32 stream_close(U32 h) {
   stream *s = (stream *)(uintptr_t)h;
   s->free_(s, s->user, __FILE__, __LINE__);
   return ASI_NOERR;
}

static S32 stream_property(U32 h, U32 index, void *before, const void *nv, void *after) {
   stream *s = (stream *)(uintptr_t)h;
   if (index >= 0x14 && index <= 0x16) {    /* Requested rate / bit width / channels */
      S32 *r = &s->req[index - 0x14];
      if (before) { *(S32 *)before = *r; }
      if (nv) { *r = *(const S32 *)nv; }
      if (after) { *(S32 *)after = *r; }
      return 1;
   }
   if (!before) { return 0; }
   const U32 pcm_bps = (U32)s->rate * (U32)s->channels * 16u;
   U32 v;
   switch (index) {
   case 0x5:  v = (U32)s->bitrate_kbps * 1000u; break;                  /* Input bit rate */
   case 0x6:                                                             /* Input sample rate */
   case 0xa:  v = (U32)s->rate; break;                                   /* Output sample rate */
   case 0x7: {                                                           /* Input sample width */
      U32 r = s->bitrate_kbps ? pcm_bps / ((U32)s->bitrate_kbps * 1000u) : 1;
      v = 16u / (r ? r : 1);
      break;
   }
   case 0x8:                                                             /* Input channels */
   case 0xc:  v = (U32)s->channels; break;                               /* Output channels */
   case 0x9:  v = pcm_bps; break;                                        /* Output bit rate */
   case 0xb:  v = 16; break;                                             /* Output sample width */
   case 0xd:  v = (U32)(s->pcm_bytes - s->pcm_off); break;               /* Output reservoir */
   case 0xe:  v = s->in_pos; break;                                      /* Position */
   case 0xf:  memcpy(before, &s->emitted, 8); return 1;                  /* Emitted samples */
   case 0x10: *(float *)before = s->total ? (float)s->in_pos * 100.0f / (float)s->total : 0.0f;
              return 1;                                                  /* Percent done */
   case 0x11: v = 0x800; break;                                          /* Minimum input block size */
   case 0x12: v = (U32)s->version; break;                                /* MPEG version */
   case 0x13: v = (U32)s->layer; break;                                  /* MPEG layer */
   default:   return 0;
   }
   *(U32 *)before = v;
   return 1;
}

/* ---- registration (mssmp3.asi's table) ------------------------------------ */

/* property subtypes copied verbatim from mssmp3.asi (0x80000000 = read-only) */
#define FN(n, f)            { RIB_FUNCTION, n, (const void *)(f), 0 }
#define PROP(n, tok, sub)   { RIB_PROPERTY, n, (const void *)(intptr_t)(tok), (sub) }

static const RIB_ENTRY codec[] = {
   FN("PROVIDER_property", provider_property),
   PROP("Name", -100, 0x80000007),
   PROP("Version", -101, 0x80000003),
   PROP("Input file types", 0, 0x80000007),
   PROP("Editor-supported input file types", 1, 0x80000007),
   PROP("Input wave tag", 2, 0x80000002),
   PROP("Output file types", 3, 0x80000007),
   PROP("Maximum frame size", 4, 0x80000002),
   FN("ASI_startup", asi_startup),
   FN("ASI_error", asi_error),
   FN("ASI_shutdown", asi_shutdown),
   PROP("Profile index", 0x1002, 0x80000002),
   PROP("SPU flags", 0x40000000, 0x80000002),
};

static const RIB_ENTRY strm[] = {
   FN("ASI_stream_open", stream_open),
   FN("ASI_stream_process", stream_process),
   FN("ASI_stream_property", stream_property),
   FN("ASI_stream_seek", stream_seek),
   FN("ASI_stream_close", stream_close),
   PROP("Input bit rate", 5, 0x80000002),
   PROP("Input sample rate", 6, 0x80000002),
   PROP("Input sample width", 7, 0x80000002),
   PROP("Input channels", 8, 0x80000002),
   PROP("Output bit rate", 9, 0x80000002),
   PROP("Output sample rate", 0xa, 0x80000002),
   PROP("Output sample width", 0xb, 0x80000002),
   PROP("Output channels", 0xc, 0x80000002),
   PROP("Output reservoir", 0xd, 0x80000002),
   PROP("Position", 0xe, 0x80000002),
   PROP("Percent done", 0x10, 0x80000005),
   PROP("Minimum input block size", 0x11, 0x80000002),
   PROP("MPEG version", 0x12, 0x80000002),
   PROP("MPEG layer", 0x13, 0x80000002),
   PROP("Emitted samples", 0xf, 0x80000001),
   PROP("Requested sample rate", 0x14, 2),
   PROP("Requested bit width", 0x15, 2),
   PROP("Requested # of channels", 0x16, 2),
};

#define N(a) ((S32)(sizeof(a) / sizeof((a)[0])))

static S32 rib_main(U32 provider, S32 up, void *alloc, RIB_REGISTER reg, RIB_UNREGISTER unreg) {
   (void)alloc;
   if (!up) { unreg(provider, NULL, 0, NULL); return 1; }
   reg(provider, "ASI codec", N(codec), codec);
   reg(provider, "ASI stream", N(strm), strm);
   return 1;
}

/* Kill M64_NO_MILES_MP3=1 (no provider: MP3 streams stay mute); guard miles-mp3. */
__attribute__((constructor)) static void miles_mp3_register(void) {
   if (getenv("M64_NO_MILES_MP3")) { return; }
   RIB_load_static_provider_library(rib_main, "mssmp3");
}
