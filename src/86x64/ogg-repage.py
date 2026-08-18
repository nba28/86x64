#!/usr/bin/env python3
"""ogg-repage.py — build controlled VARIANTS of an Ogg stream for A/B testing a
decoder, changing exactly ONE property at a time.

    ogg-repage.py <in.ogg> --split-headers <out.ogg>
    ogg-repage.py <in.ogg> --break-crc      <out.ogg>
    ogg-repage.py <in.ogg> --headers-only   <out.ogg>

WHY.  Halo's translated libVorbis fails ov_open with OV_EBADHEADER after
consuming exactly two 8500-byte chunks, on a bitstream whose three header
packets all end at offset 4344 -- i.e. it had everything it needed inside the
FIRST chunk and went back for more anyway.  Two very different faults produce
that same signature:

  (A) the SYNC layer stops accepting pages after the first one (CRC check,
      or the header/body length arithmetic), so it burns through the buffer
      hunting for a capture pattern; or
  (B) PACKET REASSEMBLY across a page boundary is broken -- the 4140-byte setup
      header spans pages 1->2 via 255-byte lacing continuation, so if the
      continuation is mishandled the third header never completes and the
      decoder keeps fetching pages looking for it.

--split-headers discriminates them.  It re-pages ONLY the header region so that
each of the three header packets sits alone on its own page and nothing spans a
boundary.  Page count, page sequence numbers, granule positions and every audio
page are left byte-identical, so the ONLY thing that changes is whether a packet
crosses a page.  If the split stream opens and the original does not, it is (B).

--break-crc flips one bit in every page's checksum.  It answers a different
question: is the checksum being VERIFIED at all?  If a stream with 21 bad CRCs
behaves exactly like the good one, the check is not discriminating.

--headers-only truncates after the third header packet, so the decoder hits a
real EOF instead of audio pages -- it separates "gave up" from "ran past the
headers".

The CRC is libogg's own (polynomial 0x04C11DB7, MSB-first, no reflection, no
final xor) and is SELF-CHECKED against the input before anything is emitted: if
this file cannot reproduce the CRCs already in the stream, it refuses to write,
because a variant carrying wrong checksums would fail the decoder for a reason
that has nothing to do with the experiment.
"""
import argparse, struct, sys

def crc_table():
    t = []
    for i in range(256):
        r = i << 24
        for _ in range(8):
            r = ((r << 1) ^ (0x04c11db7 if r & 0x80000000 else 0)) & 0xffffffff
        t.append(r)
    return t

TAB = crc_table()

def crc32(buf):
    r = 0
    for b in buf:
        r = ((r << 8) & 0xffffffff) ^ TAB[((r >> 24) & 0xff) ^ b]
    return r


class Page:
    __slots__ = ('htype', 'granule', 'serial', 'seq', 'segs', 'body')

    def parse(data, off):
        p = Page()
        if data[off:off + 4] != b'OggS':
            raise ValueError('no capture pattern at %#x' % off)
        p.htype = data[off + 5]
        p.granule = struct.unpack_from('<q', data, off + 6)[0]
        p.serial, p.seq, _crc = struct.unpack_from('<III', data, off + 14)
        nseg = data[off + 26]
        p.segs = list(data[off + 27:off + 27 + nseg])
        body = off + 27 + nseg
        blen = sum(p.segs)
        p.body = data[body:body + blen]
        return p, body + blen
    parse = staticmethod(parse)

    def emit(self):
        hdr = bytearray(b'OggS\x00')
        hdr.append(self.htype)
        hdr += struct.pack('<q', self.granule)
        hdr += struct.pack('<III', self.serial, self.seq, 0)
        hdr.append(len(self.segs))
        hdr += bytes(self.segs)
        raw = bytes(hdr) + self.body
        c = crc32(raw)
        return raw[:22] + struct.pack('<I', c) + raw[26:]

    def stored_crc(data, off):
        return struct.unpack_from('<I', data, off + 22)[0]
    stored_crc = staticmethod(stored_crc)


