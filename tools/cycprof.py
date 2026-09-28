#!/usr/bin/env python3
"""Record the 4-patch x 4-DSP timing audit, or summarize its existing logs.

Disassembly is embedded by g1patchtest, using the same core disassembler as dspdis.
Logs contain ROM-derived code: keep them OUTSIDE the repository.
"""

import argparse
import filecmp
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PATCHES = {"WavetableSynth", "WavetablePad", "DungeonDub", "SimpleOSC"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--seconds", type=float, default=3)
    parser.add_argument("--summary-only", action="store_true")
    args = parser.parse_args()
    out = (args.output_dir or Path(tempfile.mkdtemp(prefix="g1-cycprof-"))).resolve()
    if out == ROOT or ROOT in out.parents:
        parser.error("profile logs must be stored outside the repository")
    if args.seconds <= 0:
        parser.error("--seconds must be positive")
    if args.summary_only and not args.output_dir:
        parser.error("--summary-only requires --output-dir")
    out.mkdir(parents=True, exist_ok=True)
    binary = os.environ.get("G1_PATCHTEST", str(ROOT / "build/tools/patchtest/g1patchtest_artefacts/Release/g1patchtest"))
    rom = os.environ.get("G1_ROM", str(ROOT / "Roms/NORD-MODULAR-RACK-VER-3.03.BIN"))
    profiles = []
    for line in (ROOT / "tools/bench/patches.txt").read_text().splitlines():
        if not line or line.startswith("#"):
            continue
        name, path, note, extra = line.split("|")
        if name not in PATCHES:
            continue
        reference = out / f"{name}-unprofiled.wav"
        env = dict(os.environ, G1_JITBLOCK="1", G1_VERBOSE="1")
        env.pop("G1_CYCPROF", None)
        for item in extra.split():
            key, value = item.split("=", 1)
            env[key] = value
        command = [binary, rom, path, "--note", note, "--seconds", str(args.seconds)]
        if not args.summary_only:
            with (out / f"{name}-unprofiled.txt").open("w") as stream:
                subprocess.run(command + ["--wav", str(reference)], cwd=ROOT, env=env,
                               stdout=stream, stderr=subprocess.STDOUT, check=True)
        for dsp in range(4):
            log = out / f"{name}-dsp{dsp}.txt"
            wav = out / f"{name}-dsp{dsp}.wav"
            if not args.summary_only:
                env["G1_CYCPROF"] = str(dsp)
                print(f"Profiling {name}, DSP {dsp}: {log}", flush=True)
                with log.open("w") as stream:
                    subprocess.run(command + ["--wav", str(wav)],
                                   cwd=ROOT, env=env, stdout=stream, stderr=subprocess.STDOUT, check=True)
            if not filecmp.cmp(reference, wav, shallow=False):
                raise RuntimeError(f"profiling changed the WAV: {name}/DSP{dsp}")
            profiles.append((name, dsp, log.read_text()))

    # Resolve the core's stable numeric form IDs without duplicating its enum in G1.
    header = (ROOT / "build/g1-dsp/dsp56kEmu/opcodetypes.h").read_text(errors="replace")
    body = re.search(r"enum Instruction\s*\{(.*?)\};", header, re.S).group(1)
    names = {}
    value = 0
    for field in body.split(","):
        field = field.strip()
        if not field:
            continue
        if "=" in field:
            field, assigned = field.split("=")
            field, value = field.strip(), int(assigned)
        names[value] = field
        value += 1
    forms = {}
    opcodes = (ROOT / "build/g1-dsp/dsp56kEmu/opcodeinfo.h").read_text(errors="replace")
    patterns = dict(re.findall(r'OpcodeInfo\((\w+),\s*"([^"\n]{24})"', opcodes))
    modes = {}

    def field(pattern, letter, op):
        bits = "".join(str((op >> (23 - i)) & 1) for i, char in enumerate(pattern) if char == letter)
        return int(bits, 2) if bits else None

    print("\nPatch | DSP | Cycles | Instructions (REP expanded) | Cycles/instruction | Unaccounted | Errors")
    for name, dsp, data in profiles:
        summary = re.search(r"^CYCPROF dsp=.*$", data, re.M)
        if not summary:
            raise RuntimeError(f"missing profile: {name}/DSP{dsp}")
        fields = dict(re.findall(r"(\w+)=([^ ]+)", summary.group()))
        print(" | ".join([name, str(dsp)] + [fields[k] for k in
              ("cycles", "instructions", "cycles_per_instruction", "unaccounted_cycles", "errors")]))
        if int(fields["errors"]) or int(fields["unaccounted_cycles"]):
            raise RuntimeError(f"invalid accounting: {name}/DSP{dsp}")
        for op, a, b in re.findall(r"^CYCPROF_PC .* op=([0-9a-f]+) .* form_a=(\d+) form_b=(\d+) \|", data, re.M):
            for number in (int(a), int(b)):
                if number not in names:
                    continue
                form = names[number]
                pattern = patterns[form]
                opcode = int(op, 16)
                m = field(pattern, "M", opcode)
                mode = "-"
                if pattern.count("M") == 3:
                    mode = ["post-N", "post+N", "post-1", "post+1", "Rn", "Rn+N", "extension", "pre-1"][m]
                    if m == 6:
                        r = field(pattern, "R", opcode)
                        mode = "immediate" if r == 4 else "absolute" if r == 0 else f"reserved-RRR={r}"
                elif m is not None:
                    mode = f"MM={m}"
                elif form in {"Movex_aa", "Movey_aa"}:
                    mode = "read" if field(pattern, "W", opcode) else "write"
                modes.setdefault(form, set()).add(mode)
        for form, count, cycles, table, assembly in re.findall(
                r"^CYCPROF_FORM dsp=\d+ form=(\d+) count=(\d+) inclusive_cycles=(\d+) table=([\d,]+) \| (.*)$", data, re.M):
            key = names[int(form)]
            row = forms.setdefault(key, [0, table, assembly])
            if row[1] != table:
                raise RuntimeError(f"mixed core versions in logs: {key}")
            row[0] += int(count)
    print(f"\nExecuted instruction forms: {len(forms)} (parallel halves are inclusive)")
    print("Form | Count | Core T,pru,lab,lim | Observed modes | Assembly template")
    for name, row in sorted(forms.items()):
        print(f"{name} | {row[0]} | {row[1]} | {','.join(sorted(modes.get(name, [])))} | {row[2]}")
    print(f"\n{len(profiles)} profiles: all cycles accounted for; all WAVs byte-identical to unprofiled G1_JITBLOCK=1.")


if __name__ == "__main__":
    main()
