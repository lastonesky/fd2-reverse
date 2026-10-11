#!/usr/bin/env python3
"""prepare_image.py - regenerate the data-segment image from the original EXE.

The native build needs the game's dseg02/dseg03 *initial* bytes (the runtime
mirror fd2_data1/fd2_data2: file-name strings, the per-scene BGM table and the
static tables).  Those bytes are copyrighted game data, so they are NOT stored
in the repository.  Instead this script derives them from a locally supplied
copy of the original executable:

    1. If analysis/seg_dseg02.bin + seg_dseg03.bin already exist, reuse them.
    2. Otherwise extract them from FD2.EXE (DOS/4GW LE): map the object pages,
       apply the LE fixups (the pointer tables carry pre-relocation offsets),
       and write the two data segments.
    3. If there is no segment data AND no FD2.EXE, STOP with instructions.
    4. Generate src/fd2_image.c (data-only; the dead cseg01 code segment is
       intentionally not emitted - see tools/gen_headers.py gen_image()).

Put your FD2.EXE in the repository root (it is .gitignore'd) or pass
--exe PATH / set FD2_EXE.  Everything written here is derived and gitignored.

Usage: python tools/prepare_image.py [--exe FD2.EXE] [--force] [--quiet]
"""
import argparse
import hashlib
import os
import struct
import sys

ROOT    = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ANAL    = os.path.join(ROOT, "analysis")
SEG2    = os.path.join(ANAL, "seg_dseg02.bin")
SEG3    = os.path.join(ANAL, "seg_dseg03.bin")
IMAGE   = os.path.join(ROOT, "src", "fd2_image.c")

# (linear base, full size incl. BSS, friendly name) - see README/data-layout.md
WANT = [(0x50000, 0x56B0, "dseg02"), (0x60000, 0x4000, "dseg03")]

FD2_MD5 = "a6e341a8decc6ebf7f4872076d9cf161"   # EXTERNAL_ARTIFACTS.md anchor
PAGE    = 0x1000


def log(msg, quiet=False):
    if not quiet:
        print("prepare_image: " + msg)


# --------------------------------------------------------------- LE parse ---
def find_le_header(d):
    for i in range(0, len(d) - 0x80):
        if d[i:i+4] != b"LE\x00\x00":
            continue
        pages  = struct.unpack_from("<I", d, i + 0x14)[0]
        objtab = struct.unpack_from("<I", d, i + 0x40)[0]
        objcnt = struct.unpack_from("<I", d, i + 0x44)[0]
        if not (0 < pages <= 4096):
            continue
        if not (0x40 <= objtab <= 0x1000):
            continue
        if not (0 < objcnt <= 64):
            continue
        if i + objtab + objcnt * 24 > len(d):
            continue
        return i
    return None


