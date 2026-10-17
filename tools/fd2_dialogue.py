# -*- coding: utf-8 -*-
"""FD2 对白全文导出：FDTXT.DAT -> docs/dialogue.md + docs/dialogue.html

用法:
    python tools/fd2_dialogue.py [--text E:/FD2/FDTXT.DAT] [--outdir docs]

说话人:
- opcode -17/-18 的操作数是**角色 id**，名字 = res0 子流 [id+1]
  （box_popup.c: vm_run(dword_53A7D, r[8]+1)）。39 = FACE_KEEP，沿用上一位。
- opcode -19/-20 的操作数是**当前场景 80 字节记录表的槽位**（vm.c: dword_53A45 +
  80*idx），同一槽位在不同场景是不同人，无法静态定名，显示为「場景#N」。
"""
import argparse
import json
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fd2_fonttool as F  # noqa: E402

DEFAULT_TEXT = "E:/FD2/FDTXT.DAT"
FACE_KEEP = 39


def load_streams(path):
    """返回 ([(res, sub, text)], {id: name})。"""
    dat, _, _ = F.load_dat(path)
    mapping = F.load_mapping()

    names = {}
    res0 = dat[0]
    n0 = struct.unpack_from("<h", res0, 0)[0] // 2 if len(res0) >= 2 else 0
    for sub in range(1, n0):
        t = F.decode_ops(F.stream(dat, 0, sub, keep_ops=True), mapping)
        t = re.sub(r"\s+", "", t)
        if 1 <= len(t) <= 6 and not re.search(r"[『』\[\]]", t):
            names[sub - 1] = t

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
    return out, names


def clean_speech(txt):
    """一段子流 -> [{'k': 'id'|'slot'|'plain'|'keep', 'v': N, 't': 文本}]。"""
    parts = re.split(r"\{(頭像|單位):(\d+)\}", txt)
    speeches = []
    if parts[0].strip():
        speeches.append({"k": "plain", "v": None, "t": parts[0]})
    for i in range(1, len(parts), 3):
        kind = "id" if parts[i] == "頭像" else "slot"
        pid = int(parts[i + 1])
        body = re.sub(r"\s+", "", parts[i + 2])
        if kind == "id" and pid == FACE_KEEP:
            kind = "keep"
        speeches.append({"k": kind, "v": pid, "t": body})
    return speeches


def speaker_label(sp, names):
    """(显示名, 悬停说明, 颜色键)。"""
    k, v = sp["k"], sp["v"]
    if k == "plain":
        return None
    if k == "keep":
        return ("〃", "沿用上一位（FACE_KEEP=39）", "keep")
    if k == "id":
        nm = names.get(v)
        hint = f"角色 id {v}" + (f"（{nm}）" if nm else "（名字表无此项）")
        return (nm or f"#{v}", hint, v)
    return (f"場景#{v}", f"場景記錄表槽位 {v}（同一槽位在不同場景是不同人）", "slot")


def first_line(txt, n=26):
    sp = clean_speech(txt)
    s = sp[0]["t"] if sp else ""
    return re.sub(r"[『』\s]", "", s)[:n]


def md_escape_cell(s):
    return s.replace("|", "\\|")


