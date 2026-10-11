#!/usr/bin/env python3
# gen_headers.py - generate restoration scaffolding from analysis/ exports.
#
# Inputs  (analysis/): data_items.csv, prototypes.csv, strings.csv, decomp/*.c, seg_*.bin
# Outputs (src/):
#   include/fd2_globals.h  - segment images + access macro for every referenced data symbol
#                            (incl. synthesized unk_* for unnamed referenced items)
#   include/fd2_funcs.h    - prototype for every function
#   fd2_image.c            - image bytes + fd2_data_init()
#   fd2_relocate.c         - data-segment absolute code pointers -> real targets
#
# Macro shape is chosen from ACTUAL USAGE in the pseudocode corpus:
#   name[i]  -> pointer view   ((T *)&seg[off])
#   otherwise-> scalar lvalue  (*(T *)&seg[off])
#
# Phase-1 discipline: this only WRITES SOURCE. Nothing here is compiled or run.
import csv, os, re, struct, glob, collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AN = os.path.join(ROOT, 'analysis')
# Matches a function definition/declaration line and captures its name.
# Escape-free on purpose: '\b' etc. are avoided so the pattern survives
# every editing path (see tools/_fix_fd.py).
_FD_DEF_RE = re.compile('^(?:[ ]*)(?:static[ ]+)?(?:const[ ]+)?'
                        '[A-Za-z_][A-Za-z0-9_ *]*?'
                        '([A-Za-z_][A-Za-z0-9_]*)[ ]*[(]')

PORT = os.path.normpath(os.path.join(ROOT, '..', 'port'))
SRC = os.path.join(ROOT, 'src')
INC = os.path.join(SRC, 'include')

SEGMENTS = [
    ('cseg01', 0x10000, 'seg_cseg01.bin'),
    ('dseg02', 0x50000, 'seg_dseg02.bin'),
    ('dseg03', 0x60000, 'seg_dseg03.bin'),
]
ARR = {'cseg01': 'fd2_code', 'dseg02': 'fd2_data1', 'dseg03': 'fd2_data2'}
BASE = {s[0]: s[1] for s in SEGMENTS}
CODE_LO, CODE_HI = 0x10000, 0x4F000

def load_bin(fn):
    p = os.path.join(AN, fn)
    return open(p, 'rb').read() if os.path.exists(p) else b''

def seg_of(ea):
    if 0x10000 <= ea < 0x4F000: return 'cseg01'
    if 0x50000 <= ea < 0x556B0: return 'dseg02'
    if 0x60000 <= ea < 0x64000: return 'dseg03'
    return None

# ---------------------------------------------------------------- usage scan
def scan_usage():
    """-> dict name -> set(token-contexts) detecting indexed vs scalar use."""
    idx = collections.Counter()
    sca = collections.Counter()
    pat = re.compile(r'\b((?:dword|word|byte|qword|dbl|tbyte|off|funcs|jpt|asc|unk|tbl|str)_[0-9A-Fa-f]+|MEMORY)\b')
    for f in glob.glob(os.path.join(AN, 'decomp', '*.c')):
        txt = open(f, encoding='utf-8', errors='replace').read()
        for m in pat.finditer(txt):
            nm = m.group(1)
            nxt = txt[m.end():m.end() + 1]
            if nxt == '[':
                idx[nm] += 1
            else:
                sca[nm] += 1
    return idx, sca

# ---------------------------------------------------------------- item kinds
def base_type(name, strings_set, size):
    if name.startswith('funcs_'): return 'FUNC'
    if name.startswith('off_'):   return 'PTR'
    if name.startswith('jpt_'):   return 'JPT'
    import re as _re
    # ONE rule for string labels, shared with gen_globals.emit():
    # 'a' + [A-Z0-9_] (aFdotherDat / aRb_0 / a123456789abcde) or listed in strings.csv
    if name in strings_set or bool(_re.match(r'^a[A-Z0-9_]', name)):
        return 'STR' if name in strings_set else 'STR?'
    for p, t in (('qword_', 'uint64_t'), ('dbl_', 'double'), ('tbyte_', 'long double'),
                 ('dword_', 'uint32_t'), ('word_', 'uint16_t'), ('byte_', 'uint8_t'),
                 ('unk_', 'uint8_t')):
        if name.startswith(p): return t
    return 'uint8_t'

