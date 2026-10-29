# FUNCTION-COUNCIL-R2 — Debt recheck

Old items come from `artifacts/function-council-r1/COUNCIL_FINDINGS.md` (tree
`a1b50f2`). Each is re-checked on `70a6c14` (`app/src/main` = `f3db953`). Primary
seat record: `SEAT_E.md` "Debt recheck" (D1..D7, import during sketch); other seats
cited per row. Source lines marked **(synth)** were re-read for this file with
`sed -n` / `grep` on this tree; nothing was executed for this file beyond those
greps. Adversarial verification is recorded in `work/VERIFICATION.md` (three
lenses per finding; 30 of 38 P0–P2 findings panel-verified, all 30 survived; 10 of
12 strongest positive claims judged overstated); this file was re-checked against
it (see the note under §1). Fresh device runs on this product tree (emulator only,
25/25 startup tokens in order) are listed in `TESTS.md`: CI DEVICE `40797031704`,
`40800569636`, `40805804385`; evidence under `evidence/device-run-<id>/`.

Status vocabulary: **CLOSED** (fixed in code, with proof), **CONFIRMED-OPEN**
(mechanism still present), **PARTIAL** (narrowed, mitigated, or only part still
true), **UNDETERMINED** (not decidable from what this audit ran or read).

## 1. FC-R1 defects D1..D7

| Old | Item | Status | Current evidence (file:line) | Test proof | Backlog entry (one per root cause) |
| --- | --- | --- | --- | --- | --- |
| D1 | Import GLB reachable and unguarded in Sculpt | **CLOSED** | `forgeshape_jni.cpp:8592` refuses `RefusedInSculpt`; chrome `EditorWorkspaceView.java:2823` (`showImportAvailable(!sculpting)`), `:5837/5856/5884` | `ImportGlbSculptGuardTest` d1_02..04 ran fresh **3/3** in CI DEVICE `40800569636` (77/77, E1 emulator; `evidence/device-run-40800569636/instrumentation-junit.xml`, 0 failures); earlier CI DEVICE `36643101662` (E3, FUNCTION-COUNCIL-C1-D1); GLTF_IMPORT host suite 189/0 in this audit's run (E1) | none. Its sibling (import during a SKETCH) is open, see §2 — not a regression of D1 |
| D2 | Navigator Flip / ±90° do not move the camera | **CONFIRMED-OPEN** | `beginSketchView` reads `sketchSession().frame()` (`forgeshape_jni.cpp:318-319` **(synth)**); `SketchSession::viewFrame()` (`forgeshape_sketch_session.cpp:648`) has no production caller | seat C probe P1 (E1): `frame()_unchanged_view_after_flip=1 … viewFrame()_differs=1`. `SketchUxTest` (`:199-233`) reads only state bits, so its green run in `40800569636` does not cover it (F-02) | RC-03 (C-01 = F-02) |
| D3 | A palette change forgets that the project was saved | **CONFIRMED-OPEN, wider** | `persistedFingerprint` / `everPersisted` are View fields (`EditorWorkspaceView.java:271-272`), not in `EditorUiState.carryAcrossRecreation`; `requestTheme` → `recreate()` (`ForgeShapeActivity.java:268-280`); also any configuration change missing from `AndroidManifest.xml:31` (`uiMode`, `fontScale`, `locale`, `keyboard`, `navigation`) | E2 only; seat A states the existing test is blind to it | RC-04 (A-05 = E-04); same root as E-02, A-06 |
| D4 | Undo after an over-cap stroke lands on a never-existing state | **CONFIRMED-OPEN** | `SculptHistory::record` clears `redo_` then returns `NotRetained` without clearing `undo_` (`forgeshape_sculpt_history.cpp:91-97` **(synth)**) | seat D `diag_d3.out` (E1): `undo_lands_on_a_state_that_existed=0`, `hasEdits=0` after Undo. Existing `SCUNDO_23_an_oversized_stroke_is_refused_by_name` asserts NotRetained only with an empty undo stack (`forgeshape_sculpt_selftest.cpp:1777-1797`) and is misnamed (the stroke is applied) | RC-11 (D-01) |
| D5 | Sketch overlay stale after a pinch zoom | **CLOSED** | `SketchSession::overlay` bumps `overlayRevision_` on a camera-only rebuild (`forgeshape_sketch_session.cpp:3768-3779`, commit `5da5d85`) | host check `CADFC1_S4_a_a_zoom_rebuild_advances_the_overlay_revision`, SKETCH_UX 118/0 in this audit's host run (E1) | none. `PROJECT_STATUS.md:382` and `:4245-4246` still list it open → `CONTRACT_CONFLICTS.md` CC-26 (not a regression) |
| D6 | Clear Mask drawn and then refused | **PARTIAL (mechanism CONFIRMED-OPEN; impact narrowed)** | `canClearMask` has no entry-size test (`forgeshape_sculpt.cpp:1687-1690` **(synth)**). Round 2 (seat G → D #5): reachable only for a mask over ~87,371 vertices (`n·12 + 128` B > 1 MiB) or after eviction | E2 only; no test drives an over-cap Clear Mask | RC-10 (D-05) |
| D7 | `closeProject` does not refuse an open Construction edit, while load does | **PARTIAL** | `forgeshape_jni.cpp:8968-8990` (`closeProject` clears the history, `ConstructionHistory::clear` sets `editOpen_ = false`, `forgeshape_history.cpp:221-227`); load refuses at `forgeshape_project_state.cpp:250`. All five callers are shell paths after the dirty guard (`EditorWorkspaceView.java:2420, 2446, 2580, 2641, 2721`) | `FSR1A_13_a_load_during_an_open_edit_is_refused` (`forgeshape_project_selftest.cpp:2490`) covers the load side only (E1 via PROJECT 256/0) | RC-07 (E-10). Whether this asymmetry is a defect is a recorded disagreement (`COUNCIL_DELIBERATION.md` D-8) |

**Cross-check against `work/VERIFICATION.md`.** No disposition above is
contradicted by the panel. The open items' members were upheld: D2 (C-01, F-02,
3/3), D3 (A-05, E-04, 3/3), D4 (D-01, 3/3). D6 (D-05) and D7 (E-10) are not among
the 30 panel-verified findings, so they keep their seat grade and are
not panel-verified. The two CLOSED items rest on fresh E1 runs (D1: `40800569636`;
D5: host SKETCH_UX), not on a positive claim the panel judged overstated.

