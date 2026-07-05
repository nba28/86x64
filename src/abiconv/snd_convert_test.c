/*
 * snd_convert_test.c — headless unit test for the pure classic Sound Manager
 * decoder (snd_convert.c). Builds i386-layout SoundHeaders and a 'snd ' resource
 * with known PCM and verifies snd_header_parse / snd_resource_find_header derive
 * the correct sample rate / channels / bit depth / sample location / byte count.
 *
 * No CoreAudio, no speaker: this validates the marshalling + format derivation
 * that feeds AudioQueueEnqueueBuffer in sndmgr_shim.c. Build & run:
 *   cc -Wall -Wextra -o /tmp/snd_test snd_convert.c snd_convert_test.c && /tmp/snd_test
 */
#include "snd_convert.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int g_fail = 0;
#define CHECK(cond, ...) do { if (!(cond)) { \
      printf("  FAIL: " __VA_ARGS__); printf("  [%s:%d]\n", __func__, __LINE__); \
      g_fail++; } } while (0)

/* little-endian field writers (simulate an i386 app building the struct) */
static void w16le(uint8_t *p, uint16_t v){ p[0]=v&0xff; p[1]=v>>8; }
static void w32le(uint8_t *p, uint32_t v){ p[0]=v&0xff; p[1]=(v>>8)&0xff; p[2]=(v>>16)&0xff; p[3]=v>>24; }
static void w16be(uint8_t *p, uint16_t v){ p[0]=v>>8; p[1]=v&0xff; }
static void w32be(uint8_t *p, uint32_t v){ p[0]=v>>24; p[1]=(v>>16)&0xff; p[2]=(v>>8)&0xff; p[3]=v&0xff; }

/* --- stdSH: 8-bit unsigned mono, inline samples ---------------------------- */
static void test_stdSH_inline(void) {
   printf("test_stdSH_inline:\n");
   uint8_t buf[22 + 100];
   memset(buf, 0, sizeof buf);
   w32le(buf + 0, 0);                       /* samplePtr = 0 -> inline */
   w32le(buf + 4, 100);                     /* length = 100 bytes */
   w32le(buf + 8, 22254u << 16);            /* sampleRate 22254.xx Hz (16.16) */
   buf[20] = SND_STDSH;                      /* encode */
   buf[21] = 60;                             /* baseFrequency = middle C */
   for (int i = 0; i < 100; i++) buf[22 + i] = (uint8_t)(128 + i);  /* samples */

   snd_pcm_desc d;
   int r = snd_header_parse(buf, sizeof buf, 0x10000000, &d);
   CHECK(r == 0, "parse rc=%d\n", r);
   CHECK(d.channels == 1, "channels=%u\n", d.channels);
   CHECK(d.bits == 8, "bits=%u\n", d.bits);
   CHECK(d.is_signed == 0, "is_signed=%d\n", d.is_signed);
   CHECK(fabs(d.sample_rate - 22254.0) < 1.0, "rate=%.2f\n", d.sample_rate);
   CHECK(d.samplePtr == 0, "samplePtr=%u\n", d.samplePtr);
   CHECK(d.inline_off == 22, "inline_off=%u\n", d.inline_off);
   CHECK(d.nbytes == 100, "nbytes=%u\n", d.nbytes);
   CHECK(d.base_freq == 60, "base_freq=%u\n", d.base_freq);
   if (!g_fail) printf("  ok\n");
}

/* --- stdSH: external samplePtr --------------------------------------------- */
static void test_stdSH_ptr(void) {
   printf("test_stdSH_ptr:\n");
   uint8_t buf[22];
   memset(buf, 0, sizeof buf);
   w32le(buf + 0, 0x08123456);              /* absolute i386 samplePtr */
   w32le(buf + 4, 4096);
   w32le(buf + 8, 11025u << 16);
   buf[20] = SND_STDSH;
   buf[21] = 60;
   snd_pcm_desc d;
   int r = snd_header_parse(buf, sizeof buf, 0x20000000, &d);
   CHECK(r == 0, "parse rc=%d\n", r);
   CHECK(d.samplePtr == 0x08123456, "samplePtr=0x%x\n", d.samplePtr);
   CHECK(d.nbytes == 4096, "nbytes=%u\n", d.nbytes);
   CHECK(fabs(d.sample_rate - 11025.0) < 1.0, "rate=%.2f\n", d.sample_rate);
   if (!g_fail) printf("  ok\n");
}

/* --- extSH: 16-bit signed stereo, little-endian fields --------------------- */
static void test_extSH_16_stereo(void) {
   printf("test_extSH_16_stereo:\n");
   uint8_t buf[64 + 8];
   memset(buf, 0, sizeof buf);
   w32le(buf + 0, 0);                        /* inline */
   w32le(buf + 4, 2);                        /* numChannels = 2 */
   w32le(buf + 8, 44100u << 16);             /* 44100 Hz */
   buf[20] = SND_EXTSH;
   buf[21] = 60;
   w32le(buf + 22, 200);                     /* numFrames = 200 */
   w16le(buf + 48, 16);                      /* sampleSize = 16 */
   snd_pcm_desc d;
   int r = snd_header_parse(buf, sizeof buf, 0x30000000, &d);
   CHECK(r == 0, "parse rc=%d\n", r);
   CHECK(d.channels == 2, "channels=%u\n", d.channels);
   CHECK(d.bits == 16, "bits=%u\n", d.bits);
   CHECK(d.is_signed == 1, "is_signed=%d\n", d.is_signed);
   CHECK(fabs(d.sample_rate - 44100.0) < 1.0, "rate=%.2f\n", d.sample_rate);
   CHECK(d.nbytes == 200u * 2u * 2u, "nbytes=%u\n", d.nbytes);
   CHECK(d.inline_off == 64, "inline_off=%u\n", d.inline_off);
   if (!g_fail) printf("  ok\n");
}

