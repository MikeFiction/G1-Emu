# G1-Emu v0.1.0-alpha.21

A faster emulator, and cleaner sound in busy patches. Nothing new on the panel this time: this
release is about the engine under it.

**About 50 % faster.** The emulated DSPs now run on the current DSP56300 core of the dsp56300
project (the one Gearmulator uses), and the work around them was measured and cut down: the
DSPs' serial ports no longer emulate, word by word, data the G1 never looks at. On our test
machine, `BellsMorph.pch` from the factory presets went from 3.4× to 5.1× real time. In practice:
lower CPU use for the same patches, more room for several instances, and slower computers that
could not keep up may now do so.

**No more crackles in busy patches** (BellsMorph and others that keep all four DSPs loaded).
Three causes, all fixed: one DSP lost sample clocks when its interrupts piled up, the clock of
the DSPs' serial ports fell behind under load, and a busy DSP could read the audio coming from
the previous DSP while it was half rewritten with the next sample. Patches that already sounded
clean sound exactly the same: six of our ten reference recordings are byte-identical, and the
other four have the same level and pitch with less noise above 20 kHz.

Your ROM, banks, settings and projects are not affected by any of this.

## Known

Same as alpha.20: on Windows the plugin has no PC Port of its own (it uses the direct link); the
standalone still needs loopMIDI for the editor when the direct link is not used.