# ---------------------------------------------------------------- globals
def gen_globals(items, strings_set, unk_needed):
    os.makedirs(INC, exist_ok=True)
    idx, sca = scan_usage()
    # Migrated semantic names (c_names) are FUNCTIONS defined by the port/ modules;
    # never emit data macros for them (e.g. tables.c names its accessors tbl_6238D).
    # Names the migrated sources DEFINE as functions (incl. file-local statics like
    # unit_load.c's tbl_4E838) must never become data macros - the macro would
    # destroy the definition. Collect them from the actual sources.
    defined_funcs = set()
    for f in (glob.glob(os.path.join(SRC, 'game', '*.c')) +
              glob.glob(os.path.join(SRC, 'game', '*.h'))):
        for ln in open(f, encoding='utf-8', errors='replace'):
            if ln.lstrip().startswith(('#', '/*', '*', '//')):
                continue
            m = _FD_DEF_RE.match(ln)
            if m:
                defined_funcs.add(m.group(1))
    migrated_names = set()
    tmv = os.path.join(PORT, 're', 'translation_map.csv') if os.path.isdir(PORT) else None
    if tmv and os.path.exists(tmv):
        for r in list(csv.reader(open(tmv, encoding='utf-8')))[1:]:
            if len(r) > 1:
                migrated_names.add(r[1])
    # Verified-declaration overrides harvested from port/ (tools/extract_data_types.py):
    # port/ compiled against these exact types/access modes and passed differential
    # testing, so they win over corpus-usage heuristics.
    ov = {}
    ov_name = {}
    for name in ('data_type_overrides.csv', 'data_type_overrides.manual.csv'):
        ovp = os.path.join(AN, name)
        if os.path.exists(ovp):
            for r in list(csv.reader(open(ovp, encoding='utf-8')))[1:]:   # manual wins (later)
                if len(r) >= 4:
                    ov[r[0].strip().lower()] = (r[1], r[2], r[3])
                    ov_name[r[1]] = (r[2], r[3])   # by-symbol override (same addr, two names)
    # Symbols the migrated sources use but data_items.csv lacks (addr from hex suffix
    # or from the override row).
    extra = {}
    for f in (glob.glob(os.path.join(SRC, 'game', '*.c')) +
              glob.glob(os.path.join(SRC, 'game', '*.h'))):
        txt = open(f, encoding='utf-8', errors='replace').read()
        for nm in re.findall(r'\b((?:dword|word|byte|qword|unk|off|funcs|jpt|tbl|str|asc)_[0-9A-Fa-f]+)\b', txt):
            try:
                extra.setdefault(nm, int(nm.split('_', 1)[1], 16))
            except ValueError:
                pass
    for k, (nm, typ, mode) in ov.items():
        try:
            extra.setdefault(nm, int(k, 16))
        except ValueError:
            pass
    out = []
    out.append('/* fd2_globals.h - GENERATED by tools/gen_headers.py - do not edit.')
    out.append(' * Segment images mirror the LE layout. Every data symbol referenced by the')
    out.append(' * Hex-Rays corpus gets an access macro, so restored modules and pseudocode')
    out.append(' * alike resolve globals through ONE place.')
    out.append(' *   indexed use  -> pointer view   name[i]')
    out.append(' *   scalar use   -> lvalue view    *(T*)&seg[off]  (=name / name=x)')
    out.append(' * Unaligned uint32 (e.g. table @0x51B91) is fine on x86/-m32 builds.')
    out.append(' * memory-model: 32-bit pointer width assumed in phase 2 builds (see README §4).')
    out.append(' */')
    out.append('#ifndef FD2_GLOBALS_H')
    out.append('#define FD2_GLOBALS_H')
    out.append('#include <stdint.h>')
    out.append('#include <stddef.h>')
    out.append('')
    out.append('#ifdef FD2_GUEST_FIXED')
    out.append('/* ---- hybrid profile (assumption A, uncertainties.md §5) ----------')
    out.append(' * The regions are mapped at their ORIGINAL linear addresses by the guest')
    out.append(' * loader (port/src/le.c), so machine code and C observe the same bytes -')
    out.append(' * this keeps the port *check.c differential harnesses and repl.c usable')
    out.append(' * over port3-restored modules. Macro NAMES and offsets are')
    out.append(' * identical to the standalone profile; module sources do not change.')
    out.append(' * Function pointers inside the image are already fixed up by the loader.')
    out.append(' * FD2_GUEST_LOW_BASE: low-mem mirror base (BDA); default 0 = identity,')
    out.append(' * override to the hosts DOS_LOWMEM_BASE equivalent when needed.')
    out.append(' */')
    out.append('#ifndef FD2_GUEST_LOW_BASE')
    out.append('#define FD2_GUEST_LOW_BASE 0u')
    out.append('#endif')
    out.append('#define fd2_code    ((uint8_t *)(uintptr_t)0x00010000u)')
    out.append('#define fd2_data1   ((uint8_t *)(uintptr_t)0x00050000u)')
    out.append('#define fd2_data2   ((uint8_t *)(uintptr_t)0x00060000u)')
    out.append('#define fd2_lowmem  ((uint8_t *)(uintptr_t)FD2_GUEST_LOW_BASE)')
    out.append('#define MEMORY ((volatile uint8_t *)fd2_lowmem) /* IDA MEMORY[0x46C]=BDA tick */')
    out.append('#else /* standalone profile: ordinary arrays, no fixed mapping */')
    for sname, base, binf in SEGMENTS:
        size = len(load_bin(binf)) or 0x1000
        out.append('extern uint8_t %s[0x%X];  /* %s @0x%X, %d bytes */' % (ARR[sname], size, sname, base, size))
    out.append('extern uint8_t fd2_lowmem[0x100000]; /* real-mode IVT+BDA mirror */')
    out.append('#define MEMORY ((volatile uint8_t *)fd2_lowmem) /* IDA MEMORY[0x46C]=BDA tick */')
    out.append('#endif /* FD2_GUEST_FIXED */')
    out.append('')
    warns, dup = [], []
    skipped_odd = []
    seen = set()

    def emit(name, ea):
        if name in seen:
            dup.append(name); return
        # Non-prefixed odd names (p/buf/fp/mode/name/n/___begtext...) are NOT emitted:
        # short macros collide with local variables in restored modules. If a module
        # truly needs one (e.g. string labels name/mode in sub_10010), add a
        # module-local alias at restore time and log it in uncertainties/§3.5.
        std_prefixes = ('dword_', 'qword_', 'word_', 'byte_', 'dbl_', 'tbyte_',
                        'off_', 'funcs_', 'jpt_', 'asc_', 'unk_', 'tbl_', 'str_')
        import re as _re2
        is_string = (name in strings_set or
                     bool(_re2.match(r'^a[A-Z0-9_]', name)))   # IDA string labels: aWord, aRb_0, aFdotherDat
        # Short/common identifiers are NEVER emitted as macros: they collide with
        # parameter/local names in the migrated sources (uncertainties §3.5).
        SHORT_BLACKLIST = {
            'name', 'mode', 'buf', 'p', 'n', 'fp', 'len', 'i', 'j', 'k', 'x', 'y',
            'w', 'h', 'src', 'dst', 'out', 'in', 'data', 'size', 'count', 'index',
            'value', 'result', 'cur', 'ptr', 'tbl', 'end', 'start', 'off', 'msg',
        }
        if name in SHORT_BLACKLIST:
            skipped_odd.append(name + '(blacklist)')
            return
        if not (name.startswith(std_prefixes) or is_string):
            skipped_odd.append(name)
            return
        seen.add(name)
        if name in migrated_names or name in defined_funcs:
            return  # migrated function name, not a data cell
        seg = seg_of(ea)
        if seg is None:
            out.append('/* %s @0x%X outside known segments - manual */' % (name, ea))
            warns.append(name); return
        off = ea - BASE[seg]
        arr = ARR[seg]
        on = ov_name.get(name)
        if on is not None:
            otype, omode = on
            if omode == 'lvalue':
                m = '#define %s (*(%s *)&%s[0x%X])' % (name, otype, arr, off)
            elif omode in ('ptr', 'array') and '*' not in otype:
                otype = otype + ' *'          # byte_53D34[i] needs a byte POINTER view
                m = '#define %s ((%s)(uintptr_t)&%s[0x%X])' % (name, otype, arr, off)
            elif omode == 'array':
                m = '#define %s (*(%s (*)[1])&%s[0x%X])' % (name, otype, arr, off)
            else:
                m = '#define %s ((%s)(uintptr_t)&%s[0x%X])' % (name, otype, arr, off)
            out.append(m)
            return
        o = ov.get(('0x%X' % ea).lower())
        if o is not None:
            _oname, otype, omode = o
            if omode == 'lvalue':
                m = '#define %s (*(%s *)&%s[0x%X])' % (name, otype, arr, off)
            elif omode == 'array':
                # element-typed lvalue array of 1: supports BOTH `name[i]` and `&name`
                m = '#define %s (*(%s (*)[1])&%s[0x%X])' % (name, otype, arr, off)
            else:
                m = '#define %s ((%s)(uintptr_t)&%s[0x%X])' % (name, otype, arr, off)
            out.append(m)
            return
        t = base_type(name, strings_set, 0)
        indexed = idx.get(name, 0) > 0 and sca.get(name, 0) == 0
        both = idx.get(name, 0) > 0 and sca.get(name, 0) > 0
        if t == 'FUNC':
            m = '#define %s ((void (**)(void))&%s[0x%X])' % (name, arr, off)
        elif t == 'PTR':
            m = '#define %s (*(void **)&%s[0x%X])' % (name, arr, off)
        elif t == 'JPT':
            m = '#define %s ((const uint8_t *)&%s[0x%X])' % (name, arr, off)
        elif t == 'STR' or t == 'STR?':
            m = '#define %s ((const char *)&%s[0x%X])' % (name, arr, off)
            if t == 'STR?': warns.append(name)
        else:
            if indexed or both:
                m = '#define %s ((%s *)&%s[0x%X])' % (name, t, arr, off)
            else:
                m = '#define %s (*(%s *)&%s[0x%X])' % (name, t, arr, off)
            if both: warns.append(name + '(both[]and=)')
        if not name.startswith(('dword_', 'word_', 'byte_', 'qword_', 'dbl_', 'tbyte_',
                                'off_', 'funcs_', 'jpt_', 'asc_', 'unk_')) and name not in strings_set:
            m += '  /* TODO verify type */'
        out.append(m)

    for ea, name, seg, size in items:
        emit(name, int(ea, 16))
    for name, ea in sorted(unk_needed.items()):
        emit(name, ea)
    for nm, addr in sorted(extra.items(), key=lambda kv: kv[1]):
        if addr:
            emit(nm, addr)
    out.append('')
    out.append('#include "fd2_native.h" /* de-guest phase C: real pointer cells */')
    out.append('#endif /* FD2_GLOBALS_H */')
    open(os.path.join(INC, 'fd2_globals.h'), 'w', encoding='utf-8').write('\n'.join(out) + '\n')
    return len(seen), sorted(set(warns)), sorted(set(dup)) + ['skipped:' + n for n in sorted(set(skipped_odd))]

