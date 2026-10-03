"""Fits the Chamberlin LP to each cutoff step: f, q and a gain, by magnitude; reports the phase."""
import sys, json
import numpy as np, svf, fmeasure as m
from fit import grid_fit, dB
def fit_one(fr, H):
    keep = (fr > 5) & (fr < 40000)
    fr, H = fr[keep], H[keep]
    pk = int(np.argmax(np.abs(H)))
    idx = np.unique(np.concatenate([np.searchsorted(fr, np.geomspace(5, 40000, 500)).clip(0, len(fr) - 1),
                                    np.arange(max(0, pk - 60), min(len(fr), pk + 60))]))
    w, Hs = fr[idx], H[idx]
    ok = dB(Hs) > dB(Hs).max() - 50
    def cost(p):
        r = svf.resp(np.exp(p[0]), np.exp(p[1]), w)['lp']
        return np.abs(dB(Hs) - dB(r) - p[2])[ok].max()
    fc_guess = fr[pk]
    f0 = np.log(2 * np.sin(np.pi * min(fc_guess, 30000) / 96000))
    p, c = grid_fit(cost, [(f0 - 1.5, f0 + 1.5), (np.log(0.002), np.log(3)), (-20, 20)], iters=7, n=17)
    f, q = np.exp(p[0]), np.exp(p[1])
    r = svf.resp(f, q, w)['lp'] * 10 ** (p[2] / 20)
    # A constant extra delay (the filter runs further down the DSP chain than the direct wire)
    # is taken out before judging the phase.
    ph = np.unwrap(np.angle(Hs / r)); k = np.polyfit(w[ok], ph[ok], 1)
    delay = -k[0] / (2 * np.pi) * 96000
    resid = np.degrees(np.abs(ph - np.polyval(k, w)))[ok].max()
    return f, q, p[2], c, resid, delay
if __name__ == '__main__':
    d = np.load(f'{m.OUT}/{sys.argv[1]}')
    fr = d['fr']; res = {}
    for v in range(128):
        f, q, g, c, ph, delay = fit_one(fr, d[f'v{v}'])
        fc = 96000 / np.pi * np.arcsin(min(f / 2, 1))
        res[v] = dict(f=f, q=q, g=g, err=c, phase=ph, delay=delay, fc=fc)
        if v % 8 == 0 or v > 119:
            print(f'v {v:3d}: f {f:.5f} (fc {fc:8.1f} Hz, editor {330*2**((v-60)/12):8.1f})  Q {1/q:6.2f}  g {g:5.2f}  err {c:5.2f} dB  phase {ph:5.1f} deg  delay {delay:5.2f}')
    json.dump(res, open(f'{m.OUT}/{sys.argv[1]}.fit.json', 'w'), indent=1)
