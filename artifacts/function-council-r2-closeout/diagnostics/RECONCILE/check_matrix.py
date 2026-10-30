#!/usr/bin/env python3
"""Closeout check of the 681-row FC-R2 traceability matrix.

Reads the TSV and the totals table in REQUIREMENTS_TRACEABILITY.md (both
read-only inputs), checks ID uniqueness and column counts, recomputes
domain x horizon x verdict, and diffs every cell against the published table.
Usage: python3 -I check_matrix.py <function-council-r2 dir>
"""
import collections
import re
import sys

base = sys.argv[1]
tsv = base + "/requirements_traceability.tsv"
md = base + "/REQUIREMENTS_TRACEABILITY.md"

ABBR = ["PR", "PSR", "IU", "PART", "DEF", "REG", "INA", "DOC", "NIBD", "FND", "BO", "OOS", "USG"]
FULL = ["PASS-REPRODUCED", "PASS-SOURCE-AND-RECORDED-RUNTIME", "IMPLEMENTED-UNTESTED",
        "PARTIAL", "DEFECT-CONFIRMED", "REGRESSION-CONFIRMED", "INACCESSIBLE",
        "DOC-DIVERGENCE", "NOT-IMPLEMENTED-BUT-DUE", "FUTURE-NOT-DUE", "BLOCKED-OWNER",
        "OUT-OF-SCOPE", "UNDETERMINED-SOURCE-GAP"]

with open(tsv, encoding="utf-8") as f:
    lines = f.read().split("\n")
if lines and lines[-1] == "":
    lines.pop()
header = lines[0].split("\t")
rows = [l.split("\t") for l in lines[1:]]
print("HEADER_COLUMNS", len(header))
print("DATA_ROWS", len(rows))
bad_cols = [(i + 2, len(r)) for i, r in enumerate(rows) if len(r) != len(header)]
print("ROWS_WRONG_COLUMN_COUNT", len(bad_cols), bad_cols[:5])
ci = {h: i for i, h in enumerate(header)}
ids = [r[ci["ID"]] for r in rows]
dups = [k for k, v in collections.Counter(ids).items() if v > 1]
print("UNIQUE_IDS", len(set(ids)), "DUPLICATES", dups)
badv = [(r[ci["ID"]], r[ci["verdict"]]) for r in rows if r[ci["verdict"]] not in FULL]
print("VERDICTS_OUTSIDE_VOCAB", badv)
print("HORIZONS", dict(collections.Counter(r[ci["horizon"]] for r in rows)))

calc = collections.Counter((r[ci["domain"]], r[ci["horizon"]], r[ci["verdict"]]) for r in rows)

# Parse the published table.
pub = {}
pub_tot = {}
with open(md, encoding="utf-8") as f:
    in_tab = False
    for l in f:
        if l.startswith("| Domain | Horizon |"):
            in_tab = True
            continue
        if in_tab:
            if not l.startswith("|"):
                break
            cells = [c.strip() for c in l.strip().strip("|").split("|")]
            if set(cells[0]) <= set("-"):
                continue
            dom = cells[0].replace("**", "")
            hor = cells[1].replace("**", "")
            vals = [0 if c == "." else int(c.replace("**", "")) for c in cells[2:15]]
            tot = int(cells[15].replace("**", ""))
            pub[(dom, hor)] = dict(zip(FULL, vals))
            pub_tot[(dom, hor)] = tot

mism = []
# domain rows
for (dom, hor), cells in pub.items():
    for v, n in cells.items():
        if dom == "All":
            if hor == "all":
                got = sum(c for (d, h, vv), c in calc.items() if vv == v)
            else:
                got = sum(c for (d, h, vv), c in calc.items() if h == hor and vv == v)
        else:
            got = calc.get((dom, hor, v), 0)
        if got != n:
            mism.append((dom, hor, v, "published", n, "recomputed", got))
    rowsum = sum(cells.values())
    if rowsum != pub_tot[(dom, hor)]:
        mism.append((dom, hor, "ROW_TOTAL_ARITH", "published", pub_tot[(dom, hor)], "cells_sum", rowsum))
# cells present in data but absent from table
for (d, h, v), c in calc.items():
    if (d, h) not in pub:
        mism.append((d, h, v, "published", "absent", "recomputed", c))
print("PUBLISHED_TABLE_LINES", len(pub))
print("CELL_MISMATCHES", len(mism))
for m in mism:
    print("  MISMATCH", m)
print("RECOMPUTED_ALL", {a: sum(c for (d, h, vv), c in calc.items() if vv == v) for a, v in zip(ABBR, FULL)},
      "TOTAL", sum(calc.values()))
for hor in ["H0", "H1", "H2", "NA"]:
    print("RECOMPUTED", hor, sum(c for (d, h, v), c in calc.items() if h == hor))
