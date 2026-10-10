#!/usr/bin/env python3
# migrate_port_assets.py - import the 309 verified translations from port/ into port3.
#
# Strategy (README §5: reuse, do not rewrite):
#   * copy port/src/game/*.{c,h} + a few port/src headers into port3/src/
#   * strip the port copies' `#define dword_xxx (*(T*)(uintptr_t)0xADDRu)` and
#     `((T)(uintptr_t)0xADDRu)` data macros; port3 routes data through fd2_globals.h
#     (types overridden from port/'s own declarations, tools/extract_data_types.py).
#     ORIG_*/RES_LOAD/... function-pointer constants stay (hybrid profile needs them).
#   * emit src/include/fd2_aliases.h (IDA name -> semantic name) for every c_name
#     that the module header actually declares (file-local statics are skipped).
#   * emit src/game/fd2_game.h umbrella.
#   * append translation_map rows to src/restored_manifest.txt (fd2_funcs.h skips them).
#
# Run from port3 root:  python tools/migrate_port_assets.py
import csv, os, re, shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT = os.path.normpath(os.path.join(ROOT, '..', 'port'))
SRC = os.path.join(ROOT, 'src')
GAME = os.path.join(SRC, 'game')
AN = os.path.join(ROOT, 'analysis')
NL = chr(10)

COPY = []
for fn in sorted(os.listdir(os.path.join(PORT, 'src', 'game'))):
    if fn.endswith(('.c', '.h')):
        COPY.append(('game/' + fn, 'game/' + fn))
for fn in ('dos.h', 'platform.h', 'le.h'):
    COPY.append((fn, fn))

# data-macro forms (both are stripped from the copies; fd2_globals.h replaces them)
DATA_MACRO = re.compile(r'^\s*#define\s+[A-Za-z_]\w*\s+\(\*\(.*uintptr_t\)0x[0-9A-Fa-f]+u?\)')
DATA_PTR = re.compile(r'^\s*#define\s+([A-Za-z_]\w*)\s+\(\(\s*[A-Za-z_][\w \*]*?\s*\)\s*\(uintptr_t\)0x[0-9A-Fa-f]+u?\)')
DATA_PREFIXES = ('dword_', 'word_', 'byte_', 'qword_', 'unk_', 'off_', 'funcs_',
                 'jpt_', 'tbl_', 'str_', 'asc_')
INCLUDE_LINE = re.compile(r'^\s*#include\s+["<]')

# Per-file adaptations applied AFTER the copy (idempotent, survive re-runs).
POST_PATCH = {
    'game/world_load.c': [
        ('#include "../include/fd2_globals.h"',
         '#include "../include/fd2_globals.h"' + NL + '#include "world_load_alias.inc"'),
    ],
}

def is_data_name(n):
    if n.startswith(DATA_PREFIXES):
        return True
    return len(n) > 1 and n[0] == 'a' and (n[1].isupper() or n[1].isdigit() or n[1] == '_')

def rewrite(path):
    lines = open(path, encoding='utf-8').read().splitlines()
    out, stripped = [], 0
    for ln in lines:
        if DATA_MACRO.match(ln):
            stripped += 1
            continue
        m = DATA_PTR.match(ln)
        if m and is_data_name(m.group(1)):
            stripped += 1
            continue
        out.append(ln)
    text = NL.join(out) + NL
    if stripped and '#include "../include/fd2_globals.h"' not in text:
        ls = text.splitlines()
        last = -1
        for i, ln in enumerate(ls):
            if INCLUDE_LINE.match(ln):
                last = i
        if last < 0:
            last = 0
            while last < len(ls) and not ls[last].startswith('*/'):
                if ls[last].startswith('/*') or ls[last].startswith(' *') or not ls[last].strip():
                    last += 1
                else:
                    break
        ls.insert(last + 1, '#include "../include/fd2_globals.h"')
        text = NL.join(ls) + NL
    open(path, 'w', encoding='utf-8').write(text)
    return stripped

def apply_post_patches():
    for rel, subs in POST_PATCH.items():
        path = os.path.join(SRC, rel)
        if not os.path.exists(path):
            continue
        txt = open(path, encoding='utf-8').read()
        for a, b in subs:
            if a in txt and 'world_load_alias.inc' not in txt:
                txt = txt.replace(a, b, 1)
        open(path, 'w', encoding='utf-8').write(txt)

