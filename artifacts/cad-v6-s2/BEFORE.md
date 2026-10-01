# CAD-V6-S2-PLANAR-RUNTIME-R1 — BEFORE

Written before any product edit, on `feature/cad-v6-sketch-face-r1` at
`f91c6dd` (`origin/main = 6c9f156`, merge-base `6c9f156`, clean tree,
`git diff --check` clean). Paths are under `app/src/main/cpp/`; line numbers
are at that commit.

## 1. `CadFaceToken` layout

`forgeshape_sketch.h:345` — `{CadFaceKind kind; SketchEntityId edgeEntityId;
uint32_t edgeLocalIndex}`. `CadFaceKind` is `CapPlane | CapFar | Side`
(`:328`). A cap carries zeros; a side names the profile-edge owner and that
owner's edge index (rectangle side 0..3, polyline segment k, line 0, circle
polygon edge k). `cadFaceTokenCode` (`forgeshape_sketch.cpp:604`) is
`kind<<56 | entity<<16 | (local & 0xFFFF)`, and **`sameCadFaceToken` compares
those codes** (`:613`), so equality is only as good as the 64-bit packing.

**The defect S2 must fix first.** The token has no notion of a *fragment*. A
PF-S1 arrangement splits one source edge at every crossing and T-junction
(`REFERENCE_FACES.txt`: PF-S1-02 splits rectangle side `1.1` into
`[S>X(2.0#0)]`, `[X(2.0#0)>X(4.0#0)]`, `[X(4.0#0)>E]`). Two such pieces on one
union boundary would both be `Side(1, 1)` — two different planar faces
wearing one token, so a support would silently resolve to whichever came
first.

## 2. Topology-signature inputs

`deriveFeature` (`forgeshape_cad_feature.cpp:170-180`): FNV-1a 64 over
`extrude.profileEntityId`, the face count, then per face in order
`cadFaceTokenCode(token)` and `eligible`; `0` is mapped to `1`. Face order:
`CapPlane`, `CapFar`, then one `Side` per polygon edge, component by component
(outer loop, then holes). The format rule is `DATA_PACKAGE_SPEC.md` §7c/§7f.

## 3. Current PlanarFaces regeneration refusal

* `validateCadFeatureGeometry` (`forgeshape_cad_body.cpp:787-788`) returns
  `PlanarFaceRegenerationUnavailable` for a `PlanarFaces` selection.
* `walkCadChain` (`forgeshape_cad_feature.cpp:293-306`): in non-validating
  mode (every `buildCadChainGeometry` → `regenerateCadBody`) a PlanarFaces
  feature refuses `PlanarFaceRegenerationUnavailable`; in validating mode it
  runs `validatePlanarFaceSelection` and steps over the feature.
* `sketchPlacement` (`:217-220`) refuses a sketch standing on a face of a
  PlanarFaces feature with the same status.
* `validatePlanarFaceSelection` (`forgeshape_cad_body.cpp:723-782`) is the
  one judge of a stored selection: form, canonical order, duplicates, then
  exact `resolvePlanarFaceRef` against `deriveSketchArrangement`, mapping
  arrangement failures through `planarFaceStatusFor` (`:689`).

## 4. Current runtime project-load refusal

`runtimeCanEvaluateProject` (`forgeshape_project_state.cpp:100-109`) returns
false for any body where `cadBodyStateUsesPlanarFaces`; `loadProjectDocument`
(`:222`) and the JNI load/recovery check (`forgeshape_jni.cpp:6851`) refuse
the whole project, reported above JNI as `MissingRequiredSection`.

## 5. SketchSession region selection

* `finish()` (`forgeshape_sketch_session.cpp:1206`) → `extractSketchRegions`
  → `reconcileRegionSelection()` (`:1232`): keep a still-valid selection,
  else auto-select only when exactly one region exists.
* Canvas tap: `onExtrudeTouch` arms on Down, and on Up calls
  `toggleRegionAt` (`:1329`) → `sketchRegionAt` → `toggleRegion` (`:1293`),
  a pure toggle validated by `validateRegionSelection`.
