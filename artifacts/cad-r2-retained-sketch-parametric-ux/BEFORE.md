# CAD-R2 — BEFORE

Stage: `CAD-R2-RETAINED-SKETCH-PARAMETRIC-UX-AND-GEOMETRY-HARDENING`
(OWNER authorisation 2026-10-28). Read with `DESIGN.md` and
`ACCEPTANCE_MATRIX.md`.

## Baseline refs (verified 2026-10-28 with `git fetch origin`)

| Ref | SHA | Expected by the prompt |
| --- | --- | --- |
| `origin/main` | `6c9f156c1df91fe1a55445aed6186105a075ab31` | same |
| `origin/feature/modeling-r1-owner-shell-sketch-fix-r1` | `636797e518898c31c4cd691d453db2833ef00896` | same |

- `origin/main` is an ancestor of the task head (`git merge-base --is-ancestor`).
- Working tree clean (`git status --short` empty) before the branch was made.
- Task branch `feature/cad-r2-retained-sketch-parametric-ux-r1` created
  locally at exactly `636797e`; the name did not exist on `origin`
  (`git ls-remote` empty). No other branch was touched.

## Source audit

Every document the prompt names was found in the repository except the
optional `FORGESHAPE_PROJECT_STATE_v4.1.62.md` and
`FORGESHAPE_MASTER_PLAN_ADDENDUM_v4.1.62.md` (not attached to the session —
**source gap**, the repository is treated as authoritative) and a dedicated
`CAD-V6-S3` spec (none exists: `CAD-V6-S3` appears only as a deferred line in
`CLAUDE.md` — "creating, sharing, deleting or browsing sketches from the UI" —
and in `PROJECT_STATUS.md` "S3 not started").

### What already exists (not re-implemented)

| Capability | Where |
| --- | --- |
| Retained sketch table, `CadSketchId`, high-water marks, branch id lifetime | `forgeshape_cad_body.h` (`CadBodyState`, `CadSketchRecord`, `CadSketchId` comment) |
| A sketch no feature consumes is LEGAL truth, validated (entities + placement) | `validateCadChain` (`forgeshape_cad_feature.cpp`) |
| Shared sketches (two features, one sketch) and `CADB` v6 persistence of the table | `appendCadLaterFeature`, `cad_sketch_shared` fixture |
| Parametric History timeline (Ok/Failed/NotRegenerated/Pending/Unused) | `forgeshape_cad_timeline.{h,cpp}`, `FeatureHistoryView`, `FeatureHistoryPresentation` |
| Staged edit of ANY feature by durable id, first-failure + issue card, Fix/Cancel | `SketchSession::beginEditFeature`, `commitEdit`; `RegenerationIssue` card |
| Extrude New Body / Add / Cut, One Side / Symmetric / Two Sides, Flip | `SketchSession`, `CAD-EXT-R1` |
| Revolve New Body (angle, sense, axis) and its edit | `forgeshape_cad_revolve`, `CAD-V6-REVOLVE-NEWBODY-E2E-R1` |
| PlanarFaces arrangement, 9216-face selection bound, point-touch split | `forgeshape_sketch_arrangement`, `MODELING-R1-OWNER-CORRECTION` |
| Face support per face (Cut floor/walls), stale chooser re-validated on confirm | `CadFeatureFace::eligible`, `confirmChosenSupportLocked` / `refreshChosenSupport` |
| Edit Sketch chip withdrawn while the support chooser is up | `CadExtrudeCanvasView` (`DEV-OSS-03`) |
| New Sketch from the project drawer and Add Primitive | `ProjectDrawerPolicy`, `EditorWorkspaceView.onNewSketchSpatial` |

### What is genuinely missing (this stage's gap)

1. **No sketch inventory.** The timeline lists sketches but not WHO consumes a
   sketch, its support kind, or which acts are permitted on it.
2. **No way to create a retained sketch without a feature.** Every face
   sketch session ends in New Body / Add / Cut; an Unused sketch can only come
   from a file.
3. **An Unused sketch cannot be edited** (`FeatureHistoryPresentation.Row.editable`
   is false for Unused: there is no feature to open).
4. **No reuse act.** `appendCadLaterFeature(state, op, sketchId, extrude)`
   exists in the domain but no session, JNI or UI path reaches it.
5. **No delete of a sketch.** Nothing removes a sketch record.
6. **No per-row action menu.** A History row tap opens one editor; there is no
   `Edit Sketch` vs `Edit Feature` choice and no dependency read-out.
7. **No independent geometry oracle.** Existing checks compare volumes against
   expected numbers inline; no suite measures area, bounds, edge-manifold
   closure and winding of the produced mesh with formulas written apart from
   the model.

## Caps in force (unchanged by this stage)

| Cap | Value | Owner |
| --- | --- | --- |
| features per CAD body (base included) | 16 (`kMaxCadFeatures`) | `forgeshape_cad_body.h` |
| retained sketches per body | 16 (`kMaxCadSketches`) | same |
| sketch entities | 256 (`kMaxSketchEntities`) | `forgeshape_sketch.h` |
| loop regions per selection / holes | 16 / 64 | `forgeshape_sketch_region.h` |
| planar faces per selection | `kMaxArrangementFaces` (9216) | `forgeshape_cad_body.h` |
| timeline rows | 32 | `forgeshape_cad_timeline.h` |

## Baseline gates (this container, before any change)

- Host: `bash scripts/host-native-selftests.sh` → `HOST_SELFTESTS_OK (4304
  checks, 0 failed)`, 25 suites (`host_baseline_suites.txt`), 2 m 32 s.
- JVM: recorded in `TESTS.md` (local Android SDK installed in the container
  with the pinned NDK `29.0.14206865`, `platforms;android-36`,
  `build-tools;36.1.0`, `cmake;3.22.1`; `local.properties` is git-ignored and
  never committed).
- Corpus: 72 fixtures in `testdata/forge/v1`; the parity check is CI FAST's
  (`pwsh` is not available in this container).

## Behaviour before (to be changed)

| Act | Before |
| --- | --- |
| Open History, see who uses a sketch | Not shown |
| Sketch on a body face, keep it without extruding | Impossible (Cancel discards it) |
| Tap an Unused sketch row | Static row, nothing opens |
| Extrude an existing sketch a second time | No path |
| Delete an unused sketch | No path |
| Feature row: choose Edit Feature or Edit Sketch | One fixed act per row kind |
