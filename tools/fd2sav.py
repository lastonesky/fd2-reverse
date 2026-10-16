#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""fd2sav.py - <炎龙骑士团2 / Flame Dragon 2> (FD2.EXE, md5 a6e341a8...) 存档修改器.

Everything here is derived from the hand-restored sources in this repo, not from
guessing at the bytes:

  FD2.SAV layout
  --------------
  22987 bytes (0x59CB), obfuscated with a rolling 16-bit XOR stream
  (util_deobfuscate 0x4DF28) and sealed with a trailing checksum
  (util_sum_tail4 0x4DF09).  Sources:
    src/game/util.c        util_deobfuscate / util_sum_tail4
    src/game/scenario_load.c  (0x10010) reads the "current game" head
    src/game/sys_menu.c    (0x19DF7) writes the head
    src/game/save_menu.c   (0x2968D / 0x2986F / 0x29AB2) the 4-slot menu

  head ("继续游戏" / scenario_load 0x10010)
    0x08A3  u8[0xA00]  party table  (dword_53BF7, 32 records x 80)
    0x12A3  u8[80*N]   roster table (dword_53A45, N = field[1])
    0x30A3  u8[0x20]   flag block   (dword_53AD5)
    0x30C3  u8[18]     scene field  f[0..17]
        f[0] location  f[1] roster count  f[2] scene/state id
        f[9] party count    f[0x0A..0x0D] u32 MONEY (dword_53BF3)

  4 save slots ("读取进度" / save_menu 0x2968D / 0x2986F)
    base_i = 12587 + 2600*i     (12587 = 0x312B; the machine hardcodes the
                                 scenario-id bytes at 0x3B2B/0x4553/0x4F7B/0x59A3)
    base+0x000  u8[0xA00]  party table snapshot (32 records x 80)
    base+0xA00  u8        scene/state id   (0xFF = empty slot)
    base+0xA01  u8        party count
    base+0xA02  u32 LE    MONEY

  80-byte record (src/game/unit.c unit_add 0x112A5, rec.c, status/map_status)
    +0x00 u16   (id pair)
    +0x02 u8    icon index (rebuilt from FDICON.B24)
    +0x05 u8    flag byte (bit0 = rec_flag; |=0x80 used-state)
    +0x06 u8    kind/type byte
    +0x07 u8    DATO portrait resource index
    +0x08 u8    identity byte (party/roster join key)
    +0x0A .. +0x19   8 x [state, item id] slots
    +0x1F u8    class/race (D[0])      +0x20 u8  class/race (D[1])
    +0x21 u8    LEVEL
    +0x26..+0x30  5 flag bytes (rec_collect_slot_bits 0x1C269)
    +0x31 u8    0xFF marker
    +0x37 u16   base AP (signed)   +0x39 u16 base DP   +0x3E u16 base DX
    +0x40 u16   HP current         +0x42 u16 HP max
    +0x44 u16   MP current         +0x46 u16 MP max
    +0x48/+0x4A/+0x4C/+0x4E u16  derived AP/DP/DX/DX+ (unit_recalc 0x1145A;
                the game recomputes these from the equipped items, so an editor
                usually only touches the base values at +0x37/+0x39/+0x3E)

