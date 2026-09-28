/*
 * quicktime_movie_bridge.m — GOLDEN functional QuickTime Movie Toolbox ->
 * modern AVFoundation video-playback bridge.
 *
 * The classic QuickTime Movie Toolbox (EnterMovies / NewMovieFromDataRef /
 * StartMovie / MoviesTask / GetMoviePict / ...) decoded and composited movies
 * through the Component Manager + QuickDraw, both DEAD on 64-bit macOS. The
 * companion quicktime_movie_shim.c used to just DECLINE every movie load
 * (couldNotResolveDataRef) so the caller took its no-movie path. This bridge
 * REGAINS real playback by mapping the Movie Toolbox onto AVFoundation:
 *
 *   NewMovieFromDataRef  -> AVURLAsset + AVPlayerItem + AVPlayer, plus an
 *                           AVPlayerItemVideoOutput (32ARGB) tap for frame pull
 *   SetMovieGWorld       -> bind the classic offscreen GWorld/port we render into
 *   StartMovie/StopMovie -> [AVPlayer play]/[pause]; SetMovieRate -> setRate
 *   MoviesTask/UpdateMovie-> pull the current AVPlayerItemVideoOutput CVPixelBuffer
 *                           and blit it into the bound GWorld's low-4GB ARGB buffer
 *                           (qd_port_pixels — the CG-backed QuickDraw substrate),
 *                           exactly the buffer QuickDraw/CopyBits reads back
 *   GetMovie{Box,Duration,TimeScale,Time}/IsMovieDone -> AVAsset/AVPlayerItem timing
 *   DisposeMovie         -> tear the AVFoundation graph down
 *
 * EnterMovies / InitCodecManager stay no-ops (quicktime_movie_shim.c) — AVFoundation
 * owns decoding, so the classic codec/component registration is dead.
 *
 * UNIVERSAL: any translated i386 app that plays a movie through the Movie Toolbox
 * (Civ IV intro/diplo movies, iMovie/QuickTime Player, countless Carbon apps) is
 * served — no app-specific logic; keyed only on the Movie Toolbox API shape.
 *
 * MEMORY MODEL: the Movie the i386 caller holds is just the (low-4GB, since
 * libabiconv's malloc heap is < 4GB — see carbon_shim.h) pointer to our qt_movie,
 * validated against a live registry so a bogus handle never derefs. Manual
 * retain/release for the AVFoundation objects (no ARC, matching nav_shim.m).
 *
 * MTSHIM convention: rdi -> &i386 args[0] (4-byte cdecl slots); result in eax.
 */

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <ImageIO/ImageIO.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include "carbon_shim.h"

/* The CG-backed classic QuickDraw offscreen substrate (qd_gworld.c): resolve a
 * bound GWorld/port (0 => current port) to its low-4GB ARGB pixel buffer. This
 * is the SAME accessor the GraphicsImporter (quicktime_image.c) draws through. */
extern int qd_port_pixels(uint32_t port_h, void **base, int *rowBytes, int *w, int *h);

/* couldNotResolveDataRef (-2000): "this data reference can't be opened". */
#define QT_COULD_NOT_RESOLVE_DATAREF (-2000)
/* Classic default movie time scale (units per second). */
#define QT_TIMESCALE 600

/* ---- the AVFoundation-backed Movie ------------------------------------- */

#define QTM_MAGIC 0x71746d76u   /* 'qtmv' */

typedef struct qt_movie {
   uint32_t          magic;
   AVAsset          *asset;   /* retained */
   AVPlayerItem     *item;    /* retained */
   AVPlayer         *player;  /* retained */
   AVPlayerItemVideoOutput *vout; /* retained */
   uint32_t          gw;      /* bound GWorld/port (i386 handle), or 0 */
   int32_t           natW, natH;   /* natural pixel size */
   int32_t           timeScale;    /* units/sec */
   double            durationSec;
   int               started;
} qt_movie;

/* ---- live registry (validate handles; never deref a bogus Movie) -------- */

#define QT_MAX_MOVIES 128
static qt_movie *g_movies[QT_MAX_MOVIES];
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

