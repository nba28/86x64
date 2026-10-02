#!/usr/bin/env python3
"""steamgen.py — generate libabiconv's Steamworks interface bridge tables.

  steamgen.py OUT.inc NATIVE_libsteam_api.dylib PRIMARY.json [FALLBACK.json ...]

Reads Valve's machine-readable Steamworks SDK description (steam_api.json:
every interface, its version string, and its methods in VTABLE order with
typed params) and emits C for src/abiconv/steam_bridge.c: for each interface
VERSION, one marshaller per method that unpacks the i386 cdecl stack slots and
calls SLOT k of the NATIVE object's own vtable with exactly typed arguments.
Calling the vtable (not the SDK's flat SteamAPI_ISteamX_Y exports) is what
makes a version the native libsteam_api was not compiled for still work: the
object steamclient hands out for "SteamInput006" has the 006 layout.

VTABLE SLOTS COME FROM THE NATIVE BINARY, not the JSON order. Every flat
export SteamAPI_ISteamX_Y in the native libsteam_api loads the vtable from self
(`movq (%rdi),%R`, or %rsi for a struct return) and calls slot `0xNN(%R)`; that
is ground truth. The JSON order is wrong after a deprecated method that still
occupies a slot but has no JSON/flat entry (ISteamClient has 5 such holes,
ISteamGameServer slot 0), after a virtual destructor (2 slots, ISteamHTMLSurface)
and for overloads (GetStat int/float differ only in methodname_flat). A version
the native lib was not compiled for (no SteamAPI_<accessor>_vNNN export) falls
back to JSON order and is marked UNVERIFIED in the output. Holes become GAP slots.

A version defined in several JSONs comes from the first one listed. ISteamClient
carries no version_string in the JSON; it is the native libsteam_api's own
SteamClient020 (verified: its 36 methods == the 36 SteamAPI_ISteamClient_*
exports of Portal 2's x86_64 libsteam_api).

What cannot be marshalled safely becomes a GAP method (loud, returns 0): a
pointer to a pointer, an i386 interface/response object or function pointer
passed IN, a struct whose layout has a pointer member, a float-carrying struct
by value. Implement those only when coverage shows them REACHED.
"""
import json, re, sys

import subprocess
OUT, NATIVE, JSONS = sys.argv[1], sys.argv[2], sys.argv[3:]
docs = [json.load(open(p)) for p in JSONS]


def native_slots(lib):
    """{flatname: slot} and the set of accessor exports, from the x86_64 slice."""
    asm = subprocess.run(['otool', '-arch', 'x86_64', '-tV', lib], capture_output=True,
                         text=True).stdout.splitlines()
    funcs, cur = {}, None
    for l in asm:
        m = re.match(r'^_(SteamAPI_ISteam\w+):$', l)
        if m: cur = m.group(1); funcs[cur] = []; continue
        if re.match(r'^_\w+:$', l): cur = None; continue
        if cur and len(funcs[cur]) < 60: funcs[cur].append(l)
    def slot(body, selfreg):
        vreg = None
        for l in body:
            m = re.search(r'movq\s+\(%' + selfreg + r'\), %(r\w+)', l)
            if m and vreg is None: vreg = m.group(1); continue
            if vreg:
                m = re.search(r'(-?0x[0-9a-f]+)?\(%' + vreg + r'\)', l)
                if m: return int(m.group(1), 16) // 8 if m.group(1) else 0
        return None
    out = {}
    for name, body in funcs.items():
        s = slot(body, 'rdi')
        if s is None: s = slot(body, 'rsi')
        if s is not None: out[name] = s
    exports = set(l.split()[-1].lstrip('_') for l in subprocess.run(
        ['nm', '-gU', '-arch', 'x86_64', lib], capture_output=True, text=True).stdout.splitlines() if l.strip())
    return out, exports


NSLOT, NEXPORTS = native_slots(NATIVE)

typedefs, enums, structs = {}, set(), {}
for d in reversed(docs):                 # primary wins
    typedefs.update({t['typedef']: t['type'] for t in d['typedefs']})
    enums.update(e['enumname'] for e in d['enums'])
    for s in d['structs'] + d['callback_structs']:
        structs[s['struct']] = s['fields']

