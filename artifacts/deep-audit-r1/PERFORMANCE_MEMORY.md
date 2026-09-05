# Performance and memory — DEEP-AUDIT-R1 (Part L)

Measurements are the device self-test prints from the `d783d4b` debug launch on `emulator-5580` (`ForgeShape_Stage006`), plus reasoned bounds where no instrument exists. No new instrumentation was added.

## Measured (device launch)

| Metric | Value |
| --- | --- |
| Startup: `ACTIVITY_CREATE` → `FORGESHAPE_NATIVE_VIEWPORT_OK` | ~6 s (20 debug suites, 2981 checks, run once on the UI thread; release skips them) |
| `FORGESHAPE_CAD_PERFORMANCE` | rectangle extract 32 µs / triangulate 18.5 / regen 17.1 (8:36); polyline128 extract 359 / triangulate 184 / regen 754 (256:1524) |
| `FORGESHAPE_CAD_A3_PERFORMANCE` (face resolve) | chain depth 1 = 45 µs; depth 8 = 441 µs; **depth 32 = 4.35 ms** |
| `FORGESHAPE_SKETCH_UX_PERFORMANCE` | arc_tess 4.2 µs, spline_tess 2.1 µs, curve_profile 23 µs, curve_regen 51 µs (per unit) |
| Sculpt rebuild (shading cost record) | 0.56–0.73 ms per accepted move at 482 v; 2 rebuilds over 4448 frames |

## Reasoned bounds

* **F-07 (P2): face-supported world placement re-extracts per frame under `g_stateMutex`.** `resolveWorldModel` runs `cadTopologySignature` + `resolveCadFace` (full `enumerateCadFaces` / profile extraction) for every face-supported body, up the producer chain, every frame the snapshot is taken. Cost scales with chain depth (4.35 ms at depth 32). A realistic 1–3-deep chain is sub-millisecond and invisible; a pathological deep chain would be felt and holds the state lock while felt. Fix worth its own stage: cache the resolved face frame + signature and invalidate on producer edit.
* **F-06 (P2): sculpt publication is whole-mesh per move** (O(vertices)); Inflate normals O(triangles); the affected set and picking are linear scans. Free at 482–514 v (1170 uploads, no stall); a 500 k-vertex stress mesh (debug driver) is the case a partial/async path must beat. The affected-set and pick scans both want the same missing spatial acceleration.
* **F-09 (P2): CAD triangulation O(n³), nesting O(P²·n·m)** on a hostile `.forge` — bounded (256 profiles, 1024 verts), ~1 s worst case at load, no crash.

## Memory

* Sculpt history ≤ 4 MiB per body (undo+redo, enforced on `record` and every eviction); realistic bound `(sculpted bodies)·4 MiB` + detached bodies held by the ≤64-step Construction history. No global cap added (no proven P0/P1) — see `SCULPT_HISTORY.md`.
* Construction history: 64 snapshots, ~20 doubles/body each, plus whole `SceneObject`s only for steps that created/deleted a body (`holdDetachedBody`).
* GPU: two `VkDeviceMemory` allocations per body; `releaseBodiesAbsentFromScene` prevents the add/delete-loop leak (bounded by the live body count, not the session's history of ids).
* No leak found in the Java layer (autosave thread released per Activity; no static context held; `Diagnostics` ring is capped at 200 records / 64 KiB rendered).

## Not measured (NOT AVAILABLE)

Frame-time instrumentation, GPU memory counters and allocation tracking are not built into the product and were not added (the brief forbids turning this into a benchmarking project). The startup-to-first-frame figure is from log timestamps, not a profiler.