def gen_markdown(idx, names, out_path):
    L = ["# FD2 对白全文（解码自 FDTXT.DAT）\n"]
    L.append("> 由 `tools/fd2_dialogue.py` 生成; 字形映射见 `tools/fd2data/glyphs.json`。\n"
             "> 说话人: `【名字】` = 角色 id 查到 res0 名字表（-17/-18）; "
             "`【場景#N】` = 场景记录表槽位（-19/-20, 同一槽位在不同场景是不同人, 无法静态定名）。\n"
             "> `{子文本}`/`{num}` 为运行期替换的占位符。\n")
    dialog_res = [r for r in sorted(idx) if r != 0]
    L.append("## 场景索引\n")
    L.append("| 资源 | 段数 | 开头 |")
    L.append("|---|---:|---|")
    for r in dialog_res:
        subs = idx[r]
        L.append(f"| res{r} | {len(subs)} | {md_escape_cell(first_line(subs[0][1]))} |")
    L.append("")
    for r in dialog_res:
        subs = idx[r]
        L.append(f"\n## res{r}（{len(subs)} 段）\n")
        for sub, txt in subs:
            L.append(f"### sub{sub}\n")
            prev = None
            for sp in clean_speech(txt):
                if sp["k"] == "plain":
                    L.append(f"> {sp['t']}\n")
                    continue
                lbl = speaker_label(sp, names)
                if sp["k"] == "keep" and prev:
                    lbl = (prev[0], lbl[1], lbl[2])
                prev = lbl
                L.append(f"**【{lbl[0]}】**{sp['t']}\n")
    if 0 in idx:
        L.append("\n## 附：res0 字符串表（人名/职业/物品等）\n")
        L.append("| sub | 内容 |")
        L.append("|---:|---|")
        for sub, txt in idx[0]:
            L.append(f"| {sub} | {md_escape_cell(re.sub(r'\\s+', '', txt))} |")
    open(out_path, "w", encoding="utf-8", newline="\n").write("\n".join(L))
    print(f"wrote {out_path}")


PALETTE = ["#c62828", "#1565c0", "#2e7d32", "#6a1b9a", "#ef6c00", "#00838f",
           "#ad1457", "#4e342e", "#546e7a", "#b71c1c", "#283593", "#1b5e20",
           "#0277bd", "#8e24aa", "#5d4037", "#00695c"]


