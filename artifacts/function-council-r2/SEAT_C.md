# FUNCTION-COUNCIL-R2 — Seat C: CAD / sketch / parametric modeling

Audited product tree: `app/src/main` at `70a6c14f703bfda2ab689b855126c1d12a541e41`
(audit branch HEAD `9cfc3f5` adds only `artifacts/function-council-r2/BEFORE.md`).
This seat did research only. It modified nothing under `app/`, `testdata/`, `scripts/` or
`.github/`, no Gradle file and none of the owner documents, and it did not run Gradle.
Scratch diagnostics live in `/tmp/claude-0/fc2-scratch/seatC/` and are never built into the product.

## Scope

- World-plane and planar-face sketch entry: the support chooser, `TopoRef`,
  `CadFaceToken`, the lineage token, aim/confirm, nearest-target ownership,
  Cut floor/wall supports, curved and Revolve refusals, and the far-orthographic
  seam pick (the C2 fix).
- The sketch session: Line, Polyline, Rectangle, Circle, Arc and Spline; the
  Construction role; snap; Driving and Reference dimensions;
  Trim/Extend/Offset/Mirror; multi-select; the line dimension; the orientation
  navigator.
- Regions with holes, the union rule, the planar arrangement, point-touch
  splitting, selection caps and the named refusals.
- Extrude New Body/Add/Cut, One Side/Symmetric/Two Sides, Flip, feature edit,
  upstream regeneration, and multiple features and bodies.
- Revolve New Body (axis, angle, direction) and its limits.
- The retained sketch table (`CAD-R2`): Keep Sketch, reuse by id, safe delete,
  the History surface, staged edit with Fix/Cancel, and Undo/Redo.
- Preview == commit, codec round trip, determinism and capacity errors.
- Recorded only, not solved: the A2 world-plane retained sketch
  (`BLOCKED-DESIGN-DECISION-R2-WORLD-PLANE-RETAINED-SKETCH`).

## Sources read (exact paths, revision 70a6c14 product tree)

- `CLAUDE.md` rules: CAD-R0-A1A2; CAD-A3/ARCH-OWNER-13; APP-H1/CAD-A3-C1; UI-OWNER-46; MODELING-R1-OWNER-CORRECTION;
  SKETCH-UX-R1 C/D/E/F; CAD-UX-S1/-C1; CAD-EXT-R1; CAD-VERTICAL-SLICE-R1 (regions, chain, preview, HUD);
  CAD-FOUNDATION-C1; CAD-PLANAR-FACE-PF-S1; CAD-V6-S1/-C1; CAD-V6-S2 and its corrections; CAD-R2/-C1;
  CAD-V6-REVOLVE-NEWBODY-E2E-R1; CAD-SKETCH-DRAFTING-TOOLKIT-E2E-R1; MODELING-FOUNDATIONS-R1 A; `.forge` CADB v1..v8.
- `PRODUCT.md` §§ "Starting a project" (131-202, navigator claim at 153-160), "Sketching a CAD Body" (592-900),
  "Drafting inside a sketch" (901-962), "How a body was built — the History" (963-1028), "Not yet implemented" (2209+).
- `PROJECT_STATUS.md` §§ Owner Decision Baseline (2486-2606, UI-OWNER-04 detail), Known Issues (4018-4215, navigator
  item at ~4106), Next Stage (4851-4880).
- `ARCHITECTURE.md` §§ CAD domain 2424-3160 (navigator claim at 2632-2635), row at 175.
- `DATA_PACKAGE_SPEC.md` §§ 7b-7i (CADB v1..v8).
- `artifacts/cad-r2-c1-quality-gate/SUMMARY.md`, `artifacts/cad-r2-c2-picking-seam/SUMMARY.md` and `TESTS.md`,
  `artifacts/cad-r2-c1-quality-gate/TESTS.md`, `artifacts/cad-vertical-slice-r1/TEST_EVIDENCE.md`,
  `artifacts/cad-vertical-slice-r1/FULLSHARDED_C3.md`, `artifacts/cad-ux-s1/SUMMARY.md`,
  `artifacts/cad-v6-s2-owner-feedback-multiface/SUMMARY.md`; `artifacts/function-council-r1/COUNCIL_FINDINGS.md`
  (older tree `a1b50f2`, used as leads only: D2 and D5 re-checked below); `artifacts/function-council-r2/SEAT_A.md`
  (A-04 cross-reference).
- Fresh outputs of this audit (read only, not produced by this seat):
  `/tmp/claude-0/-home-user-Forgeshape/2f5454c6-63b1-52b8-b92c-5f8b69c420c6/scratchpad/fc2/host_selftests.txt`
  (`HOST_SELFTESTS_OK (4392 checks, 0 failed)`; CAD 155, CAD_A3 66, SKETCH_UX 118, CAD_FEATURE 638 incl.
  `CAD_R2_ORACLE_PASS=17`), `host_corpus_verdicts.txt` (`CORPUS_FIXTURES 72`), `run_status.txt`
  (`host_exit=0 corpus_verdicts_exit=0 gradle_exit=0 guard_exit=2`), `release_guard.txt`
  (`RELEASE_SELFTEST_GUARD=PASS`). `guard_exit=2` and the PASS line disagree. Seat B read an
  ANDROID_HOME error from the same file, which means it was rewritten. This seat does not settle
  that discrepancy. JVM XML in `app/build/test-results/testDebugUnitTest/` dated 2026-10-29T00:57Z:
  CadHud3dPresentationTest 6, CadHudPresentationTest 22, CadHudTouchAttributionTest 3,
  CadRevolvePresentationTest 8, SketchBrowserPresentationTest 17, SketchChromePolicyTest 4,
  SketchDraftingPresentationTest 20, with 0 failures and 0 errors.

