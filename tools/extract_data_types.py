#!/usr/bin/env python3
# extract_data_types.py - harvest data-symbol declarations from the verified port/ sources.
#
# port/ declares each game data symbol it touches, in one of two forms:
#   A lvalue : #define name (*(TYPE *)(uintptr_t)0xADDRu)   -> value of the object
#   B pointer: #define name ((TYPE)(uintptr_t)0xADDRu)      -> typed pointer to it
# Both encode the exact access the verified code was compiled against, which is
# stronger evidence than guessing from the Hex-Rays corpus. This script emits
#   analysis/data_type_overrides.csv   addr,name,type,mode,source
# for the globals generator to consume, plus the set of symbols that only the
# port sources know about (referenced but absent from data_items.csv).
import csv, os, re, glob

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT = os.path.normpath(os.path.join(ROOT, '..', 'port'))
AN = os.path.join(ROOT, 'analysis')

RE_A = re.compile(r'^\s*#define\s+(\w+)\s+\(\*\(\s*([^()]+?)\s*\*\)\s*\(uintptr_t\)\s*0x([0-9A-Fa-f]+)u?\s*\)')
RE_B = re.compile(r'^\s*#define\s+(\w+)\s+\(\(\s*([A-Za-z_][\w \*]*?)\s*\)\s*\(uintptr_t\)\s*0x([0-9A-Fa-f]+)u?\s*\)')
DATA_PREFIX = ('dword_', 'word_', 'byte_', 'qword_', 'unk_', 'off_', 'funcs_',
               'jpt_', 'tbl_', 'str_', 'asc_')
DATA_RANGE = ((0x50000, 0x556B0), (0x60000, 0x64000), (0x10000, 0x4F000))

def in_data_range(a):
    return any(lo <= a < hi for lo, hi in DATA_RANGE)

def main():
    rows, by_addr = [], {}
    # ONLY the module sources (port/src/game/**), never the check harnesses
    # (port/src/*check.c re-declare the same addresses with their own local types).
    files = glob.glob(os.path.join(PORT, 'src', 'game', '*.c')) + \
            glob.glob(os.path.join(PORT, 'src', 'game', '*.h'))
    decls = {}   # addr -> list of (name, type, mode, src)
    for f in files:
        src = os.path.basename(f)
        for ln in open(f, encoding='utf-8', errors='replace'):
            m = RE_A.match(ln) or RE_B.match(ln)
            if not m:
                continue
            name, typ, addr = m.group(1), m.group(2).strip(), int(m.group(3), 16)
            mode = 'lvalue' if RE_A.match(ln) else 'ptr'
            is_data_name = name.startswith(DATA_PREFIX) or \
                (name.startswith('a') and len(name) > 1 and name[1].isupper())
            # Only IDA-style DATA labels qualify. Uppercase macro names like
            # ORIG_VIEW / RES_LOAD / AIL_* are file-local function-pointer
            # constants (code addresses), not data cells - never turn them into
            # fd2_globals macros (they would collide with the migrated defines).
            if not is_data_name:
                continue
            decls.setdefault(addr, []).append((name, typ, mode, src))
    for addr, ds in sorted(decls.items()):
        # Different files legitimately declared the same cell differently in port/
        # (each TU had its own macro). One port3 macro must serve all users, so
        # prefer a POINTER type (permissive: pointer arithmetic and uintptr_t
        # casts both work) and the 'lvalue' mode when a pointer lvalue exists.
        def score(d):
            name, typ, mode, src = d
            ptr = '*' in typ
            return (1 if ptr else 0, 1 if mode == 'lvalue' else 0)
        best = max(ds, key=score)
        note = ''
        if len(ds) > 1:
            kinds = {d[2] + '/' + d[1] for d in ds}
            if len(kinds) > 1:
                note = 'conflict: ' + '; '.join('%s/%s(%s)' % (d[2], d[1], d[3]) for d in ds[:4])
        rows.append(['0x%X' % addr, best[0], best[1], best[2], best[3], note])
    with open(os.path.join(AN, 'data_type_overrides.csv'), 'w', newline='', encoding='utf-8') as fh:
        w = csv.writer(fh)
        w.writerow(['addr', 'name', 'type', 'mode', 'source', 'note'])
        w.writerows(rows)
    lv = sum(1 for r in rows if r[3] == 'lvalue')
    print('type overrides: %d (lvalue %d / ptr %d)' % (len(rows), lv, len(rows) - lv))
    conf = [r for r in rows if r[5]]
    if conf:
        print('conflicts:', len(conf))
        for r in conf[:5]:
            print('  ', r)

if __name__ == '__main__':
    main()
