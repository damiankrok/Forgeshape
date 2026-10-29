# FUNCTION-COUNCIL-R2 — End-to-end journeys

Audited tree `70a6c14` (product tree = `f3db953`). 53 journeys: 31 positive
(J01–J31) and 22 negative / fail-closed (N01–N22). Journeys that cross modes or
representations: J09, J22, J23, J24, J26, J27, J30, J31, N09, N11, N18, N19,
N20. Built on the controls in
`FEATURE_INVENTORY.md`; proposals taken from the seats' "Journeys proposed"
sections (J-A*, J-B*, J-C*, J-D*, J-E*, J-F*, J-G*, J-H*, J-I*, JR-J*), merged
where two seats proposed one journey for one defect.

## Status rules

- **PASS-RECORDED** — ONE named test method (or, where stated, the ordered
  stages of ONE staged class) asserts the whole journey as written here,
  including its Undo and byte/reopen clauses. The run tag says how fresh that
  evidence is.
- **PARTIAL** — the journey's components are asserted by different tests, or
  the named test covers the journey minus a clause (the missing clause is
  named). Components passing separately never make a journey PASS-RECORDED.
- **UNVERIFIED** — no test asserts the journey. Where source proves the journey
  would fail today, the line says "source predicts FAIL" and names the seat
  finding; that is a defect claim graded by the seat, not a run result.

Run tags (detail in `FEATURE_INVENTORY.md` §0; ids recorded in `TESTS.md`):
`FC2-D1` = CI DEVICE `40797031704`, `FC2-D2` = `40800569636`, `FC2-D3` =
`40805804385` — dispatched by this audit, API 36 x86_64 emulator, SwiftShader
(E1, emulator only, never a physical-device PASS); `C2` = CI DEVICE
`40776968165` / `40782861799` on the identical product tree (E3); `OLD` = last
green run on an older tree (E3); `HOST` = this audit's host suites (E1).

Input path matters: a test that reaches a control by `performClick()` or by a
direct JNI call does not prove the control can be touched (`TEST_COVERAGE_MATRIX.md`
FG-3). Each journey names the path its test uses: **touch** (decor-root
`MotionEvent`, `ModelingOwnerRig`), **viewport** (event sent to the
SurfaceView), **click** (`performClick`), **JNI**.

Defect verdicts are the seats' Round-2 verdicts, reconciled with
`work/VERIFICATION.md` (three-lens panel; 30 of 38 P0–P2 findings
panel-verified, all 30 survived). The panel upheld the findings behind the
"source predicts FAIL" journeys: A-01 (N12), A-02 (N13), A-04
(N11; same root cause as C-02 / D-02 / E-03), B-01 and B-02 / E-01 (N18),
C-01 / F-02 (N17; F-01, also cited there, survived too), D-01 (N19), E-02 and
A-05 (N21), F-03 (N14), F-04 (N15) and F-05 (N16), each upheld 3/3 by the lenses.
**D-03 (N20) and J-01 (N22) were NOT panel-verified** — neither appears in
`VERIFICATION.md` — so N20 and N22 rest on the seat grade alone (Seat D / Seat
J host diagnostics, E1). The panel also judged 10 of 12 strongest positive
claims overstated at their stated grade, among them "a failed open is
all-or-nothing" (A, E) and "System Back closes the topmost surface on API 36"
(F); the PASS-RECORDED statuses of N01, N02 and the Back observation below
are therefore read strictly as "the named test passed on the named path", not
as proof of the broader property.

## Summary

