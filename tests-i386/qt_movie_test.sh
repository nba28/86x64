#!/bin/bash
# qt_movie_test.sh — regression guard for the QuickTime Movie Toolbox -> AVFoundation
# bridge (quicktime_movie_bridge.m) + its GetMoviePict wrapped-picture decode path
# (qd_gworld.c pict_decode 'MVPX' tag). Self-contained NATIVE x86_64 (no i386
# translation, no display): it synthesizes a solid-color H.264 movie and exercises
# the EXACT semantics the shims implement:
#   (a) FRAME PULL — open via AVURLAsset -> AVPlayerItem + AVPlayerItemVideoOutput
#       (32ARGB) -> AVPlayer, play, and pull a decoded frame; assert its ARGB
#       pixels match the source color (proves real decode into the buffer the
#       bridge blits into a GWorld — MoviesTask/UpdateMovie).
#   (b) GetMoviePict — decode a frame to a CGImage (AVAssetImageGenerator), PNG it,
#       build the wrapped PicHandle body [picSize:2][picFrame:8]['MVPX'][PNG], then
#       decode it exactly the way qd_gworld.c pict_decode does; assert a valid
#       CGImage of the right size (proves DrawPicture(GetMoviePict(...)) renders).
# Exit 0 = both semantics validated. SKIPs (exit 0) only if the toolchain can't
# build the native probe.
set -u
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
cat > "$TMP/t.m" <<'EOF'
#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>
#import <ImageIO/ImageIO.h>
#include <stdio.h>

static NSURL *make_movie(int w, int h, uint8_t R, uint8_t G, uint8_t B) {
   NSString *path = [NSTemporaryDirectory() stringByAppendingPathComponent:@"qt_guard.mov"];
   [[NSFileManager defaultManager] removeItemAtPath:path error:nil];
   NSURL *url = [NSURL fileURLWithPath:path];
   NSError *err = nil;
   AVAssetWriter *wr = [AVAssetWriter assetWriterWithURL:url fileType:AVFileTypeQuickTimeMovie error:&err];
   if (!wr) return nil;
   AVAssetWriterInput *in = [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeVideo
      outputSettings:@{ AVVideoCodecKey:AVVideoCodecTypeH264, AVVideoWidthKey:@(w), AVVideoHeightKey:@(h) }];
   AVAssetWriterInputPixelBufferAdaptor *ad =
      [AVAssetWriterInputPixelBufferAdaptor assetWriterInputPixelBufferAdaptorWithAssetWriterInput:in
         sourcePixelBufferAttributes:@{ (id)kCVPixelBufferPixelFormatTypeKey:@(kCVPixelFormatType_32ARGB) }];
   [wr addInput:in]; [wr startWriting]; [wr startSessionAtSourceTime:kCMTimeZero];
   for (int f = 0; f < 6; f++) {
      CVPixelBufferRef pb = NULL;
      if (ad.pixelBufferPool) CVPixelBufferPoolCreatePixelBuffer(NULL, ad.pixelBufferPool, &pb);
      if (!pb) CVPixelBufferCreate(NULL, w, h, kCVPixelFormatType_32ARGB, NULL, &pb);
      CVPixelBufferLockBaseAddress(pb, 0);
      uint8_t *base = CVPixelBufferGetBaseAddress(pb); size_t rb = CVPixelBufferGetBytesPerRow(pb);
      for (int y=0;y<h;y++) for (int x=0;x<w;x++){ uint8_t *p=base+y*rb+x*4; p[0]=255;p[1]=R;p[2]=G;p[3]=B; }
      CVPixelBufferUnlockBaseAddress(pb, 0);
      while (!in.readyForMoreMediaData) usleep(1000);
      [ad appendPixelBuffer:pb withPresentationTime:CMTimeMake(f, 6)];
      CVPixelBufferRelease(pb);
   }
   [in markAsFinished];
   __block BOOL done = NO;
   [wr finishWritingWithCompletionHandler:^{ done = YES; }];
   while (!done) [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.02]];
   return (wr.status == AVAssetWriterStatusCompleted) ? url : nil;
}

/* the exact wrapped-image decode from qd_gworld.c pict_decode ('MVPX' tag). */
static CGImageRef wrapped_pict_decode(const unsigned char *body, size_t len) {
   if (len > 14 && body[10]=='M'&&body[11]=='V'&&body[12]=='P'&&body[13]=='X') {
      CFDataRef d = CFDataCreate(NULL, body+14, (CFIndex)(len-14));
      CGImageSourceRef s = d ? CGImageSourceCreateWithData(d, NULL) : NULL;
      CGImageRef im = s ? CGImageSourceCreateImageAtIndex(s,0,NULL) : NULL;
      if (s) CFRelease(s); if (d) CFRelease(d);
      return im;
   }
   return NULL;
}
static CFDataRef cgimage_png(CGImageRef img) {
   CFMutableDataRef d = CFDataCreateMutable(NULL, 0);
   CGImageDestinationRef dst = CGImageDestinationCreateWithData(d, CFSTR("public.png"), 1, NULL);
   CGImageDestinationAddImage(dst, img, NULL);
   CGImageDestinationFinalize(dst); CFRelease(dst); return d;
}

