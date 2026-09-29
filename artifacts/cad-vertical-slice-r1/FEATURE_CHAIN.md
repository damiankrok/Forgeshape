# The retained feature chain: New Body, Add and Cut on the same body

This page describes how an Add or a Cut changes the body the sketch stands on,
**in place**. There is no second `SceneObject`, no hidden tool body, no
renderer compositing and no baked mesh.

- **Owning code:**
  - `forgeshape_cad_body.{h,cpp}` — the chain, regeneration and the operation
    rules;
  - `forgeshape_cad_feature.{h,cpp}` — per-feature geometry, faces and
    placement;
  - `forgeshape_cad_kernel.{h,cpp}` — the boolean seam;
  - `forgeshape_sketch_session.{h,cpp}` — staging, candidate evaluation and
    commit.
- **Native checks:** `CADVS_OPS_01..29` and `CADVS_SES_01..31`.
- **Device checks:** `CadVerticalSliceTest.new_body_then_add_same_body`,
  `same_body_cut`, `operation_refusal` and `feature_edit_roundtrip`.

## 1. Durable truth

```text
CadBodyState
    sketch + extrude        feature 1 — the base New Body extrusion (R0, field for field)
    laterFeatures[]         features 2..n
        featureId           stable, strictly ascending, > 1, never an index
        operation           Add | Cut   (a later feature never creates a body)
        support             { featureId (earlier), CadFaceToken, lineageToken }
        sketch              canonical XY sketch in the support face's frame, no TopoRef
        extrude             regions (outer + holes), extent, distances, direction
```

- **The base keeps R0's meaning.** Feature 1 is exactly what a CAD Body always
  was, so every `CADB` v1–v4 record still means what it meant.
- **Bounds.** `kMaxCadFeatures = 16`, base included (`CADVS_OPS_25`, `_27`).
  Regions are bounded as `PROFILE_REGIONS.md` §3 states.
- **Never stored.** Nothing derived is stored: no intermediate solid, no kernel
  output, no face tag, no triangle.

## 2. Where a later feature stands

- **Its support.** A later feature's sketch stands on a planar face of an
  EARLIER feature of the SAME body, named semantically as
  `(featureId, CadFaceToken)`.
- **Its placement.** The sketch's placement is DERIVED on every regeneration.
  It comes from that earlier feature's own prism, face frame and double
  precision, never from a boolean result.
- **Why not a `TopoRef`.** A `TopoRef` names ANOTHER body and derives that
  body's world placement. A body that supported itself through one would be a
  cycle.
- **Lineage.** The support carries the earlier feature's lineage signature, on
  the `TopoRef` rule (DATA_PACKAGE_SPEC §7f):
  - a size edit upstream keeps the feature attached and carries it with the
    face (`CADVS_OPS_18`, `_19`);
  - a structural edit that changes the face structure refuses the edit by name
    (`FeatureSupportInvalid`, `CADVS_OPS_20`, `CADVS_SES_30`);
  - nothing is ever retargeted to the nearest face (`CADVS_OPS_12`).
- **The face must survive.** A support face must still exist on the solid built
  by the features BEFORE the dependent one. A face an earlier Cut carved away
  entirely is `SupportFaceLost` (`CADVS_OPS_16`).
- **Ineligible faces.**
  - A Cut's faces are the inside of a pocket, facing the other way, and a
    sketch may not stand on them.
  - The curved side of a circle, an arc or a spline is ineligible, as it always
    was.
  - Both are refused (`CADVS_OPS_13`, `_15`).

## 3. Regeneration: ordered and atomic

`regenerateCadBody` has three paths.

- **Legacy.** A base-only body with one simple region takes the unchanged R0
  float path. It is bit-identical to every earlier build (`CADVS_EXT_12`).
- **Regions.** A base-only body with holes or several regions uses the
  double-precision prism generator. No kernel is needed, because the selected
  regions are validated disjoint.
- **Chain.** The chain path works as follows:
  - it builds the base solid, then applies every later feature in order through
    ONE kernel boolean each (`Union` for Add, `Difference` for Cut);
  - it checks each result against the operation rules below;
  - it publishes only when the WHOLE chain is valid.

`CadBody::applyState` validates the whole requested state and writes nothing
unless all of it passes:

- a failing chain changes neither the state nor the cache (`CADVS_OPS_22`);
- regenerating twice is bit-identical (`CADVS_OPS_21`);
- a capture and a restore are bit-exact (`CADVS_OPS_24`).

