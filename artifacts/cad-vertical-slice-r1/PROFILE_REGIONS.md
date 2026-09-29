# Profile regions with holes

This page describes how a user chooses what to extrude, now that a closed loop
drawn inside another becomes a hole instead of being refused.

- **Owning code:** `app/src/main/cpp/forgeshape_sketch_region.{h,cpp}` (the
  model), `forgeshape_sketch_session.cpp` (selection and taps), and
  `forgeshape_cad_feature.cpp` (the extruded prism).
- **Native checks:** `CADVS_REG_01..26` and `CADVS_EXT_01..16` in
  `forgeshape_cad_feature_selftest.cpp`.
- **Device check:** `CadVerticalSliceTest.owner_rectangle_circle_region`.

## 1. What a region is

`extractClosedProfiles` still reads every closed, simple loop out of the sketch.
Two things changed:

- It no longer throws the OUTER loop of a nest away as
  `NestedProfileUnsupported`.
- `extractSketchRegions` then turns each loop into exactly one region:

```text
region(L) = interior(L) − interior(each DIRECT child of L)
```

- **Direct child.** `B` is a direct child of `L` when `L` is the smallest loop
  that cleanly contains `B`. "Cleanly" means `B` lies strictly inside `L` and no
  edge of either loop touches or crosses an edge of the other.
- **The owner's sketch.** A rectangle around a centred circle therefore yields
  exactly two regions: the disk, and the rectangle minus the disk.
- **Deeper nesting.** It follows even/odd from the same sentence, with no second
  rule. A ring around an island is one region with one hole, and the island is
  its own region inside that hole (`CADVS_REG_09`, `_10`).

This is **nesting, not a planar arrangement**: loops never split each other.

- A loop that touches or crosses another is never a hole. It stays its own
  region, exactly as every earlier version read it.
- That is what keeps every `CADB` v1–v4 record meaning what it meant
  (`CADVS_REG_14`).

## 2. Identity is semantic

- **A region.** It is named by the anchor entity of its outer loop: the
  entity's own id for a rectangle, circle or polyline, and the smallest member
  id for a chain.
- **A stored selection.** A `ProfileRegionRef` holds the outer anchor and the
  ascending anchors of the holes it was chosen with.
- **What is never identity.** Nothing is a triangle index, a tessellation index
  or a list position.
- **When the holes change.** If a loop is later added inside a selected region,
  or a hole is removed or moved out, the stored holes no longer equal the
  derived ones. The selection is then refused by name (`ProfileRegionMismatch`)
  and never re-read as a different area (`CADVS_REG_18`, `_21`).
- **When the sizes change.** Resizing a rectangle or a circle keeps the region's
  identity and the stored selection (`CADVS_REG_20`).
- **Order.** Selections are canonical, ascending by outer anchor. A
  non-canonical stored order is a mismatch and is never silently re-sorted
  (`CADVS_REG_19`).

## 3. What is refused, by name

| Case | Status | Where |
| --- | --- | --- |
| A region's holes touch or cross each other | `OverlappingHoles` (listed, not selectable) | `CADVS_REG_16` |
| Two selected regions overlap, touch, or share a loop (a region and its own hole, e.g. ring + disk) | `OverlappingRegions` | `CADVS_REG_06`, `_15`, `_17` |
| More than 16 regions in one feature, or more than 64 holes in one region | `TooManyRegions` | caps below |
| A stored selection whose holes differ from the derived holes | `ProfileRegionMismatch` | `CADVS_REG_18` |
| Self-intersecting, zero-area, open or forked loops | as before (`SelfIntersectingProfile`, …) | `CADVS_REG_13` |
| A commit with nothing selected and more than one region | `AmbiguousProfile` | `CADVS_SES_03` |

**Caps** (`forgeshape_sketch_region.h`):

- `kMaxProfileRegions = 16` regions per feature.
- `kMaxRegionHoles = 64` holes per region.

Both keep a feature bounded, and with it a history step and a `.forge` record.
Both sit far above anything a finger draws on a phone.

## 4. Selection behaviour

- **Automatic selection.** `SketchSession::finish` selects automatically only
  when the sketch has **exactly one** selectable region. With more than one,
  nothing is selected, there is no arrow, and the toolbar's Extrude is absent and refused by
  name (`CADVS_SES_02`, `_03`; device: `owner_rectangle_circle_region`).
- **Choosing by tap.** In Ready a single-finger tap toggles the region under the
  finger (`sketchRegionAt`, innermost by area).
  - Adding a region drops any selected region it would overlap or share a loop
    with.
  - A tap outside every region changes nothing (`CADVS_SES_04..07`).
  - The tap is armed on Down and cancelled by travel past the tap slop or by a
    second pointer. It toggles on Up and never consumes the gesture, so an orbit
    still orbits.
- **The precision surface.** It lists every region with its outer-loop kind,
  its hole count and its area. Tapping a row makes that region the one region,
  which is what a list of choices means. Multi-region selection is done in the
  viewport, where the regions are seen.

## 5. What the user sees

- **Hatch.** A selected region is hatched in the sketch overlay
  (`sketchRegionHatch`, at most 96 lines, even/odd). A hole stays EMPTY: it is
  not hatched as solid (`CADVS_REG_24`).
- **Chosen loops.** They are emphasised.
- **Preview.** Each selected region's outer loop and holes get the extrusion
  preview's cap and wall lines.
- **Label anchor.** The value and the arrow stand at the region's area centroid
  when that point is on material. For a ring, whose centroid falls in the hole,
  they stand at an interior point that is on material instead (`CADVS_REG_05`).

## 6. The prism

A selected region with holes extrudes to a closed, watertight, outward-wound
prism (`appendCadFeatureSolid`):

- **Caps.** A cap with holes is triangulated by the kernel's region
  triangulator (`cadKernelTriangulateRegion`). The area it covers is checked
  against the region's own area.
- **Walls.** Outer walls face out and hole walls face into the hole
  (`CADVS_EXT_08`).
- **Volume.** For the owner's ring it is `(12 − 32-gon area) × depth`, within
  1e-9 relative (`CADVS_EXT_03`).
- **Legacy geometry.** A single simple region still goes through the product's
  own ear clipper on the unchanged R0 float path. A pre-existing body's mesh
  therefore does not move by one bit (`CADVS_EXT_12`, `_14`), and the disk of a
  nested sketch is bit-identical to a lone circle's cylinder (`CADVS_EXT_15`).
- **Several regions.** Two disjoint regions extrude as two closed components of
  one New Body (`CADVS_EXT_16`).

## 7. Not in this slice

- A planar arrangement that splits crossing loops into faces.
- Picking a region by a curve's inside where the loops touch.
- Region selection on a face sketch's projected edges (no projected edges
  exist).