## 2. Debts named in the task

### 2.1 Import during an open sketch — **CONFIRMED-OPEN**

- JNI: `importGlbDurable` asks only `inSculptMode()` (`forgeshape_jni.cpp:8586-8604`).
  Domain: `commitImportedGlbScene` guards only `editInProgress`
  (`forgeshape_import_commit.cpp:61-63`), records a `ScopedConstructionEdit` (`:150`)
  and sets the active body (`:175`). Chrome: `EditorWorkspaceView.java:2823`; the
  mark is withdrawn only in the bootstrap (`GlobalToolbarView.java:685`), so the
  drawer's Import row is reachable inside an in-project sketch.
- Contrast: `sceneAddBody` refuses `in_sketch` (`forgeshape_jni.cpp:3246-3265`).
- Proof: E1 twice — seat A `diag_import_during_sketch` (`commit=Ok bodies 1→2
  active 1→2 undo_depth=1`, `SEATA_IMPORT_DURING_SKETCH=ACCEPTED`), seat D
  `diag_d6.out` (`status=Ok`, sketch still active). Panel-verified: A-04, C-02,
  D-02 and E-03 each upheld 3/3 (`work/VERIFICATION.md`). No device test drives
  it: `ImportGlbSculptGuardTest`, green in `40800569636`, covers Sculpt only.
- FC-R1 recorded this as debt in `FUNCTION-COUNCIL-C1-D1`; it is the same
  mechanism as then, so **not a regression**. One backlog entry: RC-02 (members
  A-04, C-02, D-02, E-03, ADV-046).

### 2.2 Project-establishment reset paths — **CONFIRMED-OPEN (partially)**

FC-R1 I-1 (seat C): "five project routes reset five different hand-written
lists". On this tree **(synth, `forgeshape_jni.cpp:8462-8480` and `:8968-8990`)**:

