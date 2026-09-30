# Profile / region selection audit (FABLE-CAD-ARCHITECTURE-AUDIT-R1)

Read-only. Baseline `main = 509de02`. Paths: `cpp/` = `app/src/main/cpp`.
Evidence tags: `SOURCE_CONFIRMED`, `TEST_CONFIRMED` (READ, not run),
`DEVICE_EVIDENCE`, `DOC_ONLY`, `INFERRED`, `UNVERIFIED`.

Answers OWNER observation **O2** and required conclusion **C3**.

## 1. The chain, sketch entity → persisted selection

| Step | File / function | Data | Owner | Notes |
| --- | --- | --- | --- | --- |
| Entities | `cpp/forgeshape_sketch.h:322-425` (`SketchEntity`, kinds Line/Polyline/Rectangle/Circle/Arc/Spline), `CadSketch` `:478-496` | `SketchEntityId` per entity, monotonic, never reused (`nextEntityId`) | authored truth | SOURCE_CONFIRMED |
| Closed loops | `extractClosedProfiles`, `cpp/forgeshape_sketch.cpp:1076-1149` | `ProfileExtraction { profiles (anchor-ascending), rejections }`; `ClosedProfile` carries CCW polygon, area, per-edge `(edgeEntityId, edgeLocalIndex, edgeCurved)` | derived | Refusals by name: `OpenProfile` `:1088-1101, :996-1002`; `BranchingChain` `:992-998`; `DuplicateEdge` `:799-803`; `SelfIntersectingProfile` `:808-819` (collinear touch counts); `ZeroAreaProfile` `:820-823`. Nesting is NOT decided here (`sketch.h:586-592`); `NestedProfileUnsupported` is retained for ABI and never produced. |
| The anchor | `sketch.cpp:1081` (own id for rect/circle/polyline), `:987` (first chained member for a Line/Arc/Spline chain) | a `SketchEntityId`, never a point or index | derived | `INFERRED`: the header says "smallest member id" (`sketch.h:531`); the code takes the first-in-storage member, which is the smallest only while entity storage is id-ascending. `CADVS_REG_22` permutes single-entity loops only; the chain case is UNVERIFIED. |
| Regions | `extractSketchRegions`, `cpp/forgeshape_sketch_region.cpp:170-260` | `SketchRegionExtraction { loops, regions (one per loop), parent[], conflict[n×n], contains[n×n] }` | derived | "Cleanly contains" = no edge of either loop touches or crosses (`loopsTouch`, `:23-48`) AND the first vertex is strictly inside by even-odd (`:184-195`). Parent = smallest-area clean container (`:198-213`). Region(L) = L minus its DIRECT children (`:216-229`). `OverlappingHoles` when two direct children conflict (`:232-239`); `kMaxRegionHoles` 64. |
| Semantic identity | `ProfileRegionRef { outerAnchorId, holeAnchorIds }`, `region.h:66-71`; `sketchRegionRef` `:272-274` | entity ids only | stored form | SOURCE_CONFIRMED |
| Selected set (session) | `SketchSession`, `cpp/forgeshape_sketch_session.cpp`: `finish` `:1215-1235`, `reconcileRegionSelection` `:1237-1261`, `selectProfile` `:1268-1282` (panel row, REPLACES), `toggleRegion` `:1294-1333` (tap), `toggleRegionAt` `:1336-1350` | lives in `extrude_` (`ExtrudeFeature`), spread over `profileEntityId` + `profileHoleIds` (first region) + `additionalRegions` | session | see §3 for the tap rule |
| Validation | `validateRegionSelection`, `region.cpp:276-349` | the ONE rule for a stored selection | derived | quoted in §2 |
| Durable truth | `ExtrudeFeature`, `cpp/forgeshape_cad_body.h:123-157`; per feature (`CadFeature::extrude`) | region list lives in the EXTRUSION, never in the sketch | authored truth | SOURCE_CONFIRMED |
| Triangulation | `appendCadFeatureSolid`, `cpp/forgeshape_cad_feature.cpp:305-314`: `polys.size()==1 ? triangulateSimplePolygon (sketch.cpp:1165-1231, own ear clipper) : cadKernelTriangulateRegion (cad_kernel.cpp:270-353, Manifold TriangulateIdx, result checked for index range, winding, area == outer − holes)` | per region | derived | no bridging in ForgeShape |
| Boolean candidate | `appendCadFeatureSolid` `:290-374`: ONE PRISM PER SELECTED REGION, concatenated with a fresh vertex block (`:317`); `regenerateCadBody` `cad_body.cpp:586-729`: the base is `sortSolidByTag` only when there is no later feature (`:717-719`), `components = chosen.size()` (`:650`); later features through ONE `cadKernelBoolean` (`:683`) | | derived | **No 2D union of regions exists anywhere and no 3D union between the regions of one feature** (grep: the only Manifold entry points are `Manifold::Boolean` and `TriangulateIdx`). A single simple region takes the unchanged R0 float path (`:619`, `generateSimpleProfileMesh` `:452-528`; `TEST_CONFIRMED` bit-identical `CADVS_EXT_12/14/15`). |
| Persistence | `CADB` v5, `cpp/forgeshape_project_document.cpp`: `cadDocumentNeedsV5` `:366-374`, `writeCadRegions` `:974-986`, `readCadRegions` `:1637-1682` (structural only), load → `validateCadBodyState` `:723` | `u32 holeCount, holes…, u32 additionalCount, per region {u32 outer, u32 holeCount, holes…}` | `.forge` | `DATA_PACKAGE_SPEC.md` §7f `:774-784, :810-836` agrees with the code. |

