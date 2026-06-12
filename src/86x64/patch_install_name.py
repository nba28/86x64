#!/usr/bin/env python3
"""Patch an LC_LOAD_DYLIB path in-place. New path must be <= old length;
extras are null-padded so the LC structure stays intact."""
import sys, struct, subprocess

LC_LOAD_DYLIB    = 0x0c
LC_LOAD_WEAK_DYLIB = 0x18 | 0x80000000
LC_REEXPORT_DYLIB  = 0x1f | 0x80000000

def main():
    if len(sys.argv) != 4:
        print("usage: patch_install_name.py <file> <old_path> <new_path>"); sys.exit(2)
    path, old, new = sys.argv[1:4]
    if len(new) >= len(old):
        print(f"new path ({len(new)} bytes) must be shorter than old ({len(old)})"); sys.exit(1)
    data = bytearray(open(path,'rb').read())
    old_b = old.encode() + b'\x00'
    new_b = new.encode() + b'\x00'
    # find all occurrences and patch
    n_patched = 0
    pos = 0
    while True:
        i = data.find(old_b, pos)
        if i == -1: break
        # overwrite with new_b, null pad the rest
        data[i:i+len(old_b)] = new_b + b'\x00' * (len(old_b) - len(new_b))
        n_patched += 1
        pos = i + len(old_b)
    open(path,'wb').write(bytes(data))
    print(f"patched {n_patched} occurrence(s)")
    subprocess.run(['codesign','--force','--sign','-',path],check=False)
if __name__ == '__main__':
    main()
