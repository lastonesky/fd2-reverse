#!/usr/bin/env python3
# gen_function_map.py - build function-map.csv (knowledge base, phase-1 deliverable)
# Merges: analysis/funcmap.csv + analysis/callgraph.csv + analysis/prototypes.csv
#         port/re/func_ranking.csv (zone/usage/wired) + port/re/translation_map.csv
#         src/restored_manifest.txt + analysis/prototype_heal_log.json (needs-analysis)
# Status vocabulary (task spec):
#   已确认且无需重写 / 已还原，尚未动态验证 / 待还原 / 暂时保留接口桩 /
#   需要进一步分析 / 已验证行为一致
import csv, os, json

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AN = os.path.join(ROOT, 'analysis')
PORT = os.path.normpath(os.path.join(ROOT, '..', 'port'))

def rd(path):
    with open(path, encoding='utf-8', newline='') as fh:
        return list(csv.reader(fh))

def key(a):
    return a.strip().lower()

funcmap = {key(r[0]): r for r in rd(os.path.join(AN, 'funcmap.csv'))[1:]}
callgraph = {key(r[0]): r for r in rd(os.path.join(AN, 'callgraph.csv'))[1:]}
protos = {key(r[0]): r for r in rd(os.path.join(AN, 'prototypes.csv'))[1:]}
try:
    ranking = {key(r[0]): r for r in rd(os.path.join(PORT, 're', 'func_ranking.csv'))[1:]}
except FileNotFoundError:
    ranking = {}
try:
    tmap = {key(r[0]): r for r in rd(os.path.join(PORT, 're', 'translation_map.csv'))[1:]}
except FileNotFoundError:
    tmap = {}

restored = {}
mf = os.path.join(ROOT, 'src', 'restored_manifest.txt')
for ln in open(mf, encoding='utf-8'):
    ln = ln.strip()
    if ln and not ln.startswith('#'):
        a, n, h = ln.split()[:3]
        restored[key(a)] = (n, h)

# functions whose prototypes needed healing or are flagged manual (from exports)
needs_analysis = {}
try:
    for ea, why in json.load(open(os.path.join(AN, 'prototype_heal_log.json')))['final_fails']:
        needs_analysis[key(ea)] = 'hex-rays 失败: %s' % ea
except Exception:
    pass
for r in rd(os.path.join(AN, 'prototypes.csv'))[1:]:
    sig = r[3]
    if '@<' in sig or '__spoils' in sig:
        needs_analysis.setdefault(key(r[0]), '寄存器 ABI 原型未解')

TOOLLIB_LO, TOOLLIB_HI = 0x4DED4, 0x4F000
def zone_of(ea_s, rank_zone):
    ea = int(ea_s, 16)
    if TOOLLIB_LO <= ea < TOOLLIB_HI: return 'toollib'   # game tool lib wins (RE_MAP §2.4)
    if rank_zone:
        return rank_zone
    if ea < 0x37000: return 'game'
    if 0x37D3E <= ea < 0x4D000: return 'ail'
    return 'crt'

def importance(usage, cg):
    try: u = int(usage)
    except Exception:
        u = (int(cg[4]) if len(cg) > 4 and cg[4].isdigit() else 0)
    if u >= 50: return '高'
    if u >= 10: return '中'
    if u >= 1: return '低'
    return '极低'

out = []
hdr = ['addr','size','orig_name','c_name','source_file','module_group','zone',
       'callers','callees','usage','restored','verified','status','importance',
       'confidence','prototype','evidence','notes']
