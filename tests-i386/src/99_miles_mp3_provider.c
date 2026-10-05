/* 99_miles_mp3_provider — Miles finds and drives an MP3 "ASI codec" provider
 * the way Portal 2's vaudio_miles does (RIB_find_files_provider(".RAW" out, ".MP3" in) +
 * RIB_request_interface("ASI stream")), against the game's own libMilesX86 and
 * the replacement provider (src/86x64/shims/miles_mp3). Decodes $MP3_IN to
 * 16-bit PCM in $PCM_OUT. Exit 42 = decoded; 1 = no MP3 provider (the mute-voice
 * bug); 2.. = provider found but the stream failed. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct { uint32_t type; const char *name; uint32_t token; uint32_t subtype; } RIB_ENTRY;
extern int32_t AIL_startup(void);
extern void AIL_set_redist_directory(const char *dir);
extern uint32_t RIB_find_files_provider(const char *iface, const char *p1, const char *s1, const char *p2, const char *s2);
extern int32_t RIB_request_interface(uint32_t provider, const char *iface, int32_t n, RIB_ENTRY *e);

static FILE *g_in;
static int32_t fetch(uint32_t user, void *dest, int32_t bytes, int32_t offset) {
   (void)user;
   if (offset >= 0) { fseek(g_in, offset, SEEK_SET); }
   return (int32_t)fread(dest, 1, (size_t)bytes, g_in);
}
static void *a(uint32_t size, uint32_t user, const char *f, uint32_t l) { (void)user; (void)f; (void)l; return malloc(size); }
static void fr(void *p, uint32_t user, const char *f, uint32_t l) { (void)user; (void)f; (void)l; free(p); }

static int run(void) {
   const char *in = getenv("MP3_IN"), *outp = getenv("PCM_OUT");
   if (!in || !outp || !(g_in = fopen(in, "rb"))) { return 3; }
   fseek(g_in, 0, SEEK_END); const long size = ftell(g_in); fseek(g_in, 0, SEEK_SET);
   AIL_set_redist_directory(".");
   AIL_startup();
   const uint32_t p = RIB_find_files_provider("ASI codec", "Output file types", ".RAW", "Input file types", ".MP3");
   if (!p) { printf("no MP3 provider\n"); return 1; }
   /* a REQUEST entry's token is where Miles stores the provider's token */
   uint32_t t[6] = { 0 };
   RIB_ENTRY e[] = {
      { 0, "ASI_stream_open", (uint32_t)&t[0], 0 }, { 0, "ASI_stream_process", (uint32_t)&t[1], 0 },
      { 0, "ASI_stream_close", (uint32_t)&t[2], 0 }, { 0, "ASI_stream_property", (uint32_t)&t[3], 0 },
      { 1, "Output sample rate", (uint32_t)&t[4], 0 }, { 1, "Output channels", (uint32_t)&t[5], 0 },
   };
   if (RIB_request_interface(p, "ASI stream", 6, e) != 0 || !t[0] || !t[1] || !t[2] || !t[3]) { return 4; }
   uint32_t (*open_)(void *, void *, uint32_t, void *, uint32_t) = (void *)t[0];
   int32_t (*process)(uint32_t, void *, int32_t) = (void *)t[1];
   int32_t (*close_)(uint32_t) = (void *)t[2];
   int32_t (*prop)(uint32_t, uint32_t, void *, const void *, void *) = (void *)t[3];
   const uint32_t s = open_(a, fr, 0, fetch, (uint32_t)size);
   if (!s) { return 5; }
   uint32_t rate = 0, ch = 0;
   prop(s, t[4], &rate, NULL, NULL);
   prop(s, t[5], &ch, NULL, NULL);
   FILE *out = fopen(outp, "wb");
   static char buf[8192];
   long total = 0;
   int32_t n;
   while ((n = process(s, buf, sizeof buf)) > 0) { fwrite(buf, 1, (size_t)n, out); total += n; }
   fclose(out);
   close_(s);
   printf("rate=%u channels=%u pcm_bytes=%ld\n", rate, ch, total);
   return total > 0 && rate && ch ? 42 : 6;
}

int main(void) { exit(run()); }   /* -e _main: no crt, no argv, nothing to return to */
