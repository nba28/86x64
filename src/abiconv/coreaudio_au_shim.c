/*
 * coreaudio_au_shim.c — ONE job: an i386 AudioUnit (Component Manager era).
 *
 * i386 code opens its AudioUnits with FindNextComponent/OpenAComponent and
 * calls AudioUnit* on the ComponentInstance it gets back. Two things broke that:
 *   - carbon_component.c had no AudioUnit backend (a fake instance -> -50), and
 *   - abigen marshals AudioUnit as `struct ComponentInstanceRecord *` by DEEP
 *     COPY, i.e. hands CoreAudio a pointer to a stack copy of one word.
 * PvZ's sound effects (its own mixer on an output unit) were silent.
 *
 * Here AU component types ('au..') map to AudioComponentFindNext /
 * AudioComponentInstanceNew, handles cross as x64_objc_wrap tokens, and the
 * AudioUnit calls unwrap them. Property values with pointers are converted:
 * MakeConnection {AudioUnit, u32, u32} and SetRenderCallback {proc, refCon}.
 * Render/notify callbacks and AudioConverterFillComplexBuffer get i386-layout
 * AudioBufferLists (coreaudio_abl.h).
 * Kill switch M64_NO_AU_SHIM=1 (the Component Manager keeps its fake instance).
 * ABI: MTSHIM (rdi -> &i386 args[0]); symbols in custom.syms.
 */
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>
#include <os/lock.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "cb_bridge.h"
#include "coreaudio_abl.h"

#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#define P(x)   ((void *)(uintptr_t)(x))
#define P32(p) ((uint64_t)(uint32_t)(uintptr_t)(p))

static int off(void)
{
   static int v = -1;
   if (v < 0) { v = getenv("M64_NO_AU_SHIM") != NULL; }
   return v;
}

static AudioUnit au_in(uint32_t h) { return (AudioUnit)(uintptr_t)x64_objc_unwrap(h); }

/* ---- Component Manager hooks (carbon_component.c) ----------------------- */

static int is_au_type(uint32_t t) { return (t >> 16) == (('a' << 8) | 'u'); }

/* ponytail: fixed tables; a game opens a handful of units. */
#define MAX_COMPS 32
static struct { uint32_t h; AudioComponent c; } g_comp[MAX_COMPS];
static int            g_ncomp;
static os_unfair_lock g_lock = OS_UNFAIR_LOCK_INIT;

static uint32_t comp_handle(AudioComponent c)
{
   uint32_t h = 0;
   os_unfair_lock_lock(&g_lock);
   for (int i = 0; i < g_ncomp; ++i) { if (g_comp[i].c == c) { h = g_comp[i].h; } }
   if (!h && g_ncomp < MAX_COMPS) {
      h = x64_objc_wrap((uint64_t)(uintptr_t)c);
      g_comp[g_ncomp].h = h; g_comp[g_ncomp].c = c; ++g_ncomp;
   }
   os_unfair_lock_unlock(&g_lock);
   return h;
}

static AudioComponent comp_native(uint32_t h)
{
   AudioComponent c = NULL;
   os_unfair_lock_lock(&g_lock);
   for (int i = 0; i < g_ncomp; ++i) { if (g_comp[i].h == h) { c = g_comp[i].c; } }
   os_unfair_lock_unlock(&g_lock);
   return c;
}

/* FindNextComponent for an AU type: 1 = handled (*out set). cd = i386
 * ComponentDescription, 5 x u32 — the same layout as AudioComponentDescription. */
int au_find_next(uint32_t prev, const uint32_t *cd, uint32_t *out)
{
   if (off() || !cd || !is_au_type(cd[0])) { return 0; }
   AudioComponentDescription d = { cd[0], cd[1], cd[2], cd[3], cd[4] };
   AudioComponent c = AudioComponentFindNext(prev ? comp_native(prev) : NULL, &d);
   *out = c ? comp_handle(c) : 0;
   return 1;
}

/* OpenAComponent on a handle we issued: 1 = handled (*inst set, 0 on failure). */
int au_open(uint32_t comp, uint32_t *inst)
{
   AudioComponent c = off() ? NULL : comp_native(comp);
   if (!c) { return 0; }
   AudioComponentInstance u = NULL;
   *inst = (AudioComponentInstanceNew(c, &u) == noErr && u) ? x64_objc_wrap((uint64_t)(uintptr_t)u) : 0;
   return 1;
}

/* CloseComponent on an AU instance: 1 = handled. */
int au_close(uint32_t inst, uint32_t *result)
{
   if (off()) { return 0; }
   AudioUnit u = au_in(inst);
   if ((uintptr_t)u == inst) { return 0; }   /* not one of our wrapped handles */
   *result = (uint32_t)AudioComponentInstanceDispose(u);
   return 1;
}

/* ---- render / notify callbacks ----------------------------------------- */

typedef struct {
   uint32_t fn32, ref32;
   uint32_t (*call)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
   abl_low  io;                                /* low list + sample buffers */
   AudioUnitRenderActionFlags *flags;          /* low scratch */
   AudioTimeStamp *ts;
} au_cb;

