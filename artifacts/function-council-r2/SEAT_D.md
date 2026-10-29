# FUNCTION-COUNCIL-R2 — Seat D: Sculpt / Freeform / Surface / representation boundaries

Audited product tree: `app/src/main` at `70a6c14f703bfda2ab689b855126c1d12a541e41`
(audit branch HEAD `9cfc3f5` adds only `artifacts/function-council-r2/BEFORE.md`;
`git diff --stat 70a6c14 HEAD -- app` is empty). Research only: nothing under `app/`,
`testdata/`, `scripts/`, `.github/`, Gradle files or the owner documents was modified, no
Gradle was run, no branch-changing git command was run. Requirement rows:
`work/req_D.tsv` (114 rows, `MOD-001`..`MOD-114`).

## Scope

Sculpt: the seven brushes (`SculptStroke`), Mask and Clear Mask, `SculptHistory` and the
History navigator jump, sculpting an Imported Mesh (`IMPORT-01B`), stroke cancellation,
Isolate (Stage027), mode restrictions, persistence of the Frozen Sculpt Mesh against the
volatile mask and history. Freeform (`forgeshape_freeform*`): cage invariants, V/E/F
selection, cage gizmo, Push/Pull, Extrude, Insert Edge Loop, Crease, Delete Face, Symmetry,
Catmull-Clark levels, history, `FRFM`. Surface (`forgeshape_surface*`): PlanarPatch,
ExtrudedSurface, RevolvedSurface, LoftSurface, planar Trim, Stitch, Thicken, applicability
limits, feature list, `SURF`. Every mode transition between Construction, Sculpt, sketch,
Freeform cage editing and project replacement, for safe refusal vs silent loss, invalid
mixed states, import during Sculpt/sketch, history ownership, and Freeform/Surface bodies as
CAD sketch supports. CAD regeneration, the shell journeys, the codec layout and the renderer
belong to other seats and were read only where this domain depends on them.

## Sources read (exact paths, revisions)

- `CLAUDE.md` (worktree at `9cfc3f5`; rules `SCULPT-FCM-R1`, `ARCH-OWNER-12`, `SCULPT-H1`,
  Stage027 Isolate/GUARD-1/GUARD-2, `IMPORT-01A`/`IMPORT-01B`, `MODELING-FOUNDATIONS-R1` B/C,
  `CAD-R0-A1A2` sketch-mode refusals, `CAD-A3` support identity, Stage 018A, `MIRROR-01`,
  export rules, "a control that cannot succeed is not drawn").
- `PROJECT_STATUS.md` (`70a6c14`): header closeouts (lines 1-200, incl. `MODELING-FOUNDATIONS-R1`
  at 133-170), Owner Decision Baseline (2486-2606), Known Issues (4018-4215, STYLUS-G1 at
  ~4115), Technical Debt (Stage027 GUARD-2 load-path debt at 4265-4269).
- `PRODUCT.md`: History (963-1027), Freeform (1029-1046), Surface (1048-1074), Sculpting
  (1568-1840), Export (2060-2090).
- `ARCHITECTURE.md`: Clear Mask (3670-3679), export representation (4610-4612), module table
  (183-186).
- `artifacts/function-council-r2/BEFORE.md`, `SEAT_A.md`, `SEAT_B.md` (for overlap only).
- `artifacts/modeling-foundations-r1/SUMMARY.md`; `artifacts/function-council-r1/COUNCIL_FINDINGS.md`
  (old D1, D4, D6 re-checked below); `artifacts/function-council-c1-d1/TEST_EVIDENCE.md`;
  `artifacts/cad-vertical-slice-r1/FULLSHARDED_C4.md`; `artifacts/cad-r2-c2-picking-seam/TESTS.md`.
- Fresh audit outputs (read, not produced by this seat):
  `/tmp/claude-0/-home-user-Forgeshape/2f5454c6-63b1-52b8-b92c-5f8b69c420c6/scratchpad/fc2/host_selftests.txt`
  (`HOST_SELFTESTS_OK (4392 checks, 0 failed)`; `SCULPT_BRUSH_KERNEL` 655, `FREEFORM` 74,
  `SURFACE` 40, `PROJECT` 256, `GLTF_EXPORT` 93), `host_corpus_verdicts.txt` (72 fixtures),
  `gradle.txt`/`gradle_unit_rerun.txt`, `release_guard.txt` (`RELEASE_SELFTEST_GUARD=PASS`),
  `run_status.txt`; JVM XML `app/build/test-results/testDebugUnitTest/TEST-com.forgeshape.app.{Freeform,Surface}PresentationTest.xml`
  (timestamps 2026-10-29T00:57, 6/0 and 7/0).
