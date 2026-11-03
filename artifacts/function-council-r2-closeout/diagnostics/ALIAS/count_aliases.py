#!/usr/bin/env python3
"""Counts for work/alias_counts.md.  Usage: python3 -I count_aliases.py <repo_root>"""
import csv, re, sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(sys.argv[1])
FC2 = ROOT / "artifacts/function-council-r2"
rows = list(csv.DictReader(open(ROOT / "artifacts/function-council-r2-closeout/ALIAS_TO_ROOT_CAUSE.tsv"), delimiter="\t"))
by = {r["alias_id"]: r for r in rows}
parent = lambda rc: re.sub(r"[a-z]$", "", rc)          # RC-13a -> RC-13
rcs = lambda r: r["root_cause_id"].split(";")

print("== 1. alias rows by kind")
print(dict(Counter(r["alias_kind"] for r in rows)), "total", len(rows))

# Unit A: DEFECT-CONFIRMED requirement rows (from the dossier TSV itself)
tsv = list(csv.DictReader(open(FC2 / "requirements_traceability.tsv"), delimiter="\t"))
dc = [r["ID"] for r in tsv if r["verdict"] == "DEFECT-CONFIRMED"]
dc_rc = sorted({x for i in dc for x in rcs(by[i])})
print("\n== 2. unit A: DEFECT-CONFIRMED requirement rows")
print("rows", len(dc), "| seat", dict(Counter(r["seat"] for r in tsv if r["verdict"] == "DEFECT-CONFIRMED")))
print("distinct mechanisms", len(dc_rc), dc_rc)
print("distinct parent RCs", len({parent(x) for x in dc_rc}))
print("by category", dict(Counter(by[i]["category"] for i in dc)))

# Unit B: the 38 P0-P2 seat findings (VERIFICATION.md tables)
ver = (FC2 / "work/VERIFICATION.md").read_text()
panel = re.findall(r"^\| ([A-Z]-?[A-Z]?\d\d) \| [A-J] \| P[0-2] \|", ver.split("## Positive-claim")[0], re.M)
unver = re.findall(r"^\| ([A-Z]-\d\d) \| [A-J] \| P2", ver.split("## Not panel-verified")[1], re.M)
p02 = panel + unver
print("\n== 3. unit B: P0-P2 seat findings (VERIFICATION.md)")
print("panel", len(panel), "not panel-verified", len(unver), "total", len(p02))
dups = {"X-A01": "A-01", "E-01": "B-02", "C-02": "A-04", "D-02": "A-04", "E-03": "A-04", "F-02": "C-01", "E-04": "A-05"}
dedup = [i for i in panel if i not in dups]
print("panel de-duplicated by the dossier's own duplicate list:", len(dedup), dedup)
p02_rc = sorted({x for i in p02 for x in rcs(by[i])})
print("all 38 -> distinct mechanisms", len(p02_rc), p02_rc)
print("all 38 -> distinct parent RCs", len({parent(x) for x in p02_rc}))
print("23 panel roots -> distinct mechanisms", len({x for i in dedup for x in rcs(by[i])}))
print("by category (38)", dict(Counter(by[i]["category"] for i in p02)))

# Unit C: every seat finding of every severity
sf = [r for r in rows if r["alias_kind"] == "seat_finding"]
print("\n== 4. unit C: every seat finding")
print("rows", len(sf), "| by status", dict(Counter(r["status"] for r in sf)))
print("non-hypothesis findings", sum(r["status"] != "hypothesis" for r in sf))
print("P3/INFO/other severities (non-hypothesis, not in the 38):",
      sum(1 for r in sf if r["status"] != "hypothesis" and r["alias_id"] not in p02))

# Mechanism level (all aliases)
cat_of = defaultdict(set); stat_of = defaultdict(set)
for r in rows:
    for x in rcs(r):
        if r["alias_kind"] in ("seat_finding", "requirement_row"):
            cat_of[x].add(r["category"]); stat_of[x].add(r["status"])
print("\n== 5. mechanism level (seat findings + requirement rows)")
cat_of.pop("NONE", None); stat_of.pop("NONE", None)
print("distinct mechanism ids (excluding NONE)", len(cat_of))
live = lambda x: bool(stat_of[x] & {"current", "narrowed"})
for cat in ["BUG", "DESIGN_HOLD", "TEST_HARNESS_DEFECT", "DOC_DIVERGENCE", "PERFORMANCE_RISK", "FUTURE_OPPORTUNITY"]:
    live_ids = sorted(x for x in cat_of if cat in cat_of[x] and live(x))
    hyp_ids = sorted(x for x in cat_of if cat in cat_of[x] and not live(x) and stat_of[x] == {"hypothesis"})
    print(f"{cat:20s} live={len(live_ids):3d} hypothesis-only={len(hyp_ids):2d}  {live_ids}")

def bug_ids(statuses):
    return sorted({x for r in rows if r["alias_kind"] in ("seat_finding", "requirement_row")
                   and r["category"] == "BUG" and r["status"] in statuses for x in rcs(r)})
strict = bug_ids({"current"}); union = bug_ids({"current", "narrowed"})
print("\n== 6. unique CURRENT defect mechanisms (category BUG)")
print("status current only:", len(strict), strict)
print("status current or narrowed (HEADLINE):", len(union), union)
print("narrowed-only BUG mechanisms:", sorted(set(union) - set(strict)))
print("parent RCs of the headline set:", len({parent(x) for x in union}))
e1 = sorted({x for r in rows if r["alias_kind"] in ("seat_finding", "requirement_row") and r["category"] == "BUG"
             and r["status"] in ("current", "narrowed") and r["confidence"] == "E1" for x in rcs(r)})
print("of which reproduced in this closeout (some alias at E1):", len(e1), e1)
best = {}
order = ["E1", "E2", "E3", "E4", "E5", "E0"]
for r in rows:
    if r["alias_kind"] in ("seat_finding", "requirement_row") and r["category"] == "BUG" and r["status"] in ("current", "narrowed"):
        for x in rcs(r):
            c = r["confidence"]
            if x not in best or order.index(c) < order.index(best[x]): best[x] = c
print("best confidence per headline mechanism:", dict(Counter(best.values())), best)