INT = {'char': 1, 'signed char': 1, 'unsigned char': 1, 'bool': 1, 'short': 2,
       'unsigned short': 2, 'int': 4, 'unsigned int': 4, 'long long': 8,
       'unsigned long long': 8, 'float': 4, 'double': 8, 'long': 4,
       'unsigned long': 4, 'uint64_steamid': 8, 'uint64_gameid': 8}
CTYPE = {'char': 'int8_t', 'signed char': 'int8_t', 'unsigned char': 'uint8_t',
         'short': 'int16_t', 'unsigned short': 'uint16_t', 'int': 'int32_t',
         'unsigned int': 'uint32_t', 'long long': 'int64_t',
         'unsigned long long': 'uint64_t', 'long': 'int32_t', 'unsigned long': 'uint32_t'}
# 8-byte value classes passed/returned like a uint64 on both ABIs
U64_CLASSES = {'CSteamID', 'CGameID'}


def base(t):
    t = t.replace('const ', '').strip()
    seen = 0
    while t in typedefs and seen < 20:
        t = typedefs[t].replace('const ', '').strip(); seen += 1
    return t


def struct_has_pointer(name, depth=0):
    if depth > 6 or name not in structs:
        return False
    for f in structs[name]:
        ft = f['fieldtype']
        if '*' in ft or '(' in ft:
            return True
        b = base(re.sub(r'\[.*\]', '', ft).strip())
        if '*' in b or struct_has_pointer(b, depth + 1):
            return True
    return False


def struct_size(name, depth=0):
    """pack(4) size (Steam's mac callback/struct packing); None if unknown."""
    if name in U64_CLASSES:
        return 8
    if depth > 6 or name not in structs:
        return None
    off = 0
    for f in structs[name]:
        ft = f['fieldtype']
        m = re.match(r'(.*?)\s*\[\s*(\d+)\s*\]\s*$', ft)
        n = int(m.group(2)) if m else 1
        b = base(m.group(1) if m else ft)
        if b in INT: sz = INT[b]
        elif b in enums: sz = 4
        elif b in structs or b in U64_CLASSES: sz = struct_size(b, depth + 1)
        else: return None
        if sz is None: return None
        al = min(sz, 4) if sz else 1
        off = (off + al - 1) // al * al + sz * n
    return (off + 3) // 4 * 4


def struct_has_float(name, depth=0):
    if depth > 6 or name not in structs:
        return False
    for f in structs[name]:
        b = base(re.sub(r'\[.*\]', '', f['fieldtype']).strip())
        if b in ('float', 'double') or struct_has_float(b, depth + 1):
            return True
    return False


