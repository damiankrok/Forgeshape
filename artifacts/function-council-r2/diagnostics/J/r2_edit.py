import sys
p='/home/user/Forgeshape/artifacts/function-council-r2/SEAT_J.md'
s=open(p).read()
def rep(old,new):
    global s
    if old not in s: sys.exit('MISSING: '+old[:80])
    s=s.replace(old,new,1)
rep("""- **androidTest.** None of these classes ran in this audit's fresh device run
  `40797031704`, which ran 32 UI tests in other classes. The most recent
  aggregate containing them is `FULL_SHARDED_SUITE_PASS` `36607747079` on
  `fa0b6b4`, an **older tree** (E3).""","""- **androidTest.** None of these classes ran in this audit's fresh device run
  `40797031704`, which ran 32 UI tests in other classes. `HomeFlowTest` and
  `EditorWorkspaceProjectActionsTest` ran in C2 DEVICE `40782861799` at
  `30b561b`, 67/67 (`artifacts/cad-r2-c2-picking-seam/TESTS.md:75`), whose
  `app/src/main` and `app/src/androidTest` equal this tree's (`git diff --stat
  30b561b 70a6c14 -- app/src/main app/src/androidTest` is empty) (E3). For the
  other classes below the most recent aggregate is `FULL_SHARDED_SUITE_PASS`
  `36607747079` on `fa0b6b4`, an **older tree** (E3). (Corrected in Round 2,
  objection C-12.)""")
rep("""  - No test drives the real revolve-axis pick (J-05).
""","""  - ~~No test drives the real revolve-axis pick (J-05).~~ Withdrawn in Round 2
    (C-1): `CadRevolveOwnerTest` drives it with real `MotionEvent`s.
""")
rep("### J-01 — P3 — A hidden CAD or Surface body can be edited through History and Edit, with an invisible preview",
    "### J-01 — P3 — A hidden CAD or Surface body can be edited through History and Edit; no solid candidate is drawn for it (Round 2: BLOCKED-OWNER OD-J1 for the refusal question, PARTIAL for the missing solid preview)")
rep("""    row, and drag or type a depth. The viewport shows no preview, and the
    commit lands on a body nobody can see.""","""    row, and drag or type a depth. No solid candidate is drawn (only a target
    already in the snapshot is substituted); the sketch overlay — profile,
    hatch and extrude arrow — is still drawn (`forgeshape_jni.cpp:1988-1992`).
    The commit lands on a body that is not drawn.""")
rep("""- **Impact.** The user edits geometry blind. This contradicts the hidden-body
  policy every sibling path already follows:""","""- **Impact.** The user edits a solid whose result is not drawn. No repo rule
  requires CAD or Surface edits to refuse a hidden body (Stage027 GUARD-2 is
  Sculpt-only; the Dimensions refusal states its own reason; `PRODUCT.md:530-535`
  says a hidden body can still be selected from its row), so this is a
  consistency question for the owner, not a rule breach. Sibling paths that do
  refuse:""")
rep("""  Locked behaves the same way; that half is D-09. Nothing is lost, because
  the commit is one Undo.""","""  The locked half is NOT a defect: `PRODUCT.md:537-542` says a locked body
  refuses only being moved and "Everything else still works", and CLAUDE.md
  Stage 018A names exactly two lock guards, so CAD/Surface commits on a locked
  body are the documented behaviour (PASS-SOURCE, E2). The outlier is
  Freeform's `BodyLocked` (`forgeshape_freeform_session.cpp:277-278`), which no
  owner document states (DOC-DIVERGENCE, OD-J1). Nothing is lost, because the
  commit is one Undo.""")
rep("""- **Evidence and verdict.** E1 for the domain composition (`j1_hidden_edit.out`).
  E2 for the JNI, the preview and the Java gating. **DEFECT-CONFIRMED.**""","""- **Evidence and verdict.** E1 for the domain composition (`j1_hidden_edit.out`).
  E2 for the JNI, the preview and the Java gating. Round 2 (objections C-3,
  C-4): **BLOCKED-OWNER (OD-J1), horizon H2-pending-decision** for whether a
  hidden body's CAD/Surface edit should be refused; **PARTIAL, P3** for the
  missing solid candidate (CAD-VERTICAL-SLICE-R1 "the renderer draws that
  evaluation" does not happen for a target outside the snapshot). Formerly
  DEFECT-CONFIRMED.""")
rep("""  3. Add `convertDisplayUnit` to `FreeformEditorView`, `SurfaceEditorView`
     and the `SketchModifyView` offset field, and call it from
     `onDisplayUnitRequested`.""","""  3. Add `convertDisplayUnit` to `FreeformEditorView` and `SurfaceEditorView`,
     and call it from `onDisplayUnitRequested`.
  4. (Narrowed in Round 2, C-16.) The `SketchModifyView` offset field is
     seeded correctly from native metres through `displayUnit().format(...)`
     on entering Offset mode (`SketchModifyView.java:280-282`); only a unit
     change made while Offset mode is already open is not converted (E2).""")