- Not available: Master Plan v4.1.61, OWNER_DECISIONS_*, MASTER_PLAN_HISTORY_*, PROJECT_STATE.
  Horizons are reconstructed: H0 = Stage 015B/E2E-era sculpt and persistence; H1 =
  `SCULPT-FCM-R1`, `ARCH-OWNER-12`, `SCULPT-H1`, Stage027, `IMPORT-01B`,
  `MODELING-FOUNDATIONS-R1`; H2 = each rule's "Not this stage" list.

## Inspected code paths

| File:lines | What |
| --- | --- |
| `forgeshape_sculpt.h:87-171, 790-1114` | tool enum, brush constants, `FrozenSculpt`, `SculptSession` API (Isolate, history, Clear Mask) |
| `forgeshape_sculpt.cpp:159, 183, 465-600, 1300-1322, 1427-1479, 1537-1600, 1640-1770` | `sculptToolMovesGeometry`, `sculptMaskFactor`, `freezeFrom` (mask zeroed 487-497), `applyMask`, freeze/enterConstruction/enterSculpt, stroke record, undo/redo, `canClearMask`/`clearMask`, jump |
| `forgeshape_sculpt_history.cpp:15-25, 80-101` | `payloadBytes` (28 B/vertex geometry, 12 B/vertex mask), `record` (NotRetained keeps undo, clears redo) |
| `forgeshape_scene.cpp:522-560, 629` | `buildSculptSourceMesh` (CAD/Surface/Freeform → false), `viewSceneSnapshot` |
| `forgeshape_jni.cpp:2871-2989` | `freezeToSculpt` (hidden, Surface, Freeform, CAD, in_sketch refusals) |
| `forgeshape_jni.cpp:2991-3034` | `enterConstructionMode`, `enterSculptMode` (hidden refusal only) |
| `forgeshape_jni.cpp:3051-3085` | `setSculptIsolate`, `sculptIsolated` |
| `forgeshape_jni.cpp:3244-3266, 4113-4125, 7995-8030, 8191-8285` | add-body / sketch-begin / Construction-history refusals in Sculpt and sketch; Clear Mask; Undo/Redo routing |
| `forgeshape_jni.cpp:5442-5625, 9803-9837` | Freeform JNI (`freeformModeRefusalLocked`), touch arbitration |
| `forgeshape_jni.cpp:5813-6060` | Surface JNI (`kSurfaceRefusedInSculpt`, begin/commit/stitch/thicken/value) |
| `forgeshape_jni.cpp:4488-4500, 7569-7596` | CAD commit / Keep Sketch refuse while `SurfaceSketchPurpose` is set |
| `forgeshape_jni.cpp:8440-8496, 8506-8520, 8560-8606, 8968-8990` | `loadProject`, `exportGlb`, `importGlbDurable`, `closeProject` |
| `forgeshape_freeform_session.h` / `.cpp:261-450, 614-830` | session begin/end/reconcile/select/tap/drag/tools, `createFreeformBody` |
| `forgeshape_freeform.h:100-372` | status codes, bounds, tool signatures |
| `forgeshape_surface.h:60-126, 185-361`; `forgeshape_surface_authoring.cpp:35-420` | status codes, state/mesh types, authoring acts |
| `forgeshape_support_chooser.cpp:40-50, 145-247` | support only on CAD faces; non-CAD surface occludes plane |
| `forgeshape_project_document.cpp:1110-1175`; `forgeshape_project_state.cpp:226-230, 405-430` | `SCUL` validation (refused over CADB/FRFM/SURF), `hasEdits` capture, load enters Sculpt |
| `forgeshape_gltf_export.cpp:200-300` | per-representation export; sculpt only when `kind == Sculpt` |
| `forgeshape_body_commands.cpp:58-80, 215-317`; `forgeshape_body_mirror.cpp:183-200` | `cloneFrozenSculpt`, Duplicate per representation, Mirror eligibility |
| `forgeshape_import_commit.cpp:50-63` | import refuses only an open edit |
| Java `GlobalToolbarView.java:825-905` | Start/Resume/Back visibility |
| Java `EditorWorkspaceView.java:2440-2455, 2635-2662, 2776-2830, 5267-5320, 5483-5513, 5835-5935, 6088-6112, 6354-6368` | New Freeform project, leave/open flows, `syncFromNative` (`showImportAvailable(!sculpting)`), sculpt transitions, import, `refreshFreeformEdit` |
| Java `ProjectDrawerPolicy.java:100-170`; `SculptContextView.java:178-180, 243-260, 290-305`; `FreeformEditorView.java:196-266`; `res/values/strings.xml:742-751` | drawer policy, Clear Mask / Reset / Isolate, Freeform chips, mask strings |

