"""Where a constant on a modulation input puts the cutoff: Noise -> FilterE (LP12, res 96) with a
Constant on one of its inputs, cutoff estimated from the spectrum of the filtered noise."""
import json, sys, os, subprocess, concurrent.futures as cf
import numpy as np, fmeasure as m, svf
from fit import grid_fit, dB
R = 96000
QTAB = json.load(open(f'{m.OUT}/qfit_gc1.json'))
def patch(cvals, fparams, conn):
    """Up to 4 chains Noise -> FilterE -> out k, each with a Constant on input `conn`."""
    mods = [(1, 3)]; names = {1: '4Output'}; params = ["1 3 1 100 "]; cables = []; idx = 2
    for k, (cv, fp) in enumerate(zip(cvals, fparams)):
        n, c, fe = idx, idx + 1, idx + 2; idx += 3
        mods += [(n, 31), (c, 43), (fe, 51)]
        names.update({n: f'Noise{k}', c: f'Const{k}', fe: f'FilterE{k}'})
        params += [f"{n} 31 1 0 ", f"{c} 43 2 {cv} 0 ", f"{fe} 51 10 " + " ".join(map(str, fp)) + " "]
        cables += [f"0 {fe} 2 0 {n} 0 1", f"0 {fe} {conn} 0 {c} 0 1", f"0 1 {k} 0 {fe} 0 1"]
    L = ["[Header]", "Version=Nord Modular patch 3.0", m.b.HEADER, "[/Header]", "[ModuleDump]", "1 "]
    for row, (i, t) in enumerate(mods): L.append(f"{i} {t} {2 + (row % 3) * 7} {2 + (row // 3) * 4} ")
    L += ["[/ModuleDump]", "[ModuleDump]", "0 ", "[/ModuleDump]", "[CurrentNoteDump]", "64 0 0 64 0 0 ", "[/CurrentNoteDump]", "[CableDump]", "1 "] + [c + " " for c in cables]
    L += ["[/CableDump]", "[CableDump]", "0 ", "[/CableDump]", "[ParameterDump]", "1 "] + params
    L += ["[/ParameterDump]", "[ParameterDump]", "0 ", "[/ParameterDump]", "[MorphMapDump]", "0 0 0 0 ", "[/MorphMapDump]",
          "[KeyboardAssignment]", "0 0 0 0 ", "[/KeyboardAssignment]", "[KnobMapDump]", "[/KnobMapDump]", "[CtrlMapDump]", "[/CtrlMapDump]", "[NameDump]", "1 "]
    for i, _ in mods: L.append(f"{i} {names[i]}")
    L += ["[/NameDump]", "[NameDump]", "0 ", "[/NameDump]"]
    return "\n".join(L) + "\n"
def run(tag, cvals, fparams, conn, seconds=4.0):
    pch = f"{m.OUT}/{tag}.pch"; wav = f"{m.OUT}/{tag}.wav"
    open(pch, 'w').write(patch(cvals, fparams, conn))
    subprocess.run([m.BIN, m.ROM, pch, '--seconds', str(seconds), '--note', '-1', '--wav', wav],
                   capture_output=True, text=True, timeout=900, cwd=m.G1)
    y = m.read_wav(wav) if hasattr(m, 'read_wav') else None
    return y, wav, pch
def spectrum(x, nseg=16384):
    win = np.hanning(nseg); P = np.mean([np.abs(np.fft.rfft(x[s:s + nseg] * win)) ** 2
                                        for s in range(4000, len(x) - nseg, nseg // 2)], 0)
    return np.fft.rfftfreq(nseg, 1 / R), P
def fit_cutoff(fr, P, res):
    keep = (fr > 15) & (fr < 40000); fr, P = fr[keep], P[keep]
    w = np.geomspace(15, 40000, 500); Pw = np.interp(w, fr, P); Hm = 10 * np.log10(Pw)
    ok = Hm > Hm.max() - 50
    def cost(p):
        f = 2 * np.sin(np.pi * np.exp(p[0]) / R); q = QTAB[str(res)]['q0'] * (1 - f / 2)
        return np.abs(Hm - dB(svf.resp(f, q, w)['lp']) - p[1])[ok].max()
    p, c = grid_fit(cost, [(np.log(10), np.log(30000)), (-150, 50)], iters=8, n=41)
    return float(np.exp(p[0])), c
