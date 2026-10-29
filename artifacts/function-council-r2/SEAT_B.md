# FUNCTION-COUNCIL-R2 — Seat B: Construction / objects / scene / transforms

Audited product tree: `app/src/main` at `70a6c14f703bfda2ab689b855126c1d12a541e41`
(audit branch HEAD `9cfc3f5`, which only adds `artifacts/function-council-r2/BEFORE.md`;
`git diff --stat 70a6c14 HEAD` = 1 file, 93 insertions, no product file). Research only:
nothing under `app/`, `testdata/`, `scripts/`, `.github/`, Gradle files or the owner
documents was modified. Gradle was not run by this seat.

## Scope

The six primitives and their parameters; body dimensions, Relative Scale, Absolute
Scale; world/local placement; the transform gizmo (Move / Rotate / Scale,
coordinate-space selector) and the exact-value placement editor; Mirror, Duplicate,
Delete, Rename, Show/Hide, Lock; selection and the selection outline; multi-object
scenes; Construction Undo/Redo and transaction boundaries; the ObjectId allocator;
hidden/locked enforcement; units and axis conventions; New Project / Home /
closeProject as they affect the scene. The face-supported CAD body is in scope only
where it meets placement, the gizmo, the transform editors and export placement.

## Sources read (exact paths, revision 70a6c14 product tree)

- `CLAUDE.md` (rules: viewport handle, rotation, scale, body dimension, selection
  outline, Delete UI-OWNER-45, Stage 018A name/visibility/lock, MIRROR-01, one
  Construction history, APP-H1, CAD-A3 face support, export bake ARCH-OWNER-07).
- `PRODUCT.md` §§ "Several bodies…" (431-590), "Moving, rotating and sizing" (1213-1466),
  "Undo and Redo", "Selection".
- `PROJECT_STATUS.md` §§ Owner Decision Baseline (2486-2606), Known Issues (4018),
  Technical Debt (4216), Next Stage (4851, OQ-01 / OQ-02 at 4947-4949), Android UI
  suites table (3379-3412), historic run records (1010-1064, 3570-3590).
- `ARCHITECTURE.md` 2598-2618 (face-supported placement claim).
- `artifacts/function-council-r2/BEFORE.md`; `artifacts/function-council-r1/*`
  (older tree `a1b50f2`, used only as leads; every lead re-checked here).
- Fresh run outputs of this audit (not produced by this seat, read only):
  `/tmp/claude-0/-home-user-Forgeshape/2f5454c6-63b1-52b8-b92c-5f8b69c420c6/scratchpad/fc2/host_selftests.txt`
  (`HOST_SELFTESTS_OK (4392 checks, 0 failed)`, 25 suites), `run_status.txt`
  (`host_exit=0`, `corpus_verdicts_exit=0`, `gradle_exit=0`, `guard_exit=2`),
  `release_guard.txt` (as it stands now, mtime 00:57: ends `RELEASE_SELFTEST_GUARD=PASS` —
  release x86_64 and arm64-v8a 0 symbols / 0 strings, debug 35 symbols / 398 strings;
  corrected in Round 2, the earlier "ERROR" quote was wrong; `run_status.txt` still
  says `guard_exit=2` at 00:54, an orchestrator question about a rerun), and JVM XML results in
  `app/build/test-results/testDebugUnitTest/` (timestamps 2026-10-29T00:57Z, 0 failures).

## Inspected code paths (file:line, functions)

