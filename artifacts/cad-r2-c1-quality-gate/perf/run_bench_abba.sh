#!/usr/bin/env bash
# Isolated-harness A/B: bench_c1 built from the SAME source against each tree's own
# product objects, alternated in ABBA blocks; each process warms up then records samples.
# Usage: run_bench_abba.sh <binA> <binB> <blocks> <samples> <warmup> <outdir>
set -euo pipefail
A="$1"; B="$2"; BLOCKS="$3"; N="$4"; W="$5"; OUT="$6"; mkdir -p "$OUT"
one() { cut -d' ' -f1 /proc/loadavg >> "$OUT/$1.load"; "$2" "$N" "$W" > "$OUT/$1_$3.tsv"; }
for ((b = 1; b <= BLOCKS; b++)); do
    if (( b % 2 )); then one A "$A" "$b-1"; one B "$B" "$b-2"; one B "$B" "$b-3"; one A "$A" "$b-4"
    else one B "$B" "$b-1"; one A "$A" "$b-2"; one A "$A" "$b-3"; one B "$B" "$b-4"; fi
done
