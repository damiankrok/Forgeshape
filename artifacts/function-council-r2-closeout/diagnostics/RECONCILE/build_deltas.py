#!/usr/bin/env python3
"""Build REQUIREMENT_DELTAS.tsv and print the plan-coverage counts.

Inputs (read-only): the three closeout extracts under work/ and the 681-row
FC-R2 matrix. Output: <closeout>/REQUIREMENT_DELTAS.tsv. Every count printed
here is recomputed from the files; nothing is typed in by hand except the
existing-row deltas in EXISTING_ROW_DELTAS, whose basis is cited per row.

Usage: python3 -I build_deltas.py <closeout dir> <function-council-r2 dir>
"""
import collections
import re
import sys

closeout, fc2 = sys.argv[1], sys.argv[2]
COLS = ["src_id", "source_file", "section", "date", "doc_version", "quote", "kind", "claimed",
        "matches", "mq", "superseded_by", "authority", "evidence", "notes"]
AUTH = ["CURRENT_REPO_CONFIRMED", "SUPERSEDED", "HISTORICAL", "OWNER_DECISION_REQUIRED",
        "UNRESOLVED_AUTHORITY", "UNKNOWN_CURRENT_PLAN"]
KINDS = ["requirement", "decision", "gate", "non_goal", "deferral", "status_claim"]
NORMATIVE = set(KINDS) - {"status_claim"}


def read_tsv(path):
    with open(path, encoding="utf-8") as f:
        lines = f.read().rstrip("\n").split("\n")
    return lines[0].split("\t"), [l.split("\t") for l in lines[1:]]


extracts = []
for s in ["SRC-410", "SRC-ADD", "SRC-4058"]:
    h, rows = read_tsv(f"{closeout}/work/extract_{s}.tsv")
    assert len(h) == 14, (s, len(h))
    for r in rows:
        assert len(r) == 14, (s, r[0], len(r))
        d = dict(zip(COLS, r))
        d["extract"] = s
        extracts.append(d)
mh, mrows = read_tsv(f"{fc2}/requirements_traceability.tsv")
matrix = {r[mh.index("ID")]: dict(zip(mh, r)) for r in mrows}
assert len(matrix) == 681

ids = [e["src_id"] for e in extracts]
assert len(ids) == len(set(ids)), "duplicate src_id"
for e in extracts:
    assert e["kind"] in KINDS, e["src_id"]
    assert e["authority"] in AUTH, e["src_id"]
    assert e["mq"] in ("exact", "partial", "none"), e["src_id"]
    for m in re.split(r"[;,]", e["matches"]):
        m = m.strip()
        if m and m != "-":
            assert m in matrix, (e["src_id"], m)


def matched_ids(e):
    return [m.strip() for m in re.split(r"[;,]", e["matches"]) if m.strip() and m.strip() != "-"]


def confidence(e):
    # E2 when the extract cites a current repo record by file:line, else E4 (doc only).
    return "E2" if re.search(r"[A-Za-z_]+\.(md|cpp|h|java|kt|sh|ps1|xml)(\s+v[\d.]+)?:\d", e["evidence"]) else "E4"