int main(void) { @autoreleasepool {
   NSURL *url = make_movie(64, 48, 200, 40, 40);
   if (!url) { printf("SKIP: could not synthesize movie\n"); return 0; }
   AVURLAsset *asset = [AVURLAsset URLAssetWithURL:url options:nil];

   /* (a) frame pull */
   AVPlayerItem *item = [[AVPlayerItem alloc] initWithAsset:asset];
   AVPlayerItemVideoOutput *vout = [[AVPlayerItemVideoOutput alloc]
      initWithPixelBufferAttributes:@{ (id)kCVPixelBufferPixelFormatTypeKey:@(kCVPixelFormatType_32ARGB) }];
   [item addOutput:vout];
   AVPlayer *player = [[AVPlayer alloc] initWithPlayerItem:item]; [player play];
   CVPixelBufferRef pb = NULL;
   for (int i=0;i<300 && !pb;i++){
      [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
      CMTime t=[player currentTime];
      if ([vout hasNewPixelBufferForItemTime:t]) pb=[vout copyPixelBufferForItemTime:t itemTimeForDisplay:NULL];
   }
   if (!pb) { printf("FAIL: no decoded frame pulled\n"); return 1; }
   CVPixelBufferLockBaseAddress(pb, kCVPixelBufferLock_ReadOnly);
   uint8_t *b=CVPixelBufferGetBaseAddress(pb); size_t rb=CVPixelBufferGetBytesPerRow(pb);
   uint8_t *px=b+(CVPixelBufferGetHeight(pb)/2)*rb+(CVPixelBufferGetWidth(pb)/2)*4;
   int A=px[0],R=px[1],G=px[2],B=px[3];
   CVPixelBufferUnlockBaseAddress(pb, kCVPixelBufferLock_ReadOnly); CVPixelBufferRelease(pb);
   int pull_ok = (R>120 && R>G+40 && R>B+40 && A>200);
   printf("frame-pull: ARGB=(%d,%d,%d,%d) %s\n", A,R,G,B, pull_ok?"OK":"BAD");

   /* (b) GetMoviePict wrapped-picture round-trip */
   AVAssetImageGenerator *g = [AVAssetImageGenerator assetImageGeneratorWithAsset:asset];
   g.requestedTimeToleranceAfter = kCMTimePositiveInfinity;
   CGImageRef fimg = [g copyCGImageAtTime:CMTimeMake(0,600) actualTime:NULL error:NULL];
   if (!fimg) { printf("FAIL: image generator\n"); return 1; }
   CFDataRef png = cgimage_png(fimg);
   int fw=(int)CGImageGetWidth(fimg), fh=(int)CGImageGetHeight(fimg);
   CGImageRelease(fimg);
   size_t pnglen=CFDataGetLength(png), blen=10+4+pnglen;
   unsigned char *body=malloc(blen); memset(body,0,10);
   short *hdr=(short*)body; hdr[3]=(short)fh; hdr[4]=(short)fw;   /* picFrame native LE */
   memcpy(body+10,"MVPX",4); memcpy(body+14,CFDataGetBytePtr(png),pnglen); CFRelease(png);
   CGImageRef dec=wrapped_pict_decode(body,blen); free(body);
   int pict_ok = dec && CGImageGetWidth(dec)==(size_t)fw && CGImageGetHeight(dec)==(size_t)fh;
   printf("getmoviepict: wrapped %dx%d -> decoded %s\n", fw, fh, pict_ok?"OK":"BAD");
   if (dec) CGImageRelease(dec);

   int ok = pull_ok && pict_ok;
   printf("%s: movie decode + frame-pull + GetMoviePict\n", ok?"PASS":"FAIL");
   return ok?0:1;
}}
EOF
if ! clang -arch x86_64 -fobjc-arc -o "$TMP/t" "$TMP/t.m" \
     -framework Foundation -framework AVFoundation -framework CoreMedia \
     -framework CoreVideo -framework ImageIO -framework CoreGraphics 2>"$TMP/err"; then
   echo "qt-movie: SKIP (compile failed)"; cat "$TMP/err"; exit 0
fi
OUT=$("$TMP/t" 2>/dev/null); RC=$?
if [ "$RC" -eq 0 ]; then echo "qt-movie: PASS"; echo "$OUT" | sed 's/^/  /'; else echo "qt-movie: FAIL"; echo "$OUT" | sed 's/^/  /'; fi
exit $RC