| # | Journey | Kind | Modes | Status | Primary test (tag) |
| --- | --- | --- | --- | --- | --- |
| J01 | Cold launch → New CAD → first Extrude creates the project | + | Home, Sketch, CAD | PASS-RECORDED | HomeFlowTest#e2eAppH1_03 (C2) |
| J02 | New Sculpt → stroke → Undo / Redo | + | Home, Sculpt | PARTIAL | SculptUndoTest#e2eScundo01 (FC2-D2) |
| J03 | New Freeform → vertex tap → handle drag → Undo / Redo | + | Freeform | PASS-RECORDED | FreeformOwnerTest#devFf01 (FC2-D2) |
| J04 | Freeform tools are one Undo each | + | Freeform | PASS-RECORDED | FreeformOwnerTest#devFf02 (FC2-D2) |
| J05 | Freeform symmetry + subdivision → encode → reopen bit for bit | + | Freeform, persistence | PASS-RECORDED | FreeformOwnerTest#devFf03 (FC2-D2) |
| J06 | Surface Patch → Trim → tube → Stitch → Undo/Redo → reopen | + | Surface | PASS-RECORDED | SurfaceOwnerTest#devSurf01 (FC2-D2) |
| J07 | Surface open Extrude → Thicken → History edit → staged failure Cancel | + / − | Surface, History | PASS-RECORDED | SurfaceOwnerTest#devSurf02 (FC2-D2) |
| J08 | Surface Revolve of an open profile and Loft | + | Surface | PASS-RECORDED | SurfaceOwnerTest#devSurf03 (FC2-D2) |
| J09 | CAD face chain: Extrude → Add on cap → Cut → History edit → Undo → save → reopen | + | CAD, History, persistence | PARTIAL | several (C2, FC2-D2) |
| J10 | New Sketch from the drawer on a Cut pocket floor → Add commits | + | CAD | PASS-RECORDED | OwnerProjectShellSketchSupportTest#devOss03 (C2) |
| J11 | Keep Sketch on a face → save → reopen keeps its id | + | CAD, History | PASS-RECORDED | CadRetainedSketchOwnerTest#d02 (C2) |
| J12 | Edit Sketch staged → Cancel → Finish → Undo / Redo | + | CAD | PASS-RECORDED | SketchUxTest#cadUxR1_32_33_34_35 (FC2-D2) |
| J13 | Revolve New Body: tap axis → angle → commit → Undo → reopen | + | CAD Revolve | PARTIAL | CadRevolveOwnerTest#devRev01/02/07/08 (C2) |
| J14 | Extrude extent Symmetric / Two Sides → exact values → save → Open | + | CAD | PARTIAL | CadExtrudeExtentTest#cadext06a/06b/12 (FC2-D2) |
| J15 | Twenty fill cells by real taps → first Extrude | + | Sketch, CAD | PASS-RECORDED | CadMultiFaceOwnerTest#devMf04 (C2) |
| J16 | Drafting: Construction role + dimensions → Save → reopen exact (CADB v8) | + | Sketch drafting, persistence | PASS-RECORDED | CadSketchDraftingOwnerTest#devDr10 (C2) |
| J17 | Add Primitive → gizmo drag → one Undo / Redo | + | Construction | PARTIAL | EditorWorkspaceGizmoTest (OLD) |
| J18 | Objects row: Rename, Hide/Show, Lock/Unlock, Duplicate | + / − | Construction objects | PASS-RECORDED | ObjectCommandsTest#e2eObj018a01 (FC2-D3) |
| J19 | Mirror YZ through the row → one new body | + | Construction objects | PASS-RECORDED | MirrorSmokeTest#e2eMirror0101 (FC2-D3) |
| J20 | Dimensions + anchors + Relative Scale | + | Construction | PASS-RECORDED | BodyDimensionsSmokeTest#e2eDim020m01 (FC2-D3) |
| J21 | Delete → Undo restores the same body → Redo removes it | + | Objects | PASS-RECORDED | ObjectsDeleteTest#imp01b18and19 (FC2-D3) |
| J22 | Import GLB → six objects → one Undo removes all → Redo | + | Import, history | PARTIAL | ImportedMeshDurableTest#e2eImp01a01 + #e2eImp01a03 (FC2-D3) |
| J23 | Imported Mesh → Start Sculpting → stroke → save / autosave / recover / reload | + | Import → Sculpt → persistence | PASS-RECORDED | ImportedMeshSculptTest#imp01b11and13 (FC2-D2) |
| J24 | Sculpt mask → Back → Resume → reopen with an empty mask | + | Sculpt, persistence | PASS-RECORDED | SculptBrushStage025Test#e2eFcm09and10 (FC2-D2) |
| J25 | Sculpt History navigator: jump back, new stroke drops the future | + | Sculpt | PASS-RECORDED | SculptHistoryNavigatorTest#e2eSchnav05 (FC2-D2) |
| J26 | Save → process death → Open Saved Project | + | persistence, lifecycle | PASS-RECORDED (staged class) | ProjectProcessDeathTest#stage1 + #stage2 (FC2-D3) |
| J27 | Unsaved edit → process death → Recover | + | autosave, recovery | PASS-RECORDED (staged class) | ProjectProcessDeathTest#stage5 + #stage6 (FC2-D3) |
| J28 | Change every preference → project bytes identical | + | Settings | PASS-RECORDED | SettingsPreferencesTest#uiprefr1_09_10_11_37 (FC2-D3) |
| J29 | Six-body project → Export → one valid GLB | + | Export | PASS-RECORDED (OLD) | GlbExportTest#e2er1c01 (OLD) |
| J30 | Cross-mode: CAD body beside a sculpted Imported Mesh, Construction Undo/Redo | + | CAD, Import, Sculpt | PASS-RECORDED (OLD) | SketchExtrudeTest#e2eCadr0_16 (OLD) |
| J31 | Cross-mode: CAD + Freeform + Surface in one project → save → kill → reopen | + | all three | UNVERIFIED on device; host PARTIAL | seat J `j4_replay` (HOST scratch) |
| N01 | Open a corrupt `.forge` from Home | − | Home, persistence | PASS-RECORDED | HomeFlowTest#e2eAppH1_07 (C2) |
| N02 | Open a damaged / truncated / newer file over an open project | − | persistence | PASS-RECORDED | EditorWorkspaceProjectActionsTest#e2er1a04_* / #e2er1a05 (C2) |
| N03 | Refused Cut and disjoint Add change no byte | − | CAD | PASS-RECORDED | CadVerticalSliceTest#operation_refusal (FC2-D2) |
| N04 | Revolve about a crossing axis is refused, then a valid axis commits | − | CAD Revolve | PASS-RECORDED | CadRevolveOwnerTest#devRev06 (C2) |
| N05 | History edit that breaks a later Cut → named → Cancel byte-identical → Fix | − | CAD History | PASS-RECORDED | CadParametricHistoryOwnerTest#devPar02 (C2) |
| N06 | Last body Delete absent and refused; Delete withdrawn in Sculpt | − | Objects, Sculpt | PASS-RECORDED | ObjectsDeleteTest#imp01b21_* (×2) (FC2-D3) |
| N07 | Corrupt / unsupported recovery candidate quarantined, never offered again | − | recovery | PASS-RECORDED | ProjectAutosaveRecoveryTest#fsr1b08_* (FC2-D3) |
| N08 | Hidden body cannot Start or Resume Sculpting | − | Sculpt | PASS-RECORDED | Stage027SculptWorkflowTest#guard2_* (FC2-D2) |
| N09 | Import GLB in Sculpt: absent, late picker result refused, native refuses | − | Import, Sculpt | PASS-RECORDED | ImportGlbSculptGuardTest#d1_03 (FC2-D2) |
| N10 | Injected device loss costs the project nothing | − | renderer | PASS-RECORDED | DiagnosticsAndRendererLossTest#e2er1b07_aLostDeviceCostsTheProjectNothing (FC2-D3) |
| N11 | Import GLB while a sketch is open inside a project | − | Sketch, Import | UNVERIFIED — source predicts FAIL (A-04) | none |
| N12 | "Save and continue" before Open Saved Project | − | persistence | UNVERIFIED — source predicts FAIL (A-01) | none |
| N13 | Recover, then leave or lose the process | − | recovery | UNVERIFIED — source predicts FAIL (A-02) | FSR1B-05 pins the current behaviour |
| N14 | CAD bootstrap: System Back with the precision surface / a palette open | − | Sketch, Back | UNVERIFIED — source predicts FAIL (F-03) | none |
| N15 | In-project Revolve: System Back on API 36 | − | Revolve, Back | UNVERIFIED — source predicts FAIL (F-04) | none |
| N16 | Type "2,5" in the depth / length / dimension editors | − | CAD HUD, Dimensions | UNVERIFIED — source predicts FAIL (F-05) | none |
| N17 | Navigator Flip / ±90° move the camera, not the sketch | − | Sketch | UNVERIFIED — source predicts FAIL (C-01) | existing tests assert stored flags only |
| N18 | Face-supported CAD body: Transform refused, Export at drawn place | − | CAD, Export | UNVERIFIED — source predicts FAIL (B-01, E-01) | host diags (E1) |
| N19 | Sculpt Undo after a stroke too large to retain | − | Sculpt | UNVERIFIED — source predicts FAIL (D-01) | host diag (E1) |
| N20 | Open another project during a Freeform cage edit | − | Freeform, persistence | UNVERIFIED — source predicts FAIL (D-03) | host diag (E1) |
| N21 | Save → palette change → New Project… asks nothing; no recovery question | − | Settings, lifecycle | UNVERIFIED — source predicts FAIL (A-05, E-02) | none |
| N22 | Hidden CAD body edited through History | − | CAD History | UNVERIFIED — BLOCKED-OWNER (J-01 / OD-J1) | host diag (E1) |