Raw bytes are the authority.  Every write re-stamps the checksum, re-obfuscates
and leaves a timestamped .bak next to the file.
"""
from __future__ import annotations

import argparse
import os
import shutil
import struct
import sys
import time

SAVE_SIZE = 0x59CB          # 22987
SLOT_BASE = 12587           # 0x312B
SLOT_SIZE = 2600            # 0xA28
SLOT_TABLE = 0xA00          # 2560 = 32 * 80
SLOT_HDR = SLOT_TABLE       # 0xA00
REC_STRIDE = 80
SLOT_RECS = SLOT_TABLE // REC_STRIDE        # 32

HEAD_PARTY = 0x08A3
HEAD_ROSTER = 0x12A3
HEAD_XTRA = 0x30A3
HEAD_FIELD = 0x30C3
HEAD_MONEY = HEAD_FIELD + 0x0A              # 0x30CD
ROSTER_MAX_RECS = (HEAD_XTRA - HEAD_ROSTER) // REC_STRIDE   # 96

REGIONS = ["head", "roster", "s0", "s1", "s2", "s3"]

# dseg03 mirror, for the item bonus table (src/game/unit.c tbl_602AD).
#   tbl_602AD(i) = fd2_data2[0x02AD + 23*i]   (tbl_ptr: base + stride*index)
#   entry: +1 s16 AP bonus, +3 s16 DX, +5 s16 DP, +7 s16 DX-of-derived
DSEG03_CANDIDATES = [
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..",
                 "analysis", "seg_dseg03.bin"),
]
ITEM_TBL_OFF = 0x02AD
ITEM_STRIDE = 23


def load_item_bonus_table():
    for p in DSEG03_CANDIDATES:
        p = os.path.normpath(p)
        if os.path.exists(p):
            d = open(p, "rb").read()
            tbl = {}
            for i in range(len(d) // ITEM_STRIDE):
                o = ITEM_TBL_OFF + ITEM_STRIDE * i
                if o + 9 > len(d):
                    break
                tbl[i] = (
                    struct.unpack_from("<h", d, o + 1)[0],
                    struct.unpack_from("<h", d, o + 3)[0],
                    struct.unpack_from("<h", d, o + 5)[0],
                    struct.unpack_from("<h", d, o + 7)[0],
                )
            return tbl
    return None


ITEM_BONUS = load_item_bonus_table()


# --------------------------------------------------------------------------- #
# cipher / checksum  (util.c 0x4DF28 / 0x4DF09)
# --------------------------------------------------------------------------- #
def deobfuscate(buf: bytearray) -> bytearray:
    """state = 0xA5; per byte: state = rol16(state + 0x9014, 3); b ^= state & 0xFF.
    The key stream is independent of the data, so this is its own inverse."""
    state = 0x00A5
    for i in range(len(buf)):
        state = (state + 0x9014) & 0xFFFF
        state = ((state << 3) | (state >> 13)) & 0xFFFF
        buf[i] ^= state & 0xFF
    return buf


def sum_tail4(buf) -> int:
    """Sum of bytes [0 .. n-5] - the last dword is excluded (original `sub ecx,4`)."""
    return sum(buf[: len(buf) - 4])


def stamp(buf: bytearray) -> None:
    struct.pack_into("<I", buf, SAVE_SIZE - 4, sum_tail4(buf) & 0xFFFFFFFF)


def checksum_ok(buf) -> bool:
    return sum_tail4(buf) == struct.unpack_from("<I", buf, SAVE_SIZE - 4)[0]


# --------------------------------------------------------------------------- #
# load / store
# --------------------------------------------------------------------------- #
def load(path: str) -> bytearray:
    raw = bytearray(open(path, "rb").read())
    if len(raw) != SAVE_SIZE:
        raise SystemExit(f"{path}: size {len(raw)} != {SAVE_SIZE} (0x{SAVE_SIZE:X})")
    return deobfuscate(raw)


def store(path: str, buf: bytearray, backup=True, dry=False) -> str | None:
    if len(buf) != SAVE_SIZE:
        raise SystemExit("refusing to write: buffer size changed")
    stamp(buf)
    obf = bytearray(buf)
    deobfuscate(obf)                       # same routine - it is an involution
    bpath = None
    if dry:
        return None
    if backup and os.path.exists(path):
        bpath = f"{path}.bak-{time.strftime('%Y%m%d-%H%M%S')}"
        shutil.copy2(path, bpath)
    with open(path, "wb") as f:
        f.write(obf)
    return bpath


# --------------------------------------------------------------------------- #
# region views
# --------------------------------------------------------------------------- #
class Region:
    """A record table plus, optionally, the region's money cell."""

    def __init__(self, buf, name):
        self.buf = buf
        self.name = name
        if name == "head":
            self.base = HEAD_PARTY
            self.count_off = HEAD_FIELD + 9          # f[9] dword_53BFB
            self.money_off = HEAD_MONEY
            self.max_recs = SLOT_RECS
            self.label = "head (继续游戏 scenario_load 0x10010)"
        elif name == "roster":
            self.base = HEAD_ROSTER
            self.count_off = HEAD_FIELD + 1          # f[1] dword_53BEB
            self.money_off = None
            self.max_recs = ROSTER_MAX_RECS
            self.label = "head roster (dword_53A45)"
        elif name.startswith("s") and name[1:].isdigit() and 0 <= int(name[1:]) < 4:
            i = int(name[1:])
            self.base = SLOT_BASE + SLOT_SIZE * i
            self.count_off = self.base + SLOT_HDR + 1
            self.money_off = self.base + SLOT_HDR + 2
            self.max_recs = SLOT_RECS
            self.label = f"slot {i} (base 0x{self.base:X})"
        else:
            raise SystemExit(f"unknown region '{name}'")

    @property
    def count(self):
        return self.buf[self.count_off]

    @count.setter
    def count(self, v):
        self.buf[self.count_off] = v & 0xFF

    @property
    def money(self):
        if self.money_off is None:
            return None
        return struct.unpack_from("<I", self.buf, self.money_off)[0]

    @money.setter
    def money(self, v):
        if self.money_off is None:
            raise SystemExit(f"region '{self.name}' has no money cell")
        struct.pack_into("<I", self.buf, self.money_off, v & 0xFFFFFFFF)

    def rec(self, i) -> memoryview:
        if not 0 <= i < self.max_recs:
            raise SystemExit(f"record {i} out of range (0..{self.max_recs-1})")
        o = self.base + REC_STRIDE * i
        return memoryview(self.buf)[o:o + REC_STRIDE]

    def empty(self) -> bool:
        """A slot is empty when its scenario id byte is 0xFF."""
        if self.name == "head" or self.name == "roster":
            return False
        return self.buf[self.base + SLOT_HDR] == 0xFF

    def scenario_id(self):
        if self.name == "head":
            return self.buf[HEAD_FIELD + 2]
        if self.name == "roster":
            return None
        return self.buf[self.base + SLOT_HDR]


