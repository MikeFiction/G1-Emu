"""24 dB slope: q per section (model: the same Chamberlin LP twice) and the gain, per resonance
step, at 3.3 kHz with gain control on."""
import json, concurrent.futures as cf
import numpy as np, fresp as f_, fmeasure as m, svf
from fit import grid_fit, dB
V = 100
FC = 330.0 * 2 ** ((V - 60) / 12); F = 2 * np.sin(np.pi * FC / 96000)
def fit(fr, H):
    keep = (fr > 40) & (fr < 30000); fr, H = fr[keep], H[keep]
    pk = int(np.argmax(np.abs(H)))
    idx = np.unique(np.concatenate([np.searchsorted(fr, np.geomspace(40, 30000, 400)).clip(0, len(fr) - 1),
                                    np.arange(max(0, pk - 40), min(len(fr), pk + 40))]))
    w, Hs = fr[idx], H[idx]; ok = dB(Hs) > dB(Hs).max() - 60
    def cost(p): return np.abs(dB(Hs) - 2 * dB(svf.resp(F, np.exp(p[0]), w)['lp']) - p[1])[ok].max()
    p, c = grid_fit(cost, [(np.log(0.002), np.log(3)), (-60, 20)], iters=8, n=31)
    return np.exp(p[0]), p[1], c
def job(vals):
    fr, Hs = f_.measure([f_.fe(0, gainctl=1, freq=V, res=r, slope=1) for r in vals],
                        amp=0.05, seconds=8.0, nseg=65536, tag=f'q24_{vals[0]}')
    band = (fr > 40) & (fr < 400)
    out = {}
    for r, H in zip(vals, Hs):
        q, g, c = fit(fr, H)
        out[r] = dict(q0=q / (1 - F / 2), g_fit=g, gain=float(20 * np.log10(np.abs(H[band]).mean())), err=c)
    return out
if __name__ == '__main__':
    out = {}
    with cf.ProcessPoolExecutor(max_workers=4) as ex:
        for d in ex.map(job, [list(range(g, min(g + 3, 128))) for g in range(0, 128, 3)]):
            out.update(d)
    json.dump(out, open(f'{m.OUT}/q24.json', 'w'), indent=1)
    for r in list(range(0, 128, 8)) + [120, 124, 126, 127]:
        print(f'res {r:3d}: q0 {out[r]["q0"]:.5f} (Q/section {1/out[r]["q0"]:6.2f})  gain {out[r]["gain"]:6.2f} dB  fit err {out[r]["err"]:.2f}')
