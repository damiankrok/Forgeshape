# Parent/child transform contract (`CAD-A3` F)

The highest-risk part of the stage. The rule: a sketch attached to a face stays
physically attached.

## The mechanism

A face-supported CAD body has NO independent placement. Its authored geometry is
in its own local XY space; its WORLD model is derived every snapshot as

    childWorldModel = producerWorldModel * cadFaceFrameMatrix(resolvedFace)

The renderer, the picker and the exact-value editors all read this ONE model
from the scene snapshot (the snapshot writes the resolved model, its affine
inverse and its normal matrix, all arbitrary matrices the pipeline already
consumes). So:

- Parent MOVE / ROTATE: the dependent follows exactly, because the composition
  is recomputed from the producer's current model. Nothing is stored on the
  child, so there is no follow-state to go stale. Verified (native `CADA3-33`:
  a producer move and a 90-degree rotate carry the dependent).
- Parent SCALE: the dependent follows too (the composed matrix includes the
  scale). A non-uniform parent scale scales the dependent's apparent size on the
  face -- honest and bounded.
- Parent SUPPORTED PARAMETER EDIT (rectangle size, depth, radius, direction):
  the face re-resolves and the dependent regenerates deterministically, its own
  authored dimensions untouched. Verified (native `CADA3-31/32`: a cap-supported
  child follows a depth edit, a side-supported child follows a height edit).

## Independent child placement

R1 decision, per the brief's F3 allowance: a face-supported child is NOT
independently transformable. Its placement is fully derived, `transform()` is
not its placement, and `SceneObject::isFaceSupportedCad()` marks it so the gizmo
and the exact-value editors leave it alone. The geometry remains a useful,
durable CAD body; a future stage may add a stored offset relative to the support
frame. Documented as bounded debt rather than a half-attached behaviour.

## Why this is not BLOCKED-CAD-DEPENDENCY-CONTRACT

The existing `SceneObject` model made a safe associative relation POSSIBLE
without a hierarchy rewrite, because the snapshot already carries arbitrary
model/inverse/normal matrices. The derived-placement path is small and additive;
no second solver, pivot or placement was introduced above JNI.
