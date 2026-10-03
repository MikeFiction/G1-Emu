"""Coarse-to-fine grid fitting (no scipy needed) and analog prototypes."""
import numpy as np
def grid_fit(cost, ranges, iters=6, n=41):
    """Coarse-to-fine grid search over len(ranges) parameters (log or lin ranges)."""
    lo=[r[0] for r in ranges]; hi=[r[1] for r in ranges]
    best=None
    for _ in range(iters):
        axes=[np.linspace(l,h,n) for l,h in zip(lo,hi)]
        mesh=np.meshgrid(*axes,indexing='ij'); pts=np.stack([g.ravel() for g in mesh],1)
        c=np.array([cost(p) for p in pts]); i=int(np.argmin(c)); best=(pts[i],c[i])
        span=[(h-l)/(n-1)*3 for l,h in zip(lo,hi)]
        lo=[max(r[0],b-sp) for r,b,sp in zip(ranges,best[0],span)]; hi=[min(r[1],b+sp) for r,b,sp in zip(ranges,best[0],span)]
    return best
def lp2(fc,Q,s): x=s/(2*np.pi*fc); return 1/(1+x/Q+x*x)
def hp2(fc,Q,s): x=s/(2*np.pi*fc); return x*x/(1+x/Q+x*x)
def dB(H): return 20*np.log10(np.abs(H)+1e-15)
