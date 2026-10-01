# CAD-FOUNDATION-C2 — planar-face model proposal (Class B)

Status: **PROPOSAL, not implemented.** Result of the C2 preflight
(`PLANAR_FACE_BEFORE.md`): `BLOCKED-CAD-C2-PLANAR-FACE-IDENTITY`. Nothing in
this file changed code, format or fixtures.

## 1. What is wanted, and why it cannot ride on v5

The OWNER's sketches — a circle crossing a rectangle, a line-built protrusion
closed against a rectangle's edge, two crossing circles — are one idea: the
sketch's curves divide the plane into bounded **atomic faces**, and a tap
chooses one. Today a region is one WHOLE source loop minus whole loops nested
in it (`ProfileRegionRef {outerAnchorId, holeAnchorIds}`). A lens bounded by an
arc of a circle and a piece of a rectangle's edge has no single outer anchor,
and the same two anchors would have to name three different cells. Encoding
those cells in the v5 fields would either reinterpret existing records (a v5
`{outer: rectangle}` over a crossed sketch means the WHOLE rectangle today) or
infer identity from derivation order. Both are prohibited. The new identity is
therefore a format change and belongs in `CADB` v6.

## 2. Pipeline (all derived, nothing new is authored)

```
authored entities (unchanged)
→ nodes: authored endpoints + T-junctions (an endpoint ON another curve,
  within kSketchCoincidenceMeters) + proper pairwise intersections
→ fragments: each source edge split at its nodes
→ planar graph (half-edge), dangling fragments pruned
→ bounded atomic faces (outer cycle + hole cycles)
→ face identity: canonical signed-fragment boundary (below)
→ selection = a SET of face identities
→ union (the C1 parity rule, restated over faces: a fragment bounds the union
  exactly when selection membership differs across it)
→ preview / New Body / Add / Cut through the unchanged candidate path
```

Intersections are ANALYTIC for the curve kinds that have a closed form — line
segments (rectangle sides, polyline segments, lines) and circles/arcs — never
computed on render tessellation. A Spline is intersected on its DERIVED
tessellation from `tessellateSketchCurve` (camera-independent, already the
extruded truth), and a fragment of a spline is named by the same rule; if that
is judged too fragile, splines stay whole-loop-only in the first slice and are
refused as face boundaries by name (`PlanarFaceUnsupportedCurve`). No arbitrary
curve kinds are added.

## 3. Minimal durable entities

Only ONE new durable thing: the **face reference** a feature's selection
stores. Everything it names is already durable (entity ids are monotonic,
never reused, persisted with `nextEntityId`).

```
Cut          = SourceEnd                                   (the edge's own end)
             | Intersection(partnerEntityId, partnerEdgeIndex, ordinal)
FragmentRef  = (entityId, edgeLocalIndex, startCut, endCut, reversed)
Cycle        = FragmentRef[]  (closed, canonical start)
PlanarFaceRef= (outer: Cycle, holes: Cycle[])
```

- `edgeLocalIndex` is the existing per-edge identity (`ClosedProfile
  ::edgeLocalIndex`, `CadFaceToken`): rectangle side 0..3, polyline segment i,
  0 for a line, circle, arc or spline.
- `ordinal` disambiguates several intersections between the same two edges:
  the k-th along the SOURCE edge's own parameter. A T-junction is an
  Intersection whose partner is the curve the endpoint lies on.
- Canonical form: each cycle rotated to start at its smallest FragmentRef
  (lexicographic on the tuple), orientation CCW for the outer and CW for
  holes, holes sorted by their first fragment. Two derivations of one sketch
  produce byte-identical refs; no array order, tessellation index, triangle
  index or screen coordinate enters.

Nothing else is stored: nodes, fragments, the graph, faces, areas and
centroids are regenerated on every read, exactly as regions are today.

## 4. Identity rules

1. **A face IS its canonical boundary.** A stored `PlanarFaceRef` resolves
   only if the derived arrangement contains a face whose canonical boundary is
   EXACTLY equal. There is no nearest-face search and no seed point.
2. **Nested sketches are unchanged.** When no two curves touch, cross or form
   a T-junction, every arrangement face is exactly one v5 region (outer loop
   minus direct children), fragment for fragment (every Cut is SourceEnd).
   Such a selection is still WRITTEN as v5 `REGIONS` (§6), so every existing
   file and fixture keeps its bytes.
3. **Union is above faces.** The C1 union rule is unchanged in meaning;
   `mergeSelectedRegions` becomes the special case of a face-set union whose
   shared fragments are whole loops.
4. **Side faces of a fragment.** An extruded side face gets
   `CadFaceToken{Side, edgeEntityId, edgeLocalIndex}` today. A side standing on
   a FRAGMENT needs the two Cuts added to the token; a side on a whole source
   edge keeps the old token byte-for-byte, so every existing `TopoRef` and
   lineage token still resolves.

## 5. Edit / rebind rules

- Editing an entity re-derives the arrangement. A stored face that still
  derives with the identical canonical boundary is the same face — moving a
  circle's centre so it still crosses the same edges twice keeps the lens.
