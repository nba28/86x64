/* 99_augraph_nodeinfo -- guard for two AudioUnit contract fixes
 * (src/abiconv/coreaudio_au_shim.c; Call of Duty 4's 3D-mixer setup).
 *  1. AUGraphGetNodeInfo (Carbon-era, like AUGraphNewNode: ComponentDescription*, classDataSize*,
 *     classData**, AudioUnit*) must hand back the node's unit: the generated
 *     bridge left the 4-byte slot NULL. Kill M64_NO_AUGRAPH_NODEINFO=1.
 *  2. AudioUnitSetProperty(AudioChannelLayout, Stereo) on the 3DMixer's 2-channel
 *     OUTPUT scope: today's mixer answers -10879 (measured natively), the 2008 one
 *     accepted it. A layout that restates the stream format is accepted.
 *     Kill M64_NO_AU_LAYOUT_IMPLIED=1.
 * Exit 42 iff the unit is non-NULL, answers StreamFormat, and the layout set is
 * noErr. Values go to stderr. AudioToolbox is an undefined dynamic_lookup import
 * bound to libabiconv shims. No crt0: end with exit(). */
extern int fprintf(void *, const char *, ...);
extern void *__stderrp;
extern void exit(int);

typedef struct { unsigned type, sub, manuf, flags, mask; } CD;
typedef struct { unsigned tag, bitmap, ndesc; } Layout;
typedef void *AUGraph;
typedef void *AudioUnit;
typedef int OSStatus;

extern OSStatus NewAUGraph(AUGraph *);
extern OSStatus AUGraphNewNode(AUGraph, const CD *, unsigned csz, const void *cdata, int *node);
extern OSStatus AUGraphOpen(AUGraph);
extern OSStatus AUGraphGetNodeInfo(AUGraph, int node, CD *, unsigned *csz, void **cdata, AudioUnit *u);
extern OSStatus AudioUnitGetProperty(AudioUnit, unsigned id, unsigned scope, unsigned el, void *v, unsigned *sz);
extern OSStatus AudioUnitSetProperty(AudioUnit, unsigned id, unsigned scope, unsigned el, const void *v, unsigned sz);

int main(void)
{
    AUGraph g = 0;
    CD mix = { 0x61756d78 /*aumx*/, 0x33646d78 /*3dmx*/, 0x6170706c /*appl*/, 0, 0 };
    CD out = { 0x61756f75 /*auou*/, 0x64656620 /*def */, 0x6170706c, 0, 0 };
    int nm = 0, no = 0;
    OSStatus e1 = NewAUGraph(&g);
    OSStatus e2 = AUGraphNewNode(g, &mix, 0, 0, &nm);
    OSStatus e3 = AUGraphNewNode(g, &out, 0, 0, &no);
    OSStatus e4 = AUGraphOpen(g);
    fprintf(__stderrp, "new=%d newmix=%d newout=%d open=%d\n", e1, e2, e3, e4);

    CD d; unsigned csz = 7; void *cd = (void *)7; AudioUnit u = 0;
    OSStatus e5 = AUGraphGetNodeInfo(g, nm, &d, &csz, &cd, &u);
    fprintf(__stderrp, "nodeinfo=%d unit=%p desc=%08x/%08x\n", e5, u, d.type, d.sub);

    unsigned fmt[10] = {0}; unsigned sz = sizeof fmt;
    OSStatus e6 = u ? AudioUnitGetProperty(u, 8 /*StreamFormat*/, 2 /*Output*/, 0, fmt, &sz) : -1;
    fprintf(__stderrp, "getfmt=%d channels=%u\n", e6, fmt[7]);

    Layout l = { 0x640002 /*kAudioChannelLayoutTag_Stereo*/, 0, 0 };
    OSStatus e7 = u ? AudioUnitSetProperty(u, 19 /*AudioChannelLayout*/, 2, 0, &l, sizeof l) : -1;
    fprintf(__stderrp, "setlayout=%d\n", e7);

    exit(!e5 && u && !e6 && fmt[7] == 2 && !e7 ? 42 : 1);
}
