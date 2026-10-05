/* 99_snd_compression_info — the Sound Manager's GetCompressionInfo (removed from
 * 64-bit; its abigen bridge called NULL). Call of Duty 4 asks it about 'NONE'
 * 16-bit stereo at startup. ON exits 42 with the PCM frame layout filled.
 * OFF (M64_NO_SND_COMPINFO=1, run time): paramErr, layout zeroed (exit 1). */
extern int printf(const char *, ...);
extern void exit(int);
typedef short OSErr;
#pragma pack(push, 2)
typedef struct { long recordSize; unsigned int format; short compressionID;
                 unsigned short samplesPerPacket, bytesPerPacket, bytesPerFrame,
                 bytesPerSample, futureUse1; } CompressionInfo;
#pragma pack(pop)
OSErr GetCompressionInfo(short compressionID, unsigned int format, short numChannels,
                         short sampleSize, CompressionInfo *cp);
int main(void) {
   CompressionInfo ci = { sizeof ci, 0xdeadbeef, 77, 9, 9, 9, 9, 9 };
   OSErr e = GetCompressionInfo(0, 0x4E4F4E45 /*'NONE'*/, 2, 16, &ci);
   printf("err=%d size=%ld spp=%u bpp=%u bpf=%u bps=%u\n", e, ci.recordSize,
          ci.samplesPerPacket, ci.bytesPerPacket, ci.bytesPerFrame, ci.bytesPerSample);
   exit(sizeof ci == 20 && e == 0 && ci.recordSize == 20 && ci.format == 0x4E4F4E45 &&
        ci.samplesPerPacket == 1 && ci.bytesPerPacket == 2 && ci.bytesPerFrame == 4 &&
        ci.bytesPerSample == 2 ? 42 : 1);
}
