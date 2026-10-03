#!/usr/bin/env python3
"""Builds FilterTekTables.hpp from the FilterE measurements: q per resonance step for 12 and 24 dB,
and the gain control's input gain. Run after qsweep/qfit, ring.py (60 100 118), q24.py, gainsweep.py."""
import json, sys
import numpy as np, fmeasure as m
O = m.OUT
q12_noise = json.load(open(f'{O}/qfit_gc1.json'))
ring = json.load(open(f'{O}/ring.json'))
q24 = json.load(open(f'{O}/q24.json'))
gain = json.load(open(f'{O}/gain_gc1.json'))
br = json.load(open(f'{O}/br.json'))
qlow = json.load(open(f'{O}/qlow.json'))

def smooth_monotone(vals, upto):
    """Light smoothing in log below `upto` (the noise fits wobble at low resonance) and a
    monotone decrease everywhere: more resonance never means a lower Q."""
    lv = np.log(np.array(vals))
    sm = lv.copy()
    for i in range(1, upto):
        sm[i] = lv[max(0, i - 2):i + 3].mean()
    out = np.exp(sm)
    for i in range(1, len(out)):
        out[i] = min(out[i], out[i - 1])
    return out

# 12 dB: the noise fits up to 111, the ring-down (mean of 3.3 and 10 kHz) from 112, where it is
# the only one that resolves the peak.
q12 = []
for r in range(128):
    if r >= 112:
        vs = [ring[v][str(r)]['q0'] for v in ('100', '118') if np.isfinite(ring[v][str(r)]['q0']) and ring[v][str(r)]['q0'] > 0]
        q12.append(float(np.mean(vs)) if vs else q12[-1] * 0.5)
    else:
        q12.append(q12_noise[str(r)]['q0'])
def with_low(vals, slope):
    """Up to step 64, the low-resonance measurements (qlow.py), interpolated in log between the
    steps they took: they are more precise there than the sweeps made for high resonance."""
    pts = sorted((int(r), q) for r, q in qlow[str(slope)].items())
    xs = [p[0] for p in pts]; ys = np.log([p[1] for p in pts])
    out = list(vals)
    for r in range(0, xs[-1] + 1):
        out[r] = float(np.exp(np.interp(r, xs, ys)))
    return out
q12 = smooth_monotone(with_low(q12, 0), 0)
q24v = smooth_monotone(with_low([q24[str(r)]['q0'] for r in range(128)], 1), 0)
g = [gain[str(r)] for r in range(128)]

def br_table(slope):
    """Band reject q0 per step: a cubic in log fitted through the measured steps (every fourth),
    since the single fits wobble, made monotone like the others."""
    pts = sorted((int(r), v['q0']) for r, v in br[str(slope)].items())
    x = np.array([p[0] for p in pts]); y = np.log([p[1] for p in pts])
    c = np.polyfit(x, y, 3)
    vals = np.exp(np.polyval(c, np.arange(128)))
    for i in range(1, 128):
        vals[i] = min(vals[i], vals[i - 1])
    return vals
br12 = br_table(0)
br24 = br_table(1)

def table(name, vals, fmt):
    L = [f'static const float {name}[128] = {{']
    for i in range(0, 128, 8):
        L.append('    ' + ', '.join(fmt.format(v) for v in vals[i:i + 8]) + ',')
    return L + ['};', '']

L = ['// Generated from measurements (G1-Emu tools/filtersweep). Do not edit by hand: re-run them.',
     '#pragma once', '', 'namespace filtertek {', '',
     '// Damping q0 per resonance step; the filter uses q = q0 * (1 - f/2), f = 2 sin(pi fc / 96k).',
     '// 12 dB: one section. Up to step 64 from fits at two cutoffs with plenty of level, to 111 from',
     '// noise fits at 330 Hz, from 112 from the ring-down of an impulse at 3.3 and 10 kHz, which',
     '// resolves peaks the noise fits cannot.']
L += table('Q12', q12, '{:.6g}f')
L += ['// 24 dB: two identical sections in cascade, each with this q0.']
L += table('Q24', q24v, '{:.6g}f')
L += ['// Band reject: its notch has a damping of its own, much gentler than the resonance. A cubic',
      '// through the measured steps; 12 dB matches the measured notch to 0.4-1.8 dB, 24 dB to 0.2-0.4.']
L += table('BR_Q12', br12, '{:.6g}f')
L += table('BR_Q24', br24, '{:.6g}f')
L += ['// Gain control: input gain in dB per resonance step, the same for 12 and 24 dB.']
L += table('GAIN_CONTROL_DB', g, '{:.3f}f')
L += ['} // namespace filtertek', '']
open(sys.argv[1], 'w').write('\n'.join(L))
print('q12', ' '.join(f'{q12[r]:.4g}' for r in (0, 32, 64, 96, 112, 120, 124, 127)))
print('q24', ' '.join(f'{q24v[r]:.4g}' for r in (0, 32, 64, 96, 112, 120, 124, 127)))
print('gain', ' '.join(f'{g[r]:.2f}' for r in (0, 32, 64, 96, 112, 120, 124, 127)))
print('br12', ' '.join(f'{br12[r]:.4g}' for r in (0, 32, 64, 96, 112, 120, 124, 127)))
print('br24', ' '.join(f'{br24[r]:.4g}' for r in (0, 32, 64, 96, 112, 120, 124, 127)))
