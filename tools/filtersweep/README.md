# filtersweep

Measures FilterE in the emulator and turns the measurements into the tables of FILTERtek, the
filter module in the Animatek VCV Rack plugin (`UZZ-VCV-RACK`), the way `../envsweep` did for the
envelopes. The module is modelled on these recordings, not on the DSP code. Needs a built
`g1patchtest`, `../Nomad2026` and numpy; data goes under `build/filtersweep`.

The emulator's audio inputs currently run at 48 kHz (only even samples get in; the real G1 runs at
96 kHz), so test signals repeat every value twice: what reaches the filter is exactly what was sent.

| Script | What it does |
| --- | --- |
| `fmeasure.py` | Patch AudioIn -> modules -> outs 1-4, a signal through it with `--input-raw`. |
| `fresp.py` | Transfer functions: noise in, out 1 wired straight, H = Y / D. |
| `svf.py`, `fit.py` | The Chamberlin filter (simulation and closed forms) and grid fitting. |
| `cutsweep.py`, `cutfit.py` | Every cutoff step at a fixed resonance, fitted with the Chamberlin LP. |
| `qsweep.py`, `qfit.py`, `qhigh.py`, `ring.py`, `qlow.py` | Every resonance step: noise fits, ring-down of an impulse for high resonance, precise low-resonance fits. |
| `q24.py`, `gainsweep.py`, `brsweep.py` | The 24 dB damping, the gain control table, the band reject damping. |
| `fmsweep.py` | Where a Constant on FM or resonance mod puts the cutoff or the Q. |
| `clipwave.py`, `satsim.py` | Waveforms driven into clipping, and the simulation that places the saturation. |
| `gen_filter_header.py` | Writes `FilterTekTables.hpp`. |
| `frun.cpp`, `fcompare.py` | The module run offline against the emulator on the same signals. |

What the measurements say:

- A **Chamberlin state-variable filter** at 96 kHz (lp += f bp; hp = x - lp - q bp; bp += f hp),
  matching to 0.02 dB and a degree of phase across the cutoff range, with one sample more than the
  closed form. f = 2 sin(pi fc / 96000), fc = 330 Hz * 2^((step - 60) / 12) * 0.99876.
- The damping follows the cutoff: **q = q0(resonance) * (1 - f/2)**. Q from 0.5 to about 5000.
- **24 dB is two identical sections**, each with its own q0 table; not a ladder.
- **Band reject** is HP + LP of a section with a damping of its own, much gentler than the
  resonance (Q 0.5 to about 2).
- **Gain control** is an input gain from a table (0 to -40 dB), the same for both slopes, at the
  input for 12 dB and **between the sections for 24 dB**; band reject gets (1 + g) / 2.
- **Saturation**: lp, bp and hp are each limited to +-1 when stored, as fixed point does. That
  reproduces driven waveforms to 0.02 dB in the harmonics.
- **Modulation**: FM is exponential, units * amount / 63.5 semitones; resonance mod adds
  units * amount / 63.5 steps; both clamp to the 128-step range.