| Process session | `loadProject` (JNI) | `closeProject` (JNI) |
| --- | --- | --- |
| `g_grabbing`, `g_strokePending` | reset | reset |
| `sketchSession()` | cancelled + `endSketchView()` **before** `loadProjectDocument` can refuse | cancelled; `endSketchView()` |
| `supportChooser()` | **not cancelled** | cancelled |
| `sculptSession()` stroke / mode | via `loadProjectDocument` | `cancelStroke()`, `enterConstruction()` |
| `gizmoSession()` | not deactivated in the wrapper | `setActive(false)` |
| `g_selection` gesture | not reset | `resetGesture()` |
| `constructionHistory()` | replaced by `loadProjectDocument` | `clear()` |
| `freeformEditSession()` | **not ended** | **not ended** |
| Dimensions session | not ended (Java path untraced) | not ended (Java path untraced) |

Consequences proven so far:
- Freeform session and selection survive a load — E1 (`freeform_load`,
  `diag_d2`): `selection=[6]` on a cage never selected. E-07 = D-03 = APP-087, P3.
- A refused load has already cancelled the sketch and moved the camera — E2
  (E-12, P3). The post-decode refusals are `forgeshape_project_state.cpp:250-262`
  and the E-05 class.
- Chooser / Dimensions / gizmo after a load: **UNDETERMINED** (Round 2 U-2: the
  Java post-load `dismissPrimarySurfacesExcept(null)` path was not traced).
- Load into Sculpt over a hidden active body (`forgeshape_project_state.cpp:421-425`):
  CONFIRMED-OPEN recorded debt (`PROJECT_STATUS.md:4263-4269`), E1 `hidden_sculpt`
  (E-09 = D-04). Only a crafted file reaches it.

FC-R1's proposed "one reset-every-session function" does not exist. One backlog
entry: RC-07. Not a regression: none of these were fixed and re-broken.

### 2.3 JNI exports — **PARTIAL**

| FC-R1 item | Now | Evidence |
| --- | --- | --- |
| Six exports nothing calls (`sculptUndo`, `sculptRedo`, `sculptRedoAvailable`, `supportChooserSelect`, `supportChooserConfirm`, `sketchToggleRegion`) | **5 still uncalled**; `sketchToggleRegion` is now called by tests only (`CadFillPickR2Test.java:935`, `CadPlanarFaceRuntimeTest.java:529`) | **(synth)** `grep -rnw <name> app/src/main/java` (excluding the `native` declaration) → 0 for all six; `app/src/androidTest` → `sculptUndo` hits are local variables / a javadoc only (`ImportGlbSculptGuardTest.java:259`, `SculptUndoTest.java:114`) |
| New orphans since FC-R1 | `freeformSelect`, `freeformClearSelection`, `sketchToggleSelectEntity` | same grep: 0 product, 0 test. Seat J lists the eight orphans (`SEAT_J.md` J-05) |
| Two dead helpers | **both still present** — `signedAreaTwice(const std::vector<SketchPoint>&)` (`forgeshape_sketch_region.cpp:50`, no caller in that TU; the `forgeshape_cad_revolve.cpp:153` overload is a different function) and `pointStrictlyInside` (`forgeshape_sketch.cpp:235`, 1 occurrence: its definition) | **(synth)** `grep -rnw` |
| Release-exported debug readers / preview seam behind `NDEBUG` | **PARTIAL**: 13 `debug*` exports remain in the release `.so` (seat I `llvm-readelf`), each with an `#ifdef NDEBUG` stub body (four inspected, e.g. `debugMeshCommand` returns `JNI_FALSE`, `forgeshape_jni.cpp:2267-2270`); seat J: no-op bodies at `:7967-7972`, `:9083` | release guard: self-test symbols/strings 0 on both release ABIs (CI FAST `40796992679`, E1; local file provenance incomplete — `COUNCIL_DELIBERATION.md` U-1). The guard does not probe the debug seams (`ci-release-selftest-guard.sh:11-17`) |
| Export count | 275 unique `Java_com_forgeshape_*` symbols **(synth)**; `BEFORE.md:36` says 276 | `COUNCIL_DELIBERATION.md` D-9 |