def classify(t):
    """-> (kind, ctype, slots) ; kind in B I32 I64 F D P SV GAP"""
    raw = t.strip()
    if raw.endswith('&'):
        raw = raw[:-1].strip() + ' *'
    if raw.count('*') >= 2:
        return ('GAP', 'pointer to pointer', 0)
    if raw.endswith('*'):
        pointee = base(raw[:-1].strip())
        if pointee.startswith('ISteam') or pointee.endswith('Response'):
            return ('GAP', f'i386 object {pointee} passed in', 0)
        if pointee in structs and struct_has_pointer(pointee):
            return ('GAP', f'{pointee} has pointer members', 0)
        return ('P', 'void *', 1)
    b = base(raw)
    if '(' in b:
        return ('GAP', 'function pointer', 0)
    if b.endswith('*'):                       # pointer typedef: an opaque handle
        return ('P', 'void *', 1)
    if b in ('intptr_t', 'intp'): return ('I32', 'intptr_t', 1)   # i386 slot sign-extends
    if b in ('uintptr_t', 'uintp'): return ('I32', 'uintptr_t', 1)
    if '::' in b and b.split('::')[-1] in enums | {b.split('::')[-1]} and b.split('::')[-1].startswith('E'):
        return ('I32', 'int32_t', 1)                               # nested enum
    if b == 'bool': return ('B', 'bool', 1)
    if b == 'float': return ('F', 'float', 1)
    if b == 'double': return ('D', 'double', 2)
    if b in U64_CLASSES or b in ('uint64_steamid', 'uint64_gameid'):
        return ('I64', 'uint64_t', 2)
    if b in CTYPE:
        ct = CTYPE[b]
        return ('I64', ct, 2) if INT[b] == 8 else ('I32', ct, 1)
    if b in enums: return ('I32', 'int32_t', 1)
    if b in structs:
        sz = struct_size(b)
        if sz is None: return ('GAP', f'{b} size unknown', 0)
        if struct_has_pointer(b): return ('GAP', f'{b} has pointer members', 0)
        if sz <= 16 and struct_has_float(b): return ('GAP', f'{b} float struct by value', 0)
        return ('SV', f'struct sbv{sz}', (sz + 3) // 4)
    if b == 'void': return ('V', 'void', 0)
    return ('GAP', f'unknown type {t}', 0)


def ret_class(t, params):
    raw = t.strip()
    if raw == 'void': return ('V', 'void', None)
    if raw.replace(' ', '') in ('constchar*', 'char*'): return ('STR', 'const char *', None)
    if raw.endswith('*'):
        pointee = base(raw[:-1].strip())
        ver = next((p['paramname'] for p in params if p['paramname'] == 'pchVersion'), None)
        if (pointee.startswith('ISteam') or pointee == 'void') and ver:
            return ('IFACE', 'void *', ver)
        return ('PTR', 'void *', None)
    k, ct, _ = classify(raw)
    if k == 'SV':
        sz = int(ct[len('struct sbv'):])
        if sz in (4, 8) and not struct_has_float(base(raw)):
            return ('SVREG', ct, sz)
        return ('SRET', ct, sz)
    if k in ('B', 'I32', 'I64', 'F', 'D'): return (k, ct, None)
    if k == 'P': return ('PTR', 'void *', None)
    return ('GAP', ct, None)


ifaces = {}
for d in docs:
    for i in d['interfaces']:
        ver = i.get('version_string') or ('SteamClient020' if i['classname'] == 'ISteamClient' else None)
        if ver and ver not in ifaces:
            ifaces[ver] = i

out, sizes, nmeth, ngap = [], set(), 0, 0
out.append('/* GENERATED by src/86x64/steamgen.py from Steamworks SDK steam_api.json —\n'
           ' * do not edit; regenerate. One marshaller per method, vtable order. */\n')
tables = []
for ver, i in sorted(ifaces.items()):
    cls = i['classname']
    names = []
    native = ver == 'SteamClient020' or any(a.get('name_flat') in NEXPORTS for a in i.get('accessors', []))
    placed = {}
    for k, m in enumerate(i['methods']):
        s_ = NSLOT.get(m['methodname_flat']) if native else k
        if s_ is None: s_ = k if not native else None
        if s_ is not None and s_ not in placed: placed[s_] = m
    nslots = max(placed) + 1 if placed else 0
    if not native:
        out.append(f'/* {ver}: UNVERIFIED layout (JSON order; native lib has no {ver}) */')
    for k in range(nslots):
        m = placed.get(k)
        if m is None:
            m = {'methodname': f'slot{k}', 'params': [], 'returntype': 'void', 'hole': True}
        fn = f'sbm_{re.sub(r"[^A-Za-z0-9]", "_", ver)}_{k}'
        names.append((fn, m))
        nmeth += 1
        label = f'{cls}::{m["methodname"]}'
        params = [(p, classify(p['paramtype'])) for p in m['params']]
        rk, rct, rx = ret_class(m['returntype'], m['params'])
        gaps = [f'{p["paramname"]}: {c[1]}' for p, c in params if c[0] == 'GAP']
        if rk == 'GAP': gaps.append(f'return: {rct}')
        if m.get('hole'): gaps.append('vtable slot with no SDK method (deprecated)')
        out.append(f'/* {ver} #{k} {label} */')
        if gaps:
            ngap += 1
            out.append(f'static void {fn}(const uint32_t *a, struct sb_ret *r, void *self) {{\n'
                       f'   (void)self; GAP_ONCE("steam", "{label}", 0, a[-1]); r->v = 0; r->kind = SB_INT;'
                       f' /* {"; ".join(gaps)} */\n}}')
            continue
        decl, call, slot = ['void *'], ['self'], 0
        body = []
        for p, (kind, ct, n) in params:
            v = f'a[r->argbase + {slot}]'
            if kind == 'B':   e = f'(bool)({v} & 0xff)'
            elif kind == 'I32': e = f'({ct}){v}'
            elif kind == 'I64': e = f'({ct})sb_u64(&{v})'
            elif kind == 'F': e = f'sb_f32(&{v})'
            elif kind == 'D': e = f'sb_f64(&{v})'
            elif kind == 'P': e = f'(void *)(uintptr_t){v}'
            elif kind == 'SV':
                sizes.add(int(ct[len('struct sbv'):]))
                body.append(f'   {ct} p{slot}; memcpy(&p{slot}, &{v}, sizeof p{slot});')
                e = f'p{slot}'
            decl.append(ct); call.append(e); slot += n
        rtype = {'V': 'void', 'STR': 'const char *', 'IFACE': 'void *', 'PTR': 'void *',
                 'SVREG': rct, 'SRET': rct}.get(rk, rct)
        if rk in ('SVREG', 'SRET'): sizes.add(rx)
        out.append(f'static void {fn}(const uint32_t *a, struct sb_ret *r, void *self) {{')
        out.append(f'   typedef {rtype} (*fn_t)({", ".join(decl)});')
        out.append(f'   fn_t f = (fn_t)(*(void ***)self)[{k}];')
        out.extend(body)
        c = f'f({", ".join(call)})'
        if rk == 'V':      out.append(f'   {c}; r->kind = SB_VOID;')
        elif rk == 'B':    out.append(f'   r->v = (uint8_t){c}; r->kind = SB_INT;')
        elif rk == 'I32':  out.append(f'   r->v = (uint32_t){c}; r->kind = SB_INT;')
        elif rk == 'I64':  out.append(f'   r->v = (uint64_t){c}; r->kind = SB_INT;')
        elif rk == 'F':    out.append(f'   r->f = {c}; r->kind = SB_FLT;')
        elif rk == 'D':    out.append(f'   r->f = {c}; r->kind = SB_FLT;')
        elif rk == 'STR':  out.append(f'   r->v = sb_lowstr({c}); r->kind = SB_INT;')
        elif rk == 'PTR':  out.append(f'   r->v = sb_lowptr({c}, "{label}"); r->kind = SB_INT;')
        elif rk == 'IFACE':
            vi = next(n for n, (p, _) in enumerate(params) if p['paramname'] == rx)
            vslot = sum(c[2] for _, c in params[:vi])
            out.append(f'   r->v = sb_wrap({c}, (const char *)(uintptr_t)a[r->argbase + {vslot}]); r->kind = SB_INT;')
        elif rk == 'SVREG':
            out.append(f'   {rct} v = {c}; uint64_t u = 0; memcpy(&u, &v, sizeof v); r->v = u; r->kind = SB_INT;')
        elif rk == 'SRET':
            out.append(f'   {rct} v = {c}; memcpy((void *)(uintptr_t)a[0], &v, sizeof v);'
                       f' r->v = a[0]; r->kind = SB_SRET;')
        out.append('}')
    tables.append((ver, cls, names))

pre = ['#include <stdbool.h>']
for s in sorted(sizes):
    pre.append(f'struct sbv{s} {{ uint8_t b[{s}]; }};')
out.insert(1, '\n'.join(pre) + '\n')
for ver, cls, names in tables:
    cid = re.sub(r'[^A-Za-z0-9]', '_', ver)
    out.append(f'static const struct sb_method SBT_{cid}[] = {{')
    for fn, m in names:
        rk, _, _ = ret_class(m['returntype'], m['params'])
        out.append(f'   {{ {fn}, {1 if rk == "SRET" else 0}, "{cls}::{m["methodname"]}" }},')
    out.append('};')
out.append('static const struct sb_iface SB_IFACES[] = {')
for ver, cls, names in tables:
    cid = re.sub(r'[^A-Za-z0-9]', '_', ver)
    out.append(f'   {{ "{ver}", {len(names)}, SBT_{cid} }},')
out.append('};')
out.append(f'#define SB_MAX_METHODS {max(len(n) for _, _, n in tables)}')
open(OUT, 'w').write('\n'.join(out) + '\n')
print(f'{len(tables)} interface versions, {nmeth} methods, {ngap} GAP methods -> {OUT}', file=sys.stderr)
