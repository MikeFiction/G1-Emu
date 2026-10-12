# G1-Emu v0.1.0-alpha.15

**The synth settings (each slot's MIDI channel, the clock, and the rest of Synth Settings) now
travel with the project** (#46): saved as the OS has them, stored with Shift+Store or not, and
sent back to the G1 as soon as it starts.

## Presets and the panel

- A patch stored from Animatek NME now shows on the Presets page without leaving and coming back.
- A patch loaded only (not stored) also shows in NME at once.
- A knob assigned to a morph group follows the group's value on the panel.
- Parameter displays fade on and off; a control's name and value linger half a second before
  fading out.
- Settings opens as a card over the panel, like About.

## If you reported no notes from the DAW on Windows (#53)

Tested on Windows 11 with Ableton Live 12 (VST3): notes from a clip, live from the keyboard, and
after saving and reopening a project all reach the plugin correctly with this build. Please try
this version and tell us if you still see it.

## README

A new Getting Started and FAQ, for people who only download the release: which archive to get,
where the plugin goes, the macOS warning, the four ways to load a sound, and MIDI channels.

## Known

Same as alpha.14: on Windows the plugin has no PC Port of its own (it uses the direct link); the
standalone still needs loopMIDI for the editor when the direct link is not used.
