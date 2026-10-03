#!/usr/bin/env python3
"""Every cutoff step of FilterE at a fixed resonance: cutsweep.py <res> <gain control> <slope> <type>.
Three filters per run (out 1 is the direct wire), runs in parallel."""
import sys, concurrent.futures as cf
import numpy as np, fresp as f, fmeasure as m
res, gc, slope, ftype = (int(a) for a in sys.argv[1:5])
def job(vals):
    fr, Hs = f.measure([f.fe(ftype, gainctl=gc, freq=v, res=res, slope=slope) for v in vals],
                       amp=0.02, seconds=4.0, nseg=16384, tag=f'cut{ftype}{slope}{res}_{vals[0]}')
    return vals, fr, Hs
if __name__ == '__main__':
    groups = [list(range(g, min(g + 3, 128))) for g in range(0, 128, 3)]
    out = {}
    with cf.ProcessPoolExecutor(max_workers=4) as ex:
        for vals, fr, Hs in ex.map(job, groups):
            for v, H in zip(vals, Hs): out[f'v{v}'] = H
    np.savez(f'{m.OUT}/cut_t{ftype}_s{slope}_r{res}_gc{gc}.npz', fr=fr, **out)
    print('saved', len(out))