| Area | Path |
| --- | --- |
| Scene, body, flags | `forgeshape_scene.h:85-291` (`SceneObject`, `visible()/locked()`, `isFaceSupportedCad` 165-168); `forgeshape_scene.cpp:18` `addBody`, `:83` `closeProject`, `:88-109` `activeBody` (counted null object), `:120` `detachBody`, `:146` `reserveObjectIdsThrough`, `:152-200` `make*Body`, `:281-326` `resolveWorldModelDepth`, `:328-383` `snapshot` (hidden skip at 332), `:599-647` active-body accessors, `viewSceneSnapshot` |
| Allocator | `forgeshape_object_id.h`; `forgeshape_scene.h:561` `nextObjectId_` |
| Transform | `forgeshape_transform.cpp:166` `validateTransformValue`, `:179` `validateScaleValue`, `:198` `setValues`, `:395-409` `constructionTransform`/`applyConstructionTransform` |
| Primitives | `forgeshape_construction.h:178` `PrimitiveKind`; `forgeshape_construction.cpp:173` `validateDimensionMeters`, `:675` `validateCapsuleMeters` |
| Gizmo | `forgeshape_gizmo.cpp:474` `gizmoWorldScale`, `:973` `gizmoVisualScaleIsValid`, `:1134` `setActive`, `:1155/1173` `setMode/setSpace`, `:1197-1235` `snapshot`, `:1415` `applyTarget`, `:1428-1597` `beginDrag`, `:1807` `commitDrag`, `:1825` `cancelDrag` |
| JNI | `forgeshape_jni.cpp:131` `readJavaString`, `:1335-1420` `applyBoxTransform` (NoProject 1350, lock 1358), `:3198` `sceneSelectBody`, `:3238` `sceneAddBody`, `:3311` `sceneDeleteBody`, `:3408` `objectCommandsBlockedByMode`, `:3413-3640` rename/visible/lock/duplicate/mirror, `:3826-3860` `setGizmoActive`, `:7995` `runHistoryStep`, `:8266` `historyUndo`, `:8455-8480` load, `:8965-8990` closeProject, `:9858-9935` gizmo arbitration, `:10146-10170` tap pick |
| Object commands | `forgeshape_body_commands.cpp:17` `resolveTarget`, `:98` rename, `:123` visible, `:140` lock, `:154` `derivedBodyName`, `:187` `duplicateSceneBody` (face-support refusal 205), `:319` `mirrorSceneBody`; `forgeshape_body_delete.cpp:19` `deleteSceneBody` |
| Mirror | `forgeshape_body_mirror.cpp:119` `mirrorPlacement`, `:183` `mirrorEligibilityOf` |
| Dimensions | `forgeshape_body_dimensions.cpp:302` `solveAxisDimension`, `:337` `solveRelativeScale`, `:391` `bodySizeEditable`, `:395` `sceneBodyDimensions`, `:425` `resolveSizeTarget`, `:456` `commitSolvedPlacement` |
| History | `forgeshape_history.h:57` capacity 64; `forgeshape_history.cpp:82` capture, `:113` `beginEdit`, `:122` `commitEdit`, `:170` session-init bracket, `:193/207` undo/redo, `:407/414` held bodies |
| Selection | `forgeshape_selection_pulse.h:38,44,48`; `forgeshape_selection_outline.h:56-58,128-130` |
| Export placement | `forgeshape_gltf_export.cpp:155-200` `captureGlbExportScene` (reads `body.transform()` at 178-180); `forgeshape_gltf_export.h:138-160` |
| Android | `EditorWorkspaceView.java:569-640` (gesture settled refresh), `:1596` `onAddPrimitiveChosen`, `:3007-3037` `refreshTransformGizmo`, `:3690-3735` inspector body; `WorkspaceTrailingHostView.java:605-620` Construction rail; `ForgeShapeSurfaceView.java:58-121`; `ConstructionPlacementEditorView.java:269` |

## Test coverage of the domain (what each actually asserts)

Host (fresh run of this audit, `host_selftests.txt`, E1): `CONSTRUCTION_BOX` 100,
`CONSTRUCTION_TRANSFORM` 121, `CONSTRUCTION_PRIMITIVE` 126, `CONSTRUCTION_SPHERE` 105,
`CONE_CAPSULE` 163, `SCENE` 175, `CONSTRUCTION_HISTORY` 147, `GIZMO` 176,
`BODY_DIMENSIONS` 102, `MIRROR` 62, `PICKING` 185, `RENDER_SHADING` 412 — all 0 failed.
What they assert, by family:

- `SCENE` `s17_*`: unique monotonic ids, insertion order, per-body revisions and
  transforms, selection-only highlight, nearest-body pick, per-body freeze/sculpt
  isolation. `obj018a_01..14`: rename one step / undo / same-name no-op / empty
  refused / UTF-16 exact; hide removes from snapshot but keeps revision and selection;
  lock keeps draw+pick; duplicate fresh id, same representation/placement/lock, copy
  name and ordinal, cloned sculpt mesh with empty sculpt history; `SCNE` v1/v2
  promotion and fingerprint movement; hidden+locked still deletes.
- `CONSTRUCTION_HISTORY`: empty history, shape/placement one step, rejected/no-op
  record nothing and keep redo, nesting refused, cancel restores, creation undo/redo
  same id and position, eviction at 64 oldest-first, sculpt geometry untouched by
  Construction undo, held bodies keep Frozen Sculpt Mesh, `IMP01B_15..21` delete rules.
