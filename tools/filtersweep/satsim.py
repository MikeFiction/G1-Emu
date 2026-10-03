"""Chamberlin with saturating states, with the gain control's input gain at different places."""
import numpy as np, json, fmeasure as m
R = 96000
Q12 = json.load(open(f'{m.OUT}/qfit_gc1.json')); Q24 = json.load(open(f'{m.OUT}/q24.json'))
GAIN = json.load(open(f'{m.OUT}/gain_gc1.json'))
def run(x, freq, res, slope, gc, place, kind=0):
    fc = 330 * 2 ** ((freq - 60) / 12); f = 2 * np.sin(np.pi * fc / R)
    q0 = (Q24 if slope else Q12)[str(res)]['q0']; q = q0 * (1 - f / 2)
    g = 10 ** (GAIN[str(res)] / 20) if gc else 1.0
    c = lambda v: -1.0 if v < -1 else (1.0 if v > 1 else v)
    st = [[0.0, 0.0], [0.0, 0.0]]; out = np.empty(len(x))
    def sec(s, v):
        lp = c(s[0] + f * s[1]); hp = c(v - lp - q * s[1]); bp = c(s[1] + f * hp)
        s[0], s[1] = lp, bp
        return (lp, bp, hp)[kind]
    for i, v in enumerate(x):
        if place == 'input': v *= g
        y = sec(st[0], v)
        if slope:
            if place == 'between': y *= g
            y = sec(st[1], y)
        if place == 'output': y *= g
        out[i] = y
    return out
