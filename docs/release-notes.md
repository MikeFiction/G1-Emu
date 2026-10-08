# G1-Emu v0.1.0-alpha.14

**This pre-release brings the Presets page: the synth's banks at a glance, a click to load a
patch, and .pch files sent to the synth with no editor.** Plus more of Mike Fiction's panel, and a
fix for a crash in Nuendo. Please try it and report what you find with the **Report issue** button.

## Presets

Press **Presets** (right of slot D) and the page shows over the knobs:

- **The banks, from the synth itself.** Pick a bank (1 to 9) at the top right; its patches show in
  three columns. **Hide empty** (on by default) leaves out the positions with nothing stored, and
  the count beside it says how many are used.
- **A click loads a patch** into the slot lit on the panel, marked in orange. Animatek NME, if it
  is connected, follows it at once, as when you load with the dial.
- **Load .pch...** sends a patch file straight to the synth, with no editor. You pick the bank and
  the position, each named after what it holds (the first empty one is offered); a position that
  holds a patch is named in a warning and the button says **Replace**. **Load only** sends it to
  the slot without storing it.

Press Presets again, or Escape, to go back to the panel.

## On the panel (Mike Fiction)

- **Presets and Settings** lettered in the panel's own font.
- **Switching between the two pages** no longer shows the panel in between.
- **Oscillator pitch reads as a note** on the knob displays (C4), as on the G1; a click on the
  display shows it in Hz.
- **Right-click menus** in the panel's look, and a menu for the panel itself (right-click anywhere
  else): **GUI Scale** from 75 % to 250 %, Settings and About.

## In your DAW

- **Knobs turned on the panel reach the DAW as one move** (#42): one touch when the knob starts to
  move, its values, a release once it stops, where the plugin used to send a touch thirty times a
  second. Learning a knob with Cubase's and Nuendo's modulators crashed Nuendo; automation in Touch
  mode also records a turn as one.
- **Animatek NME 0.21** connects to every G1-Emu, window or plugin instance, with no MIDI port to
  set up (the direct link), also on Windows.

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
- **Windows:** the plugin has no PC Port of its own (Windows makes no virtual MIDI ports): connect
  with the direct link, which needs **Animatek NME 0.21** or later. The standalone also works with
  loopMIDI (below).
- **Nuendo and Cubase modulators** (#42): learning a G1-Emu knob crashed Nuendo. Two things of ours
  that could do it are fixed in this release, but it has not been tried in Nuendo yet: please tell
  us how it goes.
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