def regions(buf, names):
    out = []
    for n in names:
        out.append(Region(buf, n))
    return out


def expand_regions(spec: str):
    """`all` | a single region | a comma list, e.g. 'head,roster'.
    'continue' (alias 'cont') = the whole "继续游戏" header: head party + roster.
    'all' is the whole file: head + roster + the four save slots."""
    if spec == "all":
        return list(REGIONS)
    if spec in ("continue", "cont"):
        return ["head", "roster"]
    names = [x.strip() for x in spec.split(",") if x.strip()]
    for n in names:
        if n not in REGIONS:
            raise SystemExit(f"unknown region '{n}' (known: {', '.join(REGIONS)}, all)")
    return names


# --------------------------------------------------------------------------- #
# record field access
# --------------------------------------------------------------------------- #
def u16(rec, off) -> int:
    return rec[off] | (rec[off + 1] << 8)


def set_u16(rec, off, v) -> None:
    rec[off] = v & 0xFF
    rec[off + 1] = (v >> 8) & 0xFF


# field name -> (kind, offset)
FIELDS = {
    "hp":     ("u16", 0x40),
    "maxhp":  ("u16", 0x42),
    "mp":     ("u16", 0x44),
    "maxmp":  ("u16", 0x46),
    "ap":     ("u16", 0x37),
    "dp":     ("u16", 0x39),
    "dx":     ("u16", 0x3E),
    "level":  ("u8",  0x21),
    "kind":   ("u8",  0x06),
    "id":     ("u8",  0x08),
    "portrait": ("u8", 0x07),
    "flag":   ("u8",  0x05),
    "class0": ("u8",  0x1F),
    "class1": ("u8",  0x20),
}
for _i in range(8):
    FIELDS[f"item{_i}"] = ("slot", 0x0A + 2 * _i)     # [state, item id]
    FIELDS[f"equip{_i}"] = ("equip", 0x0A + 2 * _i)   # state bit 6
    FIELDS[f"itemstate{_i}"] = ("u8", 0x0A + 2 * _i)  # raw state byte


