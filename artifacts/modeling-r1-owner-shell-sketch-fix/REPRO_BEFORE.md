# REPRO_BEFORE — the OWNER inner-area failure, reproduced before any fix

Program: `repro_before.cpp` (scratch, compiled against the host objects of
`scripts/host-native-selftests.sh` on the unmodified baseline `7e02240`; never
shipped). Full output: `repro_before_output.txt`.

## The sketch (OWNER-like topology, XY, metres)

Drawn through the product's own sketch session (a real drag per entity, then
typed exact), then Finish:

| entity | geometry |
| --- | --- |
| e1 rectangle | centre (0, 0), 6 × 4 |
| e2..e4 circles | r 1.0 at (-1.2, 0), (0, 0), (1.2, 0) — a chain of overlaps |
| e5 circle | r 0.8 at (0, 0.9) — crossing all three |
| e6 spline | (-3, -1) → (-1.5, 1.2) → (0.4, -1.3) → (1.8, 1.1) → (3, 0.6), ends ON the rectangle's sides |
| e7 rectangle | centre (1.6, -1.2), 2 × 1.2, crossing the outer bottom-right |

Finish: `Ok`, selection kind `PlanarFaces`, arrangement `Ok`, **23 atomic
faces**, 33 nodes, 55 fragments. The 23 face areas sum to **24.000000** m²
(the rectangle), i.e. the arrangement tiles exactly. 46 face pairs touch at a
node without sharing a fragment.

## The failing selections

Selecting ALL 23 faces merges (`Ok`, one component). Selecting every face
EXCEPT two inner faces that meet only at a node is refused: **45 of the 253
"all but two" selections** return `PinchedSelection`. Three, exactly:

| selection (21 faces) | groups | repeated node in the group's union boundary | `mergePlanarFaces` | session candidate (New Body) |
| --- | --- | --- | --- | --- |
| all but {0, 4} | 1 | node 14 at (-0.6000, -0.8000), passed twice | `PinchedSelection` | `PlanarFacesTouchAtPoint` |
| all but {0, 8} | 1 | node 17 at (-1.2019, 1.0000), passed twice | `PinchedSelection` | `PlanarFacesTouchAtPoint` |
| all but {0, 11} | 1 | node 24 at (0.0159, -0.9999), passed twice | `PinchedSelection` | `PlanarFacesTouchAtPoint` |

For each, every one of the 21 toggles returned `Ok`, `selectedAreaCount()` is
21 (no cell dropped) and the session's `lastStatus` is `Ok` — the refusal is
the CANDIDATE's alone, which is exactly the OWNER's SCREEN B message with the
selection standing. The exact `PlanarFaceRef` cycles of every selected face
(entity . edge-local, `r` = reversed) and every face's index, area and
interior point are in `repro_before_output.txt`.

## Root cause (proved, not assumed)

It is NOT a stale status and NOT two separate components meeting at a point
(those already merge as separate components since `CAD-V6-S2-CORRECTION-FILL-PICK-R2`).
All 21 faces are ONE edge-connected group (`partitionSelectedPlanarFacesBySharedBoundary`
returns one group). The two unselected faces are HOLES of that group's union
(or one hole touching the outer boundary), and they touch at a single node, so
the union boundary walk passes that node twice. `mergeEdgeConnectedFaces`
refuses on the first node revisit (`nodeUsed`), before the walk completes.

## Is it decomposable? Yes.

The walk already turns by the tightest-turn rule inside the union, so a loop
that revisits node P splits at P into simple sub-loops, each wholly on one
side: here one counter-clockwise outer and clockwise holes that touch each
other (or the outer) at P. Nothing is dropped (every boundary half-edge stays
in exactly one sub-loop), no area is duplicated (the sub-loops bound exactly
the same region), and shared-edge union semantics are unchanged (the same
fragments cancel). Each sub-loop already gets its OWN vertex ring in
`appendPrism`, so the two coincident points at P are two different 3D
vertices: the extruded solid is a closed, oriented 2-manifold that merely
touches itself at a vertical edge. The fix splits at revisited nodes instead of
refusing.

`CadMultiFaceOwnerTest` and `forgeshape_cad_multiface_selftest.cpp`
(`MF_15`, `MF_15B`, `MF_15C`) currently PIN the refusal of the 6 × 6 and
6 × 4 grid rings around an unchosen cell — the same topology — and are updated
to the corrected contract.
