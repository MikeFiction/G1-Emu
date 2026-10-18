# G1-Emu v0.1.0-alpha.22

The expansion board, the G1 as an effect, and projects that come back as you left them.

**Regular or Expanded (#39).** The G1 can now be the Expanded: the G1 with its expansion board,
eight DSPs instead of four and about twice the voices per patch (BellsMorph goes from 8 voices to
16). Pick it in Settings, "ROM and model", in the standalone (it applies on the next start or
Restart) or on the plugin's Settings card, where it is saved with the project. A project from
before runs a Regular, as it did. The Expanded needs more CPU (about 2.75 cores against 1.6 on our
test machine), so pick it only for the patches that need the voices.

**G1-Emu FX (#76).** A second plugin, installed next to the instrument: the same G1, but as an
effect, with its input on from the start. Put it on an audio track and the track's sound goes into
the G1's two audio inputs, ready for a patch's AudioIn module: filters, delays, vocoders, anything
the G1 can do to a sound. Notes and the editor work as in the instrument.

**Projects bring back their slots as they were (#75).**

- Which slots are on and which one is active now come back with the project. Before, every slot
  holding a patch came back on, which split the voices among them.
- A heavy patch in slot A no longer comes back broken, showing `Error`. When Animatek NME connected
  while the project was opening, its first messages arrived in the middle of the patch going back
  into the slot, and the G1 dropped it half done. The editor now waits until the slots are back.

Your ROM, banks, settings and projects are not affected: a project saved with an earlier version
opens as before.

## Known

- On Windows the plugin has no PC Port of its own (it uses the direct link); the standalone still
  needs loopMIDI for the editor when the direct link is not used.
- Very heavy patches on the Expanded can still drop out on some machines. We are working on it.
