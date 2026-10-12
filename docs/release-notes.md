# G1-Emu v0.1.0-alpha.16

**A log file now, shared by the standalone and the plugin** (#59, #63, #65): every instance
writes timestamped lines to `g1-emu.log`, beside the settings file — startup (version, ROM, PC
Port, direct link), a restart, a DSP's JIT failure, and a dropout (the first one, then again
every 100, with the load at the time). If you've reported "no audio", "no signal" or "connection
lost" without a clear way to reproduce it, this is for you: **please try again, and if it still
happens, press Report issue** (bottom right of the panel) — it now also opens the log's folder,
so it's right there to attach to the issue it opens for you.

## Also in this build

- **The plugin can raise its output gain** (#65), like the standalone's "Output level" slider —
  until now the plugin was stuck at a fixed +36 dB with no way to make up for a quiet patch
  beyond the DAW's own fader. A new **Output Gain** control is on the plugin's Settings card,
  host-automatable, saved with the project.
- **An overload turns the status bar red** (#64), with "OVERLOAD" ahead of the line, for about a
  second on a new dropout — so an isolated one is still seen, not just a sustained one. Amber at
  90% load with none yet. The DAW's own meter watches its audio callback, which the emulator does
  not run in, so it can read near-idle while the G1 is actually dropping audio; this is the first
  place that shows the real picture.

## Known

Same as alpha.15: on Windows the plugin has no PC Port of its own (it uses the direct link); the
standalone still needs loopMIDI for the editor when the direct link is not used.
