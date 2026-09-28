/*
 * coreaudio_hal_shim.c — ONE job: pointer-valued properties through the
 * deprecated CoreAudio HAL getters (AudioHardwareGetProperty,
 * AudioDeviceGetProperty).
 *
 * The caller sizes the value buffer by its OWN pointer width. A CFStringRef
 * property ('lnam' device name, 'uid ' ...) is 4 bytes to i386 code and 8 to
 * native CoreAudio, which answers kAudioHardwareBadPropertySizeError ('!siz')
 * to the 4-byte request. BASS (PvZ's audio) asks every device for 'lnam',
 * gets '!siz' each time, finds no usable device and plays nothing.
 *
 * Structural rule: a 4-byte request refused with '!siz' is retried with 8
 * bytes; if the value really is 8 bytes it is a pointer, handed back as a
 * 32-bit handle (x64_objc_wrap, which passes a low pointer through). Every
 * other request is forwarded untouched. Kill switch M64_NO_HAL_PTR_PROPS=1.
 *
 * Same job, one value type: 'slay' (kAudioDevicePropertyStreamConfiguration)
 * is an AudioBufferList, whose AudioBuffer is 12 bytes on i386 and 16 natively
 * (8-byte mData, padded header). Passed through raw, BASS read nch from the
 * padding and sized its mix from garbage, then wrote ~15 MB past its buffer.
 * Converted to the i386 layout for GetProperty and sized for GetPropertyInfo.
 * ABI: MTSHIM (rdi -> &i386 args[0]); symbols in custom.syms.
 */
#include <CoreAudio/CoreAudio.h>
#include <stdint.h>
#include <stdlib.h>

#pragma clang diagnostic ignored "-Wdeprecated-declarations"

extern uint32_t x64_objc_wrap(uint64_t real);   /* objc_shim.c */

#define P(x) ((void *)(uintptr_t)(x))

static int off(void)
{
   static int v = -1;
   if (v < 0) { v = getenv("M64_NO_HAL_PTR_PROPS") != NULL; }
   return v;
}

/* A refused 4-byte request that the native side holds as 8 bytes: wrap it. */
/* `asked` is the caller's size BEFORE the call: CoreAudio zeroes *sz on failure. */
static OSStatus widen(OSStatus r, UInt32 asked, UInt32 *sz, void *data,
                      OSStatus (^get8)(UInt32 *sz8, uint64_t *v))
{
   if (r != kAudioHardwareBadPropertySizeError || off() || !sz || asked != 4 || !data) { return r; }
   UInt32 sz8 = 8;
   uint64_t v = 0;
   const OSStatus r8 = get8(&sz8, &v);
   if (r8 != noErr || sz8 != 8) { return r; }
   *(uint32_t *)data = x64_objc_wrap(v);
   *sz = 4;
   return noErr;
}

/* OSStatus AudioHardwareGetProperty(AudioHardwarePropertyID, UInt32 *, void *) */
uint32_t shim_AudioHardwareGetProperty(uint32_t *a)
{
   UInt32 *sz = P(a[1]);
   const UInt32 asked = sz ? *sz : 0;
   const OSStatus r = AudioHardwareGetProperty(a[0], sz, P(a[2]));
   return (uint32_t)widen(r, asked, sz, P(a[2]), ^(UInt32 *s8, uint64_t *v) {
      return AudioHardwareGetProperty(a[0], s8, v);
   });
}

/* Native AudioBufferList -> i386 layout {n; {nch, size, data32}[n]}. */
static OSStatus get_stream_config(uint32_t dev, uint32_t ch, Boolean in, UInt32 *sz, void *data)
{
   UInt32 nsz = 0;
   OSStatus r = AudioDeviceGetPropertyInfo(dev, ch, in, kAudioDevicePropertyStreamConfiguration, &nsz, NULL);
   if (r != noErr) { return r; }
   AudioBufferList *abl = malloc(nsz ? nsz : sizeof(AudioBufferList));
   if (!abl) { return kAudioHardwareUnspecifiedError; }
   r = AudioDeviceGetProperty(dev, ch, in, kAudioDevicePropertyStreamConfiguration, &nsz, abl);
   if (r == noErr) {
      const uint32_t need = 4 + 12 * abl->mNumberBuffers;
      if (!data) { *sz = need; }
      else if (*sz < need) { r = kAudioHardwareBadPropertySizeError; }
      else {
         uint32_t *o = data;
         o[0] = abl->mNumberBuffers;
         for (uint32_t i = 0; i < abl->mNumberBuffers; ++i) {
            o[1 + 3 * i] = abl->mBuffers[i].mNumberChannels;
            o[2 + 3 * i] = abl->mBuffers[i].mDataByteSize;
            o[3 + 3 * i] = 0;   /* no buffer behind a configuration query */
         }
         *sz = need;
      }
   }
   free(abl);
   return r;
}

/* OSStatus AudioDeviceGetPropertyInfo(AudioDeviceID, UInt32 ch, Boolean isInput,
 *                                     AudioDevicePropertyID, UInt32 *, Boolean *) */
uint32_t shim_AudioDeviceGetPropertyInfo(uint32_t *a)
{
   UInt32 *sz = P(a[4]);
   if (!off() && a[3] == kAudioDevicePropertyStreamConfiguration && sz) {
      if (a[5]) { *(Boolean *)P(a[5]) = false; }
      return (uint32_t)get_stream_config(a[0], a[1], (Boolean)a[2], sz, NULL);
   }
   return (uint32_t)AudioDeviceGetPropertyInfo(a[0], a[1], (Boolean)a[2], a[3], sz, P(a[5]));
}

/* OSStatus AudioDeviceGetProperty(AudioDeviceID, UInt32 ch, Boolean isInput,
 *                                 AudioDevicePropertyID, UInt32 *, void *) */
uint32_t shim_AudioDeviceGetProperty(uint32_t *a)
{
   UInt32 *sz = P(a[4]);
   const UInt32 asked = sz ? *sz : 0;
   const Boolean in = (Boolean)a[2];
   if (!off() && a[3] == kAudioDevicePropertyStreamConfiguration && sz) {
      return (uint32_t)get_stream_config(a[0], a[1], in, sz, P(a[5]));
   }
   const OSStatus r = AudioDeviceGetProperty(a[0], a[1], in, a[3], sz, P(a[5]));
   return (uint32_t)widen(r, asked, sz, P(a[5]), ^(UInt32 *s8, uint64_t *v) {
      return AudioDeviceGetProperty(a[0], a[1], in, a[3], s8, v);
   });
}