def lace(n):
    """libogg lacing for a packet of n bytes: 255s then a final <255."""
    out = [255] * (n // 255)
    out.append(n % 255)
    return out


def read_pages(data):
    pages, off = [], 0
    while off < len(data) and data[off:off + 4] == b'OggS':
        stored = Page.stored_crc(data, off)
        p, nxt = Page.parse(data, off)
        pages.append((p, off, nxt, stored))
        off = nxt
    return pages


def packets_of(pages):
    """[(bytes, index of the page the packet ENDED on)]"""
    out, cur = [], b''
    for i, (p, _o, _n, _c) in enumerate(pages):
        q = 0
        for s in p.segs:
            cur += p.body[q:q + s]; q += s
            if s < 255:
                out.append((cur, i)); cur = b''
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('src')
    ap.add_argument('out')
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--split-headers', action='store_true')
    g.add_argument('--break-crc', action='store_true')
    g.add_argument('--headers-only', action='store_true')
    a = ap.parse_args()

    data = open(a.src, 'rb').read()
    pages = read_pages(data)
    if not pages:
        sys.exit('no Ogg pages in %s' % a.src)

    # SELF-CHECK: our CRC must reproduce every checksum already in the stream.
    bad = sum(1 for (p, _o, _n, stored) in pages
              if struct.unpack_from('<I', p.emit(), 22)[0] != stored)
    if bad:
        sys.exit('refusing to write: this CRC implementation disagrees with %d '
                 'of %d checksums already in %s, so any variant it produced '
                 'would be wrong for reasons unrelated to the experiment'
                 % (bad, len(pages), a.src))
    print('crc self-check: %d/%d pages reproduce exactly' % (len(pages), len(pages)))

    if a.break_crc:
        out = bytearray(data)
        for (_p, off, _n, stored) in pages:
            struct.pack_into('<I', out, off + 22, stored ^ 0x00000001)
        open(a.out, 'wb').write(out)
        print('wrote %s: %d pages, every CRC flipped by one bit'
              % (a.out, len(pages)))
        return

    pkts = packets_of(pages)
    if len(pkts) < 3:
        sys.exit('fewer than three packets - not a vorbis stream')
    hdr = [pkts[i][0] for i in range(3)]
    for i, h in enumerate(hdr):
        want = (1, 3, 5)[i]
        if not (h[:1] == bytes([want]) and h[1:7] == b'vorbis'):
            sys.exit('packet %d is not vorbis header type %d' % (i, want))
    last_hdr_page = pkts[2][1]

    if a.headers_only:
        end = pages[last_hdr_page][2]
        open(a.out, 'wb').write(data[:end])
        print('wrote %s: %d bytes, truncated after the third header (page %d)'
              % (a.out, end, last_hdr_page))
        return

    # --split-headers: one header packet per page, audio pages untouched.
    serial = pages[0][0].serial
    newpages = []
    for i, h in enumerate(hdr):
        p = Page()
        p.htype = 0x02 if i == 0 else 0x00   # BOS only on the first
        p.granule = 0
        p.serial = serial
        p.seq = i
        p.segs = lace(len(h))
        p.body = h
        newpages.append(p)
    if last_hdr_page != 2:
        sys.exit('headers span %d pages, not 3 - the audio pages would have to '
                 'be renumbered and this variant would no longer be a '
                 'single-variable change' % (last_hdr_page + 1))
    out = b''.join(p.emit() for p in newpages) + data[pages[3][1]:]
    open(a.out, 'wb').write(out)
    print('wrote %s: 3 header pages (%d/%d/%d bytes of packet), audio pages '
          'byte-identical from page 3; %d bytes total'
          % (a.out, len(hdr[0]), len(hdr[1]), len(hdr[2]), len(out)))
    print('  ONLY difference vs the original: no packet spans a page boundary '
          'in the header region.')


if __name__ == '__main__':
    main()
