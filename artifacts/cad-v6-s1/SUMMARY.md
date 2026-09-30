# CAD-V6-S1-MODEL-CODEC-R1 — the retained sketch table, the selection variant and `CADB` v6

Intermediate branch `feature/cad-v6-sketch-face-r1`, from `origin/main =
6c9f156`. **Not merged to `main`, by design**: the branch encodes v6 before any
runtime path consumes planar faces. Model and persistence only.
`BEFORE.md` (commit `7bb671c`) was written before any product edit.

## The model

```
CadBodyState
    sketches[]      CadSketchRecord { sketchId, hasFeatureSupport, featureSupport, sketch }
                    strictly ascending, body-local, non-zero ids; THE sketch truth
    nextSketchId    high-water mark
    baseSketchId    the base feature's sketch (the ROOT: workplane or TopoRef)
    extrude         the base's ExtrudeFeature (+ CadSelectionKind, planarFaces)
    laterFeatures[] CadFeature { featureId, operation, sketchId, extrude }
    nextFeatureId   high-water mark (the audit's id-reuse finding)
```

* **No feature carries a sketch.** A feature names one by id; two may name the
  same one; a sketch no feature names may be retained.
* **Placement is the sketch's**: exactly one ROOT sketch (the base's, on its
  workplane or `TopoRef`); every other stands on a face of one of the body's own
  features (`CadFeatureSupport`, moved from `CadFeature` onto the record).
* **The base is a transitional structure, and why**: its id (1) and operation
  (New Body) are the two facts every v1..v5 record implies and `CadFeatureView`
  synthesises; a slot for them would be two fields whose only legal values are
  constants. Its SKETCH is in the table and referenced by id like every other.
  There is one authoritative record per sketch; no inline copy survives.
* **`nextFeatureId` was necessary**: with retained sketches whose support names
  a feature id, a deleted feature's id handed to a new feature would silently
  retarget that support. Persisted in v6 only; v1..v5 derive `last + 1`.
* **Selection variant**: `ExtrudeFeature::selection` is `LoopRegions` (the v5
  fields) or `PlanarFaces` (`std::vector<PlanarFaceRef>`, PF-S1's own type).
  A selection carrying the other kind's payload is refused.

Doors: `cadBaseSketch`, `findCadSketchRecord`, `cadFeatureSketchRecord`,
`makeCadBodyState`, `addCadSketchRecord`, `appendCadLaterFeature[WithSketch]`,
`cadBodyStateLegacyRepresentable`, `cadBodyStateUsesPlanarFaces`,
`validatePlanarFaceRefForm`, `validatePlanarFaceSelection`, `validateCadChain`.

## Statuses (appended `CadStatus` 45..60)

`SketchIdInvalid`, `DuplicateSketchId`, `SketchNotFound`, `HighWaterInvalid`,
`TooManySketches`, `SketchSupportInvalid`, `InvalidSelectionKind`,
`PlanarFaceRefMalformed`, `PlanarFaceRefNotCanonical`, `DuplicatePlanarFace`,
`PlanarFaceUnresolved`, `PlanarFaceUnsupportedCurve`,
`PlanarFaceAmbiguousOverlap`, `PlanarFaceCapExceeded`, `PlanarFaceDegenerate`,
`PlanarFaceRegenerationUnavailable`. Reused: `TooManyFeatures` (feature ids),
`FeatureSupportInvalid` (a support that does not resolve), `ProfileNotFound`
(no face chosen), `TooManyRegions` (more than 16 faces). Codec:
`ImpossibleCount`, `InvalidSemanticValue`, `BadPayload` (reversed byte ≠ 0/1),
`MissingRequiredSection` (the runtime refusal of a face selection on load).

## The format

`DATA_PACKAGE_SPEC.md` §7g: body = objectId, nextSketchId, nextFeatureId,
sketch table (placement 1 workplane / 2 TopoRef / 3 feature face, then the §7b/§7d
entities), features (id, operation, sketchId, extent, direction, depths,
selectionKind, then `REGIONS` or `FACES`). A cut is its kind (1/2/3) plus, for
an intersection only, partner entity, partner edge and ordinal. No coordinate,
float, index, tessellation or triangle identifies a face.

**Conditional writer**: v6 only when `cadBodyStateLegacyRepresentable` is false
(shared or retained sketch, non-derived ids or high-water marks, a face
selection). **Legacy read**: base → sketch 1, later feature k → sketch k+1,
high-water marks derived, two identical inline sketches stay two.

## Scope held

No session tap change, no viewport face selection, no Extrude/Add/Cut from
faces, no JNI exposure of `SketchId`, no Android change, no hatching, no sketch
browser. `regenerateCadBody` refuses a face selection by name; the runtime
refuses to load one (`runtimeCanEvaluateProject`). The one regeneration change:
the `SupportFaceLost` check now runs only for a sketch that stands on a feature
face, because a later feature re-extruding the ROOT sketch stands on none.