def get_field(rec, name):
    kind, off = FIELDS[name]
    if kind == "u16":
        return u16(rec, off)
    if kind == "u8":
        return rec[off]
    if kind == "equip":
        return 1 if rec[off] & 0x40 else 0
    return rec[off + 1]                                # slot item id


def set_field(rec, name, val):
    kind, off = FIELDS[name]
    if kind == "u16":
        set_u16(rec, off, val)
    elif kind == "u8":
        rec[off] = val & 0xFF
    elif kind == "equip":
        rec[off] = (rec[off] | 0x40) if val else (rec[off] & ~0x40)
    else:
        # writing an item id: keep the slot state coherent (0x80 == empty)
        rec[off + 1] = val & 0xFF
        if val in (0xFF, 0):
            rec[off] |= 0x80
        else:
            rec[off] &= 0x7F


def _s16(rec, off) -> int:
    v = rec[off] | (rec[off + 1] << 8)
    return v - 0x10000 if v >= 0x8000 else v


def derived(rec):
    """unit_recalc 0x1145A: base +0x37/+0x39/+0x3E plus every slot whose state
    has bit 6 set (equipped), through the tbl_602AD bonus table."""
    ap = _s16(rec, 0x37)
    dp = _s16(rec, 0x39)
    dx = _s16(rec, 0x3E)
    de = dx
    if ITEM_BONUS:
        for k in range(8):
            if rec[0x0A + 2 * k] & 0x40:
                b = ITEM_BONUS.get(rec[0x0B + 2 * k])
                if b:
                    ap += b[0]
                    dx += b[1]
                    dp += b[2]
                    de += b[3]
    return ap, dp, dx, de


def apply_recalc(rec) -> None:
    ap, dp, dx, de = derived(rec)
    set_u16(rec, 0x48, ap & 0xFFFF)
    set_u16(rec, 0x4A, dp & 0xFFFF)
    set_u16(rec, 0x4C, dx & 0xFFFF)
    set_u16(rec, 0x4E, de & 0xFFFF)


def select_members(reg: Region, sel: str):
    """sel: '#i' index | 'id=<n>' | '*' all populated."""
    if sel == "*":
        n = reg.count
        return list(range(0 if n else 0, max(n, 1)))
    if sel.startswith("id="):
        want = int(sel[3:], 0)
        hits = [i for i in range(reg.max_recs) if reg.rec(i)[8] == (want & 0xFF)]
        if not hits:
            raise SystemExit(f"no member with id {want} in {reg.name}")
        return hits
    if sel.startswith("#"):
        return [int(sel[1:], 0)]
    return [int(sel, 0)]


# --------------------------------------------------------------------------- #
# commands
# --------------------------------------------------------------------------- #
def cmd_info(args):
    buf = load(args.file)
    ok = checksum_ok(buf)
    print(f"file     : {args.file}")
    print(f"size     : {len(buf)} = 0x{len(buf):X}")
    print(f"checksum : {'OK' if ok else 'MISMATCH'}  "
          f"(stored 0x{struct.unpack_from('<I', buf, SAVE_SIZE-4)[0]:08X})")

    f = buf[HEAD_FIELD:HEAD_FIELD + 18]
    print("\n== head (继续游戏) ==")
    print(f"  location #      f[0]      = {f[0]}")
    print(f"  roster count    f[1]      = {f[1]}")
    print(f"  scene/state id  f[2]      = {f[2]}")
    print(f"  party count     f[9]      = {f[9]}")
    print(f"  MONEY           f[A..D]   = {struct.unpack_from('<I', buf, HEAD_MONEY)[0]}")
    print(f"  misc            f[E..11]  = "
          f"{f[0x0E]},{f[0x0F]},{f[0x10]},{f[0x11]}")
    _dump_region(Region(buf, "head"))
    _dump_region(Region(buf, "roster"))

    for i in range(4):
        print()
        _dump_region(Region(buf, f"s{i}"))