- `GIZMO`: handle codes, 48-unit hit floor, World/Local basis, rotate pre/post-multiply,
  ring continuity past 720°, singularity finite, scale local-only and never through 0,
  one drag one step, cancel restores nine values, second begin refused,
  `selection_moved_but_the_drag_did_not_retarget`, seeding leaves empty history,
  `uipref_*` visual scale/stroke weight (Regular 1116 vertices).
- `CONSTRUCTION_TRANSFORM`: Euler convention, scale refusal, non-uniform normal/pick.
- `BODY_DIMENSIONS` `DIM020M-01..17`; `MIRROR` `MIRROR01-01..12` (identity
  `Model_mirror(q) = F·Model_source(Qx·q)`, one step, eligibility refusals, byte identity).

Absent from every suite: any check that a face-supported CAD body refuses a transform
write or a gizmo, any check of the export placement of a face-supported CAD body, any
check of the gizmo over a hidden active body, any check of derived names at the
96-byte cap.

JVM (fresh XML results, E1): `EditorUiStateTest` 10/10, `LengthUnitTest` 6/6 (the
only JVM classes in this domain).

androidTest (historic only, E3; none re-run on `70a6c14` by this seat):
`EditorWorkspaceGizmoTest` (46), `EditorWorkspaceHistoryTest` (20),
`EditorWorkspaceObjectsTest` (10), `ObjectsDeleteTest` (8), `ObjectCommandsTest` (1,
asserts gizmo withdrawn on lock and `APPLY_REJECTED_LOCKED`), `MirrorSmokeTest` (1),
`BodyDimensionsSmokeTest` (1), `SelectionOutlineTest` (19). Their last recorded green
runs are the Stage 018A / MIRROR-01 / Stage 020M / SEL-OUT-R1 focused runs named in
`PROJECT_STATUS.md` (lines 1013-1064, 3570-3590) on the isolated AVD — emulator
evidence, never a physical-device PASS. The C2 device unions (`40776968165`,
`40782861799`) target the CAD-R2 classes; this seat did not confirm they included
any class above.

Fixtures: `object_state` / `object_state_bad_flags` (`SCNE` v2); corpus verdicts
exit 0 in this audit's run (`run_status.txt`).

Seat diagnostics (E1, scratch only, never in the build), built against the host
objects of this audit's run:

```
cd /tmp/claude-0/fc2-scratch/seatB
CPP=/home/user/Forgeshape/app/src/main/cpp; OBJ=/home/user/Forgeshape/build/host-selftests/obj
g++ -std=c++17 -O1 -I$CPP -I$CPP/third_party/manifold/include -DMANIFOLD_PAR=-1 \
    -DMANIFOLD_NO_IOSTREAM -DMANIFOLD_NO_FILESYSTEM <diag>.cpp $(ls $OBJ/*.o) \
    /home/user/Forgeshape/build/host-selftests/kernel/libmanifold.a -o <diag> && ./<diag>
```

for `diag_face_supported`, `diag_objects`, `diag_math` (sources in the same directory).

## Confirmed findings

### B-01 — P2 — A face-supported CAD body is transformable, but the write does nothing visible

- Path: `forgeshape_jni.cpp:1335-1385` (`applyBoxTransform`: guards NoProject and
  Locked only), `forgeshape_gizmo.cpp:1197-1235` / `:1415` / `:1428` (no face-support
  check), `EditorWorkspaceView.java:3028-3033` (gizmo `offered` has no face-support
  term), `WorkspaceTrailingHostView.java:605-620` (Transform entry for every CAD body);
  the derived placement ignores the stored transform at `forgeshape_scene.cpp:292-321`.
- Rule: `CLAUDE.md` CAD-A3 "such a body is NOT independently transformable
  (`SceneObject::isFaceSupportedCad`)". `isFaceSupportedCad` is read only by Duplicate
  (`body_commands.cpp:205`) and the JNI query `sceneActiveBodyIsFaceSupportedCad`
  (`jni:7676`), which no Java product code calls (only androidTests).
- Minimal repro (E1, `diag_face_supported`): producer = XY rectangle 2×1 extruded 3 m;
  dependent on its `CapFar`. Drawn/picked model translation `(0,0,3)`; stored transform
  `(0,0,0)`; gizmo `visible=1 pivot=(0,0,0)`. A typed write `positionX=5` through
  `ScopedConstructionEdit` + `applyTransformValues` (what `applyBoxTransform` does):
  `applyStatus=Applied`, `undoDepth 0->1`, `fingerprintChanged=1`, resolved model still
  `(0,0,3)`, gizmo pivot now `(5,0,0)`.