def proposal(e):
    a, k, mq = e["authority"], e["kind"], e["mq"]
    ids_ = matched_ids(e)
    if a == "CURRENT_REPO_CONFIRMED":
        if mq == "none":
            hz = "H2" if k in ("non_goal", "deferral") else "H1"
            return f"CURRENT_REPO_CONFIRMED/{hz} (new row)", "new_row"
        hz = sorted({matrix[i]["horizon"] for i in ids_})
        return (f"CURRENT_REPO_CONFIRMED/{'+'.join(hz)} (extend scope of {';'.join(ids_)}; no new row)",
                "extend")
    if a == "OWNER_DECISION_REQUIRED":
        if mq == "none":
            return "OWNER_DECISION_REQUIRED/H2 BLOCKED-OWNER (new row)", "new_row_owner"
        return f"OWNER_DECISION_REQUIRED (add owner_decision on {';'.join(ids_)}; no new row)", "owner_note"
    if a == "UNRESOLVED_AUTHORITY":
        return "UNRESOLVED_AUTHORITY (no horizon assigned; plan-only or unconfirmed)", "unresolved"
    if a == "UNKNOWN_CURRENT_PLAN":
        return "UNRESOLVED_AUTHORITY (UNKNOWN_CURRENT_PLAN: standing depends on absent v4.1.61)", "unresolved"
    if a == "SUPERSEDED":
        sb = e["superseded_by"] if e["superseded_by"] not in ("", "-") else "a later record"
        return f"SUPERSEDED by {sb} (no row)", "no_row"
    return "HISTORICAL (closed or stage-internal; no row)", "no_row"