## Inspected code paths (file:line, functions)

| Area | Path |
| --- | --- |
| Status codes | `forgeshape_sketch.h:57-334` `CadStatus` (93, `kCadStatusCount`), `:350-388` bounds; Java `NativeViewport.CAD_*`, `CadStatusMessages.java:20-147` |
| Session lifecycle | `forgeshape_sketch_session.cpp:175` `begin`, `:147` `beginOnFace`, `:235` `cancel`, `:280` `beginEdit`, `:285` `beginEditFeature`, `:350` `checkDependentsKeepTheirFaces`, `:378` `commitEdit`, `:433` `beginReuseSketch`, `:495` `beginEditSketch`, `:556-565` `canKeepSketch`, `:567-641` `commitSketch` |
| View | `:648-668` `viewFrame`, `:670` `rotateView`, `:685` `setViewFlipped`, `:693` `setSupportPlane`; JNI `forgeshape_jni.cpp:313-319` `beginSketchView` (reads `frame()`), `:6567` `sketchViewState`, `:6592` `sketchSetSupportPlane`, `:6611` `sketchSetViewFlipped`, `:6630` `sketchRotateView` |
| Line dimension | `:789` `selectedLineDimensionAnchor`, `:808-845` `applyLineLength`; JNI `:6700`; Java `EditorWorkspaceView.java:4081` |
| Finish / regions / faces | `:2177` `finish`, `:2329` `toggleRegion`, `:2372` `toggleRegionAt`, `:2435` `unchosenStatus`, `:2549` `togglePlanarFace`, `:2591/2605` clear/select-all; `forgeshape_sketch_region.cpp:211` `extractSketchRegions`, `:303-379` `validateSelectedRegions`; `forgeshape_sketch_arrangement.cpp:312` `collectSourceEdges` (`:395-397` UnsupportedCurve only for an unknown kind), `:1680` `deriveSketchArrangement`, `:2362` partition, `:2439` `mergePlanarFaces`; `forgeshape_cad_body.cpp:998` `decideSketchSelectionMode`, `:1016` `mergePlanarFaceSelection` |
| Extrude / operations / preview | `:2675` `applyExtrudeFeature`, `:2733` `setExtrudeExtent`, `:2782` `flipExtrudeDirection`, `:2807-2903` `candidateState`, `:2905-2934` `operationAvailable`, `:2987-3030` `evaluateCandidate`, `:3048-3094` `commit`, `:3096-3141` `commitIntoTarget`; `forgeshape_cad_body.cpp:232` `extrudeFeatureCanonical`, `:261` `extrudeFeatureWithExtent`, `:533-593` `addCadSketchRecord`/`appendCadLaterFeatureWithSketch`, `:1180` `validateCadBodyState`, `:1379` `regenerateCadBody`, `:1591-1618` `CadBody::applyState` |
| Revolve | `:3147` `revolveAvailable`, `:3162` `beginRevolve`, `:3214` `setRevolveAxis`, `:3277` `setRevolveAngle`, `:3468` `onRevolveTouch`; `forgeshape_cad_body.cpp:102` `resolveRevolveAxis`, `:136-148` `validateRevolveParameters`; `forgeshape_cad_revolve.cpp:215` `revolveSegmentCount`, `:305` `appendRevolveSolid` |
| Faces / support | `forgeshape_cad_face.cpp:107` `resolveCadFace`, `:111` `cadFeatureTopologySignature`; `forgeshape_support_chooser.cpp:26` `refreshChosenSupport`, `:146` `resolve`; `forgeshape_scene.cpp:215` `validateCadFaceSupport` |
| Drafting | `forgeshape_sketch_drafting.cpp:155,184,241,481,691,809,835,865,922`; `forgeshape_sketch_dimension.cpp:164,188,309,355` |
| Retained sketches / History | `forgeshape_cad_sketch_inventory.cpp:72,91,128,217`; `forgeshape_cad_timeline.cpp:63`; JNI `:7355,7452,7479,7528,7569,7594,5344` |
| Import guard (cross-seat) | `forgeshape_jni.cpp:8560-8600` `importGlbDurable` (Sculpt guard only); `EditorWorkspaceView.java:2823` `showImportAvailable(!sculpting)` |
| Overlay revision (FC-R1 D5) | `forgeshape_sketch_session.cpp:3757-3782` `overlay` |

## Test coverage of the domain (what is actually asserted)

**Host native suites.** The 2026-10-29 run of this audit (E1) passed every suite listed here:
- **CAD (155).** Checks: `forgeshape_cad_selftest.cpp` CADR0_* (119) and CADEXT_* (33).
- **CAD_A3 (66).** CADA3_* (44) and CADA3_BOOT_* (13).
- **SKETCH_UX (118).** CADUXR1_* (48), CADUXS1_* (41), CADUXS1C1_* (13), CADFC1_E/S3/S4. These check:
  - the viewFrame algebra and right-handedness;
  - `setSupportPlane` refusals;
  - the line length;
  - curve validation;
  - the extrude manipulator scale, drag and cancel;
  - `cadFeatureViewPose`.
