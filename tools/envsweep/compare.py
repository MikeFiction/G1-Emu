#!/usr/bin/env python3
"""Emulator vs ADSRtek on the same settings and the same gates."""
import subprocess, os, sys
import numpy as np
import measure as m
FS = 0.051041483879089355; R = 96000
HERE = __import__('measure').OUT
CASES = [
    # name, shape, A, D, S, R, events, seconds, mode
    ('pluck lin',       1, 10, 70, 0,   60, '+0.05,-0.60', 1.2, 0),
    ('pad log',         0, 80, 90, 100, 85, '+0.05,-2.00', 4.0, 0),
    ('exp swell',       2, 75, 60, 64,  70, '+0.05,-0.80', 1.6, 0),
    ('short gate mid-attack', 1, 70, 60, 100, 60, '+0.05,-0.15', 0.8, 0),
    ('retrigger in release log', 0, 60, 50, 90, 70, '+0.05,-0.40,+0.47,-0.90', 1.6, 0),
    ('retrigger in release exp', 2, 60, 50, 90, 70, '+0.05,-0.40,+0.47,-0.90', 1.6, 0),
    ('fast snap',       1, 0,  40, 0,   30, '+0.05,-0.10,+0.20,-0.25', 0.5, 0),
    ('sustain change',  1, 20, 55, 30,  55, '+0.05,-0.50', 0.9, 0),
]
def emu(c, k):
    name, shape, A, D, S, Rl, ev, sec, mode = c
    x, _ = m.run(f'cmp{k}', 20, [shape, A, D, S, Rl, 0], 1, 0, seconds=sec, events=ev)
    return x[:, 0] / FS
def vcv(c, k, steps=1):
    name, shape, A, D, S, Rl, ev, sec, mode = c
    f = f'{HERE}/vcv{k}.f32'
    subprocess.run([os.path.join(HERE, 'vcvrun'), str(shape), str(A), str(D), str(S), str(Rl), ev, str(sec), f, str(steps), str(mode)], check=True)
    return np.fromfile(f, dtype=np.float32).astype(np.float64)
def reaction_times(e, events):
    """When the emulator actually reacted to each event: the note reaches the OS
    through the editor's port with a latency that varies from event to event, so
    each one is found on the curve itself. A press is where the curve starts to
    rise, a release where it starts to fall; one that changes nothing visible
    (a release in mid-decay) takes the median latency of the others."""
    out, lat = [], []
    for tok in events.split(','):
        t = float(tok[1:]); press = tok[0] == '+'
        i0 = int(t * R); seg = e[i0:i0 + int(0.04 * R)]
        d = seg[4:] - seg[:-4]
        hit = np.nonzero(d > 1e-6)[0] if press else np.nonzero(d < -1e-6)[0]
        if len(hit) and not (not press and (e[i0 - 8] - e[i0 - 4]) > 1e-6):
            out.append([tok[0], (i0 + hit[0]) / R]); lat.append(hit[0] / R)
        else:
            out.append([tok[0], None])
    med = float(np.median(lat)) if lat else 0.004
    return ','.join(f'{p}{(tt if tt is not None else float(tok[1:]) + med):.6f}'
                    for (p, tt), tok in zip(out, events.split(',')))

if __name__ == '__main__':
    steps = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    for k, c in enumerate(CASES):
        e = emu(c, k)
        ev = reaction_times(e, c[6])
        v = vcv(c[:6] + (ev,) + c[7:], k, steps)
        n = min(len(e), len(v))
        d = e[:n] - v[:n]
        print(f'{c[0]:28s} max err {np.abs(d).max():.4f}  rms {np.sqrt((d**2).mean()):.5f}  at {np.argmax(np.abs(d))/R*1e3:.1f} ms')
