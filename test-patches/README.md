# Test patches

Two small patches to tell where a "no sound" problem is. Load them into slot A (the Presets page's
**Load .pch**, or your editor) and watch the status bar's `output 1/2` and `MIDI in` as you go.

| Patch | What it does | If it is silent |
| --- | --- | --- |
| `Test Audio.pch` | An oscillator straight to outputs 1/2 through a mixer. **It sounds with no notes at all.** | The audio chain is the problem, not MIDI: say so in your issue, with the status bar's line and `g1-emu.log`. |
| `Test MIDI.pch` | Keyboard, AD envelope and filter: **it sounds only while a note is held**, on slot A's MIDI channel (1 by default). | If `Test Audio` sounds but this does not, the notes are not reaching the slot: check `MIDI in` counts up and the slot's MIDI channel. |

The step by step is in the main README, [Getting started, step 6](../README.md#getting-started). Try `Test Audio` first, with your speakers down: the tone is loud. Both are made in Animatek NME and are deliberately tiny, so they load the
same everywhere and nothing in them depends on knobs, morphs or the synth settings.
