#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成「字形原图 + 当前识别字」的对照表 PNG，供人工核对 glyphs.json。

为什么需要：glyphs.json 的映射是早期按图谱肉眼识别的，一旦某处认错一个字，
后面的字会整体顺延错位（2026-10-17 就发现 169 起错了 6 个字：人界 -> 人類）。
把位图和标注并排画出来，一眼就能看出「图上是什么字 / 标注写的是什么字」。

字库来源：FDOTHER.DAT 资源 4，16x16 1bpp，每字形 32 字节
（boot.c: dword_53A75 = res_load(FDOTHER,4)；gfx_draw_glyph 0x4ED7A: table + idx*32，
 每行 u16 小端读出后 xchg al,ah，与下面的 rows() 一致）。

不依赖 Pillow：直接 CreateDIBSection 写像素，中文标签用 GDI TextOutW 画
（Windows 专用，字体 SimSun / Consolas）。

用法：
    python tools/glyph_atlas.py                     # 全部 1824 个，16 页
    python tools/glyph_atlas.py --lo 160 --hi 200   # 只看一段
    python tools/glyph_atlas.py --outdir .workbuddy/atlas --per-page 256
"""
from __future__ import annotations

import ctypes
import ctypes.wintypes as wt
import json
import os
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
FD2DATA = os.path.join(HERE, "fd2data")
DEFAULT_DAT = "E:/FD2/FDOTHER.DAT"
FONT_RES = 4
GLYPH_BYTES = 32

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32

gdi32.CreateDIBSection.restype = ctypes.c_void_p
gdi32.CreateDIBSection.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint,
                                   ctypes.POINTER(ctypes.c_void_p), ctypes.c_void_p,
                                   ctypes.c_uint]
gdi32.CreateCompatibleDC.restype = ctypes.c_void_p
gdi32.CreateCompatibleDC.argtypes = [ctypes.c_void_p]
gdi32.SelectObject.restype = ctypes.c_void_p
gdi32.SelectObject.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
gdi32.CreateFontW.restype = ctypes.c_void_p
gdi32.CreateFontW.argtypes = [ctypes.c_int] * 5 + [ctypes.c_uint] * 8 + [ctypes.c_wchar_p]
gdi32.TextOutW.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int,
                           ctypes.c_wchar_p, ctypes.c_int]
gdi32.SetTextColor.argtypes = [ctypes.c_void_p, ctypes.c_uint]
gdi32.SetBkMode.argtypes = [ctypes.c_void_p, ctypes.c_int]
gdi32.DeleteObject.argtypes = [ctypes.c_void_p]
gdi32.DeleteDC.argtypes = [ctypes.c_void_p]
user32.GetDC.restype = ctypes.c_void_p
user32.GetDC.argtypes = [ctypes.c_void_p]
user32.ReleaseDC.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
gdi32.GetTextExtentPoint32W.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p,
                                        ctypes.c_int, ctypes.POINTER(wt.SIZE)]
gdi32.MoveToEx.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_void_p]
gdi32.LineTo.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
gdi32.CreatePen.restype = ctypes.c_void_p
gdi32.CreatePen.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_uint]


class BIH(ctypes.Structure):
    _fields_ = [("biSize", wt.DWORD), ("biWidth", wt.LONG), ("biHeight", wt.LONG),
                ("biPlanes", wt.WORD), ("biBitCount", wt.WORD),
                ("biCompression", wt.DWORD), ("biSizeImage", wt.DWORD),
                ("biXPelsPerMeter", wt.LONG), ("biYPelsPerMeter", wt.LONG),
                ("biClrUsed", wt.DWORD), ("biClrImportant", wt.DWORD)]


def load_font(path=DEFAULT_DAT, index=FONT_RES):
    raw = open(path, "rb").read()
    tab = [struct.unpack_from("<I", raw, 6 + 4 * i)[0] for i in range(index + 2)]
    blob = raw[tab[index]:tab[index + 1]]
    if len(blob) % GLYPH_BYTES:
        raise SystemExit(f"entry {index} size {len(blob)} not multiple of 32")
    return blob


def glyph_rows(font, idx):
    """16 行 x 16 列 0/1，与 gfx_draw_glyph 一致。"""
    g = font[idx * GLYPH_BYTES:(idx + 1) * GLYPH_BYTES]
    out = []
    for r in range(16):
        w = struct.unpack_from("<H", g, r * 2)[0]
        w = ((w >> 8) | (w << 8)) & 0xFFFF
        out.append([(w >> (15 - c)) & 1 for c in range(16)])
    return out


class Canvas:
    """32bpp top-down DIB + GDI 文本。"""

    def __init__(self, w, h, bg=0xFFFFFF):
        self.w, self.h = w, h
        bih = BIH()
        bih.biSize = ctypes.sizeof(BIH)
        bih.biWidth = w
        bih.biHeight = -h                      # top-down
        bih.biPlanes = 1
        bih.biBitCount = 32
        self.bits = ctypes.c_void_p()
        self.screen = user32.GetDC(0)
        self.hbmp = gdi32.CreateDIBSection(self.screen, ctypes.byref(bih), 0,
                                           ctypes.byref(self.bits), None, 0)
        self.dc = gdi32.CreateCompatibleDC(self.screen)
        gdi32.SelectObject(self.dc, self.hbmp)
        ctypes.memset(self.bits, 0xFF, w * h * 4)
        self.buf = (ctypes.c_ubyte * (w * h * 4)).from_address(self.bits.value)
        gdi32.SetBkMode(self.dc, 1)            # TRANSPARENT
        self._fonts = []

    def px(self, x, y, r, g, b):
        if not (0 <= x < self.w and 0 <= y < self.h):
            return
        o = (y * self.w + x) * 4
        self.buf[o] = b
        self.buf[o + 1] = g
        self.buf[o + 2] = r

    def rect(self, x, y, w, h, r, g, b):
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                self.px(xx, yy, r, g, b)

    def hline(self, x0, x1, y, r, g, b):
        for x in range(x0, x1):
            self.px(x, y, r, g, b)

    def vline(self, x, y0, y1, r, g, b):
        for y in range(y0, y1):
            self.px(x, y, r, g, b)

    def text(self, x, y, s, size=12, face="Consolas", color=0x000000, bold=False):
        f = gdi32.CreateFontW(-size, 0, 0, 0, 700 if bold else 400, 0, 0, 0,
                              1, 0, 0, 4, 0, face)   # DEFAULT_CHARSET, ANTIALIASED
        old = gdi32.SelectObject(self.dc, f)
        gdi32.SetTextColor(self.dc, color)
        gdi32.TextOutW(self.dc, x, y, s, len(s))
        gdi32.SelectObject(self.dc, old)
        gdi32.DeleteObject(f)

    def text_center(self, cx, y, s, size=12, face="Consolas", color=0x000000, bold=False):
        f = gdi32.CreateFontW(-size, 0, 0, 0, 700 if bold else 400, 0, 0, 0,
                              1, 0, 0, 4, 0, face)
        old = gdi32.SelectObject(self.dc, f)
        sz = wt.SIZE()
        gdi32.GetTextExtentPoint32W(self.dc, s, len(s), ctypes.byref(sz))
        gdi32.SetTextColor(self.dc, color)
        gdi32.TextOutW(self.dc, cx - sz.cx // 2, y, s, len(s))
        gdi32.SelectObject(self.dc, old)
        gdi32.DeleteObject(f)

    def close(self):
        gdi32.DeleteObject(self.hbmp)
        gdi32.DeleteDC(self.dc)
        user32.ReleaseDC(0, self.screen)


def write_png(path, w, h, bgra):
    """bgra: ctypes buffer, 32bpp top-down BGRA -> PNG (RGB)."""
    raw = bytearray()
    mv = bytes(bgra)
    for y in range(h):
        raw.append(0)
        row = mv[y * w * 4:(y + 1) * w * 4]
        rgb = bytearray(w * 3)
        rgb[0::3] = row[2::4]
        rgb[1::3] = row[1::4]
        rgb[2::3] = row[0::4]
        raw.extend(rgb)

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 6))
    png += chunk(b"IEND", b"")
    open(path, "wb").write(png)


def render_page(font, mapping, lo, hi, out, cols=16, scale=4,
                cell_w=86, cell_h=110):
    rows = (hi - lo + cols - 1) // cols
    w, h = cols * cell_w, rows * cell_h + 46
    cv = Canvas(w, h)

    # 页眉
    cv.text(12, 8, f"FD2 glyph atlas   {lo:04d}..{hi - 1:04d}   "
                   f"({hi - lo} glyphs)   [16x16 1bpp from FDOTHER.DAT res4]",
            size=17, face="Consolas", color=0x202020, bold=True)
    cv.text(12, 28, "每格：上为原始位图，下为「编号 + 当前识别字」。"
                    "若图上字形与标注不符，即为错位/误识。", size=14, face="SimSun",
            color=0x555555)

    for k in range(hi - lo):
        idx = lo + k
        r, c = divmod(k, cols)
        x0 = c * cell_w
        y0 = r * cell_h + 46
        # 网格
        cv.hline(x0, x0 + cell_w, y0, 0xE2, 0xE2, 0xE2)
        cv.vline(x0, y0, y0 + cell_h, 0xE2, 0xE2, 0xE2)
        if r == rows - 1:
            cv.hline(x0, x0 + cell_w, y0 + cell_h - 1, 0xE2, 0xE2, 0xE2)

        bm = glyph_rows(font, idx)
        blank = not any(any(row) for row in bm)
        gx = x0 + (cell_w - 16 * scale) // 2
        gy = y0 + 6
        for yy in range(16):
            for xx in range(16):
                r_, g_, b_ = (24, 24, 24) if bm[yy][xx] else (236, 236, 236)
                for dy in range(scale):
                    for dx in range(scale):
                        cv.px(gx + xx * scale + dx, gy + yy * scale + dy, r_, g_, b_)

        ch = mapping.get(str(idx))
        ly = gy + 16 * scale + 3
        cv.text_center(x0 + cell_w // 2, ly, f"{idx}", size=12,
                       color=0x777777)
        # COLORREF 是 0x00BBGGRR（BGR 序）
        if blank:
            label, color = "空", 0xAAAAAA            # 灰
        elif ch is None:
            label, color = "?", 0x0000CC             # 红 = 未识别，重点核对
        else:
            label, color = ch, 0xAA3C00              # 深蓝 = 当前识别字
        cv.text_center(x0 + cell_w // 2, ly + 13, label, size=20,
                       face="SimSun", color=color, bold=True)

    write_png(out, w, h, cv.buf)
    cv.close()
    return w, h


def main(argv=None):
    import argparse
    ap = argparse.ArgumentParser(description="生成字形-识别字对照表 PNG")
    ap.add_argument("--dat", default=DEFAULT_DAT)
    ap.add_argument("--map", default=os.path.join(FD2DATA, "glyphs.json"))
    ap.add_argument("--lo", type=int, default=0)
    ap.add_argument("--hi", type=int, default=0, help="不含；0=到末尾")
    ap.add_argument("--outdir", default=os.path.join(ROOT, ".workbuddy", "atlas"))
    ap.add_argument("--per-page", type=int, default=256)
    ap.add_argument("--scale", type=int, default=4)
    args = ap.parse_args(argv)

    font = load_font(args.dat)
    mapping = json.load(open(args.map, encoding="utf-8"))
    total = len(font) // GLYPH_BYTES
    lo = args.lo
    hi = args.hi or total
    hi = min(hi, total)
    os.makedirs(args.outdir, exist_ok=True)

    per = args.per_page
    cols = 16
    n = 0
    for start in range(lo, hi, per):
        end = min(start + per, hi)
        out = os.path.join(args.outdir, f"atlas_{start:04d}_{end - 1:04d}.png")
        w, h = render_page(font, mapping, start, end, out, cols=cols,
                           scale=args.scale)
        print(f"  {os.path.basename(out)}  {w}x{h}")
        n += 1
    print(f"{n} page(s) -> {args.outdir}")


if __name__ == "__main__":
    main()
