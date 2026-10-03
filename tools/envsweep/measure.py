#!/usr/bin/env python3
"""Measure G1 envelopes in the emulator: Keyboard gate -> up to four envelopes -> outs 1-4.
Writes the patch and the WAV under build/envsweep and returns the four outputs (96 kHz).
Full scale (an envelope at 1) reads 0.0510 in g1patchtest's WAV, after its +36 dB."""
import os, subprocess, sys, struct
import numpy as np
G1=os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),'..','..'))
sys.path.insert(0, G1+'/tools/battery'); import battery as b
MODS=b.read_modules(os.path.join(G1,'..','Nomad2026','data','modules.xml'))
BIN=G1+'/build/tools/patchtest/g1patchtest_artefacts/Release/g1patchtest'
ROM=os.environ.get('G1_ROM', G1+'/Roms/NORD-MODULAR-RACK-VER-3.03.BIN')
OUT=os.environ.get('ENVSWEEP_OUT', os.path.join(G1,'build','envsweep'))
os.makedirs(OUT, exist_ok=True)

def patch(env_type, params, gate_in, env_out, extra_cables=()):
    """params: one list per envelope (up to 4), each to its own output."""
    if params and not isinstance(params[0], (list, tuple)): params=[params]
    mods=[(1,1),(2,3)]+[(3+k,env_type) for k in range(len(params))]
    names={1:'Keyboard',2:'4Output'}
    cables=list(extra_cables)
    for k in range(len(params)):
        names[3+k]=f"{MODS[env_type].name}{k}"
        cables+=[f"0 {3+k} {gate_in} 0 1 1 1", f"0 2 {k} 0 {3+k} {env_out} 1"]
    L=["[Header]","Version=Nord Modular patch 3.0",b.HEADER,"[/Header]","[ModuleDump]","1 "]
    for row,(i,t) in enumerate(mods): L.append(f"{i} {t} {2+(row%2)*8} {2+row*4} ")
    L+=["[/ModuleDump]","[ModuleDump]","0 ","[/ModuleDump]","[CurrentNoteDump]","64 0 0 64 0 0 ","[/CurrentNoteDump]","[CableDump]","1 "]+[c+" " for c in cables]
    L+=["[/CableDump]","[CableDump]","0 ","[/CableDump]","[ParameterDump]","1 "]
    L.append("2 3 1 100 ")
    for k,p in enumerate(params): L.append(f"{3+k} {env_type} {len(p)} "+" ".join(map(str,p))+" ")
    L+=["[/ParameterDump]","[ParameterDump]","0 ","[/ParameterDump]","[MorphMapDump]","0 0 0 0 ","[/MorphMapDump]",
        "[KeyboardAssignment]","0 0 0 0 ","[/KeyboardAssignment]","[KnobMapDump]","[/KnobMapDump]","[CtrlMapDump]","[/CtrlMapDump]","[NameDump]","1 "]
    for i,_ in mods: L.append(f"{i} {names[i]}")
    L+=["[/NameDump]","[NameDump]","0 ","[/NameDump]"]
    return "\n".join(L)+"\n"

def run(tag, env_type, params, gate_in, env_out, on=0.1, off=None, seconds=2.0, events=None):
    pch=f"{OUT}/{tag}.pch"; wav=f"{OUT}/{tag}.wav"
    open(pch,'w').write(patch(env_type,params,gate_in,env_out))
    cmd=[BIN,ROM,pch,'--seconds',str(seconds),'--note','60','--note-at',str(on),'--wav',wav]
    if off is not None: cmd+=['--note-off-at',str(off)]
    if events: cmd+=['--events',events]
    r=subprocess.run(cmd,capture_output=True,text=True,timeout=600,cwd=G1)
    if not os.path.exists(wav): print(r.stdout[-2000:],r.stderr[-2000:]); raise SystemExit
    return read(wav)

def read(wav):
    d=open(wav,'rb').read()
    i=d.index(b'fmt '); ch,rate,bits=struct.unpack('<H',d[i+10:i+12])[0],struct.unpack('<I',d[i+12:i+16])[0],struct.unpack('<H',d[i+22:i+24])[0]
    j=d.index(b'data'); n=struct.unpack('<I',d[j+4:j+8])[0]; raw=d[j+8:j+8+n]
    if bits==32: a=np.frombuffer(raw,dtype='<i4').astype(np.float64)/2**31
    elif bits==16: a=np.frombuffer(raw,dtype='<i2').astype(np.float64)/2**15
    else:
        a=np.frombuffer(raw,dtype=np.uint8).reshape(-1,3); a=(a[:,0].astype(np.int32)|(a[:,1].astype(np.int32)<<8)|(a[:,2].astype(np.int32)<<16)); a=np.where(a>=2**23,a-2**24,a).astype(np.float64)/2**23
    return a.reshape(-1,ch), rate
