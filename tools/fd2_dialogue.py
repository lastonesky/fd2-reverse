# -*- coding: utf-8 -*-
"""FD2 对白全文导出：FDTXT.DAT -> docs/dialogue.md + docs/dialogue.html

用法:
    python tools/fd2_dialogue.py [--text E:/FD2/FDTXT.DAT] [--outdir docs]

说明:
- 文本按资源(res)/子流(sub)组织; res0 是名字/术语字符串表, res1..res33 是
  场景对白。每个 {頭像:N} 标记一次说话人切换, N 是游戏内头像编号
  (与角色名的对应表未在文本数据中, 暂以编号显示)。
- 生成物: Markdown(便于 git 阅读) + 单文件 HTML(侧栏导航 + 全文搜索)。
"""
import argparse
import html
import json
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fd2_fonttool as F  # noqa: E402

DEFAULT_TEXT = "E:/FD2/FDTXT.DAT"


def load_streams(path):
    """返回 [(res, sub, text)], 过滤空流。"""
    dat, _, _ = F.load_dat(path)
    mapping = F.load_mapping()
    out = []
    for ri, S in enumerate(dat):
        if len(S) < 2:
            continue
        first = struct.unpack_from("<h", S, 0)[0]
        nsub = (first // 2) if first > 0 else 0
        for sub in range(nsub):
            if 2 * sub + 2 > len(S):
                break
            ops = F.stream(dat, ri, sub, keep_ops=True)
            if not ops:
                continue
            txt = F.decode_ops(ops, mapping)
            if txt.strip():
                out.append((ri, sub, txt))
    return out


def clean_speech(txt):
    """把一段子流整理成若干条发言 [(portrait, text)]。

    - {頭像:N} 分割发言人; 段内换行(游戏 12 字换行)合并为一句。
    - 去掉标点/引号后多余空格。
    """
    parts = re.split(r"\{頭像:(\d+|Ｔ)\}", txt)
    speeches = []
    # parts: [前导, id, 文本, id, 文本...]
    if parts[0].strip():
        speeches.append((None, parts[0]))
    for i in range(1, len(parts), 2):
        pid = parts[i]
        body = parts[i + 1]
        # 合并游戏内换行(用于 320px 屏幕的手动折行)
        body = body.replace("\n", "")
        # 清理折行产生的多余空格: 仅保留词间必要空格(中文文本里空格都是折行残留)
        body = re.sub(r"\s+", "", body) if not re.search(r"[A-Za-z0-9]\s+[A-Za-z0-9]", body) else body
        speeches.append((pid, body))
    return speeches


def first_line(txt, n=24):
    s = clean_speech(txt)
    if s and s[0][1]:
        s = s[0][1]
    else:
        s = "".join(t for _, t in s)
    s = re.sub(r"[『』\s]", "", s)
    return s[:n]


def build_index(streams):
    """[(res, sub, text)] -> {res: [(sub, text)]}"""
    idx = {}
    for ri, sub, txt in streams:
        idx.setdefault(ri, []).append((sub, txt))
    return idx


def md_escape_cell(s):
    return s.replace("|", "\\|")


def gen_markdown(idx, out_path):
    L = []
    L.append("# FD2 对白全文（解码自 FDTXT.DAT）\n")
    L.append("> 由 `tools/fd2_dialogue.py` 生成; 字形映射见 `tools/fd2data/glyphs.json`。\n"
             "> `{頭像:N}` 为说话人头像编号（编号与角色名的对应关系不在文本数据里, 暂以编号显示, 详见 HTML 版头像表）。\n"
             "> `{子文本}`/`{num}` 为运行期替换的占位符。\n")
    dialog_res = [r for r in sorted(idx) if r != 0]
    L.append("## 场景索引\n")
    L.append("| 资源 | 段数 | 开头 |")
    L.append("|---|---:|---|")
    for r in dialog_res:
        subs = idx[r]
        head = md_escape_cell(first_line(subs[0][1]))
        L.append(f"| res{r} | {len(subs)} | {head} |")
    L.append("")
    for r in dialog_res:
        subs = idx[r]
        L.append(f"\n## res{r}（{len(subs)} 段）\n")
        for sub, txt in subs:
            L.append(f"### sub{sub}\n")
            for pid, body in clean_speech(txt):
                if pid is None:
                    L.append(f"> {body}\n")
                else:
                    L.append(f"**【頭像 {pid}】**{body}\n")
    if 0 in idx:
        L.append("\n## 附：res0 字符串表（人名/职业/物品等）\n")
        L.append("| sub | 内容 |")
        L.append("|---:|---|")
        for sub, txt in idx[0]:
            body = md_escape_cell(re.sub(r"\s+", "", txt))
            L.append(f"| {sub} | {body} |")
    open(out_path, "w", encoding="utf-8", newline="\n").write("\n".join(L))
    print(f"wrote {out_path}")


BADGE_COLORS = ["#c62828", "#1565c0", "#2e7d32", "#6a1b9a", "#ef6c00", "#00838f",
                "#ad1457", "#4e342e", "#546e7a", "#b71c1c", "#283593", "#1b5e20"]


def gen_html(idx, out_path):
    dialog_res = [r for r in sorted(idx) if r != 0]
    scenes = []
    for r in dialog_res:
        subs = idx[r]
        blocks = []
        for sub, txt in subs:
            sp = []
            for pid, body in clean_speech(txt):
                sp.append({"p": pid, "t": body})
            blocks.append({"sub": sub, "sp": sp})
        scenes.append({"res": r, "n": len(subs),
                       "head": first_line(subs[0][1]), "blocks": blocks})
    terms = [{"sub": sub, "t": re.sub(r"\s+", "", txt)} for sub, txt in idx.get(0, [])]

    # 头像表: 每个头像编号取前两条发言样本
    portraits = {}
    for sc in scenes:
        for b in sc["blocks"]:
            for s in b["sp"]:
                if s["p"] is None:
                    continue
                d = portraits.setdefault(s["p"], {"n": 0, "samples": []})
                d["n"] += 1
                if len(d["samples"]) < 2:
                    d["samples"].append(s["t"][:40])

    data = json.dumps({"scenes": scenes, "terms": terms, "portraits": portraits},
                      ensure_ascii=False)

    html_doc = """<!DOCTYPE html>
<html lang="zh-Hant">
<head>
<meta charset="utf-8">
<title>FD2 对白全文</title>
<style>
:root { --bg:#fafafa; --fg:#222; --muted:#777; --line:#ddd; --card:#fff; --accent:#1565c0; }
* { box-sizing:border-box; }
body { margin:0; font:14px/1.65 "Microsoft JhengHei","PingFang TC",sans-serif; color:var(--fg); background:var(--bg); }
#wrap { display:flex; height:100vh; }
#side { width:260px; overflow-y:auto; border-right:1px solid var(--line); background:var(--card); padding:10px; flex:none; }
#side h1 { font-size:15px; margin:4px 0 8px; }
#side a { display:block; padding:4px 6px; color:var(--fg); text-decoration:none; border-radius:4px; font-size:13px; }
#side a:hover { background:#eef4fb; }
#side a .n { color:var(--muted); font-size:11px; float:right; }
#main { flex:1; overflow-y:auto; padding:14px 26px 80px; max-width:900px; }
#bar { position:sticky; top:0; background:var(--bg); padding:8px 0; z-index:2; }
#q { width:340px; padding:6px 10px; font-size:14px; border:1px solid var(--line); border-radius:6px; }
#cnt { color:var(--muted); margin-left:10px; font-size:12px; }
.scene { margin-top:18px; }
.scene > h2 { font-size:16px; border-bottom:2px solid var(--accent); padding-bottom:4px; }
.block { background:var(--card); border:1px solid var(--line); border-radius:8px; padding:8px 12px; margin:10px 0; }
.block .id { color:var(--muted); font-size:11px; }
.badge { display:inline-block; min-width:52px; text-align:center; color:#fff; border-radius:4px; font-size:12px; padding:1px 6px; margin-right:6px; }
.plain { color:var(--muted); }
mark { background:#ffe082; }
table.terms { border-collapse:collapse; font-size:13px; }
table.terms td { border:1px solid var(--line); padding:2px 8px; }
#pt { margin-top:14px; border:1px solid var(--line); border-radius:8px; background:var(--card); padding:10px 14px; }
#pt h3 { margin:4px 0 8px; font-size:14px; }
#pt .row { margin:2px 0; font-size:12px; }
.hidden { display:none; }
</style>
</head>
<body>
<div id="wrap">
  <div id="side"><h1>FD2 对白</h1><div id="nav"></div></div>
  <div id="main">
    <div id="bar">
      <input id="q" placeholder="搜索对白…（如：卡蘿那 / 女神）">
      <span id="cnt"></span>
    </div>
    <div id="pt"><h3>头像编号 → 说话样本（编号↔人名对应不在文本数据中，凭样本自行对应）</h3><div id="ptbody"></div></div>
    <div id="content"></div>
  </div>
</div>
<script>
const DATA = __DATA__;
const COLORS = __COLORS__;
function esc(s){return s.replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;");}
const nav = document.getElementById("nav");
DATA.scenes.forEach(sc=>{
  const a=document.createElement("a");
  a.href="#res"+sc.res;
  a.innerHTML=`res${sc.res} <span class="n">${sc.n}</span><br><span style="color:var(--muted);font-size:11px">${esc(sc.head)}</span>`;
  nav.appendChild(a);
});
const ptb=document.getElementById("ptbody");
Object.keys(DATA.portraits).map(Number).sort((a,b)=>a-b).forEach(id=>{
  const p=DATA.portraits[id];
  const d=document.createElement("div"); d.className="row";
  d.innerHTML=`<span class="badge" style="background:${color(id)}">頭像 ${id}</span>×${p.n} — ${esc(p.samples.join(" / "))}`;
  ptb.appendChild(d);
});
function color(id){return COLORS[id % COLORS.length];}
const content=document.getElementById("content");
DATA.scenes.forEach(sc=>{
  const sec=document.createElement("div"); sec.className="scene"; sec.id="res"+sc.res;
  sec.innerHTML=`<h2>res${sc.res} <span style="color:var(--muted);font-size:12px">${sc.n} 段 · 开头：${esc(sc.head)}</span></h2>`;
  sc.blocks.forEach(b=>{
    const d=document.createElement("div"); d.className="block";
    let h=`<div class="id">sub${b.sub}</div>`;
    b.sp.forEach(s=>{
      if(s.p===null) h+=`<div class="plain">${esc(s.t)}</div>`;
      else h+=`<div><span class="badge" style="background:${color(+s.p)}">頭像 ${esc(s.p)}</span>${esc(s.t)}</div>`;
    });
    d.innerHTML=h; sec.appendChild(d);
  });
  content.appendChild(sec);
});
const terms=document.createElement("div"); terms.className="scene"; terms.id="res0";
let th='<h2>附：res0 字符串表（人名/职业/物品等）</h2><table class="terms">';
DATA.terms.forEach(t=>{ th+=`<tr><td>${t.sub}</td><td>${esc(t.t)}</td></tr>`; });
terms.innerHTML=th+"</table>"; content.appendChild(terms);
const q=document.getElementById("q"), cnt=document.getElementById("cnt");
q.addEventListener("input",()=>{
  const kw=q.value.trim();
  let total=0, hit=0;
  document.querySelectorAll(".block").forEach(el=>{total++;});
  if(!kw){ document.querySelectorAll(".block,.scene").forEach(e=>e.classList.remove("hidden"));
           document.querySelectorAll("mark").forEach(m=>{m.outerHTML=m.innerHTML;});
           cnt.textContent=""; return; }
  document.querySelectorAll(".scene").forEach(sec=>{
    let any=false;
    sec.querySelectorAll(".block").forEach(bl=>{
      const txt=bl.textContent;
      const ok=txt.includes(kw);
      bl.classList.toggle("hidden",!ok);
      bl.querySelectorAll("mark").forEach(m=>{m.outerHTML=m.innerHTML;});
      if(ok){ any=true; hit++;
        const re=new RegExp(kw.replace(/[.*+?^${}()|[\\]\\\\]/g,"\\\\$&"),"g");
        bl.querySelectorAll("div:not(.id)").forEach(dv=>{
          if(dv.textContent.includes(kw)) dv.innerHTML=dv.innerHTML.replace(re,"<mark>"+kw+"</mark>");
        });
      }
    });
    sec.classList.toggle("hidden",!any);
  });
  cnt.textContent=`${hit}/${total} 段命中`;
});
</script>
</body>
</html>"""
    html_doc = html_doc.replace("__DATA__", data).replace("__COLORS__", json.dumps(BADGE_COLORS))
    open(out_path, "w", encoding="utf-8", newline="\n").write(html_doc)
    print(f"wrote {out_path}")


def main(argv=None):
    ap = argparse.ArgumentParser(description="FD2 对白全文导出")
    ap.add_argument("--text", default=DEFAULT_TEXT)
    ap.add_argument("--outdir", default="docs")
    a = ap.parse_args(argv)
    os.makedirs(a.outdir, exist_ok=True)
    streams = load_streams(a.text)
    idx = build_index(streams)
    n_dialog = sum(len(v) for k, v in idx.items() if k != 0)
    print(f"streams={len(streams)} (对白 {n_dialog} 段, res0 字符串 {len(idx.get(0, []))} 条)")
    gen_markdown(idx, os.path.join(a.outdir, "dialogue.md"))
    gen_html(idx, os.path.join(a.outdir, "dialogue.html"))


if __name__ == "__main__":
    main()
