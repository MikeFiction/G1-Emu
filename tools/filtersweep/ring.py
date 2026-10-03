"""Q from the ring-down of an impulse. For the Chamberlin SVF the poles have radius
sqrt(1 - f q), so the envelope falls by (1 - f q)/2 in log per sample: q = (1 - r^2) / f."""
import sys, json, concurrent.futures as cf
import numpy as np, fresp as f_, fmeasure as m
R = 96000
def impulse(sec=6.0, amp=0.5):
    x = np.zeros(int(sec * R)); x[2000] = amp; x[2001] = amp; return x
def ring_q(y, f):
    """Fits the decay of the peak envelope between a third of its start and ten times the
    noise floor, with blocks short enough for fast decays and long enough to hold a cycle."""
    a = np.abs(y)
    start = int(np.argmax(a)) + 50
    seg = a[start:]
    period = max(4, int(round(np.pi / np.arcsin(min(f / 2, 1)))))   # samples per cycle
    blk = max(period, 8)
    n = len(seg) // blk
    env = seg[:n * blk].reshape(n, blk).max(1)
    t = np.arange(n) * blk + blk / 2
    floor = np.median(env[-max(5, n // 10):])
    top = env[0]
    ok = (env < top / 2) & (env > max(floor * 10, top * 1e-4))
    if ok.sum() < 4:
        return float('nan')
    k = np.polyfit(t[ok], np.log(env[ok]), 1)[0]
    return (1 - np.exp(2 * k)) / f

def job(args):
    v, vals, gc = args
    y = m.run(f'ring{v}_{vals[0]}', [f_.fe(0, gainctl=gc, freq=v, res=r, slope=0) for r in vals],
              impulse(), extra=0.1)
    return v, vals, y
if __name__ == '__main__':
    gc = 1
    freqs = [int(a) for a in sys.argv[1:]] or [60, 100, 118]
    RES = list(range(64, 128))
    jobs = [(v, RES[i:i + 4], gc) for v in freqs for i in range(0, len(RES), 4)]
    out = {}
    with cf.ProcessPoolExecutor(max_workers=4) as ex:
        for v, vals, y in ex.map(job, jobs):
            fc = 330.0 * 2 ** ((v - 60) / 12); f = 2 * np.sin(np.pi * fc / R)
            for k, r in enumerate(vals):
                q = ring_q(y[:, k], f)
                out.setdefault(v, {})[r] = dict(q=q, q0=q / (1 - f / 2), f=f)
    print('res  ' + '  '.join(f'q0 @ v{v:3d}' for v in freqs))
    for r in RES:
        print(f'{r:3d}  ' + '  '.join(f'{out[v][r]["q0"]:9.6f}' for v in freqs))
    json.dump({str(v): out[v] for v in out}, open(f'{m.OUT}/ring.json', 'w'), indent=1)
