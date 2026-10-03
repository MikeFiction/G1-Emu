"""Waveforms for the clipping study: a sine at the cutoff through LP, BP and HP at once, at rising
levels, gain control off. Saved with the direct wire for the input level."""
import concurrent.futures as cf
import numpy as np, fresp as f_, fmeasure as m
R = 96000
def sine(freq, amp, sec=0.4):
    n = int(sec * R) // 2; t = np.arange(n) * 2 / R
    return np.repeat(amp * np.sin(2 * np.pi * freq * t), 2)
AMPS = [0.01, 0.03, 0.1, 0.2, 0.3, 0.5, 0.7, 1.0]
def job(args):
    res, amp = args
    y = m.run(f'clip{res}_{amp}', [None] + [f_.fe(t, gainctl=0, freq=60, res=res, slope=0) for t in (0, 1, 2)],
              sine(330.0, amp))
    return res, amp, y
if __name__ == '__main__':
    out = {}
    with cf.ProcessPoolExecutor(max_workers=4) as ex:
        for res, amp, y in ex.map(job, [(r, a) for r in (64, 96, 112) for a in AMPS]):
            out[f'r{res}_a{amp}'] = y.astype(np.float32)
    np.savez(f'{m.OUT}/clipwave.npz', **out)
    for res in (64, 96, 112):
        row = []
        for a in AMPS:
            y = out[f'r{res}_a{a}'].astype(float)[-20000:]
            d = np.sqrt((y[:, 0] ** 2).mean())
            row.append(' '.join(f'{20*np.log10(np.sqrt((y[:,k]**2).mean())/d):+6.1f}' for k in (1, 2, 3)))
        print(f'res {res}: gain LP BP HP (dB) per input level ' + ' | '.join(f'{a}:{r}' for a, r in zip(AMPS, row)))
