#!/usr/bin/env python3
"""Builds src/AdsrTekCurves.hpp from the sweep: the time table, the three attack
curves (normalised, averaged over long attacks) and the snap level."""
import glob, json, os, sys
import numpy as np
import sweep
HERE = __import__('measure').OUT
POINTS = 257
RATE = 24000  # curves are stored decimated to the control rate

def attack_curve(shape, values):
    curves = []
    for f in sorted(glob.glob(f'{HERE}/curves/attack_s{shape}_*.npz')):
        d = np.load(f)
        for k, v in enumerate(d['vals']):
            if v not in values: continue
            c = d['y'][:, k].astype(np.float64)
            on = int(np.argmax(c > 0))
            # the sample before the first non-zero one is where the attack started
            start = max(on - 1, 0)
            top = int(np.argmax(c >= 0.9999))
            t = np.linspace(0, 1, POINTS)
            src_t = (np.arange(start, top + 1) - start) / (top - start)
            curves.append((v, np.interp(t, src_t, c[start:top + 1])))
    arr = np.array([c for _, c in curves])
    return arr.mean(axis=0), np.abs(arr - arr.mean(axis=0)).max(), [v for v, _ in curves]

if __name__ == '__main__':
    out = sys.argv[1]
    res = json.load(open(f'{HERE}/sweep.json'))
    long_vals = set(range(90, 111))
    sp = json.load(open(f'{HERE}/shape_params.json'))
    # Decay and release: the mean per-tick ratio between 60% and 2%, as a time
    # constant (the measurement resolves 0.015% steps, so the mean is what counts).
    taus = {}
    for f in sorted(glob.glob(f'{HERE}/curves/decay_s1_*.npz')):
        d = np.load(f)
        for k, v in enumerate(d['vals']):
            c = d['y'][:, k].astype(np.float64)
            seg = c[int(np.argmax(c >= 0.9999)):]
            lo = np.nonzero(seg < 0.6)[0][0]; hi = np.nonzero(seg < 0.02)[0][0]
            ratio = np.exp(np.log(seg[hi] / seg[lo]) / (hi - lo))
            taus[int(v)] = -1.0 / (RATE * np.log(ratio))
    attack_t = [[None] * 128 for _ in range(3)]
    for x in res:
        if x['stage'] == 'attack':
            attack_t[x['shape']][x['value']] = x['t_full']
    lv = [r['level_before_zero'] for r in res if r['stage'] in ('decay', 'release') and r.get('level_before_zero')]
    snap = float(np.median(lv))
    print(f'snap level: median {snap:.5f}, range {min(lv):.5f}-{max(lv):.5f}')
    L = ['// Generated from measurements (G1-Emu tools/envsweep). Do not edit by hand: re-run them.', '#pragma once', '',
         'namespace adsrtek {', '',
         '// Measured seconds for each knob step 0-127, per attack shape: start to full scale.',
         '// They follow the published table to within a few percent, except at the top, where',
         '// the measured times step and run long (Lin 123 takes 38.8 s, not the 31.4 s shown).',
         'static const float ATTACK_TIME[3][128] = {']
    for sh in range(3):
        L.append('    {')
        for i in range(0, 128, 8):
            L.append('        ' + ', '.join(f'{t:.6g}f' for t in attack_t[sh][i:i + 8]) + ',')
        L.append('    },')
    L += ['};', '',
          '// Measured time constant of decay and release, in seconds, per knob step: they',
          '// are the same curve. Falling to 1% takes ln(100) of these.',
          'static const float DECAY_TAU[128] = {']
    for i in range(0, 128, 8):
        L.append('    ' + ', '.join(f'{taus[v]:.6g}f' for v in range(i, i + 8)) + ',')
    L += ['};', '',
          '// Below this a decay or release goes to its target: about -76 dB, the floor of',
          '// what the measurement could resolve.',
          f'static constexpr float SNAP_LEVEL = {snap:.6f}f;', '',
          '// Log attack: an exponential approach to a target above full scale, cut',
          '// when it gets there. The target, per knob step, is what fits the measured',
          '// curve; the rate follows from reaching full scale in the attack time.',
          'static const float LOG_TARGET[128] = {']
    for i in range(0, 128, 8):
        L.append('    ' + ', '.join(f"{sp['0'][str(v)][0]:.6g}f" for v in range(i, i + 8)) + ',')
    L += ['};', '',
          '// Exp attack: exponential growth from y = 0 with this offset, y = r(e^(at) - 1),',
          '// a = ln(1 + 1/r). Per knob step, fitted to the measured curve.',
          'static const float EXP_OFFSET[128] = {']
    for i in range(0, 128, 8):
        L.append('    ' + ', '.join(f"{sp['2'][str(v)][0]:.6g}f" for v in range(i, i + 8)) + ',')
    L += ['};', '', '} // namespace adsrtek', '']
    open(out, 'w').write('\n'.join(L))