- **CAD_FEATURE (638).** It aggregates:
  - `forgeshape_cad_feature_selftest.cpp`: CADVS_SES/OPS/REG/IO/EXT, CADFC2_PF, FILL_R2, CADV6S2_SES.
  - Arrangement: PFS1_* (39).
  - Fill: FILL_*, FILL_MODE_* (30).
  - Multiface: MF_* (19).
  - Revolve: REV_*, REV_FMT_* (38).
  - Drafting: DR_DIM/FMT/SNAP/MIR/TRIM/OFF/SES/EXT (93).
  - Timeline: PAR_* (16).
  - CAD-R2:
    - R2_SK, R2_PF, R2_SUP, R2_HIST and R2_DATA.
    - C1_CH_01..06: the chooser's nearest-target rule.
    - PICK_C2_01..17: the seam pick.
  - CAD-V6: CADV6_P* (43).
  - The 17 R2_OR geometry oracles: volume and face-identity oracles over Add/Cut chains.
  - Golden digests: `FORGESHAPE_CAD_GOLDEN_SHA256_V5`.
  - Performance: `FORGESHAPE_CAD_FEATURE_PERFORMANCE`, for example `arrangement_cap_us=20425/21122 cells=3969`.

**Corpus.** `host_corpus_verdicts.txt` (E1) gives the following verdicts for the 39 `cad_*` and `mixed_cad*` fixtures:
- Every valid fixture is `Ok` with `regenerated=n`.
- The refusal fixtures give `InvalidSemanticValue`. `cad_dependency_cycle_v2` gives `UnresolvedReference`.

Parity with the PowerShell builder was not run locally because there is no pwsh. CI FAST
`40787465884` covers it (E3).

**JVM (fresh XML, E1).** 80 CAD/sketch presentation tests, as listed under Sources. They check:
- the HUD scale band, the 48 dp floor and value formatting;
- dock homography and touch claiming;
- revolve label placement;
- browser rows and act availability;
- chrome policy in Ready;
- drafting label stand-off and priority hiding.

**androidTest (E3 historic CI DEVICE, except the three rows run in this audit, E1 emulator).** Classes and the runs that recorded them:

| Class | Tests | Last recorded run |
| --- | --- | --- |
| CadRetainedSketchOwnerTest | D01..D22 (22 tests) | `40776968165` on the C2 tree |
| CadMultiFaceOwnerTest | 6 | C2 `40782861799` |
| CadFillPickR2Test | 6 | C2 `40782861799` |
| CadHud3dOwnerTest | 5 | C2 `40782861799` |
| CadRevolveOwnerTest | 9 | C2 `40782861799` |
| CadSketchDraftingOwnerTest | 14 | C2 `40782861799` |
| CadParametricHistoryOwnerTest | 3 | C2 `40782861799` |
| OwnerProjectShellSketchSupportTest | 5 | C2 `40782861799` |
| SketchUxTest | 14 | `40800569636` on `70a6c14` (this audit, E1 emulator) |
| SpatialSketchTest | 8 | `36465122847` (cad-vertical-slice-r1) |
| SketchExtrudeTest | 9 | cad-v6-s2-owner-feedback-multiface, 2026-10-05 |
| CadVerticalSliceTest | 9 | `40800569636` on `70a6c14` (this audit, E1 emulator) |
| CadCanvasExtrudeTest | 11 | cad-v6-s2-owner-feedback-multiface, 2026-10-05 |
| CadExtrudeExtentTest | 5 | `40800569636` on `70a6c14` (this audit, E1 emulator) |
| CadPlanarFaceRuntimeTest | 6 | cad-v6-s2-owner-feedback-multiface, 2026-10-05 |
| CadPlanarFaceOwnerCorrectionTest | 5 | cad-v6-s2-owner-feedback-multiface, 2026-10-05 |

CadRetainedSketchOwnerTest asserts the chooser ownership (D04, D15, D16), font-scale chrome
(D15-D18), the A2 refusal (D18) and the seam (D19-D22).

`SketchUxTest.cadUxR1_*` (`SketchUxTest.java:198-232`) asserts only the native `sketchViewState`
bits for the navigator. It never checks the camera.

No device evidence for this domain exists on this tree beyond the C2 union. No Tier-5 aggregate
has run on it.

**Seat C diagnostics (E1, this audit).** Source is `/tmp/claude-0/fc2-scratch/seatC/probe.cpp` and
the output is in `probe_output.txt`. Commands:

```
cd /tmp/claude-0/fc2-scratch/seatC; R=/home/user/Forgeshape; CPP=$R/app/src/main/cpp; OBJ=$R/build/host-selftests/obj
g++ -std=c++17 -O1 -I$CPP -I$CPP/third_party/manifold/include -DMANIFOLD_PAR=-1 -DMANIFOLD_NO_IOSTREAM -DMANIFOLD_NO_FILESYSTEM -c probe.cpp -o probe.o
g++ probe.o $OBJ/*.o $R/build/host-selftests/kernel/libmanifold.a -o probe && ./probe > probe_output.txt
./status_map_check.sh     # CadStatus native order vs Java NativeViewport.CAD_* constants
```

They link against the audit's fresh host objects of `70a6c14` (`build/host-selftests/obj`, 2026-10-29 00:53).

## Confirmed findings

**C-01: P2. The orientation navigator's Flip and ±90° do not move the camera. DEFECT-CONFIRMED, plus DOC-DIVERGENCE.**

- **Where:** `forgeshape_jni.cpp:313-319`. `beginSketchView` frames on `sketchSession().frame()`, the
  authoring frame.
- **Why the controls have no effect:** `sketchSetViewFlipped` (`:6611-6624`) and `sketchRotateView`
  (`:6630-6648`) toggle the state and then call `beginSketchView`. `SketchSession::viewFrame`
  (`forgeshape_sketch_session.cpp:648`) has no production caller. Its only callers are in
  `forgeshape_sketch_ux_selftest.cpp`. No renderer or overlay consumer reads `viewFlipped_` or
  `viewQuarterTurns_` (grep: only `sketchViewState` reports them).
