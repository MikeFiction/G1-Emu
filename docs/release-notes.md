# G1-Emu v0.1.0-alpha.11

**This pre-release brings G1-Emu into your DAW: a VST3 and CLAP instrument plugin**, on the same
emulation as the standalone. The plugin is new and has only been played on Linux so far: on macOS
and Windows it builds, but has not yet been tried in a DAW. Please try it and report what you find
with the **Report issue** button in the window.

## The plugin (VST3 and CLAP)

- **The G1 on a DAW track:** notes, controllers and Program Changes from the track; outputs 1/2
  and 3/4 as two stereo buses, and In L/R as an optional input. Latency is reported to the DAW,
  which compensates for it.
- **Edit it from Animatek NME while it plays in the DAW (Linux and macOS).** Each instance opens
  its own PC Port: **G1-Emu PC Port**, **G1-Emu 2 PC Port** for the second, and so on. Choose it
  in NME as input and output. On Windows the plugin cannot open a MIDI port yet: edit with the
  standalone, as in alpha.10.
- **The 18 knobs are automatable parameters**, each named after what it moves in the current
  patch (`Knob 3: OscA Freq coars`). Turning one on the panel moves the parameter too.
- **Its patches live in the DAW project.** A new instance starts from a copy of the standalone's
  banks; from then on what you store goes into the project, not into the standalone's files.
- **Program Changes:** from the track, or through the plugin's 128 programs, which is how a VST3
  host sends them (to channel 1).

**Installing it:** copy `G1-Emu.vst3` and/or `G1-Emu.clap` from the archive to the plugin folder.

| System | VST3 | CLAP |
| --- | --- | --- |
| macOS | `~/Library/Audio/Plug-Ins/VST3` | `~/Library/Audio/Plug-Ins/CLAP` |
| Windows | `C:\Program Files\Common Files\VST3` | `C:\Program Files\Common Files\CLAP` |
| Linux | `~/.vst3` | `~/.clap` |

On macOS the plugin is not signed: if the DAW refuses it, run
`xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/G1-Emu.vst3` (and the same for the
`.clap`), then rescan.

## Also new since alpha.10

- **The emulator no longer slows down with every patch load** (#6). After a number of patch
  changes the load could climb to 100 % and stay there, with dropouts. Fixed in the emulation, so
  in every build.
- **Shift and several slots at once from the keyboard.** Shift on the computer's keyboard holds
  the panel's Shift (lit while held); A, B, C and D hold the slot buttons, several together. A
  right click latches any panel button down until the next right click.
- **An extras drawer below the panel** (the chevron next to the icons): **Random** turns the 18
  knobs to random positions, a double click puts the patch's values back; **Parameter displays**
  shows above each knob the module and parameter it moves.
- **Patreon, Report issue and Settings** as icons; **Report issue** opens a GitHub issue with
  your build and setup filled in. Oct Shift is gone from the panel: the rack never used it.

## Known issues

- **macOS:** patch uploads from Animatek NME can time out (#3).
- **Windows:** the plugin has no PC Port; a direct link between NME and the emulator, with no
  MIDI port, is planned (#8).
- **CLAP:** if you remove every G1-Emu instance and then add a new one, that new one may have no
  PC Port until the DAW reloads the plugin; its Settings say so.

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
