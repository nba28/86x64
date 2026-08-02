#!/usr/bin/env python3
"""gen_gli_dispatch.py — generate gli_dispatch_names.h for cgl_macro_shim.c.

<OpenGL/CGLMacro.h> is Apple's zero-overhead GL dispatch idiom: it #defines every
glXxx() to go straight through the context object,

    #define glGetString(name)  (*(cgl_ctx)->disp.get_string)((cgl_ctx)->rend, name)

so a 32-bit app compiled with it never calls a GL entry point at all — it indexes
the `GLIFunctionDispatch` table embedded by value in `struct _CGLContextObject`.
To hand translated i386 code a working context we must build an i386-layout shadow
of that table, which means knowing which GL function each SLOT INDEX denotes.

The field ORDER is the ABI, and it is era-specific: the 10.6 and current headers
agree only for indices 0..440 and diverge after. So the mapping is generated from
the ERA-CORRECT header (10.6, the last SDK that still had a 32-bit OpenGL), never
from the running system's table.

Field names are snake_case ("get_string", "tex_image2D", "bind_vertex_array_EXT");
GL symbols are camelCase with vendor suffixes that DO NOT always match the field's
("bind_vertex_array_EXT" is really glBindVertexArrayAPPLE). So each candidate is
verified against libabiconv's ACTUAL exported bridge list rather than trusted:
whatever we cannot verify is emitted as NULL and gets a loud diagnostic stub at
runtime — never a null function pointer, which is exactly the `jmp *0` this whole
mechanism exists to prevent.

Usage:
  gen_gli_dispatch.py <gliDispatch.h> <libabiconv.dylib|symbol-list> > gli_dispatch_names.h
"""
import re
import subprocess
import sys

VENDOR = {'ARB', 'EXT', 'ATI', 'APPLE', 'NV', 'SGI', 'SGIS', 'SGIX', 'HP', 'IBM',
          'INTEL', 'MESA', 'OES', 'SUN', 'WIN', 'AMD', 'KHR', '3DFX'}
# Suffix variants to try when the field's own vendor tag does not match the real
# symbol (Apple renamed several EXT extensions to APPLE when promoting them).
ALT = ('ARB', 'EXT', 'APPLE', 'ATI', 'NV', '')


# i386 stack-slot footprint of each scalar type. Everything is passed in 4-byte
# cdecl slots except the 8-byte floating/integer types; a pointer is 4 bytes in
# the 32-bit ABI regardless of what it points at.
EIGHT = {'GLdouble', 'GLclampd', 'double', 'GLint64', 'GLuint64', 'GLint64EXT',
         'GLuint64EXT', 'long long', 'unsigned long long'}


def arg_bytes(params):
    """i386 byte size of a parameter list that has ALREADY had the leading
    GLIContext dropped. Needed exactly, not approximately: the thunk shifts the
    argument block down over the `rend` slot, and copying more than the caller
    actually pushed would write past its outgoing-argument area and into its
    locals."""
    total = 0
    for raw in params:
        t = raw.strip()
        if not t or t == 'void':
            continue
        if '*' in t or '[' in t:
            total += 4                      # any pointer is 4 bytes on i386
            continue
        t = re.sub(r'\b(const|volatile|struct|unsigned|signed)\b', ' ', t)
        toks = t.split()
        base = toks[0] if toks else ''
        total += 8 if base in EIGHT else 4
    return total


def split_params(sig):
    """Split a parameter list at top-level commas."""
    out, depth, cur = [], 0, ''
    for ch in sig:
        if ch in '([':
            depth += 1
        elif ch in ')]':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur); cur = ''
        else:
            cur += ch
    if cur.strip():
        out.append(cur)
    return out


