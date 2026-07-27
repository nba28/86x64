#import <Cocoa/Cocoa.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

static void shim_note(const char *sym) {
  static const char *seen[2048]; static int n;
  static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_lock(&mtx);
  for (int i = 0; i < n; i++)
    if (seen[i] == sym) { pthread_mutex_unlock(&mtx); return; }
  if (n < 2048) seen[n++] = sym;
  fprintf(stderr, "[shimauto:%s] %s: removed-OS symbol CALLED with no curated impl -- returning 0 (write one in shimdb/impl/ if its behavior matters)\n", "FWAVCPrivate", sym);
  if (getenv("SHIMGEN_STUB_ABORT")) abort();
  pthread_mutex_unlock(&mtx);
}

long FWAVCTSDemuxerCreate(long a, long b, long c_, long d, long e, long f) __asm("_FWAVCTSDemuxerCreate");
long FWAVCTSDemuxerCreate(long a, long b, long c_, long d, long e, long f) { shim_note("_FWAVCTSDemuxerCreate"); return 0; }

long FWAVCTSDemuxerCreateWithPIDs(long a, long b, long c_, long d, long e, long f) __asm("_FWAVCTSDemuxerCreateWithPIDs");
long FWAVCTSDemuxerCreateWithPIDs(long a, long b, long c_, long d, long e, long f) { shim_note("_FWAVCTSDemuxerCreateWithPIDs"); return 0; }

long FWAVCTSDemuxerFlush(long a, long b, long c_, long d, long e, long f) __asm("_FWAVCTSDemuxerFlush");
long FWAVCTSDemuxerFlush(long a, long b, long c_, long d, long e, long f) { shim_note("_FWAVCTSDemuxerFlush"); return 0; }

long FWAVCTSDemuxerGetPMTInfo(long a, long b, long c_, long d, long e, long f) __asm("_FWAVCTSDemuxerGetPMTInfo");
long FWAVCTSDemuxerGetPMTInfo(long a, long b, long c_, long d, long e, long f) { shim_note("_FWAVCTSDemuxerGetPMTInfo"); return 0; }

long FWAVCTSDemuxerNextTSPacket(long a, long b, long c_, long d, long e, long f) __asm("_FWAVCTSDemuxerNextTSPacket");
long FWAVCTSDemuxerNextTSPacket(long a, long b, long c_, long d, long e, long f) { shim_note("_FWAVCTSDemuxerNextTSPacket"); return 0; }

long FWAVCTSDemuxerPESPacketGetLen(long a, long b, long c_, long d, long e, long f) __asm("_FWAVCTSDemuxerPESPacketGetLen");
long FWAVCTSDemuxerPESPacketGetLen(long a, long b, long c_, long d, long e, long f) { shim_note("_FWAVCTSDemuxerPESPacketGetLen"); return 0; }

long FWAVCTSDemuxerPESPacketGetPESBuf(long a, long b, long c_, long d, long e, long f) __asm("_FWAVCTSDemuxerPESPacketGetPESBuf");
long FWAVCTSDemuxerPESPacketGetPESBuf(long a, long b, long c_, long d, long e, long f) { shim_note("_FWAVCTSDemuxerPESPacketGetPESBuf"); return 0; }

long FWAVCTSDemuxerPESPacketGetStreamType(long a, long b, long c_, long d, long e, long f) __asm("_FWAVCTSDemuxerPESPacketGetStreamType");
long FWAVCTSDemuxerPESPacketGetStreamType(long a, long b, long c_, long d, long e, long f) { shim_note("_FWAVCTSDemuxerPESPacketGetStreamType"); return 0; }

long FWAVCTSDemuxerRelease(long a, long b, long c_, long d, long e, long f) __asm("_FWAVCTSDemuxerRelease");
long FWAVCTSDemuxerRelease(long a, long b, long c_, long d, long e, long f) { shim_note("_FWAVCTSDemuxerRelease"); return 0; }

long FWAVCTSDemuxerReset(long a, long b, long c_, long d, long e, long f) __asm("_FWAVCTSDemuxerReset");
long FWAVCTSDemuxerReset(long a, long b, long c_, long d, long e, long f) { shim_note("_FWAVCTSDemuxerReset"); return 0; }

long FWAVCTSDemuxerReturnPESPacket(long a, long b, long c_, long d, long e, long f) __asm("_FWAVCTSDemuxerReturnPESPacket");
long FWAVCTSDemuxerReturnPESPacket(long a, long b, long c_, long d, long e, long f) { shim_note("_FWAVCTSDemuxerReturnPESPacket"); return 0; }
