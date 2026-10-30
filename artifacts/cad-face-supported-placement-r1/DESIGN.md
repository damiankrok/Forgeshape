# CAD-FACE-SUPPORTED-PLACEMENT-R1 — DESIGN

## One root cause, two symptoms

A face-supported CAD Body (`SceneObject::isFaceSupportedCad`) has no
independent placement: `ConstructionScene::resolveWorldModel` composes
`producerWorldModel · cadFaceFrameMatrix(face)` and never reads the body's own
stored transform. The renderer and the picker already use that model. Two other
consumers did not:

| Symptom | Consumer | What it read | Fix |
| --- | --- | --- | --- |
| CR-01 | GLB export (`captureGlbExportScene`) | the inert stored transform, so the body landed at the origin | read `resolveWorldModel` |
| CR-02 | typed Apply, gizmo activation, gizmo draw/hit/drag | wrote the stored transform: +1 Undo, a fingerprint move and a `.forge` change, with no visible effect | refuse, and withdraw the UI |

Fixing one alone leaves the other, so both land in this stage.
`resolveWorldModel`, face identity, lineage, tokens, `.forge`, `CADB`, the
fixtures and `DATA_PACKAGE_SPEC.md` are unchanged.

## T-01 export (`forgeshape_gltf_export.{h,cpp}`)

For a face-supported body, `captureGlbExportScene` asks
`scene.resolveWorldModel(id, &model)`:
- `model` gives the node translation (columns 12..14);
- the rest is the baked linear part `L`, with `mat4NormalMatrix(L)`
  (`transpose(inverse(L))`) for normals.

This is the same `Model = T·L` split every other body gets (ARCH-OWNER-07).
For an independent body the code path is untouched, so its bytes are
unchanged (`FSP_E08` pins the base exporter's digest).

If the support does not resolve (stale lineage, missing producer, bad token or
cycle), the export is refused as the appended
`GlbExportStatus::UnresolvedPlacement`, with zero bytes and nothing changed.
There is no fallback to the stored transform: a file that silently misplaces a
body is the defect being fixed. JNI already logs
`FORGESHAPE_GLB_EXPORT_FAIL:<name>` and returns null, and Java already shows
the generic export failure, so no JNI or Java mapping was needed.

## T-02 refusal (`forgeshape_body_commands`, `forgeshape_gizmo`, `forgeshape_jni`)

**Typed write.** `applySceneBodyTransform(id, values, scene, history, &why)`
is the one guarded typed placement write. Before any edit opens it refuses:
- `UnknownBody`;
- `Locked`, which keeps the existing lock's precedence and code;
- `DerivedPlacement`.

Otherwise it does `ScopedConstructionEdit` + `applyTransformValues`, exactly
what JNI did inline before. JNI `applyBoxTransform` calls it. The
`DerivedPlacement` refusal returns the appended code
`APPLY_REJECTED_DERIVED_PLACEMENT = 11` and logs
`FORGESHAPE_CONSTRUCTION_TRANSFORM_REJECTED:<label>:DerivedPlacement`.

**Gizmo.**
- `GizmoSession::snapshot` reports nothing visible over a dependent, so
  nothing is drawn and nothing can be hit.
- `beginDrag` returns false.
- JNI `setGizmoActive(true)` refuses one and logs
  `FORGESHAPE_GIZMO_REFUSED:derived_placement`.

The predicate is the one `deleteSceneBody` and Duplicate's
`RefusedFaceSupportedCad` already use, not a second rule.

**UI (Java).** `EditorWorkspaceView` asks the existing
`sceneActiveBodyIsFaceSupportedCad` on every refresh. For a dependent:
- the gizmo is not offered;
- `transformOffered` is false;
- `PresentationState.placeOffered` is false, so the Tool Rail draws Shape only.

A user holding Transform when a dependent becomes active is moved to Shape, and
Relative Scale is closed, so the rail, the precision surface and the handles
agree. The placement editor maps code 11 to
`reject_transform_derived_placement`. Nothing is remembered in Java.

## What did not change, and why

| Item | Why it stays |
| --- | --- |
| The stored transform | Still decoded, encoded and fingerprinted exactly as before (OD-16). A legacy file with a non-identity stored dependent transform round-trips byte for byte (`FSP_T06`). Normalising it would be a format decision. |
| Lock on a dependent (OD-29) | Still settable and still reported first: a locked dependent refuses with `Locked`. |
| Producer and independent bodies | Fully transformable; a producer move carries its dependents (`FSP_T03`, device E2E). |
| Undo and Redo | A step recorded before this stage restores whatever it recorded (`forgeshape_history.cpp`). No migration. |
| Body Dimensions / Relative Scale, Mirror, Duplicate | They already refused a CAD Body or a dependent. |

## Cost

The export resolves a dependent's whole producer chain once per body. That is
O(depth) per body, so O(n²) only for a pathological n-deep chain. Measured on
the host: a 12-body scene with an 11-deep chain exports in about 0.42 ms, and a
12-independent-body scene is unchanged at about 0.09 ms (`evidence/perf`).
Export is a one-shot read, so no cache was added.
