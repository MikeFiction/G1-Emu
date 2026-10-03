"""Gain control table: with the cutoff at 3.3 kHz, the passband level (40-400 Hz) of the LP is
the input gain gain control applies, since the Chamberlin LP has unity gain at DC."""
import json, concurrent.futures as cf
import numpy as np, fresp as f_, fmeasure as m
def job(vals):
    fr, Hs = f_.measure([f_.fe(0, gainctl=1, freq=100, res=r, slope=0) for r in vals],
                        amp=0.05, seconds=3.0, nseg=16384, tag=f'gain_{vals[0]}')
    band = (fr > 40) & (fr < 400)
    return {r: float(20 * np.log10(np.abs(H[band]).mean())) for r, H in zip(vals, Hs)}
if __name__ == '__main__':
    out = {}
    with cf.ProcessPoolExecutor(max_workers=4) as ex:
        for d in ex.map(job, [list(range(g, min(g + 3, 128))) for g in range(0, 128, 3)]):
            out.update(d)
    json.dump(out, open(f'{m.OUT}/gain_gc1.json', 'w'), indent=1)
    print(' '.join(f'{r}:{out[r]:.2f}' for r in range(0, 128, 8)), '| 120:', round(out[120], 2), '124:', round(out[124], 2), '127:', round(out[127], 2))
