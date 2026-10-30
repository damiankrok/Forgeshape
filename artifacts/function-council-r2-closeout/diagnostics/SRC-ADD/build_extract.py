#!/usr/bin/env python3
"""SRC-ADD extraction builder (FUNCTION-COUNCIL-R2-CLOSEOUT-R1).

Holds the hand-extracted atomic statements of the v4.1.62..66 addenda, history
addenda and PROJECT_STATE v4.1.66, verifies that every statement_quote is a
VERBATIM substring of its source file, checks every matches_existing_ids entry
against the previous dossier's requirements_traceability.tsv, and writes
work/extract_SRC-ADD.tsv. Run with: python3 -I build_extract.py <repo_root>
"""
import csv, sys, os, collections

ROOT = sys.argv[1] if len(sys.argv) > 1 else "/home/user/Forgeshape"
SRC = os.path.join(ROOT, "artifacts/function-council-r2-closeout/sources")
OUT = os.path.join(ROOT, "artifacts/function-council-r2-closeout/work/extract_SRC-ADD.tsv")
TRACE = os.path.join(ROOT, "artifacts/function-council-r2/requirements_traceability.tsv")

F = {
    "A62": ("FORGESHAPE_MASTER_PLAN_ADDENDUM_v4.1.62.md", "2026-10-28", "4.1.62"),
    "H62": ("FORGESHAPE_MASTER_PLAN_HISTORY_ADDENDUM_v4.1.62.md", "2026-10-28", "4.1.62"),
    "A63": ("FORGESHAPE_MASTER_PLAN_ADDENDUM_v4.1.63.md", "2026-10-28", "4.1.63"),
    "H63": ("FORGESHAPE_MASTER_PLAN_HISTORY_ADDENDUM_v4.1.63.md", "2026-10-28", "4.1.63"),
    "A64": ("FORGESHAPE_MASTER_PLAN_ADDENDUM_v4.1.64.md", "2026-10-28", "4.1.64"),
    "H64": ("FORGESHAPE_MASTER_PLAN_HISTORY_ADDENDUM_v4.1.64.md", "2026-10-28", "4.1.64"),
    "A65": ("FORGESHAPE_MASTER_PLAN_ADDENDUM_v4.1.65.md", "2026-10-29", "4.1.65"),
    "H65": ("FORGESHAPE_MASTER_PLAN_HISTORY_ADDENDUM_v4.1.65.md", "2026-10-29", "4.1.65"),
    "A66": ("FORGESHAPE_MASTER_PLAN_ADDENDUM_v4.1.66.md", "2026-10-29", "4.1.66"),
    "H66": ("FORGESHAPE_MASTER_PLAN_HISTORY_ADDENDUM_v4.1.66.md", "2026-10-29", "4.1.66"),
    "S66": ("FORGESHAPE_PROJECT_STATE_v4.1.66.md", "2026-10-29", "4.1.66"),
    "S65": ("FORGESHAPE_PROJECT_STATE_v4.1.66.md", "2026-10-29", "4.1.65 (ARCHIVED block inside STATE v4.1.66)"),
    "S64": ("FORGESHAPE_PROJECT_STATE_v4.1.66.md", "2026-10-28", "4.1.64 (ARCHIVED block inside STATE v4.1.66)"),
}

# Evidence shorthands (current repo records at this tree).
PS = "PROJECT_STATUS.md v0.103.0"
PS_HEAD = PS + ":1-30 (FC-R2 audit + CAD-R2-C2 status lines)"
PS_NEXT = PS + ":4868-4900 (Next Stage)"
PS_C1 = PS + ":47-75 (CAD-R2-C1 closeout)"
PS_R2 = PS + ":77-110 (CAD-R2 closeout)"
CL = "CLAUDE.md"
A2CUR = (CL + ":674-681 (Keep Sketch: a second ROOT/world-plane sketch stays refused, SketchSupportInvalid); "
         + PS + ":29-30,4878-4880 (A2 BLOCKED, option 0 is current behaviour, not a decision); "
         "app/src/main/cpp/forgeshape_cad_r2_selftest.cpp:439 (R2_SK_06)")
PHYS = CL + ":122-126 (Emulator CI closes no physical-device gate); " + PS_NEXT + " (OWNER physical review = the one next step, OD-27)"
MERGE = PS_NEXT + " (Nothing merges before the OWNER verdict; v6 merges to main only after Tier-5 once); " + CL + ":115-119"
DONOT = PS + ":4895-4900 (Do not start ... list)"
LIB = CL + ":96-110 (Manifold is the ONE approved third-party library; a second needs its own gate); " + PS + ":4899-4900"
NOFMT = CL + " CADB v1..v8 rules (a version only when its feature exists); 72-fixture corpus " + CL + ":1150; " + PS + ":40-44"
GAP = PS + ":5-11 (canonical Master Plan v4.1.61 unavailable; compliance NOT claimed; PARTIAL-PLAN-SOURCE-GAP)"
NOREC = "no statement in CLAUDE.md / PROJECT_STATUS.md / PRODUCT.md / ARCHITECTURE.md / DATA_PACKAGE_SPEC.md"

R = []


def row(k, section, kind, claimed, quote, matches, mq, sup, auth, ev, notes=""):
    R.append(dict(k=k, section=section, kind=kind, claimed=claimed, quote=quote,
                  matches=matches, mq=mq, sup=sup, auth=auth, ev=ev, notes=notes))


def items(k, section, kind, claimed, head, lst, notes_common=""):
    """One atomic row per item of a verbatim enumerated list."""
    for (frag, matches, mq, sup, auth, ev, note) in lst:
        row(k, section, kind, claimed, frag, matches, mq, sup, auth, ev,
            ("list head: '" + head + "'. " + note + (" " + notes_common if notes_common else "")).strip())

# ---------------------------------------------------------------- A62
row("A62", "header", "decision", "binding (document governance)",
    "Do dopięcia do pełnego, kanonicznego `FORGESHAPE_MASTER_PLAN_v4.1.61.md`, a NIE do użycia zamiast niego.",
    "APP-079", "partial", "-", "UNKNOWN_CURRENT_PLAN", GAP,
    "Addendum is a delta to the ABSENT v4.1.61; restated by MANIFEST.md (2026-10-29). Process rule, not product behaviour.")
row("A62", "A. Nowa dyspozycja OWNER", "decision", "OWNER decision (provisional)",
    "OWNER prowizorycznie przyjmuje ostatni debug APK jako wystarczający do rozpoczęcia dalszej pracy nad CAD",
    "-", "none", "-", "HISTORICAL", PS_R2 + " (CAD-R2 started from 636797e)",
    "Provisional acceptance of the correction APK (b5578...); it is NOT a physical PASS (see H62/H63).")
row("A62", "A. Nowa dyspozycja OWNER", "decision", "OWNER authorisation",
    "autoryzuje duży, wielofazowy prompt Claude Code inspirowany **techniką** SolidCraft",
    "-", "none", "-", "HISTORICAL", PS_R2, "Authorisation consumed by CAD-R2 (delivered, TECH PASS).")
items("A62", "A. Nowa dyspozycja OWNER", "non_goal", "explicitly NOT approved",
      "Nie zatwierdza przez to finalnego pełnego OWNER physical checklist, merge, pierwszej premiery sklepowej, nowego kernela CAD ani nie ogranicza przyjętego MVP.",
      [("finalnego pełnego OWNER physical checklist", "TST-041;CAD-113;MOD-115", "partial", "-", "CURRENT_REPO_CONFIRMED", PHYS, "Physical checklist still open (OD-27)."),
       ("merge", "TST-039;TST-040", "partial", "-", "CURRENT_REPO_CONFIRMED", MERGE, ""),
       ("pierwszej premiery sklepowej", "-", "none", "-", "UNRESOLVED_AUTHORITY", NOREC + " about a store release", "Candidate gap: no repo record of a release/store gate."),
       ("nowego kernela CAD", "PORT-011;PORT-012;PORT-013", "partial", "-", "CURRENT_REPO_CONFIRMED", LIB, ""),
       ("ani nie ogranicza przyjętego MVP", "-", "none", "-", "UNKNOWN_CURRENT_PLAN", GAP, "The 'accepted MVP' is defined in Master v4.1.0/4.1.58 (OQ-B) and v4.1.61 (absent).")])
row("A62", "A. Nowa dyspozycja OWNER", "decision", "superseded rule quoted",
    "Dotychczasową zasadę „nie rozpoczynaj S3 przed OWNER physical review” zastępuje w ograniczonym zakresie",
    "-", "none", "v4.1.62 §A itself (limited scope: CAD-R2 on a separate branch)", "SUPERSEDED", PS + " (stage lines record 'S3 not started')",
    "The old S3 hold is replaced only for CAD-R2; S3 itself is undefined in this archive (lives in v4.1.61 / older status).")
row("A62", "A. Nowa dyspozycja OWNER", "decision", "authorised",
    "**można teraz rozpocząć CAD-R2 na osobnej gałęzi z korekty**",
    "-", "none", "-", "HISTORICAL", PS_R2, "")
row("A62", "A. Nowa dyspozycja OWNER", "gate", "binding",
    "Nie wolno integrować wyników przed końcowym OWNER gate.",
    "TST-039;TST-040", "partial", "-", "CURRENT_REPO_CONFIRMED", MERGE, "")
row("A62", "B. Nowa aktywna implementacja", "status_claim", "active (at 2026-10-28)",
    "ID: `CAD-R2-RETAINED-SKETCH-PARAMETRIC-UX-AND-GEOMETRY-HARDENING`.",
    "CAD-087;CAD-088;CAD-090;CAD-091;CAD-092", "partial", "v4.1.63 CURRENT EXECUTION (delivered, TECH PASS)", "SUPERSEDED", PS_R2, "")
row("A62", "B. Nowa aktywna implementacja", "requirement", "planned",
    "Task branch z `feature/modeling-r1-owner-shell-sketch-fix-r1@636797e`",
    "-", "none", "-", "HISTORICAL", PS_R2 + " (from exactly 636797e)", "")
row("A62", "B. Nowa aktywna implementacja", "status_claim", "missing doc reference",
    "Prompt: `PROMPT_CAD_R2_ULTRACODE_RETAINED_SKETCH_PARAMETRIC_QUALITY.md`.",
    "-", "none", "-", "HISTORICAL", "prompt file absent from archive and repo", "Internal reference to a document NOT in the archive.")
items("A62", "B. Nowa aktywna implementacja", "requirement", "planned theme",
      "Motyw przewodni: dopracowanie już dostarczonego CAD, szczególnie retained sketch workflow i parametric edit, poprawność regionów i dokładny support oraz testy oraklowe.",
      [("retained sketch workflow", "CAD-087;CAD-088;CAD-090;CAD-091;CAD-092", "exact", "-", "CURRENT_REPO_CONFIRMED", CL + ":674-688 (History Sketches); PRODUCT.md:989-1018", ""),
       ("parametric edit", "CAD-068;CAD-094", "exact", "-", "CURRENT_REPO_CONFIRMED", CL + " MODELING-FOUNDATIONS-R1 A (staged History edit); PRODUCT.md:1018", ""),
       ("poprawność regionów", "CAD-047;CAD-049", "partial", "-", "CURRENT_REPO_CONFIRMED", CL + " CAD-VERTICAL-SLICE-R1 region rule", ""),
       ("dokładny support", "CAD-004;CAD-010", "partial", "-", "CURRENT_REPO_CONFIRMED", CL + ":330-340 (NEAREST chooser target)", ""),
       ("testy oraklowe", "TST-056;TST-015", "partial", "-", "CURRENT_REPO_CONFIRMED", PS + ":13 (host 4392/0, 17 oracles)", "")])
row("A62", "B. Nowa aktywna implementacja", "requirement", "planned",
    "5 faz: Phase 0 preflight/design; Phase 1 retained sketches; Phase 2 feature edits/history; Phase 3 geometry selection/support; Phase 4 oracle+reliability/quality.",
    "-", "none", "-", "HISTORICAL", PS_R2, "Stage-internal plan; executed.")
row("A62", "B. Nowa aktywna implementacja", "gate", "planned",
    "Każda faza ma własne testy, a final kończy się DEVICE i FAST z exact APK.",
    "TST-056", "partial", "-", "HISTORICAL", PS_R2 + " (DEVICE + FAST + APK SHA recorded)", "")
row("A62", "B. Nowa aktywna implementacja", "non_goal", "binding for CAD-R2",
    "Nie powoduje zmiany `.forge` layout ani kodeków",
    "TST-017;DOC-009", "partial", "-", "CURRENT_REPO_CONFIRMED", NOFMT, "")
row("A62", "B. Nowa aktywna implementacja", "non_goal", "binding for CAD-R2",
    "nie rozszerza automatycznie listy operacji poza istniejący model",
    "CAD-109;CAD-084", "partial", "-", "CURRENT_REPO_CONFIRMED", CL + ":132-133 (No scope expansion); " + DONOT, "")
row("A62", "B. Nowa aktywna implementacja", "requirement", "binding",
    "Gdy nie da się poprawnie zrealizować podfunkcji bez niezatwierdzonego schematu/warstwy — jawny design block.",
    "CAD-089", "exact", "-", "CURRENT_REPO_CONFIRMED", A2CUR, "Applied: A2 is the recorded design block.")
items("A62", "C. Kryteria releasability i testów", "gate", "required for CAD-R2 final PASS",
      "Gate przed final PASS: existing 25 native suites + nowe tests, JVM full, ≥72 legacy corpus byte-parity, release guard, bounded DEVICE journeys prawdziwymi dotykami, CI FAST na tym samym final candidate, signed APK SHA.",
      [("existing 25 native suites + nowe tests", "TST-001;TST-003", "exact", "-", "CURRENT_REPO_CONFIRMED", CL + ":21-89 (twenty-five suites)", ""),
       ("JVM full", "TST-028", "partial", "-", "HISTORICAL", PS_R2 + " (JVM 196/196)", "Stage gate; met per record."),
       ("≥72 legacy corpus byte-parity", "TST-017;DOC-009", "exact", "-", "CURRENT_REPO_CONFIRMED", CL + ":1150 (seventy-two-fixture corpus)", ""),
       ("release guard", "TST-014;TST-015", "exact", "-", "CURRENT_REPO_CONFIRMED", PS + ":13,41 (release guard PASS)", ""),
       ("bounded DEVICE journeys prawdziwymi dotykami", "ADV-064;TST-056", "partial", "-", "HISTORICAL", PS_R2, "ADV-064 (journeys drive real gestures) is PARTIAL in the dossier."),
       ("CI FAST na tym samym final candidate", "TST-028;TST-027", "partial", "-", "CURRENT_REPO_CONFIRMED", CL + ":113-118 (CI-CLOUD-R1)", ""),
       ("signed APK SHA", "-", "none", "-", "HISTORICAL", PS_R2 + " (debug APK SHA recorded; no signed artifact)", "Wording says 'signed' but every recorded artifact is the debug APK (see next row).")])