/* --- extSH big-endian fields (resource-sourced) auto-detected -------------- */
static void test_extSH_bigendian(void) {
   printf("test_extSH_bigendian:\n");
   uint8_t buf[64];
   memset(buf, 0, sizeof buf);
   w32be(buf + 0, 0);
   w32be(buf + 4, 1);                        /* mono */
   w32be(buf + 8, 22050u << 16);             /* BE 16.16 rate */
   buf[20] = SND_EXTSH;
   buf[21] = 60;
   w32be(buf + 22, 500);                     /* numFrames */
   w16be(buf + 48, 16);
   snd_pcm_desc d;
   int r = snd_header_parse(buf, sizeof buf, 0, &d);
   CHECK(r == 0, "parse rc=%d\n", r);
   CHECK(d.is_bigendian == 1, "endian detect failed be=%d\n", d.is_bigendian);
   CHECK(d.channels == 1, "channels=%u\n", d.channels);
   CHECK(fabs(d.sample_rate - 22050.0) < 1.0, "rate=%.2f\n", d.sample_rate);
   CHECK(d.nbytes == 500u * 1u * 2u, "nbytes=%u\n", d.nbytes);
   if (!g_fail) printf("  ok\n");
}

/* --- cmpSH: recognised but declined (rc == +1) ----------------------------- */
static void test_cmpSH_declined(void) {
   printf("test_cmpSH_declined:\n");
   uint8_t buf[64];
   memset(buf, 0, sizeof buf);
   w32le(buf + 4, 1);
   w32le(buf + 8, 22050u << 16);
   buf[20] = SND_CMPSH;
   snd_pcm_desc d;
   int r = snd_header_parse(buf, sizeof buf, 0, &d);
   CHECK(r == 1, "cmpSH rc=%d (expected +1 declined)\n", r);
   if (!g_fail) printf("  ok\n");
}

/* --- 'snd ' format-1 resource with bufferCmd + dataOffsetFlag -------------- */
static void test_snd_resource_fmt1(void) {
   printf("test_snd_resource_fmt1:\n");
   /* Layout (big-endian): format(2)=1, numMods(2)=0, numCommands(2)=1,
    * SndCommand(8) = {cmd=bufferCmd|0x8000, param1=0, param2=hdr_off}, then the
    * SoundHeader at hdr_off. */
   uint8_t res[6 + 8 + 22 + 10];
   memset(res, 0, sizeof res);
   w16be(res + 0, SND_FMT_1);
   w16be(res + 2, 0);                        /* numModifiers */
   w16be(res + 4, 1);                        /* numCommands */
   uint32_t hdr_off = 6 + 8;
   w16be(res + 6, snd_bufferCmd | SND_DATA_OFFSET_FLAG);
   w16be(res + 8, 0);                        /* param1 */
   w32be(res + 10, hdr_off);                 /* param2 = offset to header */
   /* SoundHeader (big-endian, as in a real resource) */
   uint8_t *h = res + hdr_off;
   w32be(h + 0, 0);                          /* samplePtr = 0 inline */
   w32be(h + 4, 10);                         /* length */
   w32be(h + 8, 22254u << 16);
   h[20] = SND_STDSH;
   h[21] = 60;

   uint32_t off = 0;
   int r = snd_resource_find_header(res, sizeof res, 0x40000000, &off);
   CHECK(r == 0, "find rc=%d\n", r);
   CHECK(off == hdr_off, "hdr_off=%u expected %u\n", off, hdr_off);

   snd_pcm_desc d;
   int pr = snd_header_parse(res + off, sizeof res - off, 0x40000000 + off, &d);
   CHECK(pr == 0, "hdr parse rc=%d\n", pr);
   CHECK(d.encode == SND_STDSH, "encode=%u\n", d.encode);
   CHECK(d.nbytes == 10, "nbytes=%u\n", d.nbytes);
   CHECK(fabs(d.sample_rate - 22254.0) < 1.0, "rate=%.2f\n", d.sample_rate);
   if (!g_fail) printf("  ok\n");
}

int main(void) {
   printf("=== snd_convert unit tests ===\n");
   test_stdSH_inline();
   test_stdSH_ptr();
   test_extSH_16_stereo();
   test_extSH_bigendian();
   test_cmpSH_declined();
   test_snd_resource_fmt1();
   if (g_fail) { printf("=== %d CHECK(s) FAILED ===\n", g_fail); return 1; }
   printf("=== all snd_convert tests passed ===\n");
   return 0;
}