# ---------------------------------------------------------------- funcs
def normalize_type(t):
    t = t.strip()
    for a, b in [('unsigned __int8', 'uint8_t'), ('unsigned __int16', 'uint16_t'),
                 ('unsigned __int32', 'uint32_t'), ('unsigned __int64', 'uint64_t'),
                 ('signed __int8', 'int8_t'), ('__int8', 'int8_t'), ('__int16', 'int16_t'),
                 ('__int32', 'int32_t'), ('__int64', 'int64_t'), ('__int128', 'int64_t'),
                 ('_DWORD', 'uint32_t'), ('_WORD', 'uint16_t'), ('_BYTE', 'uint8_t'),
                 ('_QWORD', 'uint64_t'), ('bool', 'int'),
                 ('_UNKNOWN', 'void'),
                 ('__noreturn ', ''), ('__noreturn', ''),
                 ('__near ', ''), ('__far ', '')]:
        t = t.replace(a, b)
    return t.strip()

_TYPE_WORDS = {'int', 'char', 'short', 'long', 'float', 'double', 'void',
               'unsigned', 'signed', 'uint8_t', 'uint16_t', 'uint32_t', 'uint64_t',
               'int8_t', 'int16_t', 'int32_t', 'int64_t', 'size_t', 'time_t',
               'wchar_t', 'bool'}

