#!/usr/bin/env python3
"""Apply cmake/Dsp56300.cmake's g1_dsp_replace patches, in order, to a dsp56kEmu source tree and
say for each one whether it applies, is already there, or its anchor is gone (the code changed).
Reads only; writes nothing. See docs/core-upgrade-plan.md.

    python3 tools/overlaycheck.py cmake/Dsp56300.cmake ~/src/dsp56300-upstream/source/dsp56kEmu
"""
import re, sys, pathlib
cm = open(sys.argv[1]).read()
src = pathlib.Path(sys.argv[2])
def unescape(s):  # CMake quoted-argument escapes used in this file
    return s.replace('\\n','\n').replace('\\t','\t').replace('\\"','"').replace('\\\\','\\')
tok = re.compile(r'\[=\[(.*?)\]=\]|"((?:[^"\\]|\\.)*)"', re.S)
files = {}
results = []
pos = 0
for m in re.finditer(r'g1_dsp_replace\((\S+)', cm):
    name = m.group(1); i = m.end(); args = []
    while len(args) < 2:
        t = tok.search(cm, i); args.append(t.group(1) if t.group(1) is not None else unescape(t.group(2))); i = t.end()
    before, after = args
    line = cm.count('\n', 0, m.start()) + 1
    # nearest comment block above
    head = cm[:m.start()].rstrip().split('\n')
    c = [l for l in head[-12:] if l.startswith('#')]
    title = (c[0][2:] if c else '')[:90]
    if name not in files:
        p = src / name
        files[name] = p.read_text() if p.exists() else None
    txt = files[name]
    if txt is None: st = 'FILE MISSING'
    elif before in txt:
        st = 'applies'; files[name] = txt.replace(before, after)
    elif after.strip() and after in txt: st = 'already there'
    else: st = 'ANCHOR GONE'
    results.append((line, name, st, title))
for r in results: print(f"{r[0]:4d} {r[1]:28s} {r[2]:13s} {r[3]}")
from collections import Counter; print(Counter(r[2] for r in results))
