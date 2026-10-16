#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""sav_diff.py - byte-level diff of two FD2.SAV files, de-obfuscated.

Prints every differing byte as [absolute offset] [record] [+off] old -> new so a
change can be attributed to a specific record field (the obfuscation is an
involution over a fixed keystream, so offsets in the plain view line up with the
file layout described in fd2sav.py).
"""
import sys

sys.path.insert(0, __file__.rsplit("/", 1)[0])
from fd2sav import load, REC_STRIDE, HEAD_PARTY, HEAD_ROSTER, SLOT_BASE, SLOT_SIZE  # noqa: E402


def where(off):
    if off >= 0x59C7:
        return "checksum(dword@0x59C7)"
    if HEAD_PARTY <= off < 0x12A3:
        i, r = divmod(off - HEAD_PARTY, REC_STRIDE)
        return f"head.party[{i}]+0x{r:02X}"
    if HEAD_ROSTER <= off < 0x30A3:
        i, r = divmod(off - HEAD_ROSTER, REC_STRIDE)
        return f"head.roster[{i}]+0x{r:02X}"
    for k in range(4):
        b = SLOT_BASE + SLOT_SIZE * k
        if b <= off < b + 0xA28:
            i, r = divmod(off - b, REC_STRIDE)
            return f"s{k}[{i}]+0x{r:02X}"
    return "?"


def main(a, b):
    A, B = load(a), load(b)
    n = 0
    for i in range(len(A)):
        if A[i] != B[i]:
            n += 1
            print(f"0x{i:04X}  {where(i):<22} {A[i]:02X} -> {B[i]:02X}")
    print(f"\n{n} byte(s) differ")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
