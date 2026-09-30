# CAD-PLANAR-FACE-PF-S1 — the planar arrangement engine

Result: **`PASS-CAD-PLANAR-FACE-PF-S1`** (pending the one CI FAST on the final
candidate, recorded in `PROJECT_STATUS.md`). Not user-visible by design.

## Refs

- Start: `origin/main = 3872120`, C2 branch `f751773`.
- **C2 integrated:** `main` fast-forwarded `3872120 → f751773` (this task's
  Phase 0; `cc99400..f751773` is docs/evidence only).
- PF-S1 branch `feature/cad-planar-face-pf-s1-arrangement-r1`, base `f751773`.

## What was built

`forgeshape_sketch_arrangement.{h,cpp}` — one portable module that reads a
`CadSketch` and returns derived values only. No JNI, Android, renderer,
session, feature-chain or codec dependency; nothing in the product calls it.

| Stage | Rule |
| --- | --- |
| Source edges | Line (0), polyline segment k (closing n-1 when closed), rectangle side 0..3 (`rectangleProfilePolygon` order), circle (0; angle from its own +u, CCW, closed), arc (0; `arcGeometry`, authored endpoints exact). Sorted by (entity id, local). Any spline → `UnsupportedCurve`. |
| Contacts | Analytic segment/segment, segment/round, round/round with sweep filtering; endpoint proximity at `kSketchCoincidenceMeters` (1 µm, pinned). Classified crossing / T-junction / endpoint coincidence; tangency counted and makes NO node; a shared stretch > 1 µm → `AmbiguousOverlap`. |
| Nodes | Every authored endpoint and contact point; clusters = connected components of "within 1 µm" (order-free); canonical order by smallest semantic key. |
| Fragments | Each edge cut at interior contacts; a cut is `Intersection(partner entity, partner edge, ordinal along this edge)`, the smallest name winning where curves meet; ends are `SourceStart`/`SourceEnd`; an uncut circle is one fragment origin → origin. |
| Half-edges | Two per fragment; per node sorted CCW by quantized tangent angle, then signed curvature, then semantic identity; `next` = immediately clockwise of the twin (face on the left). Dangling fragments and bridges pruned to a fixpoint. |
| Faces | Exact signed area (arcs by Green): positive cycle = bounded face, negative = component outline → hole of the smallest positive cycle of another component containing a sample on it (exact winding with arcs; golden-ratio samples); the unbounded face is never emitted. |
| Identity | `PlanarFaceRef` = outer CCW + holes CW, each rotated to its smallest `FragmentRef`, holes sorted. No coordinate, index, tessellation or triangle. `resolvePlanarFaceRef` = exact tuple equality; no fallback. |
| Caps | `kMaxArrangementSourceEdges` 1024, `kMaxArrangementContacts` 4096 → `CapExceeded`. Status enum module-local (does not extend `CadStatus`). |

## Test matrix (CAD_FEATURE suite, `PFS1_*`, 39 checks, all PASS)

Canonical refs of the four reference sketches: `REFERENCE_FACES.txt`.

| Case | Result |
| --- | --- |
| 01 circle crossing rectangle | 3 faces (11.607301, 0.392699 ×2); lens = rectangle-side fragment + circle fragment; no whole-loop alias; right side 3 fragments, circle 2 |
| 02 line protrusion | 2 T-junctions split side 1 into 3; 2 faces (12, 1); protrusion = reversed middle of side 1 + 3 lines; 2 dangling lines pruned |
| 03 two crossing circles | lens 1.585347 + two crescents 1.556246 |
| 04 nested | faces = the v5 regions exactly (outer loop, holes {A, B}), all whole-loop fragments; 10.429204 = 12 − 2·π/4 |
| 05 line/line; 05b three at a point | 1 node, 4 fragments, ordinal 0; three curves → one node, side cut once, named by the smaller partner |
| 06 segment/circle | cuts ordered (ordinal 0 then 1), two half-disks |
| 07 tangent segment; inscribed circle | no node, no bogus face; disk + rectangle-with-hole (4 − π) |
| 08 tangent circles (external, internal) | no lens |
| 09 arcs | segment/arc, circle/arc, arc/arc crossings filtered by sweep; arc + line = half-disk face |
| 10 tolerance | 0.5 µm splits once, 2 µm does not; endpoint on endpoint splits nothing; 1e-6 pinned |
| 11 input order | 6 permutations: identical nodes, fragments, faces |
| 12 repetition | 100 derivations tuple-identical |
| 13 stable edit | circle moved (2,0,r .5 → 2.1,0.3,r .6): all 3 refs resolve both ways |
| 14 topology loss | circle pulled clear: all 3 old refs lost, no fallback; crossing the top side instead: 0 of 3 resolve |
| 15 spline | `UnsupportedCurve`, nothing derived |
| 16 overlap | collinear overlap, identical circles, shared rectangle side, overlapping arcs → `AmbiguousOverlap`; end-to-end touch and two semicircles are fine |
| 17 caps | 64×64 grid = exactly 4096 contacts → 3969 faces, 20 runs identical; 65×65 → `CapExceeded`; 256 polylines × 256 vertices (largest legal sketch) → `CapExceeded` in < 1 ms; 256 rectangles = exactly 1024 edges → 256 faces |

## Performance (64×64 at-cap grid, 20 derivations)

- Optimized host (`-O2`, Xeon 2.8 GHz): median **22.2 ms**, max 27.8 ms and
  48.3 ms in two runs (noisy), min 20.8 ms.
- Host self-test build (`-O1`): median 24–29 ms, max 32–40 ms.
- Largest legal sketch refused in 0.44–0.57 ms.

Reported, not gated.

## Regression and guards

- `HOST_SELFTESTS_OK (3852 checks, 0 failed)`; baseline 3813; CAD_FEATURE
  184 → 223; every other suite's count unchanged.
- NDK `assembleDebug` and `assembleRelease` build locally; the self-test file
  is debug-only (`FORGESHAPE_SELFTEST_SOURCES`).
- `git diff f751773 -- DATA_PACKAGE_SPEC.md testdata scripts
  forgeshape_project_document.* forgeshape_jni.cpp app/src/main/java
  app/src/androidTest app/src/test` is EMPTY: no format, codec, fixture,
  encoder, JNI, Android or test-harness change.
- **CI DEVICE NOT RUN** (no JNI/Android/session integration).
  **FullSharded NOT RUN.**

## Not started

PF-S2 (`CADB` v6, sketch identity), session tapping, Extrude/Add/Cut wiring,
JNI, UI. The C2 OWNER APK is unchanged (CI FAST `36744733518`, 10,910,129
bytes, SHA-256 `81f5cd73195e4c59af11d89161a3881b30d9916ba8844fb693a148dcb4882b9e`).
