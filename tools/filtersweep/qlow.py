"""Low resonance q0, 12 and 24 dB, measured with gain control off at two cutoffs (steps 70 and
100) with plenty of level: more precise than the high-resonance sweeps were down there."""
import json, concurrent.futures as cf
import numpy as np, fresp as f_, fmeasure as m, svf
from fit import grid_fit, dB
RES = [0, 8, 16, 24, 32, 40, 48, 56, 64]
def job(args):
    slope, v, vals = args
    fc = 330 * 2 ** ((v - 60) / 12) * 0.99876; f = 2 * np.sin(np.pi * fc / 96000)
    fr, Hs = f_.measure([f_.fe(0, gainctl=0, freq=v, res=r, slope=slope) for r in vals], amp=0.2, seconds=4.0, nseg=16384, tag=f'ql{slope}{v}_{vals[0]}')
    out = {}
    for r, H in zip(vals, Hs):
        k = (fr > 20) & (fr < 20000); w = np.geomspace(20, 20000, 500); Hm = np.interp(w, fr[k], np.abs(H[k])); ok = dB(Hm) > -60
        def cost(p): return np.abs(dB(Hm) - (1 + slope) * dB(svf.resp(f, np.exp(p[0]), w)['lp']))[ok].max()
        p, c = grid_fit(cost, [(np.log(0.2), np.log(3))], iters=8, n=41)
        out[r] = (float(np.exp(p[0]) / (1 - f / 2)), float(c))
    return slope, v, out
if __name__ == '__main__':
    res = {0: {}, 1: {}}
    jobs = [(s, v, RES[i:i + 3]) for s in (0, 1) for v in (70, 100) for i in range(0, len(RES), 3)]
    with cf.ProcessPoolExecutor(max_workers=4) as ex:
        for s, v, d in ex.map(job, jobs):
            for r, (q, c) in d.items():
                res[s].setdefault(r, []).append((q, c))
    final = {s: {r: float(np.mean([q for q, _ in vals])) for r, vals in res[s].items()} for s in res}
    json.dump(final, open(f'{m.OUT}/qlow.json', 'w'), indent=1)
    for s in (0, 1):
        print(f'{12*(1+s)} dB: ' + '  '.join(f'res {r}: {final[s][r]:.4f} ({res[s][r][0][0]:.4f}/{res[s][r][1][0]:.4f})' for r in RES))