**Face identity through booleans.** Every triangle of every input carries a
face tag, which indexes a table of `(featureId, face token, eligible)`. The
kernel carries the tag through the boolean, so a triangle of an Add or Cut
result still says which semantic face it came from. The chooser and the
dependents read faces through that table and never through a triangle index.

## 4. Operation rules

These are measured on the kernel's own result:

| Rule | Status |
| --- | --- |
| An Add that increases the number of shells would make a separate piece | `AddDisjoint` (`CADVS_OPS_07`) |
| An Add that gains less than `1e-9 × tool volume` changes nothing | `AddNoEffect` (`CADVS_OPS_06`) |
| A Cut whose result is empty would remove the body | `CutRemovesBody` (`CADVS_OPS_09`) |
| A Cut that removes less than `1e-9 × tool volume` misses the body | `CutNoIntersection` (`CADVS_OPS_08`) |
| A later feature marked New Body, or an unknown code | `InvalidFeatureOperation` (`CADVS_OPS_10`) |
| Add or Cut from a sketch with no body to act on | `OperationNeedsTarget` (session) |
| The kernel refuses an input or a result | `KernelFailed` |

When a later feature is refused, the refusal names that feature's id in
`CadRegenerationReport::failedFeatureId`, and the previous state stands.

## 5. Target, operation and direction

- **The target.** A sketch on a planar face of a CAD body targets **that body**.
  The face-sketch chooser stages the producer's chain, so Add and Cut are
  offered there (`CADVS_SES_14`). A world-plane sketch has no body to act on,
  so it offers New Body alone (`CADVS_SES_10`). The prompt allows a free
  workplane sketch to *suggest* a target; this slice does not suggest one, and
  never guesses.
- **A face-sketch New Body.** It remains possible, and it still creates an
  independent body whose world placement follows the face (`CAD-A3`).
- **Direction follows the operation.** For a new One Side feature on a face,
  choosing Cut points the extrusion INTO the body (against the face's outward
  normal), and Add or New Body points it out (`CADVS_SES_31`). It is a
  direction change through the one writer, never a negative depth. Flip still
  reverses it, and an edit keeps the side the user already chose.

## 6. Commit, edit, Undo

- **Commit.** An Add or a Cut commits through `SketchSession::commitIntoTarget`:
  - ONE `ScopedConstructionEdit` around ONE `applyState` on the SAME
    `SceneObject`;
  - the id, the transform and the Objects list do not move;
  - one Undo removes exactly that feature and Redo restores it
    (`CADVS_SES_16..21`; device: `new_body_then_add_same_body`,
    `same_body_cut`).
- **Refused commits.** A refused commit changes nothing and records nothing
  (`CADVS_SES_23`, `_24`; device: `operation_refusal` compares project bytes).
- **Editing a feature.** It opens as a staged copy (`sketchBeginEditFeature`).
  - Feature 1 opens the ordinary Edit Sketch.
  - A later feature opens in Ready on its own extrusion, with Back to Sketch one
    step away for its drawing.
  - Finish is ONE step that rewrites that feature and regenerates it and
    everything after it; Undo and Redo restore whole chains (`CADVS_SES_25..28`;
    device: `feature_edit_roundtrip`).
- **Dependents.** Another body whose sketch stands on a face of this body is
  checked on the existing `DependentFaceLost` terms. The check is geometric:
  the named face must still be carried by the new mesh.

## 7. Preview is the candidate

- **One candidate.** The session evaluates ONE candidate at a time,
  latest-only. It is keyed by a candidate revision that every authoring change
  bumps: a region toggle, a distance, an extent, an operation or a flip.
- **The same object.** The preview the renderer draws and the commit are the
  SAME evaluation, so what was seen is what is committed.
- **Drawing it.** On the render thread the target body's draw item is
  substituted with the candidate mesh:
  - it uses the same key, with a revision drawn from a separate space;
  - it is tinted by operation (Add green, Cut red, New Body blue, through the
    existing selection-tint slot);
  - a New Body preview is an extra draw item.
- **Nothing becomes truth.** None of it moves a revision of truth, a history
  step, the fingerprint or the autosave.
- **An invalid preview.** It is named in the status line and in the HUD badge,
  and the toolbar's Extrude is withdrawn. Native still refuses the commit.
- **Timings.** See `PERF_NOTES.md`.

## 8. Not in this slice

- Feature delete and reorder.
- Suppression.
- A timeline.
- Intersect.
- Revolve.
- Fillet, chamfer, shell and draft.
- A sketch on a curved face.
- Projected edges.
- A constraint solver.
- A target chosen for a world-plane sketch.
- Multi-body Cut.
