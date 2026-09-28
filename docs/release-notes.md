# G1-Emu v0.1.0-alpha.10

**This pre-release makes the emulator 50–70 % faster and halves its CPU use in real time**, with
exactly the same sound. It is built and DSP-tested in CI on Linux x86-64 (native and JUCE backends),
Linux arm64, macOS universal and Windows x86-64. The changes are in the emulation shared by every
platform; they were measured on Linux.

## What alpha.10 changes

- **Faster.** Measured over nine patches, from light to very heavy, the emulator now runs 2.5–3.3
  times faster than real time where it ran 1.7–2.2 times: more headroom before a heavy patch
  stutters, and room for slower computers. How:
  - the emulated CPU runs on while the four DSPs catch up, instead of waiting for them;
  - when the CPU talks to one DSP, only that DSP has to be brought up to date, not all four;
  - a DSP with little to do no longer emulates its idle waiting instruction by instruction;
  - builds use link-time optimization.
- **Lighter.** In real time the emulator used over three CPU cores whatever the patch, because its
  DSP threads kept spinning while waiting. They now sleep when there is nothing to do: about 1.7
  cores on the same machine.
- **Same sound.** Every change was checked against recordings of nine patches, which come out
  byte-for-byte identical, and against a battery of all 109 module types.

## Also in this build (from alpha.5 to alpha.9)

- Heavy patches no longer turn into noise or grit, on the first load or after others (alpha.8,
  alpha.9).
- The emulator no longer stops answering after many uploads in a row (alpha.8).
- The synth's internal master clock runs (sequencers and clocked patches). Set the clock to
  **internal** in the synth settings; in Animatek NME's Synth Settings dialog that option is shown
  the wrong way round for now: choose **External** there.
- Upload timeouts fixed; the sawtooth of `OscA`, `OscB` and `OscSlvC` sounds on every platform.

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
