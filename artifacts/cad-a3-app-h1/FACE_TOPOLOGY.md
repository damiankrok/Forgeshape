# Semantic CAD face topology (`CAD-A3`, `ARCH-OWNER-13`)

`forgeshape_cad_face.{h,cpp}` turns a `CadBodyState` into a bounded set of
PLANAR faces, each with a stable SEMANTIC identity and a deterministic frame.

## Which faces exist

An extrusion of a profile with `n` edges exposes `2 + n` planar faces:

| Face | Token kind | Eligible as a sketch support |
| --- | --- | --- |
| The cap ON the sketch's support plane | `CapPlane` | yes |
| The cap at the far end of the extrusion | `CapFar` | yes |
| One planar side per profile edge | `Side(edgeEntityId, edgeLocalIndex)` | yes, EXCEPT a circle's |

A rectangle has four sides (tokens `Side(R, 0..3)`), a closed polyline `n`
(`Side(P, 0..n-1)`), a line chain one per line (`Side(lineId, 0)`). A CIRCLE's
side is cylindrical: its ranges are still reported so a tap on it resolves, but
it is never eligible, and no fake planar face is invented for it.

## The frame

Each face carries `origin, u, v, n` in the body's LOCAL space, right-handed and
orthonormal (`u x v = n`), with `n` the OUTWARD normal. A cap's normal points
away from the solid; a side's is `edgeDir x planeNormal` (outward for a CCW
profile, direction-independent). A child sketch authors on its own canonical
local XY, and `cadFaceFrameMatrix` (columns `u, v, n`, translation `origin`)
maps that onto the face; the child's world model is the producer's world model
times that matrix.

## Triangle ranges, and why they are transient

`cadFaceRanges` returns the index RANGES of the generated mesh tagged with their
semantic tokens, in the exact order `generateCadMesh` emits them (far cap, near
cap, then six indices per side edge). It exists ONLY so a rendered-triangle pick
resolves to a semantic face; it is NEVER persisted. Which mesh cap is `CapPlane`
and which is `CapFar` flips with the extrusion direction, and the ranges account
for that.

## The lineage signature

`cadTopologySignature` hashes the SET of face tokens a body exposes (the chosen
profile id, the face count and every token/eligibility). A supported edit -- a
rectangle's size, a circle's radius, a depth, a direction -- does not change
that set, so a reference stays valid; a change that removed the referenced face
changes the signature and the reference fails closed rather than retargeting.

## Verified

`CADA3-16..23` in the native suite: two caps and four sides for a rectangle,
frames orthonormal and outward, ranges tiling the index buffer, lineage stable
across size/depth/direction edits and changed by a profile-kind change, and a
circle's caps eligible with its cylindrical sides not.
