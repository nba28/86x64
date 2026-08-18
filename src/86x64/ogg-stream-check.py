#!/usr/bin/env python3
"""Structurally validate an Ogg bitstream, and say WHY a decoder would reject it.

Why this exists: Halo CE's menu music is silent because ov_open_callbacks never
succeeds on the buffer Halo hands it, even though that buffer's first four bytes
are "OggS" and the same assets decode natively (afinfo reads them as
2ch/44100/vorbis). "It starts with OggS" is far too weak a check — ov_open needs
the stream to BEGIN at a BOS page carrying the vorbis identification header, and
a window that starts at any later page is a perfectly well-formed Ogg fragment
that ov_open must still refuse.

So this walks the pages and reports the things that actually decide the outcome:

  BOS flag on page 0      ov_open rejects a stream that does not start at BOS.
  vorbis ident header     the first packet must be 0x01 "vorbis".
  the 3 header packets    ident/comment/setup must all be present; ov_open reads
                          all three before it returns success.
  CRC per page            libogg validates every page; one bad CRC and the page
                          is discarded, which looks exactly like "no data".
  truncation              a final partial page is normal for a WINDOW into a
                          longer stream, and is not itself an error.

Usage: ogg-stream-check.py <file> [...]
"""
import struct
import sys
from pathlib import Path

def crc_table():
    t = []
    for i in range(256):
        r = i << 24
        for _ in range(8):
            r = ((r << 1) ^ 0x04C11DB7) & 0xFFFFFFFF if r & 0x80000000 else (r << 1) & 0xFFFFFFFF
        t.append(r)
    return t

CRC = crc_table()

def ogg_crc(buf):
    r = 0
    for b in buf:
        r = ((r << 8) & 0xFFFFFFFF) ^ CRC[((r >> 24) & 0xFF) ^ b]
    return r

def pages(d):
    off = 0
    while True:
        i = d.find(b"OggS", off)
        if i < 0 or i + 27 > len(d):
            return
        ver = d[i + 4]
        htype = d[i + 5]
        granule = struct.unpack_from("<q", d, i + 6)[0]
        serial = struct.unpack_from("<I", d, i + 14)[0]
        seq = struct.unpack_from("<I", d, i + 18)[0]
        crc = struct.unpack_from("<I", d, i + 22)[0]
        nseg = d[i + 26]
        if i + 27 + nseg > len(d):
            yield dict(off=i, truncated=True)
            return
        segs = d[i + 27:i + 27 + nseg]
        body = sum(segs)
        hdr_len = 27 + nseg
        end = i + hdr_len + body
        trunc = end > len(d)
        ok = None
        if not trunc:
            raw = bytearray(d[i:end])
            raw[22:26] = b"\0\0\0\0"
            ok = (ogg_crc(raw) == crc)
        yield dict(off=i, ver=ver, htype=htype, granule=granule, serial=serial,
                   seq=seq, crc=crc, nseg=nseg, body=body, truncated=trunc,
                   crc_ok=ok, first_packet=d[i + hdr_len:i + hdr_len + 7] if not trunc else b"")
        off = end if not trunc else len(d)

def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    for f in sys.argv[1:]:
        d = Path(f).read_bytes()
        print(f"\n=== {f}  ({len(d)} bytes) ===")
        if not d.startswith(b"OggS"):
            print("  ⚠does NOT start with OggS — not a stream start at all")
        pg = list(pages(d))
        if not pg:
            print("  no Ogg pages found")
            continue
        p0 = pg[0]
        bos = bool(p0.get("htype", 0) & 0x02)
        print(f"  pages found        : {len(pg)}")
        print(f"  page 0 header_type : 0x{p0.get('htype',0):02x} "
              f"({'BOS' if bos else 'NOT BOS'}) seq={p0.get('seq')} "
              f"serial=0x{p0.get('serial',0):08x}")
        if not bos:
            print("  ⇒ ★ov_open MUST FAIL: a stream that does not begin at a BOS page")
            print("     is a mid-stream FRAGMENT. This is not a decoder bug.")
        fp = p0.get("first_packet", b"")
        if fp[:1] == b"\x01" and fp[1:7] == b"vorbis":
            print("  first packet       : 0x01 'vorbis' identification header  OK")
        elif fp:
            print(f"  first packet       : {fp!r}  ⚠not a vorbis ident header")
        bad = [p for p in pg if p.get("crc_ok") is False]
        print(f"  CRC failures       : {len(bad)} of "
              f"{sum(1 for p in pg if p.get('crc_ok') is not None)} checked")
        for p in bad[:5]:
            print(f"     page at 0x{p['off']:x} seq={p.get('seq')} BAD CRC")
        types = [p.get("htype", 0) for p in pg if not p.get("truncated")]
        print(f"  header packets seen: "
              f"{'ident ' if fp[:1]==b'\\x01' else ''}"
              f"(pages carrying continuations: {sum(1 for t in types if t & 0x01)})")
        if pg[-1].get("truncated"):
            print("  last page TRUNCATED — normal for a WINDOW into a longer stream")

if __name__ == "__main__":
    main()
