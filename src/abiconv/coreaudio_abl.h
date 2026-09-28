/*
 * coreaudio_abl.h — an AudioBufferList as i386 code sees it.
 *
 *     i386   { u32 n; { u32 nch; u32 size; u32 mData } [n] }        12-byte buffers
 *     x86_64 { u32 n; pad; { u32 nch; u32 size; u64 mData } [n] }  16-byte buffers
 *
 * CoreAudio hands callbacks native lists whose mData are its own high-memory
 * buffers. abl_lower() builds the i386 list over low-4GB sample buffers
 * (libabiconv's malloc is the low arena); abl_raise() copies the callback's
 * result back. Shared by the HAL IOProc and AudioUnit render shims.
 */
#ifndef ABICONV_COREAUDIO_ABL_H
#define ABICONV_COREAUDIO_ABL_H

#include <CoreAudio/CoreAudioTypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ABL32_MAX 16
typedef struct { uint32_t nch, size, data; } buf32;
typedef struct { uint32_t n; buf32 b[ABL32_MAX]; } abl32;

/* An abl32 header plus the low sample buffers behind it, grown on demand. */
typedef struct {
   abl32   *hdr;
   void    *data[ABL32_MAX];
   uint32_t cap[ABL32_MAX];
} abl_low;

/* Native -> low i386 list (NULL for a NULL list). `copy_in` carries the native
 * samples over; otherwise the low buffers start zeroed. */
static inline abl32 *abl_lower(abl_low *l, const AudioBufferList *src, int copy_in)
{
   if (!src) { return NULL; }
   if (!l->hdr && !(l->hdr = (abl32 *)calloc(1, sizeof *l->hdr))) { return NULL; }
   const uint32_t n = src->mNumberBuffers < ABL32_MAX ? src->mNumberBuffers : ABL32_MAX;
   l->hdr->n = n;
   for (uint32_t i = 0; i < n; ++i) {
      const AudioBuffer *s = &src->mBuffers[i];
      if (l->cap[i] < s->mDataByteSize) {   /* grows once, off the steady state */
         free(l->data[i]);
         l->data[i] = malloc(s->mDataByteSize);
         l->cap[i]  = l->data[i] ? s->mDataByteSize : 0;
      }
      const uint32_t sz = l->cap[i] ? s->mDataByteSize : 0;
      l->hdr->b[i] = (buf32){ s->mNumberChannels, sz, (uint32_t)(uintptr_t)l->data[i] };
      if (!sz) { continue; }
      if (copy_in && s->mData) { memcpy(l->data[i], s->mData, sz); }
      else                     { memset(l->data[i], 0, sz); }
   }
   return l->hdr;
}

/* The callback's i386 list -> the native one. The callback may have pointed a
 * buffer at its own (low) data; a native buffer with no storage adopts it. */
static inline void abl_raise(const abl_low *l, AudioBufferList *dst)
{
   if (!dst || !l->hdr) { return; }
   for (uint32_t i = 0; i < l->hdr->n; ++i) {
      AudioBuffer *d = &dst->mBuffers[i];
      void *src = (void *)(uintptr_t)l->hdr->b[i].data;
      const uint32_t sz = l->hdr->b[i].size;
      d->mNumberChannels = l->hdr->b[i].nch;
      if (!d->mData) { d->mData = src; d->mDataByteSize = sz; }   /* low memory is native memory */
      else if (src && d->mData != src) { memcpy(d->mData, src, sz < d->mDataByteSize ? sz : d->mDataByteSize); }
   }
}

#endif