* "Anything chosen" is tested everywhere as
  `extrude_.profileEntityId != kNoSketchEntity` — `evaluateCandidate`
  (`:1606`), `commit` (`:1660`), `commitEdit` (`:319`) — a LoopRegions-only
  predicate.
* JNI lists regions by outer anchor (`sketchProfiles`, `sketchProfileInfo`,
  `sketchToggleRegion`, `forgeshape_jni.cpp:4575-4665`); `sketchState[5]` is
  the region count and `[6]` the chosen anchor.

## 6. Preview profile construction

`buildOverlay` (`:1812`) derives `mergeSelectedRegions(regions_, …)` and draws
each component's loops (`sketchComponentLoops`), their hatch
(`sketchComponentHatch`) and the preview prism edges. The arrow's base is
`extrudeSelectionAnchorPoint` (`forgeshape_cad_extrude_tool.cpp:141`), which
also requires a LoopRegions anchor. The solid preview is
`evaluateCandidate()` → `regenerateCadBody(candidateState())` — the same
function a commit applies.

## 7. NewBody / Add / Cut entry points

* New Body: `SketchSession::commit` (`:1643`) → `scene.addCadBody(candidateState())`.
* Add/Cut: `commitIntoTarget` (`:1692`) → `appendCadLaterFeatureWithSketch` in
  `candidateState()` (`:1510`) → `CadBody::applyState`.
* Edit: `commitEdit` (`:312`).
* All regenerate through `regenerateCadBody` (`forgeshape_cad_body.cpp:1047`):
  `buildCadChainGeometry` → per feature `appendCadFeatureSolid`
  (`forgeshape_cad_feature.cpp`) → kernel `Union`/`Difference`. The R0 fast
  path is taken only for a single simple LoopRegions profile.

## 8. Support chooser confirm

`confirmChosenSupportLocked` (`forgeshape_jni.cpp:4044`) passes the
`SupportChooser::selected_` `TopoRef` AND the world frame stored at tap time
straight into `SketchSession::beginOnFace` (`forgeshape_sketch_session.cpp:123`),
which does not re-resolve either. `runHistoryStep` (`forgeshape_jni.cpp:5931`)
refuses Undo/Redo only while a sketch session is active, not while the
chooser is, and does not cancel the chooser. So after an Undo/Redo that moved
or removed the producer face, a confirm begins a sketch on a stale frame
(commit-time validation still refuses a lost support; nothing refuses a moved
one before authoring).

## 9. Legacy face-token serialization (v1..v5)

Written by `cadFaceKindFileCode` (`forgeshape_project_document.cpp:282`, codes
1/2/3) followed by `u32 edgeEntityId, u32 edgeLocalIndex`:
* v2+ base-sketch `TopoRef`: writer `:1274-1281`, reader `:2143-2150`.
* v5 later-feature `CadFeatureSupport`: `writeCadFeatureSupport` via `:1316`,
  reader `:2230-2247`.
`cadFaceKindFromFileCode` (`:349`) refuses any code other than 1..3.

## 10. v6 face-token serialization

`writeCadBodyV6` (`:1080`): placement 2 (body `TopoRef`) `:1094-1101`,
placement 3 (`writeCadFeatureSupport`) `:1093`; readers `:1929-1942` and
`:1943-1955`, both through the same `cadFaceKindFromFileCode`. No v6 fixture
encodes a fragment, because none exists in the model.

## Consequences for the plan

* A fragment side token can be v6-ONLY by giving it a NEW file code (4) that
  every v1..v5 reader already refuses, while codes 1..3 keep their bytes — so
  the 44 legacy fixtures AND the 12 existing v6 fixtures need not move.
* `sameCadFaceToken` must become field equality, because a fragment token's
  identity does not fit a 64-bit packing; the legacy code stays bit-exact for
  the lineage signature.
* `cadBodyStateLegacyRepresentable` must exclude a fragment token anywhere
  (a cross-body `TopoRef` can name one while the dependent body is otherwise
  legacy-shaped).
* The chooser must re-resolve at confirm and be cancelled by a successful
  history step.