def _dump_region(reg: Region):
    tag = "[EMPTY]" if reg.empty() else ""
    sid = reg.scenario_id()
    extra = f" scenario_id={sid}" if sid is not None else ""
    money = reg.money
    print(f"\n-- {reg.label}{extra} {tag}")
    if money is not None:
        print(f"   MONEY = {money} (0x{money:08X})")
    print(f"   count = {reg.count}   (records present below)")
    for i in range(min(reg.count, reg.max_recs)):
        r = reg.rec(i)
        items = " ".join(f"{get_field(r, f'item{k}'):02X}" for k in range(8))
        print(f"   #{i:<2} id={r[8]:<3} kind={r[6]:<3} lv={r[0x21]:<3} "
              f"flag={r[5]:02X} HP={u16(r,0x40):>5}/{u16(r,0x42):<5} "
              f"MP={u16(r,0x44):>5}/{u16(r,0x46):<5} "
              f"AP={u16(r,0x37):>5} DP={u16(r,0x39):>5} DX={u16(r,0x3E):>5} "
              f"port={r[7]:<3}")
        print(f"       items[{items}]  "
              f"slots={[f'{r[0x0A+2*k]:02X}:{r[0x0B+2*k]:02X}' for k in range(8)]}")


def cmd_get(args):
    buf = load(args.file)
    reg = Region(buf, args.region)
    for mi in select_members(reg, args.member):
        r = reg.rec(mi)
        vals = {k: get_field(r, k) for k in (args.fields or
                ["id", "kind", "level", "hp", "maxhp", "mp", "maxmp",
                 "ap", "dp", "dx"] + [f"item{i}" for i in range(8)])}
        print(f"{reg.name} #{mi}: " + "  ".join(f"{k}={v}" for k, v in vals.items()))


def cmd_set(args):
    buf = load(args.file)
    targets = expand_regions(args.region)
    applied = []
    for rname in targets:
        reg = Region(buf, rname)
        if rname != "head" and rname != "roster" and reg.empty():
            print(f"  skip {rname}: empty slot")
            continue
        for mi in select_members(reg, args.member):
            r = reg.rec(mi)
            for kv in args.assign:
                k, _, v = kv.partition("=")
                k = k.strip()
                if k not in FIELDS:
                    raise SystemExit(f"unknown field '{k}' "
                                     f"(known: {', '.join(sorted(FIELDS))})")
                set_field(r, k, int(v, 0))
                applied.append(f"{rname}#{mi}.{k}={int(v,0)}")
            if args.recalc:
                apply_recalc(r)
    b = store(args.file, buf, backup=not args.no_backup, dry=args.dry_run)
    print(f"applied {len(applied)} change(s): " +
          ", ".join(applied[:12]) + (" ..." if len(applied) > 12 else ""))
    print(f"{'dry-run, nothing written' if args.dry_run else 'written'}"
          + (f"  backup: {b}" if b else ""))


def cmd_money(args):
    buf = load(args.file)
    targets = expand_regions(args.region)
    done = []
    for rname in targets:
        reg = Region(buf, rname)
        if reg.money_off is None:
            continue
        if rname not in ("head", "roster") and reg.empty():
            print(f"  skip {rname}: empty slot")
            continue
        if args.add:
            reg.money = reg.money + args.amount
        else:
            reg.money = args.amount
        done.append(f"{rname}={reg.money}")
    b = store(args.file, buf, backup=not args.no_backup, dry=args.dry_run)
    print("money: " + ", ".join(done))
    print(f"{'dry-run, nothing written' if args.dry_run else 'written'}"
          + (f"  backup: {b}" if b else ""))


def is_real_char(rec) -> bool:
    """A live character record: unit_add (0x112A5) and world_load_party
    (0x1088D) both stamp byte +6 with 2, while an unused map slot is
    `memset(rec,0,80); rec[5]=1` - so +6 != 2 means "template /空位",
    not somebody whose HP should be touched."""
    return rec[6] == 2


def filter_real(reg: Region, idxs, only_real: bool):
    if not only_real:
        return idxs
    return [i for i in idxs if is_real_char(reg.rec(i))]


