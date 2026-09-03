# Profile extraction

`extractClosedProfiles(const CadSketch&) -> ProfileExtraction { profiles[], rejections[] }`

## Candidates

| Source | Rule |
| --- | --- |
| Rectangle | one loop: its four corners, CCW from `(-w/2, -h/2)` |
| Circle | one loop: 32 vertices starting at +U, exact cardinal points |
| Polyline | one loop when `closed`, or when >= 4 vertices and the last coincides with the first (the repeated vertex is dropped); otherwise rejected `OpenProfile` |
| Lines | chained by coincident endpoints (1 um). Per connected component: any node with degree > 2 -> `BranchingChain`; any node with degree < 2 -> `OpenProfile`; otherwise the walk from the smallest-id line visits every line once and yields one loop, anchored by that smallest id |

## Loop rules, in order

1. `TooFewVertices` - fewer than 3
2. `DuplicateEdge` - consecutive coincident vertices (closing edge included)
3. `SelfIntersectingProfile` - any two NON-adjacent edges cross or touch
   (O(n^2), n <= 256). Checked BEFORE area, because a bow tie's lobes cancel to
   zero area and "it crosses itself" is the reason the user can act on
4. `ZeroAreaProfile` - |area| < 1e-12 m^2
5. Orientation normalised to counter-clockwise

## Nesting

A profile that contains another (a vertex of B strictly inside A and no edge of
B crossing an edge of A) is refused as `NestedProfileUnsupported`; the inner one
stays extrudable on its own. Two profiles that merely overlap are both kept -
each is a simple solid. No boolean interpretation of multiple loops exists.

## Determinism

Profiles are sorted ascending by anchor entity id; rejections likewise. The
anchor is the identity a `.forge` file stores (`profileEntityId`), so deleting an
unrelated entity cannot change which profile a body was made from.

## Multiple profiles

The session leaves the choice open when there is more than one
(`selectedProfileId == 0`), `commit` refuses `AmbiguousProfile`, and the
precision surface lists the profiles for the user to pick. With exactly one it
is chosen automatically.

Proved by `CADR0-15..21` (native) and `E2E-CADR0-13/14` (device).
