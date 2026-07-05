/*
 * snd_convert.h — pure (framework-free) decoding of the classic Sound Manager
 * on-memory formats: SoundHeader / ExtSoundHeader / CmpSoundHeader and the
 * 'snd ' (SndListResource / Snd2ListResource) resource wrapper.
 *
 * This half has NO CoreAudio / AudioToolbox dependency and dereferences only the
 * caller-supplied header buffer (never an arbitrary app pointer), so it is
 * trivially unit-testable headless. sndmgr_shim.c layers the AudioQueue playback
 * and the i386 pointer following on top of these results.
 *
 * All classic Sound Manager structs are `#pragma pack(2)` (verified against the
 * 10.6 CarbonSound Sound.h) and all their integer fields are 4-byte `long`/`Ptr`
 * or 2-byte `short` on i386, so the offsets below are exact and arch-stable.
 *
 * Endianness: a SoundHeader built programmatically by an i386 app has
 * little-endian fields; one read out of a classic 'snd ' resource is big-endian
 * (68k on-disk format). We auto-detect per header by which interpretation of the
 * 16.16 sampleRate lands on a plausible audio rate, so both paths work with no
 * app-specific logic.
 */
#ifndef SND_CONVERT_H
#define SND_CONVERT_H

#include <stdint.h>

/* header encode bytes (Sound.h) */
#define SND_STDSH 0x00
#define SND_EXTSH 0xFF
#define SND_CMPSH 0xFE

/* 'snd ' resource formats and the command data-offset flag (Sound.h) */
#define SND_FMT_1          0x0001
#define SND_FMT_2          0x0002
#define SND_DATA_OFFSET_FLAG 0x8000

/* SndCommand opcodes we act on (Sound.h) */
enum {
   snd_nullCmd = 0, snd_quietCmd = 3, snd_flushCmd = 4, snd_reInitCmd = 5,
   snd_waitCmd = 10, snd_pauseCmd = 11, snd_resumeCmd = 12, snd_callBackCmd = 13,
   snd_syncCmd = 14, snd_availableCmd = 24, snd_versionCmd = 25,
   snd_ampCmd = 43, snd_volumeCmd = 46, snd_getVolumeCmd = 47,
   snd_soundCmd = 80, snd_bufferCmd = 81, snd_rateCmd = 82,
   snd_rateMultiplierCmd = 86, snd_getRateMultiplierCmd = 87
};

/* A decoded linear-PCM description. Sample location is returned as the classic
 * pair (samplePtr, inline_off): if samplePtr != 0 the samples live at that
 * absolute i386 address; otherwise they follow the header inline at
 * header+inline_off. sndmgr_shim.c (which knows the app heap is low-4GB
 * dereferenceable) resolves this to a real pointer. */
typedef struct {
   double   sample_rate;   /* Hz, from the UnsignedFixed 16.16 field */
   uint32_t channels;      /* 1 (mono) or 2 (stereo) */
   uint32_t bits;          /* 8 or 16 */
   int      is_signed;     /* 0 = unsigned (classic 8-bit), 1 = signed (16-bit) */
   int      is_bigendian;  /* sample byte order (16-bit); follows header endianness */
   uint32_t samplePtr;     /* absolute i386 ptr to samples, or 0 = inline */
   uint32_t inline_off;    /* if samplePtr==0: samples at header + inline_off */
   uint32_t nbytes;        /* total sample bytes */
   uint8_t  encode;        /* SND_STDSH / SND_EXTSH / SND_CMPSH */
   uint8_t  base_freq;     /* MIDI base note (kMiddleC == 60) */
   uint32_t cmp_format;    /* OSType for cmpSH (0 otherwise) */
   int      cmp_id;        /* compressionID for cmpSH (0 = none) */
} snd_pcm_desc;

/* Parse a SoundHeader / ExtSoundHeader / CmpSoundHeader at `hdr` (readable for
 * `maxlen` bytes). `hdr_base` is the i386 address the header lives at (used to
 * compute the inline sample offset relative to the app's view). Returns 0 on a
 * supported (uncompressed) PCM header, -1 on malformed input, and +1 on a valid
 * but compressed (cmpSH) header we cannot yet decode (caller may decline). */
int snd_header_parse(const uint8_t *hdr, uint32_t maxlen, uint32_t hdr_base,
                     snd_pcm_desc *out);

/* Locate the SoundHeader inside a 'snd ' resource (format 1 or 2). `res` points
 * at the resource bytes (readable for `reslen`); `res_base` is its i386 address.
 * On success returns 0 and sets *hdr_off to the byte offset of the SoundHeader
 * within the resource (the caller then runs snd_header_parse on res+*hdr_off).
 * Returns -1 if no bufferCmd/soundCmd with sound data is found. */
int snd_resource_find_header(const uint8_t *res, uint32_t reslen,
                             uint32_t res_base, uint32_t *hdr_off);

#endif /* SND_CONVERT_H */
