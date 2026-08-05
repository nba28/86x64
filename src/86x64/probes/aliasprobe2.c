/* Stage 2: does native FSResolveAlias resolve at all (no move)? And does the
 * CFURL bookmark route follow a move? */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <CoreFoundation/CoreFoundation.h>

typedef struct { unsigned char h[80]; } FSRefX;
static int32_t (*p_FSPathMakeRef)(const unsigned char*, void*, unsigned char*);
static int32_t (*p_FSRefMakePath)(const void*, unsigned char*, unsigned int);
static int32_t (*p_FSNewAlias)(const void*, const void*, void**);
static int32_t (*p_FSResolveAlias)(const void*, void*, void*, unsigned char*);
static int32_t (*p_FSResolveAliasWithMountFlags)(const void*, void*, void*, unsigned char*, unsigned long);
static long   (*p_GetHandleSize)(void*);
static void*  (*p_NewHandle)(long);
static int32_t (*p_FSCopyAliasInfo)(void*, void*, void*, CFStringRef*, unsigned int*, void*);

static void show(const char *tag, void *alh, const char *want)
{
    FSRefX out; unsigned char changed = 0, path[1024]; path[0] = 0;
    memset(&out, 0, sizeof out);
    int32_t e = p_FSResolveAliasWithMountFlags(NULL, alh, &out, &changed, 1 /*kResolveAliasFileNoUI*/);
    if (e == 0) p_FSRefMakePath(&out, path, sizeof path);
    printf("%-22s err=%-5d changed=%d path='%s' MATCH=%d\n", tag, e, changed, path,
           e == 0 && want && strcmp((char*)path, want) == 0);
    CFStringRef ps = NULL; unsigned int which = 0;
    if (p_FSCopyAliasInfo && p_FSCopyAliasInfo(alh, NULL, NULL, &ps, &which, NULL) == 0 && ps) {
        char buf[1024]; CFStringGetCString(ps, buf, sizeof buf, kCFStringEncodingUTF8);
        printf("%-22s FSCopyAliasInfo path='%s'\n", "", buf);
        CFRelease(ps);
    }
}

int main(void)
{
#define L(n) p_##n = (void*)dlsym(RTLD_DEFAULT, #n);
    L(FSPathMakeRef) L(FSRefMakePath) L(FSNewAlias) L(FSResolveAlias)
    L(FSResolveAliasWithMountFlags) L(GetHandleSize) L(NewHandle) L(FSCopyAliasInfo)

    char dir[256], a[300], b[300];
    snprintf(dir, sizeof dir, "/tmp/aliasprobe2.%d", (int)getpid());
    mkdir(dir, 0777);
    snprintf(a, sizeof a, "%s/one.txt", dir);
    snprintf(b, sizeof b, "%s/moved.txt", dir);
    FILE *f = fopen(a, "w"); fputs("hello", f); fclose(f);

    FSRefX ref; memset(&ref, 0, sizeof ref);
    printf("FSPathMakeRef=%d\n", p_FSPathMakeRef((const unsigned char*)a, &ref, NULL));
    void *alh = NULL;
    printf("FSNewAlias=%d\n", p_FSNewAlias(NULL, &ref, &alh));
    if (!alh) return 1;
    long n = p_GetHandleSize(alh);
    unsigned char *rec = *(unsigned char **)alh;

    show("in place", alh, a);

    /* dump the tag list so we know what Apple actually stores */
    printf("record %ld bytes; tags from 150:\n", n);
    for (long o = 150; o + 4 <= n; ) {
        short tag = (short)((rec[o] << 8) | rec[o+1]);
        unsigned short len = (unsigned short)((rec[o+2] << 8) | rec[o+3]);
        if (tag == -1) { printf("  tag -1 (end)\n"); break; }
        printf("  tag %3d len %3u : ", tag, len);
        for (unsigned i = 0; i < len && i < 90; i++) {
            unsigned char c = rec[o+4+i];
            putchar((c >= 32 && c < 127) ? c : '.');
        }
        printf("\n");
        o += 4 + len + (len & 1);
    }

    rename(a, b);
    show("after move", alh, b);

    /* ---- CFURL bookmark route, same experiment ---- */
    rename(b, a);
    CFURLRef u = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8*)a, strlen(a), false);
    CFErrorRef err = NULL;
    CFDataRef bm = CFURLCreateBookmarkData(NULL, u, 0, NULL, NULL, &err);
    printf("bookmark bytes=%ld\n", bm ? (long)CFDataGetLength(bm) : -1L);
    if (bm) {
        rename(a, b);
        Boolean stale = false; err = NULL;
        CFURLRef r = CFURLCreateByResolvingBookmarkData(NULL, bm,
                        (1UL << 8) /*WithoutUI*/ | (1UL << 9) /*WithoutMounting*/,
                        NULL, NULL, &stale, &err);
        char got[1024] = "";
        if (r) CFURLGetFileSystemRepresentation(r, true, (UInt8*)got, sizeof got);
        printf("bookmark after move -> '%s' MATCH=%d stale=%d\n", got,
               strcmp(got, b) == 0, stale);
        if (r) CFRelease(r);
        rename(b, a);
    }
    unlink(a); unlink(b); rmdir(dir);
    return 0;
}
