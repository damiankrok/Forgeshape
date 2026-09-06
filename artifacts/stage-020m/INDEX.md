# Stage 020M — Body Dimensions and Relative Scale

**Construction-only.** Exact overall body dimensions with a per-axis anchor, a
temporary Relative Scale multiplier, and one shared resize/anchor solver that
Stage 020D's Directional Scale is meant to reuse.

Run under `TEST-OWNER-03` (reduced testing): no `-FullSharded`, no screenshot
matrix, no contact sheet.

## What this stage added

| Concern | Where it lives |
| --- | --- |
| Local bounds, dimensions, the resize/anchor solver, Relative Scale, the two product acts | `app/src/main/cpp/forgeshape_body_dimensions.{h,cpp}` |
| The dimension leaders and the Dimensions interaction state | `app/src/main/cpp/forgeshape_body_dimension_overlay.{h,cpp}` |
| `DIM020M-01..16` | `app/src/main/cpp/forgeshape_body_dimensions_selftest.{h,cpp}` |
| JNI: the state read, the mode, the anchor, the two applies, the label anchor | `app/src/main/cpp/forgeshape_jni.cpp` |
| The two entries and the anchor selector | `app/src/main/java/com/forgeshape/app/WorkspaceTrailingHostView.java` |
| The three viewport labels and their compact editor | `app/src/main/java/com/forgeshape/app/BodyDimensionLabelsView.java` |
| The Relative Scale precision-surface body | `app/src/main/java/com/forgeshape/app/RelativeScaleEditorView.java` |
| The one device journey | `app/src/androidTest/java/com/forgeshape/app/BodyDimensionsSmokeTest.java` |

## The two definitions, stated once

```
dimension[a]        = unscaledLocalExtent[a] * absoluteScale[a]
newAbsoluteScale[a] = targetDimension / unscaledLocalExtent[a]
newAbsoluteScale    = oldAbsoluteScale (*) relativeMultiplier      (Relative Scale)
```

The extent is the Construction Source's own local-space bound, read from the
primitive parameters — never measured off generated vertices, and never a world
axis-aligned box. Rotation and the camera are not inputs to either.

## Anchor arithmetic

With `Model = T · Rz · Ry · Rx · S`, holding the point at local coordinate `b`
on axis `a` stationary while `S[a]` changes requires

```
T_new = T_old + (R · e_a) · (S_old[a] − S_new[a]) · b
```

`R · e_a` is column `a` of the rotation matrix, so the correction is carried
through the body's own orientation and is exact for a turned body. `b` is
`bounds.min(a)` for the negative side and `bounds.max(a)` for the positive.
**Centre writes no position at all** — that is the OWNER's stated rule, not an
inference from centred bounds.

## Evidence

- `FOCUSED_RESULTS.md` — the automated results and the elapsed time.
- `OWNER_LATER_TEST_PACK.md` — what no emulator settles.

## What this stage deliberately did NOT do

Directional Scale handles or mode (Stage 020D, blocked by OQ-01); Sculpt
dimensions (`SCULPT-DIM-01`, blocked by OQ-02); Imported Mesh and CAD Body
dimensions; CAD feature dimensions (a sketch length, a radius, an extrusion
depth — those stay CAD authored truth); multi-select and group scale; hierarchy;
snapping; Mirror; a new unit system; any `.forge` schema change.
