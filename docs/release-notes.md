# G1-Emu v0.1.0-alpha.17

**Two test patches, and a step by step to know where a "no sound" comes from** (#53, #59, #65).
[`test-patches/`](https://github.com/animatek/G1-Emu/tree/main/test-patches) has **Test Audio**,
which sounds with no notes at all, and **Test MIDI**, which sounds only while a note is held. Load
each into slot A and the one that stays silent says whether the problem is the audio or the notes:
the README walks through it in
[Getting started, step 6](https://github.com/animatek/G1-Emu#getting-started).

**If you still have no sound** (#53, #59, #65): please go through step 6 with this build. If
**Test Audio** stays silent, press **Report issue** (bottom right of the panel) and attach
`g1-emu.log` from the folder it opens, with the status bar's line. On Windows, one more thing
worth trying and telling us about: set the environment variable `G1_THREADS=0` (System
Properties, Environment Variables), restart the DAW or the standalone and load Test Audio again.
It runs the four DSPs one after another instead of in parallel, which costs more CPU; if it sounds
that way and not the other, we know where to look.

## Also in this build

- **The plugin's Output Gain starts at +48 dB** (#65), up from +36 dB, which only undid the OS's
  own -36 dB cap on the master volume and left most patches too quiet next to other instruments.
  A loud patch may now touch full scale: lower the slider on the Settings card. Projects keep the
  gain they were saved with, and those from before the slider existed open at +36 dB as before.

## Known

Same as alpha.16: on Windows the plugin has no PC Port of its own (it uses the direct link); the
standalone still needs loopMIDI for the editor when the direct link is not used.
