#!/usr/bin/env python3
"""Every resonance step at the editor's 330 Hz: qsweep.py <gain control> <slope>."""
import sys, concurrent.futures as cf
import numpy as np, fresp as f, fmeasure as m
gc, slope = int(sys.argv[1]), int(sys.argv[2])
def job(vals):
    # Less level with gain control off: the resonant peak reaches +46 dB.
    fr, Hs = f.measure([f.fe(0, gainctl=gc, freq=60, res=r, slope=slope) for r in vals],
                       amp=0.02 if gc else 0.003, seconds=8.0, nseg=65536, tag=f'q{gc}{slope}_{vals[0]}')
    return vals, fr, Hs
if __name__ == '__main__':
    groups = [list(range(g, min(g + 3, 128))) for g in range(0, 128, 3)]
    out = {}
    with cf.ProcessPoolExecutor(max_workers=3) as ex:
        for vals, fr, Hs in ex.map(job, groups):
            for v, H in zip(vals, Hs): out[f'v{v}'] = H
    np.savez(f'{m.OUT}/q_s{slope}_gc{gc}.npz', fr=fr, **out)
    print('saved', len(out))