## Test coverage of the domain (what it actually asserts)

- **Host native, fresh in this audit** (`host_selftests.txt`): `SCULPT_BRUSH_KERNEL` 655
  checks — tool indices, falloff, Grab/Clay/Smooth/Inflate behaviour, `FCM_01..19` (Flatten,
  Crease, Mask exact ends, per-body mask, lifecycle, mask history side, Clear Mask Ok /
  NothingToDo / NotSculpting, mask not in fingerprint and not reopened), `SCUNDO_*` (one stroke
  one entry, caps, NotRetained with an EMPTY undo stack `SCUNDO_23`, Freeze clears, per body,
  hasEdits stored), `SCHNAV_01..12` (rows = states, jump = repeated undo/redo, refusals),
  `IMP01B_*` (imported seed bit-exact, imported arrays untouched, never stale). `FREEFORM` 74:
  `FF_01..22` + `FF_SESSION_*` + `FF_PERF`. `SURFACE` 40: `SURF_01..24`, `SURF_AUTH_01..08`,
  `SURF_CODES`, `SURF_PERF`. `GLTF_EXPORT` `FSR1C_09_*` asserts Sculpt vs Construction export
  kinds. Not asserted anywhere: Undo after a NotRetained stroke with a non-empty undo stack;
  Clear Mask `EntryTooLarge`; GLB export of Freeform/Surface bodies; any Freeform/Surface/Sculpt
  interaction with a project load.
- **Corpus, fresh** (`host_corpus_verdicts.txt`): `freeform_box_v1` Ok,
  `freeform_crease_symmetry_v1` Ok, `freeform_bad_topology_v1` InvalidSemanticValue,
  `surface_patch_extrude_v1` Ok, `surface_loft_trim_stitch_v1` Ok, `surface_bad_ref_v1`
  InvalidSemanticValue, `imported_sculpt_v1`, `mixed_imported_sculpt_v1`, `sculpt_mixed_v1` Ok.
  PowerShell parity was not run locally (no `pwsh`); it is CI FAST's.
- **JVM, fresh**: `FreeformPresentationTest` 6/0 (transform modes need a selection, editing
  wanted only under Shape, face/edge tool visibility, symmetry toggle, refusal sentences — no
  availability case for symmetry/level chips); `SurfacePresentationTest` 7/0 (Finish kinds drawn
  exactly when the candidate succeeds, Stitch/Thicken only over a succeeding candidate, staged
  edit rules).
- **Device, historical (E3)**: `FreeformOwnerTest` devFf01..03 and `SurfaceOwnerTest`
  devSurf01..03 in `CI DEVICE 37472535341` on `30833aa` (2026-10-06, 54/54).
  `ImportGlbSculptGuardTest` d1_02..04 and `ImportedMeshSculptTest` (9) in `CI DEVICE
  36643101662` on `f29f482`. `SculptBrushStage025Test` (8), `SculptUndoTest` (10),
  `SculptHistoryNavigatorTest` (9), `Stage027SculptWorkflowTest` (4),
  `EditorWorkspaceSculptRetentionTest` (2), `EditorWorkspaceHistoryTest` s01920/s01921 last
  ran in the full aggregate `FULLSHARDED_C4` (`fa0b6b4`, 2026-09-29) — before
  `MODELING-FOUNDATIONS-R1` changed `freezeToSculpt` (`d1a0671`/`82239bf`). None of the CAD-R2 /
  C1 / C2 device unions include a Sculpt, Freeform or Surface class. Emulator only; no physical
  device evidence exists for any of this.

## Commands run by this seat

All in `/tmp/claude-0/fc2-scratch/D/` (never added to the build), linking against the fresh
host objects of the audited tree:

