#!/usr/bin/env python3
"""bridge-epilogue-scan.py — check every HAND-WRITTEN i386 bridge pops the
caller's 4-byte return address.

A translated i386 caller pushes a 4-BYTE return address, so a bridge that is
i386-callable must return with exactly:

    mov r11d, [rsp]        ; read the 4-byte return address
    add rsp, 4             ; POP it
    jmp r11

abigen emits that shape for every generated bridge. The hand-written `.asm`
bridges are written by people, and three of them (`sigaction`, `execvp`,
`getopt`) simply left out the `add rsp, 4`. The caller then returns with esp 4
LOW — silently, for the rest of that frame's life, until its own epilogue pops
one word short and `ret` jumps to whatever sat below the return address.

★ SIGNATURE of the resulting crash: `rip == r11` and both point INTO THE STACK
region (`prot=rw-`, `err=0x15` instruction fetch), with the real return address
sitting 4 bytes ABOVE the popped word and the popped word looking like a saved
`ebp` (part of a chain of frame-pointer-ish values 0x20 apart).

MEASURED: Portal 2, `CProcessUtils::Init` (libvstdlib) calls `sigaction` to
install a SIGCHLD handler; its own `ret` then popped the saved ebp.

This is a static shape check, not a proof of correctness — it answers exactly
one question and says nothing about argument marshalling.

    bridge-epilogue-scan.py [dir]      (default: src/abiconv)
"""
import glob, os, re, sys

MOV_R11 = re.compile(r'^mov\s+r11d\s*,')
JMP_R11 = re.compile(r'^jmp\s+r11\b')


def scan(d):
    bad = []
    checked = 0
    for f in sorted(glob.glob(os.path.join(d, '*.asm'))):
        lines = [l.split(';')[0].strip() for l in open(f, errors='replace')]
        for i, l in enumerate(lines):
            if not MOV_R11.match(l):
                continue
            window = [x for x in lines[i + 1:i + 6] if x]
            j = next((k for k, x in enumerate(window) if JMP_R11.match(x)), None)
            if j is None:
                continue
            checked += 1
            if not any(re.search(r'\brsp\b', x) for x in window[:j]):
                bad.append((f, i + 1))
    return checked, bad


def main():
    d = sys.argv[1] if len(sys.argv) > 1 else 'src/abiconv'
    checked, bad = scan(d)
    print("i386 return sequences checked: %d" % checked)
    if not bad:
        print("all of them pop the 4-byte return address — OK")
        return 0
    print("MISSING the `add rsp, 4` (caller's esp is left 4 LOW):")
    for f, n in bad:
        print("  %s:%d" % (f, n))
    return 1


if __name__ == '__main__':
    sys.exit(main())
