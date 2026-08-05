/* Stage 4: APPEND our bookmark tag AFTER the -1 terminator (the documented
 * custom-data extension point: GetAliasSize returns the alias-record size,
 * GetHandleSize the total). Apple's parser must be completely unaffected. */
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
static int32_t (*p_FSResolveAliasWithMountFlags)(const void*, void*, void*, unsigned char*, unsigned long);
static long   (*p_GetHandleSize)(void*);
static void*  (*p_NewHandle)(long);
static int32_t (*p_GetAliasSize)(void*);

static void *hfb(const unsigned char *b, long n)
{ void *h = p_NewHandle(n); if (h) memcpy(*(void **)h, b, n); return h; }

static int resolve(void *alh, char *out, size_t outsz)
{ FSRefX r; unsigned char ch = 0; memset(&r,0,sizeof r); out[0]=0;
  int32_t e = p_FSResolveAliasWithMountFlags(NULL, alh, &r, &ch, 1);
  if (e == 0) p_FSRefMakePath(&r, (unsigned char*)out, (unsigned int)outsz); return e; }

int main(void)
{
#define L(n) p_##n = (void*)dlsym(RTLD_DEFAULT, #n);
    L(FSPathMakeRef) L(FSRefMakePath) L(FSNewAlias) L(FSResolveAliasWithMountFlags)
    L(GetHandleSize) L(NewHandle) L(GetAliasSize)

    char dir[256], a[300], b[300];
    snprintf(dir, sizeof dir, "/tmp/aliasprobe4.%d", (int)getpid());
    mkdir(dir, 0777);
    snprintf(a, sizeof a, "%s/one.txt", dir);
    snprintf(b, sizeof b, "%s/moved.txt", dir);
    FILE *f = fopen(a, "w"); fputs("hi", f); fclose(f);

    FSRefX ref; memset(&ref,0,sizeof ref);
    p_FSPathMakeRef((const unsigned char*)a, &ref, NULL);
    void *alh = NULL; p_FSNewAlias(NULL, &ref, &alh);
    long n = p_GetHandleSize(alh);
    unsigned char *rec = *(unsigned char **)alh;
    char base[1024]; resolve(alh, base, sizeof base);

    CFURLRef u = CFURLCreateFromFileSystemRepresentation(NULL,(const UInt8*)a,strlen(a),false);
    CFDataRef bm = CFURLCreateBookmarkData(NULL,u,0,NULL,NULL,NULL);
    long bl = CFDataGetLength(bm);

    /* append: 8-byte magic + 4-byte BE length + blob, straight after the record */
    long nn = n + 12 + bl;
    unsigned char *nb = calloc(1, nn);
    memcpy(nb, rec, n);
    memcpy(nb + n, "M64ALIAS", 8);
    nb[n+8]=(unsigned char)(bl>>24); nb[n+9]=(unsigned char)(bl>>16);
    nb[n+10]=(unsigned char)(bl>>8); nb[n+11]=(unsigned char)bl;
    memcpy(nb + n + 12, CFDataGetBytePtr(bm), bl);

    void *hy = hfb(nb, nn);
    char got[1024];
    int e = resolve(hy, got, sizeof got);
    printf("orig=%ld total=%ld aliasSizeField=%u\n", n, nn, (unsigned)((nb[4]<<8)|nb[5]));
    printf("APPENDED in place: err=%d SAME=%d GetAliasSize=%d GetHandleSize=%ld\n",
           e, e==0 && strcmp(got,base)==0, p_GetAliasSize(hy), p_GetHandleSize(hy));

    rename(a, b);
    e = resolve(hy, got, sizeof got);
    printf("APPENDED after move: apple err=%d (expected -43)\n", e);
    { CFDataRef d = CFDataCreate(NULL, nb + n + 12, bl); Boolean st=false;
      CFURLRef r = CFURLCreateByResolvingBookmarkData(NULL,d,(1UL<<8)|(1UL<<9),NULL,NULL,&st,NULL);
      char p2[1024]=""; if (r) CFURLGetFileSystemRepresentation(r,true,(UInt8*)p2,sizeof p2);
      printf("BOOKMARK after move -> '%s' FOLLOWED=%d\n", p2, strstr(p2,"moved.txt")!=NULL); }

    /* the plain Apple record (no appendix) after the move, via our OWN tag-18 read */
    { long off=-1,len=0;
      for (long o=150;o+4<=n;) { short t=(short)((rec[o]<<8)|rec[o+1]);
        unsigned short l=(unsigned short)((rec[o+2]<<8)|rec[o+3]);
        if (t==-1) break; if (t==18){off=o+4;len=l;break;} o+=4+l+(l&1); }
      char p3[1024]=""; if(off>0){ memcpy(p3+1,rec+off,len); p3[0]='/'; p3[len+1]=0; }
      printf("tag18 path='%s'\n", p3); }
    rename(b, a); unlink(a); rmdir(dir);
    return 0;
}