- **Documents that say otherwise:**
  - `ARCHITECTURE.md:2633-2634`: "`beginSketchView` frames the camera on the derived one".
  - `PRODUCT.md:155-157`: "one control flips to look at the plane from the other side; and −90° and +90° turn the drawing on screen".
  - `PROJECT_STATUS.md` Known Issues lists it as "OBSERVED, NOT INVESTIGATED".
- **Prior record:** FUNCTION-COUNCIL-R1 D2, still present.
- **Minimal reproduction (E1, probe P1):** begin an XY session, frame the camera through `frame()`,
  set `setViewFlipped(true)`, frame through `frame()` again, then `rotateView(1)`. Output:
  `frame()_unchanged_view_after_flip=1 after_flip_and_roll=1 viewFrame()_differs=1`. The camera that
  JNI installs is byte-identical before and after both controls, while the view frame they are
  meant to produce differs.
- **Impact:**
  - Two drawn controls do nothing visible, though the navigator label changes. This breaks "a
    control that looks like it works and does not is worse than an absent one".
  - The device test cannot catch it.
- **Grade:** E1 (domain) + E2 (JNI).
- **Fix:** in `beginSketchView`, use `sketchSession().viewFrame()`. Then add a device or host
  assertion of the camera view matrix after Flip and after +90. *(Round 2 correction.)* The claim
  that "every caller keeps working" is E5. There are 11 `beginSketchView` callers, and the view state
  is reset only in `begin()` and `cancel()` (`forgeshape_sketch_session.cpp:225,270`). So after Flip,
  Back to Sketch and Edit Sketch would reinstall the flipped view rather than the exact aligned one.
  This needs an OWNER statement and a test.
- **Cross-references (Round 2):** merged with Seat F's F-02. Also see F-01: the Flip chip is about
  36.2 dp wide. The green `SketchUxTest` in `40800569636` does not touch C-01.

**C-02: P2. Import GLB is accepted while a sketch is open. DEFECT-CONFIRMED. This is Seat A's A-04, seen from the CAD rule.**

- **The rule:** while a sketch is drawn, "creation ... refused below JNI and withdrawn above it" (CLAUDE.md CAD-R0-A1A2).
- **What the code does:**
  - `EditorWorkspaceView.java:2823` withdraws the Import row only for Sculpt.
  - `importGlbDurable` (`forgeshape_jni.cpp:8594-8600`) guards only `inSculptMode()`.
  - By contrast, `sceneAddBody` (`:3246`), `sceneDeleteBody` (`:3317`), `objectCommandsBlockedByMode`
    (`:3409`), `historyUndo` (`:8009`), load (`:8473`) and closeProject (`:8977`) all handle an active sketch.
- **Reproduction (E1, Seat A):** `diag_import_during_sketch` printed `SEATA_IMPORT_DURING_SKETCH=ACCEPTED`,
  `bodies 1→2, active 1→2, undo_depth=1`.
- **CAD consequence (E2):** a Construction step is recorded and the active body switches mid-sketch.
  The staged commit itself uses `targetBodyId_`, not the active body (`commitIntoTarget:3098`), so a
  face-sketch Add/Cut still lands on its producer. This last point is a HYPOTHESIS for the UI path.
- **Verdict:** DEFECT-CONFIRMED.
- **Reachable UI path (Round 2, E2):** open a sketch inside a project, tap the ForgeShape mark, then Import.
  The mark is GONE only in the bootstrap (`GlobalToolbarView.java:685`).
- **Fix:** add `|| sketchSession().active() || supportChooser().active()` to the import guard, and
  `showImportAvailable(!sculpting && !sketching && !supportChooserActive())`. Apply the same condition to
  `onImportGlbRequested` and `onOpenGlbDocumentChosen` (`EditorWorkspaceView.java:5835-5859`).

**C-03: P3. With a full sketch table (16 records), Add and Cut are still offered and the refusal is misnamed. DEFECT-CONFIRMED.** *(Round 2: the Keep Sketch sub-claim is withdrawn, see below.)*

- **The rule:** "a control that cannot succeed is not drawn".
- **Where:**
  - `operationAvailable` (`forgeshape_sketch_session.cpp:2932-2933`) gates Add and Cut on
    `cadFeatureCount < kMaxCadFeatures` only. But `kMaxCadSketches = kMaxCadFeatures = 16`
    (`forgeshape_cad_body.h:418,488`), and a new face Add/Cut also needs a new sketch record
    (`addCadSketchRecord`, `forgeshape_cad_body.cpp:535`).
  - `candidateState` (`:2895-2902`) then pushes a feature with `sketchId = 0`.
  - ~~`canKeepSketch` has no table-size test~~. WITHDRAWN in Round 2: `canKeepSketch` (`:556-565`)
    calls `keepsOnProducer` (`:545-552`), which requires `target.sketches.size() < kMaxCadSketches`.
    Keep Sketch is therefore withdrawn on a full table.
- **Reproduction (E1, probe P6):**
  - A body with 15 kept face sketches gives `table=16 features=1 add_offered_by_feature_count_rule=1`.
  - The new-Add candidate as `candidateState` builds it gives `candidate_validate=HighWaterInvalid regenerate=SketchNotFound`.
- **Impact:**
  - Reaching this takes 15 Keep Sketch acts on one body.
  - The user can choose Add or Cut. The preview then names `SketchNotFound`, shown as the token
    fallback `status_cad_refused`, not the real reason, which is a full table.