def cmd_heal(args):
    buf = load(args.file)
    targets = expand_regions(args.region)
    n = 0
    skipped = 0
    for rname in targets:
        reg = Region(buf, rname)
        if rname not in ("head", "roster") and reg.empty():
            continue
        all_idx = select_members(reg, args.member)
        keep    = filter_real(reg, all_idx, args.real_only)
        for mi in keep:
            r = reg.rec(mi)
            set_u16(r, 0x40, u16(r, 0x42))         # HP  = max
            set_u16(r, 0x44, u16(r, 0x46))         # MP  = max
        n += len(keep)
        skipped += len(all_idx) - len(keep)
    b = store(args.file, buf, backup=not args.no_backup, dry=args.dry_run)
    print(f"healed {n} record(s)"
          + (f", skipped {skipped} placeholder record(s)" if skipped else ""))
    print(f"{'dry-run, nothing written' if args.dry_run else 'written'}"
          + (f"  backup: {b}" if b else ""))


def cmd_check(args):
    """Compare the stored derived block (+0x48..+0x4E) against a fresh
    unit_recalc run - proves the item-bonus table is read correctly."""
    if not ITEM_BONUS:
        raise SystemExit("item bonus table unavailable "
                         "(need analysis/seg_dseg03.bin)")
    buf = load(args.file)
    bad = 0
    total = 0
    for rname in (expand_regions(args.region)):
        reg = Region(buf, rname)
        if rname not in ("head", "roster") and reg.empty():
            continue
        for i in range(reg.count):
            r = reg.rec(i)
            got = (u16(r, 0x48), u16(r, 0x4A), u16(r, 0x4C), u16(r, 0x4E))
            want = tuple(v & 0xFFFF for v in derived(r))
            total += 1
            mark = "ok" if got == want else "DIFF"
            if got != want:
                bad += 1
            print(f"{rname}#{i:<2} id={r[8]:<3} stored AP/DP/DX/DE={got} "
                  f"recalc={want}  {mark}")
    print(f"\n{total - bad}/{total} records reproduce the game's own "
          f"unit_recalc output")


def cmd_recalc(args):
    buf = load(args.file)
    n = 0
    for rname in (expand_regions(args.region)):
        reg = Region(buf, rname)
        if rname not in ("head", "roster") and reg.empty():
            continue
        for mi in select_members(reg, args.member):
            apply_recalc(reg.rec(mi))
            n += 1
    b = store(args.file, buf, backup=not args.no_backup, dry=args.dry_run)
    print(f"recalculated {n} record(s)")
    print(f"{'dry-run, nothing written' if args.dry_run else 'written'}"
          + (f"  backup: {b}" if b else ""))


def cmd_raw(args):
    buf = load(args.file)
    reg = Region(buf, args.region)
    r = reg.rec(int(args.member, 0))
    off = int(args.offset, 0)
    data = bytes(int(x, 16) for x in args.bytes)
    if off + len(data) > REC_STRIDE:
        raise SystemExit("write would run past the 80-byte record")
    r[off:off + len(data)] = data
    b = store(args.file, buf, backup=not args.no_backup, dry=args.dry_run)
    print(f"wrote {len(data)} byte(s) at {reg.name}#{args.member} +0x{off:02X}: "
          f"{data.hex()}")
    print(f"{'dry-run, nothing written' if args.dry_run else 'written'}"
          + (f"  backup: {b}" if b else ""))


def cmd_dump(args):
    buf = load(args.file)
    reg = Region(buf, args.region)
    for mi in select_members(reg, args.member):
        r = bytes(reg.rec(mi))
        print(f"{reg.name} #{mi}  ({reg.base + REC_STRIDE*mi:#06x})")
        for o in range(0, REC_STRIDE, 16):
            hx = " ".join(f"{b:02X}" for b in r[o:o + 16])
            print(f"  +0x{o:02X}  {hx}")


