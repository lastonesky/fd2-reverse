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
    0x12A3  u8[80*N]   battlefield unit table (dword_53A45, N = field[1]);
                       this is every unit alive in the current scene, including
                       NPC allies (+6 == 1) and enemies (+6 == 0)
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
    +0x06 u8    faction: 0 = enemy, 1 = NPC/ally, 2 = player (unit_add 0x112A5
                and world_load_party 0x1088D stamp 2; unit_entry_build 0x10C50
                copies the FDFIELD scenario byte).  It is NOT an empty marker.
                A truly unused slot is memset(rec,0,80) with rec[5] = 1.
    +0x07 u8    DATO portrait resource index
    +0x08 u8    identity byte (party/roster join key)
    +0x3B u8    movement points (MV; map_move.c reads rec[0x3B])
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

Raw bytes are the authority.  Every write re-stamps the checksum and
re-obfuscates.  A load leaves a timestamped .bak next to the file -- but only
one: the first write of a session creates it, later writes of the same session
reuse that same name (see Edit.save).
"""
from __future__ import annotations

import argparse
import json
import os
import re
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
ITEM_NAMES_JSON = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                               "fd2data", "items.json")


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


def load_item_types():
    """Item entry byte +0: the item's class/type.

    1..0x14 = weapon, 0x15..0x1F = armour, 0x20 = consumable,
    0x21 = gem, 0x22 = accessory/quest item.  The byte selects which frame
    box_items_draw (0x184C0) draws and whether the item can be equipped.
    """
    for p in DSEG03_CANDIDATES:
        p = os.path.normpath(p)
        if not os.path.exists(p):
            continue
        d = open(p, "rb").read()
        tbl = {}
        for i in range(len(d) // ITEM_STRIDE):
            o = ITEM_TBL_OFF + ITEM_STRIDE * i
            if o + ITEM_STRIDE > len(d):
                break
            tbl[i] = d[o]
        return tbl
    return {}


ITEM_TYPES = load_item_types()


def load_item_names():
    try:
        with open(ITEM_NAMES_JSON, encoding="utf-8") as f:
            raw = json.load(f)
        return {int(k): v for k, v in raw.items()}
    except (OSError, ValueError):
        return {}


ITEM_NAMES = load_item_names()


def item_name(index):
    """Display name for an item id (missing ids stay visible as placeholders)."""
    return ITEM_NAMES.get(index, f"未知道具 #{index}")


def item_type(index):
    return ITEM_TYPES.get(index)


def item_category(index):
    t = item_type(index)
    if t is None:
        return "—"
    if 1 <= t <= 0x14:
        return "武器"
    if 0x15 <= t <= 0x1F:
        return "防具"
    if t == 0x20:
        return "道具"
    if t == 0x21:
        return "宝石"
    if t == 0x22:
        return "饰品/其他"
    return f"类型 {t:#04x}"


def item_is_weapon(index):
    t = item_type(index)
    return t is not None and 1 <= t <= 0x14


def item_is_equipment(index):
    """Weapons and armour can carry the state-byte 0x40 'equipped' bit."""
    t = item_type(index)
    return t is not None and 1 <= t <= 0x1F


def item_bonus(index):
    """(AP, DX, DP, DX-derived) from the tbl_602AD entry, or None."""
    return ITEM_BONUS.get(index) if ITEM_BONUS else None


def item_bonus_text(index):
    b = item_bonus(index)
    if not b:
        return ""
    names = ("AP", "DX", "DP", "DX+")
    return "  ".join(f"{n}{v:+d}" for n, v in zip(names, b) if v)


def slot_off(slot):
    """Record offset of slot `slot`'s [state, item-id] pair."""
    return 0x0A + 2 * slot


def slot_state(rec, slot):
    return rec[slot_off(slot)]


def slot_item(rec, slot):
    return rec[slot_off(slot) + 1]


def slot_empty(rec, slot):
    """Bit 7 of the state byte means empty (box_items_draw 0x184C0)."""
    return bool(rec[slot_off(slot)] & 0x80)


def set_slot(rec, slot, item, equip=None, empty=None):
    """Write one inventory slot.  `item=None` (or 0xFF) empties it.

    The game's two meaningful state-byte values are 0x80 (empty) and
    0x40 (present and equipped); a present, unequipped item uses 0x00
    (rec_slot_claim 0x1BB8C / rec_slot_remove 0x1B8E7).

    Empty slots still carry a value byte the game never reads (rec_slot_remove
    leaves the shifted byte there), so pass `empty=True` with the old `item`
    to preserve it instead of zeroing unrelated bytes.
    """
    off = slot_off(slot)
    if empty is None:
        empty = item is None or item == 0xFF
    if empty:
        rec[off] = 0x80
        rec[off + 1] = 0 if item is None else (item & 0xFF)
        return
    if equip is None:
        equip = bool(rec[off] & 0x40) and not (rec[off] & 0x80)
    rec[off + 1] = item & 0xFF
    rec[off] = 0x40 if equip else 0x00


def set_equipped(rec, slot, on):
    """Mimic box_item_select (0x1C142): one equipped item per id half.

    Weapons are ids < 0x80 and armour/consumables are ids >= 0x80 in the
    original table; the game clears the 0x40 bit on every other equipped
    slot in the same half before marking the chosen slot.
    """
    if slot_empty(rec, slot):
        return False
    off = slot_off(slot)
    if on:
        high = slot_item(rec, slot) >= 0x80
        for k in range(8):
            if k == slot:
                continue
            o = slot_off(k)
            if rec[o] & 0x80:
                continue
            if (rec[o] & 0x40) and ((rec[o + 1] >= 0x80) == high):
                rec[o] &= ~0x40
        rec[off] = (rec[off] & ~0x80) | 0x40
    else:
        rec[off] &= ~0x40
    return True


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


