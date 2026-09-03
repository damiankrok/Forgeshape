# Performance - bounded, measured, reported

Measured by the native CAD self-test on each run, printed as
`FORGESHAPE_CAD_PERFORMANCE` on every debug launch, for the four required
sizes: a rectangle (4-gon), a circle (32-gon), a 32-edge closed polyline and a
128-edge closed polyline. Each figure is one call, in microseconds: closed
profile extraction, ear-clipping triangulation, and the whole regeneration
(`generateCadMesh`: validate + extract + triangulate + extrude).

## Standalone runner (x86_64 emulator, `-O1`, warm, three runs)

| Case | extract us | triangulate us | regenerate us | vertices | indices |
| --- | ---: | ---: | ---: | ---: | ---: |
| rectangle | 12-27 | 8-9 | 10-19 | 8 | 36 |
| circle32 | 12-22 | 10-19 | 15-16 | 64 | 372 |
| polyline32 | 12 | 10 | 14 | 64 | 372 |
| polyline128 | 54-55 | 24-34 | 84-87 | 256 | 1524 |

## In-app debug launch (x86_64 emulator, first call of the process, cold)

```
FORGESHAPE_CAD_PERFORMANCE rectangle extractUs=133.5 triangulateUs=17.0 regenerateUs=27.6 v=8 i=36;
  circle32 extractUs=117.1 triangulateUs=250.7 regenerateUs=7824.7 v=64 i=372;
  polyline32 extractUs=351.1 triangulateUs=24.1 regenerateUs=331.4 v=64 i=372;
  polyline128 extractUs=256.0 triangulateUs=321.0 regenerateUs=664.6 v=256 i=1524;
```

The in-app numbers are taken on the very first regeneration the process ever
runs, during start-up, beside seventeen other suites; the one outlier
(circle32 regenerate at 7.8 ms) is a cold-cache first call and does not recur
in the warm standalone runs of the same code. Every figure is well inside a
frame, and none is on the pointer-move path: a move updates a few doubles and
bumps an overlay revision, extraction runs at Finish, and regeneration runs at
commit and at an Apply.

## Bounds that make the loops finite

- profile extraction: line chaining is O(L^2) over L <= 256 lines; the crossing
  test is O(n^2) over n <= 256 loop vertices; nesting is O(P^2 * n^2) over the
  profiles found.
- triangulation: at most n - 2 clips, each a scan of at most n candidates; a
  pass that clips nothing returns `TriangulationFailed` rather than looping.
- extrusion: linear in n.
- overlay: the grid is a fixed 65 x 65 line rhythm (the 8 m half extent at
  0.25 m) plus 2 vertices per entity edge; `kMaxSketchOverlayVertices` (65536)
  is the renderer's ceiling and the session cannot reach it within its own
  entity and vertex caps.

## Save/load impact

`CADB` records are tens of bytes per body plus 29-37 bytes per entity (see
`CORPUS.md`: 247 bytes for a one-rectangle project, 780 for a Construction
body beside two CAD bodies). Load regenerates each CAD Body once through the
same path measured above.