- Product reachability (E2): select a face-supported body → Transform rail → the gizmo
  is offered and the placement editor is shown; nothing refuses.
- Impact: the instrument is drawn away from the body; drags and typed values record
  Undo steps, dirty the project (so leaving asks the unsaved-changes question) and make
  autosave write a checkpoint for a change nobody can see; the stored transform then
  leaks into export (B-02 / E-01). It also puts non-canonical placement truth into
  `.forge`: `validateProjectDocument` checks only `transformValid`
  (`forgeshape_project_document.cpp:739`), so a non-identity transform on a
  face-supported body is saved, reopened and fingerprinted, and two projects that look
  identical have different bytes and fingerprints (E2; cross-seat with seat E). The
  touch path to `gizmo.beginDrag` (`jni:9932-9934`) has no face-support check either.
  Saved E1 artefact: `/tmp/claude-0/fc2-scratch/E-vs-B/diag_face_supported.out`.
- Verdict: DEFECT-CONFIRMED (host E1 + source E2; not reproduced on a device).
- Isolated fix: refuse `applyBoxTransform` and `GizmoSession::beginDrag`/`setGizmoActive`
  when the active body `isFaceSupportedCad()` (named status), and withdraw the
  Transform rail entry / gizmo `offered` above JNI. No format change.

### B-02 — P1 — GLB export places a face-supported CAD body at its stored transform, not where it is drawn

- Path: `forgeshape_gltf_export.cpp:178-180` reads `body.transform().modelMatrix()` /
  `localMatrix()` / `normalMatrix()`; the renderer and picker use
  `ConstructionScene::resolveWorldModel` (`forgeshape_scene.cpp:277-326`). The header
  claims "the exported place is bit-for-bit the place drawn" (`forgeshape_gltf_export.h:141-142`).
- Minimal repro (E1, `diag_face_supported` B and D): dependent drawn at `(0,0,3)`,
  exported node translation `(0,0,0)`; after the B-01 write the export moves to
  `(5,0,0)` while the viewport is unchanged. The face frame's rotation is likewise
  absent from the bake (`exported.local` is the stored identity).
- Impact: any project with a face-supported New Body exports a GLB whose dependent
  body sits at the world origin (or wherever B-01 left it), not on the producer's face —
  silent geometry misplacement in the interchange file.
- Second form (E2, Round 2): when `resolveWorldModel` returns false (stale lineage,
  missing producer) the snapshot leaves the body out (`forgeshape_scene.cpp:359-365`),
  but `captureGlbExportScene` loops over every `bodyAt(i)` with no resolve
  (`gltf_export.cpp:167-185`) and still writes it at its stored transform.
- Verdict: DEFECT-CONFIRMED (E1). MERGED with seat E's E-01 (primary, export is I/O):
  two independent E1 reproductions — seat E's fixture-based `export_face` on
  `cad_face_sketch_cap_v2`, `cad_face_sketch_side_v2`, `cad_face_chain_v2`, and this
  seat's programmatic `diag_face_supported` (saved:
  `/tmp/claude-0/fc2-scratch/E-vs-B/diag_face_supported.out`). P1.
- Isolated fix: in `captureGlbExportScene`, take the model from
  `scene.resolveWorldModel(id)` and split it into `T` (column 3) and `L` (upper 3×3);
  keep the determinant refusals; when `resolveWorldModel` returns false, REFUSE the
  export by name rather than writing the body at its stored transform. No `.forge` or
  GLB layout change. Add a `GLTF_EXPORT` check for a face-supported dependent.

### B-03 — P2 — ARCHITECTURE.md states a guard that does not exist

- Path: `ARCHITECTURE.md:2606-2608`: "`SceneObject::isFaceSupportedCad` marks such a
  body so the gizmo and the exact-value editors leave it alone".
- Repro (E2): `grep -n isFaceSupportedCad app/src/main/cpp/*.cpp` → only
  `forgeshape_body_commands.cpp:205`; Java product code never calls
  `sceneActiveBodyIsFaceSupportedCad`.
- Verdict: DOC-DIVERGENCE. Fix: implement B-01 (preferred) or correct the sentence.

### B-04 — P3 — The native lock guard holds only at activation; a selection change or Undo leaves a live gizmo over a locked body until Java refreshes

- Path: `setGizmoActive` checks lock (`jni:3842-3850`); `sceneSetBodyLocked` takes the
  gizmo down (`jni:3529-3536`); but `sceneSelectBody` (`jni:3198-3232`), the viewport
  tap (`jni:10169`) and `runHistoryStep` (`jni:7995-8030`, e.g. undoing an Unlock) do
  not, and `GizmoSession::snapshot/beginDrag/applyTarget` never read `locked()`.
