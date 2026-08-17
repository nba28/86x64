#!/usr/bin/env python3
"""Decode a captured S3TC blob to PNG, and say whether it is COHERENT.

Why this exists: the client-storage experiment's verdict is visual, and "does
the background look right" is a question about a screen that only a human can
answer. This asks a better one — is the DATA HALO HANDED TO OPENGL already
wrong? — and answers it from the bytes, with no rendering and no eyeballs.

  blob decodes coherently at its declared size  -> Halo's data is fine; the
      corruption is downstream (driver / APPLE_client_storage / our mapping)
  blob is already sheared/garbage               -> the defect is UPSTREAM in
      translated Halo code, and no GL-side change can fix it

It also re-decodes at neighbouring dimensions. A diagonal shear is what you get
when an image is interpreted at the wrong width, so if the blob looks coherent
at some OTHER width, the declared dimensions are the bug — a much more specific
finding than "the bytes are wrong".

Usage: dxt-decode.py <file.dxt> [...]      (filenames carry WxH and fmt)
"""

import re
import struct
import sys
import zlib
from pathlib import Path

FMT = {0x83F1: ("DXT1", 8), 0x83F2: ("DXT3", 16), 0x83F3: ("DXT5", 16)}


def rgb565(v):
    return (((v >> 11) & 31) * 255 // 31,
            ((v >> 5) & 63) * 255 // 63,
            (v & 31) * 255 // 31)


def decode(data, w, h, block_bytes, dxt1):
    """Decode S3TC to an RGB bytearray; colour endpoints only (alpha ignored)."""
    out = bytearray(w * h * 3)
    bw, bh = max(1, w // 4), max(1, h // 4)
    for by in range(bh):
        for bx in range(bw):
            off = (by * bw + bx) * block_bytes
            if off + block_bytes > len(data):
                return out, False
            # colour block sits at the END of a DXT3/5 block, start of a DXT1
            c = off + (0 if dxt1 else 8)
            c0, c1, bits = struct.unpack_from("<HHI", data, c)
            p = [rgb565(c0), rgb565(c1)]
            if c0 > c1 or not dxt1:
                p.append(tuple((2 * p[0][i] + p[1][i]) // 3 for i in range(3)))
                p.append(tuple((p[0][i] + 2 * p[1][i]) // 3 for i in range(3)))
            else:
                p.append(tuple((p[0][i] + p[1][i]) // 2 for i in range(3)))
                p.append((0, 0, 0))
            for py in range(4):
                for px in range(4):
                    x, y = bx * 4 + px, by * 4 + py
                    if x >= w or y >= h:
                        continue
                    col = p[(bits >> (2 * (4 * py + px))) & 3]
                    i = (y * w + x) * 3
                    out[i:i + 3] = bytes(col)
    return out, True


def write_png(path, w, h, rgb):
    raw = b"".join(b"\0" + bytes(rgb[y * w * 3:(y + 1) * w * 3]) for y in range(h))

    def chunk(tag, body):
        return (struct.pack(">I", len(body)) + tag + body +
                struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF))

    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 6))
           + chunk(b"IEND", b""))
    Path(path).write_bytes(png)


def coherence(rgb, w, h):
    """Mean |difference| across BLOCK-ROW boundaries, normalised.

    A correctly-interpreted image is smooth down columns. Interpreted at the
    wrong width it is not: each row of blocks is displaced relative to the one
    above, so vertical neighbours become unrelated. Lower is more coherent. This
    is a RELATIVE measure — only compare it across widths of the same blob.

    ⚠Compare rows FOUR apart, not adjacent ones. S3TC blocks are 4 pixels tall,
    so three out of four adjacent row-pairs lie INSIDE a block and are near
    identical whatever the width — which made an adjacent-row metric report 0.00
    for every candidate and discriminate nothing."""
    if h < 8:
        return 999.0
    tot = n = 0
    step = max(4, (h // 64) * 4)
    for y in range(0, h - 4, step):
        base, nxt = y * w * 3, (y + 4) * w * 3
        for x in range(0, w * 3, 3):
            tot += abs(rgb[base + x] - rgb[nxt + x])
            n += 1
    return tot / max(1, n)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    for f in sys.argv[1:]:
        p = Path(f)
        m = re.search(r"_(\d+)x(\d+)_fmt([0-9a-f]+)", p.name)
        if not m:
            print(f"{p.name}: cannot parse WxH/fmt from filename — skipping")
            continue
        w, h, fmt = int(m.group(1)), int(m.group(2)), int(m.group(3), 16)
        name, bb = FMT.get(fmt, ("?", 8))
        data = p.read_bytes()
        expect = max(1, w // 4) * max(1, h // 4) * bb
        print(f"\n{p.name}: {name} {w}x{h}  {len(data)} bytes "
              f"(expect {expect}) {'OK' if len(data) == expect else '⚠MISMATCH'}")

        rgb, ok = decode(data, w, h, bb, fmt == 0x83F1)
        out = p.with_suffix(".png")
        write_png(out, w, h, rgb)
        base = coherence(rgb, w, h)
        print(f"  declared {w:5}x{h:<5} vertical-delta {base:7.2f}   -> {out.name}")

        # A shear is the signature of a wrong width, so try the neighbours.
        best, bestw = base, w
        for cand in (w // 4, w // 2, w * 2, w * 4):
            if cand < 4 or (w * h) % cand:
                continue
            ch = (w * h) // cand
            if ch < 4:
                continue
            r2, _ = decode(data, cand, ch, bb, fmt == 0x83F1)
            c = coherence(r2, cand, ch)
            flag = ""
            if c < best * 0.7:
                best, bestw, flag = c, cand, "   <== MORE COHERENT"
            print(f"  alt      {cand:5}x{ch:<5} vertical-delta {c:7.2f}{flag}")
            if flag:
                write_png(p.with_name(p.stem + f"_alt{cand}x{ch}.png"), cand, ch, r2)
        if bestw != w:
            print(f"  ⇒ decodes BETTER at {bestw} wide than the declared {w}: the"
                  f" DIMENSIONS are the bug, not the bytes.")


if __name__ == "__main__":
    main()
