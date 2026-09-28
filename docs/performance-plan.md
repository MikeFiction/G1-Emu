# Performance plan

A work order for an agent (written 2026-09-27 for Codex). Goal: heavy patches must run in real
time with headroom; today some of them stutter. Read `AGENTS.md`, `CLAUDE.md` and `NOTES.md`
(sections "How each DSP processes audio", "The links between DSPs", "Control-rate modules and the
moving loop end") before touching anything. The rules there are not negotiable: English only,
`CHANGELOG.md` line in the same commit, never modify `~/src/gearmulator-md-mm` (core changes go
through the overlay in `cmake/Dsp56300.cmake` / `g1Lib/dsp56300.cpp`, like the existing ones).

**Measure first, change second, one change per commit, each with before/after numbers.** No
optimisation is accepted without the correctness gate of phase 0 passing.

---

## Read this first — "stuttering" is mostly emulated DSP time, not host CPU (2026-09-27)

Investigating #4's `WavetableSynth.pch` (Rob Hordijk; 11 voices; the user hears noise) showed that
the heavy patches that "stutter" are, at least in part, **not a host-speed problem**: headless, the
emulator runs this patch faster than real time and still outputs white noise (spectral flatness
0.71, no fundamental), identically on every run. The cause is the **emulated cycle budget**:

- The OS fills each DSP with as many voices as its own cycle table says fit in 864 cycles per
  sample. If the emulator charges more cycles than the chip, a full DSP goes past 864: the next
  IRQD (sample clock) finds the last one still pending and is dropped (`Dsp::runUntil`,
  `m_irqdPending`), the control-rate code only gets one JIT block per sample, and the output is
  noise. `g1patchtest` with `G1_VERBOSE=1` now prints `overruns=` per DSP (dropped sample clocks)
  and `cyc/ins=`. **Any patch with overruns > 0 is broken regardless of host speed.**
- **Fixed (local change): `move x:aa` / `y:aa` cost 2 cycles in the core's table**
  (`opcodecycles.h`), 1 on the chip. Overlay in `cmake/Dsp56300.cmake`. With it, DSPs 1 and 2 of
  WavetableSynth go from ~6,200 overruns in 3 s to 0; the block routine measures 682 cycles per
  sample at 1.02 cycles/instruction, plus ~150 cycles/sample of control code.
- **Still open — DSP 0 services the codec-input DMA interrupt (vector `$1E`) ~6.6 times per
  sample** (3.7 M against 564 k IRQDs; DSPs 1–3 never take it). Its receivers have no upstream DSP,
  so they run at their own CRA's rate (96 cycles/word, 9 words/sample), while on the hardware the
  codec delivers far fewer words per sample. The extra interrupts push DSP 0 over budget (28,000
  overruns in 3 s with WavetableSynth, 30,000 with Purbrick's `WavetablePad.pch`; DSPs 1–3 at 0).
  **Experiment that worked** (reverted, not committed): in `Dsp::drainAudio`, for `!m_hasUpstream`,
  `setEsaiFinePeriod(essi, 432)` (2 words per sample). DSP 0 overruns → 0 on both patches, `$1E`
  → ~1.4 per sample, WavetableSynth comes out at 261.5 Hz for note 60 and WavetablePad at flatness
  0.035. Caveat: the fine period is per ESSI, so it also slows DSP 0's **transmitter** to DSP 1
  (the audio still arrived, because links are read from memory by `tapLink`, but the TX DMA
  timing changes) — and it changes how the **audio inputs** (AudioIn, `--input-sine`) arrive. The
  right fix is probably a separate RX rate for DSP 0 matching the real codec frame (work out the
  codec's words per sample from DSP 3's CRA/CRB and the ADC wiring; see NOTES.md "Outputs, inputs
  and level"), verified with `--input-sine` and the AudioIn battery entry.
- **Audit the rest of the cycle table** against DSP56300FM Appendix A for what the G1 code uses:
  `Movel_aa` is listed as 0 (!), check `movep`, `bset/bclr/bchg/btst` on `aa`/`pp`, `Bcc`/`Jcc`/
  `brset`/`bsset` (measured ~5 cycles), `do`, `rep`, `rti`, the long-interrupt entry, and the
  `DefaultPreventInterrupt` block the core forces after every RTI. Method used: a temporary per-PC
  cycle histogram in `runUntil` with `G1_JITBLOCK=1` (one instruction per block), joined with
  `G1_DUMP` + `dspdis`; make it a proper diagnostic (`G1_CYCPROF=dsp`) in `g1patchtest`.
- **New gate, before any speed work:** a bench of heavy patches must show `overruns=0` on every
  DSP after the note (WavetableSynth, WavetablePad, and the ones the user names). Add it to
  `bench.sh` as a hard failure. Also add a battery column or a separate run that reports overruns.
- Output is **deterministic**: `G1_THREADS=0` and threaded, two runs each, give byte-identical
  WAVs. Phase 0's golden-WAV gate can therefore be strict (byte-identical), except for the changes
  above that fix timing on purpose.

Only after this does host-side optimisation (below) matter for these patches.

## Baseline and results — 2026-09-27

The pre-fix Release build reproduced the DSP 0 timing fault in three-second runs:

| Patch | DSP 0 overruns | DSP 1–3 overruns | Result |
| --- | ---: | ---: | --- |
| `WavetableSynth.pch` | 28,738 | 0 | incorrect output, ~275–279 Hz |
| `WavetablePad.pch` | 30,249 | 0 | incorrect output |
| `SimpleOSC.pch` | 0 | 0 | 261.7 Hz, −61.8 dBFS |

The accepted fix gives DSP 0's codec receivers their own 432-cycle word clock (two words per
864-cycle sample), while DSP 0 transmitters remain at 96 cycles per word. After the fix,
standalone and `G1_BEFORE` runs of WavetableSynth and WavetablePad report `overruns=0` on all
four DSPs; WavetableSynth measures about 261.5–262.3 Hz and spectral flatness 0.012. SimpleOSC
remains at 261.7 Hz and −61.8 dBFS. The AudioIn battery entry remains 440 Hz at −61.8 dBFS with
`--input-sine 440`, and the full battery remains 62 sounds / 19 moves / 11 fixed / 17 silent.
`g1dspcheck` and `ctest --test-dir build` pass. Host benchmark mode and golden WAVs are recorded
below; the cycle-table audit follows it. Phase 1 measurements are still pending.

## Phase 0 benchmark baseline — 2026-09-28

The new `g1patchtest --bench` mode measures the fast, unslept emulation run after the note. It
reports the emulated and wall seconds, realtime factor, CPU and worker-thread busy/waiting time,
periodic and host-port barriers per second, JIT blocks per DSP per second, and dropped sample clocks.
`tools/bench/bench.sh` runs the nine entries in `tools/bench/patches.txt` and exits non-zero if any
DSP reports an overrun. `BUSIEST_THREAD` is the thread that waits least at the barrier, the one that
limits the speed; DSP 0 runs on the CPU thread and is counted with it. The CPU thread is not timed
per instruction: a first version did, and the two clock reads per 68k instruction made the run 65 %
slower (10.0 s instead of 6.0 s for the same WavetableSynth run), so its busy time is the wall time
minus the DSP work and the waits measured on that thread. Three-second Release baseline (Ryzen 7
5700X; the realtime factor varies ±10 % between runs with the machine's load):

| Patch | Realtime | Busiest thread | Its waiting | DSP overruns |
| --- | ---: | --- | ---: | ---: |
| WavetableSynth | 1.89× | CPU (68k + DSP 0) | 17.5 % | 0 |
| WavetablePad | 2.15× | CPU | 18.3 % | 0 |
| SimpleOSC | 1.88× | CPU | 12.7 % | 0 |
| Grainalizzer | 1.90× | CPU | 26.1 % | 0 |
| DungeonDub | 1.74× | CPU | 30.7 % | 0 |
| WindowLicker | 1.85× | CPU | 13.9 % | 0 |
| FM303 | 1.84× | CPU | 19.3 % | 0 |
| 4VoiceChoir | 1.91× | CPU | 26.6 % | 0 |
| progger (G1_CLOCKSRC=1) | 1.89× | CPU | 14.7 % | 0 |

**What it says:** the CPU thread, which runs the 68k and DSP 0 one after the other, limits every
patch; the worker threads (DSPs 1–3) wait ~45 % of the time. Phase 1 step 1 (DSP 0 on its own
worker) is therefore first. Barriers: on WavetableSynth, per emulated second, ~20,500 periodic and
**~40,000 triggered by host-port accesses** — twice as many, which makes Phase 1 step 4 (catch up
only the DSP whose port is accessed) the next candidate.

Golden WAVs for the nine entries: `tools/bench/golden.sh record` / `check`, three emulated seconds,
references outside the repository (they depend on the ROM) in
`~/.local/share/Animatek/G1-Emu/bench-golden` (`G1_BENCH_GOLDEN` overrides it). Threaded,
`G1_THREADS=0` and reference are byte-identical for all nine.

The obsolete 1.25–1.44× table from the per-instruction host-clock version has been removed;
it was not a valid speed baseline.

## Cycle-profile diagnostic and timing audit — 2026-09-28 (item 2)

```sh
G1_JITBLOCK=1 G1_CYCPROF=0 build/tools/patchtest/g1patchtest_artefacts/Release/g1patchtest \
  Roms/NORD-MODULAR-RACK-VER-3.03.BIN SimpleOSC.pch --note 60 --seconds 3
python3 tools/cycprof.py --output-dir /tmp/g1-cycle-audit
python3 tools/cycprof.py --output-dir /tmp/g1-cycle-audit --summary-only
```

`G1_CYCPROF` accepts exactly one DSP index, 0–3, and requires `G1_JITBLOCK=1` and JIT on
that DSP. It starts after the note is queued, at the benchmark boundary. `CYCPROF_PC` rows
are sorted by total emulated cycles and include execution counts, opcode snapshots and
disassembly from the same disassembler used by `dspdis`; rewritten PCs retain distinct
versions. `CYCPROF_MNEMONIC` totals are also cycle-sorted. `CYCPROF_FORM` identifies both
halves of parallel instructions and the core table tuple; these form totals are inclusive,
so must not be summed. `tools/cycprof.py` resolves their numeric IDs to core enum names.

Vectors are observed separately from the instruction following the peripheral checkpoint.
REP bodies are expanded from their measured emulated cycle delta and body cost, not the
core instruction counter (which currently overcounts by one per REP). The summary reports
both counts, reconciliation errors and unaccounted cycles; failed reconciliation is a
non-zero exit. Synthetic `g1dspcheck` cases cover REP, fast/long vectors, RTI and rewritten
P memory. This is an **emulator timing diagnostic**, not a hardware cycle measurement.

When unset, the regular execution loop has no profiling calls, counter updates or host
clock reads. Selection happens once per `runUntil`, outside the loop. Benchmark cycle/CPI
totals use only boundary snapshots. `tools/cycprof.py` runs WavetableSynth, WavetablePad,
DungeonDub and SimpleOSC on all four DSPs and checks each WAV against an unprofiled
`G1_JITBLOCK=1` render. Logs and WAVs contain ROM-derived material and must stay outside
the repository. Never use diagnostic one-instruction blocks for host-speed comparisons.

### Results

The audit itself was not finished (Codex's quota ran out). Committed so far: the diagnostic above.
Pending: the table of every executed instruction form against DSP56300FM Appendix A.

---

## How it runs today (what the plan is built on)

- One **emulator thread** (`app/emuhost.cpp`, `EmuHost::run`) runs the 68331 (Musashi) in slices
  of at most 2 ms of emulated time, then sleeps 500 µs if it is ahead of the wall clock.
- `Microcontroller::exec` (`g1Lib/g1mc.cpp`) calls `catchUpDsps()` **every 1024 CPU cycles**
  (20.97 MHz / 1024 ≈ **20,480 barriers per second**, ≈ 4,050 DSP cycles ≈ 4.7 audio blocks each),
  and **also on every CPU access to a DSP host port** (`read16`/`write16`/... at lines ~257, 281,
  329, 421), which during uploads and polling can be far more often.
- In a barrier, DSPs 1–3 run on worker threads (spin 20,000 `pause`, then condvar) and **DSP 0 runs
  on the emulator thread itself**, after which that thread spins until the workers finish. So the
  critical path per barrier is `68k slice + max(DSP0, DSP1, DSP2, DSP3) + flushAudio × 4`, and
  DSP 0 is never parallel to the 68k.
- Each DSP is a DSP56303 at **82.944 MHz** (864 cycles per 96 kHz sample) on Gearmulator's JIT,
  one JIT block per `m_dsp.exec()` call in `Dsp::runUntil` (`g1Lib/g1dsp.cpp`), with per-block
  bookkeeping: boot-ROM check, trace, pcWatch, IRQD grid, LA check, `drainAudio` every ~1024 cycles.
  `maxDoIterations = 1` (one loop iteration per block) — required by the LA fix, but costly.
- The DSP main loop is a `do forever` that polls the host port: **a DSP never idles**, it burns its
  full 83 MHz whether it has voices or not. The ESSIs run in "fine link" mode, clocked per word.
- The ROADMAP (item 6) measured ~45 % headroom on a Ryzen 7 5700X with a light patch.

---

## Phase 0 — Instruments and a correctness gate (do this first, commit it)

1. **Benchmark mode in `g1patchtest`**: `--bench` (or `G1_BENCH=1`) runs N emulated seconds after
   the note as fast as possible and prints: emulated seconds, wall seconds, **realtime factor**
   (emulated/wall), and per thread the time spent working vs. waiting at the barrier (add
   `steady_clock` accumulators around `catchUp` in `workerLoop` and around the 68k slice and DSP 0
   in `catchUpDsps`; behind a flag or cheap enough to leave on). Also count barriers per second,
   split into "periodic" and "host-port triggered", and JIT blocks executed per DSP per second.
2. **A patch set**: `tools/bench/patches.txt` listing ~10 patches from light to the ones that
   stutter. Heavy candidates (not in the repo, reference them by path; the user will name more):
   `/mnt/ARCHIVOS/Librerias/NordModular/011_patches/Hordijk_Rob/WavetableSynth.pch` (#4),
   `.../011_patches/Purbrick_Jim/WavetablePad.pch`, and nmedit's `progger.pch` / `future303.pch`
   (these two are silent under a plain `--note`: they need a clock, `G1_CLOCKSRC=1`). The library
   `/mnt/ARCHIVOS/Librerias/NordModular/` has thousands more: a sweep that reports `overruns` per
   patch is the quickest way to find the heavy ones. Plus `SimpleOSC.pch` as the light baseline. A script `tools/bench/bench.sh` runs them all and prints
   one table (patch, realtime factor, slowest thread, % time waiting).
3. **Golden outputs**: for each bench patch, record a WAV (`--wav`) of a few seconds with a fixed
   note/chord. Check first that the output is **deterministic** (two runs, and `G1_THREADS=0` vs
   threaded, give byte-identical WAVs). If it is, every later optimisation must keep it
   byte-identical; if it is not, find out why and document it before going on, then use a
   tolerance (peak and RMS per output within 0.1 dB, same frequency) instead.
4. The existing gates still apply to every commit: `ctest`, `g1dspcheck`, `jitdiff` on a P dump,
   `tools/battery/battery.py` (no module that sounded may stop sounding).
5. **Profile** one light and the two heaviest patches. `perf` is not installed on the maintainer's
   machine: ask the user to install it (`sudo pacman -S perf`) rather than doing it; meanwhile use
   the phase-0 counters. Build with `-fno-omit-frame-pointer` in a separate build dir
   (`build-prof`). Record where the time goes: JIT code, JIT compilation, Musashi, peripherals
   (ESSI/DMA/HI08 exec), `runUntil` bookkeeping, barrier spinning, `flushAudio`/link copies.

Write the numbers into this file (a "Baseline" section) before phase 1.

---

## Phase 1 — Cheap, low-risk wins (pick by what the profile says)

Each is independent; measure each one alone.

1. **DSP 0 on its own worker.** The emulator thread only runs the 68k and then waits; DSP 0 gets a
   worker like the others. Expected: the critical path becomes `max(68k, DSP0..3)` instead of
   `68k + DSP0`. Probably the biggest single win whenever the patch's voices land on DSP 0. Keep
   `G1_THREADS=0` working (serial).
2. **Pin and name threads** (optional affinity, `G1_AFFINITY=1`): workers on distinct physical
   cores, avoiding SMT siblings. Measure; do not enable by default without numbers.
3. **Barrier cost.** Measure how much wall time is spent in the barrier itself. Options, in order
   of risk: replace the `m_pending` counter spin with per-worker done flags on separate cache lines
   (avoid false sharing between `m_generation`, `m_pending`, `m_dspTarget`); `alignas(64)` the
   shared atomics; avoid `notify_all` when nobody sleeps (already done — verify).
4. **Fewer host-port barriers.** A CPU access to a host port only needs *that* DSP caught up, not
   all four. Catch up only the DSP whose port is accessed (it runs on the CPU thread while its
   worker is idle — careful: it must not race a worker; simplest is to only do the targeted
   catch-up when no barrier is in flight, which is always true on the CPU thread between
   barriers). Count host-port barriers before/after; must not change the golden WAVs nor break
   uploads (`tools/upload-e2e.sh`, the 18-patch upload sequence in `NOTES.md`).
5. **Longer periodic quantum.** The links tolerate `g_linkLatency = 8` blocks and a barrier spans
   ~4.7. Try every 2048 CPU cycles (~9.4 blocks) with `g_linkLatency` raised to 12–16. This halves
   barriers. Risk: link timing and host-port handshakes; the golden WAVs will shift by the added
   latency (document it, check the content is otherwise identical by aligning). Make the quantum
   a single constant.
6. **`runUntil` bookkeeping.** The core already has `DSP::execUntilCycles(target)`, which runs
   cached blocks under one trampoline entry with the same interrupt/peripheral checks; `runUntil`
   calls `exec()` once per block. Use it for the stretch up to the next IRQD grid point when no
   diagnostic is on (the LA check after each block is the obstacle: see phase 2.2). Also hoist the `m_traceOn` / `m_pcWatch` / `m_interpreter` checks out of
   the hot loop (a separate slow-path loop when diagnostics are on); make the LA check a single
   compare. Small, but it runs once per JIT block, and blocks are short because of
   `maxDoIterations = 1`.
7. **Compiler.** Try `-flto` (`INTERPROCEDURAL_OPTIMIZATION`) on the core + g1Lib, and PGO
   (`-fprofile-generate` / `-fprofile-use` driven by `bench.sh`) as an opt-in CMake option
   (`G1_PGO`). **No `-march=native` in release builds** (they must run on any x86-64 and arm64);
   an opt-in `G1_NATIVE=ON` for local builds is fine. CI must stay green on all five jobs.

---

## Phase 2 — Structural (only after phase 1, with profile data justifying it)

1. **Idle-loop skip.** When a DSP is in its main-loop polling (PC inside the `do forever` body up
   to the current LA, no host data/command pending, HF0 unchanged, no DMA/ESSI event due before
   the next IRQD) fast-forward its cycle counter to the next IRQD instead of executing the loop.
   Peripherals must be advanced by the same cycles (ESSI words, DMA) — this is the hard part;
   look at how the core handles `WAIT`. Big win for DSPs with no or few voices; it frees cores so
   the busy DSP gets a full one. Must be verified against the golden WAVs **and** the upload
   sequence (the host polling is exactly what the skip touches). Env var to disable
   (`G1_NO_IDLE_SKIP`).
2. **`maxDoIterations > 1`.** One iteration per JIT block exists because of the moving loop end
   (`onLaChanged`). If the profile shows block-dispatch overhead dominating, investigate letting
   the JIT run several iterations while still detecting an LA change (LA only changes by a host
   command, so it can be checked at host-command time instead of after every block). Verify with
   `g1dspcheck`, `jitdiff` and the control-rate modules of the battery. High risk; stop and write
   up findings if it gets hairy.
3. **Peripheral cost.** If ESSI/DMA exec shows up high, look at running the "fine link" clock
   lazily (compute words due since the last access instead of ticking) — through the overlay.
4. **Drop the TX side of the links** (ROADMAP item 6): the link words are already read from DSP
   memory (`tapLink`), so the ESSI transmit emulation of DSPs 0–2 may be skippable.

---

## Phase 3 — Real-time host loop

Independent of raw speed: with speed close to 100 % the host loop should not add dropouts.

- The emulator thread sleeps a fixed 500 µs and runs slices of ≤ 2 ms. Check the audio queue fill
  in `audiobridge.h` over time on a heavy patch: if underruns come from scheduling jitter rather
  than lack of speed, sleep until the queue drops below a threshold instead, and consider
  `SCHED_FIFO`/`SCHED_RR` for the emulator thread and workers where allowed (opt-in, graceful
  fallback without permissions).
- The status bar's "load" should show the **slowest thread's** load, not only the emulator thread's,
  so the user sees when a patch is at the limit.

---

## Deliverables

- One commit per step, each with its `CHANGELOG.md` line (what, why, measured before/after, how
  verified), and the global changelog entry per `AGENTS.md`.
- This file updated with the baseline and a results table after each phase.
- If something makes the output differ from the golden WAVs, stop, do not "fix" the goldens, and
  write down what differs.
- Do not push, tag or release; the maintainer does that.
