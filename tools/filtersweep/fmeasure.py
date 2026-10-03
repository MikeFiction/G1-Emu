#!/usr/bin/env python3
"""Run a signal through G1 modules in the emulator: AudioIn L -> module(s) -> outs 1-4."""
import os, subprocess, sys, struct
import numpy as np
G1=os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),'..','..'))
sys.path.insert(0, G1+'/tools/battery'); import battery as b
MODS=b.read_modules(G1+'/../Nomad2026/data/modules.xml')
BIN=G1+'/build/tools/patchtest/g1patchtest_artefacts/Release/g1patchtest'
ROM=G1+'/Roms/NORD-MODULAR-RACK-VER-3.03.BIN'
OUT=os.environ.get('FILTERSWEEP_OUT', os.path.join(G1,'build','filtersweep'))
os.makedirs(OUT, exist_ok=True)
R=96000

def patch(chains):
    """chains: up to 4 entries, each (type, params, in_conn, out_conn) or None for a direct wire."""
    mods=[(1,2),(2,3)]; names={1:'AudioIn',2:'4Output'}; params=["2 3 1 100 "]; cables=[]
    for k,ch in enumerate(chains):
        if ch is None:
            cables.append(f"0 2 {k} 0 1 0 1"); continue
        t,p,ic,oc=ch; i=3+k; mods.append((i,t)); names[i]=f"{MODS[t].name}{k}"
        cables+= [f"0 {i} {ic} 0 1 0 1", f"0 2 {k} 0 {i} {oc} 1"]
        params.append(f"{i} {t} {len(p)} "+" ".join(map(str,p))+" ")
    L=["[Header]","Version=Nord Modular patch 3.0",b.HEADER,"[/Header]","[ModuleDump]","1 "]
    for row,(i,t) in enumerate(mods): L.append(f"{i} {t} {2+(row%2)*8} {2+row*4} ")
    L+=["[/ModuleDump]","[ModuleDump]","0 ","[/ModuleDump]","[CurrentNoteDump]","64 0 0 64 0 0 ","[/CurrentNoteDump]","[CableDump]","1 "]+[c+" " for c in cables]
    L+=["[/CableDump]","[CableDump]","0 ","[/CableDump]","[ParameterDump]","1 "]+params
    L+=["[/ParameterDump]","[ParameterDump]","0 ","[/ParameterDump]","[MorphMapDump]","0 0 0 0 ","[/MorphMapDump]",
        "[KeyboardAssignment]","0 0 0 0 ","[/KeyboardAssignment]","[KnobMapDump]","[/KnobMapDump]","[CtrlMapDump]","[/CtrlMapDump]","[NameDump]","1 "]
    for i,_ in mods: L.append(f"{i} {names[i]}")
    L+=["[/NameDump]","[NameDump]","0 ","[/NameDump]"]
    return "\n".join(L)+"\n"

def read_wav(wav):
    d=open(wav,'rb').read(); i=d.index(b'fmt '); ch=struct.unpack('<H',d[i+10:i+12])[0]; bits=struct.unpack('<H',d[i+22:i+24])[0]
    j=d.index(b'data'); n=struct.unpack('<I',d[j+4:j+8])[0]; raw_=d[j+8:j+8+n]
    if bits==32: a=np.frombuffer(raw_,dtype='<i4').astype(np.float64)/2**31
    elif bits==16: a=np.frombuffer(raw_,dtype='<i2').astype(np.float64)/2**15
    else:
        a=np.frombuffer(raw_,dtype=np.uint8).reshape(-1,3); a=(a[:,0].astype(np.int32)|(a[:,1].astype(np.int32)<<8)|(a[:,2].astype(np.int32)<<16)); a=np.where(a>=2**23,a-2**24,a).astype(np.float64)/2**23
    return a.reshape(-1,ch)

def run(tag, chains, signal, extra=0.2):
    pch=f"{OUT}/{tag}.pch"; wav=f"{OUT}/{tag}.wav"; raw=f"{OUT}/{tag}.f32"
    open(pch,'w').write(patch(chains)); np.asarray(signal,dtype=np.float32).tofile(raw)
    sec=len(signal)/R+extra
    r=subprocess.run([BIN,ROM,pch,'--seconds',f'{sec:.4f}','--note','-1','--input-raw',raw,'--wav',wav],capture_output=True,text=True,timeout=900,cwd=G1)
    if not os.path.exists(wav): print(r.stdout[-1500:],r.stderr[-1500:]); raise SystemExit
    d=open(wav,'rb').read(); i=d.index(b'fmt '); ch=struct.unpack('<H',d[i+10:i+12])[0]; bits=struct.unpack('<H',d[i+22:i+24])[0]
    j=d.index(b'data'); n=struct.unpack('<I',d[j+4:j+8])[0]; raw_=d[j+8:j+8+n]
    if bits==32: a=np.frombuffer(raw_,dtype='<i4').astype(np.float64)/2**31
    elif bits==16: a=np.frombuffer(raw_,dtype='<i2').astype(np.float64)/2**15
    else:
        a=np.frombuffer(raw_,dtype=np.uint8).reshape(-1,3); a=(a[:,0].astype(np.int32)|(a[:,1].astype(np.int32)<<8)|(a[:,2].astype(np.int32)<<16)); a=np.where(a>=2**23,a-2**24,a).astype(np.float64)/2**23
    for f in (wav,raw,pch): os.remove(f)
    return a.reshape(-1,ch)
