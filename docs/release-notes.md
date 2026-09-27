# G1-Emu v0.1.0-alpha.7

**This pre-release makes the synth's internal master clock run.** Patches driven by it (anything
that takes MIDIGlobal's clock: sequencers, arpeggios, patches synced to the synth's tempo) were
silent. It is built and DSP-tested in CI on Linux x86-64 (native and JUCE backends), Linux arm64,
macOS universal and Windows x86-64. The fix was found by comparing recordings of a real Nord
Modular with the emulator, and checked on Linux; it is in the emulation shared by every platform.

## What alpha.7 fixes

- **The internal master clock.** The emulated G1 never started it, so MIDIGlobal's clock and sync
  outputs stood still and every patch clocked by them stayed silent. A sequencer patch of 110
  modules that is loud on a real G1 now sounds the same in the emulator.

## One setting to check

The master clock also has to be set to **internal** in the synth settings, and a new G1-Emu flash
comes up set to external (waiting for MIDI clock). If a clocked patch is still silent, set the
clock to internal from your editor's synth settings, or send MIDI clock to the **MIDI** port.
With Animatek NME up to now, the Synth Settings dialog shows this option the wrong way round:
choose **External** there to get the internal clock, until NME is fixed.

## Also in this build (from alpha.5 and alpha.6)

- Upload timeouts fixed: a deadlock after a DSP had been idle, commands to a busy DSP thrown away
  or never taken.
- The sawtooth of `OscA`, `OscB` and `OscSlvC` sounds on every platform.
- With `G1_MIDI_LOG=1` the JUCE MIDI backend (macOS and Windows) reports a truncated SysEx.

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
Every archive contains the README and GPLv3 licence; the Windows ZIP also contains `WINDOWS.md`.
The source of this build is the tag attached to the release.