`PROJECT_STATUS.md:382-383` ("Six dead exports and 18 unguarded test-only exports
still ship") is a historical audit record; the current numbers are above.
One backlog entry: "remove or cover the eight orphan natives and two dead
helpers" (J-05). No user-visible effect; P3.

### 2.4 UI-thread evaluation (FC-R1 I-4, seat E) — **CONFIRMED-OPEN (as a risk)**

- FC-R1: the first evaluation after a drag sample runs on the UI thread under
  `g_stateMutex`; `PERF_NOTES.md` §3 says the render thread does it.
- Now **(synth)**: `evaluateCandidate()` is called from `forgeshape_jni.cpp:1733`
  (`applyCadOperationPreviewLocked`, render thread) and from `:5098`, `:5366`,
  `:6230`, `:6445` (JNI entry points the UI thread calls). Latest-only caching
  means whichever thread asks first pays. `artifacts/cad-vertical-slice-r1/PERF_NOTES.md:56`
  still states "The render thread calls `evaluateCandidate()` at most once per frame".
- `sketchFinish` (`forgeshape_jni.cpp:4400-4420`) runs synchronously on the UI
  thread. Seat H (E1, host x86 `-O1`): `finish()` alone 275–322 ms at 63×63 / 3969
  faces; `evaluateCandidate` all faces 25.5–25.7 ms; camera-only overlay rebuild
  4.2–4.5 ms per frame under the lock. Device `finish_us` (125 placements +
  Finish, debug SwiftShader) 4,227,606 µs in `40797031704`.
- Status: mechanism CONFIRMED-OPEN (E2 + E1 host). Requirement BLOCKED-OWNER:
  no repo record sets a UI- or render-thread budget (`delib_A_to_H.md` O-2).
  Release-ARM timing OWNER-PHYSICAL-UNVERIFIED. One backlog entry: RC-15 (H-05).

### 2.5 Test isolation (FC-R1 I-1, seat F) — **PARTIAL**

- `WorkspaceTestSupport.resetToBaselineConstruction` (`:79-118` **(synth)**) now
  closes a project with no Construction body, cancels a sketch and the support
  chooser, dismisses the recovery prompt and enters Construction. It still cannot
  un-freeze a sculpted body (its own javadoc, `:73-78`) and does not close a
  project that has a Construction body plus other state (e.g. Freeform session,
  hidden or locked bodies).
- The FC-R1 "close and reopen by default" direction was not adopted.
- `SpatialSketchTest` latent order dependence: still recorded
  (`artifacts/sel-out-r1/PREEXISTING_SHARD5_FAILURE.md`; `TEST_COVERAGE_MATRIX.md:128`),
  not re-run on this tree — **UNDETERMINED** whether it still manifests.
- Native suites: seat I ran the 25 suites in reverse order on the host → identical
  counts, 0 failures (E1, `SEAT_I.md:315-317`). Caveat (B → I #13): PRIMITIVE reads
  `constructionScene().hasProject()` and short-circuits
  (`forgeshape_primitive_selftest.cpp:122-128`), so order independence holds for an
  empty process scene; a warm-restart vacuous pass is untested (E2).
- New since FC-R1: suites re-run on every Activity start (I-02, E1
  `40797031704`: 32 starts, 304.4 s of 667 s), which the device evidence couples
  to test count.
- One backlog entry: TST-049 / RC-14 (P3); I-02 under RC-13.

## 3. Other FC-R1 "recorded debts" and side claims

| FC-R1 item | Status | Evidence |
| --- | --- | --- |
| Load path can reopen a hidden sculpted body into Sculpt | **CONFIRMED-OPEN** (recorded debt) | E-09 / D-04, E1 `hidden_sculpt`; `PROJECT_STATUS.md:4263-4269` |
| `resolveWorldModel` re-extracts profiles per frame for face-supported bodies (F-07) | **CONFIRMED-OPEN** (recorded debt) | SEAT_H H-08 (E2 + E3); `PROJECT_STATUS.md:4679` "held as debt" |
| Whole-mesh sculpt republish | **CONFIRMED-OPEN** (risk, BLOCKED-OWNER budget) | SEAT_H H-06: `forgeshape_jni.cpp:10036-10040` → `publishSculptRepresentation` (`:1170-1180`); per-frame rebuild gated at `forgeshape_renderer.cpp:1119-1122` |
| Render loop never rests (FC-R1 I-5) | **CONFIRMED-OPEN** (BLOCKED-OWNER) | SEAT_H H-07: wait predicate includes `running` (`forgeshape_jni.cpp:1822-1826`) |
| Task-branch push triggers no CI (FC-R1 I-7) | **CONFIRMED-OPEN** | `ci-fast.yml:12-17`, `ci-device.yml:10-15` vs CLAUDE.md:114; SEAT_I I-03. See `CONTRACT_CONFLICTS.md` CC-12 |
| Native suites never run in CI FAST (FC-R1 I-7) | **CONFIRMED-OPEN** | SEAT_I I-05, SEAT_G G-02 (`grep -rn host-native .github` → 0) |
| `DATA_PACKAGE_SPEC.md:1124` calls the CADB v5 digest self-test UNVERIFIED | **CLOSED** | **(synth)** `grep -n UNVERIFIED DATA_PACKAGE_SPEC.md` → 0 lines; `:1788` names `CADVS_IO_22` and `CADV6_P11` as asserting the digests |
| Extrude overlay reads `metersPerPixel` at the world origin (`PROJECT_STATUS.md:378-381`) | **CLOSED in code** | **(synth)** render loop calls `session.extrudeViewFacts(...)` "at its own base anchor rather than at the origin" (`forgeshape_jni.cpp:1984-1992`); the HUD reads the same `extrudeViewFacts` (`:5122-5124`). The PROJECT_STATUS sentence is a dated audit record (FABLE-CAD audit of `509de02`) → `CONTRACT_CONFLICTS.md` CC-28 |
| `forgeshape_jni.cpp` size (FC-R1 I-3: 8087 lines) | grew to 10,337 lines; `EditorWorkspaceView.java` 5460 → 6,655 **(synth `wc -l`)** | structure debt, no verdict |
| `CADB` versions added by hand in ~7 files (FC-R1 I-6) | unchanged in approach; now v1..v8 plus `FRFM`/`SURF` v1 | CLAUDE.md fixture list; no seat recorded a defect from it |

## 4. Guard against duplicate backlog entries and false regressions

- **No FC-R1 item regressed.** D1 and D5 are closed with tests; D2, D3, D4, D6, D7
  are the same mechanisms at moved line numbers. Import-during-sketch is the
  pre-existing sibling of D1, recorded as debt in FUNCTION-COUNCIL-C1-D1, not a
  reopening of D1.
- **Do not file separately**: A-04 / C-02 / D-02 / E-03 (one entry, RC-02);
  A-05 / E-04 / D3 (RC-04); C-01 / F-02 / D2 (RC-03); D-01 / D4 (RC-11);
  D-05 / D6 (RC-10); E-10 / D7 (RC-07); E-07 / D-03 / APP-087 (RC-07);
  E-09 / D-04 (recorded debt, RC-07); H-03 / I-07; H-13 / I-01; H-02 / E-13;
  E-01 / B-02; A-12 / D-06; I-05 / G-02; F-09 / I-08.
- **Do not file as defects**: H-05, H-06, H-07 (no budget recorded,
  BLOCKED-OWNER); G-06 / J-03 (OD-J3); J-01 (OD-J1); E-08 (OWNER); A-03, A-07
  (OWNER); B-07 (withdrawn). The panel upheld the MECHANISMS of H-05, H-06
  (2/3), A-03 (2/3; regraded P2 BLOCKED-OWNER) and A-07 (2/3)
  (`work/VERIFICATION.md`); what stays undecided is the owner budget or policy,
  so they are risks / owner decisions, not refuted.
- **Doc-only corrections** (D5 listed open, `metersPerPixel` sentence, stale
  counts) belong to one documentation task, not to the product backlog; see
  `CONTRACT_CONFLICTS.md`.
