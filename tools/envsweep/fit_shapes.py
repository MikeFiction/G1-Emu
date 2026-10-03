#!/usr/bin/env python3
"""Fits the Log and Exp attacks, per knob step, to one parameter each and writes
shape_params.json next to the curves.

Log: y = T(1 - e^(-a t)), an approach to a target T above full scale, a = ln(T/(T-1)).
Exp: y = r(e^(a t) - 1), growth from zero with offset r, a = ln(1 + 1/r).
t runs 0..1 over the measured attack time, so each family has one free number."""
import json, os, warnings
import numpy as np
import measure as m
warnings.filterwarnings('ignore')

def curve(shape, v):
    g = v // 4 * 4
    d = np.load(f'{m.OUT}/curves/attack_s{shape}_{g:03d}.npz')
    c = d['y'][:, list(d['vals']).index(v)].astype(np.float64)
    on = int(np.argmax(c > 0)) - 1
    top = int(np.argmax(c >= 0.9999))
    c = c[on:top + 1]
    t = np.arange(len(c)) / (len(c) - 1)
    if len(c) > 2000:
        idx = np.linspace(0, len(c) - 1, 2000).astype(int)
        c, t = c[idx], t[idx]
    return c, t

def family(shape, P, t):
    P = P[:, None]; t = t[None, :]
    if shape == 0:
        a = np.log(P / (P - 1)); return P * (1 - np.exp(-a * t))
    a = np.log(1 + 1 / P); return P * (np.exp(a * t) - 1)

GRIDS = {0: np.exp(np.linspace(np.log(1.0005), np.log(3.0), 3000)),
         2: np.exp(np.linspace(np.log(1e-5), np.log(0.5), 3000))}

if __name__ == '__main__':
    out = {}
    for shape in (0, 2):
        out[shape] = {}
        for v in range(128):
            c, t = curve(shape, v)
            e = np.abs(family(shape, GRIDS[shape], t) - c[None, :]).max(axis=1)
            i = int(np.argmin(e))
            out[shape][v] = (float(GRIDS[shape][i]), float(e[i]))
        worst = max(out[shape].items(), key=lambda kv: kv[1][1])
        print(f'shape {shape}: worst max error {worst[1][1]:.4f} at step {worst[0]}')
    json.dump(out, open(f'{m.OUT}/shape_params.json', 'w'))
