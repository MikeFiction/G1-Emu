# G1-Emu v0.1.0-alpha.12

**This pre-release makes G1-Emu behave more like the real synth, and safer in a DAW.** The plugin
now keeps what each slot holds in your project, the internal clock runs at the right tempo, a JIT
failure no longer takes the DAW down, and you can optionally install the OS a real G1 runs, with
Clavia's own updater. Please try it and report what you find with the **Report issue** button.

## In your DAW

- **The plugin keeps each slot's patch in the project** (#25). Until now a project kept the banks,
  the knobs and the last Program Change: a patch sent from an editor, loaded from the panel, or
  edited since came back as `Empty patch`. The plugin now reads each slot from the G1 whenever it
  changes and puts it back when the project opens, with no editor needed. An edit made less than
  about 1.5 seconds before saving may not be in it yet. Projects saved with alpha.11 still open;
  their slots are read again from what loads.
- **A DSP JIT failure no longer crashes the DAW** (#17). If the DSP code generator runs out of
  memory (more likely with several instances), the DSP empties its cache and goes on; if it keeps
  failing, that DSP goes silent and the status bar shows it (`x` and the reason), instead of the
  host crashing.

## The emulation

- **The internal master clock runs at the synth's tempo** (#22). With the clock set to internal,
  MIDIGlobal's clock, and every sequencer, arpeggio or clock divider following it, ran **4 times
  too fast**; MIDI clock was always right. Two faults in the emulated 68331's timer, both fixed:
  MIDIGlobal now gives 24 pulses per beat at the synth's tempo. **Clocked patches now play slower
  than in earlier builds: that is the right speed.**
- **A kept flash is no longer formatted again at start.** (A fault introduced and fixed while this
  release was made; no earlier release had it.)

## Optional: the OS a real G1 runs

The ROM carries a factory OS, and G1-Emu runs it unless told otherwise: everything works with it.
But it is not the OS an updated G1 runs from its flash, Clavia's 3.03b update, and it does not read
the stored synth settings at power-on: **Shift+Store's slots do not come back** when G1-Emu starts
again, where a real G1 starts with them. You can now install the real one into G1-Emu the way you
would into the hardware, with **Clavia's free Windows updater** (`Nord Modular OS v3.03b
Update.exe`, which you find and download yourself; the Mac version is for Mac OS 8/9 and runs on no
current Mac) and, on Linux or macOS, **Wine**:

1. Start G1-Emu in update mode: `G1_UPDATE=1 ./G1-Emu` (Linux and macOS; on Windows set
   `G1_UPDATE=1` in the environment first). The display shows `Update utility`; your banks are not
   touched.
2. Run the updater and choose **G1-Emu's PC Port** as its MIDI output and input. Follow its steps
   until the G1's display says `Update completed` (if the updater complains first, let the G1
   finish).
3. Close G1-Emu. The OS is kept in the data folder and set as `os =` in the settings; the window,
   the console and the plugin all run it from then on. Empty `os =` to go back.

It is optional and done once per machine. A DAW project saved under one OS keeps, but does not
apply, its G1 state under the other. The OS is Clavia's, like the ROM: keep it to yourself.

## The plugin (VST3 and CLAP)

**Installing it:** copy `G1-Emu.vst3` and/or `G1-Emu.clap` from the archive to the plugin folder.

| System | VST3 | CLAP |
| --- | --- | --- |
| macOS | `~/Library/Audio/Plug-Ins/VST3` | `~/Library/Audio/Plug-Ins/CLAP` |
| Windows | `C:\Program Files\Common Files\VST3` | `C:\Program Files\Common Files\CLAP` |
| Linux | `~/.vst3` | `~/.clap` |

On macOS the plugin is not signed: if the DAW refuses it, run
`xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/G1-Emu.vst3` (and the same for the
`.clap`), then rescan. Each instance opens its own PC Port for Animatek NME on Linux and macOS
(**G1-Emu PC Port**, **G1-Emu 2 PC Port**...).

## Known issues

- **macOS:** patch uploads from Animatek NME can time out (#3).
- **Windows:** the plugin has no PC Port; a direct link between NME and the emulator, with no MIDI
  port, is planned (#8).
- **CLAP:** if you remove every G1-Emu instance and then add a new one, that new one may have no PC
  Port until the DAW reloads the plugin; its Settings say so.
- **After the OS update:** the window crashed twice in the DSP code generator right after one
  update, and has not been seen again. If it happens to you, please report it.

## Windows quick start

Read `WINDOWS.md` inside the ZIP for the complete step-by-step version. In short:

1. Install and run [loopMIDI](https://www.tobias-erichsen.de/software/loopmidi.html).
2. Create `G1→NME` and `NME→G1`.
3. In G1-Emu Settings choose **PC Port out = G1→NME** and
   **PC Port in = NME→G1**, then restart G1-Emu.
4. In Animatek NME choose **MIDI INPUT = G1→NME** and
   **MIDI OUTPUT = NME→G1**, then press **Connect**.

The two cables must remain separate. One shared loopback port can return a program's own output to
its input and make the traffic counters misleading.

## It brings no ROM, and it never will

G1-Emu emulates the hardware, not Clavia's software. You need your own 512 KB ROM dump from a Nord
Modular **rack** running OS 3.03. The program asks for it on first start. Do not distribute the ROM
with G1-Emu or upload it to the project.

The emulated synth also starts with empty patch slots. Use a compatible editor, such as
[Animatek NME](https://github.com/animatek/Animatek-NME), to create or upload a `.pch` patch over
the PC Port.

## Nothing is signed

- **macOS:** Gatekeeper may say the developer is unidentified. Right-click the app and choose
  **Open**, or run `xattr -dr com.apple.quarantine G1-Emu.app`.
- **Windows:** SmartScreen may warn. Choose **More info**, verify that the file came from this
  GitHub release, and choose **Run anyway**.

Signing and notarisation are not done yet.

## Platforms

| System | Audio | Editor MIDI | Status |
| --- | --- | --- | --- |
| Linux | JACK/PipeWire or ALSA | ALSA sequencer ports | used daily |
| macOS 11+ | CoreAudio | CoreMIDI virtual ports | verified on real hardware |
| Windows 11 | WASAPI/DirectSound/ASIO | two loopMIDI cables | verified on real hardware |

The macOS package is universal (Apple Silicon and Intel). Windows' loopMIDI step is temporary:
[Microsoft/MIDI issue #1047](https://github.com/microsoft/MIDI/issues/1047) is fixed upstream but
awaiting the November 2026 Windows release.

## Files

`G1-Emu` / `G1-Emu.exe` is the panel application. `g1run` / `g1run.exe` is the console front end.
`G1-Emu.vst3` and `G1-Emu.clap` are the plugin.
Every archive contains the README and GPLv3 licence; the Windows ZIP also contains `WINDOWS.md`.
The source of this build is the tag attached to the release.
