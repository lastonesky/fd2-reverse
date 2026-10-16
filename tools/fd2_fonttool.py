#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""fd2_fonttool.py - FD2 (炎龙骑士团2) 字形反查 / 繁->简字形重生成.

背景（全部来自本仓库还原源码 + 对 FDOTHER.DAT 的解析）：

  * 游戏文本存在 FDTXT.DAT：每个资源开头是一张 u16 偏移表，
    `vm_run` (src/game/vm.c) 取 `p = s + *(int16*)(s + 2*sub)`，
    然后按 int16 读字形号（>=0 画字），负数是 opcode（-1 结束）。
  * 字形位图在 FDOTHER.DAT 资源 4（boot.c: dword_53A75 = res_load(FDOTHER,4)），
    每个字形 32 字节 = 16x16 1bpp（gfx_draw_glyph 0x4ED7A: table + index*32）。
  * 所以文本里每个 u16 就是字形号；要替换繁/简，**只要把对应字形号的位图换成
    简体字形**，文本流不用动。

  FD2 的字形是自绘的 16x16 点阵，通用 OCR（tesseract/RapidOCR）和自动模板匹配
  都不可靠；本仓库的做法是用 `--sheet` 导出带编号的字形图谱，人工/视觉逐一认字，
  把映射写进 fd2data/glyphs.json（{字形号: 汉字}）。fd2data/glyph_map.seed.json
  是从道具名反推出来的 202 个字（零冲突），可直接当种子。

用法（需要 Pillow；转换还需要 opencc 命令行，Arch: `pacman -S opencc`）：
    python3 tools/fd2_fonttool.py items
    python3 tools/fd2_fonttool.py sheet --lo 0 --hi 256 --out /tmp/sheet.png
    python3 tools/fd2_fonttool.py decode --text FDTXT.DAT --res 0 --sub 192
    python3 tools/fd2_fonttool.py build-map --exe /path/FDOTHER.DAT --items
    python3 tools/fd2_fonttool.py convert --data /path/to/FD2 --out /tmp/FDOTHER.simp.DAT
