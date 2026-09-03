# Triangulation

`triangulateSimplePolygon(polygon, &indices) -> CadStatus`, bounded ear clipping.

- Input: a counter-clockwise simple polygon, 3 <= n <= 256. Clockwise input is
  refused (`TriangulationFailed`) rather than reversed - orientation was decided
  once, upstream.
- Ear test: convex at `b` (cross > 0) and no OTHER remaining vertex inside the
  triangle, with a vertex ON the ear's boundary counting as inside, so a
  collinear vertex is never cut across.
- Bounded: each outer pass clips one ear or gives up; at most n - 2 clips, each
  a search over at most n candidates. O(n^2), no unbounded loop.
- Deterministic: the cursor resumes at the clipped position, so the same polygon
  yields the same triangles in the same order (`CADR0_22`).
- Output triangles keep the polygon's orientation; every one has positive area
  and they sum to the polygon's area (`CADR0_22`: the concave L gives 4
  triangles of total area 5).
- Circle tessellation: 32 segments, deterministic, exact cardinal points,
  independent of camera, zoom, GPU and device (`CADR0_23`). The radius is truth;
  the polygon is regenerated.

No geometry dependency was introduced. No holes are triangulated (nested
profiles are refused upstream).
