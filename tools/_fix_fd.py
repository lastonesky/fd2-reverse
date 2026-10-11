#!/usr/bin/env python3
# One-off patcher: make gen_headers.py's defined-functions regex escape-free.
# (Repeated heredoc edits kept losing \b to JSON unescaping; a plain script file
# avoids the problem entirely.)
import re

P = 'tools/gen_headers.py'
t = open(P, encoding='utf-8').read()

# 1) module-level, escape-free regex
if '_FD_DEF_RE' not in t:
    anchor = "AN = os.path.join(ROOT, 'analysis')"
    decl = (anchor + '\n'
            "# Matches a function definition/declaration line and captures its name.\n"
            "# Escape-free on purpose: '\\b' etc. are avoided so the pattern survives\n"
            "# every editing path (see tools/_fix_fd.py).\n"
            "_FD_DEF_RE = re.compile('^[ ]*(static[ ]+)?(const[ ]+)?'\n"
            "                        '[A-Za-z_][A-Za-z0-9_ *]*?'\n"
            "                        '([A-Za-z_][A-Za-z0-9_]*)[ ]*[(]')\n")
    t = t.replace(anchor, decl, 1)

# 2) use it in the scan loop (replace whatever the current line is)
lines = t.split('\n')
out = []
for ln in lines:
    if '_fd_re' in ln or ('re.match(' in ln and 'A-Za-z_' in ln and 'ln)' in ln):
        continue
    if 'defined_funcs.add(m.group(1))' in ln:
        out.append('                defined_funcs.add(m.group(1))')
        continue
    out.append(ln)
t = '\n'.join(out)

# insert the match call right before the add
if '_FD_DEF_RE.match(ln)' not in t:
    t = t.replace('                defined_funcs.add(m.group(1))',
                  '            m = _FD_DEF_RE.match(ln)\n'
                  '            if m:\n'
                  '                defined_funcs.add(m.group(1))', 1)

open(P, 'w', encoding='utf-8').write(t)
print('patched gen_headers.py')