static void qt_register(qt_movie *m) {
   pthread_mutex_lock(&g_lock);
   for (int i = 0; i < QT_MAX_MOVIES; i++)
      if (!g_movies[i]) { g_movies[i] = m; break; }
   pthread_mutex_unlock(&g_lock);
}
static void qt_unregister(qt_movie *m) {
   pthread_mutex_lock(&g_lock);
   for (int i = 0; i < QT_MAX_MOVIES; i++)
      if (g_movies[i] == m) { g_movies[i] = NULL; break; }
   pthread_mutex_unlock(&g_lock);
}
/* Resolve an i386 Movie handle to its live qt_movie, or NULL. */
static qt_movie *qt_from_i386(uint32_t h) {
   if (!h) return NULL;
   qt_movie *m = (qt_movie *)(uintptr_t)h;
   pthread_mutex_lock(&g_lock);
   int live = 0;
   for (int i = 0; i < QT_MAX_MOVIES; i++)
      if (g_movies[i] == m) { live = 1; break; }
   pthread_mutex_unlock(&g_lock);
   return (live && m->magic == QTM_MAGIC) ? m : NULL;
}

/* ---- data-reference -> CFURL (mirrors quicktime_image.c gi_set_dataref) -- */

#define kHandleDataRef kFourCC('h','n','d','l')
#define kURLDataRef    kFourCC('u','r','l',' ')

/* Returns a retained CFURLRef for the movie source, or NULL. Handle data refs
 * (raw bytes) are spooled to a temp file so AVFoundation can open them by URL. */
static CFURLRef qt_url_from_dataref(uint32_t dataRef, uint32_t dataRefType) {
   if (dataRefType == kURLDataRef) {
      const char *u = (const char *)cm_handle_block(dataRef);
      if (!u) return NULL;
      CFStringRef s = CFStringCreateWithCString(NULL, u, kCFStringEncodingUTF8);
      if (!s) return NULL;
      CFURLRef url = CFURLCreateWithString(NULL, s, NULL);
      CFRelease(s);
      return url;
   }
   if (dataRefType == kHandleDataRef) {
      void *refblk = cm_handle_block(dataRef);
      uint32_t dataHandle = refblk ? *(uint32_t *)refblk : 0;
      void *bytes = cm_handle_block(dataHandle);
      uint32_t len = cm_handle_size(dataHandle);
      if (!bytes || !len) return NULL;
      char tmpl[] = "/tmp/abiconv_qtmovie_XXXXXX";
      int fd = mkstemp(tmpl);
      if (fd < 0) return NULL;
      ssize_t wr = write(fd, bytes, len);
      close(fd);
      if (wr != (ssize_t)len) { unlink(tmpl); return NULL; }
      return CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)tmpl,
                                                      (CFIndex)strlen(tmpl), false);
   }
   return NULL;
}

/* Build the AVFoundation playback graph from a source URL (consumes url ref). */
static qt_movie *qt_open_url(CFURLRef url) {
   if (!url) return NULL;
   @autoreleasepool {
      AVURLAsset *asset = [AVURLAsset URLAssetWithURL:(__bridge NSURL *)url options:nil];
      if (!asset) return NULL;

      qt_movie *m = (qt_movie *)calloc(1, sizeof(qt_movie));
      if (!m) return NULL;
      m->magic = QTM_MAGIC;
      m->timeScale = QT_TIMESCALE;
      m->asset = [asset retain];

      /* natural size + duration from the first video track (synchronous read —
       * acceptable for a shim; the file is local). */
      NSArray *vtracks = [asset tracksWithMediaType:AVMediaTypeVideo];
      if (vtracks.count) {
         CGSize sz = [(AVAssetTrack *)vtracks[0] naturalSize];
         m->natW = (int32_t)(sz.width  + 0.5);
         m->natH = (int32_t)(sz.height + 0.5);
      }
      CMTime dur = asset.duration;
      m->durationSec = CMTIME_IS_NUMERIC(dur) ? CMTimeGetSeconds(dur) : 0.0;

      m->item = [[AVPlayerItem alloc] initWithAsset:asset];
      NSDictionary *attrs = @{
         (id)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32ARGB),
      };
      m->vout = [[AVPlayerItemVideoOutput alloc] initWithPixelBufferAttributes:attrs];
      [m->item addOutput:m->vout];
      m->player = [[AVPlayer alloc] initWithPlayerItem:m->item];
      m->player.actionAtItemEnd = AVPlayerActionAtItemEndPause;

      qt_register(m);
      return m;
   }
}

/* Convert seconds -> classic TimeValue in the movie's time scale. */
static int32_t qt_units(qt_movie *m, double sec) {
   if (sec < 0) sec = 0;
   return (int32_t)(sec * (double)m->timeScale + 0.5);
}

