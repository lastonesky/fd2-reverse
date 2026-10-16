#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""fd2secrets.py - 炎龙骑士团2 (FD2) 隐藏事件扫描器.

数据来源全部是本仓库还原源码里的布局（不是猜的）：

  FDFIELD.DAT / FDSHAP.DAT 是 "LLLLLL" 容器（见 FD2_analysis.md）：
    +0x00  6 字节魔数 "LLLLLL"
    +0x06  u32  header_size (= 头部总字节数)
    +0x0A  u32[N] 偏移表, N = (header_size-10)/4
       注意：FD2 的偏移表存的是每个资源的**结束偏移**（END offset）。
       资源 i 的数据区间 = [ (i==0 ? header_size : offs[i-1]), offs[i] ).

  scenario_load (0x10010) 为状态 s 载入：
      FDFIELD res(3s+0) -> dword_53A51  地图（W,H + W*H 个 4 字节格）
      FDFIELD res(3s+1) -> dword_53A55  状态头（事件表；存档里的同一段）
      FDFIELD res(3s+2) -> dword_53A59  单位/记录摆放
      FDSHAP  res(2*tileset+0)          精灵库（dword_53A5D）
      FDSHAP  res(2*tileset+1)          每 tile 4 字节信息表（dword_53A69）

  地图格 (x,y) 在 map + 4 + 4*(x + W*y)，4 字节：
      [0..1] u16 tile & 0x3FF        （见 map_cell_info 0x12E38）
      [2]    flags = & 0x1F          （5 bit，索引事件表）
      [3]    sprite/palette 字节（0xFF = 无；util_fix_records 0x4DF4C）

  事件表：状态头 + 0x53，每 flag 3 字节 = [kind, value_lo, value_hi]
      （map_cell_event 0x190AC / map_turn.c 的 0x91cb）：
        kind 0 -> 给道具（value = 道具 id）
        kind 1 -> 给钱   （value = 金额）
        其它   -> funcs_1199C[value] 场景脚本
  一个格要能触发事件，还要 info[4]（= FDSHAP 信息表[4*tile]）的 bit5/bit6 非 0，
  即 `tile_info & 0x60 != 0` —— 这就是地图上"没有宝箱但有个微小亮点"的格。

用法：
    python3 tools/fd2secrets.py [DATA_DIR] [--state N] [--all] [--items] [--save FD2.SAV]
      DATA_DIR   含 FDFIELD.DAT / FDSHAP.DAT 的目录，默认当前目录
      --state N  只看状态 N
      --all      列出所有状态
      --items    只列 kind==0（道具）
      --save     额外读存档，标出已经拿过的 flag（dword_53AD5，存档 +0x30A3）