- If the boundary changes — an intersection appears or disappears, an ordinal
  shifts, a partner entity is deleted — the selection is **refused by name**
  (`PlanarFaceLost`), never retargeted, exactly as `ProfileRegionMismatch`
  refuses a changed hole set today. Edit Sketch is staged, so the user sees the
  refusal at Finish and re-picks; the committed body is untouched.
- An edit that would strip a fragment side face another body's sketch stands
  on is refused as `DependentFaceLost`, on the existing terms.
- A tap chooses exactly one atomic face; the pure toggle and the
  `OverlappingRegions` refusal become unnecessary for faces (faces never
  overlap), and stay for v5 loop-region selections.

## 6. `CADB` migration impact

| Version | Written when | Meaning |
| --- | --- | --- |
| v1–v5 | exactly as today | unchanged; read verbatim; never converted |
| **v6** | a body has a sketch table entry used by more than one feature (sketch identity) OR any feature whose selection is `PlanarFaces` | superset of v5 |

v6 changes, in one version:

- **Sketch table** (from `CAD-SKETCH-IDENTITY-R1`, audit D3): `u32
  sketchCount`, per sketch `u32 sketchId`, placement (workplane code + support
  block, or support feature), `u32 nextEntityId`, entities; features reference
  `u32 sketchId` instead of carrying a sketch inline. Plus the feature-id
  high-water mark (audit T9).
- **Selection kind** per feature: `u8 selectionKind` — `1` LoopRegions,
  followed by the unchanged v5 `REGIONS` block; `2` PlanarFaces, followed by
  `FACES`:

```
u32 faceCount                    1 .. kMaxProfileRegions (16)
repeat faceCount, canonical order:
  u32 cycleCount                 1 .. 1 + kMaxRegionHoles
  repeat cycleCount (outer first):
    u32 fragmentCount            3 .. bounded by the sketch caps
    repeat fragmentCount:
      u32 entityId
      u32 edgeLocalIndex
      CUT start, CUT end         u8 kind (0 SourceEnd, 1 Intersection)
                                 [kind 1: u32 partnerEntityId,
                                          u32 partnerEdgeIndex, u32 ordinal]
      u8  reversed               0 / 1
```

- **Lineage token** (`DATA_PACKAGE_SPEC.md` §7c) extended ONLY for fragment
  side faces: their token code folds in the two Cuts. A whole-edge side keeps
  its code, so every v2–v5 signature is unchanged.
- **Validation**: a non-canonical `FACES` block, an unknown kind, a reserved
  bit, a face that does not derive, overlapping faces or more than the caps is
  refused by name; load stays all-or-nothing.
- **Older builds** refuse v6 as a required section at an unknown version —
  the existing rule; nothing opens with a face silently missing.
- **Corpus**: new v6 fixtures (e.g. `cad_face_lens`, `cad_face_protrusion`,
  `cad_face_two_circles`, `cad_sketch_shared`, and refusals `cad_bad_face_ref`
  -style `cad_bad_planar_face`, `cad_bad_face_order`), constructed by
  `scripts/build-forge-corpus.ps1`, which must also reimplement the extended
  lineage rule from the spec text. All 44 existing fixtures byte-identical.

## 7. One v6, not two

Sketch identity and planar faces must share ONE `CADB` v6, because both
change what a feature's input is: sketch identity changes WHERE a feature's
sketch lives (a table, referenced by `sketchId`), planar faces change WHAT it
selects from that sketch. The arrangement is derived per SKETCH and shared by
all its consumers, which is exactly the audit's "regions are already
sketch-relative" point (`SKETCH_FEATURE_HISTORY_AUDIT.md` §5.5). Designing the
table first and bolting a selection kind on in a v7 would give two migrations
of the same record; designing both into v6 costs one `u8` now.

## 8. Smallest next slices

1. **PF-S1 — the derived arrangement, no format, no UI.**
   `forgeshape_sketch_arrangement.{h,cpp}`: analytic segment/circle/arc
   intersections, T-junction nodes, fragments, half-edge graph, bounded faces,
   canonical `PlanarFaceRef`. Pure and bounded. Native tests: PF-01..03 derive
   3 / 2 / 3 faces; the canonical refs are identical across two derivations
   and across an entity edit that keeps the crossings; a nested sketch's faces
   equal its v5 regions fragment-for-fragment (PF-04 and every `CADVS_REG_*`
   nesting case); performance at the sketch caps. No session, JNI, UI or codec
   change — nothing user-visible, so nothing to verify on a device.
2. **PF-S2 — v6 with sketch identity** (`CAD-SKETCH-IDENTITY-R1` merged with
   this): the sketch table, `selectionKind`, `FACES`, the extended lineage
   rule, the codec, the PowerShell encoder and the v6 fixtures; the session's
   tap/preview/commit switch to faces for a sketch that has any
   crossing/touching/T-junction and stay on loop regions otherwise.
3. **PF-S3 — authoring**: on-curve and intersection snapping so Case B can be
   drawn reliably by touch; the BEFORE checks flipped by name.

OWNER decision needed before PF-S2: approve `CADB` v6 as ONE combined
version (sketch table + selection kind), and whether splines may bound a face
in the first release.
