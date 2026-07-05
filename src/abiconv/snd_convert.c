/*
 * snd_convert.c — pure decoder for the classic Sound Manager memory formats.
 * See snd_convert.h. No framework dependencies; unit-tested by snd_convert_test.c.
 */
#include "snd_convert.h"
#include <string.h>

/* --- endian-parameterised field readers ------------------------------------ */
static uint16_t rd16(const uint8_t *p, int be) {
   return be ? (uint16_t)((p[0] << 8) | p[1])
             : (uint16_t)((p[1] << 8) | p[0]);
}
static uint32_t rd32(const uint8_t *p, int be) {
   return be ? ((uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
                (uint32_t)p[2] << 8  | (uint32_t)p[3])
             : ((uint32_t)p[3] << 24 | (uint32_t)p[2] << 16 |
                (uint32_t)p[1] << 8  | (uint32_t)p[0]);
}

/* A 16.16 UnsignedFixed sample rate is "plausible" if its integer part is a
 * real audio rate. Used to auto-detect the header's endianness. */
static int plausible_rate(uint32_t fixed) {
   uint32_t hz = fixed >> 16;
   return hz >= 4000 && hz <= 96000;
}

/* Field offsets (pack(2), i386). Shared by std/ext/cmp for the common prefix. */
#define OFF_SAMPLEPTR   0   /* Ptr / long   */
#define OFF_LEN_OR_CHAN 4   /* SoundHeader.length | Ext/Cmp.numChannels */
#define OFF_RATE        8   /* UnsignedFixed 16.16 */
#define OFF_ENCODE      20  /* UInt8 */
#define OFF_BASEFREQ    21  /* UInt8 */
/* Std inline samples */
#define STD_SAMPLEAREA  22
/* Ext/Cmp extra */
#define OFF_NUMFRAMES   22  /* unsigned long */
#define EXT_SAMPLESIZE  48  /* UInt16 */
#define EXT_SAMPLEAREA  64
#define CMP_FORMAT      40  /* OSType */
#define CMP_COMPID      56  /* short compressionID */
#define CMP_SAMPLESIZE  62  /* UInt16 */
#define CMP_SAMPLEAREA  64

int snd_header_parse(const uint8_t *hdr, uint32_t maxlen, uint32_t hdr_base,
                     snd_pcm_desc *out) {
   if (!hdr || !out || maxlen < 22) return -1;
   memset(out, 0, sizeof *out);

   uint8_t encode = hdr[OFF_ENCODE];
   if (encode != SND_STDSH && encode != SND_EXTSH && encode != SND_CMPSH)
      return -1;

   /* Auto-detect field endianness from the sample-rate field. Default LE
    * (native i386 struct); fall back to BE (classic 'snd ' resource). */
   int be = 0;
   uint32_t rate_le = rd32(hdr + OFF_RATE, 0);
   uint32_t rate_be = rd32(hdr + OFF_RATE, 1);
   if (plausible_rate(rate_le))      be = 0;
   else if (plausible_rate(rate_be)) be = 1;
   else be = 0;                       /* neither plausible: assume LE, proceed */

   uint32_t rate_fixed = rd32(hdr + OFF_RATE, be);
   out->sample_rate = (double)(rate_fixed >> 16) +
                      (double)(rate_fixed & 0xFFFF) / 65536.0;
   if (out->sample_rate < 1.0) out->sample_rate = 22254.545454; /* rate22khz */
   out->samplePtr  = rd32(hdr + OFF_SAMPLEPTR, be);
   out->encode     = encode;
   out->base_freq  = hdr[OFF_BASEFREQ];
   out->is_bigendian = be;

   if (encode == SND_STDSH) {
      /* 8-bit unsigned mono; length is in bytes == frames. */
      out->channels  = 1;
      out->bits      = 8;
      out->is_signed = 0;
      out->nbytes    = rd32(hdr + OFF_LEN_OR_CHAN, be);
      out->inline_off = STD_SAMPLEAREA;
      (void)hdr_base;
      return 0;
   }

   /* ext / cmp share numChannels @4, numFrames @22 */
   uint32_t channels = rd32(hdr + OFF_LEN_OR_CHAN, be);
   if (channels == 0) channels = 1;
   if (channels > 2)  channels = 2;
   uint32_t numFrames = (maxlen >= 26) ? rd32(hdr + OFF_NUMFRAMES, be) : 0;

   if (encode == SND_EXTSH) {
      uint32_t bits = (maxlen >= 50) ? rd16(hdr + EXT_SAMPLESIZE, be) : 16;
      if (bits != 8 && bits != 16) bits = 16;
      out->channels   = channels;
      out->bits       = bits;
      out->is_signed  = (bits == 16) ? 1 : 0;   /* classic 8-bit is unsigned */
      out->nbytes     = numFrames * channels * (bits / 8);
      out->inline_off = EXT_SAMPLEAREA;
      return 0;
   }

   /* SND_CMPSH — compressed (IMA4/MACE/...); decode not yet implemented. Fill
    * what we can so the caller can trace/decline gracefully. */
   out->channels   = channels;
   out->bits       = (maxlen >= 64) ? rd16(hdr + CMP_SAMPLESIZE, be) : 16;
   if (out->bits != 8 && out->bits != 16) out->bits = 16;
   out->is_signed  = (out->bits == 16) ? 1 : 0;
   out->cmp_format = (maxlen >= 44) ? rd32(hdr + CMP_FORMAT, be) : 0;
   out->cmp_id     = (maxlen >= 58) ? (int16_t)rd16(hdr + CMP_COMPID, be) : 0;
   out->inline_off = CMP_SAMPLEAREA;
   out->nbytes     = numFrames * channels * (out->bits / 8);
   return 1;   /* valid but compressed */
}

int snd_resource_find_header(const uint8_t *res, uint32_t reslen,
                             uint32_t res_base, uint32_t *hdr_off) {
   if (!res || !hdr_off || reslen < 6) return -1;
   /* 'snd ' resources are stored big-endian (classic on-disk format). */
   uint16_t format = rd16(res, 1);
   uint32_t p;      /* running offset into the resource */
   uint16_t numCommands;

   if (format == SND_FMT_1) {
      /* short format; short numModifiers; numModifiers * ModRef(6);
       * short numCommands; numCommands * SndCommand(8); data. */
      if (reslen < 4) return -1;
      uint16_t numMods = rd16(res + 2, 1);
      p = 4 + (uint32_t)numMods * 6;
      if (p + 2 > reslen) return -1;
      numCommands = rd16(res + p, 1);
      p += 2;
   } else if (format == SND_FMT_2) {
      /* short format; short refCount; short numCommands; commands; data. */
      if (reslen < 6) return -1;
      numCommands = rd16(res + 4, 1);
      p = 6;
   } else {
      return -1;
   }

   for (uint16_t i = 0; i < numCommands; i++) {
      if (p + 8 > reslen) return -1;
      uint16_t cmd    = rd16(res + p, 1);
      /* param1 at +2 (unused here) */
      uint32_t param2 = rd32(res + p + 4, 1);
      uint16_t base   = cmd & ~(uint16_t)SND_DATA_OFFSET_FLAG;
      if (base == snd_bufferCmd || base == snd_soundCmd) {
         if (cmd & SND_DATA_OFFSET_FLAG) {
            /* param2 is an offset from the resource start to the SoundHeader. */
            if (param2 >= reslen) return -1;
            *hdr_off = param2;
         } else {
            /* param2 is an absolute i386 pointer to the SoundHeader; express it
             * as an offset within the resource if it points inside, else the
             * caller must follow it directly (return the raw ptr via a sentinel
             * offset by making hdr_off == param2 - res_base only when inside). */
            if (param2 >= res_base && param2 < res_base + reslen)
               *hdr_off = param2 - res_base;
            else
               return -1;   /* out-of-resource abs ptr: caller path not this one */
         }
         return 0;
      }
      p += 8;
   }
   return -1;
}