def positional_args(args):
    """Drop parameter names: 'void *p, int a2, unsigned int' -> 'void *, int, unsigned int'.
    Prototypes must never collide with globals macros like #define p / #define buf."""
    inner = args.strip()
    if inner.startswith('('): inner = inner[1:]
    if inner.endswith(')'): inner = inner[:-1]
    if inner.strip() in ('', 'void'): return '(void)'
    parts, depth, cur = [], 0, ''
    for ch in inner:
        if ch in '<([': depth += 1
        elif ch in '>)]': depth -= 1
        if ch == ',' and depth == 0:
            parts.append(cur); cur = ''
        else:
            cur += ch
    parts.append(cur)
    outp = []
    for a in parts:
        a = a.strip()
        if not a or a == '...':
            outp.append(a or '...'); continue
        if '*' in a:
            import re as _re
            m = _re.match(r'^(.*\*\s*)(?:[A-Za-z_][A-Za-z0-9_]*)?$', a)
            a = m.group(1).strip() if m else a.rsplit(' ', 1)[0].strip()
        else:
            toks = a.split()
            if len(toks) >= 2 and toks[-1] not in _TYPE_WORDS:
                a = ' '.join(toks[:-1])
        outp.append(a)
    return '(' + ', '.join(outp) + ')'

