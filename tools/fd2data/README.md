# FD2 字形映射（glyphs.json）说明

`glyphs.json` = `{字形号: 汉字}`，是 FDTXT.DAT 文本流 → 可读文本的解码表。
存档修改器（`fd2sav.py` / `fd2sav_gui.py`）用它显示角色名、种族、职业。

## 数据从哪来

- **字库位图**：`FDOTHER.DAT` 资源 4（`boot.c`: `dword_53A75 = res_load(FDOTHER,4)`）。
  58368 字节 = 1824 个 16×16 1bpp 字形，每个 32 字节；每行 u16 小端读出后
  `xchg al,ah`（同 `gfx_draw_glyph` 0x4ED7A）。
- **文本流**：`FDTXT.DAT` 资源 0 = u16 偏移表 + int16 字形序列（负数是 opcode，
  -1 结束；`vm_run` 0x15F84）。角色名 = 流 `记录[+0x08]+1`，
  种族 = `记录[+0x1F]+140`，职业 = `记录[+0x20]+150`（`box_popup.c:60`）。
- **映射本身**：`_rebuild.py` 里的 ROWS 是早期**按图谱肉眼认字**写成的，
  `glyph_map.seed.json`（从道具名反推，零冲突）会覆盖它。

## ⚠️ 已知风险：错位

肉眼认字错一个，后面整段会**顺延错位**。2026-10-17 实例：160 行多写了"界"，
导致 169..175 六个字全错——种族名显示成「人界」，实际是「**人類**」。
同批修正：魔族 / 機械 / 元素 / 其他 / 精靈族全滅。

**判断信号**：解出的词读不通（如"械元""素其"）≈ 错位。遇到就回原图核对。

## 核对工具

```bash
# 全量对照表（8 页 PNG，每页 256 字形）：位图 + 编号 + 当前识别字
# 红色 ? = 未识别，灰"空"= 空白字形；其余深蓝 = glyphs.json 现值
python tools/glyph_atlas.py                       # 输出到 .workbuddy/atlas/
python tools/glyph_atlas.py --lo 1536 --hi 1792   # 只生成一段

# 单个/小段字形放大导出
python tools/glyph_view.py 169,170 out.png
python tools/glyph_view.py 160-179 out.png 12
```

两工具均纯 Python（ctypes GDI / 手写 PNG），无 Pillow 依赖，Windows 下运行。

## 其他相关

- `fd2_fonttool.py sheet/decode/convert`：字形图谱（需 Pillow）、文本流解码、
  繁→简字库重生成。
- 修改映射后重跑 `_rebuild.py` 重新生成 glyphs.json（seed 优先级更高），
  存档工具下次启动自动生效。