rep("""- **Evidence and verdict.** E1 (`j4_replay.out`) and E2. **PARTIAL.** The
  behaviour is internally consistent, because the allocator is file truth,
  but nothing documents it as user-facing.""","""- **Evidence and verdict.** E1 (`j4_replay.out`) and E2. Round 2 (C-15):
  **BLOCKED-OWNER (OD-J3), horizon H2-pending-decision**, formerly PARTIAL.
  The behaviour follows the stated rules: the `.forge` rule stores the id
  allocator's high-water mark, so a Save after create + Undo produces
  different bytes, and APP-H1 asks by FINGERPRINT. No repo record states the
  opposite expectation. Does not apply to CAD's `nextSketchId` /
  `nextFeatureId`, which Undo restores (CAD-V6-S1-C1).""")
rep("""- **Impact.** This narrow path, which ext4 and f2fs should not take, turns a
  failed Save into the loss of the saved project, as far as the UI can tell.
  The code comment claims the opposite.""","""- **Impact.** On this narrow path, which ext4 and f2fs should not take, the
  previous slot copy is lost and the newer bytes sit orphaned in
  `.pending`. Current work stays in memory and in the checkpoint, because
  `saveProjectToSlot` returns before `ProjectCheckpoint.clear` on a failed
  write (`EditorWorkspaceView.java:5459-5467`). The code comment's "ONLY copy"
  is never read back. (Reworded in Round 2, C-17.)""")
# J-05 rewrite
a=s.index("### J-05")
b=s.index("### Cross-seat re-verification")
s=s[:a]+"""### J-05 — P3 — A few device tests reach state through JNI seams the product never calls (narrowed in Round 2)

- **Where.** The JNI census in `native_usage.txt` covers 275 natives.
  - **66 are never called from product Java.**
  - **Seams used as setup in device tests** (Round 2, C-1/C-2: restricted to
    these):
    - `sketchSetRevolveAxis` as setup in `CadParametricHistoryOwnerTest.java:241`
      and `CadRetainedSketchOwnerTest.java:1262`;
    - `sketchToggleRegion` in `CadPlanarFaceRuntimeTest.java:529`
      (`toggleFace`). `CadFillPickR2Test.nativeToggle` (`:933-940`) also calls
      it, but only as setup; that class drives its acceptance taps with
      `realGesture` (9 call sites).
  - **Withdrawn (Round 2, C-1).** The earlier claim that the real revolve-axis
    pick is untested on the device was wrong. `CadRevolveOwnerTest` opens
    Revolve with `pressInSketchEditor(R.id.sketch_revolve_begin)` (`:555-566`),
    asserts `REVOLVE_AXIS_PICKING == 1` (`:129-131`), chooses the axis with
    `tapAxis` → `realGesture`, which dispatches real `MotionEvent` DOWN/UP
    through the decor view (`:619-636`, `:725-747`), and `devRev05` presses
    `R.id.sketch_revolve_change_axis` and taps again (`:309-321`). That class
    passed in C2 DEVICE `40782861799` at `30b561b`, 67/67, on a product and
    test tree equal to this one (E3). `CadSketchDraftingOwnerTest` chooses
    dimension targets with real taps (`:222-237`); its `sketchSetDimensionTarget`
    block (`:344-354`) is bulk setup.
  - **DOC-DIVERGENCE (test javadoc).** `CadPlanarFaceRuntimeTest.toggleFace`
    (`:521-529`) says it "adds a face through the precision surface's row
    path". No product Java calls `sketchToggleRegion` (only its declaration,
    `NativeViewport.java:2835`), and the real row makes the tapped region the
    ONE region (`SketchEditorView.java:538-545`). The javadoc describes a
    product path that does not exist.
  - **Eight natives are called by neither product nor tests:**
    - `supportChooserSelect`, `supportChooserConfirm`;
    - `freeformSelect`, `freeformClearSelection`;
    - `sculptUndo`, `sculptRedo`, `sculptRedoAvailable`;
    - `sketchToggleSelectEntity`.
- **Impact.** Where a seam is setup, the device evidence after it proves the
  domain, not the gesture that would have produced that state. The eight
  orphan exports are untested ABI surface.
- **Evidence and verdict.** E2. **PARTIAL** for seam-as-setup coverage; the
  orphans are recorded only (IMPLEMENTED-UNTESTED). The real axis pick is
  PASS-SOURCE-AND-RECORDED-RUNTIME (E3 `40782861799`).
- **Isolated fix.**
  - Correct the `toggleFace` javadoc.
  - Either delete the eight orphan natives or cover them.
  - Note in each seam-using test which UI act it stands for.

"""+s[b:]
rep("""  and reads back the slot just written. E2.""","""  and reads back the slot just written: an **unannounced overwrite** of the
  project about to be opened (the prompt says "Save the project to this device
  first", `strings.xml:63`, but not that it replaces that project). E2.""")
