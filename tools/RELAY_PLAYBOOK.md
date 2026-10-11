# RELAY_PLAYBOOK — one-batch restoration instructions (port3)

You are ONE batch in a relay that restores FD2.EXE to C in `E:/FD2/port3`.
Read `PROGRESS.md` and recent `src/game/*.c` for style. Leave the repo clean and committed.

## Primary evidence (no IDA GUI required)
- Hex-Rays pseudocode: `analysis/decomp/<ADDR>_<NAME>.c` (find with `ls analysis/decomp | grep -i <addr>`).
- Offline full disassembly: `analysis/cseg01_disasm.txt` (grep `grep -n "^ *12eaa:" analysis/cseg01_disasm.txt`).
- Inventory/status: `function-map.csv`; segment bytes: `analysis/seg_cseg01.bin` (file offset = addr - 0x10000).
- Ghidra MCP may be unavailable in your context; the offline disassembly is the authority.

> **IMPORTANT — bash exit codes abort your run.** A bash command that exits non-zero terminates this
> child immediately. Any `grep`/`ls`/pipeline that may legitimately find nothing MUST end with
> `|| true` (e.g. `grep -n "^ *1e504:" analysis/cseg01_disasm.txt || true`, `ls x | grep y || true`).
> Avoid bare `grep` as the last command of a pipeline; do not use `set -e`/`pipefail`.

## Batch selection
1. Compute closure-ready pending functions and pick a coherent cluster (1..6 funcs, <= ~1500 bytes) by usage desc, preferring same-module/callee clusters:

```
import csv
rows=list(csv.DictReader(open("function-map.csv",encoding="utf-8")))
pend={r["orig_name"] for r in rows if r["status"]=="待还原"}
free=[r for r in rows if r["status"]=="待还原" and not [c for c in r["callees"].split(";") if c in pend]]
free.sort(key=lambda r:-int(r["usage"] or 0))
[print(r["addr"],r["usage"],r["size"],r["orig_name"],r["callees"]) for r in free[:30]]
```

2. If nothing is closure-ready, take the pending function with the smallest transitive pending-closure and restore the whole closure.
3. **FALSE callees**: `sub_3702F` (Watcom chkstk) is NEVER a real dependency. A `jmp 0xNNNN` at a
   function tail that lands INSIDE another function body is a shared epilogue (plain `return`), not a
   call. Verify in the disassembly; do not transitively pull such functions in.
4. Skip clusters whose true closure exceeds ~8 functions; choose a smaller closure-ready cluster.
5. **Large single-function unblock**: if the closure-ready list has no in-budget *game-content* item
   (only CRT/library wrappers or over-budget items remain), you MAY take ONE larger single function
   (up to ~2500 B) provided its TRUE closure is just itself (every callee already restored) and you
   verify it instruction-by-instruction. State this deviation explicitly in the report.
6. **Uninitialised tick-baseline registers**: several functions read a never-written `ebp`/`esi` holding
   an (uninitialised) BIOS-tick baseline before the first render (established precedents: `map_wait_key`
   0x12DAC, `map_zoom_run` 0x2000A). Model the baseline as `0` and annotate the source; do NOT skip the
   function solely for that quirk.

## Translation rules (mandatory)
- Read BOTH the Hex-Rays decomp AND the disassembly. Disassembly is authoritative for argument
  order/count, constants, control flow, and decompiler artifacts: chkstk, shared epilogues,
  `__CHP` -> `(int)`, `JUMPOUT`, uninitialised register locals.
- Use existing `fd2_globals.h` macros (`dword_53xxx`, `qword_53xxx`, `byte_53xxx`). Pointer arithmetic
  via `(uintptr_t)`/`(intptr_t)`; keep 32-bit semantics. Match signed vs unsigned compares exactly.
- Call already-restored functions by their semantic names from module headers (`dlg_frame_tile`,
  `vm_run`, `rle2_blit`, `sprite24_plain49`, `ui_meter_bar`, `fb_slide_step`, `res_load`, `res_blit6`,
  `map_cell_info`, `guest_malloc`, `j___delay`, ...). Grep the headers first. If a needed callee is
  still pending, it is part of the closure: restore it too.
- Raw table accessors `0x4E7DD..0x4E8BC`: use `tbl_ptr(base, stride, index, offset)` with
  base = `&fd2_data2[off]` (dseg03, e.g. 0x602AD -> `&fd2_data2[0x2AD]`) or `&fd2_data1[off]` (dseg02).
  base/stride are listed in `src/game/tables.h`.
- Heap: use `guest_malloc`/`guest_free` (`src/game/guest_mem.h`), never `malloc`/`free`.
- Put functions in a semantically named module `src/game/<name>.c` + `.h`, terse style with
  original-address comments and a doc header.
- Add every restored function to `src/restored_manifest.txt` as `0xADDR sub_ADDR game/<module>.h`
  (one per line). Add the new header to the `src/game/fd2_game.h` umbrella include list (alphabetical).

## Ledger / generator rules
- **DO NOT** run `tools/gen_headers.py`, and **DO NOT** hand-edit `src/include/fd2_funcs.h`.
  Regenerating it would drop IDA-name prototypes still called by older restored modules
  (e.g. `scene_enter.c` calls `sub_24618`), breaking the build. Only run:
  `python tools/gen_function_map.py`.
- In new module headers, do not add `#define` aliases for IDA `sub_XXXX` names (they can clash with
  surviving `fd2_funcs.h` prototypes). Call semantic names only.

## Validation (mandatory before commit)
- `wsl -e bash tools/check_compile.sh --full` MUST show: `standalone errors: 0`, `hybrid errors: 0`,
  `-Werror` both profiles `rc=0`, umbrella `rc=0`. Fix until green.
- `python tools/gen_function_map.py`.
- Update `PROGRESS.md`: add the next numbered subsection under section `0-L` (functions, traps,
  findings) and update the `验证与台账` counts for 待还原 / 已还原未验证.
- Commit: `git add -A && git commit -m "P1 batch N: restore <cluster> (<addrs>)"`.

## Constraints / HALT
- One writer; no parallel git. Do not modify verified existing modules beyond the umbrella include.
- Do not hand-edit `function-map.csv` or generated headers.
- If compile cannot go green after honest fixes, or semantics stay ambiguous after decomp+disasm, or
  you would have to guess ABI/constants: revert uncommitted changes (`git checkout -- .`; delete new
  files) and end with a line starting `HALT: <reason>`.

## Final report (concise)
`BATCH`; `COMMIT` (hash or none); `NEW_PENDING` (count); `NOTES` (traps, decompiler artifacts, asset
gaps, false callees); `NEXT` (recommended cluster). Use the reserved word `HALT:` (line start) ONLY
when you actually halted as described above — never in a normal completion report, and never write a
bare `STOP:` line. If nothing is left to restore, print `ALL_DONE`.
