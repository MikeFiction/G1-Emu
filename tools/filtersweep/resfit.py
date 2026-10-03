"""Fits each resonance step of a LP12 sweep to g * lp2(fc, Q): resfit.py <gain control 0|1>."""
import numpy as np, sys
import fmeasure
from fit import *
RES=[0,16,32,48,64,80,96,104,112,116,120,122,124,126,127]
def pick(fr,H,lo=20,hi=8000,n=600):
    m=(fr>lo)&(fr<hi); fr,H=fr[m],H[m]
    pk=np.argmax(np.abs(H))
    idx=np.unique(np.concatenate([np.searchsorted(fr,np.geomspace(lo,hi,n)).clip(0,len(fr)-1),
                                 np.arange(max(0,pk-200),min(len(fr),pk+200))]))
    return fr[idx],H[idx]
def fit_lp12(fr,H):
    w,Hs=pick(fr,H); s=1j*2*np.pi*w
    ok=dB(Hs)>dB(Hs).max()-60
    def cost(fc,Q,g): return np.abs(dB(Hs)-dB(lp2(fc,Q,s))-g)[ok].max()
    p,c=grid_fit(lambda p: cost(p[0],np.exp(p[1]),p[2]),[(250,450),(np.log(0.3),np.log(300)),(-60,30)],iters=8,n=19)
    return p[0],np.exp(p[1]),p[2],c
if __name__=='__main__':
    gc=int(sys.argv[1])
    d=np.load(f'{fmeasure.OUT}/res_gc{gc}_s0.npz'); fr=d['fr']
    print(f'LP12 gain control {"on" if gc else "off"}: H = g * lp2(fc, Q)')
    for r in RES:
        fc,Q,g,c=fit_lp12(fr,d[f'r{r}'])
        print(f'  res {r:3d}: fc {fc:6.1f} Q {Q:8.3f}  g {g:6.2f} dB  (-20log Q {-20*np.log10(Q):6.2f})  err {c:5.2f}')