---

## Positive journeys

### J01 — Cold launch → New CAD → first Extrude creates the project
- **Initial state:** fresh process, no project, no recovery candidate.
- **Actions:** Home → `home_new_project` → `new_project_cad` → (rail) Rectangle → drag a rectangle in the viewport → `finish_sketch` → `extrude_sketch`.
- **Expected domain changes:** before Extrude: `projectOpen()==false`, 0 bodies, sketch on XY, Back to Home drawn, mark and Objects capsule absent. After: 1 CAD body (world-plane), `MODE_CONSTRUCTION`, mark drawn, project dirty.
- **Undo:** history empty after creation (undo 0, redo 0) — the first project is a loaded document.
- **Bytes / reopen:** `encodeProject()` non-null after creation; a reopen is not part of this journey (see J26).
- **Tests:** HomeFlowTest#e2eAppH1_03 (click + viewport; C2) asserts every clause above; host CAD_A3 `CADA3_BOOT` 13 checks (HOST); SketchUxTest#cadUxR1_05_06_07 (FC2-D2) for the bootstrap half.
- **Req:** APP-011..014, CAD-001, CAD-002. **Status: PASS-RECORDED** (C2, E3).

### J02 — New Sculpt → stroke → Undo / Redo
- **Initial:** Home. **Actions:** `home_new_project` → `new_project_sculpt` → one Clay stroke in the viewport → `undo_action` → `redo_action`.
- **Expected:** seeded sphere frozen inside the initialization bracket (empty Construction history); one stroke = one `SculptHistory` entry; Undo restores the exact pre-stroke positions and `hasEdits=false`; Redo restores the post-stroke positions and `hasEdits=true`; revision only moves forward.
- **Bytes:** no sculpt-history byte in `.forge` (SculptUndoTest#scundo20and21).
- **Tests:** SculptUndoTest#e2eScundo01 asserts stroke/Undo/Redo over a project seeded by the test fixture, not through `new_project_sculpt`; HomeFlowTest#e2eAppH1_04 (C2) asserts the New Sculpt start. Two tests → **PARTIAL**.
- **Req:** APP-015, MOD-018..023.

### J03 — New Freeform → vertex tap → handle drag → Undo / Redo
- **Initial:** Home. **Actions:** New Project → Freeform → (precision) `freeform_element_vertex` → tap vertex 7 → drag the cage gizmo's X handle 140 px → `undo_action` → `redo_action`.
- **Expected:** box cage 8/12/6; history empty at start; selection = {7}; vertex 7 moves along +X only, others fixed; exactly one step.
- **Undo:** restores x = 0.5 exactly; Redo restores the dragged value exactly.
- **Bytes:** not asserted (see J05).
- **Tests:** FreeformOwnerTest#devFf01 (touch; FC2-D2). **Status: PASS-RECORDED** (E1).
- **Req:** MOD-069, MOD-072, MOD-073, MOD-080.

### J04 — Freeform Push/Pull, Insert Loop, Crease, Extrude are one Undo each
- **Initial:** new Freeform project. **Actions:** per FreeformOwnerTest#devFf02 (face select → `field_freeform_distance` → `freeform_push_pull`; edge → `field_freeform_ratio` → `freeform_insert_loop`; `field_freeform_crease` → `freeform_set_crease`; `freeform_extrude`), Undo after each.
- **Expected / Undo:** each act one Construction step; Undo restores the exact cage.
- **Tests:** FreeformOwnerTest#devFf02 (touch; FC2-D2). **Status: PASS-RECORDED** (E1).
- **Gap noted:** the distance field is seeded with a unitless literal and read in the display unit (J-02); devFf02 runs in metres and cannot see it.
- **Req:** MOD-071, MOD-074..076, MOD-080.

### J05 — Freeform symmetry + subdivision → encode → reopen bit for bit
- **Actions:** `freeform_symmetry_x` → tap the +X face → distance 0.25 → `freeform_push_pull` → `freeform_level_0` → `freeform_level_3` → encode → `loadProject(bytes)`.
- **Expected:** mirror face moves exactly (−0.75); level 3 = 6×64 derived quads; digest differs from level 0.
- **Bytes / reopen:** after reopen every vertex bit-identical, symmetry and level kept, `freeformMeshDigest` and `projectFingerprint` equal, re-encode byte-identical.
- **Tests:** FreeformOwnerTest#devFf03 (touch for the tools; reopen through JNI `loadProject`, not the drawer; FC2-D2). **Status: PASS-RECORDED** (E1) for the journey as written; the drawer Save/Open path is J26's.
- **Req:** MOD-070, MOD-078, MOD-085.

### J06 — Surface Patch → Trim → tube → Stitch → Undo/Redo → reopen
- **Actions:** New Project → Surface → draw a 2 m square → `finish_sketch` → `surface_create_patch` (creates the project) → New Surface Sketch on the body → circle r 0.5 → Finish → `surface_create_trim` → New Surface Sketch → square → Finish → distance 1 → `surface_create_extrude` → `surface_stitch` → Undo → Redo → save and reopen.
- **Expected:** history empty after the first Finish; features 1→2→3→4; open edges 1→2→…−2 after Stitch; Loft not offered without a first section; Thicken absent over the stitched shell and native says `ThickenUnsupportedForSurfaceType`.
- **Undo:** removes exactly the Stitch; Redo restores the same digest.
- **Bytes / reopen:** `saveAndReopen()` helper (digest preserved).
- **Tests:** SurfaceOwnerTest#devSurf01 (touch + click; FC2-D2). **Status: PASS-RECORDED** (E1).
- **Req:** MOD-092..098, MOD-100..103, MOD-108.

### J07 — Surface open Extrude → Thicken → History edit → staged failure Cancel
- **Actions:** New Surface → two Lines (open chain) → Finish (Patch absent) → distance 0.5 → `surface_create_extrude` → Thickness 0.1 → `surface_thicken` → `feature_history_action` → Extruded Surface row → value 0.8 → `surface_edit_apply` → Undo → Redo → History → Thicken row → value 0 → (Apply absent, Fix shown) → `surface_edit_cancel`.
- **Expected:** volume 0.19×0.5 then 0.19×0.8 after the edit; nothing written before Apply; failure names Thicken.
- **Undo:** one Undo restores the pre-edit volume; Redo returns the depth.
- **Bytes:** Cancel of the staged failure leaves `encodeProject()` byte-identical.
- **Tests:** SurfaceOwnerTest#devSurf02 (FC2-D2). **Status: PASS-RECORDED** (E1).
- **Req:** MOD-094, MOD-099, MOD-103, MOD-104.

### J08 — Surface Revolve of an open profile, and a two-section Loft
- **Tests:** SurfaceOwnerTest#devSurf03 (FC2-D2) — `devSurf03_revolve_an_open_profile_and_loft_two_sections`. Steps and asserts as in that method (Construction straight edge as axis; Loft offered only with a first section).
- **Status: PASS-RECORDED** (E1) for revolve + loft creation; reopen not asserted in that method.
- **Req:** MOD-095, MOD-096.

### J09 — CAD face chain round trip (J-C1)
- **Actions:** New CAD → rectangle → Extrude 1 m → drawer New Sketch → aim + confirm top face → circle → Add 0.5 m → New Sketch on top face → rectangle → Cut 0.4 m (One Side, into the body) → History → Feature 1 depth 1.5 → Finish → Undo → Redo → Save → Open Saved Project.
- **Expected:** each act one step on the SAME `ObjectId`; boss and pocket follow the base cap; History lists sketches and features by id.
- **Bytes / reopen:** re-encode after reopen equals pre-close bytes and fingerprint; history empty after load.
- **Tests (components):** OwnerProjectShellSketchSupportTest#devOss02/03 (C2), CadVerticalSliceTest#new_body_then_add_same_body, #same_body_cut, #feature_edit_roundtrip (FC2-D2), CadParametricHistoryOwnerTest#devPar01 (C2), host CAD_FEATURE `CADVS_SES`, `R2_HIST`, `R2_DATA` (HOST). No single test runs the chain with the reopen. **Status: PARTIAL.**
- **Req:** CAD-003, CAD-059..068, CAD-094, CAD-104.

### J10 — New Sketch from the drawer on a Cut pocket floor → Add commits
- **Actions:** mark → `project_new_sketch` → tap the base cap twice → … Cut → mark → New Sketch → tap the pocket floor twice → draw → Add.
- **Expected:** sketch origin on the cap; Cut on the same body (2 features); sketch opens face-supported on the floor; boss is feature 3, an Add standing on feature 2; volumes as asserted.
- **Tests:** OwnerProjectShellSketchSupportTest#devOss03 (touch; C2). **Status: PASS-RECORDED** (E3). Undo and reopen are not part of this method.
- **Req:** APP-023, CAD-003, CAD-010, CAD-060.

### J11 — Keep Sketch on a face → save → reopen keeps its id
- **Actions:** on a CAD body, New Sketch on the top face → draw → `keep_sketch` → encode → `loadProject` → `feature_history_action` → `feature_history_tab_sketches`.
- **Expected:** one Undo step, no feature added, sketch id 2 `UNUSED` on feature-face support; volume unchanged.
- **Bytes / reopen:** same id and state after reopen; re-encode byte-identical; the Sketches tab reads "unused".
- **Tests:** CadRetainedSketchOwnerTest#d02 (C2). **Status: PASS-RECORDED** (E3).
- **Req:** CAD-085, CAD-087, CAD-088.

### J12 — Edit Sketch staged → Cancel → Finish → Undo / Redo
- **Actions:** CAD body → `edit_cad_sketch` → change the width → `cancel_sketch` → `edit_cad_sketch` → width 4 → Finish → Extrude → Undo → Redo.
- **Expected:** the staged edit does not reach the body; Cancel records nothing; Finish is one step and creates no second body.
- **Undo / Redo:** restore the previous / edited width exactly (Undo/Redo called through JNI in the test).
- **Tests:** SketchUxTest#cadUxR1_32_33_34_35 (viewport + click; FC2-D2). **Status: PASS-RECORDED** (E1).
- **Req:** CAD-069.

### J13 — Revolve New Body (J-C5 positive half)
- **Actions:** New CAD → square + Line → Finish → `sketch_revolve_begin` → tap the Line → angle 90 → Flip → commit → Undo → Redo → save → reopen → reopen the feature → `cancel_sketch`.
- **Expected:** axis = `{line, 0}`; 90° exact; direction negative after Flip; one step; CADB v7 written; reopened feature axis, angle and direction exact; Cancel writes nothing.
- **Tests (components):** CadRevolveOwnerTest#devRev01, #devRev02, #devRev04, #devRev07, #devRev08 (touch for axis and ring; C2). devRev08 starts from the state the earlier methods left, so the journey is spread over methods. **Status: PARTIAL.**
- **Req:** CAD-077..083, CAD-111.

### J14 — Extrude extent Symmetric / Two Sides → exact values → save → Open
- **Tests:** CadExtrudeExtentTest#cadext06a (selector changes the mode), #cadext06b (values land on their side), #cadext06c (drag moves the side it started on), #cadext12 (save and open preserve the extent) (FC2-D2). Four methods → **PARTIAL.**
- **Byte invariant:** a One-Side-only project stays at its older CADB version (host CAD suite `CADEXT`, HOST).
- **Req:** CAD-055..058.

### J15 — Twenty fill cells by real taps → first Extrude
- **Actions:** New CAD → grid sketch → Finish → tap 20 cells → `extrude_sketch`.
- **Expected:** 20 cells selected; preview area = block; Extrude drawn; no project before; after: 1 CAD body of the block volume, 1 component, PlanarFaces selection kind; no "choose one" message.
- **Tests:** CadMultiFaceOwnerTest#devMf04 (viewport taps; commit by `performClick`; C2). **Status: PASS-RECORDED** (E3).
- **Req:** CAD-043, CAD-046..048.

### J16 — Drafting: Construction role + dimensions → Save → reopen exact (CADB v8)
- **Actions:** sketch → make a Construction diagonal → Dimension width / height / Reference length → Done → Finish → Extrude → mark → `project_save` → reopen.
- **Expected / bytes:** slot written; `CADB` v8; reopened body same id; entities and dimension records bit-exact incl. role; no selection persisted.
- **Tests:** CadSketchDraftingOwnerTest#devDr10 (C2; reopen through JNI `loadProject` of the saved bytes). **Status: PASS-RECORDED** (E3).
- **Req:** CAD-027, CAD-029, CAD-102.

### J17 — Add Primitive → gizmo drag → one Undo / Redo
- **Actions:** `objects_capsule_add` → `add_primitive_sphere` → `tool_rail_place` → drag the X shaft (many samples) → Undo → Redo.
- **Expected:** creation one step; drag one step regardless of sample count; no `MeshRevision` published by the drag.
- **Tests (components):** EditorWorkspaceHistoryTest#s01909 (creation), EditorWorkspaceGizmoTest#s02012, #s02023and24, #s02038and39 (OLD). **Status: PARTIAL** (OLD tree).
- **Req:** CON-001, CON-008, CON-010, CON-022.

### J18 — Objects row: Rename, Hide/Show, Lock/Unlock, Duplicate
- **Actions:** Objects → row More → Rename "carrier plate" → commit → Hide → Show → Lock → (typed write) → Unlock → Duplicate.
- **Expected:** each act one step; hidden body keeps its row; the gizmo is withdrawn over the locked body and a transform write is rejected (`APPLY_REJECTED_LOCKED`); unlock restores the gizmo.
- **Tests:** ObjectCommandsTest#e2eObj018a01 (click; FC2-D3). **Status: PASS-RECORDED** (E1). Reopen not asserted; host `OBJ018A` covers SCNE v2 bytes (HOST).
- **Gap:** B-04 (lock + later selection change / Undo) is outside this method.
- **Req:** CON-026..041.

### J19 — Mirror YZ through the row
- **Actions:** place a body → Objects → More → `object_row_mirror` → `object_mirror_plane_yz`.
- **Expected:** chooser creates nothing and records nothing; YZ adds exactly one body with a new id in one step; source unchanged; X negated, Y/Z kept, scale carried.
- **Tests:** MirrorSmokeTest#e2eMirror0101 (click; FC2-D3). **Status: PASS-RECORDED** (E1) for the journey as written; Undo/Redo and byte-equality are host-only (`MIRROR01`, HOST).
- **Req:** CON-048..052.

### J20 — Dimensions, anchors and Relative Scale
- **Tests:** BodyDimensionsSmokeTest#e2eDim020m01 (click; FC2-D3): Dimensions withdraws the gizmo; X = 3 with centre anchor writes no position, one step; negative anchor X = 6 moves the placement, one step; Relative Scale closes Dimensions. **Status: PASS-RECORDED** (E1). Comma entry (F-05) is outside it.
- **Req:** CON-054..060.

### J21 — Delete → Undo restores the same body → Redo removes it
- **Tests:** ObjectsDeleteTest#imp01b18and19 (FC2-D3); #imp01b15and23 asserts the row's Delete is one step. **Status: PASS-RECORDED** (E1) via imp01b18and19.
- **Bytes:** a deleted body is absent from picking, save, autosave and export (#imp01b22, FC2-D3).
- **Req:** CON-042..047.

### J22 — Import GLB → objects → one Undo removes all → Redo
- **Tests (components):** ImportedMeshDurableTest#e2eImp01a01 (picker result becomes objects), #e2eImp01a03 (Undo / Redo through JNI `constructionUndo/Redo`, not the control), #e2eImp01a06 (save → reopen without the GLB, re-encode identical) (FC2-D3). **Status: PARTIAL.**
- **Req:** APP-049, APP-050.

### J23 — Imported Mesh → Start Sculpting → stroke → save / autosave / recovery / reload
- **Tests:** ImportedMeshSculptTest#imp01b11and13 (FC2-D2). **Status: PASS-RECORDED** (E1). Imported arrays immutable: #imp01b04and05.
- **Req:** MOD-050..054.

### J24 — Sculpt Mask → Back → Resume → reopen with an empty mask
- **Expected:** a mask changes no project byte and earns no checkpoint; Resume returns the same mask with Clear Mask offered; reopened SCUL equals the shown geometry, mask empty, sculpt history empty, Clear Mask absent.
- **Tests:** SculptBrushStage025Test#e2eFcm09and10 (FC2-D2). **Status: PASS-RECORDED** (E1).
- **Req:** MOD-008..012, MOD-026.

### J25 — Sculpt History navigator: jump back, new stroke drops the future
- **Tests:** SculptHistoryNavigatorTest#e2eSchnav05 (FC2-D2); jumps equal repeated Undo/Redo in #e2eSchnav02/03; Back closes the navigator first in #e2eSchnav09. **Status: PASS-RECORDED** (E1) for #e2eSchnav05's scope.
- **Req:** MOD-028..031.

### J26 — Save → process death → Open Saved Project
- **Actions:** build a Construction project → `project_save` → process killed → cold launch → `project_open`.
- **Expected:** exact ids, placements and kind; empty history after load.
- **Tests:** ProjectProcessDeathTest#stage1 + #stage2 (FC2-D3). The class is a staged journey whose ordered methods cross a real process death. **Status: PASS-RECORDED (staged class)** (E1). Sculpt variant: #stage3 + #stage4.
- **Gap:** after a relaunch the saved project can only be reopened from inside a project; Home has no Open Saved Project (A-03, BLOCKED-OWNER).
- **Req:** APP-027, APP-030, APP-032.

### J27 — Unsaved edit → process death → Recover
- **Tests:** ProjectProcessDeathTest#stage5 + #stage6 (Construction), #stage7 + #stage8 (Sculpt) (FC2-D3): the cold launch offers recovery; `recovery_recover` (click) restores exact values, placement, renderable and pickable; fresh history; candidate retired. **Status: PASS-RECORDED (staged class)** (E1).
- **What it does not check:** that the recovered work is still protected afterwards — see N13.
- **Req:** APP-053..056.

### J28 — Change every preference → project bytes identical
- **Tests:** SettingsPreferencesTest#uiprefr1_09_10_11_37 (FC2-D3). **Status: PASS-RECORDED** (E1) for bytes. The *saved baseline* surviving the palette recreation is not asserted (N21).
- **Req:** APP-065, UI-060.

### J29 — Six-body project → Export → one valid GLB
- **Tests:** GlbExportTest#e2er1c01 (independent `GlbDocument` reader), #e2er1c05 (cancel writes nothing), #e2er1c06 (no project state or slot touched) (OLD). **Status: PASS-RECORDED (OLD tree)** for e2er1c01's scope. A face-supported body is not in that fixture (N18).
- **Req:** APP-046.

### J30 — Cross-mode: CAD body beside a sculpted Imported Mesh, Construction Undo/Redo
- **Tests:** SketchExtrudeTest#e2eCadr0_16_importedMeshSculptUndoRedoStillWorksBesideACadBody (OLD). **Status: PASS-RECORDED (OLD tree)**. Host: CONSTRUCTION_HISTORY `IMP01B` 33 checks (Construction Undo never moves a sculpt vertex) (HOST).
- **Req:** CON-065, MOD-055, MOD-065.

### J31 — Cross-mode: CAD producer + face dependent + Freeform + Surface → object acts → save → kill → reopen (JR-J2)
- **Actions:** rename, lock, hide, duplicate, delete the dependent, then try to delete the producer; Save; kill; reopen; Undo-all in session.
- **Expected:** each state equals its pre-kill fingerprint; Undo-all walks back exactly; producer delete with a dependent refused `RefusedHasDependents`.
- **Tests:** seat J host replay `j4_replay` (E1, scratch, not in CI) over `mixed_cad_face_v2.forge`; it also shows the allocator moves the fingerprint after create + Undo (J-03). No device test. **Status: UNVERIFIED** on device; host PARTIAL.
- **Req:** CON-041, CAD-009, MOD-065, ADV rows.

---

## Negative / fail-closed journeys

### N01 — Open a corrupt `.forge` from Home
- **Initial:** Home, no project. **Actions:** `home_open_file` → pick a damaged file.
- **Expected:** Home stands; `projectOpen()==false`; 0 bodies; status "damaged"; no checkpoint.
- **Bytes:** nothing written.
- **Tests:** HomeFlowTest#e2eAppH1_07 (C2); all 72 fixtures' verdicts (28 refused) in `evidence/local/host_corpus_verdicts.txt` (HOST, E1); CI FAST corpus parity 72/72. **Status: PASS-RECORDED** (E3 device, E1 host).
- **Req:** APP-006, APP-031.

### N02 — Open damaged / truncated / unsupported-major / non-project over an open project
- **Expected:** scene, Undo depth and fingerprint unchanged; slot untouched.
- **Tests:** EditorWorkspaceProjectActionsTest#e2er1a04_aDamagedProjectIsRefusedAndChangesNothing, #e2er1a04_aTruncated…, #e2er1a05_anUnsupportedMajor…, #e2er1a04_bytesThatAreNotAProject… (C2); ProjectTransferTest#fsr1b11_openFileRefusesADamagedDocumentAndChangesNothing (FC2-D3). **Status: PASS-RECORDED** per refusal kind.
- **Not covered:** a file that decodes Ok but fails load (E-05) — the JNI wrapper then has already changed sketch/view/gesture state (E-12). Seat E's J-E7 is UNVERIFIED.
- **Req:** APP-031, DOC-011, DOC-087.

### N03 — Refused Cut and disjoint Add change no byte (J-C2)
- **Actions:** world-plane sketch: Add/Cut absent, Add refused `OperationNeedsTarget`; face sketch: Flip to point a Cut that misses → preview `CutNoIntersection`, Extrude absent, badge names it; commit refused; disjoint Add refused `AddDisjoint`.
- **Undo:** depth unchanged. **Bytes:** `encodeProject()` identical before/after.
- **Tests:** CadVerticalSliceTest#operation_refusal (FC2-D2). **Status: PASS-RECORDED** (E1). CadFillPickR2Test#devR2_05 (C2): a refused disjoint Add keeps every selected cell.
- **Req:** CAD-059, CAD-061, CAD-066, UI-082.

### N04 — Revolve about a crossing axis is refused, then a valid axis commits
- **Expected:** `RevolveProfileCrossesAxis`, status names it, Revolve control absent, a forced commit refused, no project created, still Ready; Change axis → valid axis commits a Revolve.
- **Tests:** CadRevolveOwnerTest#devRev06 (touch; C2). **Status: PASS-RECORDED** (E3).
- **Req:** CAD-078, CAD-081.

### N05 — History edit that breaks a later Cut (J-C3)
- **Expected:** staged rebuild names feature 2 (the Cut) as first failure; later rows "not rebuilt"; regeneration card shown; Extrude refused while failing; Cancel → byte- and fingerprint-identical; Fix returns to the sketch; a valid value withdraws the card and commits.
- **Tests:** CadParametricHistoryOwnerTest#devPar02 (C2). **Status: PASS-RECORDED** (E3).
- **Req:** CAD-094, CAD-100.

### N06 — Last-body Delete; Delete in Sculpt
- **Expected:** the only row has no Delete; native refuses `RefusedLastBody`; in Sculpt Delete is withdrawn and refused; no step, no byte.
- **Tests:** ObjectsDeleteTest#imp01b21_theLastBodyIsRefusedByNameAndItsControlIsNotDrawn, #imp01b21_deleteIsWithdrawnAndRefusedWhileSculpting (FC2-D3). **Status: PASS-RECORDED** (E1), one method each.
- **Related, PARTIAL:** producer with a face dependent → `RefusedHasDependents` is host-only (CAD_A3, HOST) (J-B3).
- **Req:** CON-044..046.

### N07 — Corrupt / unsupported recovery candidate
- **Tests:** ProjectAutosaveRecoveryTest#fsr1b08_aCorruptCandidateIsQuarantinedAndNeverOfferedAgain, #fsr1b08_anUnsupportedMajorCandidateIsAlsoQuarantined; #fsr1b03 (a failed checkpoint preserves the previous valid one) (FC2-D3). **Status: PASS-RECORDED** (E1).
- **Not covered:** a candidate `validateProject` accepts but `loadProject` refuses (E-05).
- **Req:** APP-055, DOC-038.

### N08 — Hidden body cannot Start or Resume Sculpting
- **Tests:** Stage027SculptWorkflowTest#guard2_aHiddenBodyCannotStartSculpting, #guard2_aHiddenSculptBodyCannotResume (FC2-D2). **Status: PASS-RECORDED** (E1).
- **Not covered:** loading a Sculpt project whose active body is hidden enters Sculpt anyway (D-04 / E-09, recorded debt, PARTIAL).
- **Req:** MOD-041, MOD-042.

### N09 — Import GLB in Sculpt
- **Tests:** ImportGlbSculptGuardTest#d1_03_aPickerResultArrivingInSculptIsRefusedNeutrallyThenImportsInConstruction (FC2-D2); #d1_02 (withdrawn in Sculpt, back in Construction); #d1_04 (native refuses by name, changes nothing). **Status: PASS-RECORDED** (E1).
- **Req:** APP-025, MOD-048.

### N10 — Injected device loss costs the project nothing
- **Tests:** DiagnosticsAndRendererLossTest#e2er1b07_aLostDeviceCostsTheProjectNothing, #e2er1b07_theProjectStaysEditableAfterTheDeviceComesBack (debug injection seam; FC2-D3). **Status: PASS-RECORDED** (E1, emulator; a real GPU loss is OWNER-PHYSICAL-UNVERIFIED).
- **Not covered:** a renderer that stops mid-session without a pause is reported only at the next resume (H-01; J-H2 UNVERIFIED — source predicts FAIL).
- **Req:** APP-072, RND-006.

### N11 — Import GLB while a sketch is open (J-A4 = J-D2 = J-E5)
- **Initial:** a project with one body; New Sketch on XY; one rectangle drawn.
- **Actions:** mark → Import GLB… → pick a static GLB.
- **Expected (rule CAD-R0-A1A2):** the row is absent; a direct `importGlbDurable` returns a sketch refusal; body count, active body and Construction undo depth unchanged; Cancel Sketch leaves the project byte-identical.
- **Today:** the row is drawn (`showImportAvailable(!sculpting)`, `EditorWorkspaceView.java:2823`); `commitImportedGlbScene` accepts: `bodies 1→2`, `active 1→2`, `undo_depth=1` (Seat A host diag, E1).
- **Status: UNVERIFIED — source predicts FAIL** (A-04, DEFECT-CONFIRMED seat R2, P2).

### N12 — "Save and continue" before Open Saved Project (J-A2)
- **Initial:** project A saved; edit to state B.
- **Actions:** mark → `project_open` → `unsaved_save`.
- **Expected:** A is not destroyed (Save refused for this leave, or offered as Save Copy).
- **Today:** `saveProjectToSlot` overwrites the slot with B and deletes the checkpoint, then reopens the slot (B). A is gone (E2, `EditorWorkspaceView.java:2665-2678`).
- **Tests:** none (HomeFlowTest#e2eAppH1_10 covers Save-before-Open-File only). **Status: UNVERIFIED — source predicts FAIL** (A-01, P2).

### N13 — Recover, then leave or lose the process (J-A3 / J-H6)
- **Actions:** edit → kill → relaunch → `recovery_recover` → (a) mark → New Project…, or (b) background → process death.
- **Expected (CLAUDE.md "Autosave protects work"):** (a) the unsaved question; (b) the work recoverable on the next launch.
- **Today:** Recover deletes the checkpoint and marks the project persisted (`:5675-5679`); (a) asks nothing, (b) the work is in neither slot nor checkpoint (E2).
- **Tests:** ProjectAutosaveRecoveryTest#fsr1b05 / PRODUCT.md:193-194 pin the current behaviour. **Status: UNVERIFIED — source predicts FAIL** (A-02, P1; owner decision needed because the fix inverts FSR1B-05).

### N14 — CAD bootstrap: System Back with the precision surface or a palette open (J-F1)
- **Actions:** Home → New Project → CAD → draw a rectangle → `precision_toggle` → System Back. Then Finish → tap the action dock → System Back.
- **Expected:** first Back closes the surface (sketch keeps 1 entity, Home not shown); the second closes the palette and Ready stands.
- **Today:** the bootstrap branch of `dismissTopmostSurface` (`:1177-1184`) runs first and cancels the sketch (E2).
- **Tests:** none (HomeFlowTest#e2eAppH1_11 has nothing open). **Status: UNVERIFIED — source predicts FAIL** (F-03, P2).

### N15 — In-project Revolve: System Back on API 36 (J-F2)
- **Actions:** project → New Sketch → XY → rectangle → Finish → `revolve_sketch` path (Revolve…) → `KEYCODE_BACK`.
- **Expected:** revolve ended, Ready on Extrude, `isFinishing()==false`.
- **Today:** `hasDismissibleSurface` (`:1112-1117`) does not include `extrudeInsteadShown`, so no `OnBackInvokedCallback` is armed; the task goes to the background (E2). API 33-35 UNVERIFIED.
- **Status: UNVERIFIED — source predicts FAIL** (F-04, P2).

### N16 — Type "2,5" in a length editor (J-F4)
- **Actions:** staged One Side extrusion → `cad_extrude_depth_value` → type `2,5` → `apply_cad_extrude_depth`. Repeat for `field_sketch_line_length` and `field_body_dimension`.
- **Expected:** 2.5 m, or an explicit refusal; never 25 m.
- **Today:** `DigitsKeyListener("0123456789.-")` drops the comma (`CadExtrudeCanvasView.java:644`, `SketchDimensionLabelView.java:112`, `BodyDimensionLabelsView.java:127`) while the parser accepts it (`LengthUnitTest`, JVM E1).
- **Status: UNVERIFIED — source predicts FAIL** (F-05, P2, E2).

### N17 — Navigator Flip / ±90° (J-C6 = J-F3)
- **Actions:** New CAD → read the camera pose → `sketch_navigator_flip` → read pose → `sketch_navigator_rotate_cw` → read pose.
- **Expected:** view normal reversed, then a 90° roll; authored entity values unchanged; Flip hit area ≥ 48 dp.
- **Today:** `beginSketchView` frames `frame()` not `viewFrame()`; camera byte-identical before and after (Seat C probe P1, E1); Flip 36.2 dp wide (FC2-D1, E1).
- **Tests:** SketchUxTest#cadUxR1_08_09_10 and #cadUxR1_11_12_13 assert only the stored flags (false-green). **Status: UNVERIFIED — source predicts FAIL** (C-01 / F-02, F-01, P2).

### N18 — Face-supported CAD body: Transform and Export (J-B4 = J-E4)
- **Actions:** CAD body → New Sketch on its top cap → New Body → select the dependent → Transform → type Pos X 5 → Apply; then Export.
- **Expected (CAD-A3):** no gizmo, typed write refused; each exported node translation + baked geometry equals the drawn world placement.
- **Today:** gizmo at the origin, typed X accepted with one Undo step and no visible change (B-01, host `diag_face_supported`, E1); export translation `(0,0,0)` vs drawn `(1.5,0.5,0)` for `cad_face_sketch_cap_v2.forge` (E-01, host `export_face`, E1).
- **Status: UNVERIFIED — source predicts FAIL** (B-01 P2, E-01 P1).

### N19 — Sculpt Undo after a stroke too large to retain (J-D1)
- **Actions:** import a ≥ 40k-vertex GLB → Start Sculpting → small stroke → full-mesh stroke (status: not retained) → Undo → Reset Sculpt from Imported Mesh….
- **Expected:** Undo refused or lands on a state that existed; `hasEdits` stays true; Reset asks first; a saved file stores `hasEdits=true`.
- **Today:** Undo lands on a mixed state and clears `hasEdits` (Seat D `diag_d3`, E1: `undo_lands_on_a_state_that_existed=0`), so Reset discards without asking.
- **Status: UNVERIFIED — source predicts FAIL** (D-01, P2).

### N20 — Open another project during a Freeform cage edit (J-D3 negative half)
- **Actions:** Freeform project, Shape held, face 3 selected → mark → Open Saved Project (another Freeform project whose body has the same id).
- **Expected:** cage edit ended and selection empty after the load.
- **Today:** session and selection survive; `pushPull(0.1)` acts on the other project's face 3 (Seat D `diag_d2`, E1).
- **Status: UNVERIFIED — source predicts FAIL** (D-03 = E-07, P3; seat grade only — not panel-verified in `work/VERIFICATION.md`).

### N21 — Save → palette change → leave; no recovery question (J-A6 = J-E3)
- **Actions:** Save Project → Settings → choose another palette (Activity recreated) → back → wait ≥ 0.9 s → New Project….
- **Expected:** no unsaved question (the project is saved); no recovery question at any point.
- **Today:** `everPersisted` / `persistedFingerprint` are view fields lost on recreation, so the question appears (A-05, E2); a recreation re-runs `offerRecoveryIfPresent` and can offer the session's own checkpoint (E-02, E2).
- **Tests:** SettingsPreferencesTest#uiprefr1_05 checks preferences survive recreation, not the baseline. **Status: UNVERIFIED — source predicts FAIL** (A-05 P3, E-02 P2).

### N22 — Hidden CAD body edited through History (JR-J1)
- **Actions:** CAD project, body hidden from its row → `feature_history_action` → Extrude row → type a depth → Finish → Extrude.
- **Rule status:** whether this must be refused is an owner question (OD-J1); no solid preview is drawn for the hidden target (`applyCadOperationPreviewLocked` replaces only snapshot items).
- **Today:** `beginEdit=Ok … commitEdit=Ok depth 1.5 → 3.0, undoSteps 1 → 2, visible=0` (Seat J `j1_hidden_edit`, E1).
- **Status: UNVERIFIED — BLOCKED-OWNER** for the refusal; the missing preview is PARTIAL (J-01; seat grade only — not panel-verified in `work/VERIFICATION.md`).

---

## Coverage observations

1. Of 31 positive journeys, 24 are PASS-RECORDED by one method or one staged
   class, 6 are PARTIAL (J02, J09, J13, J14, J17, J22) and 1 is UNVERIFIED on
   a device (J31). Of the 24, 22 rest on a run made in this audit (FC2-D1/2/3)
   or on the C2 run of the identical product tree; 2 (J29, J30) rest on `OLD`
   runs only.
2. Of 22 negative journeys (J07's staged-failure half is also negative), 10 are
   PASS-RECORDED (N01–N10); 12 have no test: 11 are predicted to fail by source
   (N11–N21) and 1 is BLOCKED-OWNER (N22). Of these 12, 10 (N11–N19, N21)
   rest on panel-verified findings; N20 (D-03) and N22 (J-01) rest on seat
   grades only.
3. Reopen is asserted through JNI `loadProject(encodeProject())` in J05, J06,
   J11, J16 and through the drawer Save / Open Saved Project only in J26 and
   ImportedMeshDurableTest#e2eImp01a06. No journey reopens a Freeform or
   Surface project through the drawer after a process death.
4. No device test sends System Back during a staged extrusion, an open palette,
   or a Revolve (N14, N15); `uilr106` is the only real-`KEYCODE_BACK` test and
   it runs in a Construction project (FC2-D1).
5. Physical stylus, pressure, hover, hardware GPU, 16 KB pages, arm64 FP
   behaviour (G-01) and real-device performance are OWNER-PHYSICAL-UNVERIFIED;
   no journey here is a physical-device PASS.