- Repro (E1, `diag_objects` 1, domain level): gizmo active over unlocked body 1, then
  `setActiveBody(2)` with body 2 locked → `gizmoVisible=1 pivot=(3,0,0)`, `beginDrag=1`,
  locked body `positionX 3.0 → 3.6227`, a history step recorded.
- Mitigation (E2): `onViewportGestureSettled` (`EditorWorkspaceView.java:598-602`)
  and row/Undo handlers call `onNativeStateChanged` → `refreshTransformGizmo`, which
  calls `setGizmoActive(false)` over a locked body synchronously on the UI thread, so
  no product path was found that reaches the drag.
- Verdict: PARTIAL (defence-in-depth gap against `CLAUDE.md` "removing a control is
  not removing a guard"). Fix: `GizmoSession::beginDrag` refuses a locked (and, see
  B-01, face-supported) active body by name — one refusal serving both B-04 and B-01.
  Saved E1 artefact: `/tmp/claude-0/fc2-scratch/E-vs-B/diag_objects.out` (case 1).

### B-05 — P3 — Derived names at the 96-byte cap are truncated mid-suffix or dropped

- Path: `derivedBodyName` (`forgeshape_body_commands.cpp:154-181`) appends `" copy"` /
  `" Mirror"` / `" N"` and only then sanitizes to `kMaxImportedMeshNameBytes` (96).
- Repro (E1, `diag_objects` 4): source named 96 × `a` → Duplicate copy name is
  empty (UI falls back to `Body #N`); source 93 × `b` → first copy `"bbb…b co"`, second
  copy empty.
- Impact: `PRODUCT.md` 516-553 promises `<name> copy`, `<name> copy 2`; long names get
  a cut-off `co` or no name. Cosmetic, deterministic.
- Mirror shares `derivedBodyName`; only Duplicate was exercised, so the Mirror half is
  E2, not E1. Saved E1 artefact: `/tmp/claude-0/fc2-scratch/E-vs-B/diag_objects.out` (case 4).
- Also DOC-DIVERGENCE (E2): the fallback comment "Unreachable for any scene this
  product can build" (`forgeshape_body_commands.cpp:177`) is false — a 96-byte name is
  reachable through Rename, and no Java `LengthFilter` exists in `app/src/main/java`.
- Verdict: DEFECT-CONFIRMED. Fix: truncate the base at a UTF-8 boundary to
  `96 − len(suffix)` before appending, and correct the comment.

### B-06 — owner-pending (no severity) — The gizmo and placement editor stay over a hidden active body (unspecified)

- Path: `refreshTransformGizmo` `offered` (`EditorWorkspaceView.java:3028-3033`) and
  `GizmoSession::snapshot` (`gizmo.cpp:1197`) ignore visibility, whereas Dimensions
  refuses a hidden body (`body_dimensions.cpp:443`).
- Repro (E1, `diag_objects` 2): hidden active body → `inSnapshot=0`, `gizmoVisible=1`.
- Verdict: UNDETERMINED-SOURCE-GAP (no rule decides it), owner-pending, no ranked
  severity (Round 2). Owner decision below. Saved E1 artefact:
  `/tmp/claude-0/fc2-scratch/E-vs-B/diag_objects.out` (case 2).

### B-07 — WITHDRAWN as a defect (Round 2) — `sceneAddBody` has no `hasProject()` guard

- Withdrawn: `onNewSculptProjectChosen` (`EditorWorkspaceView.java:2402-2406`) calls
  `sceneAddBody()` at Home on purpose inside `beginSessionInitialization()` — the seeded
  sphere APP-H1 describes — and `ensureConstructionProjectForTest` (`:2739-2749`) relies
  on it. APP-H1's "nothing fabricates a default primitive" is about the shell inventing
  a body, not a creation the user chose. The earlier fix (`!hasProject()`) would have
  broken New Sculpt project creation.
- Remains only as HYPOTHESIS H-B4 (E5), below.

## Hypotheses (unproven)

- H-B1: A Down arriving between a native selection change and the Java refresh could
  start a gizmo drag on a locked body. Rejected as reachable: touch delivery and
  `onViewportGestureSettled` run on the same UI thread, sequentially (E2). Left as the
  domain gap B-04.
- H-B2: A face-supported body's stored non-identity transform (after B-01) could make
  a saved file fail to reopen. Not supported: `validateProjectDocument` checks only
  `transformValid` (`forgeshape_project_document.cpp:739`), not identity, so the file
  reopens; the risk is the silent export drift (B-02), not a load refusal (E2).