def new_backup_path(path: str) -> str:
    """给这份存档起一个新的备份名；同一秒内重复也不覆盖（追加 -2、-3）。"""
    base = f"{path}.bak-{time.strftime('%Y%m%d-%H%M%S')}"
    p, n = base, 1
    while os.path.exists(p):
        n += 1
        p = f"{base}-{n}"
    return p


def store(path: str, buf: bytearray, backup=True, dry=False,
          reuse_bak: str | None = None) -> str | None:
    """写盘。

    reuse_bak 是「本次载入已经留过的备份」的名字：给了就继续用这一个，
    并保持它的内容（即载入时的样子），不再重复拷贝。所以一次载入 = 一份
    备份，连点保存也不会刷出一长串 .bak。传 None 则新起一个时间戳备份。
    """
    if len(buf) != SAVE_SIZE:
        raise SystemExit("refusing to write: buffer size changed")
    stamp(buf)
    obf = bytearray(buf)
    deobfuscate(obf)                       # same routine - it is an involution
    bpath = None
    if dry:
        return None
    if backup and os.path.exists(path):
        bpath = reuse_bak or new_backup_path(path)
        if not os.path.exists(bpath):         # 复用时不覆盖，备份停在载入那一刻
            shutil.copy2(path, bpath)
            os.utime(bpath, None)             # 备份时间应是“此刻”，不继承源文件
    try:
        with open(path, "wb") as f:
            f.write(obf)
    except PermissionError:
        raise SystemExit(f"写不进 {path}\n"
                         "  存档可能正被游戏占用，或文件是只读的——先退出游戏再试。")
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
    "mv":     ("u8",  0x3B),         # movement points 移动力
    "kind":   ("u8",  0x06),
    "faction": ("u8", 0x06),          # alias: 0 敌方 / 1 友军 / 2 我方
    "id":     ("u8",  0x08),
    "portrait": ("u8", 0x07),
    "flag":   ("u8",  0x05),
    "class0": ("u8",  0x1F),
    "class1": ("u8",  0x20),
    "x":      ("u8",  0x00),
    "y":      ("u8",  0x01),
    "dir":    ("u8",  0x03),
    "large":  ("u8",  0x04),         # 占用两格的巨型单位
    "status": ("u8",  0x34),         # rec_status_set 的 +52 状态字节
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
    """sel: '#i' index | 'id=<n>' | '*' all live units.

    '*' only returns real units (enemies and NPCs included); use an explicit
    '#i' to reach an empty/template slot."""
    n = min(reg.count, reg.max_recs)
    if sel == "*":
        return [i for i in range(n) if is_real_char(reg.rec(i))]
    if sel.startswith("id="):
        want = int(sel[3:], 0)
        hits = [i for i in range(n)
                if reg.rec(i)[8] == (want & 0xFF)
                and is_real_char(reg.rec(i))]
        if not hits:
            raise SystemExit(f"no live member with id {want} in {reg.name}")
        return hits
    if sel.startswith("#"):
        return [int(sel[1:], 0)]
    return [int(sel, 0)]


def faction_label(rec) -> str:
    """+6: 0 enemy, 1 NPC/ally, 2 player (unit_entry_build 0x10C50)."""
    return FACTION_NAMES.get(rec[6], f"?{rec[6]}")


def is_blank_record(rec) -> bool:
    """The exact unused-slot pattern written by world_load_party (0x1088D):
    memset(rec, 0, 80), then rec[5] = 1.  Everything else is a real record."""
    return rec[5] == 1 and not any(rec[k] for k in range(REC_STRIDE) if k != 5)


def is_real_char(rec) -> bool:
    """A live battlefield record.

    Byte +6 is the faction (0 enemy / 1 NPC ally / 2 player), not a "kind"
    that marks a slot empty: unit_entry_build (0x10C50) fills enemies and NPCs
    from FDFIELD with 0/1, and sys_menu saves the whole dword_53A45 table.
    Only the explicit memset pattern above (and an all-zero beyond-count slot)
    is empty."""
    return bool(any(rec)) and not is_blank_record(rec)


def filter_real(reg: Region, idxs, only_real: bool):
    if not only_real:
        return idxs
    return [i for i in idxs if is_real_char(reg.rec(i))]


# --------------------------------------------------------------------------- #
# 存档定位：不写 -f 也能找到
# --------------------------------------------------------------------------- #
SAVE_NAME = "FD2.SAV"
SAVE_ENV = "FD2_SAV"
DEFAULT_DIRS = ["E:/FD2", "D:/FD2", "C:/FD2", "E:/GAMES/FD2", "D:/GAMES/FD2"]

FACTION_NAMES = {0: "敌方", 1: "友军", 2: "我方"}

REGION_LABEL = {
    "head":   "继续游戏·队伍",
    "roster": "继续游戏·战场单位",
    "s0":     "存档槽 1",
    "s1":     "存档槽 2",
    "s2":     "存档槽 3",
    "s3":     "存档槽 4",
}
REGION_WORDS = set(REGIONS) | {"all", "continue", "cont"}


def _save_candidates():
    here = os.path.dirname(os.path.abspath(__file__))
    cands = [
        os.path.join(os.getcwd(), SAVE_NAME),
        os.path.normpath(os.path.join(here, "..", SAVE_NAME)),
        os.path.normpath(os.path.join(here, "..", "..", SAVE_NAME)),
    ]
    cands += [os.path.join(d, SAVE_NAME) for d in DEFAULT_DIRS]
    return cands