/* Pull the current decoded frame into the bound GWorld's ARGB buffer. */
static void qt_pump_frame(qt_movie *m) {
   if (!m->vout || !m->gw) return;
   CMTime t = [m->player currentTime];
   if (![m->vout hasNewPixelBufferForItemTime:t]) return;
   CVPixelBufferRef pb = [m->vout copyPixelBufferForItemTime:t itemTimeForDisplay:NULL];
   if (!pb) return;

   void *dbase = NULL; int drb = 0, dw = 0, dh = 0;
   if (qd_port_pixels(m->gw, &dbase, &drb, &dw, &dh) && dbase) {
      CVPixelBufferLockBaseAddress(pb, kCVPixelBufferLock_ReadOnly);
      const uint8_t *sbase = (const uint8_t *)CVPixelBufferGetBaseAddress(pb);
      size_t srb = CVPixelBufferGetBytesPerRow(pb);
      int sw = (int)CVPixelBufferGetWidth(pb), sh = (int)CVPixelBufferGetHeight(pb);
      /* both 32ARGB, both top-left origin -> straight row copy, clamped. */
      int rows = sh < dh ? sh : dh, cols = sw < dw ? sw : dw;
      for (int y = 0; y < rows; y++)
         memcpy((uint8_t *)dbase + (size_t)y * drb, sbase + (size_t)y * srb, (size_t)cols * 4);
      CVPixelBufferUnlockBaseAddress(pb, kCVPixelBufferLock_ReadOnly);
   }
   CVPixelBufferRelease(pb);
}

/* ======================================================================== */
/* i386-cdecl entry points                                                  */
/* ======================================================================== */

/* OSErr NewMovieFromDataRef(Movie *theMovie, short flags, short *idOut,
 *                           Handle dataRef, OSType dataRefType); */
uint32_t shim_NewMovieFromDataRef(uint32_t *a) {
   put_u32(a[0], 0);                      /* *theMovie = NULL until we succeed */
   CFURLRef url = qt_url_from_dataref(a[3], a[4]);
   qt_movie *m = qt_open_url(url);        /* qt_open_url retains its own ref */
   if (url) CFRelease(url);
   if (!m) return (uint32_t)QT_COULD_NOT_RESOLVE_DATAREF;
   put_u32(a[0], to_i386(m));
   if (a[2]) *(int16_t *)i386_ptr(a[2]) = 0;   /* newMovieID out (optional) */
   return cmNoErr;
}

/* void DisposeMovie(Movie theMovie); */
uint32_t shim_DisposeMovie(uint32_t *a) {
   qt_movie *m = qt_from_i386(a[0]);
   if (!m) return cmNoErr;
   qt_unregister(m);
   [m->player pause];
   if (m->vout)   { [m->item removeOutput:m->vout]; [m->vout release]; }
   if (m->player) [m->player release];
   if (m->item)   [m->item release];
   if (m->asset)  [m->asset release];
   m->magic = 0;
   free(m);
   return cmNoErr;
}

/* void SetMovieGWorld(Movie, CGrafPtr port, GDHandle gd); */
uint32_t shim_SetMovieGWorld(uint32_t *a) {
   qt_movie *m = qt_from_i386(a[0]);
   if (m) { m->gw = a[1]; }
   return cmNoErr;
}
/* void GetMovieGWorld(Movie, CGrafPtr *port, GDHandle *gd); */
uint32_t shim_GetMovieGWorld(uint32_t *a) {
   qt_movie *m = qt_from_i386(a[0]);
   put_u32(a[1], m ? m->gw : 0);
   put_u32(a[2], 0);
   return cmNoErr;
}

/* Fill a classic Rect (top,left,bottom,right : int16) with the natural bounds. */
static uint32_t qt_fill_bounds(uint32_t *a) {
   qt_movie *m = qt_from_i386(a[0]);
   int16_t *r = (int16_t *)i386_ptr(a[1]);
   if (!m || !r) return cmParamErr;
   r[0] = 0; r[1] = 0; r[2] = (int16_t)m->natH; r[3] = (int16_t)m->natW;
   return cmNoErr;
}
/* void GetMovieBox(Movie, Rect *boxRect);  == GetMovieNaturalBoundsRect */
uint32_t shim_GetMovieBox(uint32_t *a)                { return qt_fill_bounds(a); }
uint32_t shim_GetMovieNaturalBoundsRect(uint32_t *a)  { return qt_fill_bounds(a); }
/* void SetMovieBox(Movie, const Rect *boxRect); — scaling ignored for now */
uint32_t shim_SetMovieBox(uint32_t *a) { (void)a; return cmNoErr; }

