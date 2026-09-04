# Arc and Spline (`SKETCH-UX-R1` D)

## The one decision both share

**The authored points are the truth; everything else is derived.** That is the
same rule a rectangle already follows (centre and two sizes, not four corners)
and a circle already follows (centre and radius, not 32 vertices), applied to
curves.

## The Arc: three points ON the curve

```
struct SketchArc { SketchPoint start, mid, end; };     // 48 bytes in the file
```

`start` is where it begins, `end` is where it ends, and `mid` is a point it
passes through.

**Why not a centre, a radius and two angles?** Because three points have no
ambiguity to resolve. There is exactly one circle through three non-collinear
points, and exactly one of its two arcs contains the middle point. The
centre/angle form would have to store a sweep direction AND a major/minor flag
beside the endpoints, and every edit would have to keep all four consistent
with each other. It is also what the gesture naturally produces.

`arcGeometry` derives the circumcentre, the radius, the start angle and the
**signed** sweep. The collinearity test is scaled by the triangle's own extent
rather than an absolute epsilon, so three points a kilometre apart and three a
millimetre apart are equally collinear when their area is negligible against
their own size (`CADUXR1-25`).

Refused by name, never repaired: two coincident points, three collinear points,
a derived radius that is not a usable Construction length.

## The Spline: the points its curve passes through

```
struct SketchSpline { std::vector<SketchPoint> points; };   // 2..32 points
```

A Catmull-Rom through the authored points, converted span by span to the
equivalent cubic Bezier. The curve **interpolates** every stored point, which is
the property that makes it editable in the obvious way: move a point, the curve
moves there. `CADUXR1-29` asserts the tessellation passes exactly through each
authored point.

The end spans reflect their one neighbour, so the curve still leaves the first
point and reaches the last with a defined direction.

Deliberately not a NURBS, not a fitted stroke and not a knot vector: an
interpolating spline through a bounded point list is deterministic, editable
point by point, and small enough to store and to snapshot.

Refused by name: fewer than two points, two coincident consecutive points, and
**two ends that meet** — a chainable entity whose ends coincide is a self-loop
the one chain walker cannot read, and it is refused rather than half-supported.

## Tessellation is derived, deterministic and bounded

| | rule | bound |
| --- | --- | --- |
| Arc | the density a full circle gets: `ceil(|sweep| / 2π × 32)` | 4..64 segments |
| Spline | 8 segments per authored span | ≤ 31 spans |
| Any profile polygon | | ≤ 1024 vertices, else refused `TooManyEntities` |

**No camera, no zoom, no window** appears anywhere in the derivation. A profile
that changed shape when the user pinched would make the extruded solid depend
on how the sketch was looked at, which is the defect this rules out by
construction.

The two ENDS of both curves are written as the **authored points themselves**,
not re-evaluated from a derived angle or parameter — so a chain closes on the
numbers the snap produced rather than an ulp away from them.

## One chain walker for three entity kinds

What was `chainLines` is now `chainCurves`. Lines, Arcs and Splines are all
**chainable**: `sketchEntityEndpoints` gives each its two ends, and the walker
joins them by coincident endpoints exactly as before. A Rectangle, a Circle and
a closed Polyline still close a profile on their own.

The separation that matters: **connectivity is decided from authored
endpoints, and only then does each member contribute its derived points to the
polygon.** So a denser tessellation can never open or close a profile.

Each contributed polygon edge carries `(entityId, localIndex)` in the member's
**own canonical start→end order**, so a face token does not depend on which way
round the chain happened to be walked.

## Curved sides are never sketch supports

`ClosedProfile` gains `edgeCurved`, parallel to the polygon. A side face whose
edge is curved is a facet approximating a curved surface, so
`forgeshape_cad_face.cpp` reports it (a tap resolves cleanly) but never marks it
eligible — the same answer a circle's cylindrical side already got.

It is decided **per polygon edge** rather than per profile because one profile
may mix both: in the `cad_arc_profile` fixture the straight chord's side IS
eligible while every arc facet is not. `CADUXR1-27` counts exactly one eligible
side and more than one ineligible one on that body.

## What was not added

No tangent constraint, no radius constraint, no dimensional constraint solver,
no curve boolean, no offset, no trim, no extend, no fillet. A curve is a
deterministic bounded shape the profile engine can read; it is not the start of
a constraint system.