```
CPP=/home/user/Forgeshape/app/src/main/cpp; OBJ=/home/user/Forgeshape/build/host-selftests/obj
g++ -std=c++17 -O1 -I$CPP -I$CPP/third_party/manifold/include -DMANIFOLD_PAR=-1 \
    -DMANIFOLD_NO_IOSTREAM -DMANIFOLD_NO_FILESYSTEM diag_dN.cpp $OBJ/*.o \
    /home/user/Forgeshape/build/host-selftests/kernel/libmanifold.a -lpthread -o diag_dN
./diag_dN > diag_dN.out      # N in {"", 2, 3, 4, 5}
python3 -I gen_req.py /home/user/Forgeshape/artifacts/function-council-r2/work/req_D.tsv
```

Outputs: `diag_d.out` (Freeform/Surface parameter edges, symmetry exactness, Clear Mask cap),
`diag_d2.out` (Freeform session across a load), `diag_d3.out` (Undo after a NotRetained
stroke), `diag_d4.out` (hidden-body Sculpt load), `diag_d5.out` (Freeform/Surface export).

## Confirmed findings

### D-01 — P2 — Undo after an over-cap (NotRetained) stroke lands on a state that never existed and clears `hasEdits`

- **Where:** `forgeshape_sculpt_history.cpp:80-101` (`record`: a NotRetained delta clears
  redo but KEEPS the undo stack); `forgeshape_sculpt.cpp:1640` (`undoStroke` writes the entry's
  absolute before-positions and before-`hasEdits`); consumers `SculptContextView.java:243`
  (Reset confirmation asks only when `SCULPT_HAS_EDITS`), `forgeshape_project_state.cpp:230`
  (`SCUL` stores `hasEdits`), `:850` (fingerprint mixes it).
- **Minimal repro (E1, `diag_d3.out`):** 40,000-vertex frozen grid; stroke 1 lifts vertices 0
  and 1 (recorded); stroke 2 lifts every vertex by 5 (payload > 1 MiB → `not_retained`);
  `undoStroke()` → `ok`, then z0 = z1 = 0.000 while z2 = 5.000, `hasEdits=0`.
  `RESULT undo_lands_on_a_state_that_existed=0`.
- **Impact:** the mesh holds a mix of two states that was never on the branch; `hasEdits` now
  says "not sculpted" while stroke 2's deformation is on screen, so **Reset Sculpt from Shape
  discards it with no confirmation** (`UI-OWNER-05`), and a save writes `hasEdits=false`
  beside edited positions. Reachable only when one stroke touches more than ~37,449 vertices
  (1 MiB / 28 B) — a dense imported GLB with a large radius. Re-check of
  `function-council-r1` D4: still open on this tree.
- **Evidence:** E1 (domain, host) + E2 (consumers). **Verdict: DEFECT-CONFIRMED.**
- **Fix (isolated):** in `SculptHistory::record`'s NotRetained branch also clear `undo_`
  (Undo cannot cross an unretained stroke, exactly as it cannot cross a Freeze), and add a host
  check `SCUNDO_23b` with a non-empty undo stack. Owner decision D-OD1 if a different policy
  (refuse the stroke) is preferred.

### D-02 — P2 — Import GLB is offered and accepted while a sketch is open

- **Where:** `EditorWorkspaceView.java:2823` `projectPopover.showImportAvailable(!sculpting)`;
  `ProjectDrawerPolicy.java:107` `markShown` keeps the mark while sketching;
  `forgeshape_jni.cpp:8590-8597` refuses only `inSculptMode()`;
  `forgeshape_import_commit.cpp:50-63` refuses only an open Construction edit.
- **Minimal repro (E2, source):** open project → New Sketch (CAD or Surface) → ForgeShape mark →
  Import GLB… → pick a valid `.glb`. Nothing on either side of JNI asks `sketchSession().active()`
  or `supportChooser().active()`, so the import creates bodies, makes the first one active and
  records a Construction step while the sketch stays open. The neighbouring entry points refuse
  the same moment by name: `sceneAddBody` (`jni:3265 in_sketch`), body select (`jni:3221`),
  Construction Undo/Redo (`jni:8014`), Freeform create (`jni:5444-5446`).
