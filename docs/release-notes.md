# G1-Emu v0.1.0-alpha.19

Two fixes for "no sound" and "it drops out" (#63, #65).

**Old patches sound again.** Most of Clavia's factory patches, and many in the community archives,
are in the old **2.10** format, and **Presets → Load .pch** converted them wrong: the slave
oscillators arrived muted, sequencers empty, waveforms and outputs one step off, morphs and knob
assignments gone. The conversion now follows the rules of Clavia's own editor 3.03, morph module,
morph and knob assignments and MIDI controllers included. Of 40 2.10 factory patches, 16 were
silent; now every one sounds, except the five that are silent by design (two wait for the MIDI
clock, one for a knob its notes name, two process the audio inputs). The same conversion goes into
the next Animatek NME. Patches in the 3.0 format, Test Audio and Test MIDI among them, load exactly
as before.

**Windows: no more dropouts from the emulator's own sleeps.** When the plugin was ahead of the host
it slept 0.2 ms, but on Windows such a sleep lasts 15.6 ms unless something in the process asked
for more: the plugin fell behind and dropped out, most of all on a power-saving plan. It now
sleeps with a high-resolution timer, and its threads opt out of Windows 11's power throttling
(EcoQoS). In our test, dropouts in real time went from 353 to 9, and offline rendering got six
times faster.

**If a patch is silent or strange**, see the new FAQ entry,
[Some patches do not sound, or sound strange](https://github.com/animatek/G1-Emu#some-patches-do-not-sound-or-sound-strange):
look at it in an editor first (outputs, inputs, mute, clock, knobs), try it from Animatek NME, and
**attach the very `.pch` to the issue**. **Report issue** now asks for it.

If you were asked to try `G1_THREADS=0`: there is no need any more. That mode cannot keep up in
real time and drops out.

## Known

Same as alpha.18: on Windows the plugin has no PC Port of its own (it uses the direct link); the
standalone still needs loopMIDI for the editor when the direct link is not used.