"""
from __future__ import annotations

import argparse
import json
import os
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FD2DATA = os.path.join(HERE, "fd2data")
FONT_RES = 4              # FDOTHER.DAT resource holding the 16x16 font
GLYPH_BYTES = 32
GLYPH_W = GLYPH_H = 16


# --------------------------------------------------------------- DAT ------
def load_dat(path):
    """解析 FD2 的 "LLLLLL" 容器（偏移表是 END 偏移）。返回资源 bytes 列表。"""
    d = open(path, "rb").read()
    if d[:6] != b"LLLLLL":
        raise SystemExit(f"{path}: not an FD2 DAT")
    header = struct.unpack_from("<I", d, 6)[0]
    n = (header - 10) // 4
    ends = [struct.unpack_from("<I", d, 10 + 4 * i)[0] for i in range(n)]
    res, prev = [], header
    for e in ends:
        res.append(d[prev:e])
        prev = e
    return res, header, ends


def write_dat(path, resources, header, ends):
    """按原 header/offset 结构重写 DAT（资源大小不变）。"""
    out = bytearray()
    out += b"LLLLLL" + struct.pack("<I", header)
    for e in ends:
        out += struct.pack("<I", e)
    # 资源之间可能有对齐/空隙：用原来的 header 到第一个资源之间的填充字节
    # 这里最简单：资源大小都不变，直接顺序拼接即可（offset 表也原样）。
    for r in resources:
        out += r
    open(path, "wb").write(bytes(out))


# --------------------------------------------------------------- font -----
def glyph_bytes_to_rows(g):
    rows = []
    for r in range(GLYPH_H):
        w = struct.unpack_from("<H", g, r * 2)[0]
        w = ((w >> 8) | (w << 8)) & 0xFFFF
        rows.append([(w >> (15 - c)) & 1 for c in range(GLYPH_W)])
    return rows


def render_glyph(ch, bold=True):
    """把汉字渲染成 FD2 风格的 16x16 1bpp（返回 32 字节）。"""
    from PIL import Image, ImageDraw, ImageFont
    cands = [
        ("/usr/share/fonts/noto-cjk/NotoSansCJK-Bold.ttc", 3),
        ("/usr/share/fonts/noto-cjk/NotoSansCJK-Bold.ttc", 2),   # SC fallback
        ("/usr/share/fonts/wenquanyi/wqy-microhei/wqy-microhei.ttc", 0),
    ]
    for path, idx in cands:
        if not os.path.exists(path):
            continue
        try:
            f = ImageFont.truetype(path, 16, index=idx)
        except Exception:
            continue
        img = Image.new("L", (24, 24), 0)
        ImageDraw.Draw(img).text((4, 2), ch, fill=255, font=f)
        a = __import__("numpy").array(img) > 100
        if a.sum() == 0:
            continue
        ys, xs = __import__("numpy").where(a)
        a = a[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
        h, w = a.shape
        # 保留宽高比，缩放到 13 像素高左右（FD2 字心约 13x13），居中进 16x16
        s = min(13.0 / h, 13.0 / w)
        nh, nw = max(1, round(h * s)), max(1, round(w * s))
        im = Image.fromarray((a * 255).astype("uint8")).resize((nw, nh), Image.LANCZOS)
        a = __import__("numpy").array(im) > 110
        out = bytearray(32)
        y0, x0 = (16 - nh) // 2, (16 - nw) // 2
        for yy in range(nh):
            for xx in range(nw):
                if a[yy, xx]:
                    gx, gy = x0 + xx, y0 + yy
                    if 0 <= gx < 16 and 0 <= gy < 16:
                        out[gy * 2 + (0 if gx < 8 else 1)] |= 1 << (7 - (gx & 7))
        # FD2 stores each row as a byte-swapped u16 little-endian:
        # our out is already row-major MSB-first per byte; swap byte order per row
        for r in range(16):
            out[r * 2], out[r * 2 + 1] = out[r * 2 + 1], out[r * 2]
        return bytes(out)
    raise SystemExit("no CJK font found to render glyph")


# --------------------------------------------------------------- text ------
def load_mapping():
    m = {}
    for name in ("glyph_map.seed.json", "glyphs.json"):
        p = os.path.join(FD2DATA, name)
        if os.path.exists(p):
            for k, v in json.load(open(p)).items():
                m[int(k)] = v
    return m


def stream(dat, residx, sub):
    s = dat[residx]
    off = struct.unpack_from("<h", s, 2 * sub)[0]
    p, out = off, []
    while p + 2 <= len(s):
        v = struct.unpack_from("<h", s, p)[0]
        p += 2
        if v < 0:
            break
        out.append(v)
    return out


def decode(glyphs, mapping):
    return "".join(mapping.get(g, f"[{g}]") for g in glyphs)


# --------------------------------------------------------------- ops -------
def cmd_items(args):
    items = json.load(open(os.path.join(FD2DATA, "items.json")))
    if args.id is not None:
        print(f"{args.id}: {items.get(str(args.id), '?')}")
        return
    for k in sorted(items, key=lambda x: int(x)):
        print(f"{k:>4}  {items[k]}")


def cmd_sheet(args):
    from PIL import Image, ImageDraw
    dat, _, _ = load_dat(args.exe)
    font = dat[FONT_RES]
    n = len(font) // GLYPH_BYTES
    lo, hi = args.lo, min(args.hi, n)
    cols, cell, sc = 16, 40, args.scale
    rows = (hi - lo + cols - 1) // cols
    img = Image.new("L", (cols * cell, rows * (cell + 12)), 255)
    dr = ImageDraw.Draw(img)
    for k in range(hi - lo):
        g = lo + k
        b = font[g * GLYPH_BYTES:(g + 1) * GLYPH_BYTES]
        rows_b = glyph_bytes_to_rows(b)
        o = bytearray(256)
        for y in range(16):
            for x in range(16):
                o[y * 16 + x] = 0 if rows_b[y][x] else 255
        im = Image.frombytes("L", (16, 16), bytes(o)).resize((16 * sc, 16 * sc), Image.NEAREST)
        r, c = divmod(k, cols)
        img.paste(im, (c * cell + 4, r * (cell + 12) + 2))
        dr.text((c * cell + 10, r * (cell + 12) + 35), str(g), fill=0)
    img.save(args.out)
    print(f"saved {args.out} ({img.size}), glyphs {lo}..{hi-1}")


def cmd_decode(args):
    dat, _, _ = load_dat(args.text)
    mapping = load_mapping()
    g = stream(dat, args.res, args.sub)
    print(" ".join(str(x) for x in g))
    print(decode(g, mapping))


def cmd_build_map(args):
    """从道具名（fd2data/items.json）反推字形->字，合并进 glyph_map.seed.json。"""
    dat, _, _ = load_dat(os.path.join(args.data, "FDTXT.DAT"))
    items = json.load(open(os.path.join(FD2DATA, "items.json")))
    votes = {}
    for k, nm in items.items():
        g = stream(dat, 0, 181 + int(k))
        if len(g) == len(nm):
            for gi, ch in zip(g, nm):
                votes.setdefault(gi, {}).setdefault(ch, 0)
                votes[gi][ch] += 1
    gm = {str(g): max(v, key=v.get) for g, v in votes.items()}
    out = os.path.join(FD2DATA, "glyph_map.seed.json")
    json.dump(gm, open(out, "w"), ensure_ascii=False, indent=0)
    print(f"wrote {out}: {len(gm)} glyphs")


def cmd_convert(args):
    """把字体里已映射的字形换成繁体对应的简体字形（opencc t2s）。"""
    dat, header, ends = load_dat(os.path.join(args.data, "FDOTHER.DAT"))
    font = bytearray(dat[FONT_RES])
    ng = len(font) // GLYPH_BYTES
    mapping = load_mapping()
    chars = "".join(mapping[g] for g in sorted(mapping))
    # batch t2s via opencc
    simp = chars
    try:
        p = subprocess.run(["opencc", "-c", "t2s"], input=chars,
                           capture_output=True, text=True)
        if p.returncode == 0:
            simp = p.stdout
    except FileNotFoundError:
        print("warning: opencc not found; using identity (no conversion)", file=sys.stderr)
    simp = (simp + chars)[:len(chars)]
    changed = 0
    for i, g in enumerate(sorted(mapping)):
        c, s = chars[i], simp[i]
        if c != s and s:
            font[g * GLYPH_BYTES:(g + 1) * GLYPH_BYTES] = render_glyph(s)
            changed += 1
    dat[FONT_RES] = bytes(font)
    write_dat(args.out, dat, header, ends)
    print(f"converted {changed} glyph(s) -> {args.out} (font {ng} glyphs, mapped {len(mapping)})")


def main(argv=None):
    ap = argparse.ArgumentParser(description="FD2 字形反查 / 繁->简重生成")
    ap.add_argument("--exe", default="FDOTHER.DAT", help="FDOTHER.DAT 路径")
    sub = ap.add_subparsers(dest="cmd", required=True)

    s = sub.add_parser("items", help="打印已识别的道具名")
    s.add_argument("--id", type=int, default=None)
    s.set_defaults(func=cmd_items)

    s = sub.add_parser("sheet", help="导出带编号的字形图谱 PNG")
    s.add_argument("--lo", type=int, default=0)
    s.add_argument("--hi", type=int, default=256)
    s.add_argument("--scale", type=int, default=2)
    s.add_argument("--out", default="/tmp/sheet.png")
    s.set_defaults(func=cmd_sheet)

    s = sub.add_parser("decode", help="解码 FDTXT 的一个文本流")
    s.add_argument("--text", default="FDTXT.DAT")
    s.add_argument("--res", type=int, default=0)
    s.add_argument("--sub", type=int, default=0)
    s.set_defaults(func=cmd_decode)

    s = sub.add_parser("build-map", help="从道具名反推字形->字（种子映射）")
    s.add_argument("--data", default=".")
    s.set_defaults(func=cmd_build_map)

    s = sub.add_parser("convert", help="生成简体字形版 FDOTHER.DAT")
    s.add_argument("--data", default=".", help="含 FDOTHER.DAT 的目录")
    s.add_argument("--out", default="/tmp/FDOTHER.simp.DAT")
    s.set_defaults(func=cmd_convert)
    args = ap.parse_args(argv)
    args.func(args)
    return 0


if __name__ == "__main__":
    sys.exit(main())