/* void StartMovie(Movie); */
uint32_t shim_StartMovie(uint32_t *a) {
   qt_movie *m = qt_from_i386(a[0]);
   if (m) { m->started = 1; [m->player play]; }
   return cmNoErr;
}
/* void StopMovie(Movie); */
uint32_t shim_StopMovie(uint32_t *a) {
   qt_movie *m = qt_from_i386(a[0]);
   if (m) [m->player pause];
   return cmNoErr;
}
/* void GoToBeginningOfMovie(Movie); */
uint32_t shim_GoToBeginningOfMovie(uint32_t *a) {
   qt_movie *m = qt_from_i386(a[0]);
   if (m) [m->player seekToTime:kCMTimeZero];
   return cmNoErr;
}
/* void SetMovieRate(Movie, Fixed rate);  Fixed is 16.16 -> double */
uint32_t shim_SetMovieRate(uint32_t *a) {
   qt_movie *m = qt_from_i386(a[0]);
   if (m) { double rate = (double)(int32_t)a[1] / 65536.0;
            m->started = rate != 0.0; m->player.rate = (float)rate; }
   return cmNoErr;
}
/* Fixed GetMovieRate(Movie); */
uint32_t shim_GetMovieRate(uint32_t *a) {
   qt_movie *m = qt_from_i386(a[0]);
   double rate = m ? (double)m->player.rate : 0.0;
   return (uint32_t)(int32_t)(rate * 65536.0);
}

/* TimeScale GetMovieTimeScale(Movie); */
uint32_t shim_GetMovieTimeScale(uint32_t *a) {
   qt_movie *m = qt_from_i386(a[0]);
   return (uint32_t)(m ? m->timeScale : 0);
}
/* TimeValue GetMovieDuration(Movie); */
uint32_t shim_GetMovieDuration(uint32_t *a) {
   qt_movie *m = qt_from_i386(a[0]);
   return (uint32_t)(m ? qt_units(m, m->durationSec) : 0);
}
/* TimeValue GetMovieTime(Movie, TimeRecord *currentTime); */
uint32_t shim_GetMovieTime(uint32_t *a) {
   qt_movie *m = qt_from_i386(a[0]);
   if (!m) return 0;
   double sec = CMTimeGetSeconds([m->player currentTime]);
   int32_t units = qt_units(m, sec);
   if (a[1]) {   /* TimeRecord: {wide value(8B: hi,lo), TimeScale scale(4), TimeBase base(4)} */
      int32_t *tr = (int32_t *)i386_ptr(a[1]);
      tr[0] = 0;            /* value.hi */
      tr[1] = units;        /* value.lo */
      tr[2] = m->timeScale; /* scale    */
      tr[3] = 0;            /* base     */
   }
   return (uint32_t)units;
}
/* void SetMovieTimeValue(Movie, TimeValue); — seek */
uint32_t shim_SetMovieTimeValue(uint32_t *a) {
   qt_movie *m = qt_from_i386(a[0]);
   if (m) {
      double sec = (double)(int32_t)a[1] / (double)m->timeScale;
      [m->player seekToTime:CMTimeMakeWithSeconds(sec, m->timeScale)];
   }
   return cmNoErr;
}

/* Boolean IsMovieDone(Movie); */
uint32_t shim_IsMovieDone(uint32_t *a) {
   qt_movie *m = qt_from_i386(a[0]);
   if (!m) return 1;
   double cur = CMTimeGetSeconds([m->player currentTime]);
   return (m->durationSec > 0 && cur >= m->durationSec - 0.001) ? 1 : 0;
}

/* void MoviesTask(Movie, long maxMilliSecToUse); — pump one frame. maxMs==0/NULL
 * movie in classic code means "service all movies"; we service the one passed. */