def main():
    os.makedirs(GAME, exist_ok=True)
    tmap = list(csv.reader(open(os.path.join(PORT, 're', 'translation_map.csv'),
                                encoding='utf-8')))[1:]
    protos = {r[0].lower(): r[1] for r in
              csv.reader(open(os.path.join(AN, 'prototypes.csv'), encoding='utf-8'))}

    copied, stripped_total = 0, 0
    for rel_src, rel_dst in COPY:
        s = os.path.join(PORT, 'src', rel_src)
        d = os.path.join(SRC, rel_dst)
        if not os.path.exists(s):
            print('WARN missing', s)
            continue
        os.makedirs(os.path.dirname(d), exist_ok=True)
        shutil.copyfile(s, d)
        if rel_dst.endswith(('.c', '.h')):
            stripped_total += rewrite(d)
        copied += 1
    apply_post_patches()
    print('copied files      : %d' % copied)
    print('data macros stripped: %d' % stripped_total)

    aliases, manifest, skipped_static = [], [], []
    seen = set()
    for row in tmap:
        addr, c_name, source_file = row[0], row[1], row[2]
        ida = protos.get(addr.lower())
        base = os.path.basename(source_file)
        hdr = 'game/' + os.path.splitext(base)[0] + '.h'
        hpath = os.path.join(SRC, hdr)
        declared = False
        if os.path.exists(hpath):
            htxt = open(hpath, encoding='utf-8').read()
            declared = re.search(chr(92) + 'b' + re.escape(c_name) + chr(92) + 's*' + chr(92) + '(',
                                 htxt) is not None
        if ida and ida != c_name and (ida, c_name) not in seen:
            seen.add((ida, c_name))
            if declared:
                aliases.append('#define %-24s %s' % (ida, c_name))
            else:
                skipped_static.append('%s -> %s (%s)' % (ida, c_name, hdr))
        manifest.append('%s %s %s' % (addr, ida or c_name, hdr))

    os.makedirs(os.path.join(SRC, 'include'), exist_ok=True)
    with open(os.path.join(SRC, 'include', 'fd2_aliases.h'), 'w', encoding='utf-8') as fh:
        fh.write('/* fd2_aliases.h - GENERATED by tools/migrate_port_assets.py' + NL +
                 ' * Maps every migrated IDA name to its semantic C name, so call sites in' + NL +
                 ' * not-yet-adapted code keep working. 32-bit/guest assumptions unchanged. */' + NL +
                 '#ifndef FD2_ALIASES_H' + NL + '#define FD2_ALIASES_H' + NL + NL)
        fh.write(NL.join(sorted(aliases)))
        fh.write(NL + NL + '#endif /* FD2_ALIASES_H */' + NL)

    hdrs = sorted(f for f in os.listdir(GAME) if f.endswith('.h') and f != 'fd2_game.h')
    with open(os.path.join(GAME, 'fd2_game.h'), 'w', encoding='utf-8') as fh:
        fh.write('/* fd2_game.h - GENERATED umbrella for the migrated game modules.' + NL +
                 ' * Include this to see every verified asset + the fd2_* model + aliases. */' + NL +
                 '#ifndef FD2_GAME_UMBRELLA_H' + NL + '#define FD2_GAME_UMBRELLA_H' + NL + NL +
                 '#include "../include/fd2_globals.h"' + NL +
                 '#include "../include/fd2_funcs.h"' + NL +
                 '#include "../include/fd2_aliases.h"' + NL + NL)
        for h in hdrs:
            fh.write('#include "%s"' % h + NL)
        fh.write(NL + '#endif /* FD2_GAME_UMBRELLA_H */' + NL)

    mf = os.path.join(SRC, 'restored_manifest.txt')
    existing = set()
    for ln in open(mf, encoding='utf-8'):
        ln = ln.strip()
        if ln and not ln.startswith('#'):
            existing.add(ln.split()[0].lower())
    added = 0
    with open(mf, 'a', encoding='utf-8') as fh:
        for line in sorted(set(manifest)):
            if line.split()[0].lower() in existing:
                continue
            fh.write(line + NL)
            added += 1
    print('aliases           : %d' % len(set(aliases)))
    print('static-only skipped: %d' % len(skipped_static))
    for s in skipped_static[:6]:
        print('   skip', s)
    print('manifest added    : %d' % added)

if __name__ == '__main__':
    main()
