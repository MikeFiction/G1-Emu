# envsweep

Measures the G1's envelopes in the emulator and turns the measurements into the tables of
ADSRtek, the envelope module in the Animatek VCV Rack plugin (`UZZ-VCV-RACK`). The module is
modelled on these recordings, not on the DSP code.

Needs a built `g1patchtest` (`build/tools/patchtest`), `../Nomad2026` (module descriptions and
the editor's time table) and numpy. Everything is written under `build/envsweep`.

```bash
python3 sweep.py 4            # 160 runs, 4 at a time (~30 min): every step of attack (3 shapes),
                              # decay and release, four envelopes per run
python3 fit_shapes.py         # one parameter per step for the Log and Exp attacks
python3 gen_header.py ../../../VCV-Rack/UZZ-VCV-RACK/src/AdsrTekCurves.hpp
g++ -std=c++11 -O2 -I <UZZ-VCV-RACK>/src -I <Rack-SDK>/include -I <Rack-SDK>/dep/include \
    vcvrun.cpp -o build/envsweep/vcvrun -L<Rack-SDK> -lRack -Wl,--unresolved-symbols=ignore-all
python3 compare.py 1          # emulator against ADSRtek, same settings and gates
```

What was found, briefly:

- Times follow the editor's table to within half a percent in the median; at the top they
  run long and step (Lin 125 and 126 both take 49.9 s).
- Decay and release are the same exponential; the editor's time is the time to 1%.
- Log and Exp attacks are level-driven: an approach to a target above full scale, and a
  growth from zero. That is why a retrigger carries on from the current level.
- Everything moves at 24 kHz, one value per four samples at 96 kHz.
- The WAV resolves steps of about 0.00015 of full scale (the OS caps the master at -36 dB and
  the bench adds the 36 dB back), so nothing below about -76 dB can be seen.