row("A62", "C. Kryteria releasability i testów", "decision", "binding",
    "Debug `app-debug.apk` służy testom OWNER, nie jest produkcyjnym AAB.",
    "-", "none", "-", "CURRENT_REPO_CONFIRMED", PS + ":44-47 (OWNER APK = app-debug.apk from ci-fast-evidence)",
    "Repo confirms the OWNER APK is the debug APK; no AAB/release-signing record exists (release side = candidate gap).")
items("A62", "C. Kryteria releasability i testów", "decision", "binding",
      "FullSharded i merge wyłącznie osobna decyzja integracyjna.",
      [("FullSharded", "TST-038;TST-039", "partial", "-", "CURRENT_REPO_CONFIRMED", MERGE + "; OD-03", ""),
       ("merge", "TST-039;TST-040", "partial", "-", "CURRENT_REPO_CONFIRMED", MERGE, "")])
row("A62", "D. SolidCraft research policy", "decision", "binding for CAD-R2",
    "Reference only: SolidCraft `tangency-labs/solidcraft@504d7c948860444ab4968dab62033f34f4f9e6c2`.",
    "-", "none", "-", "UNRESOLVED_AUTHORITY", NOREC + " (SolidCraft is named in no repo record nor in artifacts/cad-r2*)", "Candidate gap (process/provenance).")
row("A62", "D. SolidCraft research policy", "decision", "inspiration list",
    "Inspiracje: derivative geometry vs authored CAD truth, semantic anchors, stable commands, deterministic/oracle tests, atomic write/recovery.",
    "CAD-087;APP-053", "partial", "-", "HISTORICAL", CL + " (authored-truth/derived rule, semantic TopoRef, atomic autosave already durable rules)",
    "The principles are already CLAUDE.md rules; the SolidCraft attribution is historical.")
row("A62", "D. SolidCraft research policy", "non_goal", "prohibited",
    "Zakaz kopiowania modułów, ikon, tekstów, UI",
    "-", "none", "-", "UNRESOLVED_AUTHORITY", NOREC, "Candidate gap: no current record of a no-copy/licence rule for SolidCraft.")
row("A62", "D. SolidCraft research policy", "non_goal", "prohibited",
    "przebudowy C++/Manifold na Rust/truck/SolidCore",
    "PORT-011;PORT-012;PORT-013", "partial", "-", "CURRENT_REPO_CONFIRMED", LIB, "")
row("A62", "D. SolidCraft research policy", "decision", "binding",
    "Priorytet: mierzalny efekt w ForgeShape.", "-", "none", "-", "HISTORICAL", "-", "")
items("A62", "E. Out of scope i otwarte kwestie", "non_goal", "not self-started",
      "Brak samoczynnego uruchomienia Fillet/Chamfer/Shell/Hole/STEP/constraint solver/AI/MCP/Revolve Add-Cut/feature reorder/NURBS.",
      [("Fillet", "CAD-109", "exact", "-", "CURRENT_REPO_CONFIRMED", DONOT, ""),
       ("Chamfer", "CAD-109", "exact", "-", "CURRENT_REPO_CONFIRMED", DONOT, ""),
       ("Shell", "CAD-109", "exact", "-", "CURRENT_REPO_CONFIRMED", DONOT, ""),
       ("Hole", "CAD-109", "exact", "-", "CURRENT_REPO_CONFIRMED", DONOT, ""),
       ("STEP", "APP-052", "partial", "-", "CURRENT_REPO_CONFIRMED", CL + ":1400-1403 (every other interchange format stays absent)", ""),
       ("constraint solver", "CAD-038;CAD-109", "exact", "-", "CURRENT_REPO_CONFIRMED", DONOT, ""),
       ("AI", "-", "none", "-", "UNRESOLVED_AUTHORITY", NOREC, "Candidate gap: no repo rule about AI features."),
       ("MCP", "-", "none", "-", "UNRESOLVED_AUTHORITY", NOREC + " (only 'holds no network permission', CLAUDE.md Diagnostics)", "Candidate gap."),
       ("Revolve Add-Cut", "CAD-084", "exact", "-", "CURRENT_REPO_CONFIRMED", DONOT + "; " + CL + ":477", ""),
       ("feature reorder", "CAD-095", "exact", "-", "CURRENT_REPO_CONFIRMED", DONOT, ""),
       ("NURBS", "MOD-092", "partial", "-", "CURRENT_REPO_CONFIRMED", DONOT + " (general NURBS)", "")])
row("A62", "E. Out of scope i otwarte kwestie", "decision", "open OWNER question",
    "Nie rozstrzyga `OQ-B` (pełny MVP vs tiering)", "-", "none", "-", "OWNER_DECISION_REQUIRED",
    NOREC + "; OQ-B is open in sources/FORGESHAPE_MASTER_PLAN_v4.1.58.md:189,241",
    "Whether v4.1.61 resolved OQ-B is UNKNOWN (absent). H0 scope depends on it. Candidate gap: OQ-B is not in the dossier's OD register.")
row("A62", "E. Out of scope i otwarte kwestie", "decision", "open",
    "release readiness", "-", "none", "-", "OWNER_DECISION_REQUIRED", NOREC, "Release readiness undecided; no repo gate defines it.")
row("A62", "E. Out of scope i otwarte kwestie", "decision", "open",
    "fizycznego OWNER aesthetic acceptance", "TST-041", "partial", "-", "OWNER_DECISION_REQUIRED", PHYS, "")
row("A62", "E. Out of scope i otwarte kwestie", "non_goal", "binding",
    "to dopracowanie retained-sketch, a nie zamiana modelu projektu.", "CAD-087", "partial", "-", "CURRENT_REPO_CONFIRMED",
    CL + ":648-688 (CAD-V6-S1 sketch table unchanged by CAD-R2); " + NOFMT, "")
row("A62", "F. Pochodzenie", "status_claim", "historical evidence",
    "fizyczny APK ma SHA-256 `b557827d56c6979cf46c3451894b38717d6c882faa7610cfbc74f088113411cb`", "-", "none", "-", "HISTORICAL", "-",
    "APK of the correction stage (636797e line); not the current OWNER APK.")
row("A62", "F. Pochodzenie", "status_claim", "true at date",
    "Pełny plik kanonicznego Master Planu v4.1.61 nie był dostępny lokalnie w trakcie tej aktualizacji", "APP-079", "partial", "-",
    "CURRENT_REPO_CONFIRMED", GAP, "Still absent at this closeout (MANIFEST.md).")
row("A62", "F. Pochodzenie", "requirement", "binding (document governance)",
    "Zachować w nim wszystkie oryginalne OWNER decisions, changelog, roadmap i source metadata.", "-", "none", "-",
    "UNKNOWN_CURRENT_PLAN", GAP, "Applies to the absent canon.")

# ---------------------------------------------------------------- H62
row("H62", "v4.1.62", "decision", "OWNER statement (quoted)",
    "OWNER mówi: „akceptuje narazie apk”", "-", "none", "-", "HISTORICAL", "-", "Same decision as A62 §A, history wording.")
row("H62", "v4.1.62", "decision", "coordinator interpretation",
    "zgoda na **kontynuację pracy produktowej na osobnej gałęzi** mimo braku zamkniętej pełnej fizycznej checklisty",
    "-", "none", "-", "HISTORICAL", PS_R2, "")
items("H62", "v4.1.62", "non_goal", "explicitly NOT approved",
      "brak zgody na merge, release, kernel change, solver czy rozbudowę MVP poza granice promptu.",
      [("merge", "TST-039;TST-040", "partial", "-", "CURRENT_REPO_CONFIRMED", MERGE, ""),
       ("release", "-", "none", "-", "UNRESOLVED_AUTHORITY", NOREC, "Candidate gap (release gate)."),
       ("kernel change", "PORT-011", "partial", "-", "CURRENT_REPO_CONFIRMED", LIB, ""),
       ("solver", "CAD-038;CAD-109", "exact", "-", "CURRENT_REPO_CONFIRMED", DONOT, ""),
       ("rozbudowę MVP poza granice promptu", "-", "none", "-", "CURRENT_REPO_CONFIRMED", CL + ":132-133 (No scope expansion)", "")])
row("H62", "v4.1.62", "status_claim", "verified 2026-10-28",
    "sprawdzono GitHub: `main 6c9f156`, v6 `bbae765`, drafting `a3643b5`, modeling `7e02240`, correction `636797e`.",
    "-", "none", "-", "HISTORICAL", "closeout BEFORE.md (origin/main still 6c9f156)", "")
row("H62", "v4.1.62", "status_claim", "historical evidence",
    "CI FAST `37516245911` i DEVICE `37507824851`/`37513387645` to poprzednie dowody techniczne; nie sugerują testów nowego CAD-R2.",
    "-", "none", "-", "HISTORICAL", "-", "")
row("H62", "v4.1.62", "requirement", "planned acceptance matrix",
    "pięć faz i matrycę A1–A10: sketch browser/create/share/delete unused, param edits/history, PlanarFace/support UX, independent geometry oracle, persistence/performance quality.",
    "CAD-087;CAD-088;CAD-090;CAD-091;CAD-094;CAD-004;TST-056", "partial", "-", "HISTORICAL",
    "repo artifacts/cad-r2-retained-sketch-parametric-ux/ACCEPTANCE_MATRIX.md (A2 recorded as design block)", "A2 of this matrix = the open world-plane decision.")
row("H62", "v4.1.62", "non_goal", "prohibited",
    "SolidCraft wyłącznie inspiracja metodą — bez kopiowania kodu i bez zastąpienia C++/Manifold.", "PORT-011", "partial", "-",
    "UNRESOLVED_AUTHORITY", NOREC + " (no-copy); Manifold half confirmed by " + LIB, "No-copy half is a candidate gap.")
items("H62", "v4.1.62", "gate", "required for CAD-R2",
      "Wymagania jakości: 30+ native logic tests, 15+ JVM logic cases, 10+ real DEVICE journeys, final FAST, corpus/guard, dokładny APK",
      [("30+ native logic tests", "TST-056", "partial", "-", "HISTORICAL", PS_R2, ""),
       ("15+ JVM logic cases", "-", "none", "-", "HISTORICAL", PS_R2 + " (JVM 196/196 total)", "Stage quantity gate."),
       ("10+ real DEVICE journeys", "ADV-064;TST-056", "partial", "-", "HISTORICAL", PS_R2, "")])
row("H62", "v4.1.62", "gate", "binding",
    "nie wolno roszczyć PASS przed wykonaniem.", "-", "none", "-", "CURRENT_REPO_CONFIRMED",
    CL + ":1710-1711 (never document a feature as implemented unless runtime-verified)", "")
row("H62", "v4.1.62", "status_claim", "missing doc reference",
    "`FORGESHAPE_PROJECT_STATE_v4.1.62.md` jest pełnym checkpointem.", "-", "none", "-", "HISTORICAL",
    "STATE v4.1.62 absent from archive", "Internal reference to a document NOT in the archive.")

# ---------------------------------------------------------------- A63
row("A63", "header", "decision", "binding (document governance)",
    "Zasada: tylko sekcja bieżącego wykonania Master może ogłaszać aktualny stan po scaleniu.", "-", "none", "-",
    "UNKNOWN_CURRENT_PLAN", GAP, "Depends on the absent canon's section 1; in the repo, PROJECT_STATUS.md owns current status (CLAUDE.md doc table).")
row("A63", "CURRENT EXECUTION", "status_claim", "TECH PASS, not merged",
    "delivered scope technically tested; no merge and no final OWNER physical verdict.", "CAD-087;CAD-088;CAD-090;CAD-091", "partial", "-",
    "CURRENT_REPO_CONFIRMED", PS_R2, "")
row("A63", "CURRENT EXECUTION", "status_claim", "historical evidence",
    "Technical evidence: host **4369/0 (report/documented; not re-run)**, 25 suites", "TST-003", "partial",
    "v4.1.64 (4375/0) and v4.1.65 (4392/0); " + PS + ":13 (FC-R2 re-ran 4392/0)", "SUPERSEDED", PS_HEAD, "")
row("A63", "CURRENT EXECUTION", "status_claim", "historical evidence",
    "APK 17 545 911 B, SHA-256 `043203e18733d10e28985bf86a4cb41ed87fc3c3fd743b391fbc20a6ad5ac272`", "-", "none",
    "v4.1.64/65 APKs", "SUPERSEDED", PS_R2, "")
items("A63", "CURRENT EXECUTION — Product outcome", "status_claim", "delivered (TECH PASS)",
      "**Product outcome:** retained sketch browser, Keep Sketch on self-feature faces, shared sketch reuse for Add/Cut, safe delete unused, feature parameter edit and dependency diagnostics, Select All/Clear, robust validation and oracles.",
      [("retained sketch browser", "CAD-087", "exact", "-", "CURRENT_REPO_CONFIRMED", CL + ":674-678; PRODUCT.md:989-993", ""),
       ("Keep Sketch on self-feature faces", "CAD-088", "exact", "-", "CURRENT_REPO_CONFIRMED", CL + ":678-681; PRODUCT.md:999-1001", ""),
       ("shared sketch reuse for Add/Cut", "CAD-090", "exact", "-", "CURRENT_REPO_CONFIRMED", CL + ":681-682; PRODUCT.md:1006", ""),
       ("safe delete unused", "CAD-091", "exact", "-", "CURRENT_REPO_CONFIRMED", CL + ":682-684; PRODUCT.md:1014", ""),
       ("feature parameter edit and dependency diagnostics", "CAD-068;CAD-094", "exact", "-", "CURRENT_REPO_CONFIRMED", CL + " MODELING-FOUNDATIONS-R1 A; PRODUCT.md:1018", ""),
       ("Select All/Clear", "CAD-049", "exact", "-", "CURRENT_REPO_CONFIRMED", "PRODUCT.md:1025-1027 (per CAD-049)", ""),
       ("robust validation and oracles", "TST-056;TST-015", "partial", "-", "CURRENT_REPO_CONFIRMED", PS + ":13 (17 oracles)", "")])
row("A63", "CURRENT EXECUTION", "non_goal", "delivered constraint",
    "No format change, no new CAD module from SolidCraft.", "TST-017", "partial", "-", "CURRENT_REPO_CONFIRMED",
    NOFMT + " (SolidCraft half: " + NOREC + ")", "Format half confirmed; SolidCraft half unrecorded.")
items("A63", "Status vocabulary", "status_claim", "status at 2026-10-28",
      "`TECH PASS (DELIVERED SCOPE) / A2-WORLD-PLANE DESIGN BLOCK / PERF INVESTIGATION OPEN / OWNER R2 PHYSICAL REVIEW OPEN / NOT MERGED`",
      [("A2-WORLD-PLANE DESIGN BLOCK", "CAD-089", "exact", "-", "CURRENT_REPO_CONFIRMED", A2CUR, ""),
       ("PERF INVESTIGATION OPEN", "-", "none", "v4.1.64 'Disposition: performance A'; " + PS + ":56-60 (NO-REPRO-STABLE)", "SUPERSEDED", PS_C1, ""),
       ("OWNER R2 PHYSICAL REVIEW OPEN", "TST-041", "partial", "-", "CURRENT_REPO_CONFIRMED", PHYS, ""),
       ("NOT MERGED", "TST-039", "partial", "-", "CURRENT_REPO_CONFIRMED", MERGE, "")])