## 2. The whole selection rule, quoted (`SOURCE_CONFIRMED`, `region.cpp:276-349`)

```
selection.empty()                       → regions.size() > 1 ? AmbiguousProfile : ProfileNotFound
selection.size() > 16                   → TooManyRegions
per ref i:
  outer anchors not strictly ascending  → ProfileRegionMismatch
  > 64 holes                            → TooManyRegions
  holes not ascending-unique            → ProfileRegionMismatch
  no loop with that anchor              → ProfileNotFound
  region->holeAnchorIds != ref.holes    → ProfileRegionMismatch     (:302-304)
  region->status != Ok                  → OverlappingHoles / TooManyRegions
per chosen pair (i<j), a = outerLoop(i), b = outerLoop(j):
  loopsConflict(a, b)                                → OverlappingRegions   (:326-328)
  loopContains(a, b) && !insideAHole(chosen[i], b)   → OverlappingRegions   (:329-331)
  loopContains(b, a) && !insideAHole(chosen[j], a)   → OverlappingRegions   (:332-334)
  a hole of i conflicts with b, or of j with a       → OverlappingRegions   (:336-345)

insideAHole(container, loop):  any hole h of container with h != loop && loopContains(h, loop)
```

"Share a loop" is not a literal comparison. It falls out of `insideAHole`
skipping `h == loop`: when region j's outer loop IS one of region i's holes,
`loopContains(a, b)` is true and no OTHER hole cleanly contains `b`, so the
pair is refused at `:329-331`. The comment at `:310-313` states the intent:
"a region and its own hole share the hole's loop as a boundary".

## 3. The rectangle + two circles case, exactly

Sketch: axis-aligned rectangle R, circles A and B strictly inside it, A and B
disjoint. Ids as drawn: R = 1, A = 2, B = 3. This is EXACTLY the sketch of
`CADVS_REG_19` (`cpp/forgeshape_cad_feature_selftest.cpp:993-996`: rect 4×3,
circles r = 0.4 at (−1, 0) and (1, 0)) and `CADVS_REG_22` (`:1041-1050`).

### 3.1 Atomic regions produced today (`SOURCE_CONFIRMED`, `TEST_CONFIRMED` `CADVS_REG_19/22`)

`conflict` all zero; `contains[R][A] = contains[R][B] = 1`; `parent[A] =
parent[B] = R`.

| Region (outer anchor) | `holeAnchorIds` | status | area |
| --- | --- | --- | --- |
| R = 1 | `{2, 3}` | Ok | rect − 2·disk |
| A = 2 | `{}` | Ok | disk |
| B = 3 | `{}` | Ok | disk |

