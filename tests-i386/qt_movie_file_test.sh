#!/bin/bash
# qt_movie_file_test.sh — A/B guard for the classic movie-FILE path into the
# AVFoundation Movie bridge (quicktime_movie_bridge.m OpenMovieFile /
# NewMovieFromFile; Halo's studio logos never played). Writes a short H.264
# movie with a native helper, then runs the translated fixture with QTMF_PATH.
# ON exits 42; OFF (M64_NO_QT_MOVIE_FILE=1) must not; M64_NO_QT_DEFAULT_PORT=1
# (new movie gets no port) exits 6.
set -u
cd "$(dirname "$0")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
cat > "$TMP/mk.m" <<'MK'
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

int main(void) {
   @autoreleasepool { NSURL *u = make_movie(64, 48, 200, 40, 40); if (!u) return 1; printf("%s\n", u.path.UTF8String); }
   return 0;
}
MK
clang -fobjc-arc -framework Foundation -framework AVFoundation -framework CoreMedia -framework CoreVideo \
   -o "$TMP/mk" "$TMP/mk.m" 2>/dev/null || { echo "qt-movie-file: FAIL (helper build)"; exit 1; }
MOV=$("$TMP/mk") && [ -s "$MOV" ] || { echo "qt-movie-file: FAIL (no movie written)"; exit 1; }
FIX=build/99_qt_movie_file.x86_64
QTMF_PATH="$MOV" $FIX >/dev/null 2>&1; on=$?
QTMF_PATH="$MOV" M64_NO_QT_MOVIE_FILE=1 $FIX >/dev/null 2>&1; off=$?
QTMF_PATH="$MOV" M64_NO_QT_DEFAULT_PORT=1 $FIX >/dev/null 2>&1; offp=$?
[ $on = 42 ] && [ $off != 42 ] && [ $offp = 6 ] && echo "qt-movie-file: PASS (on=$on off=$off offp=$offp)" ||
   { echo "qt-movie-file: FAIL (on=$on off=$off offp=$offp)"; exit 1; }