# Existing-row deltas: each is a proposed change to the horizon, verdict,
# authority or owner_decision of a row of the 681-row matrix. The verdict of
# an existing row is never raised here; nothing is promoted to H0.
EXISTING_ROW_DELTAS = [
    ("APP-078", "H2 OUT-OF-SCOPE (Recent files, templates, accounts; source HomeView javadoc + PRODUCT.md:133-138)",
     "H2 FUTURE-NOT-DUE / authority UNRESOLVED_AUTHORITY for the account/cloud half (a DEFERRED plan stage, not an exclusion); sample projects split to their own UNRESOLVED_AUTHORITY item; recent files/templates stay OUT-OF-SCOPE on repo evidence",
     "SRC-410-061;SRC-410-123;SRC-4058-265;SRC-4058-217",
     "Historical masters defer CLOUD-R0 / account UX (v4.1.0 §2.4; v4.0.58 cloud rows) and plan 2-3 sample .forge files for E2E-R1; OUT-OF-SCOPE asserts an exclusion no plan source states. app/src/main/assets holds only licenses/ (extract SRC-4058-217).",
     "E4"),
    ("APP-079", "H2 UNDETERMINED-SOURCE-GAP E5 (onboarding: whether the Master requires it)",
     "Historical 'first run / onboarding / empty state' item: CURRENT_REPO_CONFIRMED/H1, satisfied by the APP-H1 'no project' Home (covered by UI-001, APP-003); residual 'onboarding beyond start-page captions' stays H2 UNDETERMINED-SOURCE-GAP with authority UNRESOLVED_AUTHORITY",
     "SRC-4058-216",
     "v4.0.58 coverage table (FORGESHAPE_MASTER_PLAN_v4.0.58.md:814) maps first-run/onboarding/empty state to the UI-SPEC-R0 'no project' frame implemented in E2E-R1/APP-H1, so part of the gap has a historical answer; whether v4.1.61 asks for more is still unknown.",
     "E4"),
    ("APP-052", "H0 PASS-SOURCE-AND-RECORDED-RUNTIME; owner_decision '-'",
     "verdict unchanged; owner_decision := 'OBJ export: historical approved MVP (v4.1.0 GLB/OBJ export, Stage 034) vs CLAUDE.md OBJ absent in both directions' -> OWNER_DECISION_REQUIRED",
     "SRC-410-057;SRC-410-134;SRC-4058-367",
     "The repo rule is the current basis for the row's PASS; the conflict with the historical MVP is an OWNER semantics question, not a defect.",
     "E2"),
    ("UI-086", "H0 PARTIAL; owner_decision '-'",
     "verdict unchanged; owner_decision := same OBJ question as APP-052 (OWNER_DECISION_REQUIRED)",
     "SRC-410-134;SRC-4058-367",
     "Same root as APP-052: the 'no OBJ entry' half of the row rests on a repo rule that a historical MVP contradicts.",
     "E2"),
    ("MOD-017", "H2 FUTURE-NOT-DUE; authority implied by CLAUDE.md SCULPT-FCM-R1 'Not this stage'",
     "H2 FUTURE-NOT-DUE kept; authority UNRESOLVED_AUTHORITY for Sculpt Stroke X Symmetry and Subdivide/remesh (historical approved-MVP items; not promoted to H0 without v4.1.61/OQ-B)",
     "SRC-410-131;SRC-410-132;SRC-410-044;SRC-4058-149;SRC-4058-150;SRC-4058-151",
     "v4.1.0 line 189 lists 'symmetry/pressure, subdivide/remesh' in the approved MVP; PROJECT_STATUS.md:4895-4900 says do not start Stage 026; OQ-B (full MVP vs tiering) is open. Required-and-not-done by the historical plan, no current basis.",
     "E4"),
    ("MOD-007", "H2 FUTURE-NOT-DUE (pressure)",
     "H2 FUTURE-NOT-DUE kept; authority UNRESOLVED_AUTHORITY (historical MVP 'stylus pressure if available'; blocked behind STYLUS-G1 = MOD-115)",
     "SRC-410-131;SRC-4058-148;SRC-410-196",
     "Same root as MOD-017 for pressure.",
     "E4"),
    ("DOC-085", "H2 FUTURE-NOT-DUE (incl. materials/UV in GLB)",
     "H2 FUTURE-NOT-DUE kept for persistence items; the UV half re-labelled authority UNRESOLVED_AUTHORITY (historical approved-MVP 'UV MVP', Stages 029-032)",
     "SRC-410-133;SRC-410-055;SRC-4058-111",
     "No current repo record schedules or cancels the UV MVP; export carries no UVs.",
     "E4"),
    ("MOD-035", "H0 PASS-REPRODUCED; owner_decision '-'",
     "verdict unchanged; owner_decision := 'one-way Create Sculpt Copy / BRIDGE-R1 (UI-OWNER-24, plan-only) vs the shipped reversible Start Sculpting / Back to Construction' -> OWNER_DECISION_REQUIRED",
     "SRC-4058-110;SRC-4058-144;SRC-4058-302;SRC-4058-346",
     "v4.0.58 plans a one-way copy that removes Freeze/Resume; CLAUDE.md:968-979 ships the opposite; no current record schedules or cancels BRIDGE-R1.",
     "E2"),
    ("CAD-083", "H1 PASS-REPRODUCED; owner_decision '-'",
     "verdict unchanged; owner_decision := 'v4.1.58 §B: partial-Revolve planar caps become sketch supports IF they have stable refs; repo makes every Revolve face ineligible' -> OWNER confirmation",
     "SRC-410-390",
     "Whether the v4.1.58 condition held is not recorded; the repo choice is current basis but its authority trace is missing.",
     "E2"),
    ("CAD-109", "H2 FUTURE-NOT-DUE; source 'PROJECT_STATUS Next Stage Do not start' (Stage024 / CAD-A4)",
     "H2 FUTURE-NOT-DUE kept; source refreshed: Stage 024/CAD-A4 delivered except Intersect and Revolve Add/Cut; residual authority UNRESOLVED_AUTHORITY (no plan home)",
     "SRC-410-046;SRC-410-047;SRC-4058-316;SRC-4058-358",
     "PROJECT_STATUS.md:4899 still says do not start Stage024/CAD-A4 though Add/Cut and Revolve New Body shipped (stale description).",
     "E2"),
    ("RND-019", "H2 BLOCKED-OWNER; source FC-R1 I-5 '(no recorded budget)'",
     "H2 BLOCKED-OWNER kept; source += historical GATE-PERF-SCULPT (numeric vertex count + max stroke latency on reference device); authority OWNER_DECISION_REQUIRED",
     "SRC-4058-384;SRC-4058-306;SRC-4058-449",
     "The historical plan names the gate the matrix calls 'no recorded budget'.",
     "E4"),
    ("APP-064", "H1 PASS-REPRODUCED; owner_decision '-'",
     "verdict unchanged; owner_decision := 'UI-PREF-R1 aesthetic OWNER verdict pending (PROJECT_STATUS.md:2533-2541 none aesthetically approved; v4.1.0 says OWNER accepted under aggregate waiver)'",
     "SRC-410-016;SRC-410-230",
     "Two records disagree on acceptance; the repo record is newer and says not approved. APP-066 already carries this note; APP-064 does not.",
     "E2"),
    ("CON-033", "H1 PASS-REPRODUCED; owner_decision '-' (Stage 018A group: CON-026, CON-028, CON-035)",
     "verdict unchanged; owner_decision := 'Stage 018A OWNER retest READY, no verdict recorded' (OWNER-PHYSICAL-UNVERIFIED)",
     "SRC-4058-066",
     "PROJECT_STATUS.md:1068-1070 'COMPLETE, OWNER RETEST READY'; :5017-5020 lists the retest as still open.",
     "E2"),
    ("CON-054", "H1 PASS-REPRODUCED; owner_decision '-' (Stage 020M group: CON-055, CON-057)",
     "verdict unchanged; owner_decision := 'Stage 020M dimensions/anchors/Relative Scale OWNER UX verdict not recorded'",
     "SRC-4058-067",
     "v4.0.58 lists it among OWNER LATER visual reviews; no current record of a verdict.",
     "E4"),
    ("CON-048", "H1 PASS-REPRODUCED; owner_decision '-' (MIRROR-01 group: CON-052)",
     "verdict unchanged; owner_decision := 'MIRROR-01 plane chooser / two-line strip / Undo feel OWNER verdict not recorded'",
     "SRC-4058-068",
     "v4.0.58 lists it among OWNER LATER visual reviews; no current record of a verdict.",
     "E4"),
    ("CAD-113", "H1 BLOCKED-OWNER; owner_decision 'C1/C2 OWNER test packs pending'",
     "H1 BLOCKED-OWNER kept; owner_decision += 'OD-27 C2 exact-APK checklist (8 items, PASS/FAIL/NOT TESTED); OWNER APK SHA 262076... expires 2026-11-11T23:31:32Z; MODELING-R1 correction physical re-review also pending'",
     "SRC-ADD-204;SRC-ADD-339;SRC-410-406",
     "The expiry is a real deadline for the OWNER gate (PROJECT_STATUS.md v0.103.0:44-47; :4871-4880).",
     "E2"),
    ("APP-022", "H1 PASS-SOURCE-AND-RECORDED-RUNTIME; owner_decision '-'",
     "verdict unchanged; owner_decision := 'UI-OWNER-25: durable import should converge on Objects + -> Add from file; current entry is the project drawer'",
     "SRC-4058-195;SRC-410-215",
     "Plan text and shipped entry point differ; the decision body (UI-OWNER-25) is not in the archive.",
     "E4"),
    ("TST-038", "H0 PASS-SOURCE-AND-RECORDED-RUNTIME (FullSharded); matched 'exact' by extract SRC-410-280",
     "row unchanged; the extract match for TEST-OWNER-02 budgets is partial, not exact; new row proposed: 'TEST-OWNER-02 budget' with OWNER_DECISION_REQUIRED (CLAUDE.md 90/120 aggregate vs PROJECT_STATUS.md:2543-2550 30/45 closeout under the same decision id)",
     "SRC-410-259;SRC-410-280",
     "Repo-internal contract conflict under one decision id; no matrix row tracks the budget itself.",
     "E2"),
]