uint32_t shim_MoviesTask(uint32_t *a) {
   qt_movie *m = qt_from_i386(a[0]);
   if (m) qt_pump_frame(m);
   return cmNoErr;
}
/* void UpdateMovie(Movie); — force a repaint of the current frame. */
uint32_t shim_UpdateMovie(uint32_t *a) {
   qt_movie *m = qt_from_i386(a[0]);
   if (m) qt_pump_frame(m);
   return cmNoErr;
}
/* OSErr PrerollMovie(Movie, TimeValue, Fixed); — AVFoundation buffers itself. */
uint32_t shim_PrerollMovie(uint32_t *a) { (void)a; return cmNoErr; }
/* void SetMovieActive(Movie, Boolean); / void SetMovieVolume(Movie, short); */
uint32_t shim_SetMovieActive(uint32_t *a) { (void)a; return cmNoErr; }
uint32_t shim_SetMovieVolume(uint32_t *a) {
   qt_movie *m = qt_from_i386(a[0]);
   if (m) { int16_t v = (int16_t)a[1]; m->player.volume = (v <= 0) ? 0.f : (float)v / 256.f; }
   return cmNoErr;
}

/* ---- GetMoviePict: single-frame snapshot -> classic PicHandle ----------- *
 * PicHandle GetMoviePict(Movie, TimeValue) returns a QuickDraw picture of the
 * frame at `time`, which the app then hands to DrawPicture. Modern macOS has no
 * PICT *encoder* (ImageIO's com.apple.pict is read-only), so instead of forging a
 * fragile PICT opcode stream we hand back a "wrapped-image PicHandle": a valid
 * classic 10-byte header ([picSize:2][picFrame Rect:8], NATIVE i386 byte order so
 * the app reads correct bounds) followed by the 4-byte tag 'MVPX' and a PNG of the
 * frame. The substrate's decode (qd_gworld.c pict_decode) recognizes the tag and
 * decodes the PNG; DrawPicture then blits it exactly like a real picture. */
#define QT_WRAPPIC_TAG "MVPX"

/* Decode the frame at `sec` to a +1 CGImage (image generator — no playback). */
static CGImageRef qt_frame_cgimage(qt_movie *m, double sec) {
   if (!m->asset) return NULL;
   AVAssetImageGenerator *g = [AVAssetImageGenerator assetImageGeneratorWithAsset:m->asset];
   g.appliesPreferredTrackTransform = YES;
   g.requestedTimeToleranceBefore = kCMTimeZero;
   g.requestedTimeToleranceAfter  = kCMTimePositiveInfinity;   /* nearest at/after */
   CMTime t = CMTimeMakeWithSeconds(sec < 0 ? 0 : sec, m->timeScale);
   return [g copyCGImageAtTime:t actualTime:NULL error:NULL];   /* caller releases */
}

/* Encode a CGImage to PNG bytes (retained CFData), or NULL. */
static CFDataRef qt_cgimage_png(CGImageRef img) {
   CFMutableDataRef d = CFDataCreateMutable(NULL, 0);
   if (!d) return NULL;
   CGImageDestinationRef dst = CGImageDestinationCreateWithData(d, CFSTR("public.png"), 1, NULL);
   if (!dst) { CFRelease(d); return NULL; }
   CGImageDestinationAddImage(dst, img, NULL);
   BOOL ok = CGImageDestinationFinalize(dst);
   CFRelease(dst);
   if (!ok) { CFRelease(d); return NULL; }
   return d;
}

/* PicHandle GetMoviePict(Movie theMovie, TimeValue time); */
uint32_t shim_GetMoviePict(uint32_t *a) {
   qt_movie *m = qt_from_i386(a[0]);
   if (!m) return 0;
   double sec = (double)(int32_t)a[1] / (double)m->timeScale;
   CGImageRef img = qt_frame_cgimage(m, sec);
   if (!img) return 0;
   int w = (int)CGImageGetWidth(img), h = (int)CGImageGetHeight(img);
   CFDataRef png = qt_cgimage_png(img);
   CGImageRelease(img);
   if (!png) return 0;

   uint32_t pnglen = (uint32_t)CFDataGetLength(png);
   uint32_t handle = cm_new_handle(10 + 4 + pnglen, 0);
   if (!handle) { CFRelease(png); return 0; }
   uint8_t *blk = (uint8_t *)cm_handle_block(handle);
   int16_t *hdr = (int16_t *)blk;               /* NATIVE i386 (LE) so the app reads it */
   hdr[0] = 0;                                  /* picSize (low word; modern-ignored) */
   hdr[1] = 0; hdr[2] = 0;                      /* picFrame.top, .left */
   hdr[3] = (int16_t)h; hdr[4] = (int16_t)w;    /* picFrame.bottom, .right */
   memcpy(blk + 10, QT_WRAPPIC_TAG, 4);
   memcpy(blk + 14, CFDataGetBytePtr(png), pnglen);
   CFRelease(png);
   return handle;
}
