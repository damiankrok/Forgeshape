## Round 2 — objections from seat G and responses

Re-verified on the audited tree (`app/src/main` @ `70a6c14`). New diagnostic this round:
`/tmp/claude-0/fc2-scratch/D/diag_d6.cpp` → `diag_d6.out`, built exactly as the other seat-D
diagnostics (`g++ -std=c++17 -O1 -I$CPP -I$CPP/third_party/manifold/include -DMANIFOLD_PAR=-1
-DMANIFOLD_NO_IOSTREAM -DMANIFOLD_NO_FILESYSTEM diag_d6.cpp $OBJ/*.o .../kernel/libmanifold.a -lpthread`).
Row edits to `work/req_D.tsv` were applied by `/tmp/claude-0/fc2-scratch/D/round2_edit.py` (the TSV is
now authoritative; `gen_req.py` reflects round 1 only).

| # | Objection | Re-check | Response | Changes | Status |
| --- | --- | --- | --- | --- | --- |
| 1 | D-01 keep DEFECT-CONFIRMED P2; `SCUNDO_23_an_oversized_stroke_is_refused_by_name` misnamed | `forgeshape_sculpt_selftest.cpp:1777-1797` asserts `NotRetained` with `undoDepth()==0` before; the stroke is applied | CONCEDE (wording) | D-01 fix note says the stroke is APPLIED and NotRetained, check name misleading; MOD-020 tests column annotated. Verdict/severity unchanged | RESOLVED |
| 2 | D-02 needs E1 or lower severity | `diag_d6.out`: `SketchSession::begin(XY)` active, `commitImportedGlbScene` behind the JNI's Sculpt-only guard (`jni:8594`) → `status=Ok`, bodies 1→2, active 1→2, undo 0→1, sketch still active; `RESULT ...=1` | CONCEDE (reproduced). Evidence raised to E1 (domain) + E2 (guards). P2 kept, and the text now states plainly that it rests on the rule violation plus the reproduced side effects (body created, active switched, Construction step recorded under an open sketch), not on demonstrated data loss | D-02, MOD-049 | RESOLVED |
| 3 | D-03 "every New Project → Freeform starts at ObjectId 1" is wrong | `forgeshape_scene.cpp:83-86` `closeProject` clears bodies and active id only; `forgeshape_project_state.cpp:410-415` only pushes the allocator forward | CONCEDE | "Ordinary case" sentence replaced: ids start at 1 only for the first project in a process; realistic cases are reopening the just-saved slot/file, or the first project of a process against a saved Freeform-first project; cross-ref G-06. Verdict/severity unchanged | RESOLVED |
| 4 | D-04/MOD-042 is recorded debt, not an H1 rule | `CLAUDE.md` Stage027 scopes GUARD-2 to Start / Resume Sculpt; `PROJECT_STATUS.md:4263-4269` records the load path as "not in Stage027's scope" | CONCEDE | D-04 and MOD-042 → PARTIAL, P3, source = PROJECT_STATUS Technical Debt, expected behaviour rewritten; E1 reproducer kept; not counted as a new defect | RESOLVED |
| 5 | D-05/MOD-014 → PARTIAL; ARCHITECTURE.md misquoted; "cannot be removed" overstated | `ARCHITECTURE.md:3678-3679` does say "minus the size test"; payload = n·12 + 128 B (`sculpt_history.cpp:15-25`) → ~87,371 vertices; retained mask strokes stay undoable within 32 entries / 4 MiB | CONCEDE | D-05 and MOD-014 → PARTIAL P3; expected behaviour rewritten to quote ARCHITECTURE.md and SCULPT-FCM-R1 correctly; impact restated with its precondition (NotRetained mask stroke or evicted entries); MOD-015 severity text likewise | RESOLVED |
| 6 | D-06 DOC-DIVERGENCE P3 stands | — | AGREE | none | RESOLVED |
| 7 | D-07 PARTIAL P3 stands | — | AGREE | none | RESOLVED |
| 8 | D-08 too broad; fix is owner/UX | Turning a plane OFF never fails; `freeform.cpp:553` refuses only `derived > kMaxFreeformDerivedQuads` (131072, declared at `forgeshape_freeform.h:178` — G's `freeform_symmetry.h:178` is a path slip, same constant) | CONCEDE | D-08 title/scope restated ("symmetry ON over an asymmetric cage, and a level over the budget, are drawn and then refused"); MOD-079 expected behaviour and owner_decision (owner/UX) updated; PARTIAL P3 kept | RESOLVED |
| 9 | D-09/MOD-067 BLOCKED-OWNER | — | AGREE | none | RESOLVED |
| 10 | D-10 coverage gaps confirmed | — | AGREE | none | RESOLVED |
| 11 | Positive claim 4 over-graded for Freeform/Surface | Only `C1_CH_*` (CAD curved wall) exercise the chooser occlusion | CONCEDE | Claim 4: E1 for the CAD curved wall only, E2 for Freeform/Surface; MOD-064 split verdict (Freeform/Surface IMPLEMENTED-UNTESTED, J-D6) | RESOLVED |
| 12 | Positive claim 5 cites wrong tests | `forgeshape_scene_selftest.cpp:907-910` `obj018a_10_the_copy_has_an_empty_sculpt_history`, `..._the_sources_sculpt_history_is_untouched` exist; no check asserts the copy's mask | CONCEDE | Claim 5 cites `obj018a_10` (E1) for history; mask half E2 / IMPLEMENTED-UNTESTED in MOD-057 | RESOLVED |
| 13 | MOD-086/MOD-109 overclaim | `diag_d5` built only an open Freeform Plane and an open extruded Surface | CONCEDE | Both rows: PASS-REPRODUCED for "open bodies export, double-sided" only; stored level and closed/thickened single-sided are E2, IMPLEMENTED-UNTESTED | RESOLVED |
| 14 | MOD-114 contradicts itself | Row status "stale", open question UNDETERMINED | CONCEDE | MOD-114 → PARTIAL, P3 (evidence age; current-tree device result UNDETERMINED) | RESOLVED |
| 15 | MOD-007 mixes horizons | `PROJECT_STATUS.md:4113-4115` STYLUS-G1 BLOCKED-ENV, physical hardware only | CONCEDE | MOD-007 keeps the H2 pressure/tilt half (FUTURE-NOT-DUE, source SCULPT-FCM-R1 "Not this stage"); new MOD-115 = physical stylus gate, H1, BLOCKED-OWNER / OWNER-PHYSICAL-UNVERIFIED | RESOLVED |
| 16 | MOD-091/MOD-111 call host timings "interactive" | Timings are x86_64 host; open500_l4 49.6 ms > 16.7 ms frame | CONCEDE | Both rows: "bounded host timings measured; not gated"; device interactivity OWNER-PHYSICAL-UNVERIFIED | RESOLVED |
| 17 | MOD-087 adds NURBS | `CLAUDE.md` bans "T-Spline"/"SubD" to the user; "general NURBS" appears only in a "Not this stage" list | CONCEDE | NURBS dropped from MOD-087's requirement and improvement | RESOLVED |
| 18 | Add G-05 (mixed tap/brush units) cross-reference | `freeform_session.h:45` (units), `selection.h:37`, `sketch_session.h:210` (px), `sculpt.h:152-154` (px) | CONCEDE | MOD-006, MOD-072, MOD-073 dependencies note G-05 as UNDETERMINED-SOURCE-GAP (owner/UX unit policy); no defect added | RESOLVED |
| 19 | No contradiction; mention `guard_exit=2` | `run_status.txt` records `guard_exit=2` while `release_guard.txt` ends `RELEASE_SELFTEST_GUARD=PASS` | CONCEDE | Sources line notes the discrepancy as unresolved (seat G's); no seat-D conclusion rests on the guard result | RESOLVED (the guard discrepancy itself stays UNRESOLVED, owned by seat G) |

Summary: 15 conceded (1 — wording only, verdict kept — and 2, 3, 4, 5, 8, 11, 12, 13, 14, 15, 16,
17, 18, 19), 0 rebutted, 4 agreed with no change (6, 7, 9, 10).
Open item outside seat D: the `guard_exit=2` vs `RELEASE_SELFTEST_GUARD=PASS` discrepancy.