def find_save(explicit=None, quiet=False):
    """-f/--file > 环境变量 FD2_SAV > 自动搜索（当前目录、工程目录、常见安装目录）。
    多个命中时取最新的一个。"""
    if explicit:
        p = os.path.abspath(os.path.expanduser(explicit))
        if not os.path.isfile(p):
            raise SystemExit(f"找不到存档：{p}")
        return p

    env = os.environ.get(SAVE_ENV)
    if env:
        p = os.path.abspath(os.path.expanduser(env))
        if not os.path.isfile(p):
            raise SystemExit(f"{SAVE_ENV}={env} 指向的文件不存在")
        return p

    hits = []
    for p in _save_candidates():
        p = os.path.abspath(p)
        if os.path.isfile(p) and p not in hits:
            hits.append(p)
    if not hits:
        raise SystemExit(
            "找不到 FD2.SAV。\n"
            "  1) 用 -f <路径> 指定；\n"
            "  2) 或把存档放到当前目录；\n"
            f"  3) 或设置环境变量 {SAVE_ENV}=<路径>。")
    hits.sort(key=os.path.getmtime, reverse=True)
    if len(hits) > 1 and not quiet:
        print(f"（找到 {len(hits)} 个 FD2.SAV，用最新的那个）")
        for p in hits:
            print(f"    {p}")
    return hits[0]


def backups(path):
    """按文件名里的时间戳排序（copy2 会让 mtime 继承源文件，不能用来排序）。"""
    d = os.path.dirname(os.path.abspath(path)) or "."
    base = os.path.basename(path) + ".bak-"
    try:
        names = os.listdir(d)
    except OSError:
        return []
    out = [os.path.join(d, n) for n in names if n.startswith(base)]
    out.sort(key=lambda p: os.path.basename(p)[len(base):], reverse=True)
    return out


def bak_stamp(p):
    """从 FD2.SAV.bak-20261017-161343 里取出可读时间。"""
    m = re.search(r"bak-(\d{8})-(\d{6})", os.path.basename(p))
    if not m:
        return time.strftime("%Y-%m-%d %H:%M:%S",
                             time.localtime(os.path.getmtime(p)))
    d, t = m.group(1), m.group(2)
    return f"{d[0:4]}-{d[4:6]}-{d[6:8]} {t[0:2]}:{t[2:4]}:{t[4:6]}"


# --------------------------------------------------------------------------- #
# 角色名（FDTXT.DAT 文本流 + 字形表）
# --------------------------------------------------------------------------- #
# 名字不在存档里，而是 FDTXT.DAT 的文本流：
#   box_popup.c:60   vm_run(dword_53A7D, r[8]  + 1,   ...)   名字
#   box_popup.c:61   vm_run(dword_53A7D, r[31] + 140, ...)   种族 (+0x1F)
#   box_popup.c:62   vm_run(dword_53A7D, r[32] + 150, ...)   职业 (+0x20)
#
# FDTXT.DAT 是 res_load 的容器（res.c 0x111BA）：+6 起是 u32 块表，取
# [tab[i], tab[i+1]) 这一段。块 0 内部是 int16 偏移表 + 字形序列
# （vm_run 0x15F84：`s + *(int16*)(s + 2*sub)`，逐 int16 画字形，负数结束）。
# 字形号 -> 汉字来自 tools/fd2data/glyphs.json。
TXT_FILE = "FDTXT.DAT"
GLYPHS_JSON = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           "fd2data", "glyphs.json")
NAME_BASE = 1          # sub = rec[+0x08] + 1
RACE_BASE = 140        # sub = rec[+0x1F] + 140
CLASS_BASE = 150       # sub = rec[+0x20] + 150

_NAME_CACHE = {}


def find_data_file(name, save_path=None):
    """游戏数据文件（FDTXT.DAT）一般和存档放在一起，也可能在游戏目录。"""
    here = os.path.dirname(os.path.abspath(__file__))
    dirs = []
    if save_path:
        dirs.append(os.path.dirname(os.path.abspath(save_path)))
    dirs += [os.getcwd(),
             os.path.normpath(os.path.join(here, "..")),
             os.path.normpath(os.path.join(here, "..", ".."))]
    dirs += DEFAULT_DIRS
    seen = set()
    for d in dirs:
        if d in seen:
            continue
        seen.add(d)
        p = os.path.join(d, name)
        if os.path.isfile(p):
            return p
    return None