rows_tmp = []
for ea in sorted(set(funcmap) | set(protos), key=lambda x: int(x, 16)):
    fm = funcmap.get(ea, [])
    cg = callgraph.get(ea, ['', '', '', ''])
    pr = protos.get(ea, ['', '', '', ''])
    rk = ranking.get(ea, [])
    tm = tmap.get(ea, [])
    size = fm[1] if fm else (pr[2] if pr else '')
    name = fm[2] if fm else (pr[1] if pr else cg[1])
    zone_rk = rk[3] if rk else ''
    zone = zone_of(ea, zone_rk)
    usage = rk[4] if rk else ''
    group = tm[3] if tm else ''
    c_name, src = (tm[1], tm[2]) if tm else ('', '')
    check, cases = (tm[4], tm[5]) if tm and len(tm) > 5 else ('', '')
    proto = pr[3] if pr else ''
    notes = []
    # ---- status decision ----
    if tm and tm[4] and tm[6].startswith('wired'):
        status = '已验证行为一致'
        restored_f, verified = '是', '是'
        ev = '%s %s 例对拍通过；repl.c 已接入' % (check, cases)
        conf = '高'
    elif ea in restored:
        status = '已还原，尚未动态验证'
        restored_f, verified = '是', '否'
        ev = 'port3 手工还原（%s）' % restored[ea][1]
        conf = '中高'
        c_name = c_name or restored[ea][0]
        src = src or ('src/' + restored[ea][1])
    elif zone in ('ail',):
        status = '暂时保留接口桩'
        restored_f, verified = '否', '否'
        ev = 'AIL 库边界（README §5）；16 个游戏入口见 RE_MAP §3.1'
        conf = '不适用'
        notes.append('桩/适配层')
    elif zone in ('crt', 'crt_sym'):
        status = '已确认且无需重写'
        restored_f, verified = '否(映射)', '否'
        ev = 'Watcom CRT → libc/宿主层映射（port/src/dos.c 先例）'
        conf = '不适用'
        notes.append('不逐条还原')
    elif zone == 'lib_nosym' and int(ea, 16) >= 0x37000:
        status = '暂时保留接口桩'
        restored_f, verified = '否', '否'
        ev = '库区未分类内部函数（CRT/AIL internals，RE_MAP §2）'
        conf = '不适用'
        notes.append('预期桩/映射，不逐条还原；归类待定')
    elif ea in needs_analysis:
        status = '需要进一步分析'
        restored_f, verified = '否', '否'
        ev = needs_analysis[ea]
        conf = '低'
    else:
        status = '待还原'
        restored_f, verified = '否', '否'
        ev = '伪代码已导出（analysis/decomp/）'
        conf = '中' if proto and 'cdecl' in proto else '中低'
    if ea == '0x367d1':
        status = '需要进一步分析'
        ev = '静态不可达（无调用/跳转/表引用）+ 反编译失败；反汇编兜底'
        conf = '低'
    imp = importance(usage, cg)
    if not usage:
        notes.append('usage 未测(按调用者数)')
    rows_tmp.append([ea, size, name, c_name, src, group, zone,
                cg[2] if len(cg) > 2 else '', cg[3] if len(cg) > 3 else '',
                usage, restored_f, verified, status, imp, conf, proto, ev,
                ';'.join(notes)])

# ---- module inference for unmapped game/toollib funcs (caller-group vote) ----
addr_name = {r[0]: (r[2] if r[2] else r[0]) for r in rows_tmp}
group_of = {r[0]: r[5] for r in rows_tmp}
callers_of = {r[0]: [c for c in r[7].split(';') if c] for r in rows_tmp}
def norm_addr(nm):
    for a, nn in addr_name.items():
        if nn == nm: return a
    return None
name_to_addr = {v: k for k, v in addr_name.items()}
inferred = 0
for r in rows_tmp:
    if r[5] or r[6] not in ('game', 'toollib'):
        continue
    votes = {}
    for cn in r[7].split(';'):
        if not cn: continue
        ca = name_to_addr.get(cn)
        g = group_of.get(ca) if ca else None
        if g: votes[g] = votes.get(g, 0) + 1
    if votes:
        g, n = max(votes.items(), key=lambda kv: kv[1])
        tot = sum(votes.values())
        if n / max(tot, 1) >= 0.7 and n >= 1:
            r[5] = g + ' (推断)'
            inferred += 1

out = rows_tmp

with open(os.path.join(ROOT, 'function-map.csv'), 'w', newline='', encoding='utf-8') as fh:
    w = csv.writer(fh); w.writerow(hdr); w.writerows(out)

from collections import Counter
import sys
out_cfg = sys.stdout if hasattr(sys.stdout, 'reconfigure') else None
try:
    sys.stdout.reconfigure(encoding='utf-8')
except Exception:
    pass
print('rows:', len(out), 'module inferred:', inferred)
print('status:', dict(Counter(r[12] for r in out)))
print('zone  :', dict(Counter(r[6] for r in out)))
print('groups:', len({r[5] for r in out if r[5]}))
