#!/usr/bin/env python3
"""Emulator against FILTERtek on the same input: frun must be built in build/filtersweep."""
import os, subprocess
import numpy as np, fmeasure as m, fresp as f_
R = 96000; DIRECT = 0.2042
FRUN = os.path.join(m.OUT, 'frun')
def sine(freq, amp, sec=1.0):
    n = int(sec * R) // 2; t = np.arange(n) * 2 / R
    return np.repeat(amp * np.sin(2 * np.pi * freq * t), 2)
def saw(freq, amp, sec=1.0):
    n = int(sec * R) // 2; t = np.arange(n) * 2 / R
    return np.repeat(amp * (2 * ((freq * t) % 1.0) - 1), 2)
CASES = [  # name, type, slope, gain control, freq, res, signal
    ('LP24 open, noise',            0, 1, 1, 70, 0,   f_.noise(1.0, 0.2)),
    ('LP12 res 112, noise',         0, 0, 1, 60, 112, f_.noise(1.0, 0.2)),
    ('BP12 res 96 gc off, noise',   1, 0, 0, 80, 96,  f_.noise(1.0, 0.05)),
    ('HP24 res 64, noise',          2, 1, 1, 90, 64,  f_.noise(1.0, 0.3)),
    ('BR12 res 100, noise',         3, 0, 1, 75, 100, f_.noise(1.0, 0.2)),
    ('LP12 res 112 gc off, clip',   0, 0, 0, 60, 112, sine(330.0, 0.3)),
    ('LP24 res 124, saw clip',      0, 1, 1, 84, 124, saw(110.0, 0.9)),
    ('BP24 res 120 gc off, saw',    1, 1, 0, 72, 120, saw(220.0, 0.2)),
    ('BR24 res 96, noise',          3, 1, 1, 80, 96,  f_.noise(1.0, 0.2)),
    ('LP24 res 96 driven saw',      0, 1, 1, 70, 96,  saw(110.0, 0.9)),
]
def emu(c, k):
    name, t, sl, gc, fr, res, x = c
    y = m.run(f'fc{k}', [None, f_.fe(t, gainctl=gc, freq=fr, res=res, slope=sl)], x)
    return y[:, 1] / DIRECT
def vcv(c, k):
    name, t, sl, gc, fr, res, x = c
    i, o = f'{m.OUT}/fc{k}_in.f32', f'{m.OUT}/fc{k}_out.f32'
    np.asarray(x, dtype=np.float32).tofile(i)
    subprocess.run([FRUN, str(t), str(sl), str(gc), str(fr), str(res), i, o], check=True)
    y = np.fromfile(o, dtype=np.float32).astype(float); os.remove(i); os.remove(o); return y
def harmonics(y, f0, n=24):
    y = y[9600:9600 + 2 ** 15]; sp = np.abs(np.fft.rfft(y * np.hanning(len(y)))); fr = np.fft.rfftfreq(len(y), 1 / R)
    return np.array([sp[np.argmin(abs(fr - f0 * k)) - 2:np.argmin(abs(fr - f0 * k)) + 3].max() for k in range(1, n + 1)])
if __name__ == '__main__':
    for k, c in enumerate(CASES):
        e = emu(c, k); v = vcv(c, k)
        n = len(v) - 200
        # The emulator adds its chain latency; take the lag with the least error (the input
        # arrives in pairs of samples, so the correlation peak alone can sit one off).
        def err_at(L):
            a, b = e[L:L + n - 100][9600:], v[:n - 100][9600:]
            return 20 * np.log10(np.sqrt(((a - b) ** 2).mean()) / np.sqrt((a ** 2).mean())), L
        rel, lag = min(err_at(L) for L in range(20, 60))
        a = e[lag:lag + n - 100][9600:]; b = v[:n - 100][9600:]
        line = f'{c[0]:30s} lag {lag}  error {rel:6.1f} dB below the signal (peak {np.abs(a).max():.3f})'
        if 'saw' in c[0] or 'clip' in c[0]:
            f0 = 330.0 if 'clip' in c[0] else (220.0 if '220' in str(c) else 110.0)
            f0 = 110.0 if 'saw' in c[0] and c[4] in (84, 70) else f0
            he, hv = harmonics(e[lag:], f0), harmonics(v, f0); big = he > he.max() * 0.01
            d = np.abs(20 * np.log10((hv + 1e-12) / (he + 1e-12)))[big]
            line += f' | harmonics: median {np.median(d):.2f} dB, worst {d.max():.2f} dB; rms {np.sqrt((a**2).mean()):.4f} vs {np.sqrt((b**2).mean()):.4f}'
        print(line)