def load_glyphs():
    try:
        with open(GLYPHS_JSON, encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return None


def parse_fdtxt(raw, block=0, glyphs=None):
    """解析 FDTXT.DAT 的一个块，返回 {子串索引: 文本}。"""
    glyphs = glyphs if glyphs is not None else load_glyphs()
    if not glyphs or len(raw) < 6 + 4 * (block + 2):
        return {}
    start = struct.unpack_from("<I", raw, 6 + 4 * block)[0]
    end = struct.unpack_from("<I", raw, 6 + 4 * (block + 1))[0]
    if not 0 <= start < end <= len(raw):
        return {}
    blk = raw[start:end]
    if len(blk) < 4:
        return {}
    first = struct.unpack_from("<h", blk, 0)[0]      # 表长 = 数据起点
    if first <= 0:
        return {}
    n = min(first // 2, len(blk) // 2)
    out = {}
    for i in range(n):
        off = struct.unpack_from("<h", blk, 2 * i)[0]
        if off <= 0 or off >= len(blk):
            continue
        words, p = [], off
        while p + 2 <= len(blk) and len(words) < 128:
            w = struct.unpack_from("<h", blk, p)[0]
            if w < 0:                                # -1 = 子串结束
                break
            words.append(w)
            p += 2
        out[i] = "".join(glyphs.get(str(w), "") for w in words)
    return out


def name_table(save_path=None):
    """找 FDTXT.DAT 并解析块 0。找不到返回 {}，调用方自行降级。"""
    p = find_data_file(TXT_FILE, save_path)
    if not p:
        return {}
    if p in _NAME_CACHE:
        return _NAME_CACHE[p]
    try:
        with open(p, "rb") as f:
            raw = f.read()
    except OSError:
        return {}
    tbl = parse_fdtxt(raw, 0)
    _NAME_CACHE[p] = tbl
    return tbl


def char_name(rec, names):
    """记录 +0x08 是 party/roster 的身份字节，名字是文本流第 (它+1) 条。"""
    if not names:
        return ""
    return names.get(rec[0x08] + NAME_BASE, "")


def race_name(rec, names):
    if not names:
        return ""
    return names.get(rec[0x1F] + RACE_BASE, "")


def class_name(rec, names):
    if not names:
        return ""
    return names.get(rec[0x20] + CLASS_BASE, "")


# --------------------------------------------------------------------------- #
# 修改会话：加载 -> 改 -> 有变化才写盘
# --------------------------------------------------------------------------- #
class Edit:
    def __init__(self, path):
        self.path = path
        self.buf = load(path)
        self.orig = bytes(self.buf)
        self.bak = None          # 本次载入的备份：第一次写盘时生成，之后一直复用

    @property
    def dirty(self):
        return bytes(self.buf) != self.orig

    def changed_bytes(self):
        return sum(1 for a, b in zip(self.orig, bytes(self.buf)) if a != b)

    def save(self, backup=True):
        """写盘。本次载入只在第一次写盘时生成备份，之后再保存仍写同一个备份名。"""
        bpath = store(self.path, self.buf, backup=backup, reuse_bak=self.bak)
        if bpath:
            self.bak = bpath
        return bpath

    def commit(self, dry=False, backup=True):
        if not self.dirty:
            print("\n没有需要改动的内容，文件未写。")
            return None
        if dry:
            print("\n--dry-run：以上改动没有写入。")
            return None
        bpath = self.save(backup=backup)
        if not checksum_ok(load(self.path)):
            print("!! 写后校验失败，请用 restore --latest 回滚", file=sys.stderr)
            raise SystemExit(2)
        print(f"\n已写入  {self.path}")
        print(f"        实际改动 {self.changed_bytes()} 字节，校验和 OK")
        if bpath:
            print(f"备份    {bpath}")
            print(f'回滚    {prog()} -f "{self.path}" restore --latest')
        return bpath


def prog():
    return "python tools/fd2sav.py"


def hint(path, rest):
    """给用户可直接复制执行的命令行（带上存档路径，避免误操作到别的存档）。"""
    return f'{prog()} -f "{path}" {rest}'


def first_region(spec):
    """给 get/dump/raw 这种只接受单个 region 的命令用（允许写 all / continue）。"""
    return expand_regions(spec)[0]


def open_region(edit, name):
    reg = Region(edit.buf, name)
    if name not in ("head", "roster") and reg.empty():
        return None
    return reg


# --------------------------------------------------------------------------- #
# 记录显示
# --------------------------------------------------------------------------- #
def _qty(r):
    return u16(r, 0x40), u16(r, 0x42), u16(r, 0x44), u16(r, 0x46)


ROW_FMT = "  {:<4}{}{}{:<4}{:<4}{:>9}{:>10}{:>6}{:>5}{:>5}"


def _w(s):
    """显示宽度：CJK 算 2 列。"""
    return sum(2 if ord(c) > 0x2E7F else 1 for c in s)


def _pad(s, width):
    s = str(s)
    return s + " " * max(0, width - _w(s))


ROW_HDR = ROW_FMT.format("#", _pad("名字", 12), _pad("阵营", 6),
                         "id", "Lv", "HP", "MP", "AP", "DP", "DX")


def rec_flat(reg, i, names=None):
    r = reg.rec(i)
    hp, mhp, mp, mmp = _qty(r)
    mark = "  << 残血" if hp < mhp else ""
    return (ROW_FMT.format(i, _pad(char_name(r, names), 12),
                           _pad(faction_label(r), 6), r[8], r[0x21],
                           f"{hp}/{mhp}", f"{mp}/{mmp}",
                           u16(r, 0x37), u16(r, 0x39), u16(r, 0x3E)) + mark)


def rec_items(reg, i):
    r = reg.rec(i)
    items = " ".join(f"{get_field(r, f'item{k}'):02X}" for k in range(8))
    slots = [f"{r[0x0A + 2 * k]:02X}:{r[0x0B + 2 * k]:02X}" for k in range(8)]
    return f"        items[{items}]  slots={slots}"


def wounded(reg, idxs=None):
    n = []
    for i in (idxs if idxs is not None else range(min(reg.count, reg.max_recs))):
        r = reg.rec(i)
        if is_real_char(r) and u16(r, 0x40) < u16(r, 0x42):
            n.append(i)
    return n


# --------------------------------------------------------------------------- #
# 命令
# --------------------------------------------------------------------------- #
def cmd_info(args):
    buf = load(args.file)
    names = name_table(args.file)
    f = buf[HEAD_FIELD:HEAD_FIELD + 18]
    money = struct.unpack_from("<I", buf, HEAD_MONEY)[0]
    party = Region(buf, "head")
    roster = Region(buf, "roster")
    n_units = min(roster.count, roster.max_recs)
    real = [i for i in range(n_units) if is_real_char(roster.rec(i))]
    fac = {}
    for i in real:
        k = roster.rec(i)[6]
        fac[k] = fac.get(k, 0) + 1
    fac_txt = " / ".join(f"{FACTION_NAMES.get(k, '?' + str(k))} {v}"
                         for k, v in sorted(fac.items())) or "无"
    blanks = n_units - len(real)

    print(f"存档    {args.file}")
    print(f"        22,987 B   校验和 {'OK' if checksum_ok(buf) else '不匹配!'}"
          f"   备份 {len(backups(args.file))} 个")
    if not names:
        print("（没找到 FDTXT.DAT / glyphs.json，名字列留空；"
              "把 FDTXT.DAT 放到存档同目录即可显示）")
    print(f"\n【继续游戏】  地点 {f[0]}   场景 {f[2]}   "
          f"队伍 {f[9]} 人   金钱 {money:,}")
    print(ROW_HDR)
    for i in range(min(party.count, party.max_recs)):
        print(rec_flat(party, i, names))
        if args.detail:
            print(rec_items(party, i))
    hw = wounded(party)
    if hw:
        print(f"  残血 {len(hw)} 人：{', '.join('#' + str(i) for i in hw)}"
              f"   (heal 一条命令搞定)")

    print(f"\n【战场单位表】{roster.count} 条，实际单位 {len(real)} 条（{fac_txt}）"
          + (f"，空位 {blanks} 条" if blanks else ""))
    print(ROW_HDR)
    for i in real:
        print(rec_flat(roster, i, names))
        if args.detail:
            print(rec_items(roster, i))
    rw = wounded(roster)
    if rw:
        print(f"  残血 {len(rw)} 个单位：{', '.join('#' + str(i) for i in rw)}"
              f"   (heal 一条命令搞定，敌人/NPC 也会被包含)")

    print("\n【存档槽】")
    for k in range(4):
        reg = Region(buf, f"s{k}")
        if reg.empty():
            print(f"  槽 {k + 1}  空")
            continue
        hw = wounded(reg)
        tail = f"   残血 {len(hw)} 人" if hw else ""
        print(f"  槽 {k + 1}  场景 {reg.scenario_id():<3} "
              f"{reg.count} 人   {reg.money:,} 金{tail}")


def cmd_get(args):
    buf = load(args.file)
    names = name_table(args.file)
    reg = Region(buf, first_region(args.region))
    fields = args.fields or ["name", "faction", "race", "class", "id", "kind", "level",
                             "hp", "maxhp", "mp", "maxmp", "ap", "dp", "dx",
                             "x", "y"] \
        + [f"item{i}" for i in range(8)]

    def cell(r, k):
        if k == "name":
            return f"name={char_name(r, names) or '-'}"
        if k == "race":
            return f"race={race_name(r, names) or '-'}"
        if k == "class":
            return f"class={class_name(r, names) or '-'}"
        return f"{k}={get_field(r, k)}"

    for mi in select_members(reg, args.member):
        r = reg.rec(mi)
        print(f"{REGION_LABEL[reg.name]}[{mi}]: " +
              "  ".join(cell(r, k) for k in fields))


def _parse_set_items(items):
    """兼容两种写法：
         set #0 level=20 hp=999            （region 默认 继续游戏）
         set all #0 level=20 hp=999        （显式给 region）
    """
    spec = "continue"
    if len(items) >= 2 and items[0] in REGION_WORDS:
        spec, items = items[0], items[1:]
    member, assigns = items[0], items[1:]
    if not assigns:
        raise SystemExit("没给要改的字段，例如：set #0 level=20 hp=999 maxhp=999")
    return spec, member, assigns


def cmd_set(args):
    e = Edit(args.file)
    spec, member, assigns = _parse_set_items(args.items)
    pairs = []
    for kv in assigns:
        k, sep, v = kv.partition("=")
        k = k.strip()
        if not sep:
            raise SystemExit(f"看不懂 '{kv}'，应为 字段=数值")
        if k not in FIELDS:
            raise SystemExit(f"没有字段 '{k}'（可用：{', '.join(sorted(FIELDS))}）")
        pairs.append((k, int(v, 0)))

    rows = []
    for rname in expand_regions(spec):
        reg = open_region(e, rname)
        if reg is None:
            print(f"  跳过 {REGION_LABEL[rname]}（空槽）")
            continue
        for mi in select_members(reg, member):
            r = reg.rec(mi)
            changed = []
            for k, v in pairs:
                old = get_field(r, k)
                set_field(r, k, v)
                if old != get_field(r, k):
                    changed.append(f"{k} {old}->{get_field(r, k)}")
            if args.recalc:
                before = (u16(r, 0x48), u16(r, 0x4A), u16(r, 0x4C), u16(r, 0x4E))
                apply_recalc(r)
                after = (u16(r, 0x48), u16(r, 0x4A), u16(r, 0x4C), u16(r, 0x4E))
                if before != after:
                    changed.append(f"派生 AP/DP/DX/DE {before[0]}/{before[1]}/"
                                   f"{before[2]}/{before[3]} -> "
                                   f"{after[0]}/{after[1]}/{after[2]}/{after[3]}")
            rows.append(f"  {REGION_LABEL[rname]}[{mi}] id={r[8]:<4} "
                        + ("  ".join(changed) if changed else "（无变化）"))

    if rows:
        print("【修改】")
        for s in rows:
            print(s)
    if "level" in {k for k, _ in pairs} and not (
            {"maxhp", "maxmp", "ap", "dp", "dx"} & {k for k, _ in pairs}):
        print("\n提示：只改了等级，上限值不会自动跟着涨——游戏只在真正的升级流程里"
              "按成长表加。\n      要一并改就加：maxhp=... maxmp=... ap=... dp=... dx=...")
    e.commit(dry=args.dry_run, backup=not args.no_backup)


def cmd_money(args):
    e = Edit(args.file)
    raw = str(args.amount).strip()
    add = args.add or raw.startswith("+")
    try:
        val = int(raw.lstrip("+"), 0)
    except ValueError:
        raise SystemExit(f"看不懂金额 '{raw}'（可写 9999 / 0x2710 / +5000）")
    rows = []
    for rname in expand_regions(args.region):
        reg = open_region(e, rname)
        if reg is None or reg.money_off is None:
            continue
        old = reg.money
        new = old + val if add else val
        if new < 0 or new > 0xFFFFFFFF:
            raise SystemExit(f"金额越界：{new}")
        reg.money = new
        label = "继续游戏" if rname == "head" else REGION_LABEL[rname]
        rows.append(f"  {label}   {old:,} -> {reg.money:,}")
    if rows:
        print("【金钱】")
        for s in rows:
            print(s)
    e.commit(dry=args.dry_run, backup=not args.no_backup)


def cmd_heal(args):
    e = Edit(args.file)
    rows = []
    skipped = 0
    for rname in expand_regions(args.region):
        reg = open_region(e, rname)
        if reg is None:
            print(f"  跳过 {REGION_LABEL[rname]}（空槽）")
            continue
        idxs = select_members(reg, args.member)
        if args.member == "*" and not args.real_only:
            idxs = list(range(min(reg.count, reg.max_recs)))
        keep = filter_real(reg, idxs, args.real_only)
        skipped += len(idxs) - len(keep)
        for mi in keep:
            r = reg.rec(mi)
            hp, mhp, mp, mmp = _qty(r)
            if hp >= mhp and mp >= mmp:
                continue
            parts = []
            if hp < mhp:
                parts.append(f"HP {hp}->{mhp}")
            if mp < mmp:
                parts.append(f"MP {mp}->{mmp}")
            set_u16(r, 0x40, mhp)
            set_u16(r, 0x44, mmp)
            rows.append(f"  {REGION_LABEL[rname]}[{mi}] [{faction_label(r)}] "
                        f"id={r[8]:<4} " + "   ".join(parts))
    if rows:
        print("【满血满魔】")
        for s in rows:
            print(s)
        print(f"  共 {len(rows)} 条记录")
    else:
        print("全部单位已经是满血满魔。")
    if skipped:
        print(f"  （跳过 {skipped} 条空位/未使用记录）")
    e.commit(dry=args.dry_run, backup=not args.no_backup)


def cmd_recalc(args):
    e = Edit(args.file)
    n = 0
    diff = 0
    for rname in expand_regions(args.region):
        reg = open_region(e, rname)
        if reg is None:
            continue
        for mi in select_members(reg, args.member):
            r = reg.rec(mi)
            before = (u16(r, 0x48), u16(r, 0x4A), u16(r, 0x4C), u16(r, 0x4E))
            apply_recalc(r)
            after = (u16(r, 0x48), u16(r, 0x4A), u16(r, 0x4C), u16(r, 0x4E))
            n += 1
            if before != after:
                diff += 1
                print(f"  {REGION_LABEL[rname]}[{mi}] id={r[8]:<4} "
                      f"{before} -> {after}")
    print(f"重算 {n} 条记录，其中 {diff} 条确实有变化。")
    e.commit(dry=args.dry_run, backup=not args.no_backup)


def cmd_check(args):
    """Compare the stored derived block (+0x48..+0x4E) against a fresh
    unit_recalc run - proves the item-bonus table is read correctly."""
    if not ITEM_BONUS:
        raise SystemExit("道具加成表缺失（需要 analysis/seg_dseg03.bin）")
    buf = load(args.file)
    bad = 0
    total = 0
    for rname in expand_regions(args.region):
        reg = Region(buf, rname)
        if rname not in ("head", "roster") and reg.empty():
            continue
        for i in range(min(reg.count, reg.max_recs)):
            r = reg.rec(i)
            got = (u16(r, 0x48), u16(r, 0x4A), u16(r, 0x4C), u16(r, 0x4E))
            want = tuple(v & 0xFFFF for v in derived(r))
            total += 1
            if got != want:
                bad += 1
                print(f"  {REGION_LABEL[rname]}[{i}] id={r[8]:<4} "
                      f"存档 {got}  重算 {want}  << 不一致")
    print(f"{total - bad}/{total} 条记录与游戏自身的 unit_recalc 结果一致。")
    return 0 if bad == 0 else 1


def cmd_raw(args):
    e = Edit(args.file)
    reg = Region(e.buf, first_region(args.region))
    r = reg.rec(int(args.member.lstrip("#"), 0))
    off = int(args.offset, 0)
    data = bytes(int(x, 16) for x in args.bytes)
    if off + len(data) > REC_STRIDE:
        raise SystemExit("这次写入会越过 80 字节记录边界")
    old = bytes(r[off:off + len(data)])
    r[off:off + len(data)] = data
    print(f"  {REGION_LABEL[reg.name]}[{args.member}] +0x{off:02X}  "
          f"{old.hex(' ')} -> {data.hex(' ')}")
    e.commit(dry=args.dry_run, backup=not args.no_backup)


def cmd_dump(args):
    buf = load(args.file)
    reg = Region(buf, first_region(args.region))
    for mi in select_members(reg, args.member):
        r = bytes(reg.rec(mi))
        print(f"{REGION_LABEL[reg.name]}[{mi}]  (文件偏移 0x{reg.base + REC_STRIDE * mi:04X})")
        for o in range(0, REC_STRIDE, 16):
            hx = " ".join(f"{b:02X}" for b in r[o:o + 16])
            print(f"  +0x{o:02X}  {hx}")


def cmd_restore(args):
    path = args.file
    baks = backups(path)                 # 先取快照，避免把刚生成的备份算进候选
    if not baks:
        print("这个存档还没有备份。")
        return

    if args.list or (not args.latest and not args.backup):
        print(f"{path} 的备份（新的在上）：")
        for i, b in enumerate(baks):
            print(f"  [{i}] {bak_stamp(b)}   {os.path.basename(b)}")
        if not args.latest and not args.backup:
            print(f"\n恢复最新：{hint(path, 'restore --latest')}")
            print(f"恢复指定：{hint(path, 'restore --backup <序号或文件名>')}")
        return

    if args.latest:
        target = baks[0]
    else:
        key = str(args.backup)
        if key.isdigit():
            i = int(key)
            if not 0 <= i < len(baks):
                raise SystemExit(f"没有第 {i} 个备份（共 {len(baks)} 个）")
            target = baks[i]
        else:
            hit = [b for b in baks if os.path.basename(b) == key]
            target = hit[0] if hit else os.path.join(os.path.dirname(baks[0]), key)
            if not os.path.isfile(target):
                raise SystemExit(f"找不到备份 {key}")

    keep = new_backup_path(path)
    shutil.copy2(path, keep)
    shutil.copy2(target, path)

    buf = load(path)
    ok = checksum_ok(buf)
    print(f"已恢复  {os.path.basename(target)}  ->  {path}")
    print(f"恢复前  {os.path.basename(keep)}")
    print(f"校验和  {'OK' if ok else '不匹配（这个备份本身就有问题）'}")
    if not ok:
        return 1


# --------------------------------------------------------------------------- #
# 交互菜单
# --------------------------------------------------------------------------- #
MENU_TEXT = """
 1  全体满血满魔（继续游戏）
 2  全体满血满魔（含 4 个存档槽）
 3  修改金钱
 4  修改角色属性（等级 / HP / 攻防…）
 5  查看全部记录
 6  备份 / 回滚
 0  退出
"""


def _summary(path):
    buf = load(path)
    f = buf[HEAD_FIELD:HEAD_FIELD + 18]
    money = struct.unpack_from("<I", buf, HEAD_MONEY)[0]
    party = Region(buf, "head")
    roster = Region(buf, "roster")
    hw = wounded(party)
    rw = wounded(roster)
    slots = []
    for k in range(4):
        reg = Region(buf, f"s{k}")
        slots.append("空" if reg.empty() else f"{reg.count}人/{reg.money}G")
    print(f"\n=== {os.path.basename(path)}  {path} ===")
    print(f"继续游戏：地点 {f[0]}  场景 {f[2]}  队伍 {f[9]} 人  金钱 {money:,}"
          + (f"   [残血 {len(hw)} 人]" if hw else "   [满血]"))
    print(f"战场单位：{roster.count} 条（含敌人/NPC）"
          + (f"   残血 {len(rw)} 个" if rw else "   全部满血"))
    print(f"存档槽  ：1:{slots[0]}  2:{slots[1]}  3:{slots[2]}  4:{slots[3]}")


def cmd_menu(args):
    path = args.file
    while True:
        try:
            _summary(path)
            print(MENU_TEXT)
            ch = input("请选择 [0-6]> ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            return 0

        try:
            if ch in ("0", "q", "Q"):
                return 0
            if ch == "1":
                cmd_heal(argparse.Namespace(file=path, region="continue", member="*",
                                            real_only=True, dry_run=False,
                                            no_backup=False))
            elif ch == "2":
                cmd_heal(argparse.Namespace(file=path, region="all", member="*",
                                            real_only=True, dry_run=False,
                                            no_backup=False))
            elif ch == "3":
                s = input("金额（数字；+5000 表示在当前基础上增加）> ").strip()
                if not s:
                    continue
                add = s.startswith("+")
                amount = int(s.lstrip("+"), 0)
                reg = input("范围：回车=继续游戏，a=全部区域 > ").strip().lower()
                cmd_money(argparse.Namespace(file=path, amount=amount, add=add,
                                             region="all" if reg == "a" else "continue",
                                             dry_run=False, no_backup=False))
            elif ch == "4":
                who = input("改谁（0 / #0 / id=9 / * 全部）> ").strip()
                if not who:
                    continue
                pairs = input("要改什么，如 level=40 hp=999 maxhp=999 > ").strip()
                if not pairs:
                    continue
                cmd_set(argparse.Namespace(file=path, items=[who] + pairs.split(),
                                           recalc=True, dry_run=False,
                                           no_backup=False))
            elif ch == "5":
                cmd_info(argparse.Namespace(file=path, detail=True))
            elif ch == "6":
                cmd_restore(argparse.Namespace(file=path, list=True,
                                               latest=False, backup=None))
                s = input("要回滚到哪个备份？回车取消，序号或 l=最新 > ").strip()
                if s:
                    if s.lower() == "l":
                        cmd_restore(argparse.Namespace(file=path, list=False,
                                                       latest=True, backup=None))
                    else:
                        cmd_restore(argparse.Namespace(file=path, list=False,
                                                       latest=False, backup=s))
            else:
                print("没有这个选项。")
        except SystemExit as ex:
            print(f"!! {ex}")
        except Exception as ex:                      # noqa: BLE001 - 菜单不该崩
            print(f"!! 出错了：{ex}")

        try:
            input("\n回车继续…")
        except (EOFError, KeyboardInterrupt):
            print()
            return 0


# --------------------------------------------------------------------------- #
# 命令行
# --------------------------------------------------------------------------- #
def build_parser():
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("-f", "--file", metavar="SAV", default=argparse.SUPPRESS,
                        help="FD2.SAV 路径（不填则自动查找；环境变量 FD2_SAV 亦可）")

    epilog = f"""常用：
  {prog()}                             交互菜单（会自动查找存档）
  {prog()} info                        看看现在谁残血
  {prog()} heal                        继续游戏所有单位（含敌人/NPC）满血满魔
  {prog()} heal --all                  连带 4 个存档槽一起
  {prog()} money 99999                 继续游戏金钱改 99999
  {prog()} money +5000                 加 5000
  {prog()} set 0 level=40 hp=999 maxhp=999
  {prog()} set roster 11 hp=1          改战场单位（#11 是敌人也行）
  {prog()} set roster '*' hp=999       战场所有单位一起改
  {prog()} restore --latest            回滚到最近一次备份

region 写法：continue（继续游戏，默认）| head | roster | s0..s3 | all | 逗号列表
member 写法：#索引 | id=序号 | *（所有真实单位，含敌人和 NPC）
"""
    p = argparse.ArgumentParser(
        parents=[common],
        description="炎龙骑士团2 (FD2) 存档修改器",
        epilog=epilog,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd")

    s = sub.add_parser("info", parents=[common], help="查看存档（谁残血一眼看到）")
    s.add_argument("-d", "--detail", action="store_true", help="附带道具/装备槽")
    s.set_defaults(func=cmd_info)

    s = sub.add_parser("get", parents=[common], help="读取某条记录的字段")
    s.add_argument("region", help="continue|head|roster|s0..s3|all")
    s.add_argument("member", help="#索引 | id=<n> | *")
    s.add_argument("fields", nargs="*", help="要打印的字段（默认常用字段）")
    s.set_defaults(func=cmd_get)

    s = sub.add_parser("set", parents=[common],
                       help="改字段，如 set 0 level=40 hp=999 maxhp=999")
    s.add_argument("items", nargs="+",
                   metavar="member field=value",
                   help="member 可写 0 / #0 / id=9 / *；也可在前面加 region")
    s.add_argument("--no-recalc", dest="recalc", action="store_false",
                   help="不重算派生属性（默认会重算，防止伤害不对）")
    s.add_argument("--dry-run", action="store_true")
    s.add_argument("--no-backup", action="store_true")
    s.set_defaults(func=cmd_set, recalc=True)

    s = sub.add_parser("check", parents=[common],
                       help="校验派生属性是否与 unit_recalc 一致")
    s.add_argument("--region", default="all")
    s.set_defaults(func=cmd_check)

    s = sub.add_parser("recalc", parents=[common], help="重算派生属性")
    s.add_argument("--region", default="all")
    s.add_argument("--member", default="*")
    s.add_argument("--dry-run", action="store_true")
    s.add_argument("--no-backup", action="store_true")
    s.set_defaults(func=cmd_recalc)

    s = sub.add_parser("money", parents=[common], help="改金钱")
    s.add_argument("amount",
                   help="数额：9999 / 0x2710；带 + 号表示在当前基础上增加，如 +5000")
    s.add_argument("--add", action="store_true", help="在现有数值上累加")
    s.add_argument("--region", default="continue")
    s.add_argument("--all", dest="region", action="store_const", const="all",
                   help="含 4 个存档槽")
    s.add_argument("--dry-run", action="store_true")
    s.add_argument("--no-backup", action="store_true")
    s.set_defaults(func=cmd_money)

    s = sub.add_parser("heal", parents=[common], help="满血满魔（默认继续游戏）")
    s.add_argument("--region", default="continue")
    s.add_argument("--all", dest="region", action="store_const", const="all",
                   help="含 4 个存档槽")
    s.add_argument("--member", default="*")
    s.add_argument("--with-placeholders", dest="real_only", action="store_false",
                   help="连真正的空位/模板记录也一起改（配合 *；一般不需要）")
    s.add_argument("--dry-run", action="store_true")
    s.add_argument("--no-backup", action="store_true")
    s.set_defaults(func=cmd_heal, real_only=True)

    s = sub.add_parser("dump", parents=[common], help="十六进制查看记录")
    s.add_argument("region", help="continue|head|roster|s0..s3|all")
    s.add_argument("member", nargs="?", default="0")
    s.set_defaults(func=cmd_dump)

    s = sub.add_parser("raw", parents=[common], help="按字节写记录（高级）")
    s.add_argument("region", help="continue|head|roster|s0..s3|all")
    s.add_argument("member")
    s.add_argument("offset", help="记录内偏移，如 0x40")
    s.add_argument("bytes", nargs="+", help="十六进制字节，如 0xFF 0x7F")
    s.add_argument("--dry-run", action="store_true")
    s.add_argument("--no-backup", action="store_true")
    s.set_defaults(func=cmd_raw)

    s = sub.add_parser("restore", parents=[common], help="从备份回滚")
    s.add_argument("--list", action="store_true", help="只列备份")
    s.add_argument("--latest", action="store_true", help="恢复最近一次备份")
    s.add_argument("--backup", metavar="N|文件名", help="恢复指定备份")
    s.set_defaults(func=cmd_restore)

    p.set_defaults(func=cmd_menu)
    return p


def main(argv=None):
    args = build_parser().parse_args(argv)
    args.file = find_save(getattr(args, "file", None))
    return args.func(args) or 0


if __name__ == "__main__":
    sys.exit(main())
