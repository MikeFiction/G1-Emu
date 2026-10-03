"""q0 per resonance step from the qsweep at 330 Hz, assuming q = q0 * (1 - f/2) and f from
the editor's table. Fits q and a gain per step; with gain control on, the gain is its table."""
import sys, json
import numpy as np, svf, fmeasure as m
from fit import grid_fit, dB
def fit(fr, H, f):
    keep = (fr > 20) & (fr < 20000); fr, H = fr[keep], H[keep]
    pk = int(np.argmax(np.abs(H)))
    idx = np.unique(np.concatenate([np.searchsorted(fr, np.geomspace(20, 20000, 400)).clip(0, len(fr) - 1),
                                    np.arange(max(0, pk - 40), min(len(fr), pk + 40))]))
    w, Hs = fr[idx], H[idx]; ok = dB(Hs) > dB(Hs).max() - 50
    def cost(p): return np.abs(dB(Hs) - dB(svf.resp(f, np.exp(p[0]), w)['lp']) - p[1])[ok].max()
    p, c = grid_fit(cost, [(np.log(0.002), np.log(3)), (-60, 20)], iters=8, n=31)
    return np.exp(p[0]), p[1], c
if __name__ == '__main__':
    gc = int(sys.argv[1])
    d = np.load(f'{m.OUT}/q_s0_gc{gc}.npz'); fr = d['fr']
    f = 2 * np.sin(np.pi * 330.0 / 96000)
    out = {}
    for r in range(128):
        q, g, c = fit(fr, d[f'v{r}'], f)
        out[r] = dict(q0=q / (1 - f / 2), g=g, err=c)
        if r % 8 == 0 or r > 119:
            print(f'res {r:3d}: q0 {out[r]["q0"]:.5f} (Q {1/out[r]["q0"]:7.2f})  gain {g:6.2f} dB  err {c:5.2f} dB')
    json.dump(out, open(f'{m.OUT}/qfit_gc{gc}.json', 'w'), indent=1)
