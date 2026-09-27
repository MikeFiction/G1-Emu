# G1-Emu v0.1.0-alpha.8

**This pre-release fixes heavy patches that came out as noise.** It is built and DSP-tested in CI on
Linux x86-64 (native and JUCE backends), Linux arm64, macOS universal and Windows x86-64. Both fixes
are in the emulation shared by every platform, and were checked on Linux.

## What alpha.8 fixes

- **Heavy patches sounding like noise or distortion** (#4, `WavetableSynth.pch`). It was not the
  computer being too slow: the emulated DSPs believed some instructions took twice as long as on
  the real chip. The Nord Modular's OS packs each DSP with as many voices as fit in its time per
  sample, so a full DSP ran over, lost samples and turned the sound into noise. Such patches now
  keep their pitch and sound as they should.
- **The emulator stopped answering after many uploads in a row** (upload timeouts, then silence
  to everything). A stale piece of translated DSP code was reused after the OS had moved a loop.

## Known issue

On the first DSP one input-related interrupt still runs more often than on the hardware, which can
leave a little grit on the very heaviest patches. It is being worked on; reports with the patch
attached help a lot.

## Also in this build (from alpha.5 to alpha.7)

- The synth's internal master clock runs (sequencers and clocked patches). Set the clock to
  **internal** in the synth settings; in Animatek NME's Synth Settings dialog that option is shown
  the wrong way round for now: choose **External** there.
- Upload timeouts fixed: a deadlock after a DSP had been idle, commands to a busy DSP thrown away
  or never taken.
- The sawtooth of `OscA`, `OscB` and `OscSlvC` sounds on every platform.

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