- **Grade (Round 2):** E2 for the UI offer and the candidate path. Probe P6 re-implements
  `operationAvailable` and `candidateState`; it does not call them. E1 holds only for the domain
  refusals on a hand-built state. The preview-time refusal is deliberate (comment at `:2889-2892`),
  which softens the charge without removing it.
- **Fix:**
  - Add `targetBaseState_.sketches.size() < kMaxCadSketches` to `operationAvailable` (Add/Cut on a
    new face sketch).
  - Make the refused-candidate path name `TooManySketches`.
- **OWNER question:** should kept sketches share the 16-slot table with feature sketches?

**C-04: P3. A typed line length that is negative or longer than 1e5 m is refused under the name `ZeroLengthLine`. PARTIAL: the refusal is correct, its name is wrong.**

- **Where:** `applyLineLength` (`forgeshape_sketch_session.cpp:825-828`) maps every
  `validateDimensionMeters` failure and `length > kMaxSketchCoordinateMeters` to `ZeroLengthLine`.
  Its user string is "A length must be a positive number." (`strings.xml:547`).
- **Reproduction (E1, probe P4):**
  - `-1 -> ZeroLengthLine` and `1e+09 -> ZeroLengthLine`. The line is unchanged.
  - `NaN/inf -> NonFinite`; `1 -> Ok`, with P0 fixed and direction kept: `end=(0.6,0.8)`.
- **Impact:** for 1e9 m the message misstates why. The CLAUDE.md rule "refused, never clamped" holds.
- **Fix:** return `CadStatus::OutOfRange` for the too-long case, with its own message.
- **Related (Round 2):** Seat F's F-05 is P2. The field's `DigitsKeyListener("0123456789.-")`
  (`SketchDimensionLabelView.java:112`) drops `,`, so typing "2,5" applies 25 m. CAD-025 depends on it.

**C-05: INFO. Two stale `CadStatus` header comments. DOC-DIVERGENCE.**

- `forgeshape_sketch.h:141-143`: the comment on `OverlappingRegions` says it covers "a region and
  its own hole". Since CAD-FOUNDATION-C1 that pair is a legal union
  (`forgeshape_sketch_region.cpp:337-341`). Probe P7 confirms it: `ring+disk=Ok vol=4.000000 components=1`.
- `forgeshape_sketch.h:216-219`: the comment on `PlanarFaceUnsupportedCurve` says it is for a
  sketch carrying a Spline. Splines are intersected since FILL-HUD-R1, and only an unknown entity
  kind reaches `ArrangementStatus::UnsupportedCurve` (`forgeshape_sketch_arrangement.cpp:395-397`).
- `forgeshape_sketch.h:144-145` (Round 2, from Seat F): the `OverlappingHoles` comment still says
  "undefined without a planar arrangement this build does not compute". That arrangement has
  existed since CAD-PLANAR-FACE-PF-S1.
- **Grade:** E2 + E1 (P7).
- **Fix:** update all three comments.

