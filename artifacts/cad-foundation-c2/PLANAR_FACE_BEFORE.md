# CAD-FOUNDATION-C2 — planar-face preflight, BEFORE

Baseline `origin/main = 3872120`. Source-first trace of how a sketch becomes an
extrudable region, written before any geometry change. Every path is under
`app/src/main/cpp/` unless stated. Reproductions: `testPlanarFaceBefore` in
`forgeshape_cad_feature_selftest.cpp` (checks `CADFC2_PF_01a`..`CADFC2_PF_04b`,
16 checks, all PASS on the host, CAD_FEATURE 168 → 184).

## Verdict

**The region model is loop NESTING, not a planar arrangement.** No pair of
curves is ever intersected, no curve is ever split, and a region is always
"one whole source loop minus the whole source loops directly inside it". A
bounded cell whose boundary is made of pieces of two source curves — the
OWNER's lens, crescent and protrusion — does not exist anywhere in the model,
so it cannot be tapped, selected, previewed, extruded or stored.

Classification: **Class B — `BLOCKED-CAD-C2-PLANAR-FACE-IDENTITY`.** See §4
and `PLANAR_FACE_MODEL_PROPOSAL.md`.

## 1. The pipeline, as it is

| Step | Where | What it does with intersections |
| --- | --- | --- |
| Entities | `forgeshape_sketch.h:333-425`: Line, Polyline, Rectangle, Circle, Arc, Spline; ids monotonic from `CadSketch::nextEntityId` (`addSketchEntity`, `forgeshape_sketch.cpp:669-693`) | — |
| Snap | `collectSnapPoints` `forgeshape_sketch_session.cpp:73` | Offers authored points only (ends, vertices, corners, centres). No intersection or on-curve snap. |
| Coincidence | `coincident` `forgeshape_sketch.cpp:109` (1 µm) | Joins AUTHORED endpoints only. |
| Crossing test | `sketchSegmentsIntersect` `forgeshape_sketch.cpp:171` | A boolean cross-or-touch test. It computes no point. |
| Chaining | `chainCurves` `forgeshape_sketch.cpp:883` | Line/Arc/Spline only (`sketchEntityEndpoints` `:376`). Nodes are coincident endpoints; degree > 2 → `BranchingChain`, degree < 2 → `OpenProfile` (`:986-1002`). A rectangle/circle/polyline is never a node. |
| Loop validation | `validateLoop` `forgeshape_sketch.cpp:788` | Crossing WITHIN one loop → `SelfIntersectingProfile`. Never looks across loops. |
| Loop extraction | `extractClosedProfiles` `forgeshape_sketch.cpp:1076` | Rectangle / Circle / closed Polyline = one loop each, anchored by its own id; chains anchored by their smallest member. Rejections are collected, not raised. |
| Regions | `extractSketchRegions` `forgeshape_sketch_region.cpp:211` | Pairwise `loopsTouch` (`:23`) → `conflict`, else one-vertex containment; parent = smallest clean container; holes = direct children. Header states it: "This is nesting, not a planar arrangement: loops never split each other" (`forgeshape_sketch_region.h:39`). |
| Tap | `sketchRegionAt` `forgeshape_sketch_region.cpp:382` | The smallest-area WHOLE loop strictly containing the point; conflicts ignored. |
| Toggle | `SketchSession::toggleRegion` `forgeshape_sketch_session.cpp:1298`, `toggleRegionAt` `:1334`, `toggleRegionSelection` `forgeshape_sketch_region.cpp:403` | Pure toggle, then `validateRegionSelection`; refusal keeps the selection. |
| Selection rule | `validateRegionSelection` `forgeshape_sketch_region.cpp:303` | Conflicting loops chosen together → `OverlappingRegions` (`:355-357`); stored holes ≠ derived → `ProfileRegionMismatch` (`:329-331`). |
| Identity | `ProfileRegionRef {outerAnchorId, holeAnchorIds}` `forgeshape_sketch_region.h:88-91` | Names a region by WHOLE source loops. |
| Preview | `SketchSession::evaluateCandidate` `forgeshape_sketch_session.cpp:1579` | Nothing chosen → `AmbiguousProfile`; else `regenerateCadBody(candidateState())`. |
| Extrusion | `mergeSelectedRegions` `forgeshape_sketch_region.cpp:428`, `deriveFeature` `forgeshape_cad_feature.cpp:103` | Parity over the NESTING tree; one prism per component. |
| Persistence | `writeCadRegions` `forgeshape_project_document.cpp:974`, `readCadRegions` `:1637`, v5 chosen by `cadDocumentNeedsV5` `:366`; layout `DATA_PACKAGE_SPEC.md` §7f (`:710`), "There is no planar arrangement" in its region rules | Stores `profileEntityId` + REGIONS (hole anchors, further outer anchors). |

## 2. The ten questions

1. **Line-line intersections split into new semantic vertices?** No. Only
   coincident authored endpoints meet (`coincident`, `chainCurves`). A crossing
   inside one loop is refused (`SelfIntersectingProfile`); between loops it is
   only a `conflict` flag.
2. **Line-circle?** No. A circle is its own 32-gon loop
   (`circleProfilePolygon`, `kSketchCircleSegments`); a line never meets it.
