#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
patchtest=${G1_PATCHTEST:-$root/build/tools/patchtest/g1patchtest_artefacts/Release/g1patchtest}
rom=${G1_ROM:-$root/Roms/NORD-MODULAR-RACK-VER-3.03.BIN}
seconds=${G1_BENCH_SECONDS:-3}
list=${G1_BENCH_PATCHES:-$root/tools/bench/patches.txt}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

printf '%-18s %10s %18s %12s %8s\n' PATCH REALTIME BUSIEST_THREAD ITS_WAITING OVERRUNS
printf '%-18s %10s %18s %12s %8s\n' '------------------' '----------' '------------------' '------------' '--------'
failed=0
while IFS='|' read -r name patch note extra; do
	[[ -z "${name// }" || ${name:0:1} == '#' ]] && continue
	[[ "$patch" = /* ]] || patch="$root/$patch"
	log="$tmp/$name.log"
	if [[ -n "$extra" ]]; then
		env "$extra" G1_VERBOSE=1 "$patchtest" "$rom" "$patch" --note "$note" --seconds "$seconds" --bench >"$log" 2>&1 || failed=1
	else
		G1_VERBOSE=1 "$patchtest" "$rom" "$patch" --note "$note" --seconds "$seconds" --bench >"$log" 2>&1 || failed=1
	fi
	if ! grep -q '^BENCH emulated_s=' "$log"; then
		printf '%-18s %10s %18s %12s %8s\n' "$name" FAIL FAIL FAIL FAIL
		failed=1
		continue
	fi
	realtime=$(sed -n 's/.*realtime=\([^ ]*\).*/\1/p' "$log" | tail -1)
	read -r busiest waiting <<<"$(awk '
	/^BENCH thread=/ {
		split($2, a, "="); split($3, b, "="); split($4, c, "=");
		name=a[2];
		if(name == "cpu-dsp0") name="cpu";	# DSP 0 runs on the CPU thread: one thread, its time added up
		busy[name]+=b[2]; wait[name]+=c[2];
	}
	END {
		# The busiest thread (the one that waits least at the barrier) is what limits the speed.
		for(t in busy) { total=busy[t]+wait[t]; pct=total ? 100*wait[t]/total : 0; if(who == "" || pct < min) { min=pct; who=t } }
		printf "%s %.1f%%", who, min+0
	}' "$log")"
	overruns=$(awk -F'[ =]' '/^BENCH dsp=/ { if($7+0 > max) max=$7+0 } END { print max+0 }' "$log")
	printf '%-18s %10s %18s %12s %8s\n' "$name" "$realtime" "$busiest" "$waiting" "$overruns"
	if (( overruns > 0 )); then
		failed=1
	fi
done < "$list"

exit "$failed"
