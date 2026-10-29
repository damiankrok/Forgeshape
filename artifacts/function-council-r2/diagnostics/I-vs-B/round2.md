## Round 2 — objections from seat B and responses

Seat B (Construction / objects / scene / transforms) raised 21 objections. Each
was re-checked on this tree. Commands for the re-checks are listed at the end.
SEAT_I.md and `work/req_I.tsv` were corrected where an objection was conceded.

Totals:
- 14 conceded, in full or in part: 1, 2, 3, 4, 6, 8, 11, 13, 14, 16, 18, 19, 20, 21.
- 0 rebutted. Two concessions carry a counter-point: on 2, the projection's
  arithmetic is shown and still points the same way; on 8, seat B's "4" is
  also short.
- 7 had nothing to answer: 5, 7, 9, 10, 12, 15, 17.
- 3 remain UNRESOLVED. All three need the coordinator, because they touch
  files outside this seat's writable set:
  - 11: provenance of the local guard rerun.
  - 8: the stale "112 in 9 classes" line in `TEST_COVERAGE_MATRIX.md`.
  - 21: the gizmo coverage note in `TEST_COVERAGE_MATRIX.md`.

| # | Objection | Re-check | Response | Status |
| --- | --- | --- | --- | --- |
| 1 | I-01 impact worded too strongly; add the near-cap Construction suites | Host output: PROJECT records 256 with cap 8192, so no check is lost today. Caps confirmed: `forgeshape_jni.cpp:567` MIRROR 128, `:902` mesh 128, `:923` construction 128. Host counts: 62, 91, 100. | CONCEDED. I-01 now reads "latent: 0 headroom, no current loss, overflow would be silent". The three caps are added beside CAMERA. Verdict stays DEFECT-CONFIRMED, P2 (`TST-005`). | Resolved |
| 2 | I-02 verdict, cause and projection overstated | `start()` guard is only `thread.joinable()` (`forgeshape_jni.cpp:2083-2088`). `git show fa0b6b4:…ForgeShapeActivity.java` has the same `stop()` at :582, and that tree's aggregate took 65.65 min (`PROJECT_STATUS.md:420-426`). CLAUDE.md says "run once from `NativeViewport.start()` … never per frame", which the code meets. | CONCEDED. Regraded to PARTIAL (test cost and debug-path latency) plus DOC-DIVERGENCE for `ForgeShapeActivity.java:33-36`. Cause: the per-start re-run is the multiplier, which pre-dates this tree, and suite growth is the cost (CAD_FEATURE 7.5 s on device). Projection arithmetic is now in I-02: 723 × 9.51 s ≈ 114.6 min of suite re-execution alone, about 95 % of the 120-min stop. It overflows if non-suite time exceeds ≈ 0.45 s per test; this run measured 11.3 s per test. It is still E5, because it assumes every test cold-starts the Activity. `SEAT_I.md` and `TST-007` now agree. | Resolved |
| 3 | I-03: cite `CI_CLOUD.md:12` against `:22-23`, and mention the PR route | Read `docs/CI_CLOUD.md:8-26`. Both workflows have `pull_request: [main]`. | CONCEDED. Added to I-03 and `TST-027`. Verdict kept. | Resolved |
| 4 | I-04 misses `ci-device.yml:2` | `.github/workflows/ci-device.yml:2` reads "(23 self-test tokens, …)". | CONCEDED. I-04 now lists six places; new row `TST-077`. | Resolved |
| 5 | I-05 confirmed | — | Nothing to answer. | — |
| 6 | `TST-040` cites the wrong requirement; list the B-domain classes | `PROJECT_STATUS.md:4851-4865` (merge to `main` only after Tier-5) and `:2545` (Tier-5 policy). `TEST_COVERAGE_MATRIX.md:141-176` marks all seven B classes "older". | CONCEDED. `TST-040` source is now the Next Stage Tier-5 gate, horizon H1, P2 kept. The seven classes are listed in I-06 and `TST-040`. | Resolved |
| 7 | I-07 confirmed | — | Nothing to answer. | — |
| 8 | I-08 class count is 4, not 9; add the gizmo | `ModelingOwnerRig` users: 4 test classes. Classes with their OWN decor-root `realGesture`: `CadFillPickR2Test`, `CadMultiFaceOwnerTest`, `CadRevolveOwnerTest`, `CadSketchDraftingOwnerTest`. The "9" counted the rig file itself. `EditorWorkspaceGizmoTest.java:2126,2138` dispatch to the viewport. | CONCEDED IN PART. The correct count is 8 test classes: 4 through the rig and 4 with their own helpers, so seat B's 4 is also short. The re-count is ≈ 106 call sites, replacing 112. The rig's `press()` is `performClick` and unchecked. Gizmo impact added. `SEAT_I.md` and `TST-044` corrected. `TEST_COVERAGE_MATRIX.md:184` still says "112 in 9 classes" and needs the coordinator. | UNRESOLVED (coordinator: matrix line) |
| 9 | I-11 confirmed | — | Nothing to answer. | — |
| 10 | I-09, I-10, I-13, I-14 consistent | — | Nothing to answer. | — |
| 11 | Local release guard PASS not disclosed in `SEAT_I.md` | `run_status.txt`: `guard_exit=2` at 00:54:48. `release_guard.txt` mtime is 00:57 and says PASS. No rerun command or exit code is recorded anywhere this seat can read. | CONCEDED. The positive-claims row now rests on `CI FAST` `40796992679` (E1). The local run is labelled supporting-only, with its provenance incomplete; `TST-014` is annotated. This seat cannot supply the rerun command, because it did not issue it. | UNRESOLVED (coordinator: rerun provenance) |
| 12 | 4392 checks and identical device counts verified | — | Nothing to answer. | — |
| 13 | Order-independence claim overstated | `forgeshape_primitive_selftest.cpp:122-128` reads `constructionScene().hasProject()` and short-circuits. A grep for process accessors over `*_selftest.cpp` finds no other process-scoped read. | CONCEDED. The claim is now restricted to "with an empty process scene". The vacuous pass on a warm restart is noted as untested (E2) in the positive-claims table and `TST-006`. | Resolved |
| 14 | `TST-072` caveat rests on a stale comment | `forgeshape_mesh_selftest.cpp:147,370` and `forgeshape_construction_selftest.cpp:265,334` build local `MeshStore`s. The `forgeshape_jni.cpp:2120-2125` comment says the suites publish into the store. | CONCEDED. `TST-072` now says the only process-scoped read is the read-only check of item 13, and records the comment as DOC-DIVERGENCE, INFO. | Resolved |
| 15 | Goldens and 72/72 parity: no objection | — | Nothing to answer. | — |
| 16 | `TST-036` uses the non-vocabulary verdict `HYPOTHESIS` | Confirmed in the TSV. | CONCEDED. Now UNDETERMINED-SOURCE-GAP, still listed as H-I-1. | Resolved |
| 17 | Horizons correct | — | Nothing to answer. `TST-040` moved H0 → H1 under item 6. | — |
| 18 | Face-supported transform refusal has no test; `TST-065` overclaims | `isFaceSupportedCad` outside self-tests: `forgeshape_body_commands.cpp:205` (Duplicate) and `forgeshape_jni.cpp:7679` (seam). Its androidTest callers (`HomeFlowTest`, `CadA3VisualEvidenceTest`, `OwnerProjectShellSketchSupportTest`, `SpatialSketchTest`) only assert that a body IS face-supported. 0 hits in the Gizmo, ObjectCommands and GlbExport tests. Re-ran seat B's binary `/tmp/claude-0/fc2-scratch/seatB/diag_face_supported`: `gizmo visible=1`, `applyStatus=0 undoDepth 0->1 fingerprintChanged=1`, resolved model still `(0,0,3)` (E1, this response). | CONCEDED. New row `TST-076`: H0 (CAD-A3), no test, DEFECT-CONFIRMED per SEAT_B B-01, P2. `TST-065` annotated. | Resolved |
| 19 | `TST-060` says nothing about face-supported export | `forgeshape_gltf_export.cpp:177-179` reads `body.transform()`. No face-supported case in `forgeshape_gltf_export_selftest.cpp`. The same diagnostic run shows `export body 2 translation=(0,0,0)`, with the dependent drawn at `(0,0,3)`. | CONCEDED. `TST-060` annotated ("bake rule asserted for independent bodies only; face-supported placement untested and defective, B-02"). The missing GLTF_EXPORT check is added as a proposed coverage addition. | Resolved |
| 20 | `TST-066` names the wrong suite | `forgeshape_cad_a3_selftest.cpp:883` asserts `RefusedFaceSupportedCad`; it is not in the SCENE suite. | CONCEDED. Attribution corrected to CAD_A3 (66). | Resolved |
| 21 | B-04 and B-06 gizmo states not in the coverage tables | `ObjectCommandsTest` has 1 test. No test covers the gizmo after a select onto a locked body, after Undo of Unlock, or over a hidden active body. | CONCEDED. The note is added to `TST-065`, with verdicts left as SEAT_B grades them. `TEST_COVERAGE_MATRIX.md` is outside this seat's writable set, so the coordinator must add the same note there. | UNRESOLVED (coordinator: matrix note) |