row("A63", "Status vocabulary", "decision", "binding",
    "Claude's PASS token is *not* an unconditional full-scope acceptance.", "-", "none", "-", "CURRENT_REPO_CONFIRMED",
    PS + ":21-22 (audit approves no functionality; TECH PASS / OWNER PHYSICAL REVIEW REQUIRED)", "")
row("A63", "Next bounded work: CAD-R2-C1", "requirement", "recommended then executed",
    "A focused *quality-gate* after major CAD-R2 implementation, on a new task branch from exact `ea24d208`", "-", "none", "-",
    "HISTORICAL", PS_C1, "")
row("A63", "CAD-R2-C1 item 1", "requirement", "C1 work order",
    "Reproduce/isolate +31.5% `eval_all_us` and +35.1% `drafting_trim100_us` median; +27.4% `revolve_square_max_us` as watch.",
    "-", "none", "-", "HISTORICAL", PS + ":56-60", "Executed in C1 (code-placement effect).")
row("A63", "CAD-R2-C1 item 1", "requirement", "C1 method",
    "A/B fresh same-toolchain builds, warm-up, interleaved/order-balanced samples, raw traces, variance and causal attribution.",
    "-", "none", "-", "HISTORICAL", "artifacts/cad-r2-c1-quality-gate/PERF_PROTOCOL.md", "")
row("A63", "CAD-R2-C1 item 1", "decision", "binding method rule",
    "Fix only confirmed product regression; never sweep away a real issue as 'container noise' without evidence.", "-", "none", "-",
    "UNRESOLVED_AUTHORITY", NOREC + " as a durable perf rule", "Candidate gap (perf-evidence policy).")
items("A63", "CAD-R2-C1 item 2", "requirement", "C1 work order",
      "Check curved-wall tap vs world-plane chooser priority/occlusion with actual touch and screenshots/selectedKind; fix only unexpected/unsafe hit ownership; preserve explicit user world-plane selection and fail-closed no curved-face support.",
      [("Check curved-wall tap vs world-plane chooser priority/occlusion", "CAD-004", "exact", "-", "CURRENT_REPO_CONFIRMED", CL + ":336-340", "Delivered in C1."),
       ("preserve explicit user world-plane selection", "CAD-004;CAD-003", "partial", "-", "CURRENT_REPO_CONFIRMED", CL + ":336-340 (a plane in front wins); " + PS + ":62-64", ""),
       ("fail-closed no curved-face support", "CAD-010", "exact", "-", "CURRENT_REPO_CONFIRMED", CL + " (curved side never eligible)", "")])
items("A63", "CAD-R2-C1 item 3", "requirement", "C1 work order",
      "FontScale >1.0 and landscape History action 48 dp checks on emulator; test stale chooser, Undo, Cancel, no accidental UI commit",
      [("FontScale >1.0 and landscape History action 48 dp checks on emulator", "CAD-096;UI-031;UI-076", "exact", "-", "CURRENT_REPO_CONFIRMED", "PRODUCT.md:1019-1023; " + CL + ":335-336", ""),
       ("test stale chooser, Undo, Cancel, no accidental UI commit", "CAD-011", "partial", "-", "HISTORICAL", PS_C1 + " (CadRetainedSketchOwnerTest D15..D18)", "")])
row("A63", "CAD-R2-C1 item 3", "gate", "C1 gate",
    "standard host/JVM/DEVICE/FAST/corpus/guard, exact APK if product files change.", "TST-003;TST-017;TST-014", "partial", "-",
    "HISTORICAL", PS_C1, "")
row("A63", "CAD-R2-C1 item 4", "decision", "binding",
    "Clearly separate OWNER physical acceptance from emulator tests.", "TST-041", "exact", "-", "CURRENT_REPO_CONFIRMED", PHYS, "")
row("A63", "CAD-R2-C1 item 4", "requirement", "C1 report",
    "Report 1:1 feature matrix and residual risks.", "-", "none", "-", "HISTORICAL", "artifacts/cad-r2-c1-quality-gate/SUMMARY.md", "")
items("A63", "Out of scope (CAD-R2-C1)", "non_goal", "not this stage",
      "**Out of scope:** world-plane second root sketch; feature reorder, solver, Fillet, Chamfer, Shell, STEP/IGES, Revolve Add/Cut, public MCP, Rust or SolidCore kernel, `.forge` migrations, independent code copied from SolidCraft.",
      [("world-plane second root sketch", "CAD-089", "exact", "-", "CURRENT_REPO_CONFIRMED", A2CUR, ""),
       ("feature reorder", "CAD-095", "exact", "-", "CURRENT_REPO_CONFIRMED", DONOT, ""),
       ("solver", "CAD-038;CAD-109", "exact", "-", "CURRENT_REPO_CONFIRMED", DONOT, ""),
       ("Fillet", "CAD-109", "exact", "-", "CURRENT_REPO_CONFIRMED", DONOT, ""),
       ("Chamfer", "CAD-109", "exact", "-", "CURRENT_REPO_CONFIRMED", DONOT, ""),
       ("Shell", "CAD-109", "exact", "-", "CURRENT_REPO_CONFIRMED", DONOT, ""),
       ("STEP/IGES", "APP-052", "partial", "-", "CURRENT_REPO_CONFIRMED", CL + ":1400-1403", ""),
       ("Revolve Add/Cut", "CAD-084", "exact", "-", "CURRENT_REPO_CONFIRMED", DONOT, ""),
       ("public MCP", "-", "none", "-", "UNRESOLVED_AUTHORITY", NOREC, "Candidate gap."),
       ("Rust or SolidCore kernel", "PORT-011;PORT-013", "partial", "-", "CURRENT_REPO_CONFIRMED", LIB, ""),
       ("`.forge` migrations", "TST-017", "partial", "-", "HISTORICAL", NOFMT, "Stage-scoped; no current stage authorises a format change (A2 option 1 would)."),
       ("independent code copied from SolidCraft", "-", "none", "-", "UNRESOLVED_AUTHORITY", NOREC, "Candidate gap.")])
items("A63", "Out of scope (CAD-R2-C1)", "non_goal", "not this stage",
      "No silent widening of MVP, no main or v6 merge and no FullSharded without a separately authorised integration gate.",
      [("No silent widening of MVP", "-", "none", "-", "CURRENT_REPO_CONFIRMED", CL + ":132-133", ""),
       ("no main or v6 merge", "TST-039;TST-040", "partial", "-", "CURRENT_REPO_CONFIRMED", MERGE, ""),
       ("no FullSharded without a separately authorised integration gate", "TST-038;TST-039", "partial", "-", "CURRENT_REPO_CONFIRMED", MERGE + "; OD-03", "")])
row("A63", "Open OWNER design decision", "decision", "open OWNER question",
    "`OQ-CAD-R2-WORLD-PLANE-RETAINED-SKETCH`: Should independent sketches authored on XY/XZ/YZ belong to an existing CAD body without becoming the base root?",
    "CAD-089", "exact", "-", "OWNER_DECISION_REQUIRED", A2CUR + "; OD-01", "")
row("A63", "Open OWNER design decision", "status_claim", "current behaviour",
    "Current v6 model refuses a second root and New Sketch on the world plane follows its existing New Body path.",
    "CAD-089;CAD-059", "exact", "-", "CURRENT_REPO_CONFIRMED", A2CUR + "; " + CL + " (a world-plane sketch offers New Body alone)", "")
row("A63", "Open OWNER design decision", "decision", "binding",
    "Keep explicit refusal until separately approved; do not relabel A2 as full PASS.", "CAD-089", "exact", "-",
    "CURRENT_REPO_CONFIRMED", A2CUR, "")
row("A63", "Evidence/provenance", "requirement", "binding (document governance)",
    "never reconstruct missing history from conversation memory or overwrite complete files with shortened text.", "-", "none", "-",
    "UNRESOLVED_AUTHORITY", "restated by MANIFEST.md (2026-10-29) and the closeout task; " + NOREC,
    "Process rule about the plan documents; not a repo record.")
row("A63", "Evidence/provenance", "status_claim", "reference",
    "Use canonical repo `PROJECT_STATUS.md` v0.100.0", "-", "none", PS + " (current version)", "SUPERSEDED", PS, "")

# ---------------------------------------------------------------- H63
row("H63", "v4.1.63", "status_claim", "OWNER authorisation scope",
    "OWNER previously authorised R2 work following *provisional* acceptance of correction APK, not the new R2 APK and not a merge.",
    "-", "none", "-", "HISTORICAL", PS_R2, "")
row("H63", "v4.1.63", "status_claim", "Claude report",
    "Claude reported `PASS-CAD-R2-RETAINED-SKETCH-PARAMETRIC-UX`", "CAD-087", "partial", "-", "CURRENT_REPO_CONFIRMED", PS_R2, "")
row("H63", "v4.1.63", "status_claim", "coordinator verification",
    "Host 4369/0 and 17 oracles remain **reported/documented, not separately executed**.", "TST-003;TST-056", "partial",
    PS + ":13 (FC-R2 executed host 4392/0 incl. 17 oracles on C2 tree)", "SUPERSEDED", PS_HEAD, "")
row("H63", "v4.1.63", "status_claim", "gate nuance",
    "A2 world-plane retained sketch is a **design block**, cannot be declared done", "CAD-089", "exact", "-", "CURRENT_REPO_CONFIRMED", A2CUR, "")
row("H63", "v4.1.63", "status_claim", "open at date",
    "`eval_all` +31.5% and `drafting_trim100` +35.1% medians unexplained, while Revolve +27.4% watched", "-", "none",
    "v4.1.64 Disposition: performance A; " + PS + ":56-60", "SUPERSEDED", PS_C1, "")
row("H63", "v4.1.63", "status_claim", "open at date",
    "D04 curved-wall tap may choose YZ world plane, classification ambiguous", "CAD-004", "exact",
    "v4.1.64 (C1 Track B fix); " + PS + ":62-64", "SUPERSEDED", PS_C1, "")
row("H63", "v4.1.63", "status_claim", "open at date",
    "font scale >1.0 and physical OWNER R2 checklist open.", "CAD-096;UI-076;TST-041", "partial",
    "font half: v4.1.64 (C1 Track C), " + PS + ":65-68; physical half still open", "SUPERSEDED", PS_C1 + "; " + PHYS,
    "Font-scale half superseded; physical-checklist half remains CURRENT (OD-27).")
row("H63", "v4.1.63", "decision", "next prompt",
    "Next prompt selected: `CAD-R2-C1-POST-IMPLEMENTATION-QUALITY-GATE`", "-", "none", "-", "HISTORICAL", PS_C1, "")
row("H63", "v4.1.63", "non_goal", "C1 bounds",
    "preserving existing model, no .forge, no big feature addition. No merge; not an integration gate; no owner decision on second root assumed.",
    "CAD-089;TST-039", "partial", "-", "HISTORICAL", PS_C1, "")
row("H63", "v4.1.63", "status_claim", "missing doc reference",
    "state v4.1.63 is standalone.", "-", "none", "-", "HISTORICAL", "STATE v4.1.63 absent from archive", "Internal reference to a document NOT in the archive.")

# ---------------------------------------------------------------- A64
row("A64", "header", "decision", "binding (document governance)",
    "nie wolno rekonstruować nieznanych sekcji i OWNER decisions z pamięci.", "APP-079", "partial", "-", "UNKNOWN_CURRENT_PLAN", GAP, "")
row("A64", "Delta do §1 — CAD-R2-C1", "status_claim", "TECH PASS",
    "token `PASS-CAD-R2-C1-WITH-RECORDED-BLOCKERS`", "CAD-004;UI-031", "partial", "-", "CURRENT_REPO_CONFIRMED", PS_C1, "")
row("A64", "Delta do §1 — CAD-R2-C1", "status_claim", "refs",
    "Branch `feature/cad-r2-c1-quality-gate-r1@08406cfbbff471f6e53c576c24b45791f903301c`; base `ea24d20805d9487d26221d9dd45e47dc16fb6ecb`",
    "-", "none", "-", "HISTORICAL", PS_C1, "")
row("A64", "Produkt C1", "status_claim", "delivered",
    "`SupportChooser` nie pozwala promieniowi przeniknąć zakrzywionej lub innej niekwalifikowalnej powierzchni i wybrać płaszczyzny świata ZA nią",
    "CAD-004", "exact", "-", "CURRENT_REPO_CONFIRMED", CL + ":336-340; ARCHITECTURE.md:2614", "")
row("A64", "Produkt C1", "status_claim", "delivered",
    "zachowuje płaszczyznę w PRZODZIE", "CAD-004", "exact", "-", "CURRENT_REPO_CONFIRMED", CL + ":336-340", "")
row("A64", "Produkt C1", "status_claim", "delivered",
    "poprawny `TopoRef` kwalifikowalnej ściany", "CAD-010;RND-047", "partial", "-", "CURRENT_REPO_CONFIRMED", CL + ":236-247 (CAD-A3 TopoRef)", "")
row("A64", "Produkt C1", "status_claim", "delivered",
    "W History chip `Edit Sketch` z viewportu jest wycofany, więc przy font scale 1.5 nie przechwytuje akcji History.",
    "UI-031;CAD-096", "exact", "-", "CURRENT_REPO_CONFIRMED", CL + ":333-336; PRODUCT.md:1021-1023", "")
row("A64", "Produkt C1", "non_goal", "delivered constraint",
    "Bez zmiany formatu, korpusu, runtime biblioteki i renderera.", "TST-017", "partial", "-", "CURRENT_REPO_CONFIRMED", NOFMT, "")
row("A64", "Dowód", "status_claim", "historical evidence",
    "host 4375/0 z repo (nie wykonany niezależnie)", "TST-003", "partial", "v4.1.65 (4392/0); " + PS + ":13", "SUPERSEDED", PS_HEAD, "")
row("A64", "Dowód", "status_claim", "status at date",
    "**Status: TECH PASS / OWNER PHYSICAL REVIEW OPEN / NOT MERGED.**", "TST-041;TST-039", "partial", "-", "CURRENT_REPO_CONFIRMED", PS_C1, "C1's own status line still stands in PROJECT_STATUS.")
row("A64", "Disposition: performance A", "status_claim", "disposition",
    "**reprodukowalny przy danym layoucie**", "-", "none", "-", "CURRENT_REPO_CONFIRMED", PS + ":56-60", "")
row("A64", "Disposition: performance A", "status_claim", "disposition",
    "**brak potwierdzonej regresji spowodowanej CAD-R2**, a nie ogólne „performance PASS everywhere”.", "-", "none", "-",
    "CURRENT_REPO_CONFIRMED", PS + ":56-60 (NO-REPRO-STABLE)", "No trace row covers the C1 perf disposition (candidate gap in the matrix, not in product).")
row("A64", "Disposition: performance A", "status_claim", "open",
    "`ARM64 real-device timing = OPEN`", "RND-066;TST-041;CAD-113", "partial", "-", "CURRENT_REPO_CONFIRMED", PHYS, "")
