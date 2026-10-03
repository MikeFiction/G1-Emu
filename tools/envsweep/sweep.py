#!/usr/bin/env python3
"""Full sweep of the G1 ADSR in the emulator. Saves features to sweep.json and the
curves, decimated to the 24 kHz control rate, to curves/*.npz."""
import json, os, re, sys, concurrent.futures as cf
import numpy as np
import measure as m
FS = 0.051041483879089355
OUT = m.OUT
os.makedirs(OUT + '/curves', exist_ok=True)
src = open(os.path.join(m.G1, '..', 'Nomad2026', 'source', 'format', 'ValueFormatters.cpp')).read()
names = re.findall(r'"([^"]*)"', re.search(r'DATA_ADSR_TIME\[\]\s*=\s*\{(.*?)\};', src, re.S).group(1))
TABLE = [float(v[:-1]) / 1000 if v.endswith('m') else float(v[:-1]) for v in names]

def job(args):
    stage, shape, vals = args
    tmax = max(TABLE[v] for v in vals)
    if stage == 'attack':
        params = [[shape, v, 0, 127, 0, 0] for v in vals]; on = 0.05
        # 1.5x: at the top the measured attacks run up to a third longer than the table
        sec = on + tmax * 1.5 + 0.1; ev = None
    elif stage == 'decay':
        params = [[shape, 0, v, 0, 0, 0] for v in vals]; on = 0.05
        sec = on + tmax * 2.5 + 0.2; ev = None
    else:  # release: gate 0.1 s at full sustain
        params = [[shape, 0, 0, 127, v, 0] for v in vals]; on = 0.05
        sec = 0.15 + tmax * 2.5 + 0.2; ev = '+0.05,-0.15'
    tag = f'{stage}_s{shape}_{vals[0]:03d}'
    x, rate = m.run(tag, 20, params, 1, 0, on=on, seconds=sec, events=ev)
    os.remove(f'{m.OUT}/{tag}.wav'); os.remove(f'{m.OUT}/{tag}.pch')
    y = (x[:, :len(vals)] / FS).astype(np.float32)
    np.savez_compressed(f'{OUT}/curves/{tag}.npz', y=y[::4], vals=np.array(vals))
    res = []
    for k, v in enumerate(vals):
        c = y[:, k].astype(np.float64)
        onset = int(np.argmax(c > 0))
        r = {'stage': stage, 'shape': shape, 'value': v, 'table': TABLE[v], 'onset': onset / rate}
        if stage == 'attack':
            top = int(np.argmax(c >= 0.9999)) if (c >= 0.9999).any() else -1
            r['t_full'] = (top - onset) / rate if top > 0 else None
            r['peak'] = float(c.max())
            if top > onset:
                r['shape_pts'] = [float(c[onset + int((top - onset) * f)]) for f in np.linspace(0, 1, 21)]
        else:
            if stage == 'decay':
                start = int(np.argmax(c >= 0.9999))
            else:
                # the release begins where full scale is last held
                full = np.nonzero(c >= 0.9999)[0]; start = int(full[-1]) if len(full) else onset
            seg = c[start:]
            below = np.nonzero(seg <= 0.01)[0]
            zero = np.nonzero(seg <= 0.0)[0]
            r['t_1pct'] = below[0] / rate if len(below) else None
            r['t_zero'] = zero[0] / rate if len(zero) else None
            r['level_before_zero'] = float(seg[zero[0] - 1]) if len(zero) and zero[0] > 0 else None
            # exponential fit between 50% and 2%
            idx = np.nonzero((seg < 0.5) & (seg > 0.02))[0]
            if len(idx) > 20:
                tt = idx / rate; k_, b_ = np.polyfit(tt, np.log(seg[idx]), 1)
                r['tau'] = -1 / k_; r['fit_resid'] = float(np.abs(np.polyval([k_, b_], tt) - np.log(seg[idx])).max())
                r['fit_at0'] = float(np.exp(b_))
        res.append(r)
    return res

if __name__ == '__main__':
    jobs = []
    for stage, shapes in (('attack', (0, 1, 2)), ('decay', (1,)), ('release', (1,))):
        for shape in shapes:
            for g in range(0, 128, 4):
                jobs.append((stage, shape, list(range(g, g + 4))))
    jobs.sort(key=lambda j: -max(TABLE[v] for v in j[2]))  # long ones first
    out = []
    with cf.ProcessPoolExecutor(max_workers=int(sys.argv[1]) if len(sys.argv) > 1 else 8) as ex:
        for i, res in enumerate(ex.map(job, jobs)):
            out += res
            print(f'{i+1}/{len(jobs)}', res[0]['stage'], res[0]['shape'], res[0]['value'], flush=True)
    json.dump(out, open(f'{OUT}/sweep.json', 'w'), indent=1)