**Proposed coverage additions (round 2):**
- A host check that a face-supported CAD body refuses a transform write and gizmo activation (`TST-076`).
- A GLTF_EXPORT check that a face-supported dependent exports at its resolved world placement (`TST-060`).
- Device or host checks for the gizmo after a select onto a locked body, after an Undo of Unlock, and over a hidden active body (`TST-065`).

**Re-check commands (round 2):**
- `grep -n "kMax[A-Za-z]*Checks *=" app/src/main/cpp/forgeshape_jni.cpp`, then `sed -n` on lines 560-570, 898-905 and 918-926.
- `git show fa0b6b4:app/src/main/java/com/forgeshape/app/ForgeShapeActivity.java | grep -n "stop()"`
- `sed -n 2080,2128p app/src/main/cpp/forgeshape_jni.cpp`
- `grep -l ModelingOwnerRig app/src/androidTest/java/com/forgeshape/app/*.java`
- `grep -l "touchView\|realGesture" app/src/androidTest/java/com/forgeshape/app/*.java`
- `grep -nE "(^|[^.:a-zA-Z_])(constructionScene|constructionTransform|meshStore|sketchSession)\(\)" app/src/main/cpp/*_selftest.cpp`
- `/tmp/claude-0/fc2-scratch/seatB/diag_face_supported`: seat B's prebuilt binary, run read-only.
- Edits were made by `/tmp/claude-0/fc2-scratch/I-vs-B/edit_tsv.py` and `/tmp/claude-0/fc2-scratch/I-vs-B/edit_seat.py`.