There is no fourth region. "Rectangle with only hole B" and "R ∪ A" do not
exist as regions because a region is one per loop by construction
(`region.cpp:215-216`).

### 3.2 Legality of each selection (through `validateRegionSelection`)

| Selection (canonical) | Verdict | Deciding line | Evidence |
| --- | --- | --- | --- |
| rect material only `[{1,{2,3}}]` | LEGAL | passes `:302`, `:305`; no pairs | `TEST_CONFIRMED` `CADVS_REG_19` `:1015`; `DEVICE_EVIDENCE` `OWNER_FINDINGS.md` F5 (ring 9.2023 m² × depth) |
| disk A only `[{2,{}}]` | LEGAL | same | `TEST_CONFIRMED` for one circle (`CADVS_REG_06/16`); INFERRED for two |
| disk B only `[{3,{}}]` | LEGAL | same | INFERRED |
| disk A + disk B `[{2,{}},{3,{}}]` | LEGAL | neither contains the other; no conflict | INFERRED; **no test pins A+B with R present but unselected** |
| **rect material + disk A** `[{1,{2,3}},{2,{}}]` | **REFUSED `OverlappingRegions`** | `loopContains(R, A)` true and `insideAHole(regionR, A)` false (A is R's own hole; B does not contain A) → `:329-331` | `TEST_CONFIRMED` one-circle analogue `CADVS_REG_06` (`:824-828`), nested `CADVS_REG_10` (`:861-868`); INFERRED for two |
| rect material + disk B | REFUSED `OverlappingRegions` | symmetric | INFERRED |
| all three | REFUSED `OverlappingRegions` | the first refusing pair (R, A) | INFERRED |

### 3.3 What the user actually experiences (the real reason O2 exists)

`OverlappingRegions` is **unreachable through the viewport tap path**
(`SOURCE_CONFIRMED`, `sketch_session.cpp:1318-1327`): `toggleRegion` keeps a
currently selected region ONLY if `validateRegionSelection({ref, new})` is Ok,
and otherwise **silently drops it** ("a region it overlaps, touches or shares a
loop with is replaced by it"). So with the ring selected, a tap on disk A does
not refuse: it deselects the ring and selects the disk. The OWNER sees the
selection "jump", never a reason. `TEST_CONFIRMED` `CADVS_SES_05` and the
device test `owner_rectangle_circle_region` (`CadVerticalSliceTest.java:118`)
assert exactly this replacement. The refusal is reachable only from a loaded
`.forge` (`TEST_CONFIRMED` fixture `cad_bad_region_v5`, `CADVS_REG_18`), a
hand-built `applyState`, or the self-tests.

Two further facts about the current model:

- **Auto-select is stricter in code than in the docs**: `selectable == 1u &&
  regions_.regions.size() == 1u` (`:1256`) — exactly one region in total, not
  "exactly one selectable region" as `PROFILE_REGIONS.md` §4, `CLAUDE.md` and
  `sketch_session.h:606` say. A sketch with one Ok region and one
  `OverlappingHoles` region selects nothing. Untested edge; `DOC_ONLY` drift.
- **The hit rule**: `sketchRegionAt` (`region.cpp:351-370`) names the innermost
  loop that strictly contains the point, so a tap inside hole A resolves to
  disk A, never to the ring (INFERRED from the predicate). That is the right
  atomic pick; it is the drop rule above that makes it feel wrong.

### 3.4 Is "rect with hole B only" representable today?

**Yes in bytes, no in semantics.** `[{1,{3}}]` is byte-representable and reads
back structurally; it is forbidden purely by `region->holeAnchorIds !=
ref.holeAnchorIds → ProfileRegionMismatch` (`:302-304`), reached at load via
`project_document.cpp:723`. `TEST_CONFIRMED` `CADVS_REG_18` and fixture
`cad_bad_region_v5`. That check is correct for what it guards (a loop was
added or removed inside a selected region) and must stay; the answer is not to
weaken it (§5).

### 3.5 What would happen if rect + A were simply allowed today

`INFERRED`: `appendCadFeatureSolid` would emit two vertex-disjoint prisms
whose walls on circle A coincide with opposite orientation (the ring's inward
hole wall and the disk's outward wall). With no later feature the body is not
run through the kernel (`cad_body.cpp:717-719`), so the render mesh would
carry an internal double wall (z-fighting, wrong volume by concatenation is
fine but `components` would read 2). With a later feature, `cadKernelValidateSolid`
on a two-shell solid with coincident faces is `UNVERIFIED` (Manifold may merge
or may reject). Either way, allowing the pair without merging first is the
wrong fix.

## 4. What professional CAD does with the same sketch (from `CAD_BENCHMARK.md`)

- **Fusion**: a sketch with a loop inside a loop shows TWO blue-shaded
  profiles (ring and disk); one Extrude accepts "one or more coplanar sketch
  profiles"; adjacent profiles extruded together become ONE body (the shared
  edge disappears — forum-only, NON-AUTHORITATIVE, consistent with the Join
  operation). Holes are never a separate user concept.
- **Inventor**: "If there is only one profile in the sketch, it is selected
  automatically. Otherwise, select a sketch profile"; window-select for
  multiple; the Profile selector distinguishes REGION (hole excluded) from
  LOOP (hole filled) via "Select Other", and names "Nested"; profile
  reselection inside Edit Feature is documented.
- **AutoCAD**: PRESSPULL picks a bounded area by tapping inside it; BOUNDARY
  and REGION make holes an explicit, opt-in act (island detection, SUBTRACT);
  the one product whose hole semantics are implicit (PRESSPULL islands) has
  "sometimes subtracts, sometimes not" community reports.
- **Shapr3D / Onshape / FreeCAD**: see `CAD_BENCHMARK.md` §2; the pattern is
  the same: tap inside a closed area, several areas per feature, the result is
  one solid.

Every reference lets the user select the ring AND the disk in one feature and
gets a rectangle-with-one-hole solid. None of them stores "ring ∪ disk" as an
identity; they store the two selected areas and derive the merged boundary.

## 5. The cleanest semantic fix (C3)

### 5.1 The abstraction

Keep the nesting model and the atomic regions exactly as they are. Change ONE
sentence: **a selection is any set of atomic regions, and what is extruded is
their union**. Because nesting-only regions are pairwise DISJOINT by
construction (each is a loop minus its direct children, and touching or
crossing loops are never holes), the union of any subset needs no 2D boolean
library. It is a parity rule over loops:

```
loop L bounds the union  ⇔  selected(region(L)) XOR selected(region(parent(L)))
                            (with "outside" = the unselected background when L has no parent)
```

For R + A: R bounds (R selected, outside not), A does not (A and R both
selected), B bounds (B not selected, R selected). The extruded area is
"rectangle with hole B", derived, with the same even-odd triangulation the
kernel path already uses. For A + B with R unselected it is two disks (today's
result). For all three it is the plain rectangle.

This is a `DERIVED` step (`forgeshape_sketch_region` or `forgeshape_cad_feature`):
selected atomic regions → merged boundary loops (outer + holes, still named by
their loop anchors) → one prism per merged component. Identity never becomes a
triangle; a merged component's outer loop is a real authored loop and so are
its holes, so face tokens stay `(edgeEntityId, edgeLocalIndex)` and the lineage
signature stays the §7c rule over real loops.

### 5.2 What changes and what does not

| Item | Today | After |
| --- | --- | --- |
| `ProfileRegionRef { outer, holes }` and the stored list | as is | **unchanged**. The selection is still the list of chosen atomic regions, each with the holes it was chosen with, so `ProfileRegionMismatch` keeps guarding "a loop appeared or vanished inside what I chose". |
| `validateRegionSelection` | refuses a region beside its own hole (`:329-334`) | drop ONLY the "share a loop" refusal: a selected region whose outer loop is a hole of another selected region is legal. Keep every touch/cross refusal (`:326-328, :336-345`) — those are the cases no rule can merge without guessing. |
| `toggleRegion` | drops conflicting regions silently (`:1318-1327`) | toggle the atomic region under the finger and nothing else; a tap can no longer change a region the user did not touch. The touch/cross case cannot arise from a tap (such loops are never both regions of one nesting tree without conflict, and a conflicting region already has status ≠ Ok). |
| Geometry | one prism per region, concatenated | merge selected regions into boundary components by the parity rule, then one prism per component. Disjoint components still concatenate. |
| Hatch / preview | per region | per merged component (a hole stays empty, an absorbed hole is hatched). |
| Face table and signature | per region loops | per merged component loops (the same tokens, fewer of them: A's side faces vanish when A is absorbed). A dependent standing on A's side face would be refused by lineage — correct, the face is gone. |
| Precision surface rows | one row per region with hole count | unchanged; a row is a toggle. |
| `CADB` | v5 | **no byte changes**: `[{1,{2,3}},{2,{}}]` is already encodable and is the ONE encoding of that solid (there is no atomic "R minus B" to encode it a second way). An older build refuses such a file by name (`OverlappingRegions`), which is fail-closed. If the OWNER wants an explicit "unknown version" refusal in older builds instead of a semantic one, bump to v6 with no layout change; that is a taste decision, not a correctness one. |
| Two-feature reuse of one sketch ("extrude circle B later at another depth") | impossible: a sketch belongs to one feature | a SKETCH identity (`SKETCH_FEATURE_HISTORY_AUDIT.md` §5); orthogonal to this fix, and this fix does not make it harder because regions stay sketch-relative. |

### 5.3 Why not the alternatives

- **Independent selectable planar faces (a planar arrangement)**: would
  split touching and crossing loops into faces. That is what AutoCAD's
  bounded areas do, and it changes what every v1..v5 record means (the header
  at `region.h:37-50` says why nesting was chosen). Not needed for O2, and it
  needs a robust 2D arrangement — a second geometric kernel problem.
- **Storing the merged boundary** (`[{1,{3}}]`): rejected in §3.4 for a good
  reason — the stored holes are how an edit under the selection is detected.
  Storing the merge would also make two encodings of one solid whenever a
  merged and an unmerged selection produce the same area.
- **Letting the 3D kernel union the prisms**: works only when a later
  feature exists, produces coincident-face inputs the kernel is not promised
  to like, and puts a 2D question into the 3D kernel.

### 5.4 Cost and risk

Bounded: one derived function (parity merge), one predicate removed, one
session rule simplified, hatch and face-table loops iterate components instead
of regions. Tests: `CADVS_REG_06/10` flip from refused to legal and need their
expected merged area; `owner_rectangle_circle_region` needs the toggle semantic;
new checks for R+A → one component with one hole, and all three → one
component with no hole. No fixture byte changes; `cad_bad_region_v5` stays a
refusal (its holes do not match).

## 6. Documentary drift in this area (`SOURCE_CONFIRMED`)

- `sketch_session.h:394` still says "profile" where the slot carries the
  REGION count (`SKETCH_PROFILE_COUNT`, `forgeshape_jni.cpp:4249`).
- Auto-select wording (§3.3) in `PROFILE_REGIONS.md` §4, `CLAUDE.md`,
  `sketch_session.h:606` is looser than the code.
- `region.h:30` "smallest member id" holds only for id-ascending storage.
- Duplicated helpers: `orientation` / `onSegment` exist in both
  `region.cpp:9-19` and `sketch.cpp:137-149`; `pointStrictlyInside`
  (`sketch.cpp:151`) is dead (already noted by FUNCTION-COUNCIL-R1); `(void)
  loops` dead local at `feature.cpp:371`.
- The PowerShell corpus builder reimplements only the lineage signature and
  hand-authors hole lists; it never derives regions. Intended (it is the
  independent ENCODER, not a second geometry engine).

## 7. Tests that pin this area (READ)

`CADVS_REG_06/08/09/10/15/16/17/18/19/22`, `CADVS_EXT_16`, `CADVS_SES_02–07`,
`CADR0_21`; device `owner_rectangle_circle_region`. Gaps: no test selects A + B
with R present and unselected; no test exercises the "one Ok region beside one
`OverlappingHoles` region" auto-select edge; `SketchUxTest` and the sketch-UX
self-test have no region cases.
