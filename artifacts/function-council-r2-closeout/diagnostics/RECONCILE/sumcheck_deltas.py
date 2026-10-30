#!/usr/bin/env python3
"""Sum-check REQUIREMENT_DELTAS.tsv. Usage: python3 -I sumcheck_deltas.py <REQUIREMENT_DELTAS.tsv>"""
import collections, sys
rows = [l.rstrip("\n").split("\t") for l in open(sys.argv[1], encoding="utf-8")]
h, d = rows[0], rows[1:]
print("HEADER", h)
print("DATA_ROWS", len(d), "BAD_COLS", sum(1 for r in d if len(r) != 8), "UNIQUE_IDS", len({r[0] for r in d}))
print("EXISTING_ROW_DELTAS", sum(1 for r in d if r[0].startswith("DLT-E")),
      "HISTORICAL_NONEXACT_DELTAS", sum(1 for r in d if r[0].startswith("DLT-N")))
def cls(p):
    for k in ["(new row)", "extend scope", "add owner_decision", "UNKNOWN_CURRENT_PLAN",
              "UNRESOLVED_AUTHORITY (no horizon", "SUPERSEDED by", "HISTORICAL ("]:
        if k in p: return k
    return "existing-row change"
c = collections.Counter(cls(r[3]) for r in d)
print("PROPOSED_CLASSES", dict(c), "SUM", sum(c.values()))
print("CONFIDENCE", dict(collections.Counter(r[6] for r in d)))
print("REVIEWER", dict(collections.Counter(r[7] for r in d)))
print("NEW_ROW_PROPOSALS_(new row)", sum(1 for r in d if "(new row)" in r[3]),
      "PLUS_EXISTING_ROW_DELTAS_PROPOSING_A_NEW_ROW", sum(1 for r in d if r[0].startswith("DLT-E") and "new row proposed" in r[3]))