def extract_segments(exe, quiet=False):
    d = open(exe, "rb").read()
    md5 = hashlib.md5(d).hexdigest()
    if md5 != FD2_MD5:
        log("WARNING: %s md5=%s does not match the known %s" % (exe, md5, FD2_MD5))
    h = find_le_header(d)
    if h is None:
        log("ERROR: no LE header in %s - not the DOS/4GW FD2.EXE?" % exe)
        sys.exit(1)

    rd32 = lambda o: struct.unpack_from("<I", d, o)[0]
    rd16 = lambda o: struct.unpack_from("<H", d, o)[0]

    mod_pages = rd32(h + 0x14)
    objtab    = rd32(h + 0x40)
    objcnt    = rd32(h + 0x44)
    last_page = rd32(h + 0x2C)
    pt        = h + rd32(h + 0x68)          # fixup page table
    rt        = h + rd32(h + 0x6C)          # fixup record table

    objs = []
    for i in range(objcnt):
        e = h + objtab + i * 24
        vs, base, flags, pi, pc, _r = struct.unpack_from("<IIIIII", d, e)
        objs.append(dict(vs=vs, base=base, pi=pi, pc=pc))

    # Object page data is contiguous at the end of the file, in object order.
    # The last object's last page may be shorter than PAGE (BSS tail).
    spans = []
    for i, o in enumerate(objs):
        s = (o["pc"] - 1) * PAGE
        if i + 1 == objcnt:
            t = last_page
            if t == 0 or t > PAGE:
                t = o["vs"] % PAGE
            if t == 0:
                t = PAGE
            s += t
        else:
            s += PAGE
        spans.append(s)
    start = len(d) - sum(spans)
    if start < 0:
        log("ERROR: object data start is before the file (span too big)")
        sys.exit(1)

    # Page-aligned zero-filled buffers, stored bytes copied in.
    buf = {}
    cur = start
    for i, o in enumerate(objs):
        b = bytearray(spans[i])
        copy = min(o["vs"], spans[i])
        b[:copy] = d[cur:cur + copy]
        buf[o["base"]] = b
        cur += spans[i]

    # Apply the LE fixups: the data tables hold pre-relocation offsets that the
    # DOS/4GW loader rewrites to absolute 32-bit addresses (e.g. the off_61955
    # effect-script table).  Grammar per page (verified in port/src/le.c):
    #   type 0x00 : one filler byte
    #   type 0x02 : [02][size][src:2][obj:1]      -> u16 selector operand
    #   type 0x07 : [07][b1][src:2][obj:1][tgt:N] -> u32 obj.base + tgt,
    #               N = 2 + (b1 >> 4); src > 0xFFF records are bogus, skip.
    applied = cross = 0
    for page in range(1, mod_pages + 1):
        begin = rd32(pt + (page - 1) * 4)
        end   = rd32(pt + page * 4)
        if end <= begin or end > len(d):
            continue
        o = next((c for c in objs if c["pi"] <= page < c["pi"] + c["pc"]), None)
        if o is None:
            continue
        page_off = (page - o["pi"]) * PAGE
        b = buf[o["base"]]
        pos = begin
        while pos < end:
            t = d[rt + pos]
            if t == 0x00:
                pos += 1
                continue
            if t == 0x02:
                if pos + 5 > end:
                    break
                srcoff = rd16(rt + pos + 2)
                if srcoff + 2 <= PAGE and page_off + srcoff + 2 <= len(b):
                    struct.pack_into("<H", b, page_off + srcoff, 0)
                pos += 5
                continue
            if t != 0x07:
                break
            b1 = d[rt + pos + 1]
            tsize = 2 + (b1 >> 4)
            if pos + 5 + tsize > end:
                break
            srcoff = rd16(rt + pos + 2)
            tobj   = d[rt + pos + 4]
            tgtoff = 0
            for i in range(tsize):
                tgtoff |= d[rt + pos + 5 + i] << (8 * i)
            if srcoff > PAGE - 1:
                cross += 1
                pos += 5 + tsize
                continue
            if page_off + srcoff + 4 <= len(b):
                struct.pack_into("<I", b, page_off + srcoff,
                                 (objs[tobj - 1]["base"] + tgtoff) & 0xFFFFFFFF)
                applied += 1
            pos += 5 + tsize

    # Pad/trim to the exact segment sizes the mirrors are declared with.
    out = []
    for base, size, name in WANT:
        if base not in buf:
            log("ERROR: object at 0x%X (%s) not found in %s" % (base, name, exe))
            sys.exit(1)
        seg = bytearray(size)
        n = min(len(buf[base]), size)
        seg[:n] = buf[base][:n]
        out.append((name, base, bytes(seg)))

    log("extracted %s (0x%X+0x%X) and %s (0x%X+0x%X); fixups applied=%d cross-page-skipped=%d"
        % (out[0][0], out[0][1], len(out[0][2]), out[1][0], out[1][1], len(out[1][2]),
           applied, cross), quiet)
    return out


def write_segments(segs, quiet=False):
    os.makedirs(ANAL, exist_ok=True)
    for name, _base, data in segs:
        path = SEG2 if name == "dseg02" else SEG3
        with open(path, "wb") as f:
            f.write(data)
        log("wrote %s (%d bytes)" % (os.path.relpath(path, ROOT), len(data)), quiet)


# --------------------------------------------------------- image emission ---
def carr(b):
    lines = []
    for i in range(0, len(b), 16):
        lines.append("  " + ",".join("0x%02X" % x for x in b[i:i+16]) + ",")
    return "\n".join(lines)


def bss_start(b):
    """First index of the trailing 0xFF run (the IDA export pads BSS with
    0xFF); == len(b) when there is none."""
    nz = len(b)
    while nz > 0 and b[nz-1] == 0xFF:
        nz -= 1
    return nz


