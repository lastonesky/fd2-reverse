#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""从 FDOTHER.DAT 第 4 项（16x16 1bpp 字库）导出字形大图。

用法：
    python tools/glyph_view.py 160-179 out.png
    python tools/glyph_view.py 169,170 out.png
"""
import struct
import sys
import zlib

FDOTHER = "E:/FD2/FDOTHER.DAT"
FONT_ENTRY = 4
GLYPH_BYTES = 32


def load_font(path=FDOTHER, index=FONT_ENTRY):
    raw = open(path, "rb").read()
    tab = [struct.unpack_from("<I", raw, 6 + 4 * i)[0] for i in range(index + 2)]
    blob = raw[tab[index]:tab[index + 1]]
    if len(blob) % GLYPH_BYTES:
        raise SystemExit(f"entry {index} size {len(blob)} is not a multiple of 32")
    return blob


def glyph_bitmap(font, idx):
    """返回 16 行，每行 16 个 0/1；与 gfx_draw_glyph 一致（字内 xchg al,ah）。"""
    g = font[idx * GLYPH_BYTES:(idx + 1) * GLYPH_BYTES]
    rows = []
    for r in range(16):
        w = struct.unpack_from("<H", g, r * 2)[0]
        w = ((w >> 8) | (w << 8)) & 0xFFFF
        rows.append([(w >> (15 - c)) & 1 for c in range(16)])
    return rows


DIGITS = {
    "0": "111101101101111",
    "1": "010110010010111",
    "2": "111001111100111",
    "3": "111001111001111",
    "4": "101101111001001",
    "5": "111100111001111",
    "6": "111100111101111",
    "7": "111001001001001",
    "8": "111101111101111",
    "9": "111101111001111",
}


def write_png(path, w, h, rows):
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        raw.extend(rows[y])

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    open(path, "wb").write(png)


def render(font, idxs, out, scale=12, cols=5, bg=(250, 250, 250),
           fg=(24, 24, 24), grid=(224, 224, 224)):
    pad = 14
    label_h = 30
    cell_w = 16 * scale + pad * 2
    cell_h = 16 * scale + pad + label_h
    rows_n = (len(idxs) + cols - 1) // cols
    w, h = cols * cell_w, rows_n * cell_h

    img = [bytearray(bg * w) for _ in range(h)]

    def put(x, y, color):
        if 0 <= x < w and 0 <= y < h:
            img[y][x * 3:x * 3 + 3] = bytes(color)

    for k, idx in enumerate(idxs):
        cx = (k % cols) * cell_w
        cy = (k // cols) * cell_h
        ox = cx + pad
        oy = cy + pad
        bm = glyph_bitmap(font, idx)
        for r in range(16):
            for c in range(16):
                color = fg if bm[r][c] else grid
                for dy in range(scale):
                    for dx in range(scale):
                        put(ox + c * scale + dx, oy + r * scale + dy, color)
        # 编号
        sx, sy = cx + pad, cy + pad + 16 * scale + 6
        for ch in str(idx):
            for r in range(5):
                for c in range(3):
                    if DIGITS[ch][r * 3 + c] == "1":
                        for dy in range(3):
                            for dx in range(3):
                                put(sx + c * 3 + dx, sy + r * 3 + dy, fg)
            sx += 12

    write_png(out, w, h, img)
    return w, h


def parse_spec(spec):
    out = []
    for part in spec.split(","):
        part = part.strip()
        if "-" in part:
            a, b = part.split("-")
            out.extend(range(int(a, 0), int(b, 0) + 1))
        elif part:
            out.append(int(part, 0))
    return out


if __name__ == "__main__":
    spec = sys.argv[1] if len(sys.argv) > 1 else "160-179"
    out = sys.argv[2] if len(sys.argv) > 2 else "glyphs.png"
    scale = int(sys.argv[3]) if len(sys.argv) > 3 else 12
    font = load_font()
    idxs = parse_spec(spec)
    w, h = render(font, idxs, out, scale=scale)
    print(f"wrote {out}  {w}x{h}  glyphs={idxs}")