row("A64", "Disposition: performance A", "requirement", "binding (benchmark method)",
    "host code-layout noise dokumentować i kontrolować w dalszych benchmarkach", "TST-013", "partial", "-", "UNRESOLVED_AUTHORITY", NOREC,
    "Candidate gap: no durable benchmark-layout control rule in the repo.")
row("A64", "Disposition: performance A", "non_goal", "binding",
    "Nie dodawać domyślnie flag kompilatora x86 do APK ARM64.", "PORT-036", "partial", "-", "UNRESOLVED_AUTHORITY",
    NOREC + " (app/build.gradle carries no x86 alignment flag per PORT-036)", "Candidate gap (build policy; relates to OD-17).")
row("A64", "OPEN OWNER DECISION — A2", "decision", "open",
    "`OQ-CAD-R2-WORLD-PLANE-RETAINED-SKETCH`: brak decyzji.", "CAD-089", "exact", "-", "OWNER_DECISION_REQUIRED", A2CUR + "; OD-01", "")
items("A64", "OPEN OWNER DECISION — A2", "decision", "option (not chosen)",
      "Opcja 0 = utrzymanie aktualnej odmowy (stan domyślny); opcja 1 = dodatkowe body-local root sketches, nowy `CADB v9`; opcja 2 = authored construction planes, nowy model+format; opcja 3 = project-level sketches, najszersze konsekwencje.",
      [("Opcja 0 = utrzymanie aktualnej odmowy (stan domyślny)", "CAD-089", "exact", "-", "OWNER_DECISION_REQUIRED", A2CUR, "Current behaviour, not a decision (PROJECT_STATUS:4880)."),
       ("opcja 1 = dodatkowe body-local root sketches, nowy `CADB v9`", "CAD-089", "partial", "-", "OWNER_DECISION_REQUIRED", A2CUR, ""),
       ("opcja 2 = authored construction planes, nowy model+format", "CAD-089", "partial", "-", "OWNER_DECISION_REQUIRED", A2CUR, ""),
       ("opcja 3 = project-level sketches, najszersze konsekwencje", "CAD-089", "partial", "-", "OWNER_DECISION_REQUIRED", A2CUR, "")])
row("A64", "OPEN OWNER DECISION — A2", "decision", "binding",
    "Wybór nie wynika z wcześniejszej prowizorycznej akceptacji APK ani z technicznego PASS.", "CAD-089", "partial", "-",
    "CURRENT_REPO_CONFIRMED", PS + ":4878-4880", "")
row("A64", "OPEN OWNER DECISION — A2", "decision", "binding",
    "**Nie implementować A2 bez jawnego wariantu OWNER i osobnego zakresu.**", "CAD-089", "exact", "-", "CURRENT_REPO_CONFIRMED", A2CUR, "")
row("A64", "Następny etap — CAD-R2-C2", "requirement", "C2 goal",
    "usunąć odrzucenie CPU ray–triangle przy wspólnej krawędzi dwóch trójkątów widocznej płaszczyzny", "CAD-005", "exact", "-",
    "CURRENT_REPO_CONFIRMED", PS + ":23-39; app/src/main/cpp/forgeshape_picking.cpp:193-229", "Delivered by C2 (0fa12c9).")
row("A64", "Następny etap — CAD-R2-C2", "requirement", "C2 constraint",
    "nie dopuści to do retargetu na światową płaszczyznę z tyłu ani do fałszywych trafień na zewnątrz obiektu", "CAD-005;CAD-004", "exact", "-",
    "CURRENT_REPO_CONFIRMED", PS + ":31-36", "")
items("A64", "Następny etap — CAD-R2-C2", "requirement", "C2 invariants to preserve",
      "zachować semantic face identity, hit-nearest and back-face policy, renderer geometry source, no hidden state.",
      [("semantic face identity", "RND-047;CAD-005", "exact", "-", "CURRENT_REPO_CONFIRMED", CL + ":244-246 (no nearest-face retargeting); " + PS + ":37-39", ""),
       ("hit-nearest and back-face policy", "RND-043;CAD-005", "exact", "-", "CURRENT_REPO_CONFIRMED", PS + ":37-39 (same thresholds, culling)", ""),
       ("renderer geometry source", "RND-047", "partial", "-", "CURRENT_REPO_CONFIRMED", PS + ":38-39 (no renderer change)", ""),
       ("no hidden state", "-", "none", "-", "HISTORICAL", "artifacts/cad-r2-c2-picking-seam/DESIGN.md", "Stage-scoped constraint; no durable repo rule phrased so.")])
row("A64", "Gates (C2)", "gate", "C2 gate",
    "przed kodem BEFORE + failing focused host reproducer na exakt base", "CAD-005", "partial", "-", "HISTORICAL",
    PS + ":40-41 (8 red before the fix)", "")
items("A64", "Gates (C2)", "gate", "C2 gate",
      "native full 25 suites, existing 17 oracles, JVM, Android build, release selftest guard, corpus 72/72, DEVICE affected + union under time cap, FAST, identyczny product-tree po testach, artifact/APK hash.",
      [("existing 17 oracles", "TST-056", "partial", "-", "CURRENT_REPO_CONFIRMED", PS + ":13,40-41", ""),
       ("DEVICE affected + union under time cap", "CAD-112;TST-040", "partial", "-", "HISTORICAL", PS + ":42-43 (126/126)", ""),
       ("identyczny product-tree po testach", "-", "none", "-", "HISTORICAL", PS + ":26-27 (app/src/main frozen at f3db953)", ""),
       ("artifact/APK hash", "-", "none", "-", "CURRENT_REPO_CONFIRMED", PS + ":44-47", "")])
row("A64", "Gates (C2)", "decision", "stop rule",
    "Jeśli nie da się wykazać przyczyny lub poprawka jest ryzykowna — `BLOCKED-NO-SAFE-PICKING-FIX` z dowodami, bez „mock success”.",
    "-", "none", "-", "HISTORICAL", "-", "Not triggered (C2 PASS).")
row("A64", "Gates (C2)", "gate", "binding",
    "Fizyczny OWNER review obowiązkowy dla każdego nowego APK", "TST-041;MOD-115", "partial", "-", "CURRENT_REPO_CONFIRMED", PHYS, "")
items("A64", "Not authorised (C2)", "non_goal", "not authorised",
      "**Not authorised:** drugi root, CADB v9, solver, feature reorder, Revolve Add/Cut, STEP/IGES, Fillet/Chamfer/Shell, rewrite GPU, new B-rep dependency, AI/MCP, copying SolidCraft, integration merges.",
      [("drugi root", "CAD-089", "exact", "-", "CURRENT_REPO_CONFIRMED", A2CUR, ""),
       ("CADB v9", "CAD-089", "partial", "-", "CURRENT_REPO_CONFIRMED", A2CUR + " (CADB v8 is the latest format, CLAUDE.md)", ""),
       ("solver", "CAD-038;CAD-109", "exact", "-", "CURRENT_REPO_CONFIRMED", DONOT, ""),
       ("feature reorder", "CAD-095", "exact", "-", "CURRENT_REPO_CONFIRMED", DONOT, ""),
       ("Revolve Add/Cut", "CAD-084", "exact", "-", "CURRENT_REPO_CONFIRMED", DONOT, ""),
       ("STEP/IGES", "APP-052", "partial", "-", "CURRENT_REPO_CONFIRMED", CL + ":1400-1403", ""),
       ("Fillet/Chamfer/Shell", "CAD-109", "exact", "-", "CURRENT_REPO_CONFIRMED", DONOT, ""),
       ("rewrite GPU", "-", "none", "-", "CURRENT_REPO_CONFIRMED", CL + ":134-135 (do not opportunistically refactor the renderer)", ""),
       ("new B-rep dependency", "PORT-013", "partial", "-", "CURRENT_REPO_CONFIRMED", LIB, ""),
       ("AI/MCP", "-", "none", "-", "UNRESOLVED_AUTHORITY", NOREC, "Candidate gap."),
       ("copying SolidCraft", "-", "none", "-", "UNRESOLVED_AUTHORITY", NOREC, "Candidate gap."),
       ("integration merges", "TST-039;TST-040", "partial", "-", "CURRENT_REPO_CONFIRMED", MERGE, "")])
row("A64", "Not authorised (C2)", "decision", "binding",
    "Wszelkie niezidentyfikowane wymagania historii pozostają niezatwierdzone.", "APP-079", "partial", "-", "UNRESOLVED_AUTHORITY",
    NOREC + "; consistent with " + GAP, "Bars promoting unidentified historical requirements to H0 without a current basis.")
row("A64", "Źródła", "status_claim", "missing doc reference",
    "`FORGESHAPE_PROJECT_STATE_v4.1.64.md` — bieżący szybki odczyt, szczegóły refów i evidence.", "-", "none", "-", "HISTORICAL",
    "preserved verbatim as ARCHIVED block inside STATE v4.1.66", "")

# ---------------------------------------------------------------- H64
row("H64", "1", "status_claim", "Claude report",
    "Odnotowano ograniczenie A2: brak Keep Sketch na world plane wewnątrz istniejącego CAD body, opcje decyzji 0–3, bez implementacji.",
    "CAD-089", "exact", "-", "CURRENT_REPO_CONFIRMED", A2CUR, "")
row("H64", "2", "status_claim", "coordinator verification",
    "compare `cf340f0...08406cf` = wyłącznie 3 commity docs", "-", "none", "-", "HISTORICAL", PS_C1, "")
row("H64", "3", "status_claim", "coordinator verification",
    "DEVICE B 55/0, 2 pierwsze startup captures zgubiły logi (23/25, 24/25), trzecie pełne 25/25.", "TST-008;TST-001", "partial", "-",
    "HISTORICAL", CL + ":56-62 (dropped capture is not a failure until a larger buffer proves otherwise)",
    "Consistent with the durable logcat-buffer rule.")
row("H64", "3", "status_claim", "coordinator verification",
    "release selftest guard 0 symbols/0 strings x86_64 i arm64, debug 35/398.", "TST-014;TST-015", "exact", "-", "CURRENT_REPO_CONFIRMED", PS + ":13", "")
row("H64", "4", "status_claim", "historical APK",
    "`app-debug.apk` 17 552 535 B SHA-256 `fd7a11a9a7b1bbf7748b46e2d71c71a2a830b54f59e8bd7f9011113d968b1dd5`", "-", "none",
    "v4.1.65 APK 262076...", "SUPERSEDED", PS_C1, "")
row("H64", "5", "status_claim", "open at date",
    "Known pre-existing pick seam at exact shared triangle edge nadal open.", "CAD-005", "exact", "v4.1.65 (C2 fix 0fa12c9); " + PS + ":23-39",
    "SUPERSEDED", PS_HEAD, "")
row("H64", "5", "status_claim", "C1 Track A",
    "`Track A` — host regresja benchmarkowa realna przy konkretnym link layoucie, uzasadniony brak zmiany kodu produktu; ARM64 fizycznie niezmierzony.",
    "RND-066", "partial", "-", "CURRENT_REPO_CONFIRMED", PS + ":56-60; " + PHYS, "")
row("H64", "6", "decision", "coordinator verdict",
    "Koordynator uznaje `TECH PASS / OWNER PHYSICAL REVIEW REQUIRED / A2 DESIGN HOLD / NOT MERGED`, bez obietnicy pełnego produktu.",
    "-", "none", "-", "CURRENT_REPO_CONFIRMED", PS_C1, "")
row("H64", "6", "decision", "next prompt",
    "Jedyny proponowany kolejny prompt: `CAD-R2-C2-PICKING-SEAM-CORRECTNESS-R1`", "CAD-005", "partial", "-", "HISTORICAL", PS_HEAD, "")
row("H64", "7", "status_claim", "true at date",
    "Pełny Master Plan v4.1.61 nie znajdował się w bieżącym runtime", "APP-079", "partial", "-", "CURRENT_REPO_CONFIRMED", GAP, "")

# ---------------------------------------------------------------- A65
row("A65", "Delta do CURRENT EXECUTION", "status_claim", "TECH PASS",
    "Zamknięto technicznie `CAD-R2-C2-PICKING-SEAM-CORRECTNESS-R1` z tokenem Claude `PASS-CAD-R2-C2-PICKING-SEAM`", "CAD-005", "exact", "-",
    "CURRENT_REPO_CONFIRMED", PS_HEAD, "")
row("A65", "Delta do CURRENT EXECUTION", "status_claim", "refs",
    "Produkt jedna poprawka C++ `0fa12c9`, drzewo `app/src/main` stałe po `f3db953`.", "-", "none", "-", "CURRENT_REPO_CONFIRMED",
    PS + ":25-27; closeout BEFORE.md (product tree = 70a6c14 = f3db953)", "")
items("A65", "Delta do CURRENT EXECUTION", "non_goal", "delivered constraint",
      "Bez merge, FullSharded, nowych dependencies lub `.forge` version.",
      [("Bez merge", "TST-039", "partial", "-", "CURRENT_REPO_CONFIRMED", MERGE, ""),
       ("FullSharded", "TST-039", "partial", "-", "CURRENT_REPO_CONFIRMED", PS + ":28", ""),
       ("nowych dependencies", "PORT-013", "partial", "-", "CURRENT_REPO_CONFIRMED", LIB, ""),
       ("`.forge` version", "TST-017", "partial", "-", "CURRENT_REPO_CONFIRMED", PS + ":37-38 (no format change)", "")])
row("A65", "Delta do CURRENT EXECUTION", "status_claim", "delivered",
    "Usunięto reprodukowalny fałszywy miss na wspólnej przekątnej trójkątów przy distant-ortho 250 m", "CAD-005", "exact", "-",
    "CURRENT_REPO_CONFIRMED", PS + ":31-36", "")
row("A65", "Delta do CURRENT EXECUTION", "status_claim", "delivered (implementation)",
    "Möller–Trumbore `intersectRayTriangle` wykonuje obliczenia pośrednie w double przy zachowaniu publicznego float API, epsilon, facing i mapowania semantycznych TopoRef.",
    "CAD-005;RND-043;RND-045;RND-047", "exact", "-", "CURRENT_REPO_CONFIRMED",
    PS + ":37-39; app/src/main/cpp/forgeshape_picking.cpp:193-229 (double D3, det in double)", "")
row("A65", "Delta do CURRENT EXECUTION", "status_claim", "delivered",
    "8 czerwonych kontroli PICK_C2 poprzedza fix.", "CAD-005;TST-015", "partial", "-", "CURRENT_REPO_CONFIRMED", PS + ":40-41", "")
row("A65", "Delta do CURRENT EXECUTION", "status_claim", "delivered",
    "To nie jest retarget przez sąsiednią ścianę.", "CAD-005;RND-047", "exact", "-", "CURRENT_REPO_CONFIRMED", CL + ":244-246", "")
row("A65", "Delta do CURRENT EXECUTION", "status_claim", "externally verified evidence",
    "DEVICE 59/59 run `40776968165` + 67/67 run `40782861799`, startup 25/25, 0 FAIL; JVM 196/196 z 20 XML; corpus 72/72 byte-identical",
    "CAD-112;TST-001;TST-017", "partial", "-", "CURRENT_REPO_CONFIRMED", PS + ":40-43", "")
