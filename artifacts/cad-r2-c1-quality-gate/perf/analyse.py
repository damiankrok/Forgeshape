#!/usr/bin/env python3
"""Per-metric A/B statistics from a run_suite_abba.sh TSV or a bench_c1 TSV pair.

suite mode: analyse.py suite <abba.tsv> metric [metric...]
bench mode: analyse.py bench <A.tsv>,<A2.tsv>... -- <B.tsv>,... column [column...]
Prints n, median, p10, p90, IQR per side, absolute and relative median delta, and a
Mann-Whitney U two-sided p (normal approximation) so a gap can be told from noise.
"""
import math, re, statistics, sys

def q(xs, p):
    xs = sorted(xs); k = (len(xs) - 1) * p; f = math.floor(k); c = math.ceil(k)
    return xs[f] if f == c else xs[f] + (xs[c] - xs[f]) * (k - f)

def mwu(a, b):
    allv = sorted([(v, 0) for v in a] + [(v, 1) for v in b])
    ranks = {}; i = 0
    while i < len(allv):
        j = i
        while j + 1 < len(allv) and allv[j + 1][0] == allv[i][0]: j += 1
        for k in range(i, j + 1): ranks.setdefault(k, (i + j) / 2 + 1)
        i = j + 1
    ra = sum(ranks[k] for k, (v, g) in enumerate(allv) if g == 0)
    n1, n2 = len(a), len(b); u = ra - n1 * (n1 + 1) / 2
    mu = n1 * n2 / 2; sd = math.sqrt(n1 * n2 * (n1 + n2 + 1) / 12)
    z = (u - mu) / sd if sd else 0.0
    return 2 * (1 - 0.5 * (1 + math.erf(abs(z) / math.sqrt(2))))

def row(name, a, b):
    ma, mb = statistics.median(a), statistics.median(b)
    return (f"| {name} | {len(a)} | {ma:.0f} | {q(a,.1):.0f}..{q(a,.9):.0f} | {q(a,.75)-q(a,.25):.0f} "
            f"| {len(b)} | {mb:.0f} | {q(b,.1):.0f}..{q(b,.9):.0f} | {q(b,.75)-q(b,.25):.0f} "
            f"| {mb-ma:+.0f} | {100*(mb-ma)/ma:+.1f}% | {mwu(a,b):.3g} |")

HEAD = ("| metric | nA | median A | p10..p90 A | IQR A | nB | median B | p10..p90 B | IQR B "
        "| Δ median | Δ % | MWU p |\n|---|---|---|---|---|---|---|---|---|---|---|---|")

if sys.argv[1] == "suite":
    rows = [l.rstrip("\n").split("\t") for l in open(sys.argv[2])][1:]
    print(HEAD)
    for m in sys.argv[3:]:
        a, b = [], []
        for r in rows:
            v = re.search(r"\b%s=([0-9.]+)" % re.escape(m), r[4])
            (a if r[2] == "A" else b).append(float(v.group(1)))
        print(row(m, a, b))
else:
    sep = sys.argv.index("--")
    fa = sys.argv[2].split(","); fb = sys.argv[sep + 1].split(","); cols = sys.argv[sep + 2:]
    def load(files):
        out = {}
        for f in files:
            lines = [l.rstrip("\n").split("\t") for l in open(f)]
            hdr = lines[0]
            for l in lines[1:]:
                for i, h in enumerate(hdr): out.setdefault(h, []).append(float(l[i]))
        return out
    A, B = load(fa), load(fb)
    print(HEAD)
    for c in cols: print(row(c, A[c], B[c]))
