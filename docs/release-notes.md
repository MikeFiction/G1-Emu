# G1-Emu v0.1.0-alpha.13

**This pre-release gives G1-Emu a new face, and makes the plugin play along with your DAW.** The
panel is now Mike Fiction's artwork: a real-looking G1 rack, resizable, with knob displays, the
synth's settings on a page of their own and a display that says what each control does. The plugin
follows your DAW's tempo, has a mono output for each of the G1's four, and keeps its patches when
you switch OS. Please try it and report what you find with the **Report issue** button.

## A new panel, by Mike Fiction

**Panel skin and GUI design by Mike Fiction**, drawn from photographs of real hardware and
published under CC BY 4.0. Thank you, Mike.

- **Resizable**, in the window and in the plugin, and it remembers its size.
- **Parameter displays** above the knobs say what each one moves, with the value as the editor
  shows it (extras drawer). **Master volume** goes 0 to 127, and the DAW can automate it.
- **An info display** under the panel says what the control under the mouse will do now: the dial,
  the navigator, Store, Assign/Morph. **Tooltips** wait the same each time and can be turned off.
- **Presets and Settings**, two more keys right of slot D. **Settings** shows the synth's settings
  (the slots' MIDI channels, the clock, velocity, master tune...) over the knobs; press it again to
  go back. **Presets** will list the synth's banks and programs to load with a click: it is **in
  development**, coming next.
- **Shift works as on the hardware**: it lets go after the next key, and stays held for the
  navigator in Edit mode. **Shift + Patch/Load is Random**; right-click a knob to keep it out of
  Random (a padlock shows it).
- **Restart** switches the G1 off and on without closing G1-Emu (extras drawer).
- **About** (extras drawer): who made G1-Emu, the license of every part in it, and Animatek NME.
- The standalone has an **icon**, and the dial turns with a thumb indent.

## In your DAW

- **The plugin follows your DAW's tempo** (#20). With the G1's MIDI clock set to external (the
  Settings page: MIDI clock source), MIDIGlobal and everything clocked by it play at the host's
  tempo and start and stop with its transport, loops included.
- **Mono outputs** (#27): besides Out 1/2 and Out 3/4, Out 1, Out 2, Out 3 and Out 4 one by one.
  They start off; in Cubase and Nuendo turn them on with **Activate Outputs**.
- **A project saved under one OS opens under another** (#25). After installing Clavia's 3.03b
  update, projects saved with the factory OS came back empty. Now their banks and settings load,
  with the OS in use.
- **The direct link with Animatek NME** (#8): NME finds every G1-Emu, window or plugin instance,
  and connects with no MIDI port to set up, also on Windows. It needs the next Animatek NME release.

## Optional: the OS a real G1 runs

The ROM carries a factory OS, and G1-Emu runs it unless told otherwise: everything works with it.
An updated G1 runs Clavia's 3.03b instead, which also brings back **Shift+Store's synth settings**
at power-on. You can install it into G1-Emu the way you would into the hardware, with **Clavia's
free Windows updater** (`Nord Modular OS v3.03b Update.exe`, which you find and download yourself)
and, on Linux or macOS, **Wine**:

1. Start G1-Emu in update mode: `G1_UPDATE=1 ./G1-Emu` (Linux and macOS; on Windows set
   `G1_UPDATE=1` in the environment first). The display shows `Update utility`; your banks are not
   touched.
2. Run the updater and choose **G1-Emu's PC Port** as its MIDI output and input. Follow its steps
   until the G1's display says `Update completed`.
3. Close G1-Emu. The OS is kept in the data folder and set as `os =` in the settings; the window,
   the console and the plugin all run it from then on. Empty `os =` to go back.

The OS is Clavia's, like the ROM: keep it to yourself.

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
- **Windows:** the plugin has no PC Port of its own (Windows makes no virtual MIDI ports). The
  direct link reaches it with no port at all, from the next Animatek NME release on; until then,
  use the standalone with loopMIDI (below).
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