**C-06: P2 (risk). Device evidence for the core sketch classes is not current on the audited tree. PARTIAL.** *(Round 2: narrowed. SketchUxTest (14), CadVerticalSliceTest (9) and CadExtrudeExtentTest (5) are green on this tree in this audit's run `40800569636` (E1, emulator only; evidence `artifacts/function-council-r2/evidence/device-run-40800569636/`, recorded in TESTS.md). Still stale: SpatialSketchTest, SketchExtrudeTest, CadCanvasExtrudeTest, CadPlanarFaceRuntimeTest and CadPlanarFaceOwnerCorrectionTest.)*

- **The gap:** these five classes have no run on the C2 tree, `f3db953`, the same as `70a6c14`
  (none of them appears in `40797031704`, `40800569636` or `40805804385`):
  - SpatialSketchTest: last recorded at cad-vertical-slice-r1 (`36465122847`).
  - SketchExtrudeTest, CadCanvasExtrudeTest, CadPlanarFaceRuntimeTest and
    CadPlanarFaceOwnerCorrectionTest: last recorded 2026-10-05.
- **What has changed since (corrected in Round 2):** 126 is the `6c9f156..70a6c14` (main..HEAD)
  count, not commits after those runs. The command `git log --oneline --since=2026-10-06 70a6c14` gives 75 commits, of which
  26 touch `app/src/main`. Seat F counted 28 and 77; the boundary depends on the time zone.
- **What PROJECT_STATUS requires:** the Tier-5 aggregate before the v6 branch merges, and none has run on this tree.
- **Grade:** E3/E4.
- **Fix:** one focused CI DEVICE union of the five still-stale classes on the audited tree, following Tier 2 of
  TEST-OWNER-04.

## Re-checks of FUNCTION-COUNCIL-R1 leads on this tree

- **D2:** still present. It is C-01 (E1).
- **D5 (overlay stale after a pinch):** FIXED. `SketchSession::overlay` now bumps
  `overlayRevision_` on a camera-only rebuild (`forgeshape_sketch_session.cpp:3768-3779`). E2.
- **Import during sketch:** still present. It is C-02 / A-04.

## Hypotheses (unproven)

- **H-01: double regeneration on commit.** `commitIntoTarget` gates on the evaluated candidate, then
  `CadBody::applyState` regenerates the whole chain again. So a 16-feature commit pays two
  regenerations. Equality with the preview rests on determinism (P3 `deterministic_regen=1`), not
  on reuse of the evaluated mesh. Wording in CLAUDE.md: "a commit applies that same evaluation".
  Cost on ARM64 has not been measured.
- **H-02: evaluation on the UI thread.** FC-R1 I-4 E: the first evaluation after a drag sample may
  run on the UI thread under `g_stateMutex`. This seat did not re-check it.
- **H-03: near-full revolves.** A Revolve at 359.9999999° validates and produces a partial solid of
  260 triangles whose two caps are about 1.7e-7 m apart at r = 1 m. Kernel validation accepts it
  (P2). Whether a downstream consumer such as an exporter or another tool treats the near-touching
  caps as non-manifold has not been shown.
- **H-04: the import CAD consequence.** After C-02, Add/Cut still lands on its producer. This is
  source-derived only.

## Adversarial counterexample attempts

1. **Navigator view vs camera (P1).** The counterexample succeeded and became C-01.
2. **Revolve angle domain edges (P2).** Inputs: 0.000999, 0.001, 359.99, 359.9999999, 360,
   360.0000001, NaN and −90.
   - Every out-of-domain value is `RevolveAngleInvalid`, never clamped.
   - The boundary values are Ok, with 360 giving 256 triangles (a seam and no caps) and 359.99
     giving 260.
   - No defect. H-03 is recorded.
3. **Feature cap bypass (P3/P5).** The builder refuses the 17th feature
   (`append_refused_at_count=16`). A hand-built 17-feature state is refused by
   `validateCadBodyState` (`TooManyFeatures`). No bypass.
4. **Sketch-table cap (P6).** The counterexample succeeded and became C-03.
5. **Union rule edges (P7).** A ring plus its own disk is Ok with volume 4.000000 and one
   component. A stale hole list is `ProfileRegionMismatch`. No defect; it exposed the stale
   comment in C-05.
6. **Codec, determinism and lineage (P3).**
   - The shop body (base + Add + Cut) has volume 4.036, exactly as expected.
   - Regeneration is bit-identical twice.
   - encode, decode and re-encode give identical bytes (471 B).
   - The lineage token is unchanged by a depth edit and by a Symmetric conversion.
   - Deleting the root sketch gives `SketchIsRoot`; deleting a used sketch gives `SketchInUse`.
   - An Add fully inside the body gives `AddNoEffect` naming feature 4.
   - A 10 × 10 m Cut downward from the top cap is Ok. It legally leaves the boss, which stands above
     the cut, so the result is not `CutRemovesBody`.
   - No defect.
7. **Typed line length (P4).** The refusal holds. The misnaming became C-04.
8. **Java and native status-code drift (`status_map_check.sh`).** Of 93 native codes, 81 have Java
   constants, with 0 mismatches. The rest use the token fallback. No defect.

## Positive claims with evidence

| Claim | Evidence |
| --- | --- |
| The whole CAD/sketch native domain passes: 155 + 66 + 118 + 638 checks, 17 geometry oracles and 17 PICK_C2 seam checks | fresh `host_selftests.txt` (E1) |
| Every CAD corpus fixture decodes to its recorded verdict; refusal fixtures are refused by name | fresh `host_corpus_verdicts.txt` (E1) |
| Revolve angles are refused, never clamped, and 360 is exactly the full turn | P2 (E1) |
| The feature cap of 16 holds in both the builder and the validator | P3, P5 (E1) |
| Regeneration is deterministic, the CADB round trip is byte-identical, and the lineage token is stable under extent and size edits | P3 (E1) |
| Retained-sketch delete refuses root and in-use sketches by name | P3 (E1) |
| The ring+disk union is legal and stale holes are refused | P7 (E1) |
| Java `CAD_*` constants match native `CadStatus` ordinals | `status_map_check.sh` (E1) |
| The chooser's nearest-target rule and the orthographic seam fix | host C1_CH / PICK_C2 (E1); device D04, D15, D16, D19-D22 (E3: `40776968165`) |
| The A2 refusal stands | R2_SK_06 (E1); D18 (E3) |
| CAD HUD/browser/drafting presentation POLICY passes (80 JVM tests). These pure-policy tests do not assert view visibility, hit areas or field input (Round 2). | fresh XML (E1) |
| FC-R1 D5 is fixed | E2 |

## Journeys proposed for END_TO_END_JOURNEYS

1. **J-C1: face Add/Cut chain round trip.**
   - Steps:
     1. New Project → CAD → rectangle → Extrude 1 m.
     2. New Sketch → aim and confirm the top face → circle → Add 0.5 m.
     3. New Sketch → the top face → rectangle → Cut 0.4 m (One Side, into the body).
     4. History → Edit Feature 1 depth 1.5 → Finish.
     5. Undo ×1, Redo ×1.
     6. Save, close, Open.
   - Expected:
     - Each act is one Undo, and the same ObjectId is kept.
     - The boss and the pocket follow the base cap.
     - The reopened `.forge` matches the pre-close bytes and fingerprint, with an empty history after load.
   - Tests: host CADVS_SES, R2_HIST, R2_DATA; device CadVerticalSliceTest (green on this tree in
     `40800569636`, E1 emulator) and CadParametricHistoryOwnerTest.
2. **J-C2 (negative, fail closed): a refused Cut.**
   - Steps: on a body, a face sketch with a rectangle placed off the body. Choose Cut.
   - Expected:
     - The badge shows the error ring and `CutNoIntersection` appears in the status line.
     - The toolbar Extrude is absent and a commit through the precision surface is refused.
     - No history step and no fingerprint change.
   - Tests: host CADVS_OPS; device evidence NONE on this tree.
3. **J-C3 (negative): a staged History edit that breaks a later feature.**
   - Steps: shrink feature 1 so feature 3's support face vanishes.
   - Expected:
     - Row 3 shows a cross with `SupportFaceLost`, and later rows show "not rebuilt".
     - Commit is withdrawn. Fix restores editing. Cancel leaves the bytes identical.
   - Tests: host PAR, R2_HIST; device CadParametricHistoryOwnerTest.
4. **J-C4 (negative): the A2 refusal.**
   - Steps: on a CAD project, New Sketch on the XY plane, then draw.
   - Expected: Keep Sketch is absent. Native `sketchCanKeepSketch` is false and `commitSketch` gives `NotSketching`.
   - Tests: R2_SK_06, D18.
5. **J-C5: Revolve.**
   - Steps: rectangle plus axis line → Finish → Revolve → pick the axis → type 90 → Flip → type 360.0001.
   - Expected: 360.0001 is refused with `RevolveAngleInvalid` and the held 90 stands. Commit is one Undo.
   - Tests: REV_*, CadRevolveOwnerTest.
6. **J-C6 (would fail today): orientation navigator. Merged with Seat F's J-F3 in Round 2: one journey for one defect.**
   - Steps: begin a sketch, press Flip, then +90.
   - Expected: the camera view matrix changes and the authored values do not.
   - Current: FAILS (C-01).

## Owner decisions needed

1. **A2:** BLOCKED-DESIGN-DECISION-R2-WORLD-PLANE-RETAINED-SKETCH, options 0-3 as in
   `artifacts/cad-r2-c1-quality-gate/SUMMARY.md`. Option 0, the refusal, is current behaviour, not a decision.
2. **Sketch table:** should kept sketches share the 16-slot table with feature sketches (C-03)? Or
   should `kMaxCadSketches` be independent? Raising it would change the CADB bound and needs a format review.
3. **Face sketch on another body (FC-R1 §5 Q3, still open):**
   - What the code does today: New Body, which creates a TopoRef-dependent body, and also Add and
     Cut into the producer body whose face it is (`operationAvailable:2927-2933`).
   - What the OWNER must confirm: that this is the intended model.
4. **Physical checks still pending:**
   - the C1/C2 OWNER test packs: orthographic seam, large font, stylus on the chooser, ARM64 timing of 3969-cell edits;
   - the drafting and Revolve checklists. All are OWNER-PHYSICAL-UNVERIFIED.

## Unresolved dependencies and open questions

- The `release_guard.txt` contents and `guard_exit=2` disagree (Seat B reports an ANDROID_HOME error
  in the same file). This needs a run-owner reconciliation.
- C-02 is owned jointly with Seat A (A-04).
- Local corpus parity was not run (no pwsh). CI FAST `40787465884` is the evidence (E3).
- `forgeshape_renderer.cpp` (preview tint and candidate substitution) cannot be host-tested. Its
  evidence is device-only: CadVerticalSliceTest and CadExtrudeExtentTest ran on this tree in
  `40800569636` (E1, emulator only); the canvas/planar-face classes stay stale (C-06).
- H-02 (UI-thread evaluation) needs Seat D/E (performance/Android) confirmation.

Completeness: **PARTIAL**.

- **What was covered:** every requirement row in scope was traced to source, and the native domain
  has fresh E1 host evidence plus targeted E1 probes.
- **Why it is still PARTIAL:**
  - The canonical master plan and owner-decision files are unavailable, so the requirement horizons
    are reconstructed from repo records.
  - This seat ran no device or CI dispatch itself. The coordinator's runs on this tree
    (`40797031704`, `40800569636`, `40805804385`; emulator only, recorded in TESTS.md) cover
    SketchUxTest, CadVerticalSliceTest and CadExtrudeExtentTest at E1; the other Android/UI parts
    rest on historic E3 runs, and five core sketch classes (SpatialSketchTest, SketchExtrudeTest,
    CadCanvasExtrudeTest, CadPlanarFaceRuntimeTest, CadPlanarFaceOwnerCorrectionTest) are stale on
    this tree (C-06). No emulator run closes a physical-device gate.

## Round 2 — objections from seat F and responses

Product tree re-checked at `70a6c14`. `git diff --quiet 70a6c14 HEAD -- app/src/main` succeeded. No product file was modified.

Conceded: 18 of 19 objections (one in part). Rebutted: 1 (#6's commit figures, in part). Nothing remains UNRESOLVED between the seats. Two items stay open for the OWNER or the run owner (end of this section).

| # | Objection | Response | Re-verification | Status |
|---|---|---|---|---|
| 1 | C-01: merge with F-02, cross-reference F-01, and relabel "every caller keeps working" as E5 | CONCEDED | View state is reset only in `begin()` and `cancel()` (`forgeshape_sketch_session.cpp:225,270`). `backToEditing` (`:2275`) does not reset it. With `viewFrame()`, Back to Sketch and Edit Sketch would therefore reinstall the flipped view. SEAT_C C-01 and CAD-013 are amended. | RESOLVED; the OWNER statement on the post-fix Back behaviour stays open |
| 2 | C-02: record the reachable UI path and widen the Java fix to include the chooser | CONCEDED | `GlobalToolbarView.java:685` hides `markGroup` only in the bootstrap. `onProjectActionsRequested` (`EditorWorkspaceView.java:5411`) has no sketch guard. `:2823` gates on `!sculpting` only. | RESOLVED, P2 |
| 3 | C-03: the Keep Sketch half is wrong, and the Add/Cut half's E1 is overstated | CONCEDED | `keepsOnProducer` (`:545-552`) checks `target.sketches.size() < kMaxCadSketches`. Probe P6 re-implements `operationAvailable` and `candidateState` by hand. The sub-claim is withdrawn. CAD-063 is regraded to E2, with E1 only for the domain refusals. The deliberate preview-time refusal (`:2889-2892`) is noted. | RESOLVED, P3, OWNER question kept |
| 4 | Add F-05 (comma dropped, "2,5" becomes 25 m) to CAD-025 and raise the row to P2 | CONCEDED | `SketchDimensionLabelView.java:112` uses `DigitsKeyListener.getInstance("0123456789.-")`. CAD-025 is now P2 via F-05; C-04 itself stays P3. F-05 is E2: the filter behaviour is platform-documented and was not reproduced on a device by this seat. | RESOLVED |
| 5 | A third stale comment, `OverlappingHoles` | CONCEDED | Confirmed at `forgeshape_sketch.h:144-145`. C-05 and CAD-108 now list three comments. | RESOLVED |
| 6 | C-06 is overtaken by `40800569636`, and the "126 commits" figure is mis-stated | CONCEDED, with a numeric correction | `summary.json` and the junit XML show SketchUxTest 14, CadVerticalSliceTest 9 and CadExtrudeExtentTest 5, all with 0 failures. C-06 and CAD-112 are narrowed to the five still-stale classes and J-C1 is updated. 126 is the main..HEAD count. My run of `git log --oneline --since=2026-10-06 70a6c14` gives 75 commits in total and 26 on `app/src/main`, not 28 and 77; the difference is likely the time-zone boundary. SEAT_C now records my figures. | RESOLVED |
| 7 | Host suites E1 | No objection | `run_status.txt` shows `host_exit=0`. | — |
| 8 | CAD-107 should state that "no dedicated test" lets the drift return | CONCEDED | Added as a coverage note. The verdict is unchanged. | RESOLVED |
| 9 | Revolve angles | No objection | — | — |
| 10 | Chooser nearest target and seam fix | No objection (D22 is a simulated stylus; CAD-113 covers it) | — | — |
| 11 | The 80 JVM tests are pure policy | CONCEDED | The positive-claims row now says the tests are policy-only. | RESOLVED |
| 12 | CAD-018, 073, 110 and 076 are over-graded | CONCEDED | CAD-018 becomes PASS-SOURCE-AND-RECORDED-RUNTIME (E2+E3). CAD-073 is annotated "policy E1; view binding E2/E3". CAD-110 is limited to the data model. CAD-076 becomes native E1 (pending reconciliation) plus Java E2, verdict PASS-SOURCE-AND-RECORDED-RUNTIME. The timing was confirmed: `release_guard.txt` mtime 00:57:29 postdates `guard_exit=2` at 00:54:48. The Java `FORGESHAPE_CAD_HUD_TOUCH:` gate is `CadExtrudeCanvasView.java:249-250,537`. | RESOLVED; the guard rerun needs run-owner reconciliation |
| 13 | CAD-101 has the wrong horizon | CONCEDED | `GlobalToolbarView.java:896` withdraws Start Sculpting with `!cad`. Native refusal is at `forgeshape_jni.cpp:2930`. CAD-101 becomes H0/H1 PASS-SOURCE-AND-RECORDED-RUNTIME (E1 native + E2 control). A new row, CAD-114 (H2 FUTURE-NOT-DUE), covers CAD → Sculpt itself. | RESOLVED |
| 14 | CAD-089 BLOCKED-OWNER | No objection | — | — |
| 15 | CAD-001's Back path (F-03) | CONCEDED | In `dismissTopmostSurface`, the `bootstrapVisible()` branch (`:1177-1184`) runs before `topmostOpenSurface()` (`:1201`), so Back cancels the sketch even when the precision surface is open. CAD-001 becomes PARTIAL on the Back path (E2, not reproduced). The project-cost half stands. | RESOLVED |
| 16 | No requirement row for Back during an in-project Revolve (F-04) | CONCEDED | `hasDismissibleSurface` (`:1112-1117`) has no revolve term. `BackDismissal` registers the callback only while that answer is true (`ForgeShapeActivity.java:87-98,142-155`), and `targetSdk 36`. New row CAD-115: H1, DEFECT-CONFIRMED (E2), P2, cross-referenced to F-04. | RESOLVED |
| 17 | F-05 on CAD-051, 055 and 075 | CONCEDED | `CadExtrudeCanvasView.java:644` uses `"0123456789.-"`, while `NumericPropertyRow.java:109` uses `"0123456789.,-"` and `LengthUnit.java:129` maps `,` to `.`. So the comment at `EditorWorkspaceView.java:4103-4105` is false for "2,5". The dependencies are recorded. | RESOLVED |
| 18 | CAD-024's touch half (F-01) | CONCEDED | `EditorControlStyles.chip` sets only a minimum height (`:377`). `SketchDimensionLabelView` adds no minimum width, whereas `SketchDimensionLabelsView.java:177` does. F-01's 43.4 dp measurement is Seat F's E3 and was not re-measured by this seat. CAD-024 becomes PARTIAL. | RESOLVED |
| 19 | Merge J-C6 with J-F3 | CONCEDED | J-C6 is marked merged. | RESOLVED |

Open items for others (not seat disputes):
- **OWNER:** whether Back to Sketch and Edit Sketch should restore the aligned view or the user's flipped and rotated view once C-01 is fixed.
- **OWNER:** whether kept sketches share the 16-slot table (C-03).
- **Run owner:** reconcile `guard_exit=2` with the later PASS in `release_guard.txt`.
