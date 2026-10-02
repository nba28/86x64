/* i386 Bink decode harness, driven by bink-diff.sh. Env: BK_LIB (a TRANSLATED
 * libbinkmachox86.dylib), BK_FILE (.bik), BK_N (frames), BK_OUT. Writes BK_N
 * frames of BINKSURFACE32 (W*H*4, BGRX) to BK_OUT and prints "W H Frames".
 * BK_YUV=1: decode into app-registered planes exactly like Portal 2's
 * CBIKMaterial (BINKNOFRAMEBUFFERS + BinkRegisterFrameBuffers, 16-aligned
 * pitch) and write raw I420 (Y, cB, cR) per frame instead. */
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
typedef struct { unsigned Width, Height, Frames, FrameNum; } BINKHDR; /* BINK prefix */
static BINKHDR *(*BinkOpen)(const char *, unsigned);
static int  (*BinkDoFrame)(BINKHDR *);
static void (*BinkNextFrame)(BINKHDR *);
static int  (*BinkCopyToBuffer)(BINKHDR *, void *, int, unsigned, unsigned, unsigned, unsigned);
static void (*BinkClose)(BINKHDR *);
static const char *(*BinkGetError)(void);
typedef struct { int Allocate; void *Buffer; unsigned Pitch; } BINKPLANE;
typedef struct { BINKPLANE Y, cR, cB, A; } BINKFRAMEPLANESET;
typedef struct { int TotalFrames; unsigned YW, YH, CW, CH, FrameNum; BINKFRAMEPLANESET Frames[2]; } BINKFRAMEBUFFERS;
static void (*BinkGetFrameBuffersInfo)(BINKHDR *, BINKFRAMEBUFFERS *);
static void (*BinkRegisterFrameBuffers)(BINKHDR *, BINKFRAMEBUFFERS *);
static void plane_alloc(BINKPLANE *p, unsigned w, unsigned h) {
   if (!p->Allocate) { return; }
   p->Pitch = (w + 15) & ~15u;
   p->Buffer = (void *)(((unsigned long)malloc(p->Pitch * h + 0x13) + 0x13) & ~0xful);
}
static void plane_write(FILE *o, const BINKPLANE *p, unsigned w, unsigned h) {
   for (unsigned y = 0; y < h; y++) { fwrite((char *)p->Buffer + y * p->Pitch, 1, w, o); }
}
static void (*BinkSetMemory)(void *(*)(unsigned), void (*)(void *));
static void *bk_alloc(unsigned n) { return malloc(n); }
static void bk_free(void *p) { free(p); }
int main(void) {
   /* argv does not reach a translated fixture: knobs are env vars */
   char *argv[] = { 0, getenv("BK_LIB"), getenv("BK_FILE"), getenv("BK_N"), getenv("BK_OUT") };
   if (!argv[1] || !argv[2] || !argv[3] || !argv[4]) { fprintf(stderr, "need BK_LIB BK_FILE BK_N BK_OUT\n"); return 2; }
   void *h = dlopen(argv[1], RTLD_NOW);
   if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
#define SYM(n) if (!(*(void **)&n = dlsym(h, #n))) { fprintf(stderr, "no %s\n", #n); return 1; }
   SYM(BinkOpen) SYM(BinkDoFrame) SYM(BinkNextFrame) SYM(BinkCopyToBuffer) SYM(BinkClose) SYM(BinkGetError) SYM(BinkSetMemory) SYM(BinkGetFrameBuffersInfo) SYM(BinkRegisterFrameBuffers)
   BinkSetMemory(bk_alloc, bk_free); /* as Portal 2 CBik::Init: no Carbon MaxBlock */

   int yuv = getenv("BK_YUV") != NULL;
   BINKHDR *b = BinkOpen(argv[2], yuv ? 0x400 /*BINKNOFRAMEBUFFERS*/ : 0);
   if (!b) { fprintf(stderr, "BinkOpen: %s\n", BinkGetError()); return 1; }
   unsigned n = atoi(argv[3]); if (n > b->Frames) n = b->Frames;
   printf("%u %u %u\n", b->Width, b->Height, b->Frames); fflush(stdout);
   size_t sz = (size_t)b->Width * b->Height * 4;
   unsigned char *buf = malloc(sz);
   FILE *o = fopen(argv[4], "wb");
   static BINKFRAMEBUFFERS fb;
   if (yuv) {
      BinkGetFrameBuffersInfo(b, &fb);
      for (int f = 0; f < fb.TotalFrames && f < 2; f++) {
         plane_alloc(&fb.Frames[f].Y, fb.YW, fb.YH);
         plane_alloc(&fb.Frames[f].cR, fb.CW, fb.CH);
         plane_alloc(&fb.Frames[f].cB, fb.CW, fb.CH);
      }
      BinkRegisterFrameBuffers(b, &fb);
      fprintf(stderr, "framebuffers: total=%d Y %ux%u C %ux%u\n", fb.TotalFrames, fb.YW, fb.YH, fb.CW, fb.CH);
   }
   for (unsigned i = 0; i < n; i++) {
      BinkDoFrame(b);
      if (yuv) {
         BINKFRAMEPLANESET *f = &fb.Frames[fb.FrameNum];
         if (getenv("BK_DEBUG")) {
            unsigned c[2] = {0, 0};
            for (int k = 0; k < 2; k++) {
               for (unsigned y = 0; y < b->Height; y++) {
                  unsigned char *row = (unsigned char *)fb.Frames[k].Y.Buffer + y * fb.Frames[k].Y.Pitch;
                  for (unsigned x = 0; x < b->Width; x++) { c[k] += row[x] != 16; }
               }
            }
            fprintf(stderr, "frame %u FrameNum=%u Y0=%p nonblack=%u Y1=%p nonblack=%u\n",
                    i, fb.FrameNum, fb.Frames[0].Y.Buffer, c[0], fb.Frames[1].Y.Buffer, c[1]);
         }
         plane_write(o, &f->Y, b->Width, b->Height);
         plane_write(o, &f->cB, b->Width / 2, b->Height / 2);
         plane_write(o, &f->cR, b->Width / 2, b->Height / 2);
         BinkNextFrame(b);
         continue;
      }
      BinkCopyToBuffer(b, buf, b->Width * 4, b->Height, 0, 0, 3 /*BINKSURFACE32*/ | 0x80000000u /*COPYALL*/);
      fwrite(buf, 1, sz, o);
      BinkNextFrame(b);
   }
   fclose(o); BinkClose(b);
   exit(0);   /* not `return`: a translated exec's main has no return address */
}
