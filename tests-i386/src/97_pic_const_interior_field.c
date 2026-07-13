/* 97_pic_const_interior_field — PIC-anchored read of the INTERIOR of a
 * packed read-only __TEXT,__const struct.
 *
 * clang -O0 (default PIC, the suite's exact recipe) copies the {u16,u16,u16}
 * initializer template from __TEXT,__const via two anchor-relative loads:
 *
 *     movl 0x6b(%anchor), %ecx    ; fields 1+2   (template+0)
 *     movw 0x6f(%anchor), %ax     ; field 3      (template+4)  <- INTERIOR
 *
 * The 10-byte "%u %u %u\n" __cstring precedes __const, leaving the template
 * misaligned (2 mod 4), so DataParser parses it as [1B][1B][4B-Immediate...]
 * and template+4 lands INSIDE an Immediate — not at a blob boundary. The
 * PIC-anchor detector's mid-blob containing fallback (section.cc
 * DetectPicAnchoredDisps) was gated to WRITABLE data only, so the read-only
 * target stranded its placeholder and Parse2 parked it at the SECTION END:
 * the translated movw read the +6 padding (zeros) instead of field 3 and
 * this printed "0 0 0" (RGBColor c={0,0,0xFFFF} lost its blue channel — the
 * 6-byte odd-struct aggregate-init field report). Fix: admit read-only
 * opaque const/literal data too (vmaddr_in_readonly_opaque_data), exactly
 * like the M64 [rip+disp] re-parse gate (the Quinn _pieceSize1+2 fix) — an
 * anchored target is a DEREFERENCED address, never an integer constant.
 *
 * Sibling of 94_const_interior_movswl (same class, [rip+disp] flavor). */
extern int printf(const char *, ...);
extern void exit(int);

typedef struct { unsigned short red, green, blue; } RGBColor;

__attribute__((noinline)) void show(RGBColor *p) {
    printf("%u %u %u\n", p->red, p->green, p->blue);
}

int main(void) {
    RGBColor c = {0, 0, 0xFFFF};
    show(&c);
    exit(0);
    return 0;
}
