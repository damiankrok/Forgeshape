#!/usr/bin/env bash
# Suite-level A/B: the product's OWN metric definitions (FORGESHAPE_CAD_FEATURE_PERFORMANCE),
# two host binaries built with identical flags by scripts/host-native-selftests.sh, run
# alternately in ABBA blocks so drift lands on both sides equally.
# Usage: run_suite_abba.sh <binA> <binB> <blocks> <out.tsv>
set -euo pipefail
A="$1"; B="$2"; BLOCKS="$3"; OUT="$4"
echo -e "block\tslot\tbinary\tload1\tmetrics" > "$OUT"
run() {  # $1 label $2 binary $3 block $4 slot
    local load; load=$(cut -d' ' -f1 /proc/loadavg)
    local line; line=$("$2" CAD_FEATURE | grep '^FORGESHAPE_CAD_FEATURE_PERFORMANCE' | cut -d' ' -f2-)
    echo -e "$3\t$4\t$1\t$load\t$line" >> "$OUT"
}
# Warm-up: one untimed run of each (page cache, CPU frequency).
"$A" CAD_FEATURE >/dev/null; "$B" CAD_FEATURE >/dev/null
for ((b = 1; b <= BLOCKS; b++)); do
    if (( b % 2 )); then
        run A "$A" "$b" 1; run B "$B" "$b" 2; run B "$B" "$b" 3; run A "$A" "$b" 4
    else
        run B "$B" "$b" 1; run A "$A" "$b" 2; run A "$A" "$b" 3; run B "$B" "$b" 4
    fi
done