- H-B4 (E5): a stray caller of `sceneAddBody` outside the session-initialization
  bracket could create a project from Home. No such caller found. Any guard would have
  to be `!hasProject() && !sessionInitializationOpen`, never a bare `!hasProject()`.
- H-B3 (carried debt, not my seat to close): Stage027 GUARD-2 is not asked on the load
  path (`PROJECT_STATUS.md` Technical Debt "GUARD-2 guards the two Sculpt ENTRY points").

## Adversarial counterexample attempts

1. Mirror at a gimbal pose `(0,90,0)`, wrapped angles `(720.5,−270,1080.25)` and
   extreme scale `(1e-3,1e3,1)` on all three planes: worst relative error of
   `Model_mirror(q) − F·Model_source(Qx·q)` = 0; scale carried unchanged and positive.
   Failed to break (E1, `diag_math`).
2. Relative Scale pushing `scaleX` 2e-6 × 0.4 below `kMinScaleFactor`, and a −1
   multiplier: both `RefusedGeometry/NotPositive`, undo depth unchanged. Failed to break.
3. Exact dimension X → 5 m with `NegativeSide` anchor on a body turned (30, −40, 55):
   held face drift 2.4e-7 m, one step. Failed to break.
4. ObjectId reuse after `closeProject` and after delete: next id 4 after close (not 1);
   after deleting id 3 the next creation is 4. Failed to break.
5. Lock bypass via selection change with the gizmo already up: succeeded at domain
   level → B-04.
6. Face-supported body through the transform write and the exporter: succeeded → B-01, B-02.
7. Duplicate of a 96-byte / 93-byte name: succeeded → B-05.
8. `loadProject` while a gizmo drag holds an open edit: refused
   `RefusedEditInProgress` (`forgeshape_project_state.cpp:250-251`), asserted by
   `forgeshape_project_selftest.cpp:2490` `FSR1A_13_a_load_during_an_open_edit_is_refused`
   (PROJECT suite, fresh host run). Failed to break (E1).

## Positive claims with evidence

- Hidden is one predicate in one place (`forgeshape_scene.cpp:332`); hidden body absent
  from the snapshot while the gizmo's own state is untouched (E1 `diag_objects` 2,
  `obj018a_03_*`).
- One drag = one step, cancel/second pointer restores nine values exactly (E1 `GIZMO`).
- ObjectIds are never reused across delete, undo and close (E1 suites + `diag_objects`).
- Mirror is an exact reflection by a proper rotation at the SAMPLED adversarial poses
  (E1 for the samples: worst relative error 0 over 9 pose/plane pairs × 3 points,
  `/tmp/claude-0/fc2-scratch/E-vs-B/diag_math.out`); the "for any q" claim rests on
  the algebra and `MIRROR01-*`, not on the diagnostic — sampled, not proven.
- Dimension anchors and Relative Scale refusals hold (E1 `diag_math`, `DIM020M-*`).
- Rename takes UTF-16 through `readJavaString` → `utf16ToUtf8` (`jni:131-146`, used at
  `jni:3421`), not `GetStringUTFChars` (E2); round trip asserted by
  `obj018a_02_utf16_round_trip_is_exact` (E1).
- Every refusal path in object commands mints no id and records no step
  (`MIRROR01-11`, `IMP01B_21`, `DIM020M-08/15`; E1).

## Journeys proposed for END_TO_END_JOURNEYS

- J-B1 (positive): Add Primitive → Sphere → Transform → drag the X shaft (48 samples)
  → one Undo restores identity → Redo restores the drag; no MeshRevision published.
- J-B2 (negative / fail-closed): lock the active body → gizmo withdrawn, typed Position X
  → `APPLY_REJECTED_LOCKED`, undo depth unchanged; then select another unlocked body,
  activate Transform, tap-select the locked body → gizmo absent before any drag.
- J-B3 (negative): only body → Delete absent and `RefusedLastBody` below JNI; producer
  with a face-supported dependent → `RefusedHasDependents`; no step, no byte moved.
- J-B4 (defect repro, negative expectation): New CAD → New Sketch on the cap of the first
  body → New Body → select the dependent → Transform: expected (rule) no gizmo / refusal;
  observed in source: gizmo at origin, typed X accepted with an Undo step; then Export
  GLB and compare the dependent node translation with the drawn placement (B-01, B-02).
