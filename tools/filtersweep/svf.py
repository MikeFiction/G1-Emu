"""Chamberlin state-variable filter: simulation and closed-form responses.
    lp[n] = lp[n-1] + f*bp[n-1]
    hp[n] = x[n] - lp[n] - q*bp[n-1]
    bp[n] = bp[n-1] + f*hp[n]
with f = 2 sin(pi fc / fs) and q = 1/Q."""
import numpy as np
def sim(x, f, q):
    lp = bp = 0.0; out = {'lp': [], 'bp': [], 'hp': []}
    for v in x:
        lp = lp + f * bp
        hp = v - lp - q * bp
        bp = bp + f * hp
        out['lp'].append(lp); out['bp'].append(bp); out['hp'].append(hp)
    return {k: np.array(a) for k, a in out.items()}
def resp(f, q, freqs, fs=96000):
    z1 = np.exp(-2j * np.pi * freqs / fs)
    D = 1 - (2 - f * q - f * f) * z1 + (1 - f * q) * z1 * z1
    return {'lp': f * f * z1 / D, 'bp': f * (1 - z1) / D, 'hp': (1 - z1) ** 2 / D}