- **Impact:** violates the `CAD-R0-A1A2` rule "creation, deletion, body switching … are refused
  below JNI and withdrawn above it" while a sketch is drawn, at both layers; a Construction step
  is recorded under the open sketch, the very thing `runHistoryStep`'s comment says must not
  happen. No data loss was demonstrated. Not reproduced on a device in this audit. Extends
  the old council's D1 (fixed for Sculpt by C1-D1) to the sketch mode.
- **Evidence:** E2. **Verdict: DEFECT-CONFIRMED.**
- **Fix (isolated):** native `importGlbDurable` refuses `RefusedInSketch` when a sketch or the
  chooser is active; Java `showImportAvailable(!sculpting && !sketching && !chooserActive)`;
  one device case (open sketch → import → refused, body count and undo depth unchanged).

### D-03 — P3 — The Freeform edit session and its selection survive opening another project

- **Where:** `forgeshape_jni.cpp:8462-8479` (`loadProject` cancels a sketch and drops a pending
  stroke, but never ends `freeformEditSession()`); `forgeshape_freeform_session.cpp:299-308`
  (`reconcile` keeps the session when a body with the same ObjectId is Freeform, visible,
  unlocked and active); `EditorWorkspaceView.java:5483-5513, 6088-6112` (Open Saved Project /
  Open File load directly over the live project) and `:6354-6363` (`refreshFreeformEdit` sees
  the session still active and does not re-begin it). `closeProject` (`jni:8968-8990`) also
  does not end it explicitly (it ends lazily because the scene is empty).
- **Minimal repro (E1, `diag_d2.out`):** P1 = Freeform box id 1, cage editing open, face 3
  selected; `loadProjectDocument(P2)` where P2 is another Freeform box with id 1 → `sessionActive=1
  selection=1 firstSel=3`; `pushPull(0.1)` → `Ok undo=1` on P2's face 3.
- **Impact:** presentation state from the previous project leaks into the opened one; the
  gizmo stands on a face the user never selected there and a typed tool acts on it. Contradicts
  the session's own contract ("Opening resets … the selection to empty, every time",
  `forgeshape_freeform_session.h:89-92`). Every New Project → Freeform starts at ObjectId 1, so
  coincident ids are the ordinary case. Device repro not performed.
- **Evidence:** E1 (domain) + E2 (shell path). **Verdict: DEFECT-CONFIRMED.**
- **Fix (isolated):** call `freeformEditSession().end()` under the lock in `loadProject` and
  `closeProject` (beside the sketch cancel); add a host check.

### D-04 — P3 — The load path enters Sculpt over a hidden active body (recorded debt, now reproduced)

- **Where:** `forgeshape_project_state.cpp:419-423` (`enterSculpt()` for a Sculpt-kind
  document, no visibility question) vs. `jni:2885-2894, 3011-3019` (GUARD-2 on Start/Resume).
  Recorded as debt in `PROJECT_STATUS.md:4265-4269`.
- **Minimal repro (E1, `diag_d4.out`):** a Construction body frozen, hidden, captured as
  `ProjectKind::Sculpt` → `encode=Ok bytes=582 decode=Ok load=Ok inSculpt=1 activeVisible=0`.
- **Impact:** a valid file reopens in Sculpt over a body nobody can see. Not producible
  in-product (Hide is refused in Sculpt and Start/Resume refuse a hidden body); needs a foreign
  or hand-built file.
- **Evidence:** E1. **Verdict: DEFECT-CONFIRMED** (known debt). **Fix:** open such a document in
  Construction (or refuse it) — owner decision D-OD4.

### D-05 — P3 — Clear Mask is drawn and then refused for a large mask; the reason is not shown; such a mask cannot be cleared in-session

- **Where:** `forgeshape_sculpt.cpp:1687` (`canClearMask` omits the size test) and
  `:1736-1743` (`EntryTooLarge`); `SculptContextView.java:178-180, 295-300`;
  `strings.xml:751` ("Could not clear the mask. Nothing changed."); `ARCHITECTURE.md:3678-3679`
  claims the control is ABSENT.
- **Minimal repro (E1, `diag_d.out`):** 90,000 masked vertices → `canClearMask=1
  clearMask=entry_too_large maskedAfter=90000 undo=0`; 78,400 masked vertices clear normally.