row("A65", "Delta do CURRENT EXECUTION", "status_claim", "reported, not re-run by coordinator",
    "Host `4392/0`, 17 PICK_C2 i 17 oracles oraz perf per-triangle +2.4–3.0% — wg dokumentów/evidence repo, bez osobnego uruchomienia przez koordynatora.",
    "TST-003;RND-050", "partial", PS + ":13 (FC-R2 re-ran host 4392/0 on C2 tree)", "SUPERSEDED", PS_HEAD,
    "The 'not re-run' qualifier is superseded by FC-R2's fresh host run; the perf half (RND-050) remains recorded-not-reproduced.")
row("A65", "Delta do CURRENT EXECUTION", "status_claim", "current OWNER APK",
    "`app-debug.apk`, 17 582 551 B, SHA256 `262076926e8cc3f90dc70425de4363d3ba3300adfc1998092d581cdffc40be9e`; expires 2026-11-11T23:31:32Z.",
    "-", "none", "-", "CURRENT_REPO_CONFIRMED", PS + ":44-47", "Expiry is a real deadline for the OWNER gate (artifact retention).")
row("A65", "Delta do CURRENT EXECUTION", "status_claim", "binding status",
    "**Obowiązujący status:** `TECH PASS / OWNER PHYSICAL REVIEW REQUIRED / A2 DESIGN HOLD / NOT MERGED`.", "CAD-089;TST-041;TST-039", "partial", "-",
    "CURRENT_REPO_CONFIRMED", PS_HEAD, "")
row("A65", "Delta do CURRENT EXECUTION", "gate", "awaiting OWNER",
    "OWNER testuje dokładny APK według pakietu ze STATE v4.1.65, zgłasza PASS/FAIL/NOT TESTED i ewentualny dobrowolny wybór A2.",
    "TST-041;MOD-115;CAD-113", "partial", "-", "OWNER_DECISION_REQUIRED", PS_NEXT, "= OD-27 (+ OD-01).")
row("A65", "Delta do CURRENT EXECUTION", "decision", "binding",
    "Nie wykonywać teraz kolejnego feature stage ani merge; **brak active coding prompt**.", "-", "none", "-", "CURRENT_REPO_CONFIRMED", PS_NEXT,
    "v4.1.66 later added an AUDIT-only prompt (FC-R2), which is not a coding/feature stage.")
row("A65", "Delta do CURRENT EXECUTION", "decision", "binding",
    "Nie traktować zielonego emulatora jako fizycznego APPROVAL.", "TST-041", "exact", "-", "CURRENT_REPO_CONFIRMED", PHYS, "")
row("A65", "OPEN A2 decision", "decision", "open",
    "Brak zgody na drugą root sketch/body-local world sketch oraz `CADB v9`.", "CAD-089", "exact", "-", "OWNER_DECISION_REQUIRED", A2CUR, "")
row("A65", "OPEN A2 decision", "decision", "open",
    "Żaden wariant wybrany; decyzja później, podmiotem jest OWNER.", "CAD-089", "exact", "-", "OWNER_DECISION_REQUIRED", A2CUR + "; OD-01", "")
row("A65", "OPEN A2 decision", "requirement", "binding",
    "Zachować zgodność starszych readerów i nie ruszać codec bez osobnego projektu.", "DOC-009;TST-017", "partial", "-",
    "CURRENT_REPO_CONFIRMED", CL + ":266-270 (a version is a superset; older builds refuse unknown versions)", "")
items("A65", "Pozostałe ryzyka", "status_claim", "NOT TESTED",
      "Fizyczny ARM64, GPU, stylus/hover i odczuwalny koszt double precision dla gęstej Sculpt mesh: NOT TESTED.",
      [("Fizyczny ARM64", "PORT-032;RND-066", "partial", "-", "CURRENT_REPO_CONFIRMED", PHYS, "Gate P1 closed physical ARM64 for an OLD build (PROJECT_STATUS:3878); not for this APK."),
       ("GPU", "RND-066", "exact", "-", "CURRENT_REPO_CONFIRMED", PHYS, ""),
       ("stylus/hover", "MOD-115;UI-033;TST-041", "exact", "-", "CURRENT_REPO_CONFIRMED", PHYS, ""),
       ("odczuwalny koszt double precision dla gęstej Sculpt mesh", "RND-050;RND-051", "partial", "-", "UNRESOLVED_AUTHORITY",
        "artifacts/cad-r2-c2-picking-seam/PERF.md (host only); " + NOREC, "Sculpt-specific cost of double picking not recorded as a gate.")])
row("A65", "Pozostałe ryzyka", "status_claim", "known behaviour",
    "Picking dokładnie na granicy dwóch różnych ścian może wskazać jedną z tych rzeczywiście przecinanych ścian", "CAD-005;CAD-011", "partial", "-",
    "UNRESOLVED_AUTHORITY", "artifacts/cad-r2-c2-picking-seam/SUMMARY.md:70-73 ('may resolve to either face'); " + NOREC,
    "Stated only in a stage dossier, not a current repo record; tie rule (body wins exact ties) lives there.")
row("A65", "Pozostałe ryzyka", "status_claim", "known behaviour",
    "nie jest to twarda referencja do konkretnej powierzchni, dopóki nie nastąpi jej semantyczna kwalifikacja i confirm.", "CAD-011", "partial", "-",
    "CURRENT_REPO_CONFIRMED", CL + " MODELING-R1-OWNER-CORRECTION (aim then confirm; semantic TopoRef)", "")
row("A65", "Pozostałe ryzyka", "status_claim", "disposition",
    "Host JCC layout variability po C1 nie jest globalnym „perf PASS”, ale też nie jest dowodem regresji R2 produktu.", "-", "none", "-",
    "CURRENT_REPO_CONFIRMED", PS + ":56-60", "")
items("A65", "Pozostałe ryzyka", "gate", "separate, unauthorised",
      "Dodatkowy FullSharded, integracja CAD-V6→main, release AAB to oddzielne, nieautoryzowane bramki.",
      [("Dodatkowy FullSharded", "TST-038;TST-039", "partial", "-", "CURRENT_REPO_CONFIRMED", MERGE + "; OD-03", ""),
       ("integracja CAD-V6→main", "TST-039;TST-040", "exact", "-", "CURRENT_REPO_CONFIRMED", MERGE, ""),
       ("release AAB", "-", "none", "-", "UNRESOLVED_AUTHORITY", NOREC, "Candidate gap: no release/AAB gate in repo records.")])
row("A65", "Dokumentacja i źródła", "status_claim", "missing doc reference",
    "`FORGESHAPE_PROJECT_STATE_v4.1.65.md` / `FORGESHAPE_PROJECT_STATE.md` (bieżący stan)", "-", "none", "-", "HISTORICAL",
    "preserved verbatim as ARCHIVED block inside STATE v4.1.66", "")

# ---------------------------------------------------------------- H65
row("H65", "1", "status_claim", "Claude report",
    "Claude przekazał `PASS-CAD-R2-C2-PICKING-SEAM`", "CAD-005", "exact", "-", "CURRENT_REPO_CONFIRMED", PS_HEAD, "")
row("H65", "2", "status_claim", "coordinator verification",
    "Porównanie `f3db953→C2 HEAD` zawiera dokumenty i androidTest, nie ma zmian `app/src/main`.", "-", "none", "-", "CURRENT_REPO_CONFIRMED", PS + ":26-27", "")
row("H65", "2", "status_claim", "coordinator verification",
    "FAST head `14a3dbc` był docs-only względem urządzeniowego `30b561b`.", "-", "none", "-", "HISTORICAL", PS + ":44-45", "")
row("H65", "4", "status_claim", "coordinator verification",
    "startup 25/25 in-order, viewport OK, żadnego failure token", "TST-001", "exact", "-", "CURRENT_REPO_CONFIRMED", PS + ":40-43", "")
row("H65", "6", "status_claim", "code inspection",
    "obliczenie Möller–Trumbore w double z float wejść przy zachowaniu tolerancji, cullingu, rozstrzygania hit i formatu", "CAD-005;RND-043", "exact", "-",
    "CURRENT_REPO_CONFIRMED", "app/src/main/cpp/forgeshape_picking.cpp:193-229", "")
row("H65", "6", "status_claim", "code inspection",
    "identyfikatory semantyczne CAD nieruszone", "RND-047", "exact", "-", "CURRENT_REPO_CONFIRMED", PS + ":37-39", "")
row("H65", "6", "status_claim", "evidence provenance",
    "zebrany i przeanalizowany, ale bez lokalnego odtworzenia hosta przez koordynatora.", "CAD-005", "partial",
    PS + ":13 (FC-R2 host run incl. PICK_C2)", "SUPERSEDED", PS_HEAD, "")
row("H65", "7", "status_claim", "open",
    "Open real-device ARM64/stylus/GPU/perf.", "RND-066;TST-041;MOD-115", "partial", "-", "CURRENT_REPO_CONFIRMED", PHYS, "")
row("H65", "7", "decision", "coordinator decision",
    "nie aktywować kolejnego coding stage", "-", "none", "-", "CURRENT_REPO_CONFIRMED", PS_NEXT, "")
row("H65", "7", "decision", "prepared, not to run",
    "Jeden gotowy prompt `CAD-R2-C2-OWNER-PHYSICAL-REVIEW-RESULT-GATE-R1` jest docs-only i uruchamiany dopiero gdy OWNER przekaże faktyczną checklistę; nie fikcyjne PASS.",
    "-", "none", "-", "UNRESOLVED_AUTHORITY", NOREC + " (prompt file absent from archive)",
    "Restated, not superseded, by STATE v4.1.66 §1; repo names OD-27 instead.")
row("H65", "8", "status_claim", "true at date",
    "Pełny MASTER v4.1.61 / HISTORY v4.1.61 pozostają niedostępne w bieżącym runtime.", "APP-079", "partial", "-", "CURRENT_REPO_CONFIRMED", GAP, "")

# ---------------------------------------------------------------- A66
row("A66", "header", "status_claim", "type",
    "**Typ zmiany:** OWNER direction / audit execution planning. **Produkt:** bez zmian.", "-", "none", "-", "CURRENT_REPO_CONFIRMED",
    PS + ":10-12 (only artifacts/function-council-r2/** added)", "")
row("A66", "Nowa decyzja OWNER", "decision", "OWNER decision",
    "OWNER wybiera **pełny, oparty na źródłach i zautomatyzowanych testach FUNCTION-COUNCIL-R2** jako następną pracę", "-", "none", "-",
    "CURRENT_REPO_CONFIRMED", PS_HEAD, "Executed; repo records it as the latest audit.")
items("A66", "Nowa decyzja OWNER", "requirement", "audit scope",
      "Zlecony audyt obejmuje całą dotychczas należną funkcjonalność ForgeShape według zatwierdzonego Master Planu, rzeczywistą implementację i interakcje funkcji, niespełnione wymagania, problemy UX/UI Androida, analizę jakości/potencjału rozwoju oraz **studium portowalności na PC (Windows/Linux)**.",
      [("całą dotychczas należną funkcjonalność ForgeShape według zatwierdzonego Master Planu", "APP-079", "partial", "-", "UNKNOWN_CURRENT_PLAN", GAP, "Due-scope (H0) depends on the absent v4.1.61."),
       ("rzeczywistą implementację i interakcje funkcji", "ADV-064", "partial", "-", "CURRENT_REPO_CONFIRMED", PS + ":13-20 (681 requirements traced)", ""),
       ("niespełnione wymagania", "-", "partial", "-", "CURRENT_REPO_CONFIRMED", PS + ":16-20", ""),
       ("problemy UX/UI Androida", "UI-040;UI-076", "partial", "-", "CURRENT_REPO_CONFIRMED", PS_HEAD, ""),
       ("analizę jakości/potencjału rozwoju", "-", "partial", "-", "HISTORICAL", "artifacts/function-council-r2/OPPORTUNITIES.md, ARCHITECTURE_HEALTH.md", ""),
       ("**studium portowalności na PC (Windows/Linux)**", "PORT-001;PORT-037;PORT-038", "partial", "-", "CURRENT_REPO_CONFIRMED", PS + ":4-5 (Android-desktop portability audit)", "")])
items("A66", "Nowa decyzja OWNER", "non_goal", "explicitly NOT approved",
      "Jest to decyzja o **AUDYCIE**, nie zgoda na implementację nowych narzędzi CAD, desktop port, nową bibliotekę, CADB v9 czy integrację.",
      [("nowych narzędzi CAD", "CAD-109", "partial", "-", "CURRENT_REPO_CONFIRMED", PS + ":21 (approves no functionality); " + DONOT, ""),
       ("desktop port", "UI-079;APP-077", "partial", "-", "CURRENT_REPO_CONFIRMED", CL + ":1555-1562 (no portability abstraction without a target); OD-04", ""),
       ("nową bibliotekę", "PORT-013", "partial", "-", "CURRENT_REPO_CONFIRMED", LIB, ""),
       ("CADB v9", "CAD-089", "partial", "-", "CURRENT_REPO_CONFIRMED", A2CUR, ""),
       ("integrację", "TST-039", "partial", "-", "CURRENT_REPO_CONFIRMED", MERGE, "")])
row("A66", "Przewidziana praca", "requirement", "work order",
    "Wykonanie przez Claude Code na nowej gałęzi od weryfikowanej bazy `feature/cad-r2-c2-picking-seam-r1@70a6c14f703bfda2ab689b855126c1d12a541e41`; audyt jest DOCS/DIAGNOSTICS-ONLY.",
    "-", "none", "-", "CURRENT_REPO_CONFIRMED", PS + ":10-12", "")
row("A66", "Przewidziana praca", "status_claim", "status at date",
    "Nie stwierdzono jeszcze wykonania tego audytu, jego wynik pozostaje `NOT STARTED / NOT VERIFIED`.", "-", "none",
    PS + ":3-22 (v0.103.0: FC-R2 executed, PARTIAL-PLAN-SOURCE-GAP)", "SUPERSEDED", PS_HEAD, "")
row("A66", "Status produktu", "status_claim", "unchanged",
    "`CAD-R2-C2: TECH PASS / OWNER PHYSICAL REVIEW REQUIRED / A2 DESIGN HOLD / NOT MERGED`.", "CAD-089;TST-041", "partial", "-",
    "CURRENT_REPO_CONFIRMED", PS_HEAD, "")
row("A66", "Status produktu", "decision", "binding",
    "Te dowody **nie** stają się automatycznie pełnym OWNER physical PASS.", "TST-041", "exact", "-", "CURRENT_REPO_CONFIRMED", PHYS, "")
row("A66", "Council req 1", "requirement", "audit method",
    "Mapowanie H0 (zatwierdzone MVP należne do dziś), H1 (późniejsze autoryzowane funkcje), H2 (przyszłe/planned/blocked) z oddzielnymi werdyktami.",
    "-", "partial", "-", "HISTORICAL", "artifacts/function-council-r2/requirements_traceability.tsv (horizon column)",
    "H0 definition depends on the approved MVP (OQ-B / v4.1.61) — see UNKNOWN_CURRENT_PLAN rows.")