def gen_image(seg2, seg3):
    out = [
        "/* fd2_image.c - GENERATED by tools/prepare_image.py - do not edit.",
        " * Standalone profile only: the hybrid profile (FD2_GUEST_FIXED) gets its",
        " * image from the guest loader instead, so this file compiles to nothing there. */",
        '#include "include/fd2_globals.h"',
        "#include <string.h>",
        "",
        "#ifndef FD2_GUEST_FIXED",
    ]
    bodies = []
    for name, base, b in (("dseg02", 0x50000, seg2), ("dseg03", 0x60000, seg3)):
        out.append("/* %s @0x%X : %d bytes */" % (name, base, len(b)))
        out.append("static const uint8_t fd2_%s_init[0x%X] = {"
                   % ("data1" if name == "dseg02" else "data2", len(b)))
        out.append(carr(b))
        out.append("};")
        out.append("uint8_t fd2_%s[0x%X];"
                   % ("data1" if name == "dseg02" else "data2", len(b)))
        out.append("")
        bodies.append((name, b))
    out += ["uint8_t fd2_lowmem[0x100000];", "", "void fd2_data_init(void)", "{"]
    for name, b in bodies:
        arr = "fd2_data1" if name == "dseg02" else "fd2_data2"
        out.append("    memcpy(%s, %s_init, sizeof(%s));" % (arr, arr, arr))
        nz = bss_start(b)
        if nz < len(b):
            out.append("    memset(%s + 0x%X, 0, sizeof(%s) - 0x%X);"
                       % (arr, nz, arr, nz))
    out += [
        "    /* fd2_lowmem: BDA mirror; BIOS tick counter at 0x46C (40:6C) */",
        "}",
        "#else /* FD2_GUEST_FIXED: image comes from the guest loader */",
        "void fd2_data_init(void) { /* guest loader owns image init */ }",
        "#endif",
        "void fd2_jumpout(uintptr_t ea) { (void)ea; /* phase-2: resolve shared tails */ }",
    ]
    return "\n".join(out) + "\n"


def write_image(seg2, seg3, quiet=False):
    with open(IMAGE, "w", encoding="utf-8", newline="\n") as f:
        f.write(gen_image(seg2, seg3))
    log("wrote src/fd2_image.c (%d bytes)"
        % os.path.getsize(IMAGE), quiet)


# -------------------------------------------------------------------- main ---
def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    # An empty FD2_EXE (e.g. `export FD2_EXE` in the top-level Makefile with
    # no value) must fall back to the default path, not become "".
    env_exe = os.environ.get("FD2_EXE") or None
    ap.add_argument("--exe", default=env_exe or os.path.join(ROOT, "FD2.EXE"),
                    help="path to the original FD2.EXE (default: <repo>/FD2.EXE)")
    ap.add_argument("--force", action="store_true",
                    help="re-extract the segments even if they already exist")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    have_segs = os.path.exists(SEG2) and os.path.exists(SEG3)
    if args.force or not have_segs:
        if not os.path.isfile(args.exe):
            sys.stderr.write(
                "\n"
                "prepare_image: ERROR: the data-segment image is missing and no\n"
                "  original executable was found.\n\n"
                "  The native build needs the game's dseg02/dseg03 bytes at runtime,\n"
                "  but they are copyrighted data and are not stored in this repo.\n\n"
                "  Fix: put your own copy of FD2.EXE (md5 %s) in\n"
                "       %s\n"
                "  (it is gitignored), or point at it with --exe / $FD2_EXE, then\n"
                "  rebuild.\n\n" % (FD2_MD5, os.path.relpath(os.path.join(ROOT, "FD2.EXE"), ROOT)))
            sys.exit(1)
        segs = extract_segments(args.exe, args.quiet)
        write_segments(segs, args.quiet)
    else:
        log("reusing existing %s and %s"
            % (os.path.relpath(SEG2, ROOT), os.path.relpath(SEG3, ROOT)), args.quiet)

    seg2 = open(SEG2, "rb").read()
    seg3 = open(SEG3, "rb").read()
    write_image(seg2, seg3, args.quiet)


if __name__ == "__main__":
    main()
