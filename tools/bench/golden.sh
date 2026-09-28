#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
patchtest=${G1_PATCHTEST:-$root/build/tools/patchtest/g1patchtest_artefacts/Release/g1patchtest}
rom=${G1_ROM:-$root/Roms/NORD-MODULAR-RACK-VER-3.03.BIN}
seconds=${G1_BENCH_SECONDS:-3}
list=${G1_BENCH_PATCHES:-$root/tools/bench/patches.txt}
golden=${G1_BENCH_GOLDEN:-${HOME}/.local/share/Animatek/G1-Emu/bench-golden}
mode=${1:-check}
mkdir -p "$golden"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

while IFS='|' read -r name patch note extra; do
	[[ -z "${name// }" || ${name:0:1} == '#' ]] && continue
	[[ "$patch" = /* ]] || patch="$root/$patch"
	threaded="$tmp/$name-threaded.wav"
	serial="$tmp/$name-serial.wav"
	if [[ -n "$extra" ]]; then
		env "$extra" "$patchtest" "$rom" "$patch" --note "$note" --seconds "$seconds" --bench --wav "$threaded" >/dev/null 2>&1
		env "$extra" G1_THREADS=0 "$patchtest" "$rom" "$patch" --note "$note" --seconds "$seconds" --bench --wav "$serial" >/dev/null 2>&1
	else
		"$patchtest" "$rom" "$patch" --note "$note" --seconds "$seconds" --bench --wav "$threaded" >/dev/null 2>&1
		G1_THREADS=0 "$patchtest" "$rom" "$patch" --note "$note" --seconds "$seconds" --bench --wav "$serial" >/dev/null 2>&1
	fi
	if ! cmp -s "$threaded" "$serial"; then
		echo "$name: threaded and G1_THREADS=0 WAVs differ" >&2
		exit 1
	fi
	ref="$golden/$name.wav"
	if [[ "$mode" == record ]]; then
		cp "$threaded" "$ref"
		echo "$name: recorded $ref"
	elif [[ "$mode" == check ]]; then
		[[ -f "$ref" ]] || { echo "$name: missing $ref (run golden.sh record)" >&2; exit 1; }
		cmp -s "$threaded" "$ref" || { echo "$name: WAV differs from $ref" >&2; exit 1; }
		echo "$name: byte-identical (threaded, serial, reference)"
	else
		echo "usage: $0 [record|check]" >&2
		exit 2
	fi
done < "$list"