row("A66", "Council req 1", "decision", "binding",
    "Zero automatycznego rozszerzenia MVP.", "-", "none", "-", "CURRENT_REPO_CONFIRMED", CL + ":132-133", "")
row("A66", "Council req 2", "requirement", "audit structure",
    "10 odrębnych seatów: produkt/journeys; Construction; CAD/Sketch/History; Sculpt/Freeform/Surface; persistence/lifecycle; Android UI/UX/accessibility; przyszły desktop;",
    "-", "partial", "-", "HISTORICAL", "artifacts/function-council-r2/SEAT_A..J.md", "")
items("A66", "Council req 3", "requirement", "audit coverage",
      "Wszystkie dostępne funkcje oraz krytyczne ścieżki odmowy/Undo/.forge, testy i dowody; 30+ opisanych journey; brak PASS bez konkretnej podstawy.",
      [("krytyczne ścieżki odmowy/Undo/.forge", "CON-064;CAD-097;TST-020", "partial", "-", "HISTORICAL", "dossier negative_refusal_path / undo_history columns", ""),
       ("30+ opisanych journey", "ADV-064", "partial", "-", "HISTORICAL", "artifacts/function-council-r2/END_TO_END_JOURNEYS.md", ""),
       ("brak PASS bez konkretnej podstawy", "-", "none", "-", "CURRENT_REPO_CONFIRMED", CL + ":1710-1711", "")])
row("A66", "Council req 4", "requirement", "audit scope",
    "Ponowny audyt `FUNCTION-COUNCIL-R1` na aktualnej bazie; stary R1 miał sześć seatów, z których CAD, Sculpt, Android i renderer były jedynie częściowe",
    "APP-025;APP-041;CAD-074;DOC-081", "partial", "-", "CURRENT_REPO_CONFIRMED", PS + ":419; " + PS + ":4888-4893 (D1/D5 closed; D2-D4 open; D6/D7 partial)", "")
items("A66", "Council req 5", "requirement", "audit evidence policy",
      "Zautomatyzowane testy istniejących suite, kompilacji, corpus, CI/DEVICE z realistyczną informacją o faktycznie wykonanych testach. Brak zgody na FullSharded, modyfikowanie produktu/testów/CI ani merge.",
      [("Zautomatyzowane testy istniejących suite, kompilacji, corpus, CI/DEVICE", "TST-003;TST-017;TST-028", "partial", "-", "CURRENT_REPO_CONFIRMED", PS + ":13-16", ""),
       ("z realistyczną informacją o faktycznie wykonanych testach", "CAD-112;TST-040", "partial", "-", "CURRENT_REPO_CONFIRMED", PS + ":13-16 (DEVICE runs named with counts; emulator only)", ""),
       ("Brak zgody na FullSharded", "TST-039", "partial", "-", "CURRENT_REPO_CONFIRMED", PS + ":12", ""),
       ("modyfikowanie produktu/testów/CI", "-", "none", "-", "CURRENT_REPO_CONFIRMED", PS + ":10-12 (only artifacts added)", ""),
       ("ani merge", "TST-039", "partial", "-", "CURRENT_REPO_CONFIRMED", PS + ":12", "")])
items("A66", "Council req 6", "requirement", "Android UI/UX evidence",
      "Dla Android UI/UX: screenshoty z konfiguracją, realne prostokąty hit, orientacja, font scale, stylus/pointer input, IME, dostępność i spójność.",
      [("screenshoty z konfiguracją", "-", "partial", "-", "HISTORICAL", "artifacts/function-council-r2/SCREEN_EVIDENCE_INDEX.md", ""),
       ("realne prostokąty hit", "UI-015;TST-044", "partial", "-", "CURRENT_REPO_CONFIRMED", CL + " 48 dp floor is the HIT AREA", ""),
       ("orientacja", "UI-048", "partial", "-", "CURRENT_REPO_CONFIRMED", CL + " one orientation convention; UI-LAYOUT-R2", ""),
       ("font scale", "UI-015;UI-076;CAD-096", "partial", "-", "CURRENT_REPO_CONFIRMED", "PRODUCT.md:1021", ""),
       ("stylus/pointer input", "PORT-023;UI-033;MOD-115", "partial", "-", "CURRENT_REPO_CONFIRMED", "ARCHITECTURE.md:211 (Input row); " + PHYS, ""),
       ("IME", "UI-015", "partial", "-", "CURRENT_REPO_CONFIRMED", CL + " (no IME may inset the viewport)", ""),
       ("dostępność", "UI-072", "partial", "-", "UNRESOLVED_AUTHORITY", NOREC + " for an accessibility spec (OD-21)", "No accessibility specification exists (OD-21)."),
       ("spójność", "-", "none", "-", "HISTORICAL", "artifacts/function-council-r2/ANDROID_UI_UX_AUDIT.md", "")])
row("A66", "Council req 6", "decision", "binding",
    "Kwestie fizycznego GPU/termiki/rysika i subiektywnej oceny pozostają `OWNER-PHYSICAL-UNVERIFIED`.", "RND-066;TST-041;MOD-115", "exact", "-",
    "CURRENT_REPO_CONFIRMED", PHYS, "")
items("A66", "Council req 7", "requirement", "desktop study",
      "Dla przyszłego Windows/Linux: matryca przenośności i warianty adapterów C++/JNI/Java/Vulkan; **bez** implementacji, wymiany kernela lub nowej licencji runtime.",
      [("matryca przenośności i warianty adapterów C++/JNI/Java/Vulkan", "PORT-001;PORT-014;PORT-040", "partial", "-", "HISTORICAL", "artifacts/function-council-r2/PORTABILITY_MATRIX.md, DESKTOP_PORT_STRATEGIES.md", ""),
       ("**bez** implementacji", "UI-079", "partial", "-", "CURRENT_REPO_CONFIRMED", CL + ":1555-1562; OD-04", ""),
       ("wymiany kernela", "PORT-011", "partial", "-", "CURRENT_REPO_CONFIRMED", LIB, ""),
       ("nowej licencji runtime", "PORT-013", "partial", "-", "CURRENT_REPO_CONFIRMED", LIB, "")])
items("A66", "Council req 8", "requirement", "audit outputs",
      "Wyniki: wymagania→kod→test, recheck starego długu Council D1–D7, ocena UX, plan desktop, uporządkowany backlog rozdzielający DEFECT/QUALITY/FUTURE IDEA, rejestr decyzji OWNER, jeden najmniejszy rekomendowany przyszły etap — nie rozpoczęty.",
      [("wymagania→kod→test", "-", "partial", "-", "CURRENT_REPO_CONFIRMED", PS + ":16-17 (681 atomic requirements traced)", ""),
       ("recheck starego długu Council D1–D7", "APP-025;APP-041;CAD-074", "partial", "-", "CURRENT_REPO_CONFIRMED", PS + ":4888-4893", ""),
       ("ocena UX", "-", "none", "-", "HISTORICAL", "artifacts/function-council-r2/ANDROID_UI_UX_AUDIT.md", ""),
       ("plan desktop", "-", "none", "-", "HISTORICAL", "artifacts/function-council-r2/DESKTOP_POC_GATE.md", ""),
       ("uporządkowany backlog rozdzielający DEFECT/QUALITY/FUTURE IDEA", "-", "none", "-", "HISTORICAL", "artifacts/function-council-r2/PRIORITISED_BACKLOG.md", ""),
       ("rejestr decyzji OWNER", "-", "none", "-", "CURRENT_REPO_CONFIRMED", PS + ":4884-4887 (OWNER_DECISIONS_REQUIRED.md)", ""),
       ("jeden najmniejszy rekomendowany przyszły etap — nie rozpoczęty", "-", "none", "-", "CURRENT_REPO_CONFIRMED", PS + ":4884-4887 (CAD-FACE-SUPPORTED-PLACEMENT-R1, not started)", "")])
row("A66", "Council req 9", "gate", "binding",
    "wynik zgodności z Master Planem musi być `PARTIAL-PLAN-SOURCE-GAP`; zabrania się fikcyjnego odtworzenia brakującej wersji.", "APP-079", "partial", "-",
    "CURRENT_REPO_CONFIRMED", GAP, "")
row("A66", "Otwarta decyzja A2", "decision", "open",
    "`BLOCKED-DESIGN-DECISION-R2-WORLD-PLANE-RETAINED-SKETCH` pozostaje niezmienione.", "CAD-089", "exact", "-", "OWNER_DECISION_REQUIRED", A2CUR, "")
row("A66", "Otwarta decyzja A2", "decision", "binding",
    "Council może porównać opcje 0/1/2/3 jako materiał decyzyjny, nie wdraża ich i nie wybiera sam.", "CAD-089", "partial", "-",
    "CURRENT_REPO_CONFIRMED", PS + ":4878-4887 (OD-01 open; Council adds decision support only)", "")
row("A66", "Otwarta decyzja A2", "gate", "open",
    "Fizyczny przegląd najnowszego APK nadal stanowi oddzielną, nierozstrzygniętą bramkę; Council nie może go zamknąć wynikiem emulatora.",
    "TST-041;MOD-115", "exact", "-", "CURRENT_REPO_CONFIRMED", PHYS, "")
row("A66", "Provenance", "status_claim", "available sources at date",
    "W obecnym środowisku dostępne są tylko częściowy Master v4.1.58, wcześniejsze v4.1.0 / starsze archiwa i addenda v4.1.62–65", "APP-079", "partial", "-",
    "CURRENT_REPO_CONFIRMED", GAP + "; MANIFEST.md", "")
row("A66", "Provenance", "status_claim", "reference",
    "Źródła repo: `PROJECT_STATUS.md` v0.102.0", "-", "none", PS + " (current version)", "SUPERSEDED", PS, "")

# ---------------------------------------------------------------- H66
row("H66", "v4.1.66", "decision", "OWNER direction",
    "OWNER directs a comprehensive present-day requirements/function audit, including every approved feature expected by the Master Plan to this point",
    "APP-079", "partial", "-", "UNKNOWN_CURRENT_PLAN", GAP, "'Every approved feature expected by the Master Plan' cannot be enumerated without v4.1.61.")
items("H66", "v4.1.66", "non_goal", "explicitly NOT approved",
      "The request is NOT an approval of a physical phone result, FullSharded, CADB v9, desktop implementation or merge.",
      [("a physical phone result", "TST-041", "exact", "-", "CURRENT_REPO_CONFIRMED", PHYS, ""),
       ("FullSharded", "TST-039", "partial", "-", "CURRENT_REPO_CONFIRMED", PS + ":12", ""),
       ("CADB v9", "CAD-089", "partial", "-", "CURRENT_REPO_CONFIRMED", A2CUR, ""),
       ("desktop implementation", "UI-079", "partial", "-", "CURRENT_REPO_CONFIRMED", CL + ":1555-1562; OD-04", ""),
       ("merge", "TST-039", "partial", "-", "CURRENT_REPO_CONFIRMED", MERGE, "")])
row("H66", "v4.1.66", "decision", "binding",
    "conclusions and old D1–D7 are inputs requiring re-verification, not today's findings.", "APP-025;APP-041;CAD-074", "partial", "-",
    "CURRENT_REPO_CONFIRMED", PS + ":4888-4893", "")
row("H66", "v4.1.66", "status_claim", "status at date",
    "Claude Code has **not** yet reported executing this stage.", "-", "none", PS + ":3-22 (FC-R2 recorded)", "SUPERSEDED", PS_HEAD, "")
row("H66", "v4.1.66", "status_claim", "missing doc reference",
    "One comprehensive documentation/automated-evidence prompt created: `PROMPT_FORGESHAPE_FUNCTION_COUNCIL_R2_FULL_PRODUCT_PLATFORM_AUDIT.md`",
    "-", "none", "-", "HISTORICAL", "prompt file absent from archive", "Internal reference to a document NOT in the archive.")
row("H66", "v4.1.66", "gate", "binding",
    "Council must explicitly flag PARTIAL plan coverage if Claude cannot locate the canonical version.", "APP-079", "partial", "-",
    "CURRENT_REPO_CONFIRMED", GAP, "")

# ---------------------------------------------------------------- S66 (current block)
row("S66", "header", "status_claim", "status at date",
    "status `PROMPT PREPARED / NOT STARTED`; żaden wynik R2 Council nie jest jeszcze potwierdzony.", "-", "none",
    PS + ":3-22 (FC-R2 executed and recorded)", "SUPERSEDED", PS_HEAD, "")
row("S66", "1. Zmiana kierunku pracy", "decision", "binding",
    "nie może udawać fizycznego OWNER PASS ani zastępować testów realnego GPU/stylusa/termiki.", "TST-041;RND-066", "exact", "-",
    "CURRENT_REPO_CONFIRMED", PHYS, "")
row("S66", "1. Zmiana kierunku pracy", "decision", "binding",
    "Nie uruchamiać poprzedniego docs-only promptu `CAD-R2-C2-OWNER-PHYSICAL-REVIEW-RESULT-GATE-R1`, gdyż brak evidence OWNER.", "-", "none", "-",
    "UNRESOLVED_AUTHORITY", NOREC, "Repo instead names OD-27 (physical review) as the one next step.")
row("S66", "1. Zmiana kierunku pracy", "status_claim", "missing doc reference",
    "**Jeden prompt:** `/mnt/data/PROMPT_FORGESHAPE_FUNCTION_COUNCIL_R2_FULL_PRODUCT_PLATFORM_AUDIT.md`.", "-", "none", "-", "HISTORICAL",
    "prompt file absent from archive", "")
items("S66", "2. Cel i kryteria Council", "requirement", "audit outputs",
      "requirements traceability H0/H1/H2, pełny feature inventory, >=30 journeys, Android ekran×stan×orientacja/font, PC port matrix z wariantami, stare Council D1–D7 rechecked, root-cause risk list",
      [("pełny feature inventory", "-", "none", "-", "HISTORICAL", "artifacts/function-council-r2/FEATURE_INVENTORY.md", ""),
       ("Android ekran×stan×orientacja/font", "UI-048;UI-076", "partial", "-", "HISTORICAL", "artifacts/function-council-r2/UI_SCREEN_STATE_MATRIX.tsv", ""),
       ("root-cause risk list", "-", "none", "-", "HISTORICAL", "artifacts/function-council-r2/RISK_REGISTER.md", "")],
      "Other items of this list duplicate A66 Council req 1/3/7/8.")
row("S66", "2. Cel i kryteria Council", "decision", "binding",
    "nie wolno uogólniać jego starych wniosków na dzisiejszy produkt.", "-", "none", "-", "CURRENT_REPO_CONFIRMED", PS + ":4888-4893", "")
row("S66", "2. Cel i kryteria Council", "requirement", "audit scope",
    "D1 zgłoszono wcześniej jako naprawiony w C1-D1, ale Council R2 musi zbadać go ponownie, a D2–D7 zweryfikować z aktualnym kodem.",
    "APP-025;APP-041;CAD-074;DOC-081", "partial", "-", "CURRENT_REPO_CONFIRMED", PS + ":18-20,4888-4893", "")
