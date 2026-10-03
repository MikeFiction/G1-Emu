"""High resonance steps measured at a higher cutoff, where the peak is wide enough to resolve:
qhigh.py <freq step> <gain control>. Fits q0 = q / (1 - f/2) and the gain."""
import sys, json, concurrent.futures as cf
import numpy as np, fresp as f_, fmeasure as m, qfit
v, gc = int(sys.argv[1]), int(sys.argv[2])
RES = list(range(108, 128))
def job(vals):
    fr, Hs = f_.measure([f_.fe(0, gainctl=gc, freq=v, res=r, slope=0) for r in vals],
                        amp=0.02 if gc else 0.002, seconds=8.0, nseg=65536, tag=f'qh{v}{gc}_{vals[0]}')
    return vals, fr, Hs
if __name__ == '__main__':
    fc = 330.0 * 2 ** ((v - 60) / 12); f = 2 * np.sin(np.pi * fc / 96000)
    groups = [RES[i:i + 3] for i in range(0, len(RES), 3)]
    out = {}
    with cf.ProcessPoolExecutor(max_workers=4) as ex:
        for vals, fr, Hs in ex.map(job, groups):
            for r, H in zip(vals, Hs):
                q, g, c = qfit.fit(fr, H, f)
                out[r] = dict(q0=q / (1 - f / 2), g=g, err=c)
    for r in RES:
        print(f'res {r:3d}: q0 {out[r]["q0"]:.5f} (Q {1/out[r]["q0"]:7.2f})  gain {out[r]["g"]:6.2f} dB  err {out[r]["err"]:5.2f} dB')
    json.dump(out, open(f'{m.OUT}/qhigh_v{v}_gc{gc}.json', 'w'), indent=1)
