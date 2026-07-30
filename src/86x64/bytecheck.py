#!/usr/bin/env python3
"""Civ IV memdisp-code-alias FINAL-vs-FINAL byte check.

The GMemory free-list builder computes its terminator as an INTEGER constant:
    lea  0x27ec(%ecx),%edx        # 0x27ec == 0x2800 - 0x14  ->  8d 91 ec 27 00 00
The bug relocated that constant as an ADDRESS, emitting instead a PIC anchor:
    lea  r11,[rip+...]            ->  4c 8d 1d xx xx xx xx
    leal (%rcx,%r11),%edx         ->  42 8d 14 19
so `end = block + <garbage>`, the loop never meets its terminator and walks off
the heap chunk at stride 0x14 (-> SIGBUS on a non-writable page, or a spin).

FIXED  == the constant form survives (byte-identical to the pristine i386).
BROKEN == the PIC-anchor form is present.
"""
import os, sys, pathlib

FIXED  = bytes.fromhex('89c1' '8d91ec270000')          # mov %eax,%ecx ; lea 0x27ec(%ecx),%edx
CONST  = bytes.fromhex('8d91ec270000')                 # the lea alone (pristine i386 form)
ANCHOR = bytes.fromhex('4c8d1d')                       # lea r11,[rip+...]
TAIL   = bytes.fromhex('428d1419')                     # leal (%rcx,%r11),%edx


def count(hay: bytes, needle: bytes) -> int:
    n, i = 0, hay.find(needle)
    while i != -1:
        n += 1
        i = hay.find(needle, i + 1)
    return n


def broken_pairs(hay: bytes) -> int:
    """PIC-anchor immediately followed (within 8 bytes) by the r11 index add."""
    n, i = 0, hay.find(ANCHOR)
    while i != -1:
        if TAIL in hay[i + 7:i + 15]:
            n += 1
        i = hay.find(ANCHOR, i + 1)
    return n


def report(label, path):
    p = pathlib.Path(path)
    if not p.exists():
        print(f'{label:26} MISSING  {path}')
        return None
    b = p.read_bytes()
    c_fixed = count(b, FIXED)
    c_const = count(b, CONST)
    c_broke = broken_pairs(b)
    print(f'{label:26} size={len(b):>10,}  fixed[89c1+lea0x27ec]={c_fixed}'
          f'  lea0x27ec_any={c_const}  pic_anchor+r11_pairs={c_broke}')
    return c_fixed, c_const, c_broke


if __name__ == '__main__':
    SP = os.environ.get('M64_WORK', os.path.expanduser('~/.86x64-work'))
    STEAM = ("$HOME/Library/Application Support/Steam/steamapps/common/"
             "Sid Meier's Civilization IV 34440")

    print('=== ground truth ===')
    report('pristine i386', f'{SP}/civsrc.i386')
    print('=== deployed (PRE-fix, expect BROKEN) ===')
    report('deployed dylib', f'{STEAM}/Civilization IV (Steam).app/Contents/MacOS/Civilization IV.dylib')
    print('=== this run ===')
    on  = report('gate ON  (expect FIXED)',  f'{SP}/civsrc_gateon.out.dylib')
    off = report('gate OFF (expect BROKEN)', f'{SP}/civsrc_gateoff.out.dylib')

    print()
    if on:
        f, c, b = on
        verdict = 'FIXED' if (f >= 1 and b == 0) else ('BROKEN' if b >= 1 else 'INCONCLUSIVE')
        print(f'VERDICT gate ON : {verdict}   (want fixed>=1 and pic_pairs==0)')
    if off:
        f, c, b = off
        print(f'VERDICT gate OFF: {"BROKEN (good A/B control)" if b >= 1 else "did NOT reproduce - A/B INVALID"}')
