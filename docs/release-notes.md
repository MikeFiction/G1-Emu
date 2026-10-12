# G1-Emu v0.1.0-alpha.18

**The log now says where a "no sound" stops** (#65). `g1-emu.log` (Report issue opens its
folder) gets two new kinds of line:

- At start, **the machine**: the CPU, its cores (performance and efficiency ones on a hybrid
  CPU), the instruction sets the DSP emulation picks its code by, and whether the four DSPs run
  on their own threads or one after another (`G1_THREADS=0`).
- A moment after you play a note, **how far the sound gets**: the output's level, each DSP's
  state, the level on each link between the four DSPs, and how often a DSP had to go on without
  the previous one's audio, which is what we suspect goes wrong on some machines.

Every line also carries the process id, since a DAW may run each plugin in a process of its own.

**If you have no sound** (#53, #59, #65): please install this build, keep only one copy of the
plugin (look in both `C:\Program Files\Common Files\VST3` and
`C:\Users\<you>\AppData\Local\Programs\Common\VST3`), load **Test Audio** from
[`test-patches/`](https://github.com/animatek/G1-Emu/tree/main/test-patches)
([Getting started, step 6](https://github.com/animatek/G1-Emu#getting-started)), **play a few
notes**, wait a couple of seconds, then press **Report issue** and attach `g1-emu.log`. If you
can, do it once more with the environment variable `G1_THREADS=0` set, and attach both logs.

## Known

Same as alpha.17: on Windows the plugin has no PC Port of its own (it uses the direct link); the
standalone still needs loopMIDI for the editor when the direct link is not used.