row("S66", "2. Cel i kryteria Council", "gate", "binding",
    "audyt ma status `PARTIAL-PLAN-SOURCE-GAP`; nie wolno stwierdzić pełnej zgodności z planem przez zgadywanie.", "APP-079", "partial", "-",
    "CURRENT_REPO_CONFIRMED", GAP, "")
items("S66", "3. Wstrzymane decyzje i zakazy", "non_goal", "no consent",
      "**A2 world-plane retained sketches:** żadnej zgody na dodatkowy root, CADB v9 ani project-level szkice.",
      [("dodatkowy root", "CAD-089", "exact", "-", "CURRENT_REPO_CONFIRMED", A2CUR, ""),
       ("CADB v9", "CAD-089", "partial", "-", "CURRENT_REPO_CONFIRMED", A2CUR, ""),
       ("project-level szkice", "CAD-089", "partial", "-", "CURRENT_REPO_CONFIRMED", A2CUR, "")])
row("S66", "3. Wstrzymane decyzje i zakazy", "gate", "open",
    "**OWNER physical review C2:** brak pełnego PASS; zostaje oddzielną bramką.", "TST-041;MOD-115", "exact", "-", "OWNER_DECISION_REQUIRED", PHYS, "= OD-27.")
row("S66", "3. Wstrzymane decyzje i zakazy", "decision", "binding",
    "brak fizycznego testu nie blokuje dokumentacyjnego Council.", "-", "none", "-", "CURRENT_REPO_CONFIRMED",
    PS + ":4884-4887 (Council adds decision support, not a second next step)", "")
items("S66", "3. Wstrzymane decyzje i zakazy", "non_goal", "prohibited",
      "**Integration:** nie mergować do `main` ani innych gałęzi; nie rebase/cherry-pick/force push, nie uruchamiać FullSharded bez nowej autoryzacji.",
      [("nie mergować do `main` ani innych gałęzi", "TST-039", "partial", "-", "CURRENT_REPO_CONFIRMED", MERGE, ""),
       ("nie rebase/cherry-pick/force push", "-", "none", "-", "CURRENT_REPO_CONFIRMED", CL + ":117-119 (no force push, rebase, amend, squash)", "Cherry-pick is not named in CLAUDE.md."),
       ("nie uruchamiać FullSharded bez nowej autoryzacji", "TST-039", "partial", "-", "CURRENT_REPO_CONFIRMED", PS + ":12; OD-03", "")])
items("S66", "3. Wstrzymane decyzje i zakazy", "non_goal", "prohibited in the audit phase",
      "**MVP:** brak rozszerzenia. Brak nowych funkcji, zmian Android/desktop produktu, nowej biblioteki, kernela, schematu `.forge`, testów lub CI przez tę fazę.",
      [("**MVP:** brak rozszerzenia.", "-", "none", "-", "CURRENT_REPO_CONFIRMED", CL + ":132-133", ""),
       ("Brak nowych funkcji, zmian Android/desktop produktu", "-", "none", "-", "CURRENT_REPO_CONFIRMED", PS + ":10-12", ""),
       ("nowej biblioteki, kernela", "PORT-013;PORT-011", "partial", "-", "CURRENT_REPO_CONFIRMED", LIB, ""),
       ("schematu `.forge`", "TST-017", "partial", "-", "CURRENT_REPO_CONFIRMED", PS + ":10-12 (no testdata change)", ""),
       ("testów lub CI przez tę fazę", "-", "none", "-", "CURRENT_REPO_CONFIRMED", PS + ":10-12", "")])
row("S66", "3. Wstrzymane decyzje i zakazy", "requirement", "allowed output",
    "Powstają wyłącznie dossier i opcjonalnie izolowane dowody diagnostyczne.", "-", "none", "-", "CURRENT_REPO_CONFIRMED", PS + ":10-12", "")
row("S66", "3. Wstrzymane decyzje i zakazy", "gate", "pending coordinator verdict",
    "**Nie ma TECH PASS Council:** dopiero raport Claude, kontrola źródeł i testów, potem koordynatorski verdict i nowy prompt.", "-", "none", "-",
    "UNRESOLVED_AUTHORITY", NOREC + " (no coordinator verdict on FC-R2 in archive or repo)", "The closeout is part of that pending check.")
row("S66", "4. Stan techniczny, zachowany", "status_claim", "preserved state",
    "host 4392/4392 **wg repo**", "TST-003", "exact", "-", "CURRENT_REPO_CONFIRMED", PS + ":13 (FC-R2 executed it fresh)", "")
row("S66", "4. Stan techniczny, zachowany", "status_claim", "preserved state",
    "Nie ma niezależnego fizycznego wyniku OWNER.", "TST-041;MOD-115", "exact", "-", "CURRENT_REPO_CONFIRMED", PHYS, "")
row("S66", "5. Dokumenty", "requirement", "planned doc update",
    "W repo doc `PROJECT_STATUS.md` zostanie dopisany przez Claude po audycie, nie teraz.", "-", "none", "-", "CURRENT_REPO_CONFIRMED",
    PS + ":3-22 (FC-R2 entry present)", "Fulfilled.")

# ---------------------------------------------------------------- S65 (archived)
row("S65", "header", "decision", "binding at date (still standing)",
    "**Następny aktywny krok: OWNER test fizycznego APK i odrębna decyzja A2. Żaden nowy coding prompt nie jest aktywny.**", "TST-041;CAD-089", "partial", "-",
    "CURRENT_REPO_CONFIRMED", PS_NEXT, "")
row("S65", "header", "decision", "binding",
    "nie oznacza to pełnej akceptacji funkcji na telefonie, zgody na integrację ani rozpoczęcia nowego CAD modułu.", "TST-041;TST-039", "partial", "-",
    "CURRENT_REPO_CONFIRMED", PS_NEXT, "")
row("S65", "1. Zweryfikowane fakty", "status_claim", "verified",
    "| HEAD vs C1 | 12 ahead / 0 behind, 22 zmienione ścieżki (w tym dokumentacja i testy) |", "-", "none", "-", "HISTORICAL", PS_HEAD, "")
row("S65", "1. Zweryfikowane fakty", "status_claim", "verified",
    "| DEVICE razem | **126/126** z dwóch rozłącznych pul, ten sam produkt `app/src/main` |", "CAD-112", "partial", "-", "CURRENT_REPO_CONFIRMED", PS + ":42-43", "")
row("S65", "1. Zweryfikowane fakty", "status_claim", "recorded, not re-run",
    "| Geometry oracles | 17 PASS, wg repozytoryjnego zapisu; NIE uruchamiane niezależnie |", "TST-056", "partial",
    PS + ":13 (FC-R2 re-ran incl. 17 oracles)", "SUPERSEDED", PS_HEAD, "")
row("S65", "1. Zweryfikowane fakty", "status_claim", "recorded, not measured on ARM64",
    "| Perf | pojedynczy ray/triangle +2.4–3.0%, CAD scene pick ~bez zmian; wynik z repo `PERF.md`, NIEzmierzony niezależnie ani na ARM64 |",
    "RND-050;RND-051", "exact", "-", "CURRENT_REPO_CONFIRMED", PS + ":43-44", "")
row("S65", "2. Weryfikacja techniczna", "status_claim", "recorded repro numbers",
    "`u+v=1.00000429` poza, double na tych samych danych `u+v=0.999998093` wewnątrz.", "CAD-005", "partial", "-", "UNRESOLVED_AUTHORITY",
    "artifacts/cad-r2-c2-picking-seam/BEFORE_REPRO.md; " + PS + ":31-36 (cause stated without numbers)",
    "Numbers only in stage dossier; candidate for host reproduction (B02-repro diag exists in prior dossier).")
row("S65", "2. Weryfikacja techniczna", "status_claim", "coverage",
    "D19/D20 w DEVICE pokrywają szew, dziurę, krawędź sylwetki, D21 orbitę / confirm, D22 hover **symulowany**.", "CAD-005;CAD-011;UI-033", "partial", "-",
    "CURRENT_REPO_CONFIRMED", PS + ":42-43 (CadRetainedSketchOwnerTest D01..D22)", "D22 hover is simulated (MotionEvent), not hardware.")
row("S65", "2. Weryfikacja techniczna", "status_claim", "limit",
    "Nie dowodzą fizycznego stylusa ani niezawodności na każdym ABI.", "MOD-115;PORT-032", "partial", "-", "CURRENT_REPO_CONFIRMED", PHYS, "")
row("S65", "2. Weryfikacja techniczna", "status_claim", "limit at date",
    "w tym audycie NIE wykonano kompilacji repo ani testów host ponownie.", "TST-003", "partial", PS + ":13 (FC-R2 re-ran host)", "SUPERSEDED", PS_HEAD, "")
row("S65", "2. Weryfikacja techniczna", "status_claim", "known behaviour",
    "Wybór jednej z dwóch rzeczywiście trafionych ścian na wspólnej krawędzi może zależeć od porządku/tie i ABI; to nie jest nowy trwały token.",
    "CAD-005;RND-047", "partial", "-", "UNRESOLVED_AUTHORITY", "artifacts/cad-r2-c2-picking-seam/SUMMARY.md:70-73; " + NOREC,
    "ABI-dependence of the tie is unproven in any record; candidate for a host diagnostic (x86_64 only; ARM64 BLOCKED).")
row("S65", "3. OWNER PHYSICAL TEST GATE", "gate", "awaiting OWNER",
    "Testowany musi być dokładnie APK o SHA256 `262076926e8cc3f90dc70425de4363d3ba3300adfc1998092d581cdffc40be9e`.", "TST-041", "partial", "-",
    "CURRENT_REPO_CONFIRMED", PS_NEXT, "")
row("S65", "3. OWNER PHYSICAL TEST GATE", "gate", "awaiting OWNER",
    "Raport OWNER dla każdego punktu powinien zawierać `PASS / FAIL / NOT TESTED` i urządzenie / Android / sposób wejścia (palec/rysik):",
    "-", "none", "-", "OWNER_DECISION_REQUIRED", NOREC + " for the report format", "Report format is not in repo records (OD-27 points at stage SUMMARY packs).")
for (frag, m, mq, note) in [
    ("Orthographic: dwa tapnięcia w środek dna kieszeni i górnej ściany — szkic na zamierzonej ścianie, nie na world plane za bryłą.", "CAD-005;CAD-011", "exact", ""),
    ("Orbit, obrót ekranu, zachowana semantyka celu; Cancel nie tworzy Undo ani zmian projektu.", "CAD-011", "partial", ""),
    ("Otwór przelotowy: trafienie wewnątrz otworu wskazuje faktycznie widoczną płaszczyznę za nim, poza sylwetką nie ma fałszywego body hit.", "CAD-005;CAD-004", "partial", ""),
    ("Zakrzywiona Cut wall: odmawia podparcia; światowa płaszczyzna z tyłu nie przechwytuje; światowa płaszczyzna przed bryłą może być świadomie wybrana.", "CAD-004;CAD-010", "exact", ""),
    ("History → Sketches: Keep, reuse Add/Cut, Delete unused, Edit Feature/Cancel, Undo/Redo i roundtrip `.forge` na fizycznym urządzeniu.", "CAD-087;CAD-088;CAD-090;CAD-091;CAD-068;CAD-104", "partial", ""),
    ("Największa systemowa czcionka, pion/poziom: kontrolki ≥48 dp, bez nałożenia chipu na History.", "CAD-096;UI-031;UI-076", "partial", "'Largest system font' may exceed the 1.5 verified on emulator."),
    ("Sculpt stroke, Freeform cage selection, zwykłe wybieranie obiektów — brak regresji hit testów; palec + realny rysik (hover jeśli sprzęt obsługuje).", "MOD-115;MOD-072;UI-033", "partial", ""),
    ("Na ARM64 odczuwalna wydajność pod dużą siatką i w Sculpt, bez deklaracji absolutnej liczby FPS bez instrumentacji.", "RND-051;RND-066", "partial", ""),
]:
    row("S65", "3. OWNER PHYSICAL TEST GATE (checklist)", "gate", "awaiting OWNER (PASS/FAIL/NOT TESTED)", frag, m, mq, "-",
        "OWNER_DECISION_REQUIRED", PHYS, ("OD-27 checklist item. " + note).strip())
items("S65", "3. OWNER PHYSICAL TEST GATE (verdict rule)", "decision", "verdict rule",
      "**Reguła verdict:** jakikolwiek FAIL → `OWNER-PHYSICAL-FAIL`, nie merge, wąski prompt korekcyjny.",
      [("jakikolwiek FAIL → `OWNER-PHYSICAL-FAIL`, nie merge, wąski prompt korekcyjny.", "-", "none", "-", "UNRESOLVED_AUTHORITY", NOREC + " (verdict rule)", "Candidate gap: verdict rule only in coordinator state.")])
row("S65", "3. OWNER PHYSICAL TEST GATE (verdict rule)", "decision", "verdict rule",
    "Bez FAIL, ale jakikolwiek NOT TESTED → `OWNER-PHYSICAL-PARTIAL/PENDING`", "-", "none", "-", "UNRESOLVED_AUTHORITY", NOREC, "Candidate gap.")
row("S65", "3. OWNER PHYSICAL TEST GATE (verdict rule)", "decision", "verdict rule",
    "tylko wszystkie wymagane punkty PASS lub odrębnie zaakceptowane N/A → `OWNER-PHYSICAL-FULL-PASS`.", "-", "none", "-", "UNRESOLVED_AUTHORITY", NOREC, "Candidate gap.")
row("S65", "3. OWNER PHYSICAL TEST GATE (verdict rule)", "decision", "binding",
    "Nawet pełny PASS nie jest automatyczną zgodą na integrację `main`.", "TST-039;TST-040", "partial", "-", "CURRENT_REPO_CONFIRMED", MERGE, "")
row("S65", "4. OPEN DESIGN HOLD — A2", "status_claim", "current behaviour",
    "Keep Sketch na XY/XZ/YZ wewnątrz już istniejącego CAD Body jest w tym modelu niedozwolone.", "CAD-089", "exact", "-", "CURRENT_REPO_CONFIRMED", A2CUR, "")
row("S65", "4. OPEN DESIGN HOLD — A2", "decision", "option detail",
    "dodatkowe szkice body-local na płaszczyznach świata, nowa semantyka / `CADB v9`, starszy reader ma jawnie odmawiać;", "CAD-089;DOC-009", "partial", "-",
    "OWNER_DECISION_REQUIRED", A2CUR, "Option 1 detail: older reader must refuse explicitly (consistent with CLAUDE.md version rules).")
row("S65", "4. OPEN DESIGN HOLD — A2", "decision", "option detail",
    "body-owned konstrukcyjne płaszczyzny parametryczne, nowa encja + wersja formatu;", "CAD-089", "partial", "-", "OWNER_DECISION_REQUIRED", A2CUR, "Option 2 detail.")
row("S65", "4. OPEN DESIGN HOLD — A2", "decision", "option detail",
    "project-level sketches, zmiana semantyki dokumentu / Object/History/codec.", "CAD-089", "partial", "-", "OWNER_DECISION_REQUIRED", A2CUR, "Option 3 detail.")
