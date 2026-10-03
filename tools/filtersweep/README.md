# filtersweep

Work in progress: measuring FilterE in the emulator to model it for a VCV Rack module, the way
`../envsweep` did for the envelopes. Needs a built `g1patchtest`, `../Nomad2026` and numpy; data
goes under `build/filtersweep`.

- `fmeasure.py` builds a patch AudioIn -> up to four modules -> outs 1-4 and runs a signal through
  it with `g1patchtest --input-raw`.
- `fresp.py` measures transfer functions: noise in, out 1 wired straight, outs 2-4 through the
  filters, H = Y / D, which cancels the input path, its latency and its gain.
- `ressweep.py` and `resfit.py` sweep and fit the resonance.

The input path of the emulator currently runs at 48 kHz (only even samples get in; the real G1 runs
at 96 kHz), so the test signals repeat every value twice: what reaches the filter is then exactly
what was sent.

Findings so far: a two-pole digital state-variable filter with one sample of delay (the phase fits
to 1.5 degrees); 24 dB is two identical sections in cascade, not a ladder; cutoff
330 Hz * 2^((v - 60) / 12); Q from 0.47 to about 200 at full resonance, so it never self-oscillates;
gain control is an input attenuation that follows the resonance; at high levels with resonance it
compresses and distorts, probably clipping in fixed point (not simulated yet).
