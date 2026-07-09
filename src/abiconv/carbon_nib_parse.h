// carbon_nib_parse.h — pure, framework-free reader for IBCarbon `objects.xib`
// (the XML form of a classic .nib). Split out from carbon_nib_shim.c so the
// parsing logic has ONE home and can be unit-tested headlessly (no HIToolbox /
// no window server) — see tests-i386 carbon_nib_parse_test.c.
//
// The .nib stores an NSIBObjectData archive as XML: nested <object class="...">
// elements with <string>/<int>/<ostype>/<boolean name="KEY"> fields, plus a
// <dictionary name="nameTable"> mapping human names ("Graphics") to object ids.
// These helpers do byte-oriented substring scans (the format is flat ASCII); they
// never allocate and never touch any framework, so they are safe in libabiconv and
// trivially testable.

#ifndef CARBON_NIB_PARSE_H
#define CARBON_NIB_PARSE_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { int16_t top, left, bottom, right; } nibx_rect;

// Read a whole file; caller free()s. *len set to byte count. NULL on error.
static inline char *nibx_slurp(const char *p, long *len) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = (char *)malloc(n + 1); if (!b) { fclose(f); return NULL; }
    if (fread(b, 1, n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    b[n] = 0; if (len) *len = n; fclose(f); return b;
}

// Byte offset of the </object> that closes the <object ...> beginning at `start`.
static inline long nibx_match_end(const char *x, long start) {
    int d = 0; const char *p = x + start;
    while (*p) {
        const char *o = strstr(p, "<object "), *c = strstr(p, "</object>");
        if (!c) return -1;
        if (o && o < c) { d++; p = o + 8; } else { d--; p = c + 9; if (!d) return c + 9 - x; }
    }
    return -1;
}

// <string name="KEY">VALUE</string> within [lo,hi). Returns 1 + copies VALUE.
static inline int nibx_str(const char *x, long lo, long hi, const char *k, char *o, int os) {
    char pat[128]; snprintf(pat, sizeof pat, "<string name=\"%s\">", k);
    const char *p = strstr(x + lo, pat); if (!p || p - x >= hi) return 0;
    p += strlen(pat); const char *e = strstr(p, "</string>"); if (!e) return 0;
    int n = (int)(e - p); if (n >= os) n = os - 1; memcpy(o, p, n); o[n] = 0; return 1;
}

// <int name="KEY">N</int> within [lo,hi).
static inline int nibx_int(const char *x, long lo, long hi, const char *k, int *v) {
    char pat[128]; snprintf(pat, sizeof pat, "<int name=\"%s\">", k);
    const char *p = strstr(x + lo, pat); if (!p || p - x >= hi) return 0;
    p += strlen(pat); *v = atoi(p); return 1;
}

// <boolean name="KEY">TRUE|FALSE</boolean> within [lo,hi).
static inline int nibx_bool(const char *x, long lo, long hi, const char *k, int *v) {
    char pat[128]; snprintf(pat, sizeof pat, "<boolean name=\"%s\">", k);
    const char *p = strstr(x + lo, pat); if (!p || p - x >= hi) return 0;
    p += strlen(pat); *v = (p[0] == 'T' || p[0] == 't' || p[0] == 'Y' || p[0] == '1'); return 1;
}

// <ostype name="KEY">ABCD</ostype> within [lo,hi) -> FourCharCode.
static inline int nibx_ostype(const char *x, long lo, long hi, const char *k, uint32_t *v) {
    char pat[128]; snprintf(pat, sizeof pat, "<ostype name=\"%s\">", k);
    const char *p = strstr(x + lo, pat); if (!p || p - x >= hi) return 0;
    p += strlen(pat); if (strlen(p) < 4) return 0;
    *v = ((uint32_t)(uint8_t)p[0] << 24) | ((uint32_t)(uint8_t)p[1] << 16) |
         ((uint32_t)(uint8_t)p[2] << 8) | (uint32_t)(uint8_t)p[3];
    return 1;
}

static inline void nibx_rect_parse(const char *s, nibx_rect *r) {
    int t, l, b, rr;
    if (sscanf(s, "%d %d %d %d", &t, &l, &b, &rr) == 4) { r->top = t; r->left = l; r->bottom = b; r->right = rr; }
}

// Decode the handful of XML entities IB emits, in place.
static inline void nibx_unescape(char *s) {
    char *o = s;
    for (char *p = s; *p; ) {
        if (*p == '&') {
            if (!strncmp(p, "&amp;", 5))  { *o++ = '&';  p += 5; continue; }
            if (!strncmp(p, "&apos;", 6)) { *o++ = '\''; p += 6; continue; }
            if (!strncmp(p, "&quot;", 6)) { *o++ = '"';  p += 6; continue; }
            if (!strncmp(p, "&lt;", 4))   { *o++ = '<';  p += 4; continue; }
            if (!strncmp(p, "&gt;", 4))   { *o++ = '>';  p += 4; continue; }
        }
        *o++ = *p++;
    }
    *o = 0;
}

// Object id bound to `name` in <dictionary name="nameTable">, or -1.
static inline int nibx_nametable_id(const char *x, const char *name) {
    const char *nt = strstr(x, "name=\"nameTable\""); if (!nt) return -1;
    char pat[256]; snprintf(pat, sizeof pat, "<string>%s</string>", name);
    const char *p = strstr(nt, pat); if (!p) return -1;
    const char *r = strstr(p, "<reference idRef=\""); if (!r) return -1;
    return atoi(r + strlen("<reference idRef=\""));
}

// Byte offset of `<object class="IBCarbonWindow" id="ID">`, or -1.
static inline long nibx_window_offset(const char *x, int id) {
    char pat[64]; snprintf(pat, sizeof pat, "<object class=\"IBCarbonWindow\" id=\"%d\">", id);
    const char *w = strstr(x, pat); return w ? w - x : -1;
}

#endif // CARBON_NIB_PARSE_H
