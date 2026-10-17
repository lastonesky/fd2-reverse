#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""fd2sav_gui.py — FD2（炎龙骑士团 2）存档修改器 · 图形界面

核心逻辑全部复用 tools/fd2sav.py（解密 / 校验 / 记录字段 / 派生属性重算），
本文件只负责交互：区域列表、角色表格、字段编辑、批量满血、备份回滚。

运行需要 tkinter。它不是 pip 包（是 CPython 编译期绑定的 _tkinter + Tcl/Tk），
装不上；但系统里的 CPython 3.12 已经自带：
    C:\\Users\\Ted\\AppData\\Roaming\\uv\\python\\cpython-3.12-windows-x86_64-none\\pythonw.exe
直接双击 tools\\FD2GUI.BAT 即可（BAT 会挑一个带 tkinter 的解释器）。

设计原则：
  * 所有改动先落在内存缓冲，点「保存到存档」才写盘，写盘自动留时间戳备份；
  * 任何一次写盘都重算尾部校验和（util_sum_tail4）；
  * 改基础 AP/DP/DX 自动重算派生值（unit_recalc），否则战斗伤害不生效。
"""
from __future__ import annotations

import os
import shutil
import subprocess
import sys
import time
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fd2sav as core                                     # noqa: E402

FONT = ("Microsoft YaHei UI", 9)
FONT_B = ("Microsoft YaHei UI", 9, "bold")
FONT_T = ("Microsoft YaHei UI", 10, "bold")
MONO = ("Consolas", 9)

COLS = [
    ("idx",  "#",     40, "center", False),
    ("name", "名字", 100, "w",      True),
    ("id",   "id",    40, "center", False),
    ("lv",   "Lv",    42, "center", False),
    ("hp",   "HP",   104, "e",      True),
    ("mp",   "MP",   104, "e",      True),
    ("ap",   "AP",    50, "e",      False),
    ("dp",   "DP",    50, "e",      False),
    ("dx",   "DX",    50, "e",      False),
    ("note", "备注",  90, "w",      True),
]


def _enable_dpi():
    """让 Tk 在高分屏上不发虚（必须在 Tk() 之前）。"""
    try:
        import ctypes
        try:
            ctypes.windll.shcore.SetProcessDpiAwareness(1)
        except Exception:
            ctypes.windll.user32.SetProcessDPIAware()
    except Exception:
        pass


def _s16(rec, off):
    v = rec[off] | (rec[off + 1] << 8)
    return v - 0x10000 if v >= 0x8000 else v


class App(tk.Tk):
    def __init__(self, path=None, silent=False):
        super().__init__()
        self.silent = silent                # selftest 用：不弹窗
        try:
            self.tk.call("tk", "scaling", self.winfo_fpixels("1i") / 72.0)
        except tk.TclError:
            pass

        self.title("FD2 存档修改器")
        try:
            self.S = max(1.0, self.winfo_fpixels("1i") / 96.0)   # DPI 比例
        except tk.TclError:
            self.S = 1.0
        self.minsize(int(940 * self.S), int(640 * self.S))
        self.geometry(f"{int(1120 * self.S)}x{int(740 * self.S)}")

        self.path = None
        self.edit = None
        self.names = {}                     # 记录 +0x08 -> 名字（FDTXT.DAT）
        self.region_name = "head"
        self.cur = None                     # 当前选中记录下标
        self._loading = False

        self._build()
        self.protocol("WM_DELETE_WINDOW", self.on_close)

        if path:
            self.load_save(path)
        else:
            self._boot()
        self._center()

    # ------------------------------------------------------------------ UI
    def _style(self):
        st = ttk.Style(self)
        for theme in ("vista", "winnative", "clam"):
            try:
                st.theme_use(theme)
                break
            except tk.TclError:
                continue
        st.configure(".", font=FONT)
        st.configure("Treeview", rowheight=int(25 * self.S), font=FONT,
                     fieldbackground="white")
        st.configure("Treeview.Heading", font=FONT_B)
        st.configure("TLabelframe.Label", font=FONT_B)
        st.configure("Big.TButton", font=FONT_T)

    def _build(self):
        self._style()
        self.columnconfigure(0, weight=1)
        self.rowconfigure(1, weight=1)

        # ---- 顶栏 ------------------------------------------------------
        top = ttk.Frame(self, padding=(10, 8, 10, 4))
        top.grid(row=0, column=0, sticky="ew")
        top.columnconfigure(1, weight=1)
        ttk.Label(top, text="存档", font=FONT_B).grid(row=0, column=0, sticky="w")
        self.path_var = tk.StringVar(value="（未载入）")
        ttk.Entry(top, textvariable=self.path_var, state="readonly").grid(
            row=0, column=1, sticky="ew", padx=(8, 8))
        ttk.Button(top, text="打开…", width=8, command=self.ask_open).grid(row=0, column=2, padx=2)
        ttk.Button(top, text="重新载入", width=10, command=self.reload).grid(row=0, column=3, padx=2)
        ttk.Button(top, text="备份 / 回滚", width=12, command=self.open_backups).grid(row=0, column=4, padx=(2, 0))

        # ---- 中部：左区域列表 + 右表格 ---------------------------------
        pw = ttk.Panedwindow(self, orient="horizontal")
        pw.grid(row=1, column=0, sticky="nsew", padx=10)

        left = ttk.Frame(pw)
        ttk.Label(left, text="区域", font=FONT_B).pack(anchor="w", pady=(0, 4))
        self.rv = ttk.Treeview(left, columns=("n", "info", "wnd"), show="headings",
                               height=10, selectmode="browse")
        self.rv.heading("n", text="区域")
        self.rv.heading("info", text="内容")
        self.rv.heading("wnd", text="残")
        self.rv.column("n", width=int(104 * self.S), anchor="w", stretch=False)
        self.rv.column("info", width=int(96 * self.S), anchor="w", stretch=True)
        self.rv.column("wnd", width=int(38 * self.S), anchor="center", stretch=False)
        self.rv.tag_configure("wound", foreground="#b03020")
        self.rv.tag_configure("empty", foreground="#9a9a9a")
        self.rv.pack(fill="both", expand=False)
        self.rv.bind("<<TreeviewSelect>>", self._on_region_select)

        self.show_empty_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(left, text="显示空位记录", variable=self.show_empty_var,
                        command=self.refresh_table).pack(anchor="w", pady=(8, 0))
        ttk.Label(left, text="提示：继续游戏的\n队伍表与名单表是\n两份独立副本，\n「整个存档满血」\n可一次改全。",
                  foreground="#666", justify="left").pack(anchor="w", pady=(10, 0))
        pw.add(left, weight=0)

        right = ttk.Frame(pw)
        right.columnconfigure(0, weight=1)
        right.rowconfigure(1, weight=1)
        pw.add(right, weight=1)

        self.head_var = tk.StringVar(value="")
        ttk.Label(right, textvariable=self.head_var, font=FONT_T).grid(
            row=0, column=0, sticky="w", pady=(0, 4))

        self.tv = ttk.Treeview(right, columns=[c[0] for c in COLS],
                               show="headings", selectmode="browse")
        for key, text, width, anchor, stretch in COLS:
            self.tv.heading(key, text=text)
            self.tv.column(key, width=int(width * self.S), anchor=anchor,
                           stretch=stretch)
        self.tv.tag_configure("wound", foreground="#b03020")
        self.tv.tag_configure("empty", foreground="#9a9a9a")
        self.tv.grid(row=1, column=0, sticky="nsew")
        tv_sb = ttk.Scrollbar(right, orient="vertical", command=self.tv.yview)
        self.tv.configure(yscrollcommand=tv_sb.set)
        tv_sb.grid(row=1, column=1, sticky="ns")
        self.tv.bind("<<TreeviewSelect>>", self._on_record_select)
        self.tv.bind("<Double-1>", self._on_double)

        self._build_editor(right).grid(row=2, column=0, columnspan=2,
                                       sticky="ew", pady=(8, 0))

        # ---- 底部操作条 ------------------------------------------------
        bot = ttk.Frame(self, padding=(10, 6, 10, 4))
        bot.grid(row=2, column=0, sticky="ew")
        bot.columnconfigure(9, weight=1)

        ttk.Label(bot, text="金钱", font=FONT_B).grid(row=0, column=0, sticky="w")
        self.money_var = tk.StringVar()
        self.money_spin = ttk.Spinbox(bot, from_=0, to=999999999,
                                      textvariable=self.money_var, width=12, font=FONT)
        self.money_spin.grid(row=0, column=1, padx=(4, 6))
        self.btn_money_set = ttk.Button(bot, text="设为", width=6,
                                        command=lambda: self.do_money(False))
        self.btn_money_set.grid(row=0, column=2)
        self.btn_money_add = ttk.Button(bot, text="加上", width=6,
                                        command=lambda: self.do_money(True))
        self.btn_money_add.grid(row=0, column=3, padx=(2, 0))

        ttk.Separator(bot, orient="vertical").grid(row=0, column=4, sticky="ns", padx=10)

        ttk.Button(bot, text="本区域满血满魔", command=self.heal_region).grid(row=0, column=5, padx=2)
        ttk.Button(bot, text="整个存档满血满魔", command=self.heal_all).grid(row=0, column=6, padx=2)

        self.btn_revert = ttk.Button(bot, text="放弃改动", command=self.do_revert)
        self.btn_revert.grid(row=0, column=10, padx=(0, 6), sticky="e")
        self.btn_save = tk.Button(bot, text="保存到存档", command=self.do_save,
                                  font=FONT_T, bg="#2f7d32", fg="white",
                                  activebackground="#3c9440", activeforeground="white",
                                  relief="flat", padx=18, pady=4, cursor="hand2")
        self.btn_save.grid(row=0, column=11, sticky="e")

        # ---- 状态栏 ----------------------------------------------------
        self.status = tk.Label(self, text="", anchor="w", bg="#eceff1", fg="#37474f",
                               font=FONT, padx=12, pady=5)
        self.status.grid(row=3, column=0, sticky="ew")

    def _build_editor(self, parent):
        box = ttk.LabelFrame(parent, text="选中角色", padding=(10, 8))
        box.columnconfigure(20, weight=1)

        self.var_lv = tk.StringVar()
        self.var_hp = tk.StringVar()
        self.var_mhp = tk.StringVar()
        self.var_mp = tk.StringVar()
        self.var_mmp = tk.StringVar()
        self.var_ap = tk.StringVar()
        self.var_dp = tk.StringVar()
        self.var_dx = tk.StringVar()
        for v in (self.var_lv, self.var_hp, self.var_mhp, self.var_mp, self.var_mmp,
                  self.var_ap, self.var_dp, self.var_dx):
            v.trace_add("write", self._on_field_change)

        def lbl(text, r, c, bold=False):
            ttk.Label(box, text=text, font=FONT_B if bold else FONT).grid(
                row=r, column=c, sticky="w", padx=(0, 3))

        def spin(var, r, c, w=6, hi=9999):
            sp = ttk.Spinbox(box, from_=0, to=hi, textvariable=var, width=w, font=FONT)
            sp.grid(row=r, column=c, sticky="w", padx=(0, 12))
            return sp

        # row 0: Lv / HP / MP
        lbl("Lv", 0, 0, True)
        spin(self.var_lv, 0, 1, 4, 255)
        lbl("HP", 0, 2, True)
        spin(self.var_hp, 0, 3)
        lbl("/", 0, 4)
        self.sp_mhp = spin(self.var_mhp, 0, 5)
        lbl("MP", 0, 6, True)
        spin(self.var_mp, 0, 7)
        lbl("/", 0, 8)
        self.sp_mmp = spin(self.var_mmp, 0, 9)

        # row 1: 基础属性 + 派生值 + 回满
        lbl("基础", 1, 0, True)
        lbl("AP", 1, 1)
        spin(self.var_ap, 1, 2, 5)
        lbl("DP", 1, 3)
        spin(self.var_dp, 1, 4, 5)
        lbl("DX", 1, 5)
        spin(self.var_dx, 1, 6, 5)

        self.derived_var = tk.StringVar(value="派生 —")
        ttk.Label(box, textvariable=self.derived_var, foreground="#1565c0",
                  font=FONT).grid(row=1, column=7, columnspan=8, sticky="w", padx=(4, 10))

        ttk.Button(box, text="回满此角色", command=self.heal_one).grid(
            row=1, column=16, columnspan=5, sticky="e")

        box.bind("<Enter>", lambda _e: None)
        self.editor = box
        self._set_editor_state(False)
        return box

    def _set_editor_state(self, on):
        state = "normal" if on else "disabled"
        for w in self.editor.winfo_children():
            if isinstance(w, (ttk.Spinbox, ttk.Button)):
                try:
                    w.configure(state=state)
                except tk.TclError:
                    pass

    # -------------------------------------------------------------- 载入
    def _boot(self):
        try:
            p = core.find_save(quiet=True)
        except SystemExit:
            p = None
        if p:
            self.load_save(p)
        else:
            self.set_status("没有自动找到 FD2.SAV —— 点「打开…」手动选择")

    def load_save(self, path):
        try:
            ed = core.Edit(path)
        except SystemExit as e:
            self._error("打不开存档", str(e))
            return False
        except OSError as e:
            self._error("打不开存档", f"{path}\n{e}")
            return False

        self.path = path
        self.edit = ed
        self.names = core.name_table(path)
        self.cur = None
        self.path_var.set(path)
        self.refresh_regions()
        self.select_region("head")
        self.refresh_table()

        if not core.checksum_ok(ed.buf):
            self._warn("校验和不匹配",
                       "这份存档的尾部校验和与内容对不上。\n"
                       "可能是存档损坏，或之前被别的工具改过。\n"
                       "仍然可以编辑，保存时会重新写入正确的校验和。")
        self.update_status()
        return True

    def ask_open(self):
        if not self._confirm_discard():
            return
        init = os.path.dirname(self.path) if self.path else os.getcwd()
        p = filedialog.askopenfilename(
            title="选择 FD2 存档", initialdir=init,
            filetypes=[("FD2 存档", "*.SAV *.sav"), ("所有文件", "*.*")])
        if p:
            self.load_save(os.path.abspath(p))

    def reload(self):
        if not self.path:
            return
        if not self._confirm_discard():
            return
        self.load_save(self.path)

    # -------------------------------------------------------------- 区域
    def select_region(self, name):
        self.region_name = name
        if str(name) in self.rv.get_children():
            self.rv.selection_set(str(name))
        self.cur = None
        self.refresh_table()

    def _on_region_select(self, _evt=None):
        if self._loading:
            return
        sel = self.rv.selection()
        if not sel:
            return
        name = sel[0]
        if name == self.region_name:
            return
        self.region_name = name
        self.cur = None
        self.refresh_table()

    @property
    def reg(self):
        if not self.edit:
            return None
        return core.Region(self.edit.buf, self.region_name)

    def refresh_regions(self):
        if not self.edit:
            return
        self._loading = True
        self.rv.delete(*self.rv.get_children())
        for name in core.REGIONS:
            reg = core.Region(self.edit.buf, name)
            label = core.REGION_LABEL[name]
            if name not in ("head", "roster") and reg.empty():
                self.rv.insert("", "end", iid=name, values=(label, "空槽", ""),
                               tags=("empty",))
                continue
            real = [i for i in range(reg.max_recs) if core.is_real_char(reg.rec(i))]
            wnd = sum(1 for i in real
                      if core.u16(reg.rec(i), 0x40) < core.u16(reg.rec(i), 0x42))
            money = reg.money
            info = f"{len(real)} 人" + (f" · {money:,}" if money is not None else "")
            self.rv.insert("", "end", iid=name, values=(label, info, f"{wnd} 残" if wnd else ""),
                           tags=("wound",) if wnd else ())
        self._loading = False

    def _idxs(self, reg):
        out = []
        for i in range(reg.max_recs):
            r = reg.rec(i)
            if core.is_real_char(r):
                out.append(i)
            elif self.show_empty_var.get() and any(bytes(r)):
                out.append(i)
        return out

    def refresh_table(self):
        if not self.edit:
            return
        self._loading = True
        self.tv.delete(*self.tv.get_children())
        reg = self.reg
        empty_slot = (self.region_name not in ("head", "roster") and reg.empty())
        if not empty_slot:
            for i in self._idxs(reg):
                r = reg.rec(i)
                hp, mhp = core.u16(r, 0x40), core.u16(r, 0x42)
                mp, mmp = core.u16(r, 0x44), core.u16(r, 0x46)
                real = core.is_real_char(r)
                if not real:
                    note, tags = "空位", ("empty",)
                elif hp < mhp:
                    note, tags = "残血", ("wound",)
                elif mp < mmp:
                    note, tags = "缺魔", ()
                else:
                    note, tags = "", ()
                self.tv.insert("", "end", iid=str(i), values=(
                    i, core.char_name(r, self.names), r[8], r[0x21],
                    f"{hp} / {mhp}", f"{mp} / {mmp}",
                    core.u16(r, 0x37), core.u16(r, 0x39), core.u16(r, 0x3E), note),
                    tags=tags)

        kids = self.tv.get_children()
        if self.cur is not None and str(self.cur) in kids:
            self.tv.selection_set(str(self.cur))
            self.tv.see(str(self.cur))
        elif kids:
            # 切区域后自动选第一条，编辑面板不留空
            self.cur = int(kids[0])
            self.tv.selection_set(kids[0])
        else:
            self.cur = None
        self._loading = False

        self._sync_headline(reg, empty_slot)
        self._sync_money(reg)
        self._load_fields()
        self.update_status()

    def _sync_headline(self, reg, empty_slot):
        label = core.REGION_LABEL[self.region_name]
        if empty_slot:
            self.head_var.set(f"{label}    空槽")
            return
        real = [i for i in range(reg.max_recs) if core.is_real_char(reg.rec(i))]
        parts = [f"{label}", f"{len(real)} 人"]
        if reg.money is not None:
            parts.append(f"金钱 {reg.money:,}")
        if self.region_name == "head":
            f = self.edit.buf[core.HEAD_FIELD:core.HEAD_FIELD + 12]
            parts.append(f"地点 {f[0]} · 场景 {f[2]}")
        elif self.region_name.startswith("s"):
            parts.append(f"场景 {reg.scenario_id()}")
        if not self.names:
            parts.append("（未找到 FDTXT.DAT，名字不可用）")
        self.head_var.set("    ".join(parts))

    def _sync_money(self, reg):
        has = reg is not None and reg.money_off is not None and not (
            self.region_name not in ("head", "roster") and reg.empty())
        for w in (self.money_spin, self.btn_money_set, self.btn_money_add):
            try:
                w.configure(state="normal" if has else "disabled")
            except tk.TclError:
                pass
        if has:
            self.money_var.set(str(reg.money))
        else:
            self.money_var.set("")

    # -------------------------------------------------------------- 选中
    def _on_record_select(self, _evt=None):
        if self._loading:
            return
        sel = self.tv.selection()
        self.cur = int(sel[0]) if sel else None
        self._load_fields()

    def _on_double(self, _evt=None):
        """双击行 = 回满该角色（最常见的操作，给个快捷方式）。"""
        if self.cur is not None:
            self.heal_one()

    def _load_fields(self):
        self._loading = True
        r = None
        if self.cur is not None and self.edit:
            reg = self.reg
            if 0 <= self.cur < reg.max_recs:
                r = reg.rec(self.cur)
        if r is None:
            self.editor.configure(text="选中角色")
            for v in (self.var_lv, self.var_hp, self.var_mhp, self.var_mp,
                      self.var_mmp, self.var_ap, self.var_dp, self.var_dx):
                v.set("")
            self.derived_var.set("派生 —")
            self._set_editor_state(False)
        else:
            nm = core.char_name(r, self.names)
            who = f"#{self.cur}"
            if nm:
                who += f"   {nm}"
            who += f"   id={r[8]}"
            cls = core.class_name(r, self.names)
            race = core.race_name(r, self.names)
            if race or cls:
                who += f"   {'/'.join(x for x in (race, cls) if x)}"
            if not core.is_real_char(r):
                who += "   （空位记录）"
            self.editor.configure(text=f"选中角色   {who}")
            self.var_lv.set(str(r[0x21]))
            self.var_hp.set(str(core.u16(r, 0x40)))
            self.var_mhp.set(str(core.u16(r, 0x42)))
            self.var_mp.set(str(core.u16(r, 0x44)))
            self.var_mmp.set(str(core.u16(r, 0x46)))
            self.var_ap.set(str(_s16(r, 0x37)))
            self.var_dp.set(str(_s16(r, 0x39)))
            self.var_dx.set(str(_s16(r, 0x3E)))
            self._set_editor_state(core.is_real_char(r))
            self._update_derived(r)
        self._loading = False

    def _update_derived(self, r=None):
        if r is None:
            if self.cur is None or not self.edit:
                self.derived_var.set("派生 —")
                return
            r = self.reg.rec(self.cur)
        ap, dp, dx, de = core.derived(r)
        self.derived_var.set(f"派生 AP {ap}  DP {dp}  DX {dx}/{de}")

    def _on_field_change(self, *_a):
        if self._loading or self.cur is None or not self.edit:
            return
        reg = self.reg
        if self.cur >= reg.max_recs:
            return
        r = reg.rec(self.cur)
        if not core.is_real_char(r):
            return

        def num(s, cur, hi=9999):
            s = (s or "").strip()
            if not s:
                return None
            try:
                n = int(s, 0)
            except ValueError:
                return None
            return max(0, min(hi, n))

        before = bytes(r)
        base_before = (_s16(r, 0x37), _s16(r, 0x39), _s16(r, 0x3E))

        lv = num(self.var_lv.get(), r[0x21], 255)
        if lv is not None:
            r[0x21] = lv
        for var, off, hi in ((self.var_hp, 0x40, 9999), (self.var_mhp, 0x42, 9999),
                             (self.var_mp, 0x44, 9999), (self.var_mmp, 0x46, 9999)):
            v = num(var.get(), core.u16(r, off), hi)
            if v is not None:
                core.set_u16(r, off, v)
        for var, off in ((self.var_ap, 0x37), (self.var_dp, 0x39), (self.var_dx, 0x3E)):
            v = num(var.get(), 0, 32767)
            if v is not None:
                core.set_u16(r, off, v)

        if (_s16(r, 0x37), _s16(r, 0x39), _s16(r, 0x3E)) != base_before:
            core.apply_recalc(r)          # 基础属性变了 -> 派生值必须跟着重算

        if bytes(r) != before:
            self._sync_row(self.cur, r)
            self.refresh_regions()
            self.update_status()
        self._update_derived(r)

    def _sync_row(self, i, r=None):
        if str(i) not in self.tv.get_children():
            return
        reg = self.reg
        r = r if r is not None else reg.rec(i)
        hp, mhp = core.u16(r, 0x40), core.u16(r, 0x42)
        mp, mmp = core.u16(r, 0x44), core.u16(r, 0x46)
        if not core.is_real_char(r):
            note, tags = "空位", ("empty",)
        elif hp < mhp:
            note, tags = "残血", ("wound",)
        elif mp < mmp:
            note, tags = "缺魔", ()
        else:
            note, tags = "", ()
        self.tv.item(str(i), values=(i, core.char_name(r, self.names), r[8], r[0x21],
                                     f"{hp} / {mhp}",
                                     f"{mp} / {mmp}", core.u16(r, 0x37),
                                     core.u16(r, 0x39), core.u16(r, 0x3E), note),
                     tags=tags)

    # -------------------------------------------------------------- 操作
    def heal_one(self):
        if self.cur is None or not self.edit:
            return
        r = self.reg.rec(self.cur)
        if not core.is_real_char(r):
            return
        core.set_u16(r, 0x40, core.u16(r, 0x42))
        core.set_u16(r, 0x44, core.u16(r, 0x46))
        self._load_fields()
        self._sync_row(self.cur, r)
        self.refresh_regions()
        self.update_status()

    def _heal_in(self, names):
        n = 0
        for name in names:
            reg = core.Region(self.edit.buf, name)
            if name not in ("head", "roster") and reg.empty():
                continue
            for i in range(reg.max_recs):
                r = reg.rec(i)
                if not core.is_real_char(r):
                    continue
                if core.u16(r, 0x40) != core.u16(r, 0x42) or \
                        core.u16(r, 0x44) != core.u16(r, 0x46):
                    n += 1
                core.set_u16(r, 0x40, core.u16(r, 0x42))
                core.set_u16(r, 0x44, core.u16(r, 0x46))
        return n

    def heal_region(self):
        if not self.edit:
            return
        n = self._heal_in([self.region_name])
        self.refresh_regions()
        self.refresh_table()
        self.set_status(f"本区域已回满 {n} 人（尚未写盘）" if n else "本区域没有残血角色")

    def heal_all(self):
        if not self.edit:
            return
        n = self._heal_in(core.REGIONS)
        self.refresh_regions()
        self.refresh_table()
        self.set_status(f"整个存档已回满 {n} 人（继续游戏 + 4 个存档槽，尚未写盘）"
                        if n else "整个存档没有残血角色")

    def do_money(self, add):
        if not self.edit:
            return
        reg = self.reg
        if reg is None or reg.money_off is None or (
                self.region_name not in ("head", "roster") and reg.empty()):
            self._info("这里没有金钱", "名单表不单独存金钱。\n"
                                    "金钱在「继续游戏·队伍」和 4 个存档槽里。")
            return
        try:
            v = int(str(self.money_var.get()).strip() or 0, 0)
        except ValueError:
            self._error("数值不对", "金钱要填十进制或 0x 开头的整数。")
            return
        old = reg.money
        reg.money = (old + v) if add else v
        self.money_var.set(str(reg.money))
        self._sync_headline(reg, False)
        self.refresh_regions()
        self.update_status()
        self.set_status(f"金钱 {old:,} → {reg.money:,}（尚未写盘）")

    def do_revert(self):
        if not self.edit:
            return
        if not self.edit.dirty:
            self.set_status("本来就没有改动")
            return
        self.edit.buf[:] = self.edit.orig
        self.refresh_regions()
        self.refresh_table()
        self.set_status("已放弃所有未保存的改动")

    def do_save(self):
        if not self.edit:
            return
        if not self.edit.dirty:
            self._info("无需保存", "没有任何改动。")
            return
        nbytes = self.edit.changed_bytes()
        try:
            bak = core.store(self.path, self.edit.buf, backup=True)
        except SystemExit as e:
            self._error("写入失败", str(e))
            return
        except PermissionError:
            self._error("写入失败", f"写不进 {self.path}\n"
                                  "存档可能正被游戏占用，或文件是只读的。\n"
                                  "先退出游戏再试。")
            return

        try:
            ok = core.checksum_ok(core.load(self.path))
        except SystemExit:
            ok = False
        if not ok:
            self._error("写后校验失败",
                        "文件已写入但校验和对不上。\n"
                        "请用「备份 / 回滚」恢复刚才的备份。")
            return

        self.edit = core.Edit(self.path)
        self.refresh_regions()
        self.refresh_table()
        msg = f"已写入 {self.path}   改动 {nbytes} 字节，校验和 OK"
        if bak:
            msg += f"   备份 {os.path.basename(bak)}"
        self.set_status(msg)
        if not self.silent:
            self._info("保存成功",
                       f"改动 {nbytes} 字节，校验和已重算。\n\n"
                       f"备份：{os.path.basename(bak) if bak else '（无）'}\n\n"
                       "记得先退出游戏再进游戏读取，否则退出时会被内存里的进度覆盖。")

    # -------------------------------------------------------------- 备份
    def _quick_backup(self):
        base = f"{self.path}.bak-{time.strftime('%Y%m%d-%H%M%S')}"
        p, n = base, 1
        while os.path.exists(p):
            n += 1
            p = f"{base}-{n}"
        shutil.copy2(self.path, p)
        os.utime(p, None)
        return p

    def open_backups(self):
        if not self.path:
            return
        baks = core.backups(self.path)
        win = tk.Toplevel(self)
        win.title("备份 / 回滚")
        win.transient(self)
        win.grab_set()
        win.geometry("640x400")
        win.columnconfigure(0, weight=1)
        win.rowconfigure(1, weight=1)

        ttk.Label(win, text=f"{len(baks)} 个备份（新的在上面）",
                  font=FONT_B, padding=(10, 8, 10, 4)).grid(row=0, column=0, sticky="w")

        frame = ttk.Frame(win, padding=(10, 0, 10, 0))
        frame.grid(row=1, column=0, sticky="nsew")
        frame.columnconfigure(0, weight=1)
        frame.rowconfigure(0, weight=1)
        lb = tk.Listbox(frame, font=MONO, activestyle="none")
        lb.grid(row=0, column=0, sticky="nsew")
        sb = ttk.Scrollbar(frame, orient="vertical", command=lb.yview)
        lb.configure(yscrollcommand=sb.set)
        sb.grid(row=0, column=1, sticky="ns")
        for b in baks:
            lb.insert("end", f"{core.bak_stamp(b)}    {os.path.basename(b)}")
        if baks:
            lb.selection_set(0)

        bar = ttk.Frame(win, padding=(10, 8))
        bar.grid(row=2, column=0, sticky="ew")

        def restore():
            sel = lb.curselection()
            if not sel:
                return
            b = baks[sel[0]]
            if not self._ask("确认回滚",
                             f"用这份备份覆盖当前存档？\n\n{os.path.basename(b)}\n"
                             f"{core.bak_stamp(b)}\n\n"
                             "当前内容会先另存一份备份，所以回滚本身也能撤销。"):
                return
            try:
                cur_bak = self._quick_backup()
                shutil.copy2(b, self.path)
            except OSError as e:
                self._error("回滚失败", str(e))
                return
            win.destroy()
            self.load_save(self.path)
            self._info("已回滚",
                       f"已恢复到 {core.bak_stamp(b)}。\n"
                       f"回滚前的内容另存为 {os.path.basename(cur_bak)}。")

        ttk.Button(bar, text="关闭", command=win.destroy).pack(side="right", padx=(6, 0))
        ttk.Button(bar, text="恢复选中", command=restore).pack(side="right")
        if not baks:
            ttk.Label(bar, text="还没有备份——第一次保存时会自动生成。",
                      foreground="#666").pack(side="left")

    # -------------------------------------------------------------- 杂项
    def update_status(self):
        if not self.edit:
            return
        d = self.edit.changed_bytes()
        if d:
            self.status.configure(bg="#fff3cd", fg="#8a5a00")
            self.status.configure(
                text=f"[ 未保存 ]  {d} 字节改动等待写入 —— 点右下角「保存到存档」"
                     f"（会自动备份）")
        else:
            self.status.configure(bg="#eceff1", fg="#37474f")
            nb = len(core.backups(self.path)) if self.path else 0
            self.status.configure(
                text=f"已同步 · 校验和 OK · 备份 {nb} 个 · "
                     f"左键选人、双击回满、改完点「保存到存档」")

    def set_status(self, text):
        self.status.configure(text=text)

    def _center(self):
        self.update_idletasks()
        w, h = int(1120 * self.S), int(740 * self.S)
        sw, sh = self.winfo_screenwidth(), self.winfo_screenheight()
        self.geometry(f"{w}x{h}+{max(0, (sw - w) // 2)}+{max(0, (sh - h) // 3)}")

    def _confirm_discard(self):
        if self.edit and self.edit.dirty:
            return self._ask("有未保存的改动",
                             "当前改动还没写进存档，继续会丢掉。\n要放弃这些改动吗？")
        return True

    def on_close(self):
        if not self._confirm_discard():
            return
        self.destroy()

    # 弹窗包装：selftest 时静默
    def _info(self, title, msg):
        if not self.silent:
            messagebox.showinfo(title, msg, parent=self)

    def _warn(self, title, msg):
        if not self.silent:
            messagebox.showwarning(title, msg, parent=self)

    def _error(self, title, msg):
        if not self.silent:
            messagebox.showerror(title, msg, parent=self)

    def _ask(self, title, msg):
        if self.silent:
            return True
        return messagebox.askyesno(title, msg, parent=self)


# ------------------------------------------------------------------ selftest
def selftest(src):
    """无人工介入地跑一遍 UI 逻辑：切区域、改字段、满血、写盘、撤销。"""
    import tempfile
    tmp = os.path.join(tempfile.gettempdir(), "fd2_gui_selftest.sav")
    for f in os.listdir(tempfile.gettempdir()):
        if f.startswith("fd2_gui_selftest.sav"):
            try:
                os.remove(os.path.join(tempfile.gettempdir(), f))
            except OSError:
                pass
    shutil.copy2(src, tmp)

    app = App(path=tmp, silent=True)
    app.update()
    checks = []

    def ck(name, cond, extra=""):
        checks.append((name, bool(cond), extra))

    ck("区域列表 6 行", len(app.rv.get_children()) == 6,
       str(len(app.rv.get_children())))
    ck("默认区域=继续游戏·队伍", app.region_name == "head")
    ck("队伍表显示 5 人", len(app.tv.get_children()) == 5,
       str(len(app.tv.get_children())))
    ck("名字表已加载", len(app.names) > 100, f"{len(app.names)} 条")
    first_vals = app.tv.item(app.tv.get_children()[0], "values")
    ck("首行名字=索爾", str(first_vals[1]) == "索爾", str(first_vals[:3]))

    # 选中第一条并改 HP
    first = app.tv.get_children()[0]
    app.tv.selection_set(first)
    app.update()
    ck("选中后编辑框有值", app.var_mhp.get() != "" and app.var_hp.get() != "",
       f"hp={app.var_hp.get()} max={app.var_mhp.get()}")

    app.var_hp.set("1")
    app.update()
    r = app.reg.rec(app.cur)
    ck("改 HP 落到缓冲", core.u16(r, 0x40) == 1, str(core.u16(r, 0x40)))
    ck("有未保存改动", app.edit.dirty and app.edit.changed_bytes() > 0)

    app.do_money(False)
    app.money_var.set("0")

    # 满血（当前区域 + 全存档）
    app.heal_region()
    app.update()
    r = app.reg.rec(app.cur)
    ck("本区域满血", core.u16(r, 0x40) == core.u16(r, 0x42),
       f"{core.u16(r, 0x40)}/{core.u16(r, 0x42)}")
    app.heal_all()
    app.update()

    # 改基础 AP -> 派生值自动重算
    app.var_ap.set(str(int(app.var_ap.get() or 0) + 7))
    app.update()
    r = app.reg.rec(app.cur)
    ck("改 AP 触发派生重算", core.u16(r, 0x48) == (core.derived(r)[0] & 0xFFFF),
       f"derived@0x48={core.u16(r, 0x48)} calc={core.derived(r)[0]}")

    # 切换区域
    app.select_region("s0")
    app.update()
    ck("切到存档槽 1", app.region_name == "s0"
       and len(app.tv.get_children()) > 0, str(len(app.tv.get_children())))
    ck("槽 1 有金钱输入", app.money_var.get() != "", app.money_var.get())

    # 空槽
    app.select_region("s3")
    app.update()
    ck("空槽不报错", True)
    app.select_region("head")
    app.update()

    # 写盘
    before = open(tmp, "rb").read()
    app.do_save()
    app.update()
    after = open(tmp, "rb").read()
    ck("写盘后文件变化", before != after)
    ck("写盘后校验和 OK", core.checksum_ok(core.load(tmp)))
    ck("写盘后状态已同步", not app.edit.dirty)
    ck("生成了备份", len(core.backups(tmp)) >= 1, str(len(core.backups(tmp))))

    # 撤销
    app.var_hp.set("2")
    app.update()
    ck("再次产生改动", app.edit.dirty)
    app.do_revert()
    app.update()
    ck("放弃改动后回到同步", not app.edit.dirty)

    app.destroy()

    bad = [c for c in checks if not c[1]]
    for name, ok, extra in checks:
        print(("  OK   " if ok else "  FAIL ") + name + (f"   [{extra}]" if extra else ""))
    print(f"\n{len(checks) - len(bad)}/{len(checks)} passed")
    return 0 if not bad else 1


def main(argv):
    _enable_dpi()
    if len(argv) >= 2 and argv[1] == "--selftest":
        src = argv[2] if len(argv) > 2 else core.find_save(quiet=True)
        return selftest(src)
    path = argv[1] if len(argv) > 1 else None
    app = App(path=path)
    app.mainloop()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
