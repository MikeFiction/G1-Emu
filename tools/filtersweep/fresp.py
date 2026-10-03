"""Transfer functions through the emulator: noise in, one output wired straight, H = Y/D."""
import numpy as np, fmeasure as m
R=96000
def noise(seconds=2.0, amp=0.1, seed=1):
    rng=np.random.default_rng(seed); n=int(seconds*R)//2
    v=rng.uniform(-1,1,n)*amp*np.sqrt(3)  # rms = amp
    return np.repeat(v,2)
def fe(ftype=0, gainctl=1, freq=60, res=0, slope=1, kbt=0):
    # FilterE: type, gain control, fm1, frequency, kbt, res mod, resonance, slope, fm2, bypass
    return (51,[ftype,gainctl,0,freq,kbt,0,res,slope,0,0],2,0)
def measure(filters, amp=0.1, seconds=2.0, tag='fr', nseg=8192):
    """Out 1 is the direct wire, outs 2-4 the filters. Returns freqs and H per filter."""
    x=noise(seconds,amp)
    y=m.run(tag,[None]+list(filters),x)
    d=y[:,0]; res=[]
    win=np.hanning(nseg); hop=nseg//2
    starts=range(4000, len(d)-nseg, hop)
    D=np.array([np.fft.rfft(d[s:s+nseg]*win) for s in starts])
    for k in range(len(filters)):
        Y=np.array([np.fft.rfft(y[s:s+nseg,k+1]*win) for s in starts])
        H=(Y*np.conj(D)).mean(0)/(np.abs(D)**2).mean(0)
        res.append(H)
    return np.fft.rfftfreq(nseg,1/R), res
def db(H): return 20*np.log10(np.abs(H)+1e-12)
