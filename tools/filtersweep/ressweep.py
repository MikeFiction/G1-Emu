"""Resonance sweep of a FilterE LP: ressweep.py <gain control 0|1> <slope 0|1>."""
import numpy as np, fresp as f, sys
RES=[0,16,32,48,64,80,96,104,112,116,120,122,124,126,127]
gc=int(sys.argv[1]); slope=int(sys.argv[2])
out={}
for i in range(0,len(RES),3):
    rs=RES[i:i+3]
    fr,Hs=f.measure([f.fe(0,gainctl=gc,slope=slope,res=r) for r in rs],amp=0.02 if gc else 0.004,seconds=8.0,nseg=65536,tag=f'rs{gc}{slope}_{i}')
    for r,H in zip(rs,Hs): out[f'r{r}']=H
np.savez(f'{f.m.OUT}/res_gc{gc}_s{slope}.npz',fr=fr,**out)
print('done',gc,slope)