"""
from __future__ import annotations

import argparse
import os
import struct
import sys

MAGIC = b"LLLLLL"
EVENT_OFF = 0x53          # 状态头里事件表基址
STATE_HDR_TILESET = 0     # 状态头 [0] = tileset id


def load_dat(path: str):
    """解析 "LLLLLL" 容器，返回资源 bytes 列表（偏移表是 END 偏移）。"""
    d = open(path, "rb").read()
    if d[:6] != MAGIC:
        raise SystemExit(f"{path}: not an FD2 DAT (magic {d[:6]!r})")
    header = struct.unpack_from("<I", d, 6)[0]
    n = (header - 10) // 4
    ends = [struct.unpack_from("<I", d, 10 + 4 * i)[0] for i in range(n)]
    res, prev = [], header
    for e in ends:
        res.append(d[prev:e])
        prev = e
    return res


def event_table(hdr):
    """返回 [(flag, kind, value), ...]，只保留非空项。"""
    out = []
    for f in range(32):
        o = EVENT_OFF + 3 * f
        if o + 3 > len(hdr):
            break
        kind = hdr[o]
        value = hdr[o + 1] | (hdr[o + 2] << 8)
        if kind or value:
            out.append((f, kind, value))
    return out


def map_cells(m):
    """返回 (W, H, cells[list of (x,y,tile,flag,state)])。"""
    if len(m) < 4:
        return 0, 0, []
    w, h = struct.unpack_from("<H", m, 0)[0], struct.unpack_from("<H", m, 2)[0]
    if not (1 <= w <= 512 and 1 <= h <= 512):
        return 0, 0, []
    cells = []
    for y in range(h):
        for x in range(w):
            o = 4 + 4 * (x + w * y)
            if o + 4 > len(m):
                return w, h, cells
            tile = struct.unpack_from("<H", m, o)[0] & 0x03FF
            flag = m[o + 2] & 0x1F
            state = m[o + 3]
            cells.append((x, y, tile, flag, state))
    return w, h, cells


def tile_info(fshap, tileset):
    idx = 2 * tileset + 1
    return fshap[idx] if 0 <= idx < len(fshap) else b""


def scan_state(ff, fs, s):
    mi, hi = 3 * s, 3 * s + 1
    if hi >= len(ff):
        return None
    m, hdr = ff[mi], ff[hi]
    w, h, cells = map_cells(m)
    if hdr is None or len(hdr) < EVENT_OFF:
        return None
    tileset = hdr[STATE_HDR_TILESET]
    ti = tile_info(fs, tileset)
    events = event_table(hdr)
    by_flag = {}
    for c in cells:
        if c[3] in [e[0] for e in events]:
            by_flag.setdefault(c[3], []).append(c)
    rows = []
    for flag, kind, value in events:
        allc = by_flag.get(flag, [])
        special = [c for c in allc
                   if 4 * c[2] < len(ti) and (ti[4 * c[2]] & 0x60)]
        rows.append({
            "state": s, "flag": flag, "kind": kind, "value": value,
            "tileset": tileset, "cells": special, "ncells": len(allc),
        })
    return {"state": s, "w": w, "h": h, "tileset": tileset,
            "events": rows, "hdr_size": len(hdr)}


def load_save_flags(path):
    """读 FD2.SAV 的已处理 flag 位（dword_53AD5，存档 +0x30A3，0x20 字节）。"""
    try:
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import fd2sav as F
    except Exception:
        return None
    buf = F.load(path)
    return bytes(buf[0x30A3:0x30A3 + 0x20])


def main(argv=None):
    ap = argparse.ArgumentParser(description="FD2 隐藏事件/道具扫描器")
    ap.add_argument("data", nargs="?", default=".", help="含 FDFIELD.DAT 的目录")
    ap.add_argument("--state", type=int, default=None)
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--items", action="store_true", help="只列 kind==0 的道具事件")
    ap.add_argument("--save", default=None, help="FD2.SAV，标出已拿过的 flag")
    args = ap.parse_args(argv)

    ff = load_dat(os.path.join(args.data, "FDFIELD.DAT"))
    fs = load_dat(os.path.join(args.data, "FDSHAP.DAT"))
    flags = load_save_flags(args.save) if args.save else None

    states = range(len(ff) // 3) if (args.all or args.state is None) \
        else [args.state]

    for s in states:
        info = scan_state(ff, fs, s)
        if info is None:
            continue
        rows = info["events"]
        if args.items:
            rows = [r for r in rows if r["kind"] == 0 and r["value"]]
        if not rows:
            continue
        print(f"\n=== state {s}: map {info['w']}x{info['h']} "
              f"tileset {info['tileset']} hdr {info['hdr_size']}B ===")
        for r in rows:
            got = ""
            if flags is not None and r["flag"] < len(flags):
                got = " [已拿]" if flags[r["flag"]] else " [未拿]"
            kname = {0: "item", 1: "money"}.get(r["kind"], f"script/{r['kind']}")
            print(f"  flag {r['flag']:2d}: {kname:8s} value={r['value']}"
                  f"  special cells={len(r['cells'])}/{r['ncells']}{got}")
            for (x, y, tile, fl, st) in r["cells"][:12]:
                print(f"       ({x:3d},{y:3d}) tile={tile} cellstate={st:#04x}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