- J-B5 (positive): Mirror YZ on a turned, non-uniformly scaled body → one new body,
  world X negated → Undo removes only it → Redo restores the same ObjectId.
- J-B6 (positive): hide the active body → a viewport tap where it was misses → its row
  still selects it → Show restores it with the same revision, as one step each.

## Owner decisions needed

1. Confirm that a face-supported CAD body must offer no Transform entry, no gizmo and no
   placement editor (CLAUDE.md says so; the product currently offers all three), versus
   a future "offset from face" placement (would need durable semantics). Also: should the
   decoder REFUSE a non-identity stored transform on a face-supported body, or should a
   load ignore it (it is currently accepted and fingerprinted)?
2. Whether the gizmo and the exact placement editor should be withdrawn over a hidden
   active body (Dimensions already is).
3. Derived-name policy at the 96-byte cap (truncate base vs. refuse vs. current).
4. Still open and correctly not implemented: OQ-01 (Directional Scale, Stage 020D),
   OQ-02 (Sculpt dimensions), the UI-OWNER-45 Delete verdict, UI-OWNER-32 aesthetic verdict.

## Unresolved dependencies and open questions

- Device re-run of `EditorWorkspaceGizmoTest`, `ObjectCommandsTest`, `ObjectsDeleteTest`,
  `MirrorSmokeTest`, `BodyDimensionsSmokeTest`, `SelectionOutlineTest` on `70a6c14` was
  not performed by this seat (no local device); their status is E3 from older trees.
- B-02 overlaps the export seat; B-01/B-03 overlap the CAD seat — the synthesis should
  dedupe there.
- Selection outline rendering (`forgeshape_renderer.cpp`, mask + composite) is not
  host-buildable; only its policy constants and CPU reference were inspected.
- The release guard file as it stands (`release_guard.txt`, mtime 00:57) ends
  `RELEASE_SELFTEST_GUARD=PASS` (release 0 symbols / 0 strings on both ABIs, debug 35 /
  398). `run_status.txt` still records `guard_exit=2` at 00:54 — orchestrator question
  (apparent rerun after the status line), not an ERROR.
- Sketch rule cross-reference: `importGlbDurable` (`jni:8586-8600`) asks only
  `inSculptMode()`, so Import is the one creation / body-switching route not refused in
  a sketch — recorded once in the synthesis as seat E's E-03 (E2), not a seat B defect.

Completeness: PARTIAL — every in-scope rule was traced to source and host evidence and
three defects were reproduced on the host, but no device or physical-device run of this
domain's androidTest classes was executed on the audited tree, and the canonical master
plan / owner-decision files were unavailable (horizons reconstructed from repo records).

## Round 2 — objections from seat E and responses

Every objection was re-checked against the tree (`app/src/main` at 70a6c14) and against
seat E's saved outputs in `/tmp/claude-0/fc2-scratch/E-vs-B/`. Commands used: `sed -n`
over the cited lines, `grep -n` for `resolveWorldModel`, `LengthFilter`, `FSR1A_13_a` and
`expandedRow() != null`, `awk` over the `e2eMirror0101` body, and `tail` / `cat` over
`fc2/release_guard.txt` and `run_status.txt`.