- **Impact:** a control drawn that cannot succeed (`CLAUDE.md`), a generic message, and — since
  the Mask brush only adds weight (`applyMask`, `sculpt.cpp:1300-1322`) and a mask stroke over
  the per-entry cap is NotRetained — a mask over ~87,371 vertices on a dense imported mesh can be
  neither undone nor cleared; only Reset (destroys sculpting) or save-and-reopen removes it.
  Re-check of old council D6: still open.
- **Evidence:** E1. **Verdict: DOC-DIVERGENCE** (ARCHITECTURE.md vs code) plus a UX gap.
  **Fix:** include the payload test in `canClearMask` (or correct ARCHITECTURE.md) and name the
  reason in the status line; the larger question is owner decision D-OD2.

### D-06 — P3 — PRODUCT.md overstates which bodies export their sculpted shape

- **Where:** `PRODUCT.md:2081-2082` ("A body you have been sculpting exports its sculpted
  surface …") vs `forgeshape_gltf_export.cpp:203-212` (sculpt mesh only when `kind ==
  ProjectKind::Sculpt`) and `jni:8515` (kind = in Sculpt mode). `PRODUCT.md:1691` and
  `ARCHITECTURE.md:4610-4612` state the mode rule correctly.
- **Repro:** sculpt a body, Back to Construction, Export → the Construction shape is written
  (asserted by host `FSR1C_09_*`, fresh run passing).
- **Evidence:** E1 (host check) + E2. **Verdict: DOC-DIVERGENCE.** **Fix:** reword
  `PRODUCT.md:2081` to "while you are sculpting".

### D-07 — P3 — Resume Sculpt has no native sketch guard

- **Where:** `forgeshape_jni.cpp:3007-3034` (`enterSculptMode` asks only visibility) vs
  `freezeToSculpt` `jni:2933` (`in_sketch`) and `sketchBegin` `jni:4119` (refused in Sculpt).
  Java withdraws Resume while sketching (`GlobalToolbarView.java:898`).
- **Impact:** a direct or late call would put the product in Sculpt with a sketch open — the
  mixed state every other entry point refuses. Not reachable from the drawn UI.
- **Evidence:** E2. **Verdict: PARTIAL.** **Fix:** refuse `in_sketch` (and chooser) in
  `enterSculptMode` as `freezeToSculpt` does.

### D-08 — P3 — Freeform Symmetry and Subdivision chips are drawn when the act would be refused

- **Where:** `FreeformEditorView.java:196-231, 245-266` (`refreshFromNative` only sets the
  chips' active state); refusals `CageNotSymmetric`, `SubdivisionBudgetExceeded`
  (asserted by host `FF_14_symmetry_on_an_asymmetric_cage_is_refused…`, `FF_05_*`).
- **Impact:** contradicts "a control that cannot succeed is not drawn"; the refusal is named,
  so nothing is lost.
- **Evidence:** E1 (host) + E2. **Verdict: PARTIAL.** **Fix:** expose per-plane and per-level
  availability in `freeformState` and hide failing chips; add a JVM case.

### D-09 — INFO — Lock means different things per representation

- `FreeformEditSession::begin`/`reconcile` (`freeform_session.cpp:278, 302`) refuse every
  cage edit on a locked body ("placement and form are both held"); Surface value/Stitch/Thicken
  (`surface_authoring.cpp:79-110`, `jni:5976-6040`) and CAD feature edits do not ask. `CLAUDE.md`
  Stage 018A says a lock refuses being MOVED. E1 (`FFD_locked_body_refuses_tool` →
  `NotEditing`) + E2. **Verdict: BLOCKED-OWNER** (D-OD3).

### D-10 — P3 — Coverage gaps

- No device class for Sculpt/Freeform/Surface has run on the current tree (see coverage);
  the newest Sculpt device evidence predates the `freezeToSculpt` change of 2026-10-06.
- No suite asserts: D-01's scenario, Clear Mask `EntryTooLarge`, GLB export of a Freeform or a
  Surface body (diag_d5: `capture=Ok`, 14,844 B, both `doubleSided=1`), the Freeform/Surface
  JNI Sculpt/sketch refusals (`jni:5442-5448`, `5813-6030`), Surface Duplicate, or the
  T-Spline/NURBS vocabulary (strings grep: 0 matches). **Verdict: IMPLEMENTED-UNTESTED** for
  those rows.

## Hypotheses (unproven)

- **H-1:** With split touch, one finger holding a Freeform cage drag (Construction edit open via
  `history_.beginEdit()`, `freeform_session.cpp:533`) and another tapping a drawer row could ask
  for a load or close mid-edit. `loadProjectDocument` refuses an open edit
  (`forgeshape_project_state.h:108-110`), so a load would fail closed; `closeProject` does not
  refuse one (old council D7). Not reproduced.
- **H-2:** D-02 during an Edit Sketch of a face-supported CAD body might interact with the
  Edit Sketch chip or the target body binding after the active body switches; the commit uses the
  session's stored body id, so no corruption is expected. Not reproduced.
- **H-3:** a mask stroke NotRetained (D-05) combined with D-01 could restore an older mask side
  over a newer unrecorded mask; same mechanism as D-01, mask side, not separately run.

## Adversarial counterexample attempts

| # | Attempt | Outcome |
| --- | --- | --- |
| 1 | Freeform Symmetry X, translate vertex 1 by (0.1234567891234, 0.0987654321, −0.333333333333) | Held: partner bit-exact mirror, cage validates (`diag_d.out`) |
| 2 | Insert a loop at 0.5 under Symmetry X, push the on-plane vertex by (0.2, 0.1, 0) | Held: x stays exactly 0; ratio 0.3 refused `SymmetryRequiresMidpoint` |
| 3 | Freeform distances NaN/inf/0/1e-7/2e5, ratios NaN/0/1/−0.1/1e-12, creases NaN/−0.01/1.01, levels −1/5/255, delete all faces, zero scale, NaN move | Held: each refused by name (`InvalidDistance`, `InvalidRatio`, `FaceDegenerate`, `NonFinite`, `InvalidCrease`, `InvalidSubdivisionLevel`, `DeleteWouldEmpty`, `TransformDegenerate`) |
| 4 | Surface extrude distance NaN/inf/−0.5/0/1e-9/1e7; revolve angle NaN/−10/0/0.0005/360.0000001/720; thicken NaN/0/1e9; thicken a spline extrude | Held: refused by name; 0.001 and 360 accepted; ±0.2 thickens to a closed 12-triangle solid |
| 5 | Stitch half-gaps 0, 7.5e-7, 9.5e-7, 1.1e-6, 1.5e-6 m | Held: ≤1e-6 stitched with every patch position bit-identical ("nothing averaged"); >1e-6 `StitchGapTooLarge` |
| 6 | Over-cap stroke then Undo | **Broke:** D-01 |
| 7 | Clear a 90,000-vertex mask | **Broke the "absent" claim:** D-05 |
| 8 | Freeform edit session across a project load with coincident ids | **Broke:** D-03 |
| 9 | Load a valid Sculpt-kind file with a hidden active body | **Broke** (known debt): D-04 |
| 10 | Duplicate a masked sculpted body: does the copy inherit the mask? | Held: `cloneFrozenSculpt` seeds through `freezeFrom`, which zeroes every mask (`sculpt.cpp:487-497`) — E2 |
| 11 | Use a Freeform or Surface surface as a sketch support in the chooser | Held: only `cadOrNull()` faces resolve; a nearer non-CAD surface clears the plane behind (`support_chooser.cpp:184-247`) — E2 + host C1_CH |
| 12 | Make a Freeform/Surface body a sculpt source | Held: `buildSculptSourceMesh` false for both (`FFD/SFD_not_a_sculpt_source`), SCUL over FRFM/SURF refused |

## Positive claims with evidence

1. **Representation boundaries hold for Sculpt.** CAD, Freeform and Surface bodies never become
   a sculpt source (`scene.cpp:532-545`), Start Sculpting refuses each by name
   (`jni:2909-2931`), the control is absent (`GlobalToolbarView.java:896`), and the codec refuses
   `SCUL` over `CADB`/`FRFM`/`SURF` (`project_document.cpp:1142`). E1 (host `FF_22_*`, `SURF_24_*`,
   seat diag) + E2.
2. **Freeform Symmetry is exact bit for bit, including on-plane vertices.** E1 (host
   `FF_14/15`, `diag_d.out`).
3. **Surface parameter refusals are named and Stitch never averages.** E1 (host `SURF_*`,
   `diag_d.out`).
4. **Only CAD faces carry sketches; Freeform/Surface surfaces neither support nor let a plane
   behind them be chosen.** E2 + E1 (host chooser checks).
5. **Mask and Sculpt history are volatile and per body; Duplicate carries neither.** E1 (host
   `FCM_12/13/14`, `SCUNDO_15`) + E2 (`cloneFrozenSculpt`).
6. **Freeform and Surface acts are single Construction steps with exact Undo.** E1 (host
   `FF_18_*`, `SURF_21`, `SURF_AUTH_*`, `diag_d.out` `FFD_undo_restores_exact_cage`) + E3
   (`CI DEVICE 37472535341`).
7. **`FRFM`/`SURF` fixtures decode to their verdicts on this tree.** E1 (`host_corpus_verdicts.txt`).
8. **Import is refused in Sculpt at both layers** (old council D1 closed for Sculpt). E2 + E3
   (`CI DEVICE 36643101662`).

## Journeys proposed for END_TO_END_JOURNEYS

- **J-D1 (negative / fail-closed, currently failing — D-01):** Import a ≥40k-vertex GLB → Start
  Sculpting → small Clay stroke → large-radius stroke over the whole mesh (status says not
  retained) → Undo. Expected: Undo refused or lands on a branch state; `SCULPT_HAS_EDITS` stays 1;
  Reset asks for confirmation. Undo/reopen: Save, reopen, `hasEdits` true.
- **J-D2 (negative / fail-closed, currently failing — D-02):** open project → New Sketch on XY,
  draw a rectangle → mark → Import GLB… must be absent; a direct `importGlbDurable` returns a
  sketch refusal; body count, active body and Construction undo depth unchanged; Cancel Sketch
  leaves the project byte-identical.
- **J-D3 (Freeform round trip):** New Project → Freeform → Shape → Face → tap a face → Push/Pull
  0.2 → Symmetry X → vertex drag (one Undo) → Insert Loop 0.5 → Save → Open Saved Project →
  `freeformMeshDigest` equal; then, with a face selected, Open Saved Project of another Freeform
  project → selection empty (D-03).
- **J-D4 (Surface round trip):** New Project → Surface → open chain → Finish Extrude 0.5 →
  Thicken 0.1 → History: edit the Extruded Surface to 0.8 (staged, Apply) → Undo/Redo →
  Save/reopen → `surfaceMeshDigest` and feature count equal.
- **J-D5 (mask lifecycle):** Sculpt → Mask stroke → Back to Construction → Duplicate → copy has
  no mask → Resume original keeps it → Clear Mask → Undo restores → Save/reopen → mask empty,
  geometry kept.
- **J-D6 (negative):** in the New Sketch chooser, tap a Freeform or Surface body in front of the XY
  plane → nothing chosen, no sketch opened.

## Owner decisions needed

- **D-OD1:** NotRetained stroke policy — clear the undo stack (proposed) or refuse the stroke.
- **D-OD2:** how a user removes a mask too large to retain (Clear Mask without Undo after a
  confirmation, or an erase mode — Invert/Grow/Shrink/Blur stay H2).
- **D-OD3:** does Lock hold FORM as well as placement, for every representation?
- **D-OD4:** a Sculpt-kind file whose active body is hidden: open in Construction, or refuse.
- **D-OD5 (H2, recorded):** Freeform → Sculpt and Surface → Sculpt need a stale-source rule
  before they can be offered.

## Unresolved dependencies and open questions

- Device reproduction of D-02 and D-03 needs a `CI DEVICE` dispatch (not done by this seat).
- The Sculpt device classes have not run on the current tree; whether they still pass after the
  `MODELING-FOUNDATIONS-R1` JNI changes is UNDETERMINED (no run).
- Physical stylus (`STYLUS-G1`), visual mask feedback, real-device performance:
  OWNER-PHYSICAL-UNVERIFIED.
- Horizons are reconstructed; the canonical Master Plan and owner-decision files are not
  available, so whether D-09's Freeform lock behaviour was an owner instruction cannot be checked.
- Seat A owns the drawer/shell side of D-02; seat C (sketch) may hold the same rule for its own
  entry points.

**Completeness: PARTIAL** — every in-scope rule and action was traced to code and to a test or
a recorded run (114 rows), and the five strongest claims were reproduced on the host; but no
device run was made on this tree for the Sculpt, Freeform or Surface classes, D-02/D-03 were not
reproduced on a device, and the authoritative plan sources are unavailable
(`PARTIAL-PLAN-SOURCE-GAP`).
