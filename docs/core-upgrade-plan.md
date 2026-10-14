# Core upgrade plan: the DSP56300 core from upstream

A work order, written 2026-10-14 after a review in session (Claude, asked by Javier). Start a fresh
session from here. Read `CLAUDE.md`, `NOTES.md` and `docs/performance-plan.md` first.

## Why

G1-Emu builds the DSP56300 and 68k cores from `~/src/gearmulator-md-mm` (joelanders' fork,
core at `1378c430`, 2026-09-16) plus 45 patches of our own applied to a build copy
(`cmake/Dsp56300.cmake`). Upstream (`dsp56300/dsp56300`) has moved on by ~75 commits since then,
and has fixed on its own several things we patch: CMPM leaving |S1| in a cached register, GT/LE
on x86-64 and AArch64, `move x:aa` / `y:aa` taking one cycle, DMA block transfers triggered by
request, the peripherals running while a masked interrupt waits, a query for whether a given
interrupt is pending (the idea of our 2026-10-14 DMA fix), and above all **a loop end (LA) that
moves**: the JIT now compares LA at loop ends and recompiles code holding a newly known LA.

That last one is the point. G1-Emu runs the JIT with the brakes on because the G1 OS moves its main
loop's end with every patch (`NOTES.md`, "Control-rate modules and the moving loop end"):
`linkJitBlocks = false`, `maxInstructionsPerBlock = 32`, `maxDoIterations = 1`
(`g1Lib/g1dsp.cpp`). Gearmulator's Nord Lead 2x (Nodal Red) runs the same core with linked
blocks and no block limit. With upstream's LA handling those limits may be liftable, which is
the largest CPU saving in reach (`performance-plan.md`, Phase 2.2). Not measured yet.

## What the review found

`tools/overlaycheck.py` applies our patches in order to a source tree and reports each one.
Against the current fork all 45 apply (sanity check). Against upstream `da2ff232` (2026-10-12),
cloned read-only at `~/src/dsp56300-upstream`:

- **19 apply as they are**: the G1's DMA modes (dual counters, fixed-to-fixed, block by request),
  host commands collected when the next interrupt is picked, the DMA interrupt queued once
  (BellsMorph), the idle-loop skip hooks, the volatile-P fix (#6), the single-op block cache,
  the `x:aa` cycle table.
- **2 are already there** (`jitops.h` declarations).
- **24 no longer find their anchor**, because upstream changed that code:
  - CMPM, GT/LE x86-64, LE AArch64: fixed upstream the same way. Drop ours.
  - DMA block transfers (`dma.cpp`, 3 patches): rewritten upstream; check the G1's DMA0
    (DAM 011 011, `X:$6C0` → output) and DMA3 (ESSI1 RX → `X:$6C5`, vector `$1E`) still work.
  - **ESSI fine clock (`esaiclock.h/.cpp`, 8 patches): rewritten upstream. Real work.** Our
    separate RX period for DSP 0's codec input (432 cycles per word) and the cap on an idle
    port's catch-up (the korg.pch deadlock) must be redone on the new code, or found there.
  - DO FOREVER / nested DO with FV (`jitblock.cpp`, `jitops.cpp`), `opcodeanalysis.h`,
    `jitblockchain.cpp` (3), `jit.cpp` (1), `dsp.cpp` (1): check one by one whether upstream
    covers the behaviour (its log since 2026-09-16 has loop and JIT fixes).
- **Trap:** upstream fixed `x:aa` / `y:aa` cycles in `opcodecycles.cpp` while our patch
  changes the table in `opcodecycles.h`. Both together would undercount. Keep one.

Full per-patch list: rerun the script (below).

## Plan

1. **Branch** `core-upstream` in G1-Emu. Do not touch `~/src/gearmulator-md-mm`: Elektron-Emu
   builds from it too. Point the DSP core alone at the upstream clone with a new cache path
   (e.g. `G1_DSP56300_DIR`, default `~/src/dsp56300-upstream`); upstream's `source/` has the
   same layout (`add_subdirectory(... /source)`), plus a `superluminalSdk` folder. The 68k core
   stays on the fork for now.
2. **Patches:** drop the ones upstream covers, keep the 19 that apply, redo the ESSI clock ones.
   `python3 tools/overlaycheck.py cmake/Dsp56300.cmake ~/src/dsp56300-upstream/source/dsp56kEmu`
   after each step; the configure fails loudly on a stale anchor anyway.
3. **Gate, all of it, before merging** (`CLAUDE.md` has the commands):
   `ctest --test-dir build`, `g1dspcheck`, `jitdiff`, `tools/bench/bench.sh` (0 overruns,
   BellsMorph included), `tools/bench/golden.sh check` (re-record only with a reason, and
   compare pitch/level/flatness as in the 2026-10-14 CHANGELOG entry), the module battery
   (62 / 19 / 11 / 17, AudioIn 440 Hz at −62 dBFS with `--input-sine`), the 18-upload sequence
   (`G1_BEFORE`, `NOTES.md`), `tools/upload-e2e.sh`, and the plugin (`g1vst3check`). CI must stay
   green on all five jobs, the arm64 ones included (the GT/LE story).
4. **Only then, speed:** try `linkJitBlocks = true`, larger `maxInstructionsPerBlock`, and
   `maxDoIterations > 1`, one at a time, measured with `--bench` and gated as above.
5. **Separate and small, any time:** `juce::ScopedLowPowerModeDisabler` in the plugin's
   processor, as Gearmulator did (their PR #277), so macOS does not slow the plugin down on
   Apple Silicon (Nocticore's M1 reports).

## Context from Gearmulator's own issues

- Nodal Red 2x also needs a fast CPU: their #250 (stuttering on a laptop) was solved by the
  performance power profile. Same advice applies to G1-Emu users.
- Their DSP performance history (`doc/dsp_performance_history.md` upstream): +50 % JIT
  throughput on x64 since 2.2.9, mostly the left-aligned ALU and the trampoline. Our fork already
  has those; what we miss is the block linking above.