rep("""- **A-04 / C-02 / D-02 / E-03** (Import during a sketch) are not re-run here.
  The `objectCommandsBlockedByMode` guard (`forgeshape_jni.cpp:3408`) covers
  object commands, not `importGlbDurable` (8560). This is consistent with them.""","""- **A-04 / C-02 / D-02 / E-03** (Import during a sketch) are not re-run here.
  The `objectCommandsBlockedByMode` guard (`forgeshape_jni.cpp:3408`) covers
  object commands, not `importGlbDurable` (8560; it guards Sculpt only,
  8594-8600, while `sceneAddBody` refuses `in_sketch`, 3246-3265). The rule is
  the delivered **H0** CAD-R0-A1A2 guard; E1 is Seat A's
  `SEATA_IMPORT_DURING_SKETCH=ACCEPTED` (`SEAT_A.md:134`). (Round 2, C-5.)""")
rep("""  - The guard's absence is E2. The user consequence is E5.""","""  - The guard's absence is E2. The user consequence is E5.
  - Scope (Round 2, C-18): only gizmo and Freeform drags can be caught. CAD
    extrude and revolve drags are session-only and write no `CadBodyState`
    until commit (CAD-UX-S1), so they never reach a checkpoint.""")
rep("""2. **Every object-command refusal changes nothing.** Every probe in J4 and
   J5, including the numeric ones, left the semantic fingerprint and the
   undo depth equal (E1).""","""2. **The four object-command refusals probed (`RefusedHasDependents`,
   `RefusedFaceSupportedCad`, `RefusedInvalidName`, `RefusedNotMirrorable`,
   J4) and the 93 numeric refusals (J5) change nothing**: fingerprint and undo
   depth equal (E1). Delete-last-body, refusals in Sculpt or in a sketch, and
   lock/hide in a sketch were not probed. (Narrowed in Round 2, C-10.)""")
rep("""6. **The JNI enum and array inputs checked here are validated before
   use** (E2). The debug-only camera and history seams are compiled out of
   release builds (`#ifndef NDEBUG`, E2). Release builds carry no self-tests
   (`RELEASE_SELFTEST_GUARD=PASS`, fresh, E1).""","""6. **The JNI enum and array inputs checked here are validated before
   use** (E2). The debug-only seams are **inert in release**, not compiled
   out: they remain exported with no-op bodies (`debugResetConstructionHistory`
   `#ifndef NDEBUG` inside the body, `forgeshape_jni.cpp:7967-7972`;
   `debugCameraPose` the same, `:7893-7911`; `debugInjectDeviceLoss` returns
   `JNI_FALSE`, `:9083`) (PASS-SOURCE, E2; the release guard does not probe
   them). Release builds carry no self-tests (`RELEASE_SELFTEST_GUARD=PASS`,
   E1) — from a rerun at 00:57; `run_status.txt` records the first attempt as
   `guard_exit=2` at 00:54 and does not record the rerun. (Round 2, C-9.)""")
rep("""- No device run in this audit exercised the persistence, recovery or JNI
  hardening classes. Their newest aggregate evidence is from the older tree
  `fa0b6b4` (`36607747079`).""","""- No device run in this audit exercised the persistence, recovery or JNI
  hardening classes. `HomeFlowTest` and `EditorWorkspaceProjectActionsTest`
  ran in C2 `40782861799` on an identical product/test tree; for
  `ProjectAutosaveRecoveryTest`, `JniBoundaryHardeningTest` and
  `ObjectCommandsTest` the newest aggregate is from the older tree `fa0b6b4`
  (`36607747079`).""")
rep("""- **OD-J1.** Should CAD and Surface edits (Edit Sketch, History rows, Keep
  Sketch, Add/Cut, Surface value and Thicken) be refused over a hidden body,
  as Freeform, Sculpt and Dimensions are? Should they also be refused over a
  locked body, as Freeform is? (Seat D's D-09 covers lock.)""","""- **OD-J1.** Should CAD and Surface edits (Edit Sketch, History rows, Keep
  Sketch, Add/Cut, Surface value and Thicken) be refused over a hidden body,
  as Freeform, Sculpt and Dimensions are? And is Freeform's undocumented
  `BodyLocked` refusal intended, given `PRODUCT.md:537-542` ("Everything else
  still works")? (Seat D's D-09 covers lock.)""")
open(p,'w').write(s)