| # | Objection | Response | Status |
|---|---|---|---|
| 1 | B-02 and E-01 are the same defect; merge them, with E-01 as primary; add the case where an unresolved support is still exported. | **CONCEDED.** `gltf_export.cpp:177-179` reads the stored transform. `scene.cpp:359-365` leaves out a body whose support does not resolve, and `gltf_export.cpp:167-185` loops over every `bodyAt(i)` with no resolve. B-02 is now merged with E-01, which is primary, and stays P1 / DEFECT-CONFIRMED with two E1 reproductions (seat E's fixture-based `export_face`, and the programmatic `diag_face_supported.out`). The fix now says to refuse by name when `resolveWorldModel` is false; that case is graded E2. CON-025 is updated. | RESOLVED |
| 2 | B-01's impact on persistence is understated. | **CONCEDED.** The impact now covers the dirty prompt, autosave writing a checkpoint for a change no one can see, and the non-canonical `.forge` placement (`project_document.cpp:739` checks only `transformValid`; two projects that look the same have different bytes and fingerprints). It also covers the touch path `jni:9932-9934`. Owner question 1 now also asks whether the decoder should refuse or ignore that transform. Stays P2 / DEFECT-CONFIRMED, cross-seat with E. | RESOLVED |
| 3 | B-07's fix would break New Sculpt project creation. | **CONCEDED.** `onNewSculptProjectChosen` (`EditorWorkspaceView.java:2402-2406`) calls `sceneAddBody` at Home on purpose, inside `beginSessionInitialization`, and `ensureConstructionProjectForTest` (`:2739-2749`) relies on it. B-07 is withdrawn as a defect. It is kept only as HYPOTHESIS H-B4 (E5), which notes that any guard would have to be `!hasProject() && !sessionInitializationOpen`. The fix line is removed from CON-074. | RESOLVED |
| 4 | CON-074 mixes two requirements. | **CONCEDED.** The stated requirement is met at `jni:1350-1357`. CON-074 is regraded PASS-SOURCE-AND-RECORDED-RUNTIME (E2 + E3 HomeFlowTest) with no severity. | RESOLVED |
| 5 | The release guard is misreported. | **CONCEDED.** `release_guard.txt` currently ends `RELEASE_SELFTEST_GUARD=PASS`: release on both ABIs has 0 symbols and 0 strings, and debug has 35 / 398. Both sentences are corrected. The fact that `run_status.txt` still says `guard_exit=2` (00:54) is carried as an orchestrator question, not reported as an ERROR. | RESOLVED (orchestrator question open) |
| 6 | B-04 is accurate; optionally move the refusal into `beginDrag` so it covers locked and face-supported bodies together. | **ACCEPTED.** The fix is reworded so that `GizmoSession::beginDrag` refuses locked and face-supported bodies together. Saved E1 artefact: `diag_objects.out` case 1. | RESOLVED |
| 7 | B-05 stands; add the false comment as DOC-DIVERGENCE; the Mirror half is E2. | **CONCEDED.** The comment at `body_commands.cpp:177` is false, and `grep LengthFilter` over `app/src/main/java` finds nothing. That comment is now a DOC-DIVERGENCE (E2). The Mirror half is E2 because only Duplicate was exercised. Stays DEFECT-CONFIRMED / P3. CON-036 is updated. | RESOLVED |
| 8 | CON-052 cites the wrong evidence for the Back half. | **CONCEDED.** The `e2eMirror0101` body contains no Back press. Opening the chooser is E3. Back is E2 (`hasDismissibleSurface` `:1112-1116`) and its verdict is IMPLEMENTED-UNTESTED. | RESOLVED |
| 9 | CON-070's citation is missing the check for its second clause. | **CONCEDED.** Added `forgeshape_project_selftest.cpp:2490` `FSR1A_13_a_load_during_an_open_edit_is_refused`. Adversarial #8 is now E1, which matches the row. | RESOLVED |
| 10 | CON-001/038/077 miss the Import route that is not guarded in a sketch. | **CONCEDED as a cross-reference.** `importGlbDurable` (`jni:8586-8600`) checks only `inSculptMode()`. The rows keep PASS for their own entry points (`jni:3206/3246`) and now point to E-03, which is recorded once in the synthesis and not as a seat B defect. | RESOLVED |
| 11 | CON-088 is consistent; note E-07; E-10 applies only to edits the gizmo does not own. | **ACCEPTED.** E-07 is noted on CON-088. `setActive(false)` cancels a captured drag (`gizmo.cpp:1138-1143`) before the history is cleared. | RESOLVED |
| 12 | Horizon and source labels (CON-079, CON-032). | **CONCEDED.** CON-079's source is now PRODUCT.md "Several bodies, and which one you are editing" (line 431). CON-032 / B-06 now carry no ranked severity and are labelled owner-pending. No change to CON-053/061/062/078/024/025. | RESOLVED |
| 13 | Check the five strongest positive claims; reword the Mirror claim as "sampled". | **CONCEDED** for Mirror: it now says "sampled, not proven" (worst error 0 over 9 × 3 samples), and the "for any q" claim rests on the algebra and `MIRROR01-*`. The other four stand. The rename claim now also cites `jni:3421` and `obj018a_02_utf16_round_trip_is_exact`. | RESOLVED |
| 14 | No output files were saved for B's diagnostics. | **CONCEDED.** B-01, B-02, B-04, B-05, B-06, CON-024/025/030/032/036 now cite `/tmp/claude-0/fc2-scratch/E-vs-B/{diag_face_supported,diag_objects,diag_math}.out` as the saved E1 artefacts. | RESOLVED |

Net change: 12 conceded with corrections, 2 accepted with no change to the verdict
(6, 11), 0 rebutted. B-07 is withdrawn, and B-02 is merged into E-01. The only open item
is the orchestrator question about `guard_exit=2` versus `RELEASE_SELFTEST_GUARD=PASS`.