out = [["id", "existing_row_id", "old", "proposed", "source", "reason", "confidence", "reviewer"]]
n = 0
for rid, old, new, src, why, conf in EXISTING_ROW_DELTAS:
    assert rid in matrix, rid
    n += 1
    out.append([f"DLT-E{n:03d}", rid, old, new, src, why, conf, "closeout-reconcile"])
n_existing = n

cat = collections.Counter()
per_src = collections.defaultdict(collections.Counter)
new_row_quotes = collections.defaultdict(list)
n = 0
for e in extracts:
    if e["mq"] == "exact":
        cat["exact_match_no_delta"] += 1
        continue
    if e["kind"] == "status_claim":
        cat["status_claim_nonexact_no_requirement_delta"] += 1
        continue
    prop, klass = proposal(e)
    cat["delta_" + klass] += 1
    per_src[e["extract"]][klass] += 1
    if klass in ("new_row", "new_row_owner"):
        key = re.sub(r"[^a-z0-9]+", " ", e["quote"].lower()).strip()
        new_row_quotes[key].append(e["src_id"])
    n += 1
    reason = (f"{e['kind']} from {e['source_file']} {e['section']} ({e['date']}); match={e['mq']}"
              f"{' to ' + e['matches'] if e['matches'] not in ('', '-') else ''}; extract authority={e['authority']}"
              f"{'; repo: ' + e['evidence'] if e['evidence'] not in ('', '-') else ''}")
    reason = reason.replace("\t", " ")
    old = "-" if e["mq"] == "none" else f"partial match {e['matches']}"
    existing = "-" if e["mq"] == "none" else e["matches"]
    out.append([f"DLT-N{n:04d}", existing, old, prop, e["src_id"], reason, confidence(e), "closeout-reconcile"])