row("S65", "4. OPEN DESIGN HOLD — A2", "decision", "binding",
    "**Żadna opcja nie została zatwierdzona przez OWNER.**", "CAD-089", "exact", "-", "OWNER_DECISION_REQUIRED", A2CUR + "; OD-01", "")
row("S65", "4. OPEN DESIGN HOLD — A2", "decision", "binding",
    "Tymczasowa wcześniejsza akceptacja starszego APK pozwoliła kontynuować prace, ale nie rozstrzyga A2 ani pełnej akceptacji obecnego APK.", "CAD-089;TST-041", "partial", "-",
    "CURRENT_REPO_CONFIRMED", PS + ":4878-4880", "")
row("S65", "4. OPEN DESIGN HOLD — A2", "decision", "binding",
    "Nie uruchamiać implementacji formatu w drodze domyślnej preferencji koordynatora.", "CAD-089", "partial", "-", "CURRENT_REPO_CONFIRMED", A2CUR, "")
row("S65", "5. Następna praca", "decision", "binding",
    "Niech Claude nie samorozpoczyna CADB v9, FullSharded, merge ani nowego etapu funkcji.", "CAD-089;TST-039", "partial", "-", "CURRENT_REPO_CONFIRMED",
    PS_NEXT + "; " + CL + ":132-133", "")
row("S65", "5. Następna praca", "decision", "process rule",
    "Jeśli OWNER zgłosi FAIL, bezpośrednio przygotować prompt korekcyjny zamiast przedłużać docs-only.", "-", "none", "-", "UNRESOLVED_AUTHORITY", NOREC, "Candidate gap (process).")
row("S65", "6. Dokumentacja i provenance", "status_claim", "reference",
    "W repo `PROJECT_STATUS.md` v0.102.0 potwierdza stan TECH PASS/OWNER REVIEW/NO MERGE.", "-", "none", PS + " (current version)", "SUPERSEDED", PS, "")

# ---------------------------------------------------------------- S64 (archived)
row("S64", "header", "status_claim", "binding status at date",
    "**Status obowiązujący:** `CAD-R2-C1 TECH PASS / OWNER PHYSICAL REVIEW REQUIRED / A2 WORLD-PLANE DESIGN HOLD / NO MERGE`.", "-", "none",
    "STATE v4.1.65 / ADDENDUM v4.1.65 (C2 status)", "SUPERSEDED", PS_HEAD, "")
row("S64", "header", "decision", "evidence hierarchy",
    "**Hierarchia źródeł:** GitHub refs i pobrane CI artifacts > repozytoryjne `PROJECT_STATUS.md` i dossier etapu > relacja Claude > wcześniejsze wersje planu.",
    "-", "none", "-", "UNRESOLVED_AUTHORITY", "MANIFEST.md 'Priorytet źródeł' (2026-10-29) ranks PLAN sources differently; " + NOREC,
    "Ranks EVIDENCE sources; MANIFEST ranks PLAN sources (canonical Master > OWNER decisions > repo contracts > old Master > addenda). Not an explicit supersession; flag for OWNER.")
row("S64", "header", "decision", "binding",
    "Nie traktować `TECH PASS` jak pełnego OWNER PASS.", "TST-041", "exact", "-", "CURRENT_REPO_CONFIRMED", PHYS, "")
row("S64", "1. Bieżący etap", "status_claim", "C1 coverage",
    "Nowe scenariusze obejmują font scale 1.0/1.3/1.5, obrót i powrót do aplikacji.", "CAD-096;UI-076", "exact", "-", "CURRENT_REPO_CONFIRMED", "PRODUCT.md:1021", "")
row("S64", "1. Bieżący etap", "status_claim", "C1 provenance",
    "No code copied from SolidCraft.", "-", "none", "-", "UNRESOLVED_AUTHORITY", NOREC, "Candidate gap (provenance not recorded in repo).")
row("S64", "1. Otwarte decyzje / ryzyka", "decision", "binding default",
    "Domyślnie zachować zachowanie i odmowę z `R2_SK_06`, `D18`.", "CAD-089", "exact", "-", "CURRENT_REPO_CONFIRMED",
    A2CUR + "; CadRetainedSketchOwnerTest.java:471 (D18)", "Note: in the repo D18 is the History rotation/background case; the refusal is R2_SK_06.")
row("S64", "1. Otwarte decyzje / ryzyka", "decision", "binding",
    "Opcja 1 wymagałaby osobno zaakceptowanej `CADB v9`, NIE jest domyślnym zakresem.", "CAD-089", "partial", "-", "OWNER_DECISION_REQUIRED", A2CUR, "")
row("S64", "1. Otwarte decyzje / ryzyka", "status_claim", "open at date",
    "`PICK-RAY-TRIANGLE-SEAM`: zdiagnozowany stary problem `pickSceneSnapshot` na wspólnej krawędzi trójkątów przy odległym ortho eye (~250 m)", "CAD-005", "exact",
    "ADDENDUM v4.1.65 (C2 fix 0fa12c9)", "SUPERSEDED", PS_HEAD, "")
row("S64", "1. Otwarte decyzje / ryzyka", "status_claim", "disposition",
    "`NO-REPRO-STABLE` **dotyczy przyczynowości zmian CAD-R2**, nie dowodzi płaskiej wydajności urządzenia.", "-", "none", "-", "CURRENT_REPO_CONFIRMED", PS + ":56-60", "")
items("S64", "1. Otwarte decyzje / ryzyka", "status_claim", "physically unconfirmed",
      "`ARM64-PERF`, fizyczny rysik, hover, GPU, 16KB page, ergonomia: **niepotwierdzone fizycznie**.",
      [("`ARM64-PERF`", "RND-066;RND-051", "partial", "-", "CURRENT_REPO_CONFIRMED", PHYS, ""),
       ("fizyczny rysik", "MOD-115", "exact", "-", "CURRENT_REPO_CONFIRMED", PHYS, ""),
       ("hover", "UI-033;MOD-032", "partial", "-", "CURRENT_REPO_CONFIRMED", PHYS, ""),
       ("GPU", "RND-066", "exact", "-", "CURRENT_REPO_CONFIRMED", PHYS, ""),
       ("16KB page", "PORT-072;RND-066", "partial", "-", "CURRENT_REPO_CONFIRMED", PS + ":3094,4165-4168", ""),
       ("ergonomia", "-", "none", "-", "OWNER_DECISION_REQUIRED", PHYS, "Subjective; only OWNER can judge.")])
row("S64", "1. Otwarte decyzje / ryzyka", "status_claim", "binding fact",
    "CI DEVICE to emulator x86_64, nie telefon.", "TST-041", "exact", "-", "CURRENT_REPO_CONFIRMED", PHYS, "")
row("S64", "1. Otwarte decyzje / ryzyka", "decision", "binding (UX preserved)",
    "`tap None clears aim` to obecne zachowanie ze względów bezpieczeństwa confirm; zachować bez samowolnej zmiany UX.", "CAD-011", "exact", "-",
    "CURRENT_REPO_CONFIRMED", "per CAD-011 (CLAUDE.md MODELING-R1-OWNER-CORRECTION aim/confirm: a refused/missed tap clears the aim)", "")
row("S64", "1. Otwarte decyzje / ryzyka", "decision", "next stage proposal",
    "Kolejny proponowany pojedynczy etap:** `CAD-R2-C2-PICKING-SEAM-CORRECTNESS-R1`", "CAD-005", "partial", "-", "HISTORICAL", PS_HEAD, "")
row("S64", "1. Otwarte decyzje / ryzyka", "decision", "sequencing",
    "Po tym OWNER fizycznie zatwierdza najnowszy APK.", "TST-041", "partial", "-", "CURRENT_REPO_CONFIRMED", PS_NEXT, "")
row("S64", "2. GitHub HEAD", "status_claim", "refs at 2026-10-28",
    "| **`feature/cad-r2-c1-quality-gate-r1`** | **`08406cfbbff471f6e53c576c24b45791f903301c`** |", "-", "none", "-", "HISTORICAL", PS_C1, "")
row("S64", "2. GitHub HEAD", "status_claim", "compare R2→C1",
    "3 product-file locations (`forgeshape_support_chooser.cpp`, `CadExtrudeCanvasView.java`, `EditorWorkspaceView.java`)", "CAD-004;UI-031", "partial", "-",
    "HISTORICAL", PS_C1, "")
row("S64", "3. Zweryfikowane bramki jakości", "status_claim", "C1 evidence",
    "| DEVICE combined | **122/122** 2 oddzielne runy, zgodny product tree | compare i ZIP artifacts |", "-", "none", "STATE v4.1.65 (C2: 126/126)", "SUPERSEDED", PS_C1, "")
row("S64", "4. Granice następnego zadania", "requirement", "C2 bounds",
    "`CAD-R2-C2` może dotyczyć wyłącznie potwierdzonego CPU ray/triangle seam i regresji wyboru ścian/myszy/rysika.", "CAD-005", "partial", "-", "HISTORICAL", PS_HEAD, "")
row("S64", "4. Granice następnego zadania", "requirement", "C2 method",
    "Najpierw BEFORE z próbą odtworzenia oryginalnego błędu, test czerwony, potem minimalna poprawka i pełne testy.", "CAD-005", "partial", "-", "HISTORICAL",
    PS + ":40-41", "")
items("S64", "4. Granice następnego zadania", "non_goal", "prohibited for C2",
      "**Zakazy:** zmiana .forge, nowych ABI/JNI slot bez potrzeby, kernela, renderer rewrite, nowego solvera, Fillet/Chamfer, reorder/suppression, CADB v9, dodatkowych root sketch, kopiowania SolidCraft, zmiany zachowania world-plane z C1, merge/rebase/force push, FullSharded, uznania emulatora za OWNER physical PASS.",
      [("zmiana .forge", "TST-017", "partial", "-", "CURRENT_REPO_CONFIRMED", PS + ":37-38", ""),
       ("nowych ABI/JNI slot bez potrzeby", "-", "none", "-", "HISTORICAL", PS + ":37-38 (no JNI change)", "Stage-scoped."),
       ("kernela", "PORT-011", "partial", "-", "CURRENT_REPO_CONFIRMED", LIB, ""),
       ("renderer rewrite", "-", "none", "-", "CURRENT_REPO_CONFIRMED", CL + ":134-135", ""),
       ("nowego solvera", "CAD-038;CAD-109", "exact", "-", "CURRENT_REPO_CONFIRMED", DONOT, ""),
       ("Fillet/Chamfer", "CAD-109", "exact", "-", "CURRENT_REPO_CONFIRMED", DONOT, ""),
       ("reorder/suppression", "CAD-095", "exact", "-", "CURRENT_REPO_CONFIRMED", DONOT, ""),
       ("CADB v9", "CAD-089", "partial", "-", "CURRENT_REPO_CONFIRMED", A2CUR, ""),
       ("dodatkowych root sketch", "CAD-089", "exact", "-", "CURRENT_REPO_CONFIRMED", A2CUR, ""),
       ("kopiowania SolidCraft", "-", "none", "-", "UNRESOLVED_AUTHORITY", NOREC, "Candidate gap."),
       ("zmiany zachowania world-plane z C1", "CAD-004", "exact", "-", "CURRENT_REPO_CONFIRMED", CL + ":336-340", ""),
       ("merge/rebase/force push", "TST-039", "partial", "-", "CURRENT_REPO_CONFIRMED", CL + ":115-119", ""),
       ("FullSharded", "TST-039", "partial", "-", "CURRENT_REPO_CONFIRMED", MERGE, ""),
       ("uznania emulatora za OWNER physical PASS", "TST-041", "exact", "-", "CURRENT_REPO_CONFIRMED", PHYS, "")])
row("S64", "5. Provenance dokumentacji", "status_claim", "reference",
    "GitHub `PROJECT_STATUS.md` na C1 HEAD zgłasza status v0.101.0", "-", "none", PS + " (current version)", "SUPERSEDED", PS, "")

# ---------------------------------------------------------------- verify + write
trace_ids = set()
with open(TRACE, newline="") as fh:
    for r in csv.DictReader(fh, delimiter="\t"):
        trace_ids.add(r["ID"])

texts = {k: open(os.path.join(SRC, v[0]), encoding="utf-8").read() for k, v in F.items()}
bad = []
for i, r in enumerate(R):
    if r["quote"] not in texts[r["k"]]:
        bad.append(("QUOTE_NOT_VERBATIM", i, r["k"], r["quote"]))
    if len(r["quote"]) > 200:
        bad.append(("QUOTE_TOO_LONG", i, r["k"], len(r["quote"])))
    if r["matches"] not in ("-", ""):
        for m in r["matches"].split(";"):
            if m not in trace_ids:
                bad.append(("UNKNOWN_TRACE_ID", i, r["k"], m))
    if r["auth"] not in ("HISTORICAL", "SUPERSEDED", "CURRENT_REPO_CONFIRMED", "OWNER_DECISION_REQUIRED",
                         "UNKNOWN_CURRENT_PLAN", "UNRESOLVED_AUTHORITY"):
        bad.append(("BAD_AUTH", i, r["auth"]))
    if r["auth"] == "SUPERSEDED" and r["sup"] in ("-", ""):
        bad.append(("SUPERSEDED_WITHOUT_SUCCESSOR", i, r["quote"]))
    if r["mq"] == "none" and r["matches"] not in ("-", ""):
        bad.append(("NONE_WITH_IDS", i, r["quote"]))
    if r["mq"] in ("exact",) and r["matches"] in ("-", ""):
        bad.append(("EXACT_WITHOUT_IDS", i, r["quote"]))

if bad:
    for b in bad:
        print("ERROR", *b)
    sys.exit(1)

clean = lambda s: str(s).replace("\t", " ").replace("\n", " ")
hdr = ["src_id", "source_file", "section", "date", "doc_version", "statement_quote",
       "kind(requirement|decision|gate|non_goal|deferral|status_claim)", "claimed_status_in_source",
       "matches_existing_ids", "match_quality(exact|partial|none)", "later_superseded_by",
       "authority(HISTORICAL|SUPERSEDED|CURRENT_REPO_CONFIRMED|OWNER_DECISION_REQUIRED|UNKNOWN_CURRENT_PLAN|UNRESOLVED_AUTHORITY)",
       "current_repo_evidence", "notes"]
with open(OUT, "w", encoding="utf-8", newline="") as fh:
    fh.write("\t".join(hdr) + "\n")
    for n, r in enumerate(R, 1):
        f, d, v = F[r["k"]]
        fh.write("\t".join(clean(x) for x in [
            "SRC-ADD-%03d" % n, f, r["section"], d, v, r["quote"], r["kind"], r["claimed"],
            r["matches"], r["mq"], r["sup"], r["auth"], r["ev"], r["notes"]]) + "\n")

print("ROWS", len(R))
print("BY_KIND", dict(collections.Counter(r["kind"] for r in R)))
print("BY_AUTHORITY", dict(collections.Counter(r["auth"] for r in R)))
print("BY_MATCH", dict(collections.Counter(r["mq"] for r in R)))
print("BY_SOURCE", dict(collections.Counter(r["k"] for r in R)))
print("QUOTES_VERBATIM", len(R), "/", len(R))
