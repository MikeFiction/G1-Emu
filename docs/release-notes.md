# G1-Emu v0.1.0-alpha.20

MIDI controllers on the panel, octave shift per slot, a Cubase crash fixed, and a Windows installer.
Most of this release is by **Mike Fiction**: thank you, Mike.

**MIDI CC and MIDI Learn from the knobs** (#68, #14). Right-click a knob: **Assign MIDI CC** gives
the parameter it moves a CC, from a list or with **MIDI Learn** (the knob's LED or display blinks
until a controller moves). The CC belongs to the parameter in the patch, as when an editor assigns
it, so it keeps working whatever slot the patch is in, and whatever the knob is later assigned to.
A CC another parameter already has is greyed out, says which, and asks before taking it.

**Keyboard Mode and MIDI CC Mode** (#67), on the Synth Settings page. With **Keyboard Mode** on
Selected Slots, a note on any selected slot's channel plays every selected slot, each on its own
channel, with sustain, pitch bend, the mod wheel and aftertouch: layers, as on the keyboard G1.
**MIDI CC Mode** on Selected Slots does the same for the controllers (Bank Select aside).

**Octave shift per slot** (#69). The **OCT SHIFT** keys and their five LEDs, on the left of the
panel, move the active slot's notes up to two octaves up or down, as on the keyboard G1; the shift
is saved in the patch. In the plugin, **Slot A-D Octave** are four new parameters the DAW can
automate. Note: a patch saved with a shift now plays shifted, which a rack G1 does not do.

**Smoother knobs** (#70): a knob moved by a MIDI controller or by the DAW's automation now follows
it smoothly, without lagging or stepping. And smaller text on the Emulator Settings card and in the
menus' titles (#71).

**Cubase and Nuendo no longer crash when the buffer size changes** (#66). A new buffer size changes
the plugin's latency, and Steinberg's hosts answer that at once by restarting the plugin, while it
was still busy telling them: on Windows that ended in a crash. Live and Bitwig were not affected.

**Windows installer.** Next to the zip there is now `G1-Emu-windows-x86_64-setup.exe`: it installs
the standalone, the VST3 and the CLAP (each optional), replaces the previous version in place, and
removes copies of the plugins left by hand in `AppData\Local\Programs\Common`, since a DAW that
finds two may load the old one. Your ROM, banks and settings are never touched, not even by the
uninstaller. It is not signed yet, so Windows SmartScreen will warn: **More info → Run anyway**.

## Known

Same as alpha.19: on Windows the plugin has no PC Port of its own (it uses the direct link); the
standalone still needs loopMIDI for the editor when the direct link is not used.