n_new = n

with open(f"{closeout}/REQUIREMENT_DELTAS.tsv", "w", encoding="utf-8") as f:
    for r in out:
        assert len(r) == 8 and all("\t" not in c and "\n" not in c for c in r)
        f.write("\t".join(r) + "\n")

print("EXTRACT_ROWS", len(extracts), "UNIQUE_SRC_IDS", len(set(ids)))
uq = collections.Counter(re.sub(r"[^a-z0-9]+", " ", e["quote"].lower()).strip() for e in extracts)
print("UNIQUE_NORMALISED_QUOTES", len(uq), "QUOTES_REPEATED_ACROSS_ROWS", sum(1 for v in uq.values() if v > 1))
print("PER_FILE", dict(collections.Counter(e["source_file"] for e in extracts)))
for s in ["SRC-410", "SRC-ADD", "SRC-4058"]:
    sub = [e for e in extracts if e["extract"] == s]
    print("PER_EXTRACT", s, len(sub), "MQ", dict(collections.Counter(e["mq"] for e in sub)),
          "AUTH", {a: sum(1 for e in sub if e["authority"] == a) for a in AUTH})
print("TOTAL_MQ", dict(collections.Counter(e["mq"] for e in extracts)))
print("TOTAL_AUTH", {a: sum(1 for e in extracts if e["authority"] == a) for a in AUTH})
print("TOTAL_KIND", {k: sum(1 for e in extracts if e["kind"] == k) for k in KINDS})
print("AUTH_X_MQ")
for a in AUTH:
    print("  ", a, {m: sum(1 for e in extracts if e["authority"] == a and e["mq"] == m) for m in ("exact", "partial", "none")})
print("CATEGORIES", dict(cat), "SUM", sum(cat.values()))
for s, c in per_src.items():
    print("DELTA_CLASS", s, dict(c))
print("DELTA_ROWS_EXISTING", n_existing, "DELTA_ROWS_HISTORICAL_NONEXACT", n_new, "TSV_DATA_ROWS", len(out) - 1)
nr = sum(cat[k] for k in ("delta_new_row", "delta_new_row_owner"))
print("PROPOSED_NEW_ROWS_STATEMENTS", nr, "DISTINCT_BY_NORMALISED_QUOTE", len(new_row_quotes))
assert sum(cat.values()) == len(extracts)
assert cat["exact_match_no_delta"] + sum(v for k, v in cat.items() if k.startswith("delta_")) \
    + cat["status_claim_nonexact_no_requirement_delta"] == len(extracts)
print("SUM_CHECK_OK")