3. **Circle-circle?** No. Two crossing circles are two whole loops with
   `conflict = 1` (PF-03).
4. **Overlapping closed loops → planar cells?** No (region header `:39`).
5. **Touching loops → faces?** No. Touching counts as intersecting
   (`sketchSegmentsIntersect`), so touching loops are `conflict` and each stays
   its own region (`CADVS_REG_14`, `_15`, `_17`).
6. **`extractClosedProfiles` and intersections.** It ignores crossings between
   loops entirely, refuses crossings inside one loop
   (`SelfIntersectingProfile`), and treats a T-junction (an endpoint in the
   middle of another curve) as a degree-1 node → `OpenProfile` for the chain,
   while the curve it touches is unaffected.
7. **Why Case A (circle crossing rectangle) fails.** Both loops are valid and
   become two whole-loop regions with `conflict` set
   (`forgeshape_sketch_region.cpp:225-228`). Nothing is dropped and Finish
   succeeds, but: (a) no lens / rectangle-minus-lens / outer-cap cell exists;
   (b) a tap in the lens or the outer cap selects the WHOLE circle
   (`sketchRegionAt`); (c) choosing both is `OverlappingRegions`
   (`validateRegionSelection :355`); (d) choosing the rectangle alone extrudes
   the full rectangle, ignoring the circle.
8. **Why Case B (line protrusion) fails.** The rectangle's edge is not a chain
   node, so the protrusion's lines end in degree-1 nodes → `OpenProfile`
   (`chainCurves :999-1000`), whether the ends sit mid-edge or cross it. Finish
   then succeeds on the rectangle alone and auto-selects it
   (`SketchSession::finish :1221-1234`, `reconcileRegionSelection`); the
   rejection is read nowhere else, so the protrusion is SILENTLY dropped from
   what the user can choose. Drawn instead as a closed polyline through the
   edge, it is Case A.
9. **Can `ProfileRegionRef` name an intersection-derived face?** No. It names
   a region by ONE outer source loop (by anchor) and whole hole loops. The lens
   of Case A is bounded by an arc of the circle and a segment of the
   rectangle's right edge: there is no single outer anchor, and the SAME
   anchors (rectangle 1, circle 2) would have to name three different cells.
   Reusing `{outer: 1}` for "rectangle minus lens" would also silently change
   what every existing v1–v5 record `{outer: rectangle}` over such a sketch
   means (today: the whole rectangle).
10. **Can such identity survive edit/save/reopen deterministically?** Not with
    what is stored. Entity ids are stable and never reused, and
    `(edgeEntityId, edgeLocalIndex)` names a source edge, but nothing names a
    FRAGMENT of an edge between two intersections, nor which side of each
    curve a cell lies on. Deriving it from array order, tessellation or
    triangle index is what the prompt and `CLAUDE.md` forbid.

## 3. Reproductions (host, all PASS)

| Case | Checks | Current behaviour recorded |
| --- | --- | --- |
| PF-01 rectangle 4×3 + circle r 0.5 at (2, 0) | `CADFC2_PF_01a`..`01f` | Each entity valid alone; crossings at (2, ±0.5) derivable but not computed — rectangle still 4 corners, circle still 32 vertices, each loop one source curve; 2 whole-loop regions (not 3 faces), `conflict`; taps: (−1, 0) → rectangle, lens (1.8, 0) → circle, outer cap (2.3, 0) → circle; both → `OverlappingRegions`; rectangle alone = full 12 m². Session: Finish OK, candidate `AmbiguousProfile`, a lens tap selects the whole circle. |
| PF-02 rectangle + 3 lines (2,−0.5)→(3,−0.5)→(3,0.5)→(2,0.5) | `CADFC2_PF_02a`..`02e` | Chain `OpenProfile` (anchor = smallest line), 1 region (rectangle); (2.5, 0) taps nothing; the crossing variant is `OpenProfile` too; a closed crossing polyline is a whole conflicting loop (Case A); session: 4 entities, Finish OK on the rectangle alone, auto-selected, protrusion tap does nothing. |
| PF-03 circles r 1 at (±0.4, 0) | `CADFC2_PF_03a`..`03c` | 2 conflicting whole loops; the lens taps to circle A by anchor order (equal areas), each crescent to its own circle; both → `OverlappingRegions`. |
| PF-04 rectangle + circles at (±1, 0) r 0.5 (nested) | `CADFC2_PF_04a`, `04b` | 3 regions, rectangle holes {A, B}, no conflicts, taps correct; O+A+B union valid. Unchanged — the supported case keeps working. |

These checks are labelled BEFORE: a planar-face stage is expected to flip the
ones it supersedes, by name, and keep PF-04.

## 4. Classification

**Class B.** Intersection-derived faces need, at minimum, a durable name for a
curve FRAGMENT (source entity + the two intersection nodes that bound it) and
a face identity built from signed fragments, or an equivalent seed rule. None
of this is expressible in `ProfileRegionRef` / `CADB` v5 without either
reinterpreting old records or inferring identity from order. The HUD
correction proceeds; planar-face implementation stops at
`BLOCKED-CAD-C2-PLANAR-FACE-IDENTITY`. The next model, its identity, rebind and
migration rules are in `PLANAR_FACE_MODEL_PROPOSAL.md`.
