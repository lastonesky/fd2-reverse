#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""fd2_extract.py - 《炎龙骑士团2》统一资源提取器.

从原版数据目录提取：文字/对白、字库、图片、头像、战斗背景/动画、地图、音乐。
纯标准库实现，输出 PNG（索引色 + PLTE）、TXT/JSON、XMI/MIDI，不依赖 Pillow。

用法：
    python3 tools/fd2_extract.py --data /path/to/FD2 --out ./fd2-extracted
    python3 tools/fd2_extract.py --data /path/to/FD2 --out ./out --split
    python3 tools/fd2_extract.py --data /path/to/FD2 --out ./out music

解码依据来自本仓库 src/game/{res,rle,rle2,sprite24,map,map_shape,ani,figure_view}.c
与 host/xmidi.c；只读取游戏数据，不触碰 FD2.SAV。
"""
from __future__ import annotations

import argparse
import json
import math
import os
import struct
import sys
import time
import zlib
from pathlib import Path

MAGIC = b"LLLLLL"
VGA_W, VGA_H = 320, 200


# ---------------------------------------------------------------- 基础 IO --
def u16(b, o):
    return struct.unpack_from("<H", b, o)[0]


def i16(b, o):
    return struct.unpack_from("<h", b, o)[0]


def u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def dac6(v):
    return ((v & 0x3F) << 2) | ((v & 0x3F) >> 4)


def default_palette():
    out = []
    for i in range(256):
        r, g, b = (i >> 5) & 7, (i >> 2) & 7, i & 3
        out.append((dac6(r * 9), dac6(g * 9), dac6(b * 21)))
    return out


def palette_from_6bit(raw):
    if len(raw) < 768:
        return default_palette()
    return [tuple(dac6(raw[3 * i + c]) for c in range(3)) for i in range(256)]


def parse_dat_bytes(data):
    if data[:6] != MAGIC:
        raise ValueError("不是 FD2 LLLLLL 容器")
    header = u32(data, 6)
    n = (header - 10) // 4
    if n < 1 or header + 4 * n > len(data) + 4:
        raise ValueError("DAT 头不合法")
    ends = [u32(data, 10 + 4 * i) for i in range(n)]
    out, prev = [], header
    for e in ends:
        if e < prev or e > len(data):
            out.append(b"")
        else:
            out.append(data[prev:e])
        prev = e
    return out, header, ends


def load_dat(path):
    return parse_dat_bytes(Path(path).read_bytes())


def resource(data_dir, name, index):
    res, _, _ = load_dat(Path(data_dir) / name)
    if index < 0 or index >= len(res):
        raise IndexError(f"{name} 资源 {index} 越界（共 {len(res)} 个）")
    return res[index]


def find_data_dir(explicit):
    if explicit:
        p = Path(explicit).expanduser().resolve()
        if not (p / "FDTXT.DAT").exists():
            raise SystemExit(f"{p} 里没有 FDTXT.DAT")
        return p
    here = Path.cwd().resolve()
    if (here / "FDTXT.DAT").exists():
        return here
    parent = Path(__file__).resolve().parent.parent
    if (parent / "FDTXT.DAT").exists():
        return parent
    raise SystemExit("找不到游戏数据目录，请用 --data 指定（目录里要有 FDTXT.DAT）")


# ------------------------------------------------------------------- PNG --
def png_chunk(tag, data):
    return (struct.pack(">I", len(data)) + tag + data +
            struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))


def write_png(path, w, h, pixels, palette, transparent=None):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    pal = list(palette[:256])
    while len(pal) < 256:
        pal.append((0, 0, 0))
    plte = bytes(bytearray(v for rgb in pal for v in rgb))
    raw = bytearray()
    stride = w
    for y in range(h):
        raw.append(0)
        raw += bytes(pixels[y * stride:(y + 1) * stride])
    chunks = [
        png_chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 3, 0, 0, 0)),
        png_chunk(b"PLTE", plte),
    ]
    if transparent is not None and 0 <= transparent < 256:
        chunks.append(png_chunk(b"tRNS", bytes([0] * transparent + [0])))
    chunks.append(png_chunk(b"IDAT", zlib.compress(bytes(raw), 6)))
    chunks.append(png_chunk(b"IEND", b""))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + b"".join(chunks))


def make_atlas(images, cols=0, scale=1, transparent=None):
    if not images:
        return 0, 0, bytearray()
    cols = cols or max(1, math.ceil(math.sqrt(len(images))))
    cell_w = max(img[0] for img in images)
    cell_h = max(img[1] for img in images)
    rows = math.ceil(len(images) / cols)
    w, h = cols * cell_w * scale, rows * cell_h * scale
    out = bytearray(w * h)
    for i, (iw, ih, pix) in enumerate(images):
        cx, cy = (i % cols) * cell_w * scale, (i // cols) * cell_h * scale
        for y in range(ih):
            src = pix[y * iw:(y + 1) * iw]
            if scale == 1:
                dst = (cy + y) * w + cx
                out[dst:dst + iw] = src
            else:
                line = bytearray()
                for v in src:
                    line.extend(bytes([v]) * scale)
                for sy in range(scale):
                    dst = (cy + y * scale + sy) * w + cx
                    out[dst:dst + len(line)] = line
    return w, h, out


# ------------------------------------------------------------- 解码器 ------
def rle_decode(src, mode=-1, lut=None):
    if len(src) < 4:
        return 0, 0, bytearray()
    w, h = u16(src, 0), u16(src, 2)
    if not (1 <= w <= 4096 and 1 <= h <= 4096):
        return 0, 0, bytearray()
    out, p = bytearray(w * h), 4

    def color(c):
        if lut is not None:
            return lut[c]
        if mode == -1:
            return c & 0xFF
        if 0 <= mode <= 0xFF:
            return mode & 0xFF
        return ((mode & 0xFF) + (((mode >> 8) + c) & 7)) & 0xFF

    pos = 0
    for _ in range(h):
        left = w
        while left > 0 and p < len(src):
            tok, p = src[p], p + 1
            typ, n = tok >> 6, (tok & 63) + 1
            if typ == 0:
                if p >= len(src):
                    break
                c, p = color(src[p]), p + 1
                n = min(n, left)
                out[pos:pos + n] = bytes([c]) * n
                pos += n
                left -= n
            elif typ == 1:
                if p >= len(src):
                    break
                c, p = color(src[p]), p + 1
                for _k in range(n):
                    if pos + 1 >= (pos // w + 1) * w or pos + 1 >= len(out):
                        break
                    out[pos + 1] = c
                    pos += 2
                left -= 2 * n
            elif typ == 2:
                if lut is not None or mode > 0xFF:
                    n = min(n, left, len(src) - p)
                    for k in range(n):
                        out[pos + k] = color(src[p + k])
                    p += n
                else:
                    n = min(n, left, len(src) - p)
                    if 0 <= mode <= 0xFF:
                        out[pos:pos + n] = bytes([mode & 0xFF]) * n
                    else:
                        out[pos:pos + n] = src[p:p + n]
                    p += n
                pos += n
                left -= n
            else:
                n = min(n, left)
                pos += n
                left -= n
        pos += w - left
    return w, h, out


def rle2_decode(src, mirror=False, transparent=False):
    if len(src) < 4:
        return 0, 0, bytearray()
    w, h = u16(src, 0), u16(src, 2)
    if not (1 <= w <= 4096 and 1 <= h <= 4096):
        return 0, 0, bytearray()
    out, p, val, rem = bytearray(w * h), 4, 0, 0

    def nxt():
        nonlocal p, val, rem
        if rem:
            rem -= 1
            return val
        if p >= len(src):
            return 0
        c = src[p]
        p += 1
        if c <= 0xC0:
            rem, val = 0, c
            return c
        rem = c - 0xC1
        if p >= len(src):
            return 0
        val = src[p]
        p += 1
        return val

    for y in range(h):
        base = y * w
        for x in range(w):
            v = nxt()
            if mirror:
                dst = base + (w - 1 - x)
                if dst < len(out):
                    out[dst] = v
            else:
                dst = base + x
                if dst < len(out) and (not transparent or v):
                    out[dst] = v
    return w, h, out


def sprite24_decode(src, kind="plain", base=0, rot=0, lut=None, constant=0):
    out, p = bytearray(24 * 24), 0
    pos = 0

    def color(c):
        if kind == "ramp":
            return (base + ((rot + c) & 7)) & 0xFF
        if kind == "pal":
            return lut[c] if lut is not None else c
        if kind == "const":
            return constant & 0xFF
        if kind == "ramp24":
            return (c & 7) + 24
        return c & 0xFF

    for _row in range(24):
        left = 24
        while left > 0 and p < len(src):
            tok, p = src[p], p + 1
            typ, n = tok >> 6, (tok & 63) + 1
            if typ == 0:
                if p >= len(src):
                    break
                c, p = color(src[p]), p + 1
                n = min(n, left, len(out) - pos)
                out[pos:pos + n] = bytes([c]) * n
                pos += n
                left -= n
            elif typ == 1:
                if p >= len(src):
                    break
                c, p = color(src[p]), p + 1
                for _k in range(n):
                    if pos + 1 >= len(out):
                        break
                    out[pos + 1] = c
                    pos += 2
                left -= 2 * n
            elif typ == 2:
                n = min(n, left, len(src) - p, len(out) - pos)
                for k in range(n):
                    out[pos + k] = color(src[p + k])
                p += n
                pos += n
                left -= n
            else:
                n = min(n, left, len(out) - pos)
                if kind == "pal" and lut is not None:
                    for k in range(n):
                        out[pos + k] = lut[out[pos + k]]
                pos += n
                left -= n
        pos += 24 - left
    return 24, 24, out


def ani_rle_expand(dst, total, cmds, pos):
    out_pos = 0
    while out_pos != total and pos[0] < len(cmds):
        c = cmds[pos[0]]
        pos[0] += 1
        if (c & 0xC0) == 0xC0 and pos[0] < len(cmds):
            n, v = c & 0x3F, cmds[pos[0]]
            pos[0] += 1
        else:
            n, v = 1, c
        n = min(n, total - out_pos)
        dst[out_pos:out_pos + n] = bytes([v]) * n
        out_pos += n


def ani_run_frame(cmds, count, fb, pal):
    pos = [0]

    def rd8():
        v = cmds[pos[0]] if pos[0] < len(cmds) else 0
        pos[0] += 1
        return v

    def rd16():
        return rd8() | (rd8() << 8)

    for _ in range(count):
        op = rd8()
        if op == 0:
            pal[:] = bytes([rd8()]) * 768
        elif op == 1:
            n = min(768, len(cmds) - pos[0])
            pal[:n] = cmds[pos[0]:pos[0] + n]
            pos[0] += 768
        elif op == 2:
            ani_rle_expand(pal, 768, cmds, pos)
        elif op == 3:
            for _b in range(rd8()):
                off, n = 3 * rd8(), 3 * rd8()
                n = min(n, len(cmds) - pos[0], max(0, 768 - off))
                pal[off:off + n] = cmds[pos[0]:pos[0] + n]
                pos[0] += n
        elif op == 4:
            fb[:] = bytes([rd8()]) * len(fb)
        elif op == 5:
            n = min(len(fb), len(cmds) - pos[0])
            fb[:n] = cmds[pos[0]:pos[0] + n]
            pos[0] += len(fb)
        elif op == 6:
            ani_rle_expand(fb, len(fb), cmds, pos)
        elif op == 7:
            for _p in range(rd16()):
                off, v = rd16(), rd8()
                if off < len(fb):
                    fb[off] = v
        elif op == 8:
            for _r in range(rd16()):
                off, n, v = rd16(), rd8(), rd8()
                n = min(n, len(fb) - off) if off < len(fb) else 0
                if n > 0:
                    fb[off:off + n] = bytes([v]) * n
        elif op == 9:
            for _r in range(rd16()):
                off, n = rd16(), rd8()
                n = min(n, len(cmds) - pos[0], len(fb) - off) if off < len(fb) else 0
                if n > 0:
                    fb[off:off + n] = cmds[pos[0]:pos[0] + n]
                pos[0] += n
        else:
            break


# ------------------------------------------------------------- 工具函数 ----
def fonttool():
    tools = str(Path(__file__).resolve().parent)
    if tools not in sys.path:
        sys.path.insert(0, tools)
    import fd2_fonttool as F
    return F


def main_palette(data_dir):
    try:
        raw = resource(data_dir, "FDOTHER.DAT", 0)
    except Exception:
        return default_palette()
    if len(raw) >= 768 and all(v <= 63 for v in raw[:768]):
        return palette_from_6bit(raw[:768])
    return default_palette()


def find_chunk(blob, tag):
    needle = tag if isinstance(tag, bytes) else tag.encode("ascii")
    for i in range(0, len(blob) - 7):
        if blob[i:i + 4] == needle:
            n = struct.unpack_from(">I", blob, i + 4)[0]
            if n and i + 8 + n <= len(blob):
                return blob[i + 8:i + 8 + n]
    return None


def read_vlq(blob, pos):
    v = 0
    for _ in range(4):
        if pos[0] >= len(blob):
            break
        b = blob[pos[0]]
        pos[0] += 1
        v = (v << 7) | (b & 0x7F)
        if not (b & 0x80):
            break
    return v


def data_bytes(status):
    hi = status & 0xF0
    if hi in (0xC0, 0xD0):
        return 1
    if hi in (0x80, 0x90, 0xA0, 0xB0, 0xE0):
        return 2
    return 0


def parse_evnt(ev):
    events = []
    tick, i, tempo = 0, 0, 500000
    notes = 0
    while i < len(ev):
        delta = 0
        while i < len(ev) and ev[i] < 0x80:
            delta += ev[i]
            i += 1
        if i >= len(ev):
            break
        tick += delta
        status = ev[i]

        if status == 0xFF:
            if i + 1 >= len(ev):
                break
            mtype = ev[i + 1]
            i += 2
            vpos = [i]
            mlen = read_vlq(ev, vpos)
            i_pos = vpos[0]
            i = i_pos + mlen
            if i > len(ev):
                break
            data = ev[i_pos:i]
            if mtype == 0x2F:
                break
            if mtype == 0x51 and mlen == 3:
                tempo = (data[0] << 16) | (data[1] << 8) | data[2]
            raw = bytes([0xFF, mtype]) + encode_vlq(mlen) + data
            events.append((tick, raw))
            continue
        if status in (0xF0, 0xF7):
            i += 1
            vpos = [i]
            mlen = read_vlq(ev, vpos)
            i = vpos[0] + mlen
            if i > len(ev):
                break
            continue
        if status & 0x80 == 0:
            i += 1
            continue

        n = data_bytes(status)
        if not n or i + 1 + n > len(ev):
            break
        raw = bytes(ev[i:i + 1 + n])
        i += 1 + n
        events.append((tick, raw))
        if status & 0xF0 == 0x90:
            dur = read_vlq(ev, [i])
            i = read_vlq_pos(ev, i)
            off = bytes([0x80 | (status & 0x0F), raw[1], 0])
            events.append((tick + dur, off))
            notes += 1
    events.sort(key=lambda e: e[0])
    return events, tempo, notes


def encode_vlq(v):
    if v < 0:
        v = 0
    out = [v & 0x7F]
    v >>= 7
    while v:
        out.append(0x80 | (v & 0x7F))
        v >>= 7
    return bytes(reversed(out))


def read_vlq_pos(blob, start):
    pos = [start]
    read_vlq(blob, pos)
    return pos[0]


def write_smf(path, events, tempo):
    ev = list(events)
    last = max((t for t, _ in ev), default=0)
    ev.append((last + 1, b"\xFF\x2F\x00"))
    ev.sort(key=lambda e: e[0])
    track = bytearray()
    prev = 0
    for tick, raw in ev:
        track += encode_vlq(tick - prev) + raw
        prev = tick
    header = b"MThd" + struct.pack(">IHHH", 6, 0, 1, 60)
    body = b"MTrk" + struct.pack(">I", len(track)) + bytes(track)
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_bytes(header + body)


# ------------------------------------------------------------- 文本 --------
def extract_text(data_dir, out, args, stats):
    F = fonttool()
    res, _, _ = F.load_dat(str(data_dir / "FDTXT.DAT"))
    mapping = F.load_mapping()
    root = out / "text"
    root.mkdir(parents=True, exist_ok=True)
    all_parts, count = [], 0
    for ri, stream in enumerate(res):
        if len(stream) < 2:
            continue
        first = i16(stream, 0)
        nsub = first // 2 if first > 0 else 0
        for sub in range(nsub):
            if 2 * sub + 2 > len(stream):
                break
            ops = F.stream(res, ri, sub, keep_ops=True)
            if not ops:
                continue
            txt = F.decode_ops(ops, mapping)
            if not txt.strip():
                continue
            count += 1
            (root / f"fdtxt_res{ri:03d}_sub{sub:03d}.txt").write_text(
                txt + "\n", encoding="utf-8")
            all_parts.append(f"### res{ri} sub{sub}\n{txt}")
    (root / "fdtxt.txt").write_text("\n\n".join(all_parts) + "\n", encoding="utf-8")
    (root / "glyphs.json").write_text(
        json.dumps(mapping, ensure_ascii=False, indent=1), encoding="utf-8")
    stats["text_streams"] = count
    return {"streams": count, "glyphs": len(mapping)}


def extract_font(data_dir, out, args, stats):
    F = fonttool()
    res, _, _ = F.load_dat(str(data_dir / "FDOTHER.DAT"))
    if len(res) <= 4:
        return {}
    font = res[4]
    n = len(font) // 32
    cols, scale = 32, max(1, args.scale)
    cell = 16 * scale
    rows = math.ceil(n / cols)
    w, h = cols * cell, rows * cell
    pix = bytearray(w * h)
    for g in range(n):
        bits = F.glyph_bytes_to_rows(font[g * 32:(g + 1) * 32])
        cx, cy = (g % cols) * cell, (g // cols) * cell
        for y in range(16):
            for x in range(16):
                if not bits[y][x]:
                    continue
                for sy in range(scale):
                    for sx in range(scale):
                        pix[(cy + y * scale + sy) * w + cx + x * scale + sx] = 1
    path = out / "images" / "font" / "glyph_atlas.png"
    write_png(path, w, h, pix, [(0, 0, 0), (255, 255, 255)])
    stats["font_glyphs"] = n
    return {"glyphs": n, "png": str(path.relative_to(out))}


# ------------------------------------------------------------- 头像 --------
def extract_portraits(data_dir, out, args, stats):
    b = (data_dir / "FDICON.B24").read_bytes()
    root = out / "portraits"
    root.mkdir(parents=True, exist_ok=True)
    pal = main_palette(data_dir)
    n = (len(b) - 6) // 48
    count = 0
    for idx in range(n):
        base = 6 + 48 * idx
        if base + 52 > len(b):
            break
        tbl = [u32(b, base + 4 * k) for k in range(13)]
        if not (0 < tbl[0] < tbl[12] <= len(b)):
            continue
        imgs = []
        for k in range(12):
            off = tbl[k]
            if off < tbl[0] or off >= tbl[12]:
                imgs.append((24, 24, bytearray(24 * 24)))
                continue
            imgs.append(sprite24_decode(b[off:tbl[12]]))
        w, h, pix = make_atlas(imgs, cols=4, scale=max(1, args.scale))
        write_png(root / f"unit_{idx:03d}_atlas.png", w, h, pix, pal, transparent=0)
        if args.split:
            for k, img in enumerate(imgs):
                write_png(root / f"unit_{idx:03d}_frame{k:02d}.png",
                          24, 24, img[2], pal, transparent=0)
        count += 1
    stats["portraits"] = count
    return {"units": count}


# ---------------------------------------------------------- 战斗资源 -------
def figani_subimages(blob):
    if len(blob) < 12:
        return []
    count = max(blob[0], blob[2])
    out = []
    for i in range(count):
        off = u32(blob, 8 + 4 * i) if 8 + 4 * i + 4 <= len(blob) else 0
        if off + 9 > len(blob):
            continue
        x, y = u16(blob, off), u16(blob, off + 2)
        meta = list(blob[off + 4:off + 9])
        w, h, pix = rle_decode(blob[off + 9:])
        if not w or not h:
            continue
        out.append({"index": i, "x": x, "y": y, "posture": meta[0],
                    "sfx": meta[1], "frames": meta[2], "w": w, "h": h,
                    "pixels": pix})
    return out


def extract_battle(data_dir, out, args, stats):
    pal = main_palette(data_dir)
    root = out / "battle"
    bgs, bgd = root / "backgrounds", root / "figures"
    portraits = root / "portraits"
    for p in (bgs, bgd, portraits):
        p.mkdir(parents=True, exist_ok=True)

    bg_count = 0
    for i, r in enumerate(load_dat(data_dir / "BG.DAT")[0]):
        if len(r) < 4:
            continue
        w, h, pix = rle_decode(r)
        if w and h:
            write_png(bgs / f"bg_{i:03d}.png", w, h, pix, pal)
            bg_count += 1

    tai_count = 0
    for i, r in enumerate(load_dat(data_dir / "TAI.DAT")[0]):
        if len(r) < 4:
            continue
        w, h, pix = rle_decode(r)
        if w and h:
            write_png(portraits / f"tai_{i:03d}.png", w, h, pix, pal)
            tai_count += 1

    fig_count = 0
    for i, r in enumerate(load_dat(data_dir / "FIGANI.DAT")[0]):
        subs = figani_subimages(r)
        if not subs:
            continue
        imgs = [(s["w"], s["h"], s["pixels"]) for s in subs]
        w, h, atlas = make_atlas(imgs, scale=max(1, args.scale))
        write_png(bgd / f"fig_{i:03d}_atlas.png", w, h, atlas, pal, transparent=0)
        meta = [{k: v for k, v in s.items() if k != "pixels"} for s in subs]
        (bgd / f"fig_{i:03d}.json").write_text(
            json.dumps(meta, ensure_ascii=False, indent=1), encoding="utf-8")
        if args.split:
            for s in subs:
                write_png(bgd / f"fig_{i:03d}_sub{s['index']:02d}.png",
                          s["w"], s["h"], s["pixels"], pal, transparent=0)
        fig_count += 1
    stats["battle_backgrounds"] = bg_count
    stats["battle_figures"] = fig_count
    stats["battle_portraits"] = tai_count
    return {"backgrounds": bg_count, "figures": fig_count, "tai": tai_count}


# ----------------------------------------------------------------- 地图 ----
def shape_lib_decode(blob):
    if len(blob) < 6:
        return []
    count = u16(blob, 4)
    out = []
    for i in range(count):
        off = u32(blob, 6 + 4 * i) if 6 + 4 * i + 4 <= len(blob) else 0
        if off + 1 > len(blob):
            out.append((24, 24, bytearray(24 * 24)))
        else:
            out.append(sprite24_decode(blob[off:]))
    return out


def event_table(hdr):
    out = []
    for f in range(32):
        o = 0x53 + 3 * f
        if o + 3 > len(hdr):
            break
        kind, value = hdr[o], hdr[o + 1] | (hdr[o + 2] << 8)
        if kind or value:
            out.append({"flag": f, "kind": kind, "value": value})
    return out


def extract_maps(data_dir, out, args, stats):
    ff = load_dat(data_dir / "FDFIELD.DAT")[0]
    fs = load_dat(data_dir / "FDSHAP.DAT")[0]
    pal = main_palette(data_dir)
    root = out / "maps"
    (root / "tilesets").mkdir(parents=True, exist_ok=True)
    states, cached = 0, {}

    for s in range(len(ff) // 3):
        m, hdr = ff[3 * s], ff[3 * s + 1]
        if len(m) < 4:
            continue
        w, h = u16(m, 0), u16(m, 2)
        if not (1 <= w <= 512 and 1 <= h <= 512):
            continue
        tileset = hdr[0] if hdr else 0
        if tileset not in cached:
            sr = fs[2 * tileset] if 2 * tileset < len(fs) else b""
            cached[tileset] = shape_lib_decode(sr)
            if cached[tileset]:
                imgs = cached[tileset]
                aw, ah, atlas = make_atlas(imgs, cols=16)
                write_png(root / "tilesets" / f"ts_{tileset:02d}.png",
                          aw, ah, atlas, pal, transparent=0)
        shapes = cached[tileset]
        pix = bytearray(w * h * 24 * 24)
        for y in range(h):
            for x in range(w):
                o = 4 + 4 * (x + w * y)
                if o + 4 > len(m):
                    continue
                tile = u16(m, o) & 0x3FF
                if tile >= len(shapes):
                    continue
                sp = shapes[tile][2]
                dx, dy = x * 24, y * 24
                for sy in range(24):
                    dst = (dy + sy) * (w * 24) + dx
                    pix[dst:dst + 24] = sp[sy * 24:(sy + 1) * 24]
        write_png(root / f"state_{s:02d}_map.png", w * 24, h * 24, pix, pal)
        (root / f"state_{s:02d}.json").write_text(json.dumps({
            "state": s, "width": w, "height": h, "tileset": tileset,
            "events": event_table(hdr),
        }, ensure_ascii=False, indent=1), encoding="utf-8")
        states += 1
    stats["maps"] = states
    stats["tilesets"] = len(cached)
    return {"states": states, "tilesets": len(cached)}


# ----------------------------------------------------------------- 图片 ----
def generic_rle_sheet(buf, table_off, meta_len, pal, root, name, args):
    if len(buf) < table_off + 12:
        return False
    first = u32(buf, table_off)
    if first < table_off + 4 or first > len(buf):
        return False
    if (first - table_off) % 4:
        return False
    count = (first - table_off) // 4
    if not (1 <= count <= 1024):
        return False
    offsets = [u32(buf, table_off + 4 * i) for i in range(count)] + [len(buf)]
    if any(offsets[i] > offsets[i + 1] for i in range(count)):
        return False
    imgs = []
    for i in range(count):
        off, end = offsets[i], offsets[i + 1]
        src = buf[off + meta_len:end]
        if len(src) < 4:
            continue
        w, h, pix = rle_decode(src)
        if not w or not h:
            return False
        imgs.append((w, h, pix))
    if not imgs:
        return False
    w, h, atlas = make_atlas(imgs, scale=max(1, args.scale))
    write_png(root / f"{name}_atlas.png", w, h, atlas, pal, transparent=0)
    if args.split:
        for i, img in enumerate(imgs):
            write_png(root / f"{name}_sub{i:02d}.png", img[0], img[1], img[2], pal)
    return True


def dato_subimages(buf):
    for table_off in (0, 6):
        if len(buf) < table_off + 8:
            continue
        first = u32(buf, table_off)
        count = (first - table_off) // 4
        if not (1 <= count <= 256) or first > len(buf):
            continue
        offsets = [u32(buf, table_off + 4 * i) for i in range(count)] + [len(buf)]
        if any(o > len(buf) for o in offsets):
            continue
        imgs = []
        for i in range(count):
            if offsets[i] + 4 > len(buf):
                break
            w, h, pix = rle2_decode(buf[offsets[i]:offsets[i + 1]])
            if not w or not h:
                break
            imgs.append((w, h, pix))
        if len(imgs) == count:
            return imgs
    return []


def lmi1_subimages(buf):
    if len(buf) < 10 or buf[:4] != b"LMI1":
        return []
    count = u16(buf, 4)
    if not (1 <= count <= 512):
        return []
    offsets = [u32(buf, 6 + 4 * i) for i in range(count)] + [len(buf)]
    if any(o > len(buf) for o in offsets):
        return []
    imgs = []
    for i in range(count):
        w, h, pix = rle_decode(buf[offsets[i]:offsets[i + 1]])
        if not w or not h:
            break
        imgs.append((w, h, pix))
    return imgs


def extract_ani(data_dir, out, args, pal):
    root = out / "images" / "ani"
    root.mkdir(parents=True, exist_ok=True)
    count = 0
    for i, r in enumerate(load_dat(data_dir / "ANI.DAT")[0]):
        if len(r) < 0xAD + 8:
            continue
        frames = i16(r, 0xA5)
        if not (1 <= frames <= 1000):
            continue
        p = 0xAD
        fb, ani_pal = bytearray(VGA_W * VGA_H), bytearray(768)
        for k in range(frames):
            if p + 8 > len(r):
                break
            size, ncmd = u16(r, p), i16(r, p + 2)
            p += 8
            if p + size > len(r):
                break
            ani_run_frame(r[p:p + size], ncmd, fb, ani_pal)
            p += size
            framepal = [tuple(dac6(ani_pal[3 * c + j]) for j in range(3))
                        for c in range(256)]
            sub = root / f"ani_{i:02d}"
            write_png(sub / f"frame_{k:03d}.png", VGA_W, VGA_H, fb, framepal)
        count += 1
    return count


def extract_images(data_dir, out, args, stats):
    pal = main_palette(data_dir)
    root = out / "images" / "fdother"
    root.mkdir(parents=True, exist_ok=True)
    info = extract_font(data_dir, out, args, stats)
    res = load_dat(data_dir / "FDOTHER.DAT")[0]
    sheet_count = 0

    for i, r in enumerate(res):
        name = f"res{i:03d}"
        if len(r) == 768 and all(v <= 63 for v in r):
            write_png(root / f"{name}_palette.png", 256, 1,
                      bytes(range(256)), palette_from_6bit(r))
            continue
        if r[:4] == b"LMI1":
            imgs = lmi1_subimages(r)
            if imgs:
                w, h, atlas = make_atlas(imgs, scale=max(1, args.scale))
                write_png(root / f"{name}_lmi1_atlas.png", w, h, atlas, pal,
                          transparent=0)
                sheet_count += 1
            continue
        if r[:6] == MAGIC:
            try:
                nested, _, _ = parse_dat_bytes(r)
            except ValueError:
                nested = []
            for j, nr in enumerate(nested):
                for table_off, meta in ((8, 9), (6, 0), (0, 0)):
                    if generic_rle_sheet(nr, table_off, meta, pal, root,
                                         f"{name}_sub{j:02d}_{table_off}", args):
                        sheet_count += 1
                        break
            continue
        for table_off, meta in ((8, 9), (6, 0), (0, 0)):
            if generic_rle_sheet(r, table_off, meta, pal, root, name, args):
                sheet_count += 1
                break
        else:
            if args.raw and len(r) > 16:
                (root / f"{name}.bin").write_bytes(r)

    ui = out / "images" / "ui"
    ui.mkdir(parents=True, exist_ok=True)
    dato_count = 0
    for i, r in enumerate(load_dat(data_dir / "DATO.DAT")[0]):
        imgs = dato_subimages(r)
        if not imgs:
            continue
        w, h, atlas = make_atlas(imgs, scale=max(1, args.scale))
        write_png(ui / f"dato_{i:03d}_atlas.png", w, h, atlas, pal, transparent=0)
        (ui / f"dato_{i:03d}.json").write_text(json.dumps([
            {"w": im[0], "h": im[1]} for im in imgs
        ], ensure_ascii=False), encoding="utf-8")
        if args.split:
            for k, im in enumerate(imgs):
                write_png(ui / f"dato_{i:03d}_sub{k:02d}.png", im[0], im[1], im[2], pal)
        dato_count += 1

    for extra in ("TITLE.DAT", "FDOTHER.trad.DAT"):
        p = data_dir / extra
        if not p.exists():
            continue
        try:
            subres = load_dat(p)[0]
        except ValueError:
            continue
        for i, r in enumerate(subres):
            for table_off, meta in ((8, 9), (6, 0), (0, 0)):
                if generic_rle_sheet(r, table_off, meta, pal, out / "images" / extra[:-4],
                                     f"res{i:03d}_{table_off}", args):
                    sheet_count += 1
                    break

    ani_count = extract_ani(data_dir, out, args, pal)
    stats["image_sheets"] = sheet_count
    stats["dato_sheets"] = dato_count
    stats["ani_sequences"] = ani_count
    info.update({"fdother_sheets": sheet_count, "dato_sheets": dato_count,
                 "ani_sequences": ani_count})
    return info


# ----------------------------------------------------------------- 音乐 ----
def extract_timbres(data_dir, out, stats):
    src = data_dir / "SAMPLE.OPL"
    if not src.exists():
        src = data_dir / "SAMPLE.AD"
    if not src.exists():
        return {}
    b = src.read_bytes()
    entries = []
    i = 0
    while i + 6 <= len(b):
        patch, bank, off = b[i], b[i + 1], u32(b, i + 2)
        if patch == 0xFF and bank == 0xFF:
            break
        if off + 16 <= len(b):
            size = u16(b, off)
            trans = b[off + 2] - 256 if b[off + 2] & 0x80 else b[off + 2]
            entries.append({
                "patch": patch, "bank": bank, "size": size, "transposition": trans,
                "regs": list(b[off + 3:off + 16]),
            })
        i += 6
    root = out / "music"
    root.mkdir(parents=True, exist_ok=True)
    (root / "sample_opl.json").write_text(json.dumps({
        "source": src.name, "count": len(entries), "instruments": entries,
    }, ensure_ascii=False, indent=1), encoding="utf-8")
    stats["timbres"] = len(entries)
    return {"timbres": len(entries)}


def extract_music(data_dir, out, args, stats):
    root = out / "music"
    root.mkdir(parents=True, exist_ok=True)
    res = load_dat(data_dir / "FDMUS.DAT")[0]
    manifest, mid_count = [], 0
    for i, blob in enumerate(res):
        item = {"track": i, "size": len(blob)}
        if len(blob) >= 12 and blob[:4] in (b"FORM", b"CAT "):
            (root / f"track_{i:02d}.xmi").write_bytes(blob)
            ev = find_chunk(blob, "EVNT")
            if ev:
                events, tempo, notes = parse_evnt(ev)
                write_smf(root / f"track_{i:02d}.mid", events, tempo)
                mid_count += 1
                item.update({
                    "xmi": f"track_{i:02d}.xmi", "midi": f"track_{i:02d}.mid",
                    "events": len(events), "note_ons": notes, "tempo_us": tempo,
                    "last_tick": max((t for t, _ in events), default=0),
                })
            else:
                item["note"] = "IFF 容器内无 EVNT，仅导出原始 XMI"
        else:
            (root / f"track_{i:02d}.bin").write_bytes(blob)
            item["note"] = "非 XMI 序列（占位/文本），原始字节已保留"
        manifest.append(item)
    (root / "manifest.json").write_text(json.dumps({
        "source": "FDMUS.DAT", "tracks": len(manifest), "midi": mid_count,
        "items": manifest,
    }, ensure_ascii=False, indent=1), encoding="utf-8")
    info = extract_timbres(data_dir, out, stats)
    stats["music_tracks"] = len(manifest)
    stats["midi_files"] = mid_count
    info.update({"tracks": len(manifest), "midi": mid_count})
    return info


# ------------------------------------------------------------------ CLI ----
def run_all(data_dir, out, args, stats):
    results = {}
    for name, fn in (
        ("text", extract_text), ("font", extract_font),
        ("portraits", extract_portraits), ("battle", extract_battle),
        ("maps", extract_maps), ("images", extract_images),
        ("music", extract_music),
    ):
        t0 = time.time()
        try:
            results[name] = fn(data_dir, out, args, stats)
        except Exception as exc:
            results[name] = {"error": f"{type(exc).__name__}: {exc}"}
            print(f"[{name}] 失败: {exc}", file=sys.stderr)
        print(f"[{name}] {time.time() - t0:.1f}s")
    return results


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="FD2 资源提取：文字/图片/头像/战斗背景/地图/音乐")
    ap.add_argument("--data", default=None, help="游戏数据目录（含 FDTXT.DAT）")
    ap.add_argument("--out", default="fd2-extracted", help="输出目录")
    ap.add_argument("--scale", type=int, default=2, help="图集放大倍数（默认 2）")
    ap.add_argument("--split", action="store_true", help="额外逐帧导出精灵")
    ap.add_argument("--raw", action="store_true", help="未识别资源保留 .bin")
    sub = ap.add_subparsers(dest="cmd")
    for name, help_text in (
        ("all", "全部资源（默认）"), ("text", "文字/对白"), ("font", "字库图谱"),
        ("images", "通用图片/UI/动画"), ("portraits", "角色头像"),
        ("battle", "战斗背景与动画"), ("maps", "地图与 tile"),
        ("music", "音乐 XMI/MIDI 与音色库"),
    ):
        sub.add_parser(name, help=help_text)
    args = ap.parse_args(argv)
    args.cmd = args.cmd or "all"

    data_dir = find_data_dir(args.data)
    out = Path(args.out).expanduser().resolve()
    if out == data_dir:
        raise SystemExit("--out 不能等于游戏数据目录")
    out.mkdir(parents=True, exist_ok=True)
    print(f"data: {data_dir}\nout:  {out}\n")

    funcs = {
        "text": extract_text, "font": extract_font, "images": extract_images,
        "portraits": extract_portraits, "battle": extract_battle,
        "maps": extract_maps, "music": extract_music,
    }
    stats = {}
    t0 = time.time()
    results = run_all(data_dir, out, args, stats) if args.cmd == "all" else {
        args.cmd: funcs[args.cmd](data_dir, out, args, stats)
    }
    manifest = {
        "generated": time.strftime("%Y-%m-%d %H:%M:%S"),
        "data_dir": str(data_dir), "command": args.cmd,
        "elapsed_sec": round(time.time() - t0, 2),
        "stats": stats, "results": results,
        "note": "地图为静态 base tile 渲染；运行期调色板换色/闪色未应用。"
                "音乐导出 XMI 与转换后的 SMF（音色仍由 OPL3 决定）。",
    }
    (out / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=1), encoding="utf-8")
    print(f"\n完成：{time.time() - t0:.1f}s -> {out / 'manifest.json'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
