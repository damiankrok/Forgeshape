# FUNCTION-COUNCIL-R2 — Council deliberation

Tree `70a6c14` (product tree `app/src/main` = `f3db953`). Inputs: `SEAT_A.md` …
`SEAT_J.md` (each with its Round 2 section), `work/req_*.tsv`, `work/delib_*.md`,
`SOURCE_REGISTER.md`. Fresh test and CI run ids belong to `TESTS.md` (the
coordinator's file, not yet written when this was synthesised); run ids quoted
here are the ones the seats cite, and each was checked against the seat text, not
re-derived.

Seat statuses: A PARTIAL, B PARTIAL, C PARTIAL, D PARTIAL, E COMPLETE, F PARTIAL,
G COMPLETE, H PARTIAL, I COMPLETE, J PARTIAL. Every seat shares one gap:
`FORGESHAPE_MASTER_PLAN_v4.1.61.md`, `OWNER_DECISIONS_*`, `MASTER_PLAN_HISTORY_*`
and `PROJECT_STATE` are unavailable (`BEFORE.md` "Authoritative sources"), so
horizons H0/H1/H2 are reconstructed from repo records (**PARTIAL-PLAN-SOURCE-GAP**).

## 0. Adversarial verification — outcome

**`work/VERIFICATION.md` does not exist at synthesis time** (checked:
`ls artifacts/function-council-r2/work/` lists only `delib_*.md` and `req_*.tsv`;
a filesystem search found no `VERIFICATION*.md` newer than `BEFORE.md`).
Consequences, applied throughout this file and its two siblings:

- No finding below is labelled "verified by the adversarial pass". A finding's
  standing is its seat's grade **after Round 2** plus any cross-seat
  re-reproduction recorded in the deliberation files (listed in §3).
- Where a seat's own "Adversarial counterexample attempts" section refuted a
  candidate, that refutation is reported as the seat's (E1/E2 as it states).
- When `VERIFICATION.md` is written, any finding it does not uphold must be
  re-labelled hypothesis/withdrawn here; this file must not be cited as
  verified until then.

Findings withdrawn or narrowed by deliberation (not by verification):

| Item | Change | Where recorded |
| --- | --- | --- |
| B-07 `sceneAddBody` has no `hasProject()` guard | **Withdrawn** as a defect: New Sculpt deliberately calls it at Home inside `beginSessionInitialization` (`EditorWorkspaceView.java:2402-2406`); kept as hypothesis H-B4 (E5) | `delib_E_to_B.md` #3 |
| C-03 Keep Sketch sub-claim | **Withdrawn**: `keepsOnProducer` (`forgeshape_sketch_session.cpp:545-552`) checks the table size | `delib_F_to_C.md` #3 |
| J-05 revolve-axis half | **Withdrawn**: `CadRevolveOwnerTest` taps the axis through the decor view (`:619-636`, `:725-747`), E3 `40782861799` | `delib_C_to_J.md` C-1 |
| J-01 hidden-body edit as a defect | Regraded to **BLOCKED-OWNER** (OD-J1); only the missing solid preview stays PARTIAL P3 | `delib_C_to_J.md` C-3 |
| J-03 create+undo leaves project dirty | Regraded to **BLOCKED-OWNER** (OD-J3) | `delib_C_to_J.md` C-15 |
| G-06 re-encode bytes depend on prior loads | Regraded to **FUTURE-NOT-DUE** (requirement was seat-derived) | `delib_J_to_G.md` #6 |
| E-08, E-10, D-04, D-05, A-07, A-13, B-04 | Regraded from defect to PARTIAL and/or BLOCKED-OWNER | respective delib files |
| H-09, H-12 | Never assigned; no content | `delib_A_to_H.md` O-17 |
| RND-019/037/038/046/051/065 (seat H requirement rows) | Moved to H2 / BLOCKED-OWNER: no repo record states a budget | `delib_A_to_H.md` O-2..O-4, O-9 |

## 1. Round 1 — each seat's headline findings (as they stand after Round 2)

Severity and verdict are the post-Round-2 values. "P" is the seat's ranking;
grades are the seat's.

### Seat A — product, journeys, plan reconstruction
- **A-02 P1 DEFECT-CONFIRMED (E2)** — Recover deletes the checkpoint and marks the
  project persisted (`EditorWorkspaceView.java:5652-5687`), so recovered work is in
  neither slot nor checkpoint. Conflicts with `PRODUCT.md:193-194` and the pinned
  assertion FSR1B-05 (`ProjectAutosaveRecoveryTest.java:321-322`); fix needs OWNER decision 2.
- **A-01 P2 DEFECT-CONFIRMED (E2)** — "Save and continue" before Open Saved Project
  overwrites the slot it then opens (`:2665-2678`, `:5458-5471`).
- **A-03 P2 BLOCKED-OWNER** — the saved slot cannot be reopened from Home (`HomeView.java:47-79`).
- **A-04 P2 DEFECT-CONFIRMED (E1)** — Import GLB accepted while a sketch is open
  (`SEATA_IMPORT_DURING_SKETCH=ACCEPTED`). Merged with D-02, C-02, E-03.
- A-05 P3 (D3), A-06 P3 (empty Save Copy/Export file after recreation, E2+E5),
  A-07 BLOCKED-OWNER, A-08 P3 wrong failure string, A-09 DOC-DIVERGENCE,
  A-10 P3 split, A-11 P3, A-12 DOC-DIVERGENCE (= D-06), A-13 PARTIAL, APP-087 P3 (= D-03/E-07).

### Seat B — Construction, objects, scene, transforms
- **B-02 P1** — merged into E-01 (GLB export of a face-supported body).
- **B-01 P2 DEFECT-CONFIRMED (E1 host + E2)** — a face-supported CAD body accepts a
  transform write and a gizmo; `applyStatus=Applied`, undo 0→1, fingerprint
  changes, drawn model unchanged (`diag_face_supported.out`).
- B-03 P2 DOC-DIVERGENCE (`ARCHITECTURE.md:2606-2608`), B-04 PARTIAL (lock guard
  only at activation; Java refresh mitigates), B-05 P3 (derived names truncated at
  96 bytes), B-06 owner-pending (gizmo over a hidden active body), B-07 withdrawn.

### Seat C — CAD, sketch, parametric
- **C-01 P2 DEFECT-CONFIRMED (E1 domain + E2 JNI)** — navigator Flip / ±90° do not
  move the camera (`forgeshape_jni.cpp:313-319`; `viewFrame()` has no production
  caller). = F-02 = FC-R1 D2.
- **C-02 P2** — import during sketch (= A-04).
- C-03 P3 (Add/Cut offered on a full 16-record sketch table; refusal misnamed),
  C-04 PARTIAL (length out-of-range refused as `ZeroLengthLine`), C-05 INFO three
  stale `CadStatus` comments, C-06 P2 risk (five core sketch device classes have no
  run on this tree after `40800569636` covered three).

### Seat D — Sculpt, Freeform, Surface, representation boundaries
- **D-01 P2 DEFECT-CONFIRMED (E1 `diag_d3.out`)** — Undo after a NotRetained stroke
  lands on a never-existing state and clears `hasEdits` (= FC-R1 D4).
- **D-02 P2 (E1 `diag_d6.out`)** — import during sketch (= A-04).
- D-03 P3 Freeform session survives a load (= E-07, APP-087), D-04 PARTIAL (load
  into Sculpt over a hidden body; recorded debt, = E-09), D-05 PARTIAL (Clear Mask
  drawn then refused, = FC-R1 D6), D-06 DOC-DIVERGENCE (= A-12), D-07 P3 Resume
  Sculpt has no native sketch guard, D-08 PARTIAL (Symmetry ON / level over budget
  drawn then refused), D-09 INFO lock differs per representation, D-10 coverage gaps.

### Seat E — document, I/O, lifecycle, failure containment
- **E-01 P1 DEFECT-CONFIRMED (E1)** — GLB export writes a face-supported body at its
  stored identity transform (`forgeshape_gltf_export.cpp:176-185`) instead of
  `resolveWorldModel`; fixtures `cad_face_sketch_cap_v2`, `_side_v2`, `cad_face_chain_v2`.
- **E-02 P2 (E2 + E3 mechanism `fsr1b04`)** — recovery question can reappear over the
  live project after a recreation (`recoveryResolved` set only when a candidate existed).
- E-03 P2 (= A-04), E-04 P2 (= A-05, FC-R1 D3), E-05 P3 (PlanarFaces body passes
  decode/validate, fails load; E1 `pf_gap`), E-06 P3 (two encodings of one face
  token; E1 `faceidx`), E-07 P3 (= D-03), E-08 PARTIAL + OWNER, E-09 P3 (= D-04),
  E-10 PARTIAL (= FC-R1 D7), E-12 P3 (JNI load cancels sketch/view before a
  post-decode refusal), E-13 P3 (= H-02), E-14 P3 (checkpoint lands after Save), E-11 INFO doc list.

### Seat F — Android UI/UX, accessibility
- **F-01 P2 DEFECT-CONFIRMED (E1 container 43.4 dp + E2)** — controls below 48 dp in width.
- **F-02 P2** — = C-01; names the false-green `SketchUxTest` assertions (`:199-233`).
- **F-03 P2 (E2)** — in the CAD bootstrap, Back discards the sketch even with a panel open (`dismissTopmostSurface` order, `:1177-1184` before `:1201`).
- **F-04 P2 (E2, API 36 / targetSdk 36 only)** — Back during an in-project Revolve backgrounds the app (`hasDismissibleSurface` `:1112-1117` has no revolve term).
- **F-05 P2 (defect E2, parser E1)** — three canvas editors' `DigitsKeyListener("0123456789.-")` drop `,`: "2,5" applies 25.
- F-06 P3, F-07 BLOCKED-OWNER (screen-reader state), F-08 P3 (focus ring 2.48:1),
  F-09 P3 (no 48 dp sweep test; merged with I-08), F-10/F-11 INFO, F-12 P3.

### Seat G — desktop readiness (design only)
- **G-01 P2 DEFECT-CONFIRMED (E1 host under FMA + E2 arm64 asm)** — two self-test
  oracles fail under FP contraction (`history_selftest.cpp:474-480`,
  `cad_feature_selftest.cpp:5392-5394`); arm64 device outcome E5 / OWNER-PHYSICAL-UNVERIFIED.
- **G-02 P2 PARTIAL** — platform-neutrality is not guarded by CI (= I-05 root).
- G-03 P3 DOC-DIVERGENCE (`ARCHITECTURE.md:214`), G-04 P3 (uninitialised floats in
  self-tests, clang `-Wsometimes-uninitialized`), G-05 DOC-DIVERGENCE (Freeform tap
  slop), G-06 FUTURE-NOT-DUE, G-07..G-09 INFO.

### Seat H — renderer, picking, performance, concurrency
- **H-01 P2 DEFECT-CONFIRMED (E2)** — a mid-session renderer stop is not reported
  until the next resume (only `onResume` polls; `ForgeShapeActivity.java:336`).
- H-05 P2 risk (CAD derivation under `g_stateMutex` on UI/render thread; host 275–322 ms
  Finish at 63×63; requirement BLOCKED-OWNER), H-06 P2 risk (per-sample whole-mesh
  publish), H-07 P2 risk / BLOCKED-OWNER (loop never idles), H-08 P2 debt (FC-R1 F-07,
  per-frame face-frame re-derivation), H-02 P3 (= E-13), H-03 P3 (= I-07), H-04
  DOC-DIVERGENCE (D5 listed open), H-10/H-14 P3 risks, H-11 DOC-DIVERGENCE (5 s
  `surfaceDestroyed` bound), H-13 PARTIAL (= I-01), H-15 INFO, H-16 DOC-DIVERGENCE,
  H-17 IMPLEMENTED-UNTESTED.

### Seat I — test, CI, evidence, build integrity
- **I-01 P2 DEFECT-CONFIRMED (latent)** — a device suite silently drops checks past
  its cap; PROJECT is 256/256 (= H-13; caps at `forgeshape_jni.cpp:567, 821, 902, 923`).
- **I-02 P2 PARTIAL + DOC-DIVERGENCE (E1 `40797031704`)** — 25 suites re-run per
  Activity start, ~9.5 s each; 304.4 s of 667 s test time.
- **I-03 P2 DOC-DIVERGENCE** — a task-branch push triggers no CI.
- **I-05 P2 PARTIAL** — CI FAST runs neither the native suites nor corpus verdicts.
- **I-06 P2 PARTIAL** — 54 of 70 device classes have no run on the current product tree.
- **I-08 P2 PARTIAL** — most device touches bypass hit testing (8 classes use decor-root input).
- I-04 P3 stale counts (six places), I-07 P3 (= H-03), I-09..I-11, I-13 P3, I-10 IMPLEMENTED-UNTESTED, I-14 INFO.

### Seat J — adversarial integration, data loss, trust
- J-01 BLOCKED-OWNER (OD-J1) + PARTIAL P3 (no solid preview for a hidden target),
  J-02 P3 (Freeform/Surface length fields seeded unitless), J-03 BLOCKED-OWNER (OD-J3,
  = G-06 root), **J-04 P3 DEFECT-CONFIRMED (E2)** (Save fallback leaves the only copy
  in an unread `.pending`), J-05 P3 narrowed (eight orphan natives; stale
  `CadPlanarFaceRuntimeTest.toggleFace` javadoc).

## 2. Round 2 — objections and responses

Ten directed pairings. Totals are the responders' own tallies.

| File | Objector → responder | Raised | Conceded | Rebutted | Agreed / nothing to answer | Unresolved |
| --- | --- | --- | --- | --- | --- | --- |
| `delib_A_to_H.md` | A → H | 19 | 17 | 0 | 2 | 0 (guard provenance handed to coordinator) |
| `delib_B_to_I.md` | B → I | 21 | 14 | 0 | 7 | 3 (coordinator) |
| `delib_C_to_J.md` | C → J | 21 | 18 | 0 | 3 | 0 |
| `delib_D_to_A.md` | D → A | 24 | 19 (one, #2, with a narrow counter-point) | 0 | 5 | 0 as fact; 5 OWNER decisions open |
| `delib_E_to_B.md` | E → B | 14 | 12 | 0 | 2 | 1 (guard exit code, orchestrator) |
| `delib_F_to_C.md` | F → C | 19 | 18 (one numeric correction) | 1 in part (#6 commit counts) | — | 0 as fact; 3 open for OWNER/run owner |
| `delib_G_to_D.md` | G → D | 19 | 15 | 0 | 4 | 0 (guard discrepancy owned by G) |
| `delib_H_to_E.md` | H → E | 22 | 15 | 0 | 7 | 2 (obj 17 trace; guard exit) |
| `delib_I_to_F.md` | I → F | 18 | 16 | 0 | 2 | 1 (guard provenance, coordinator) |
| `delib_J_to_G.md` | J → G | 14 | 14 (4 as agreements) | 0 | — | 0 |

The pattern is one-sided: **across 191 objections there is no full rebuttal and two partial ones** (F #6 → C, D #2 → A). Most
concessions lowered a grade (E1 claimed on a re-implementation → E2), moved a
horizon (seat-invented requirement → H2/BLOCKED-OWNER), or merged duplicates. Few
objections changed whether a mechanism exists.

### 2.1 Substantive objections by outcome

**Conceded — verdict raised or new finding created**

| Objection | Result |
| --- | --- |
| A O-1 → H-01 | PARTIAL → DEFECT-CONFIRMED (E2); stale javadoc `ForgeShapeActivity.java:363-369` added |
| A O-16 → HYP-H2 | split: "no lifecycle gate on attach" is now E2 fact (`jni.cpp:1852-1856`); the visible effect stays hypothesis |
| B 18 → I | new row TST-076: face-supported transform refusal has no test; seat I re-ran seat B's binary (E1: `applyStatus=0 undoDepth 0->1 fingerprintChanged=1`) |
| B 19 → I | TST-060 annotated: export bake rule tested only for independent bodies |
| C-5 → J | ADV-046 (import during sketch) H1→H0, E2→E1 |
| C-8 → J | ADV-067 cause corrected to the key-listener set (`CadExtrudeCanvasView:644`, `SketchDimensionLabelView:112`, `BodyDimensionLabelsView:127`) → DEFECT-CONFIRMED |
| D 17 → A | new APP-087 DEFECT-CONFIRMED P3: `diag_d2` reproduced the Freeform session surviving a load (`sessionActive=1 firstSel=3`) |
| E 1 → B | B-02 merged into E-01; fix must also refuse an unresolved support (E2; reachability E5) |
| F 15/16 → C | CAD-001 PARTIAL on Back (F-03); new CAD-115 DEFECT-CONFIRMED P2 (F-04) |
| G 2 → D | D-02 raised to E1 (`diag_d6.out`: `status=Ok`, bodies 1→2, undo 0→1, sketch still active) |
| H 8 → E | new E-12 (load cancels sketch/view before a post-decode refusal); "Both stay fail-closed" struck |
| H 9 → E | HYP-E6/E7 (unsynchronised `ProjectCheckpoint` I/O, delete-then-rename fallback) added; DOC-038/041 PARTIAL "single-writer only" |
| H 11/12 → E | new E-13 (= H-02) and E-14 (promoted from H-E1), both P3 E2, timing E5 |
| J 7 → G | PORT-048 PASS → PARTIAL (delete-then-rename keeps no readable file; owning finding J-04) |

**Conceded — verdict lowered, narrowed or re-sourced**

| Objection | Result |
| --- | --- |
| D 1, 3, 5, 6, 7 → A | A-01 P1→P2 (H1), A-03 → BLOCKED-OWNER, A-05 P2→P3, A-06 → P3 E2+E5, A-07 → BLOCKED-OWNER |
| D 10, 11, 13 → A | A-10 split; A-11 narrowed to New Freeform; A-13 narrowed to stale-after-clean-leave |
| A O-2..O-5, O-9 → H | six requirement rows moved to H2/BLOCKED-OWNER (no repo budget) |
| A O-8 → H, B 1 → I | I-01/H-13 restated as "latent, 0 headroom, no current loss" |
| B 2 → I | I-02 regraded PARTIAL + DOC-DIVERGENCE; per-start re-run pre-dates this tree (`fa0b6b4` had the same `stop()`) |
| B 8 → I | decor-root classes re-counted 8 (not 9, not 4), ~106 call sites (not 112) |
| C-3, C-4, C-15 → J | J-01 → BLOCKED-OWNER + PARTIAL; ADV-053 → DOC-DIVERGENCE (Freeform `BodyLocked`); J-03 → BLOCKED-OWNER |
| E 3, 4 → B | B-07 withdrawn; CON-074 → PASS-SOURCE-AND-RECORDED-RUNTIME |
| F 3, 6, 12 → C | C-03 E1→E2 (probe re-implemented the code); C-06 narrowed; four CAD rows regraded |
| G 4, 5, 8 → D | D-04, D-05 → PARTIAL; D-08 scope narrowed |
| H 7, 13 → E | E-08 → PARTIAL + OWNER; E-10 → PARTIAL P3/INFO |
| I 4, 7, 9 → F | F-04 scoped to API 36; UI-073/074 → BLOCKED-OWNER; UI-087 → OUT-OF-SCOPE |
| J 5, 6, 9, 11, 13 → G | G-05 → DOC-DIVERGENCE; G-06 → FUTURE-NOT-DUE; seven PORT rows PASS-REPRODUCED → IMPLEMENTED-UNTESTED (grep-only) |

**Rebutted (in part)**

| Objection | Rebuttal |
| --- | --- |
| F #6 → C (commit counts after the last core-sketch runs) | Seat C conceded C-06 was overtaken by `40800569636` for three classes, but recomputed: `git log --oneline --since=2026-10-06 70a6c14` gives 75 commits / 26 touching `app/src/main`, not seat F's 77 / 28. Attributed to the time-zone boundary of `--since`. No conclusion depends on the difference. |
| D 2 → A (narrow) | A-02's sentence "no test leaves a project after Recover" stands: FSR1B-05 asserts checkpoint retirement but does not background or leave afterwards. |

### 2.2 Items still UNRESOLVED after Round 2

| # | Item | Raised in | Owner of closure |
| --- | --- | --- | --- |
| U-1 | Release-guard provenance: `run_status.txt` records `guard_exit=2` at 00:54:48; `release_guard.txt` (mtime 00:57:29) reads `RELEASE_SELFTEST_GUARD=PASS`; the rerun's command and exit code are recorded nowhere. Seats now rest the PASS on CI FAST `40796992679` (`release-selftest-guard.md` PASS) and label the local file "provenance incomplete". Seat G marked it RESOLVED by timestamp ordering; seats B, E, I, F, D still list it open. | A→H O-18, B→I 11, E→B 5, F→C 12, G→D 19, H→E 18-21, I→F 15, J→G 12 | coordinator (`TESTS.md`) |
| U-2 | Whether `loadProject` ends the support chooser, Dimensions mode and the gizmo. Native `loadProject` does not call `supportChooser().cancel()` (`closeProject` does, `forgeshape_jni.cpp:8976`); the Java post-load path was not traced. | H→E 17 | open trace (UNDETERMINED-SOURCE-GAP) |
| U-3 | `TEST_COVERAGE_MATRIX.md:184` still says "112 in 9 classes" (seat I's corrected count: 8 classes, ~106 call sites); the gizmo coverage note (locked/hidden/undo-of-unlock) is missing from the matrix. | B→I 8, 21 | coordinator |
| U-4 | Count of refused-corpus status names: seat E counts 22 of 28 `InvalidSemanticValue` (`fc2/host_corpus_verdicts.txt`), seat H's text had 24. | H→E 15 | coordinator (re-read the one file) |

## 3. Cross-seat re-reproductions made during Round 2 (E1)

These are the only claims that a second seat executed on this tree:

| Claim | Binary / command | Re-run by | Output recorded |
| --- | --- | --- | --- |
| B-01 face-supported transform write | `/tmp/claude-0/fc2-scratch/seatB/diag_face_supported` | seat I | `gizmo visible=1`, `applyStatus=0 undoDepth 0->1 fingerprintChanged=1`, model `(0,0,3)`; export body 2 `(0,0,0)` |
| Import during sketch | `diag_d6` (seat D) after seat A's `diag_import_during_sketch` | seats D and A | `status=Ok`, bodies 1→2, active 1→2, undo 0→1, sketch active |
| Freeform session survives load | `/tmp/claude-0/fc2-scratch/D/diag_d2` | seat A | `sessionActive=1 … firstSel=3`, pushPull Ok after load |
| 16 KB page alignment | `readelf -lW` on the release APK libs | seat G (reproducing J) | every LOAD 0x4000 on arm64-v8a and x86_64 |
| No `Uri` in the domain | `grep -rnE "android/net/Uri|ContentResolver|content://" app/src/main/cpp` | seat A | 0 files |

## 4. De-duplication by root cause

One backlog entry per root cause. "Members" are the seat ids that describe it.

| RC | Root cause | Members | Synthesis verdict / severity | Horizon (source) |
| --- | --- | --- | --- | --- |
| RC-01 | Consumers of placement read `body.transform()` instead of `resolveWorldModel`, and nothing reads `isFaceSupportedCad` except Duplicate | E-01 (=B-02), B-01, B-03, TST-076, TST-060, DOC-067, CON-025 | DEFECT-CONFIRMED; export P1 (E1), transform/gizmo P2 (E1 host) | H0/H1 (CLAUDE.md CAD-A3 "NOT independently transformable"; Export "exports EVERY body") |
| RC-02 | Import's guards check Sculpt only; no sketch/chooser term in JNI or chrome | A-04, C-02, D-02, E-03, ADV-046, CON-001/038/077 (xref) | DEFECT-CONFIRMED P2 (E1 domain, E2 JNI/Java) | H0 (CLAUDE.md CAD-R0-A1A2) |
| RC-03 | `beginSketchView` frames on `frame()`, never `viewFrame()` | C-01, F-02, UI-018, FC-R1 D2 | DEFECT-CONFIRMED P2 (E1+E2); test `SketchUxTest` asserts state bits only | H1 (CLAUDE.md SKETCH-UX-R1 C) |
| RC-04 | Session memory held in View fields is lost on Activity recreation | A-05 (=E-04, FC-R1 D3), A-06, E-02 | DEFECT-CONFIRMED; E-02 P2, A-05 P3 (A) / P2 (E) — see D-3, A-06 P3 | H0 (UI-PREF-R1, Autosave rules) |
| RC-05 | Checkpoint/slot writers are not coordinated: two locks for fingerprint and bytes, unsynchronised write/clear, delete-then-rename fallback | E-13 (=H-02), E-14, J-04, HYP-E6/E7, PORT-048 | DEFECT-CONFIRMED P3 (E2; timing E5) | H0 (CLAUDE.md Autosave) |
| RC-06 | Persistence POLICY around Save / Recover / Discard / Open | A-01, A-02, A-03, A-13, E-08, A-07 | A-02 P1 and A-01 P2 DEFECT-CONFIRMED (E2); A-03, A-07, E-08 BLOCKED-OWNER; A-13 PARTIAL | H0/H1, but A-02's fix contradicts FSR1B-05 and PRODUCT.md:193-194 → OWNER |
| RC-07 | Project establishment (load / close) resets a hand-written, incomplete, mis-ordered list of process sessions | E-07 (=D-03, APP-087), E-12, H-A1, E-10 (D7), U-2, E-09 (=D-04) | E-07, E-12 DEFECT-CONFIRMED P3; E-10 PARTIAL; E-09 recorded debt | H0 ("a failed load changes nothing at all") |
| RC-08 | Back routing does not follow innermost-first in every phase | F-03, F-04 (=CAD-115), A-10, UI-083 | F-03, F-04 DEFECT-CONFIRMED P2 (E2, not reproduced); A-10 split | H0/H1 (APP-H1 "Back is one step"); F-03 conflicts with PRODUCT.md:196-198 → OWNER |
| RC-09 | Decimal comma dropped by three key listeners | F-05, ADV-067, CAD-025, CAD-051/055/075 | DEFECT-CONFIRMED P2 (defect E2, parser E1) | H1 |
| RC-10 | "A control that cannot succeed is not drawn" not met where availability omits a refusal condition | D-05 (FC-R1 D6), C-03, D-08 | D-05, D-08 PARTIAL P3; C-03 DEFECT-CONFIRMED P3 | H0 |
| RC-11 | `SculptHistory::record` keeps the undo stack across a NotRetained stroke | D-01 (FC-R1 D4) | DEFECT-CONFIRMED P2 (E1) | H0 (ARCH-OWNER-12) |
| RC-12 | Renderer RestartRequired is polled only on resume | H-01, DOC-077b, HYP-H2 | DEFECT-CONFIRMED P2 (E2) | H0 (GPU rule) |
| RC-13 | Native evidence channel is lossy or narrow: device result caps, 1023-char log line, per-start re-run, no host suites in CI FAST, FP-contraction-fragile oracles, release never executed | I-01 (=H-13), H-03 (=I-07), I-02, I-05 (=G-02), G-01, G-04, I-10, I-14 | mixed: I-01 DEFECT (latent) P2; H-03 DEFECT P3; G-01 DEFECT P2; I-02/I-05 PARTIAL P2 | H0/H1 |
| RC-14 | Device evidence is older than the tree and mostly bypasses hit testing | I-06, C-06, D (MOD-114), I-08 (=F-09), J-05 | PARTIAL P2 (coverage, not product) | H1 (Tier-5 gate, `PROJECT_STATUS.md` Next Stage) |
| RC-15 | Synchronous heavy work on UI/render thread under `g_stateMutex`; continuous redraw | H-05 (FC-R1 I-4), H-06, H-07, H-08 (F-07), H-15 | risks P2; requirement rows BLOCKED-OWNER (no budget recorded) | H2 until a budget exists |
| RC-16 | Codec admits non-canonical or unloadable documents | E-05, E-06 | DEFECT-CONFIRMED P3 (E1) | H0 (".forge is all-or-nothing"; one solid one encoding) |
| RC-17 | Hidden / locked semantics differ per representation and per tool | J-01, B-04, B-06, D-09, ADV-053 | BLOCKED-OWNER / PARTIAL / DOC-DIVERGENCE | H1 pending OWNER |
| RC-18 | Unit policy (display unit, px vs density units) | J-02, G-05 | J-02 DEFECT-CONFIRMED P3; G-05 DOC-DIVERGENCE | H1 |
| RC-19 | Allocator high-water mark in the fingerprint and bytes | J-03, G-06 | BLOCKED-OWNER / FUTURE-NOT-DUE | H2 pending OD-J3 |
| RC-20 | Touch-target width below 48 dp | F-01, CAD-024 | DEFECT-CONFIRMED P2 | H0 (48 dp rule) |
| RC-21 | Documentation drift | I-04, A-09, E-11, H-04, H-11, H-16, C-05, G-03, D-06 (=A-12), B-03, B-05 comment | DOC-DIVERGENCE — itemised in `CONTRACT_CONFLICTS.md` | — |

Singletons not merged: A-08, A-11, B-05, D-07, F-06, F-07, F-08, F-10 (=G-09),
F-11, F-12, H-10, H-14, H-17, I-09, I-11, I-13.

## 5. FINAL DISAGREEMENTS (not resolved by this synthesis)

No fictitious consensus is claimed; these remain open.

| # | Disagreement | Positions | Why unresolved |
| --- | --- | --- | --- |
| D-1 | Release-guard provenance (U-1) | G: resolved by mtime ordering. B, D, E, F, I: open until the rerun's command and exit are recorded. All agree CI FAST `40796992679` shows PASS. | Only the coordinator ran the rerun. |
| D-2 | Recover semantics (A-02) | A, D: P1 defect against CLAUDE.md "Autosave protects work". The shipped text (`PRODUCT.md:193-194`) and the pinned test FSR1B-05 say recovered work is not asked about and the checkpoint is retired. | Fixing it inverts a pinned assertion; OWNER decision 2. |
| D-3 | Severity of the lost "saved" baseline (D3) | Seat A: P3 (fail-safe: an extra question, never a loss). Seat E: P2 (E-04). Neither seat objected to the other's grade in Round 2. | Severity scale is seat-local; both agree on the mechanism (E2). |
| D-4 | Grade of import-during-sketch | Seat E's E-03 and debt table still say "E2, runtime not reproduced"; seats A and D reproduced it at E1. | Seat E's section predates the cross-seat runs. Synthesis uses E1 (domain) + E2 (JNI/Java). |
| D-5 | Bootstrap Back with a panel open (F-03) | Seat F: DEFECT P2 against the innermost-first rule (`ForgeShapeActivity.java:68-72`). The code comment (`EditorWorkspaceView.java:1150-1154`) orders the bootstrap deliberately, and `PRODUCT.md:196-198` describes Back leaving a New-CAD sketch for Home. | Two written rules point different ways; OWNER decision. |
| D-6 | Post-fix behaviour of C-01 | Seats C and F agree on the defect. After `viewFrame()` is used, Back to Sketch and Edit Sketch would reinstall the flipped view, because view state is reset only in `begin()`/`cancel()` (`forgeshape_sketch_session.cpp:225,270`), while CLAUDE.md CAD-UX-S1-C1 says those paths reinstall "the exact aligned view". | OWNER statement needed. |
| D-7 | Continuous rendering (H-07) and UI-thread derivation (H-05) | Seat H: P2 risks with measurements. Seat A (O-2, O-4): no repo record sets a budget, so the requirement is BLOCKED-OWNER. FC-R1 recorded the same disagreement (its §4 item 3). | No budget in any repo source. |
| D-8 | D7 / E-10 | FC-R1 and `PROJECT_STATUS.md:4249-4250` list it as a defect. Seat E (after H obj 13): PARTIAL, no reachable wrong outcome (all five `closeProject` callers are shell paths after the dirty guard). | Whether a defensive guard asymmetry is a defect is a policy question. |
| D-9 | JNI export count | `BEFORE.md:36` says 276; seat G (`SEAT_G.md:178`) and a unique-symbol grep in this synthesis (`grep -o "Java_com_forgeshape_[A-Za-z0-9_]*" forgeshape_jni.cpp | sort -u | wc -l` → 275) give 275. | Counting method differs; not material to any finding. |
| D-10 | Kept sketches and the 16-slot table (C-03) | Seat C fixes the offer; the OWNER question is whether kept sketches should share the table with feature sketches at all. | OWNER. |

## 6. OWNER decisions surfaced by the deliberation (collected, not resolved)

From the seats' "Owner decisions needed" after Round 2: Home access to the saved
slot (A-03); Recover semantics vs FSR1B-05 (A-02); Save offered before Open Saved
Project (A-01); an open sketch as unsaved work (A-07); Back inside in-project
modes and in the bootstrap (A-10, F-03); Discard-then-cancelled-Open protection
(E-08); post-fix navigator view on Back to Sketch (C-01); kept-sketch table sharing
(C-03); hidden-body editing and Freeform `BodyLocked` (OD-J1); allocator in the
fingerprint (OD-J3 = G-06); unit policy for tap slop (G-05); a pinned
`-ffp-contract` (G-01); a CI trigger for task branches (I-03); frame pacing and
derivation budgets (H-05, H-07); screen-reader selected state (F-07); the Sculpt
status-line layout (F-06).