# --------------------------------------------------------------------------- #
def build_parser():
    p = argparse.ArgumentParser(
        description="FD2 (炎龙骑士团2) FD2.SAV 存档修改器",
        formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--file", default="FD2.SAV", help="FD2.SAV 路径")
    sub = p.add_subparsers(dest="cmd", required=True)

    s = sub.add_parser("info", help="解析并打印存档全部内容")
    s.set_defaults(func=cmd_info)

    s = sub.add_parser("get", help="读取某个记录")
    s.add_argument("region", choices=REGIONS + ["all"])
    s.add_argument("member", help="#索引 | id=<n> | *")
    s.add_argument("fields", nargs="*", help="要打印的字段（默认全部）")
    s.set_defaults(func=cmd_get)

    s = sub.add_parser("set", help="修改记录字段，如 set s0 #0 level=20 hp=999 maxhp=999")
    s.add_argument("region", choices=REGIONS + ["all"])
    s.add_argument("member", help="#索引 | id=<n> | *")
    s.add_argument("assign", nargs="+", metavar="field=value")
    s.add_argument("--recalc", action="store_true",
                   help="改完后重算派生属性 (+0x48..+0x4E)")
    s.add_argument("--dry-run", action="store_true")
    s.add_argument("--no-backup", action="store_true")
    s.set_defaults(func=cmd_set)

    s = sub.add_parser("check", help="校验派生属性与游戏 unit_recalc 是否一致")
    s.add_argument("--region", default="all",
                   help="head|roster|s0..s3|continue|all，或逗号列表 head,roster")
    s.set_defaults(func=cmd_check)

    s = sub.add_parser("recalc", help="重算派生属性 (+0x48..+0x4E)")
    s.add_argument("--region", default="all",
                   help="head|roster|s0..s3|continue|all，或逗号列表 head,roster")
    s.add_argument("--member", default="*")
    s.add_argument("--dry-run", action="store_true")
    s.add_argument("--no-backup", action="store_true")
    s.set_defaults(func=cmd_recalc)

    s = sub.add_parser("money", help="设置/增加金钱")
    s.add_argument("amount", type=lambda x: int(x, 0), help="数额（可用 0x 前缀）")
    s.add_argument("--region", default="all",
                   help="head|roster|s0..s3|continue|all，或逗号列表 head,roster")
    s.add_argument("--add", action="store_true", help="在现有数值上累加")
    s.add_argument("--dry-run", action="store_true")
    s.add_argument("--no-backup", action="store_true")
    s.set_defaults(func=cmd_money)

    s = sub.add_parser("heal", help="全体满血满魔")
    s.add_argument("--region", default="all",
                   help="head|roster|s0..s3|continue|all，或逗号列表 head,roster")
    s.add_argument("--member", default="*")
    s.add_argument("--real-only", action="store_true",
                   help="只处理真实角色（记录 +6 == 2），跳过 id=96 之类的空位记录")
    s.add_argument("--dry-run", action="store_true")
    s.add_argument("--no-backup", action="store_true")
    s.set_defaults(func=cmd_heal)

    s = sub.add_parser("dump", help="十六进制打印记录")
    s.add_argument("region", choices=REGIONS + ["all"])
    s.add_argument("member", nargs="?", default="#0")
    s.set_defaults(func=cmd_dump)

    s = sub.add_parser("raw", help="按字节写记录（高级）")
    s.add_argument("region", choices=REGIONS + ["all"])
    s.add_argument("member")
    s.add_argument("offset", help="记录内偏移，如 0x40")
    s.add_argument("bytes", nargs="+", help="十六进制字节，如 0xFF 0x7F")
    s.add_argument("--dry-run", action="store_true")
    s.add_argument("--no-backup", action="store_true")
    s.set_defaults(func=cmd_raw)
    return p


WRITE_CMDS = {"set", "money", "heal", "recalc", "raw"}


def main(argv=None):
    args = build_parser().parse_args(argv)
    args.func(args)
    if args.cmd in WRITE_CMDS and not getattr(args, "dry_run", False):
        buf = load(args.file)
        if not checksum_ok(buf):
            print("!! post-write checksum verification FAILED", file=sys.stderr)
            return 2
        print("post-write checksum: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