def split_sig(sig):
    for cc in ('__cdecl', '__fastcall', '__stdcall', '__usercall', '__pascal'):
        if cc in sig:
            i = sig.index(cc)
            return sig[:i].strip(), sig[i + len(cc):].strip(), cc
    i = sig.find('(')
    if i > 0:
        return sig[:i].strip(), sig[i:].strip(), ''
    return sig, '()', ''

CRT_SKIP = {
    # provided by standard headers - declarations would collide
    'malloc', 'free', 'printf', 'sprintf', 'fprintf', 'snprintf', 'fopen', 'fclose',
    'freopen', 'fseek', 'ftell', 'fread', 'fwrite', 'fgetc', 'fputc', 'fputs',
    'fgets', 'setbuf', 'setvbuf', 'remove', 'rename', 'fflush', 'feof',
    'memmove', 'memset', 'memcpy', 'memcmp', 'strlen', 'strcmp', 'strncmp',
    'strcpy', 'strncpy', 'strcat', 'strchr', 'strstr', 'stricmp', 'strnicmp',
    'toupper', 'tolower', 'abs', 'labs', 'div', 'rand', 'srand', 'exit',
    'time', 'getenv', 'isatty', 'putchar', 'puts', 'scanf', 'sscanf', 'atoi',
    'strtod', 'strtol', 'strtoul', 'floor', 'ceil', 'pow', 'exp', 'log', 'log10',
    'sin', 'cos', 'tan', 'atan', 'atan2', 'sqrt', 'fabs', 'floor', 'modf',
    'frexp', 'ldexp', 'asctime', 'ctime', 'localtime', 'gmtime', 'mktime', 'tzset',
    'ungetc', 'calloc', 'realloc', 'perror', 'tmpfile', 'tmpnam',
    'main',  # defined by the restored entry module
}

# AIL library-boundary entries owned by src/lib/ail.h (correct Miles signatures);
# the IDA prototypes here have the stack-probe arity and would collide.
AIL_BOUNDARY_SKIP = {
    'AIL_init_sample', 'AIL_set_sample_address', 'AIL_set_sample_loop_count',
    'AIL_start_sample', 'AIL_stop_sample',
    'AIL_init_sequence', 'AIL_start_sequence', 'AIL_stop_sequence',
    'AIL_set_sequence_volume', 'AIL_set_sequence_loop_count',
}