#define MAX_CBS 32
static au_cb  g_cb[MAX_CBS];
static int    g_ncb;
/* six plain words in, OSStatus out */
static const x64_cb_sig k_sig6 = { 6, CBR_I32, { CBA_I32 }, { 0 } };

static au_cb *cb_new(uint32_t fn32, uint32_t ref32)
{
   os_unfair_lock_lock(&g_lock);
   au_cb *e = NULL;
   for (int i = 0; i < g_ncb && !e; ++i) {
      if (g_cb[i].fn32 == fn32 && g_cb[i].ref32 == ref32) { e = &g_cb[i]; }
   }
   if (!e && g_ncb < MAX_CBS) {
      e = &g_cb[g_ncb++];
      e->fn32 = fn32; e->ref32 = ref32;
      e->call  = (void *)(uintptr_t)x64_cb_wrap(fn32, &k_sig6);
      e->flags = calloc(1, sizeof *e->flags);     /* libabiconv malloc = low 4GB */
      e->ts    = calloc(1, sizeof *e->ts);
   }
   os_unfair_lock_unlock(&g_lock);
   return e;
}

/* AURenderCallback shape, shared by render callbacks and render notifies (a
 * post-render notify sees the rendered samples, so they are carried in). */
static OSStatus au_render_tramp(void *ctx, AudioUnitRenderActionFlags *flags,
                                const AudioTimeStamp *ts, UInt32 bus, UInt32 frames,
                                AudioBufferList *io)
{
   au_cb *e = ctx;
   if (flags) { *e->flags = *flags; }
   if (ts)    { *e->ts = *ts; }
   abl32 *l32 = abl_lower(&e->io, io, 1);
   const OSStatus r = (OSStatus)e->call(e->ref32, flags ? P32(e->flags) : 0, ts ? P32(e->ts) : 0,
                                        bus, frames, P32(l32));
   if (flags) { *flags = *e->flags; }
   if (l32)   { abl_raise(&e->io, io); }
   return r;
}

/* ---- AudioUnit calls ---------------------------------------------------- */

/* OSStatus AudioUnitSetProperty(AudioUnit, PropertyID, Scope, Element, const void *, UInt32) */
uint32_t shim_AudioUnitSetProperty(uint32_t *a)
{
   AudioUnit u = au_in(a[0]);
   const uint32_t *v = P(a[4]);
   if (a[1] == kAudioUnitProperty_MakeConnection && v && a[5] >= 12) {
      AudioUnitConnection c = { v[0] ? au_in(v[0]) : NULL, v[1], v[2] };
      return (uint32_t)AudioUnitSetProperty(u, a[1], a[2], a[3], &c, sizeof c);
   }
   if (a[1] == kAudioUnitProperty_SetRenderCallback && v && a[5] >= 8) {
      AURenderCallbackStruct s = { NULL, NULL };
      if (v[0]) {
         au_cb *e = cb_new(v[0], v[1]);
         if (!e) { return (uint32_t)kAudioUnitErr_FailedInitialization; }
         s.inputProc = au_render_tramp; s.inputProcRefCon = e;
      }
      return (uint32_t)AudioUnitSetProperty(u, a[1], a[2], a[3], &s, sizeof s);
   }
   return (uint32_t)AudioUnitSetProperty(u, a[1], a[2], a[3], v, a[5]);
}

/* OSStatus AudioUnitGetProperty(AudioUnit, PropertyID, Scope, Element, void *, UInt32 *) */
uint32_t shim_AudioUnitGetProperty(uint32_t *a)
{
   return (uint32_t)AudioUnitGetProperty(au_in(a[0]), a[1], a[2], a[3], P(a[4]), P(a[5]));
}

uint32_t shim_AudioUnitInitialize(uint32_t *a)   { return (uint32_t)AudioUnitInitialize(au_in(a[0])); }
uint32_t shim_AudioUnitUninitialize(uint32_t *a) { return (uint32_t)AudioUnitUninitialize(au_in(a[0])); }
uint32_t shim_AudioOutputUnitStart(uint32_t *a)  { return (uint32_t)AudioOutputUnitStart(au_in(a[0])); }
uint32_t shim_AudioOutputUnitStop(uint32_t *a)   { return (uint32_t)AudioOutputUnitStop(au_in(a[0])); }

/* OSStatus AudioUnitGetParameter(AudioUnit, ParameterID, Scope, Element, Float32 *) */
uint32_t shim_AudioUnitGetParameter(uint32_t *a)
{
   return (uint32_t)AudioUnitGetParameter(au_in(a[0]), a[1], a[2], a[3], P(a[4]));
}

/* OSStatus AudioUnitSetParameter(AudioUnit, ParameterID, Scope, Element, Float32, UInt32) */
uint32_t shim_AudioUnitSetParameter(uint32_t *a)
{
   Float32 v;
   memcpy(&v, &a[4], sizeof v);
   return (uint32_t)AudioUnitSetParameter(au_in(a[0]), a[1], a[2], a[3], v, a[5]);
}

