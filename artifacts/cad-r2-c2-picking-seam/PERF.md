# CAD-R2-C2 — picking hot path, baseline vs candidate

## Protocol

| Item | Value |
| --- | --- |
| A (baseline) | C1 head `08406cf` (isolated worktree), product objects only |
| B (candidate) | this branch with the double-precision `intersectRayTriangle` (`0fa12c9`), product objects only |
| Changed object | only `forgeshape_picking.o` (`.text` differs); `forgeshape_selection.o`, `forgeshape_sculpt.o` byte-identical |
| Compiler / flags | g++ 13.3.0, `-std=c++17 -O1 -g0` (the host script's), same machine (Xeon family 6 model 85, 4 cores) |
| Harness | `perf/bench_pick.cpp`, one source for both, linked without any `*_selftest.o` (the layout C1 proved comparable) |
| Binaries | A `ff7549cd61fb388aac7ce661bfbbe302d56f3d5c7678e9c0d9a156e6ce05a458`, B `864a1ca0efbb67debfa4e43c5fa415273ca10860f03f4062bc9d39a473d57d5f` |
| Design | 6 ABBA/BAAB blocks, 12 processes per side, 5 warm-up + 30 recorded samples each = **360 samples per side**; a sample = mean µs per pick over 20 picks (200 for the scene) |
| Load | 1-min load 1.37–1.65 on both sides (`/proc/loadavg` per process) |
| Raw | `PERF_RAW.tsv` (720 rows); runner `perf/run_bench_abba.sh`; stats `perf/analyse.py` |

Workloads (sanity-checked on both trees: interior / seam / far seam / scene
hit, miss misses): `pickTriangleMesh` on a 100 × 100 quad grid (20,000
triangles) — a ray through a cell interior, a ray exactly on a shared diagonal,
a ray beside the mesh (every triangle tested), the same seam from a 250 m
origin; and `pickSceneSnapshot` over 8 CAD bodies stacked along the ray.

## Result

| metric | median A | p10..p90 A | median B | p10..p90 B | Δ median | Δ % | MWU p |
|---|---|---|---|---|---|---|---|
| interior_us | 771 | 742..1021 | 794 | 758..1165 | +23 | **+2.9%** | 1.2e-09 |
| seam_us | 768 | 743..1030 | 788 | 759..1191 | +20 | +2.6% | 1.8e-10 |
| miss_us | 769 | 741..1047 | 788 | 757..1154 | +19 | +2.4% | 4.6e-08 |
| far_seam_us | 767 | 742..1001 | 790 | 759..1166 | +23 | +3.0% | 2.7e-10 |
| scene8_us | 4 | 4..6 | 4 | 4..7 | −0 | −0.2% | 0.60 |

**Disposition: a real, bounded cost of +2.4 % to +3.0 % per triangle test**
(≈ 1.1 ns of ≈ 38 ns per triangle), within the ≤ 5 % target for normal
interior picking; the CAD scene pick (8 small bodies) shows none. A pick is
on demand (a tap, a sculpt stroke sample, a chooser hover), never per frame,
and allocates nothing new.

## Caveats

- Host x86_64 numbers. The arm64 device has native double FMA at the same
  scalar throughput class, but no device timing was taken here; physical-phone
  performance stays an OWNER check.
- Sculpt picks a frozen mesh per stroke sample through `pickTriangleMesh`; on
  a large sculpt mesh the same ~3 % applies to that one ray test per sample.