# DOS/Watcom runtime boundary owned by src/lib/crt.h.
CRT_BOUNDARY_SKIP = { 'int386' }

def gen_funcs(protos, restored):
    os.makedirs(INC, exist_ok=True)
    out = []
    out.append('/* fd2_funcs.h - GENERATED by tools/gen_headers.py - do not edit.')
    out.append(' * Prototypes repaired from call-site evidence (analysis/arity.json).')
    out.append(' * - parameter names are positional only; return types are Hex-Rays guesses')
    out.append(' * - 145 CRT/AIL leftovers keep __usercall/__fastcall: marked MANUAL below')
    out.append(' * - raw pseudocode needs cleanup (@<reg> args, named args) before compiling')
    out.append(' */')
    out.append('#ifndef FD2_FUNCS_H')
    out.append('#define FD2_FUNCS_H')
    out.append('#include <stdint.h>')
    out.append('#include <stddef.h>')
    out.append('#include <stdbool.h>')
    out.append('#include <time.h>')
    out.append('struct exception; /* Watcom matherr payload, forward-declared for _matherr */')
    out.append('')
    out.append('/* shared tail-merge targets (JUMPOUT in pseudocode) */')
    out.append('void fd2_jumpout(uintptr_t ea);')
    out.append('')
    manual = []
    overrides = {}
    ov_path = os.path.join(AN, 'prototype_overrides.csv')
    if os.path.exists(ov_path):
        for r in list(csv.reader(open(ov_path, encoding='utf-8')))[1:]:
            if len(r) >= 3:
                overrides[r[0].strip().lower()] = (r[2], r[3] if len(r) > 3 else '')
    for addr, name, size, sig in protos:
        if name in CRT_SKIP or name in AIL_BOUNDARY_SKIP or name in CRT_BOUNDARY_SKIP or name in restored:
            continue  # standard header or restored module provides it
        # Watcom CRT/library internals keep their own header or are mapped to libc;
        # declaring them here collides with the platform CRT (MSVC _fsopen/unlink/...).
        if not re.match(r'^(sub_|nullsub_|j_|jpt_|funcs_|unk_|tbl_|str_|asc_|off_|dword_|word_|byte_|AIL_|fd2_)', name):
            continue
        if '@' in name:
            out.append('/* %s @ %s : %s  <- MANUAL (non-C identifier) */' % (name, addr, sig))
            manual.append(name); continue
        if re.match(r'^\s*\w+\s*\(\s*\*', sig):   # function-pointer-typed sig, e.g. 'int (*)(int)'
            out.append('/* %s @ %s : %s  <- MANUAL (fn-pointer sig) */' % (name, addr, sig))
            manual.append(name); continue
        if not sig:
            out.append('/* %s @ %s : NO PROTOTYPE - MANUAL */' % (name, addr)); manual.append(name)
            continue
        if '@<' in sig or '__spoils' in sig:
            out.append('/* %s @ %s : %s  <- MANUAL (register-arg ABI) */' % (name, addr, sig))
            manual.append(name); continue
        ov = overrides.get(addr.strip().lower())
        if ov is not None:
            ov_sig, ov_reason = ov
            ret, args, cc = split_sig(ov_sig)
            note = '  /* OVERRIDE: %s */' % (ov_reason or ov_sig)
        else:
            ret, args, cc = split_sig(sig)
            if cc not in ('__cdecl', ''):
                manual.append(name)
            note = '  /* %s */' % sig if cc not in ('__cdecl', '') else ''
        ret = normalize_type(ret)
        args = normalize_type(args).strip().replace('__cdecl', '').replace('__fastcall', '').replace('__stdcall', '')
        args = positional_args(args)
        if ret.endswith('*'):
            out.append('%s%s;%s' % (ret, name + args, note))
        else:
            out.append('%s %s%s;%s' % (ret, name, args, note))
    out.append('')
    out.append('#endif /* FD2_FUNCS_H */')
    open(os.path.join(INC, 'fd2_funcs.h'), 'w', encoding='utf-8').write('\n'.join(out) + '\n')
    return sorted(set(manual))

