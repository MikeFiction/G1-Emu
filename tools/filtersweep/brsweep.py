"""Band reject: its notch has a damping of its own. Fits q per resonance step (gain control off)
with the Chamberlin notch (hp + lp) at 1049 Hz, for 12 dB (one) and 24 dB (two in cascade)."""
import json, sys, concurrent.futures as cf
import numpy as np, fresp as f_, fmeasure as m, svf
from fit import grid_fit, dB
V = 80; FC = 330 * 2 ** ((V - 60) / 12); F = 2 * np.sin(np.pi * FC / 96000)
RES = list(range(0, 128, 4)) + [126, 127]
def fit(fr, H, sections):
    k = (fr > 30) & (fr < 20000); w = np.geomspace(30, 20000, 500)
    Hm = np.interp(w, fr[k], np.abs(H[k])); ok = dB(Hm) > -40
    def cost(p):
        r = svf.resp(F, np.exp(p[0]), w); n = (r['hp'] + r['lp']) ** sections
        return np.abs(dB(Hm) - dB(n) - p[1])[ok].max()
    p, c = grid_fit(cost, [(np.log(0.01), np.log(4)), (-10, 10)], iters=8, n=31)
    return np.exp(p[0]), p[1], c
def job(args):
    slope, vals = args
    fr, Hs = f_.measure([f_.fe(3, gainctl=0, freq=V, res=r, slope=slope) for r in vals],
                        amp=0.05, seconds=3.0, nseg=16384, tag=f'br{slope}_{vals[0]}')
    return slope, {r: fit(fr, H, 1 + slope) for r, H in zip(vals, Hs)}
if __name__ == '__main__':
    out = {0: {}, 1: {}}
    jobs = [(s, RES[i:i + 3]) for s in (0, 1) for i in range(0, len(RES), 3)]
    with cf.ProcessPoolExecutor(max_workers=4) as ex:
        for s, d in ex.map(job, jobs):
            out[s].update(d)
    json.dump({s: {r: dict(q0=v[0] / (1 - F / 2), g=v[1], err=v[2]) for r, v in out[s].items()} for s in out},
              open(f'{m.OUT}/br.json', 'w'), indent=1)
    for r in RES:
        a, b = out[0][r], out[1][r]
        print(f'res {r:3d}: 12 dB q0 {a[0]/(1-F/2):.4f} (Q {(1-F/2)/a[0]:5.2f}, err {a[2]:.2f})   24 dB q0 {b[0]/(1-F/2):.4f} (Q {(1-F/2)/b[0]:5.2f}, err {b[2]:.2f})')
