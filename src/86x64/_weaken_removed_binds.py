#!/usr/bin/env python3
"""Mark binds to GENUINELY-REMOVED symbols as weak-import in a translated
modern-LC_DYLD_INFO Mach-O, so dyld NULLs them at load instead of hard-failing.

Prototype for the pipeline step. A translated modern-dyld-info framework
(QuickTime) keeps its ORIGINAL bind opcodes (synthesize_dyld_info is a no-op
when a DyldInfo already exists), so its imports carry the non-weak flags they
had in 2018. Over-linked removed symbols (e.g. _CopyDeepMask, removed from
64-bit ApplicationServices) are non-lazy, non-weak binds -> dyld aborts the
whole load. Flip ONLY the binds whose symbol is genuinely gone from the modern
dyld namespace (dlsym(RTLD_DEFAULT)==0) and is not a libabiconv shim to
WEAK_IMPORT: an in-place 1-byte patch of the SET_SYMBOL_TRAILING_FLAGS_IMM
opcode (0x40|flags -> |0x01), no re-layout. dyld then binds them NULL; the image
LOADS; import_repair + the iterate loop cover any that are actually CALLED.
Present symbols are left untouched (they resolve normally / marshal).
"""
import ctypes, struct, subprocess, sys

BIND_OPCODE_MASK = 0xF0
BIND_IMM_MASK = 0x0F
DONE=0x00; SET_DYLIB_ORDINAL_IMM=0x10; SET_DYLIB_ORDINAL_ULEB=0x20
SET_DYLIB_SPECIAL_IMM=0x30; SET_SYMBOL_TRAILING_FLAGS_IMM=0x40; SET_TYPE_IMM=0x50
SET_ADDEND_SLEB=0x60; SET_SEGMENT_AND_OFFSET_ULEB=0x70; ADD_ADDR_ULEB=0x80
DO_BIND=0x90; DO_BIND_ADD_ADDR_ULEB=0xA0; DO_BIND_ADD_ADDR_IMM_SCALED=0xB0
DO_BIND_ULEB_TIMES_SKIPPING_ULEB=0xC0
WEAK_IMPORT=0x01

libsys=ctypes.CDLL("/usr/lib/libSystem.dylib")
libsys.dlsym.restype=ctypes.c_void_p; libsys.dlsym.argtypes=[ctypes.c_void_p,ctypes.c_char_p]
RTLD_DEFAULT=ctypes.c_void_p(-2)

def removed(sym, liba):
    # Runs AFTER static-interpose, so a covered import already carries its
    # libabiconv export name (___X / data-shadow / weak-coalesced ____ZdlPv) and
    # is a direct libabiconv export -> resolves, not removed. A symbol is removed
    # only if it is NEITHER a libabiconv export NOR present in the native dyld
    # namespace (dlsym RTLD_DEFAULT). dlsym drops one leading '_'.
    if sym in liba: return False
    name = sym[1:] if sym.startswith("_") else sym
    return not libsys.dlsym(RTLD_DEFAULT, name.encode())

def uleb(b, i):
    v=0; s=0
    while True:
        c=b[i]; i+=1; v |= (c&0x7f)<<s; s+=7
        if not c&0x80: break
    return v, i

def patch_stream(data, off, size, liba, verbose):
    """Walk one bind opcode stream; return list of (abs_off) SET_SYMBOL opcode
    bytes to OR with WEAK_IMPORT (removed symbols)."""
    patches=[]; i=off; end=off+size
    while i < end:
        op=data[i]&BIND_OPCODE_MASK; imm=data[i]&BIND_IMM_MASK; opi=i; i+=1
        if op==DONE: pass
        elif op==SET_DYLIB_ORDINAL_IMM: pass
        elif op==SET_DYLIB_ORDINAL_ULEB: _,i=uleb(data,i)
        elif op==SET_DYLIB_SPECIAL_IMM: pass
        elif op==SET_SYMBOL_TRAILING_FLAGS_IMM:
            j=i
            while data[j]!=0: j+=1
            sym=data[i:j].decode(); i=j+1
            if not (imm & WEAK_IMPORT) and removed(sym, liba):
                patches.append((opi, sym))
        elif op==SET_TYPE_IMM: pass
        elif op==SET_ADDEND_SLEB: _,i=uleb(data,i)
        elif op==SET_SEGMENT_AND_OFFSET_ULEB: _,i=uleb(data,i)
        elif op==ADD_ADDR_ULEB: _,i=uleb(data,i)
        elif op==DO_BIND: pass
        elif op==DO_BIND_ADD_ADDR_ULEB: _,i=uleb(data,i)
        elif op==DO_BIND_ADD_ADDR_IMM_SCALED: pass
        elif op==DO_BIND_ULEB_TIMES_SKIPPING_ULEB: _,i=uleb(data,i); _,i=uleb(data,i)
        else:
            raise SystemExit("unknown bind opcode 0x%02x at 0x%x"%(data[opi],opi))
    return patches

def main():
    path=sys.argv[1]
    liba_path=sys.argv[2] if len(sys.argv)>2 else None
    data=bytearray(open(path,'rb').read())
    liba=set()
    if liba_path:
        liba=set(subprocess.run(["nm","-gU",liba_path],capture_output=True,text=True).stdout.split())
    magic=struct.unpack_from('<I',data,0)[0]; assert magic==0xfeedfacf, "need 64-bit"
    ncmds=struct.unpack_from('<I',data,16)[0]; o=32
    bind_off=bind_size=lazy_off=lazy_size=weak_off=weak_size=0
    for _ in range(ncmds):
        cmd,sz=struct.unpack_from('<II',data,o)
        if cmd in (0x22,0x80000022):  # LC_DYLD_INFO / _ONLY
            _,_,bind_off,bind_size,weak_off,weak_size,lazy_off,lazy_size,_,_=struct.unpack_from('<10I',data,o+8)
        o+=sz
    allp=[]
    for name,foff,fsz in (("bind",bind_off,bind_size),("lazy",lazy_off,lazy_size),("weak",weak_off,weak_size)):
        if fsz: allp += [(name,)+p for p in patch_stream(data,foff,fsz,liba,True)]
    for name,opi,sym in allp:
        data[opi] |= WEAK_IMPORT
    open(path,'wb').write(data)
    print("weakened %d removed-symbol binds in %s"%(len(allp), path))
    for name,opi,sym in allp[:20]:
        print("  [%s] %s"%(name,sym))
    if len(allp)>20: print("  ... +%d more"%(len(allp)-20))

if __name__=="__main__":
    main()
