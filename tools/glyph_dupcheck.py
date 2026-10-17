#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""glyph_dupcheck.py - 重复映射审计：找出「同一个字映射到多个字形号」并给出文本语境。

为什么要这个工具
----------------
glyphs.json 是早期按图谱肉眼认字写成的，**同一个字被认到两个不同字形号**几乎
必然是其中一处认错。2026-10-17 的批量清洗就是从这条线索切进去的：

  * 643 认成「愛」，但全文 36 处语境是「受不了 / 受傷 / 接受」→ 实为「受」；
    真正的「愛」在 927（位图带心底）。
  * 607 认成「起」，全文语境是「趕快」→ 实为「趕」。
  * 1337 认成「學」，语境「一舉殲滅 / 大舉進攻 / 不識抬舉」→ 实为「舉」。

同时该工具会比对两处位图是否完全相同：
  * 相同   ⇒ 字库确实画了重复字形，映射重复绝对正常。
  * 不同   ⇒ **嫌疑**，需要按语境 + 原图判定。注意本字库是分区域手绘的，
    同一个字在不同区域会有不同变体（实测 2026-10-17 清洗后剩下的 13 组
    重复字位图全部不同，但语境都成立，属于合法重复）。

用法
----
    python tools/glyph_dupcheck.py                 # 全部重复字 + 语境
    python tools/glyph_dupcheck.py --only-diff     # 只看位图不同的（错认嫌疑）
    python tools/glyph_dupcheck.py --context 8     # 每个字形号最多列 N 条语境
    python tools/glyph_dupcheck.py --dat D:/game/FDTXT.DAT
"""
from __future__ import annotations

import argparse
import json
import os
import struct
import sys
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import fd2_fonttool as F          # noqa: E402  (load_dat / stream / load_mapping)
import glyph_view as GV           # noqa: E402  (load_font / glyph_bitmap)

FD2DATA = os.path.join(HERE, "fd2data")
DEFAULT_DAT = "E:/FD2/FDTXT.DAT"


def bitmap(font, idx):
    return tuple(tuple(r) for r in GV.glyph_bitmap(font, idx))


def collect_contexts(dat, mapping, wanted, reach):
    """{(res, sub): [glyph...]} → 每个目标字形号的上下文片段。"""
    ctx = defaultdict(list)
    for ri, S in enumerate(dat):
        if len(S) < 2:
            continue
        first = struct.unpack_from("<h", S, 0)[0]
        for sub in range(first // 2 if first > 0 else 0):
            ops = F.stream(dat, ri, sub, keep_ops=True)
            if not ops:
                continue
            for i, v in enumerate(ops):
                if v not in wanted:
                    continue
                pre = "".join(mapping.get(x, "?") if x >= 0 else "|"
                              for x in ops[max(0, i - reach):i])
                post = "".join(mapping.get(x, "?") if x >= 0 else "|"
                               for x in ops[i + 1:i + 1 + reach])
                ctx[v].append(f"{pre}[{v}]{post}   ({ri}.{sub})")
    return ctx


def main(argv=None):
    ap = argparse.ArgumentParser(description="重复映射审计（同字多字形号）")
    ap.add_argument("--dat", default=DEFAULT_DAT, help="FDTXT.DAT 路径")
    ap.add_argument("--map", default=os.path.join(FD2DATA, "glyphs.json"))
    ap.add_argument("--font", default=GV.FDOTHER, help="FDOTHER.DAT 路径")
    ap.add_argument("--context", type=int, default=4, help="每字形号列几条语境")
    ap.add_argument("--reach", type=int, default=6, help="语境左右各取几个字")
    ap.add_argument("--only-diff", action="store_true", help="只列位图不同的")
    args = ap.parse_args(argv)

    mapping = {int(k): v for k, v in json.load(open(args.map, encoding="utf-8")).items()}
    font = GV.load_font(args.font)
    dat, _, _ = F.load_dat(args.dat)

    groups = defaultdict(list)
    for idx, ch in mapping.items():
        groups[ch].append(idx)

    dups = {c: sorted(i) for c, i in groups.items() if len(i) > 1}
    wanted = {i for ids in dups.values() for i in ids}
    ctx = collect_contexts(dat, mapping, wanted, args.reach)

    n_same = n_diff = 0
    for ch, ids in sorted(dups.items(), key=lambda x: x[1]):
        bms = {i: bitmap(font, i) for i in ids}
        distinct = len({b for b in bms.values()})
        if distinct == 1:
            n_same += 1
            if args.only_diff:
                continue
            tag = "位图相同(合法重复)"
        else:
            n_diff += 1
            tag = f"位图不同({distinct} 种) ← 需按语境确认"
        print(f"==== {ch!r} {ids}  {tag}")
        for i in ids:
            hits = ctx.get(i, [])
            if not hits:
                print(f"   {i:>4}  (文本中未出现)")
            for line in hits[:args.context]:
                print(f"   {i:>4}  {line}")

    print(f"\n重复字 {len(dups)} 个：位图相同 {n_same}，位图不同 {n_diff}"
          f"（共涉及 {len(wanted)} 个字形号）")
    print("判定方法：位图不同 → 看文本语境里的固定词，再回 glyph_view.py 原图确认；")
    print("          改完务必重跑 tools/fd2data/_rebuild.py。")


if __name__ == "__main__":
    main()