/* OSStatus AudioUnitAddRenderNotify(AudioUnit, AURenderCallback, void *) */
uint32_t shim_AudioUnitAddRenderNotify(uint32_t *a)
{
   au_cb *e = cb_new(a[1], a[2]);
   if (!e) { return (uint32_t)kAudioUnitErr_FailedInitialization; }
   return (uint32_t)AudioUnitAddRenderNotify(au_in(a[0]), au_render_tramp, e);
}

/* OSStatus AudioUnitRemoveRenderNotify(AudioUnit, AURenderCallback, void *) */
uint32_t shim_AudioUnitRemoveRenderNotify(uint32_t *a)
{
   au_cb *e = cb_new(a[1], a[2]);
   if (!e) { return (uint32_t)kAudioUnitErr_InvalidParameter; }
   return (uint32_t)AudioUnitRemoveRenderNotify(au_in(a[0]), au_render_tramp, e);
}

/* ---- AudioConverterFillComplexBuffer ------------------------------------
 * Same family: the input proc and both buffer lists are i386-layout. The
 * converter handle crosses as the generated bridges' wrap token. */
/* five plain words in, OSStatus out */
static const x64_cb_sig k_sig5 = { 5, CBR_I32, { CBA_I32 }, { 0 } };
typedef struct {
   uint32_t user32, conv32;
   uint32_t (*call)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
   uint32_t *packets;                              /* low scratch */
   abl32    *abl;
   uint32_t *desc;                                 /* i386 AudioStreamPacketDescription * */
} acfill_ctx;

static OSStatus acfill_tramp(AudioConverterRef conv, UInt32 *packets, AudioBufferList *io,
                             AudioStreamPacketDescription **desc, void *vctx)
{
   (void)conv;
   acfill_ctx *c = vctx;
   *c->packets = packets ? *packets : 0;
   const uint32_t n = io ? (io->mNumberBuffers < ABL32_MAX ? io->mNumberBuffers : ABL32_MAX) : 0;
   c->abl->n = n;
   for (uint32_t i = 0; i < n; ++i) {             /* the proc points data at its own */
      c->abl->b[i] = (buf32){ io->mBuffers[i].mNumberChannels, io->mBuffers[i].mDataByteSize, 0 };
   }
   *c->desc = 0;
   const OSStatus r = (OSStatus)c->call(c->conv32, P32(c->packets), io ? P32(c->abl) : 0,
                                        desc ? P32(c->desc) : 0, c->user32);
   if (packets) { *packets = *c->packets; }
   for (uint32_t i = 0; i < n; ++i) {               /* i386 data is low: valid natively */
      io->mBuffers[i].mNumberChannels = c->abl->b[i].nch;
      io->mBuffers[i].mDataByteSize   = c->abl->b[i].size;
      io->mBuffers[i].mData           = P(c->abl->b[i].data);
   }
   if (desc) { *desc = P(*c->desc); }               /* same 16-byte layout on both */
   return r;
}

/* OSStatus AudioConverterFillComplexBuffer(AudioConverterRef, InputDataProc, void *user,
 *          UInt32 *ioPackets, AudioBufferList *out, AudioStreamPacketDescription *outDesc) */
uint32_t shim_AudioConverterFillComplexBuffer(uint32_t *a)
{
   static __thread acfill_ctx *tc;                  /* per thread, reused: no alloc per call */
   if (!tc) {
      tc = calloc(1, sizeof *tc);
      if (!tc) { return (uint32_t)kAudioConverterErr_UnspecifiedError; }
      tc->packets = calloc(1, sizeof *tc->packets);
      tc->abl     = calloc(1, sizeof *tc->abl);
      tc->desc    = calloc(1, sizeof *tc->desc);
   }
   acfill_ctx c = *tc;                              /* reentrancy-safe copy of the pointers */
   c.user32 = a[2]; c.conv32 = a[0];
   c.call = (void *)(uintptr_t)x64_cb_wrap(a[1], &k_sig5);
   const uint32_t *o32 = P(a[4]);
   const uint32_t n = o32 ? (o32[0] < ABL32_MAX ? o32[0] : ABL32_MAX) : 0;
   struct { UInt32 n; AudioBuffer b[ABL32_MAX]; } out;
   out.n = n;
   for (uint32_t i = 0; i < n; ++i) {
      out.b[i].mNumberChannels = o32[1 + 3 * i];
      out.b[i].mDataByteSize   = o32[2 + 3 * i];
      out.b[i].mData           = P(o32[3 + 3 * i]);
   }
   const OSStatus r = AudioConverterFillComplexBuffer(
      (AudioConverterRef)(uintptr_t)x64_objc_unwrap(a[0]), acfill_tramp, &c, P(a[3]),
      o32 ? (AudioBufferList *)&out : NULL, P(a[5]));
   for (uint32_t i = 0; i < n; ++i) {
      ((uint32_t *)P(a[4]))[2 + 3 * i] = out.b[i].mDataByteSize;
   }
   return (uint32_t)r;
}