# ---------------------------------------------------------------- image
def gen_image(bins):
    def carr(b):
        lines = []
        for i in range(0, len(b), 16):
            lines.append('  ' + ','.join('0x%02X' % x for x in b[i:i+16]) + ',')
        return '\n'.join(lines)
    out = ['/* fd2_image.c - GENERATED by tools/gen_headers.py - do not edit.',
           ' * Standalone profile only: the hybrid profile (FD2_GUEST_FIXED) gets its',
           ' * image from the guest loader instead, so this file compiles to nothing there. */',
           '#include "include/fd2_globals.h"', '#include <string.h>', '',
           '#ifndef FD2_GUEST_FIXED']
    for sname, base, binf in SEGMENTS:
        b = bins[binf]; arr = ARR[sname]
        out.append('/* %s @0x%X : %d bytes */' % (sname, base, len(b)))
        if sname == 'cseg01':
            out.append('uint8_t %s[0x%X] = {' % (arr, len(b)))
            out.append(carr(b)); out.append('};')
        else:
            out.append('static const uint8_t %s_init[0x%X] = {' % (arr, len(b)))
            out.append(carr(b)); out.append('};')
            out.append('uint8_t %s[0x%X];' % (arr, len(b)))
        out.append('')
    out += ['uint8_t fd2_lowmem[0x100000];', '',
            'void fd2_data_init(void)', '{']
    for sname, base, binf in SEGMENTS:
        if sname != 'cseg01':
            out.append('    memcpy(%s, %s_init, sizeof(%s));' % (ARR[sname], ARR[sname], ARR[sname]))
            # BSS: the IDA export pads uninitialised data with 0xFF, but the LE
            # loader leaves it zero.  Find the trailing 0xFF run and zero it.
            b = bins[binf]; nz = len(b)
            while nz > 0 and b[nz-1] == 0xFF:
                nz -= 1
            bss = (nz // 0x1000 + 1) * 0x1000 if nz < len(b) else len(b)
            if bss < len(b):
                out.append('    memset(%s + 0x%X, 0, sizeof(%s) - 0x%X);' % (ARR[sname], bss, ARR[sname], bss))
    out += ['    /* fd2_lowmem: BDA mirror; BIOS tick counter at 0x46C (40:6C) */', '}',
            '#else /* FD2_GUEST_FIXED: image comes from the guest loader */',
            'void fd2_data_init(void) { /* guest loader owns image init */ }',
            '#endif',
            'void fd2_jumpout(uintptr_t ea) { (void)ea; /* phase-2: resolve shared tails */ }']
    open(os.path.join(SRC, 'fd2_image.c'), 'w', encoding='utf-8').write('\n'.join(out) + '\n')

# ---------------------------------------------------------------- relocate
def gen_relocate(protos, bins, restored_headers, funcmap_path):
    """Patch ONLY genuine function-start pointers. Other code-range VALUES in the
    data segments are data constants (0x10101/0x30303/0x20100 ...) - patching
    them would corrupt game data (caught by phase-5 validation 2026-01-10).
    Interior-pointer suspects are exported for review and left untouched."""
    func_by_addr = {int(a, 16): n for a, n, s, sig in protos}
    ranges = []
    try:
        for r in csv.reader(open(funcmap_path, encoding='utf-8')):
            if r and r[0][:2] == '0x':
                s0 = int(r[0], 16)
                ranges.append((s0, s0 + int(r[1]), r[2]))
    except Exception:
        pass
    def inside(v):
        for s, e, _n in ranges:
            if s < v < e: return True
        return False
    sites, consts, interior = [], [], []
    for sname, base, binf in SEGMENTS:
        if sname == 'cseg01': continue
        b = bins[binf]
        for off in range(0, len(b) - 3, 4):
            v = struct.unpack_from('<I', b, off)[0]
            if not (CODE_LO <= v < CODE_HI): continue
            if v in func_by_addr:
                sites.append((sname, off, v))
            elif inside(v):
                interior.append((sname, off, v))
            else:
                consts.append((sname, off, v))
    # audit sidecars
    with open(os.path.join(AN, 'reloc_suspects.csv'), 'w', newline='', encoding='utf-8') as fh:
        w = csv.writer(fh); w.writerow(['kind', 'seg', 'off', 'value'])
        for s, o, v in interior: w.writerow(['interior-ptr?', s, hex(o), hex(v)])
        for s, o, v in consts:   w.writerow(['const-untouched', s, hex(o), hex(v)])
    out = [
        '/* fd2_relocate.c - GENERATED by tools/gen_headers.py - do not edit.',
        ' *',
        ' * De-guest phase C2: data-segment code pointers are no longer patched in',
        ' * place.  The seven game dispatch tables are real host arrays',
        ' * (src/fd2_dispatch.c); the remaining code-range values live in cells that',
        ' * only machine code reads and a native build never executes.  The audit of',
        ' * every code-range data-segment value is still written to',
        ' * analysis/reloc_suspects.csv.',
        ' */',
        '#ifndef FD2_GUEST_FIXED',
        'void fd2_relocate(void) { }',
        '#else',
        'void fd2_relocate(void) { /* guest loader already applied fixups */ }',
        '#endif']
    open(os.path.join(SRC, 'fd2_relocate.c'), 'w', encoding='utf-8').write('\n'.join(out) + '\n')
    return len(sites), len(consts), len(interior), 0

# ---------------------------------------------------------------- main
def main():
    items = list(csv.reader(open(os.path.join(AN, 'data_items.csv'), encoding='utf-8')))[1:]
    protos = list(csv.reader(open(os.path.join(AN, 'prototypes.csv'), encoding='utf-8')))[1:]
    strs = list(csv.reader(open(os.path.join(AN, 'strings.csv'), encoding='utf-8')))[1:]
    strings_set = {int(r[0], 16) for r in strs}
    # restored-manifest: addr,name,header - these skip fd2_funcs.h (own header wins)
    restored = {}
    mf = os.path.join(SRC, 'restored_manifest.txt')
    if os.path.exists(mf):
        for ln in open(mf, encoding='utf-8'):
            ln = ln.strip()
            if ln and not ln.startswith('#'):
                a, n, h = ln.split()[:3]
                restored[n] = h
    # string-name map: addr -> name (for classification by content)
    addr_str = {int(r[0], 16) for r in strs}
    bins = {binf: load_bin(binf) for _, _, binf in SEGMENTS}

    # synthesized unk_* needed by corpus but unnamed in IDA
    unk_needed = {}
    pat = re.compile(r'\bunk_([0-9A-Fa-f]+)\b')
    have = {r[1] for r in items}
    for f in glob.glob(os.path.join(AN, 'decomp', '*.c')):
        for m in pat.finditer(open(f, encoding='utf-8', errors='replace').read()):
            nm = 'unk_' + m.group(1)
            if nm not in have:
                unk_needed[nm] = int(m.group(1), 16)
    # classify strings by presence in strings.csv
    item_by_name = {r[1]: int(r[0], 16) for r in items}
    str_names = {r[1] for r in items if int(r[0], 16) in addr_str}

    n, warns, dup = gen_globals(items, str_names, unk_needed)
    manual = gen_funcs(protos, restored)
    gen_image(bins)
    nfix = gen_relocate(protos, bins, sorted(set(restored.values())),
                        os.path.join(AN, 'funcmap.csv'))
    print('globals macros : %d   (verify-list: %d, dup skipped: %d)' % (n, len(warns), len(dup)))
    print('func protos    : %d   (MANUAL register-ABI: %d)' % (len(protos), len(manual)))
    print('unk synthesized: %d' % len(unk_needed))
    print('fixup slots    : %d (relocated fn-starts; see reloc_suspects.csv for the rest)' % (nfix[0] if isinstance(nfix, tuple) else nfix))
    if warns[:12]: print('verify names   :', warns[:12])
    if manual[:12]: print('manual funcs   :', manual[:12])

if __name__ == '__main__':
    main()
