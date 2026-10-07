# G1-Emu

![G1-Emu: the panel of the emulated Nord Modular G1](docs/g1gui.png)

**An emulator of the Nord Modular G1** (rack, OS 3.03) built on the
[Gearmulator](https://github.com/dsp56300/gearmulator) core: the hardware's original operating
system runs on an emulated Motorola 68331 and four emulated DSP56303s, and
[Animatek NME](https://github.com/animatek/Animatek-NME) edits it as if it were the real synth.

**Status: pre-alpha.** The OS boots, NME connects over the PC Port and uploads patches, and it
sounds clean in real time: oscillators, filters, envelopes, clocks, effects (chorus, overdrive…),
four outputs and two inputs. It has a window with the panel (display, knobs, buttons and LEDs).
**Runs on Linux, macOS and Windows.** The three systems build in CI, and their DSP tests pass on
x86-64 and arm64. The macOS and Windows applications have also been run on real machines and
connected to Animatek NME; Windows currently uses two loopMIDI cables because of a Windows MIDI
Services bug. See the [Windows first-run guide](WINDOWS.md). Builds in
[Releases](https://github.com/animatek/G1-Emu/releases) are unsigned and contain no ROM
([release notes](docs/release-notes.md)). Also missing:
module-by-module testing, and the one panel key whose job is still unknown. The technical details are in [`NOTES.md`](NOTES.md), the plan in
[`ROADMAP.md`](ROADMAP.md) and what changes in [`CHANGELOG.md`](CHANGELOG.md). Contributions are
welcome: see [`CONTRIBUTING.md`](CONTRIBUTING.md).

## You are invited: this is a collaborative project

G1-Emu was started by [Animatek](https://animatek.net), but it is a big project and it is meant to
be built together. **Everyone is warmly invited to take part** — reverse engineering, C++, DSP
code, testing patches against a real G1, recordings, documentation, builds for macOS and Windows,
or simply reporting what sounds wrong. We love collaboration, and every person who joins makes the
emulator bigger and better. There is room for all skill levels: see
[`CONTRIBUTING.md`](CONTRIBUTING.md) and the open tasks in [`ROADMAP.md`](ROADMAP.md), and say
hello in an issue.

## Please read this first

- **Not affiliated with Clavia.** G1-Emu is an independent, open-source project. It is not
  affiliated with, endorsed by or connected to Clavia DMI in any way. "Nord" and "Nord Modular" are
  trademarks of Clavia DMI; they are used here only to say which instrument is emulated.
- **No ROMs, now or ever.** No ROM or firmware is included, and none will be provided. Please do
  not ask for them in issues, e-mails or messages: you will not find them here. You need the 512 KB
  ROM of a Nord Modular rack with OS 3.03 (`Roms/NORD-MODULAR-RACK-VER-3.03.BIN`), dumped from
  your own unit. `Roms/` is ignored by Git and must stay that way.
- **No support.** This is a pre-alpha community project, made in spare time by its maintainer and
  whoever wants to join. There is no support: please do not ask for help, builds or ROMs. Bug
  reports with details, and contributions, are welcome (see [`CONTRIBUTING.md`](CONTRIBUTING.md)).

The window shows this same notice the first time it runs. After that it stays out of the way: it
is always readable in **Settings**, which is also where to put it back at startup.

## Building

Linux (tested on Arch/CachyOS with PipeWire). You need:

- A clone of [gearmulator-md-mm](https://github.com/joelanders/gearmulator-md-mm) in
  `~/src/gearmulator-md-mm` (or `-DGEARMULATOR_DIR=...`). The version tested in CI is
  **`mdmm-v0.1.0-alpha.13`**. The clone is not modified: the core fixes the G1 needs are applied to
  a copy at build time (`cmake/Dsp56300.cmake`).
- ALSA, and JACK (pipewire-jack) for the four outputs and the inputs.
- JUCE for the window and the test bench: by default the one inside Animatek NME next to this repo
  (`../Nomad2026/JUCE`), or `-DG1_JUCE_DIR=...`. Without JUCE only the console version is built.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

## Two things a Nord Modular needs, and neither comes in the box

**A G1 makes no sound on its own, and it starts empty.** It is a modular: a patch is what turns it
into an instrument, and without one there is nothing to hear. Real hardware left the factory with
a bank of patches in its flash; G1-Emu cannot give you those, because the ROM carries the
operating system and nothing else — the first time it runs, its flash is built from the OS alone
and every slot says `Empty patch`. **The patches are yours to find or to make.** There are
thousands of `.pch` files out there from twenty-five years of the Nord Modular community: the old
Clavia patch libraries, the Electro-Music archives, and whatever a search turns up. Put them
wherever you like and open them from your editor.

**And to make or load a patch you need an editor.** The G1's own panel edits parameters, not
patches: on the real instrument the patch comes down the PC Port from a computer, and the emulator
is no different. **Any editor that speaks the G1's protocol works** — G1-Emu emulates the
instrument, not one particular editor, and there is nothing in it that prefers one over another:

- **[Animatek NME](https://github.com/animatek/Animatek-NME)** — a modern editor, open source, and
  the one this emulator is developed against, which only means it is the one that gets tested
  first.
- **The original Clavia editor v3.03** — the real thing. It still works, and
  [Stage Engine](https://www.stage-engine.com/) packages it for current macOS with the Wine parts
  bundled in, free with an optional donation, for the G1 and the Micro Modular.
- **[Nomad / NMEdit](https://sourceforge.net/projects/nmedit/)** — the long-running open-source
  Java editor, GPLv2, which also reads and writes `.pch`.
- **[nordmodulareditor.com](https://nordmodulareditor.com)** — another editor project.

If yours is not on the list and it works, say so in an issue and it goes in. If it speaks the
protocol and G1-Emu does not answer it properly, that is a bug here, not in your editor, and we
want to hear about it.

## Using it

```bash
./g1gui.sh   # with the panel in a window
./g1.sh      # in the console (Ctrl+C saves and quits)
```

- **MIDI** (ALSA): client **G1-Emu** with two ports, like the hardware: **PC Port** (the editor:
  choose it in NME as input and output) and **MIDI** (notes and controllers, e.g. from a DAW).
  Programs that read **raw MIDI devices** instead (Bitwig on Linux) see no sequencer port at all:
  for them the emulator takes over a card of its own — a one-port USB MIDI gadget — and links it
  by itself, with nothing to run or route by hand. See [raw MIDI](docs/bitwig-midi.md).
- **Audio** (JACK): `G1-Emu:out_1..out_4` and `in_L`/`in_R`; `out_1`/`out_2` connect themselves to
  the sound card. Without JACK, outputs 1/2 go through ALSA.
- **The ROM** is yours to provide and is looked for in `<Documents>/Animatek/G1-Emu/roms`, in
  `roms/` next to the flash, and in `Roms/` of the current directory and the source tree. With none
  found, the window says what is needed, opens that folder for you, or lets you pick the file;
  `Settings` shows which one is running and changes it. A file that is not the right ROM is told
  apart from a missing one, and the reason is given.
- **Settings** (in the window): the ROM, audio driver and device, output level, whether outputs 1/2
  connect themselves, the raw MIDI card, and the notice above. The console reads the same settings;
  the `G1_*` environment variables win over them.
- The flash (installed OS and stored patches) is saved beside it. On Windows both live in
  `%APPDATA%\Animatek\G1-Emu`; on Linux and macOS the existing path is
  `~/.local/share/Animatek/G1-Emu`.
- The level is low because the OS itself caps the master volume at −36 dB; it is compensated with
  +36 dB (`G1_GAIN_DB`). More settings in [`CLAUDE.md`](CLAUDE.md).

## Optional: the OS a real G1 runs

The ROM carries a factory OS, and that is what G1-Emu runs unless told otherwise. **It works**:
patches load, play and edit, and the plugin keeps its state in your projects. But it is not quite
the OS an updated G1 runs from its flash, Clavia's 3.03b update, and they differ: the factory one
does not read the stored synth settings at power-on, so **Shift+Store's slots do not come back**
when G1-Emu starts again (every slot says `Empty patch`), where a real G1 starts with them. More
differences may turn up.

You can install the real one into G1-Emu the way you would into the hardware, with Clavia's own
updater. It is optional, done once per machine, and needs:

- **Clavia's free Windows updater for the Nord Modular, OS 3.03b** (`Nord Modular OS v3.03b
  Update.exe`), which you find and download yourself. The Mac version of it is for Mac OS 8/9 and
  runs on no current Mac: use the Windows one everywhere.
- **On Linux or macOS, Wine** to run it (on macOS also CrossOver or Whisky). On Windows it runs as
  it is, but G1-Emu's PC Port needs a loopback driver there for now (see Windows notes).

Then:

1. Start G1-Emu in update mode: `G1_UPDATE=1 ./g1gui.sh`. The display shows `Update utility`: the
   G1 has no OS to run and waits for one. Your banks are not touched.
2. Run the updater (`wine "Nord Modular OS v3.03b Update.exe"`) and choose **G1-Emu's PC Port** as
   its MIDI output and input. Follow its steps. The G1's display counts the percentage received
   and ends with `Update completed`. If the updater complains before the G1 is done, let the G1
   finish: what counts is its display.
3. Close G1-Emu. The OS that came in is kept in the data folder (`os/received-….bin`) and set as
   `os =` in the settings, so the window, the console and the plugin all run it from then on.

To go back to the factory OS, empty `os =` in the settings file. Two things to know: a DAW project
saved with one OS does not apply its G1 state under the other (it is kept as it is, nothing is
lost), and the received OS is Clavia's, like the ROM: keep it to yourself.

## The VST3 and CLAP plugin (beta)

The same G1 as an instrument inside a DAW: **notes from the track, audio back to the track**, with
no JACK client and no virtual MIDI card. Its one MIDI port is the PC Port, for the editor (below).
Tested on Linux; it builds everywhere the standalone does.

```bash
cmake --build build --target g1plugin_VST3 g1plugin_CLAP -j$(nproc)
cp -r build/app/plugin/g1plugin_artefacts/Release/VST3/G1-Emu.vst3 ~/.vst3/
cp build/app/plugin/g1plugin_artefacts/Release/CLAP/G1-Emu.clap ~/.clap/
```

The CLAP build needs a clone of [clap-juce-extensions](https://github.com/free-audio/clap-juce-extensions)
with its submodules in `~/src/clap-juce-extensions` (or `-DG1_CLAP_DIR=...`); without it, only the
VST3 is built.

- **Outputs 1/2 and 3/4** as two stereo buses, and **In L/R** as an optional input.
- **The ROM** is found exactly like the standalone finds it; with none, the plugin's window says
  what it needs and lets you pick the file (remembered in the same settings file).
- **Each instance has its own G1.** A new one starts from a *copy* of the standalone's flash, so
  your banks are already there; from then on its patches and synth settings live in the DAW
  project, never in the standalone's files. What is saved is only what differs from the factory
  flash — the OS, which is the ROM's, never goes into your project.
- **The 18 knobs are parameters** the DAW can automate. Each one is named after what it moves in
  the current patch (`Knob 3: OscA Freq coars`) and shows the value the OS gives it; turning one
  on the panel moves the parameter too.
- **Choosing a patch:** a MIDI Program Change (and Bank Select) from the track, or Patch/Load on
  the panel. VST3 carries no Program Change as MIDI, so the plugin also has 128 programs: a host
  that sends Program Changes to a VST3 sets those, and they reach the G1 on channel 1. The G1 does not remember what each slot had, so the plugin remembers the last Program
  Change of each channel and sends it again when the project opens.
- **Latency** is one host block plus about 5.5 ms, and it is reported to the DAW, which
  compensates for it: notes land on the exact sample the DAW put them on, whatever the block size.
- **Editing from Animatek NME:** each instance opens a virtual MIDI port for its PC Port,
  **G1-Emu PC Port** (the second instance **G1-Emu 2 PC Port**, and so on). Choose it in NME as
  input and output, as with the hardware. Linux and macOS; on Windows JUCE cannot create the port,
  and the direct link of [#8](https://github.com/animatek/G1-Emu/issues/8) is what will cover it.
  `G1_PLUGIN_PC_PORT=0` leaves the port out.
- Loading a patch mutes the G1 for a moment, as on the hardware.

## License and credits

**Panel skin and GUI design by Mike Fiction.**

The **About** button in the extras drawer shows all of this inside the program.

**Licenses.** G1-Emu's code is GPLv3 (see [`LICENSE`](LICENSE)), because it links Gearmulator. The
panel artwork in [`app/gui/skin`](app/gui/skin) and the standalone's icon are © 2026 Mike Fiction,
under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) (see
[`app/gui/skin/LICENSE.md`](app/gui/skin/LICENSE.md)). Inside the program as well: Gearmulator's
dsp56300 and mc68k (GPLv3), the Musashi 68000 core by Karl Stenerud (MIT), AsmJit (zlib), JUCE 8
(AGPLv3), the VST 3 SDK by Steinberg (MIT; VST is a trademark of Steinberg Media Technologies GmbH),
and CLAP with clap-juce-extensions (MIT).

**Credits.** Made by Javier Melgar, Animatek ([animatek.net](https://animatek.net)), who also makes
[Animatek NME](https://github.com/animatek/Animatek-NME), the editor that connects to G1-Emu by
itself. Mike Fiction designed the panel skin and the GUI. Thanks to The Usual Suspects for
[Gearmulator](https://github.com/dsp56300/gearmulator) and to joelanders for the fork with the
Monomachine and Machinedrum, which this work builds on; to Tuth for the Windows fixes in PR #5; to
Psychlist1972 for the help with Windows MIDI Services; and to everyone who tests and reports:
AlphasiaIndustries, artqcid, Garrincha568, JuliusLC, masc4ii, ModGod222, msavery123, nirsu1,
psy-dub and Waltercalling.

Nord and Nord Modular are trademarks of Clavia DMI AB, which neither makes, endorses nor supports
G1-Emu. G1-Emu ships no ROM.