def fields(header_path):
    src = open(header_path).read()
    m = re.search(r'typedef struct __GLIFunctionDispatchRec(.*?)\n\}\s*GLIFunctionDispatch',
                  src, re.S)
    if not m:
        sys.exit('gen_gli_dispatch: GLIFunctionDispatch not found in ' + header_path)
    body = m.group(1)
    out = []
    for mm in re.finditer(r'\(\s*\*\s*(\w+)\s*\)\s*\(([^;]*)\)\s*;', body, re.S):
        params = split_params(mm.group(2))
        # every dispatch entry's first parameter is the GLIContext the thunk drops
        out.append((mm.group(1), arg_bytes(params[1:])))
    return out


def camel(field):
    out = ''
    for part in field.split('_'):
        if part.upper() in VENDOR and part == part.upper():
            out += part
        else:
            out += part[0].upper() + part[1:]
    return 'gl' + out


def candidates(field):
    base = camel(field)
    yield base
    parts = field.split('_')
    stem = camel('_'.join(parts[:-1])) if (
        parts[-1].upper() in VENDOR and parts[-1] == parts[-1].upper()) else base
    for alt in ALT:
        cand = stem + alt
        if cand != base:
            yield cand


def exported(path):
    if path.endswith('.dylib') or path.endswith('.o'):
        out = subprocess.run(['nm', '-g', path], capture_output=True, text=True).stdout
    else:
        out = open(path).read()
    names = set()
    for line in out.splitlines():
        parts = line.split()
        sym = parts[-1] if parts else ''
        if sym.startswith('___gl'):
            names.add(sym[3:])            # ___glBegin -> glBegin
    return names


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    entries = fields(sys.argv[1])
    names = [n for n, _ in entries]
    have = exported(sys.argv[2])
    if not have:
        sys.exit('gen_gli_dispatch: no ___gl* exports found in ' + sys.argv[2])

    rows, mapped = [], 0
    for i, (f, nbytes) in enumerate(entries):
        hit = next((c for c in candidates(f) if c in have), None)
        if hit:
            mapped += 1
        if nbytes > 255:
            sys.exit('gen_gli_dispatch: %s arg block %d > 255 bytes' % (f, nbytes))
        rows.append((i, f, hit, nbytes))

    w = sys.stdout.write
    w('/* GENERATED by gen_gli_dispatch.py — DO NOT EDIT.\n'
      ' * Slot index -> GL entry point for struct _CGLContextObject.disp\n'
      ' * (GLIFunctionDispatch), in the 32-bit field order that CGLMacro.h call\n'
      ' * sites were compiled against. Byte offset of slot i in the i386 layout is\n'
      ' * 4 + 4*i (disp follows the 4-byte GLIContext rend at offset 0).\n'
      ' * A NULL entry is deliberate: it means "no verified bridge", and the shim\n'
      ' * installs a loud diagnostic stub rather than a null pointer. */\n')
    w('#define GLI_DISPATCH_SLOTS %d\n\n' % len(names))
    w('static const char *const gli_slot_name[GLI_DISPATCH_SLOTS] = {\n')
    for i, f, hit, _ in rows:
        w('   /* %3d %-34s */ %s,\n'
          % (i, f, ('"%s"' % hit) if hit else 'NULL'))
    w('};\n\n')
    w('/* i386 byte size of each slot\'s argument block, EXCLUDING the leading\n'
      ' * GLIContext. gli_tramp.asm shifts exactly this many bytes down over the\n'
      ' * rend slot; copying more would write past the caller\'s outgoing-argument\n'
      ' * area into its locals. */\n')
    w('static const unsigned char gli_slot_argbytes[GLI_DISPATCH_SLOTS] = {\n')
    for i, f, _, nbytes in rows:
        w('   /* %3d %-34s */ %d,\n' % (i, f, nbytes))
    w('};\n')
    sys.stderr.write('gen_gli_dispatch: %d/%d slots mapped (%.1f%%)\n'
                     % (mapped, len(names), 100.0 * mapped / len(names)))


if __name__ == '__main__':
    main()