def gen_html(idx, names, out_path):
    scenes, speakers, slots = [], {}, {}
    for r in sorted(idx):
        if r == 0:
            continue
        blocks = []
        for sub, txt in idx[r]:
            sps, prev = [], None
            for sp in clean_speech(txt):
                lbl = speaker_label(sp, names)
                if lbl is None:
                    sps.append({"k": "plain", "t": sp["t"]})
                    continue
                if sp["k"] == "keep" and prev:
                    lbl = (prev[0], lbl[1], lbl[2])
                prev = lbl
                sps.append({"k": sp["k"], "name": lbl[0], "hint": lbl[1],
                            "ck": lbl[2], "t": sp["t"]})
                if sp["k"] == "id":
                    speakers.setdefault(sp["v"], {"name": lbl[0], "n": 0})
                    speakers[sp["v"]]["n"] += 1
                elif sp["k"] == "slot":
                    slots[sp["v"]] = slots.get(sp["v"], 0) + 1
            blocks.append({"sub": sub, "sp": sps})
        scenes.append({"res": r, "n": len(blocks), "head": first_line(idx[r][0][1]),
                       "blocks": blocks})
    terms = [{"sub": sub, "t": re.sub(r"\s+", "", txt)} for sub, txt in idx.get(0, [])]

    data = json.dumps({"scenes": scenes, "terms": terms, "speakers": speakers,
                       "slots": slots, "total": sum(s["n"] for s in scenes)},
                      ensure_ascii=False)

    doc = """<!DOCTYPE html>
<html lang="zh-Hant">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="format-detection" content="telephone=no">
<meta name="theme-color" content="#ffffff">
<title>FD2 对白全文</title>
<style>
:root { --bg:#fafafa; --fg:#222; --muted:#777; --line:#e0e0e0; --card:#fff; --accent:#1565c0; }
* { box-sizing:border-box; }
body { margin:0; font:14px/1.7 "Microsoft JhengHei","PingFang TC",sans-serif; color:var(--fg); background:var(--bg);
       -webkit-text-size-adjust:100%; }
#wrap { display:flex; height:100vh; height:100dvh; }
#mbar, #scrim { display:none; }
#side { width:250px; overflow-y:auto; border-right:1px solid var(--line); background:var(--card); padding:12px 10px 40px; flex:none; }
#side h1 { font-size:15px; margin:0 0 10px; }
#side h4 { font-size:12px; color:var(--muted); margin:14px 0 4px; font-weight:600; }
#side a.nav { display:block; padding:3px 6px; color:var(--fg); text-decoration:none; border-radius:4px; font-size:12px; }
#side a.nav:hover { background:#eef4fb; }
#side a.nav .n { color:var(--muted); font-size:11px; float:right; }
.sp { display:flex; align-items:center; gap:4px; padding:2px 6px; border-radius:4px; font-size:12px; cursor:pointer; }
.sp:hover { background:#eef4fb; }
.sp.on { background:#e3f2fd; font-weight:600; }
.sp .n { color:var(--muted); font-size:11px; margin-left:auto; }
#main { flex:1; overflow-y:auto; padding:14px 26px 80px; }
#bar { position:sticky; top:0; background:var(--bg); padding:8px 0; z-index:5; display:flex; gap:10px; align-items:center; }
#q { flex:0 0 330px; padding:6px 10px; font-size:14px; border:1px solid var(--line); border-radius:6px; }
#cnt { color:var(--muted); font-size:12px; }
#clear { font-size:12px; color:var(--accent); cursor:pointer; display:none; }
.scene { margin-top:18px; }
.scene > h2 { font-size:16px; border-bottom:2px solid var(--accent); padding-bottom:4px; margin:0 0 8px; }
.block { background:var(--card); border:1px solid var(--line); border-radius:8px; padding:8px 12px; margin:10px 0; }
.block .id { color:#bbb; font-size:11px; }
.row { display:flex; gap:8px; margin:3px 0; }
.badge { flex:none; display:inline-block; min-width:62px; max-width:96px; text-align:center; color:#fff;
         border-radius:4px; font-size:12px; padding:1px 5px; height:22px; overflow:hidden; text-overflow:ellipsis; white-space:nowrap; cursor:pointer; }
.badge.slot { background:#9e9e9e !important; }
.badge.keep { background:#bdbdbd !important; }
.txt { flex:1; }
.plain { color:var(--muted); }
mark { background:#ffe082; }
table.terms { border-collapse:collapse; font-size:13px; }
table.terms td { border:1px solid var(--line); padding:2px 8px; }
.hidden { display:none !important; }
#legend { border:1px solid var(--line); border-radius:8px; background:var(--card); padding:8px 14px; margin:4px 0 10px; font-size:12px; color:var(--muted); }

/* ---------- 手机 / 窄屏 ---------- */
@media (max-width: 820px) {
  body { font-size:15px; }
  #mbar { display:flex; align-items:center; gap:10px; position:sticky; top:0; z-index:40;
          background:var(--card); border-bottom:1px solid var(--line);
          padding:8px 12px; padding-top:calc(8px + env(safe-area-inset-top)); }
  #mbar b { font-size:15px; font-weight:600; }
  #mbar small { color:var(--muted); font-size:12px; margin-left:auto; }
  #menu { font-size:18px; line-height:1; border:1px solid var(--line); background:var(--card);
          color:var(--fg); border-radius:6px; padding:4px 10px; cursor:pointer; }
  #wrap { display:block; height:auto; }
  #side { position:fixed; top:0; left:0; bottom:0; width:84vw; max-width:320px; z-index:60;
          padding-top:calc(12px + env(safe-area-inset-top));
          padding-bottom:calc(40px + env(safe-area-inset-bottom));
          transform:translateX(-103%); transition:transform .22s ease; box-shadow:2px 0 16px rgba(0,0,0,.18); }
  body.navopen #side { transform:none; }
  #scrim { position:fixed; inset:0; background:rgba(0,0,0,.35); z-index:55; }
  body.navopen #scrim { display:block; }
  #main { flex:none; overflow:visible; padding:10px 12px calc(60px + env(safe-area-inset-bottom)); }
  #bar { position:static; flex-wrap:wrap; gap:6px; padding:6px 0 8px; }
  #q { flex:1 1 100%; width:100%; }
  #legend { font-size:11px; padding:7px 10px; }
  .scene { margin-top:16px; scroll-margin-top:56px; }
  .block { padding:7px 10px; margin:8px 0; }
  .row { gap:6px; }
  .badge { min-width:0; max-width:none; font-size:11px; height:20px; padding:1px 5px; }
  table.terms { width:100%; font-size:12px; }
  table.terms td { padding:3px 6px; }
}
</style>
</head>
<body>
<div id="mbar">
  <button id="menu" aria-label="目录">☰</button>
  <b>FD2 对白</b>
  <small id="mtot"></small>
</div>
<div id="scrim"></div>
<div id="wrap">
  <div id="side">
    <h1>FD2 对白</h1>
    <div id="speakers"></div>
    <h4>场景</h4>
    <div id="nav"></div>
  </div>
  <div id="main">
    <div id="bar">
      <input id="q" placeholder="搜索对白…（卡蘿那 / 女神 / 黑森林）">
      <span id="cnt"></span><span id="clear">清除筛选</span>
    </div>
    <div id="legend">说话人来自 opcode <b>-17/-18</b> 的角色 id（名字表 = res0[id+1]）；
      <b>場景#N</b> 来自 <b>-19/-20</b>，是当前场景记录表槽位，同一槽位在不同场景是不同人，故不作人名。</div>
    <div id="content"></div>
  </div>
</div>
<script>
const DATA = __DATA__;
const PAL = __PALETTE__;
const color = ck => (typeof ck === "number") ? PAL[ck % PAL.length] : "#9e9e9e";
let onlySpeaker = null, kw = "";

function esc(s){return String(s).replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;");}

/* ---- sidebar: speakers ---- */
const spBox = document.getElementById("speakers");
const order = Object.keys(DATA.speakers).map(Number).sort((a,b)=>a-b);
spBox.innerHTML = "<h4>说话人（点选筛选）</h4>";
order.forEach(id=>{
  const s = DATA.speakers[id];
  const d = document.createElement("div");
  d.className = "sp"; d.dataset.sp = id;
  d.innerHTML = `<span class="badge" style="background:${color(id)}">${esc(s.name)}</span><span class="n">${s.n}</span>`;
  d.onclick = ()=>{ onlySpeaker = (onlySpeaker===id)?null:id; apply(); closeNav(); };
  spBox.appendChild(d);
});

/* ---- sidebar: scenes ---- */
const nav = document.getElementById("nav");
DATA.scenes.forEach(sc=>{
  const a=document.createElement("a");
  a.className="nav"; a.href="#res"+sc.res;
  a.innerHTML=`res${sc.res} <span class="n">${sc.n}</span>`;
  a.onclick = ()=> closeNav();
  nav.appendChild(a);
});

/* ---- mobile drawer ---- */
function closeNav(){ document.body.classList.remove("navopen"); }
document.getElementById("menu").onclick = ()=> document.body.classList.toggle("navopen");
document.getElementById("scrim").onclick = closeNav;
document.getElementById("mtot").textContent =
  Object.keys(DATA.speakers).length + " 人 / " + DATA.total + " 段";

/* ---- content ---- */
const content = document.getElementById("content");
DATA.scenes.forEach(sc=>{
  const sec=document.createElement("div"); sec.className="scene"; sec.id="res"+sc.res;
  sec.innerHTML=`<h2>res${sc.res} <span style="color:var(--muted);font-size:12px;font-weight:400">${sc.n} 段</span></h2>`;
  sc.blocks.forEach(b=>{
    const d=document.createElement("div"); d.className="block"; d.dataset.res=sc.res; d.dataset.sub=b.sub;
    let h=`<div class="id">sub${b.sub}</div>`;
    b.sp.forEach(s=>{
      if(s.k==="plain"){ h+=`<div class="row"><span class="txt plain">${esc(s.t)}</span></div>`; return; }
      const cls = (s.k==="slot")?"slot":(s.k==="keep"?"keep":"");
      h+=`<div class="row"><span class="badge ${cls}" style="background:${color(s.ck)}" title="${esc(s.hint)}" data-sp="${s.k==='id'?s.ck:''}">${esc(s.name)}</span><span class="txt">${esc(s.t)}</span></div>`;
    });
    d.innerHTML=h; sec.appendChild(d);
  });
  content.appendChild(sec);
});
/* clicking a badge filters to that speaker */
content.addEventListener("click", e=>{
  const b = e.target.closest(".badge");
  if(!b || b.dataset.sp==="") return;
  const id = Number(b.dataset.sp);
  onlySpeaker = (onlySpeaker===id)?null:id; apply();
});

/* ---- terms ---- */
const terms=document.createElement("div"); terms.className="scene"; terms.id="res0";
let th='<h2>附：res0 字符串表 <span style="color:var(--muted);font-size:12px;font-weight:400">'
      + DATA.terms.length + ' 条（名字表 = [id+1]，高 id 为单位/职业名）</span></h2><table class="terms">';
DATA.terms.forEach(t=>{ th+=`<tr><td>${t.sub}</td><td>${esc(t.t)}</td></tr>`; });
terms.innerHTML=th+"</table>"; content.appendChild(terms);

/* ---- filter ---- */
const q=document.getElementById("q"), cnt=document.getElementById("cnt"), clr=document.getElementById("clear");
function clearMarks(){ document.querySelectorAll("mark").forEach(m=>{ m.outerHTML = m.innerHTML; }); }
function apply(){
  clearMarks();
  document.querySelectorAll(".sp").forEach(e=>e.classList.toggle("on", onlySpeaker!==null && +e.dataset.sp===onlySpeaker));
  const active = kw!=="" || onlySpeaker!==null;
  let hit=0, total=0;
  document.querySelectorAll(".block").forEach(bl=>{
    total++;
    let ok = true;
    if(onlySpeaker!==null){
      ok = [...bl.querySelectorAll(".badge")].some(x=>x.dataset.sp!=="" && +x.dataset.sp===onlySpeaker);
    }
    if(ok && kw!=="") ok = bl.textContent.includes(kw);
    bl.classList.toggle("hidden", !ok);
    if(ok){
      hit++;
      if(kw!==""){
        const re=new RegExp(kw.replace(/[.*+?^${}()|[\\]\\\\]/g,"\\\\$&"),"g");
        bl.querySelectorAll(".txt").forEach(t=>{
          if(t.innerHTML.includes(kw)) t.innerHTML=t.innerHTML.replace(re,"<mark>"+kw+"</mark>");
        });
      }
    }
  });
  document.querySelectorAll(".scene").forEach(sec=>{
    const any = [...sec.querySelectorAll(".block")].some(b=>!b.classList.contains("hidden"));
    sec.classList.toggle("hidden", active && !any);
  });
  cnt.textContent = active ? `${hit}/${total} 段命中` : "";
  clr.style.display = active ? "inline" : "none";
}
q.addEventListener("input", ()=>{ kw=q.value.trim(); apply(); });
clr.onclick = ()=>{ q.value=""; kw=""; onlySpeaker=null; apply(); };
</script>
</body>
</html>"""
    doc = doc.replace("__DATA__", data).replace("__PALETTE__", json.dumps(PALETTE))
    open(out_path, "w", encoding="utf-8", newline="\n").write(doc)
    print(f"wrote {out_path}")


def main(argv=None):
    ap = argparse.ArgumentParser(description="FD2 对白全文导出")
    ap.add_argument("--text", default=DEFAULT_TEXT)
    ap.add_argument("--outdir", default="docs")
    a = ap.parse_args(argv)
    os.makedirs(a.outdir, exist_ok=True)
    streams, names = load_streams(a.text)
    idx = {}
    for ri, sub, txt in streams:
        idx.setdefault(ri, []).append((sub, txt))
    n_dialog = sum(len(v) for k, v in idx.items() if k != 0)
    print(f"streams={len(streams)} (对白 {n_dialog} 段, 名字表 {len(names)} 条)")
    gen_markdown(idx, names, os.path.join(a.outdir, "dialogue.md"))
    gen_html(idx, names, os.path.join(a.outdir, "dialogue.html"))


if __name__ == "__main__":
    main()
