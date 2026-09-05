# Findings — DEEP-AUDIT-R1

Severity: P0 loss/corruption or crash on a product path · P1 latent correctness or a crash on a reachable-by-API path · P2 performance / robustness / concurrency worth fixing · P3 hygiene / debt / documentation.

Behavior changes were made only for proven P1 defects that are local and non-scope-expanding (F-01, F-02, F-03). Everything P2/P3 is reported, not implemented.

## Table

| ID | Sev | Area | Finding | Reachable | Status |
| --- | --- | --- | --- | --- | --- |
| F-01 | P1 | CAD / persistence | `projectSemanticFingerprint`'s `mixCad` mixed no Arc or Spline payload, so an in-place curve edit keeping the entity id did not move the fingerprint autosave and the dirty guard trust. | API (`replaceEntity`); not UI (redraw changes `nextEntityId`) | **FIXED** + `DAR1_01` |
| F-02 | P1 | Sculpt / persistence | A sculpt stroke in flight at `loadProjectDocument` was recorded against the LOADED body's history after the session rebound, so an Undo wrote the destroyed body's positions into the loaded mesh. | API; not UI | **FIXED** + `DAR1_02` |
| F-03 | P1 | JNI / import | `sceneBodyName`, `glbRoundtripReport`, `glbCompareReport` handed `NewStringUTF` a sanitized name that may legally carry a 4-byte UTF-8 sequence — illegal Modified UTF-8, CheckJNI aborts a debuggable process. | product (import a GLB whose node name contains an emoji) in a debuggable build | **FIXED** + `DAR1_03` |
| F-07 | P2 | Threading / perf | Per-frame `resolveWorldModel` re-runs full profile extraction (`cadTopologySignature` + `resolveCadFace`) up the producer chain for every face-supported CAD body, under `g_stateMutex`. Depth 32 = 4.35 ms/resolve. | product (deep CAD dependency chains) | reported |
| F-08 | P2 | Threading | Seven unlocked JNI readers call `sculptSession()`, which writes the borrowed `target_` pointer; the autosave thread writes it under the lock — a formal data race (benign: same value, aligned store). | concurrent | reported |
| F-06 | P2 | Sculpt | Whole-mesh republish per accepted sculpt move (O(vertices)); Inflate normals O(triangles); affected-set and picking are linear scans. Free at 482–514 v; the baseline a partial path must beat. | product (very dense meshes) | reported (existing debt) |
| F-09 | P2 | CAD / perf | Nested-profile containment check is O(P²·n·m); ear-clipping `isEar` is O(n) so triangulation is O(n³). Bounded (256 profiles, 1024 verts) but a hostile CADB file can cost ~1 s at load. | crafted `.forge` | reported |
| F-16 | P2 | Build / release | The 20 self-test translation units are on the CMake source list unconditionally, so their code and their internal string literals link into the **release** `.so` (`runProjectSelfTests` is an exported dynamic symbol; the `FSR1A_01`/`CADUXR1_25`/`DAR1_0*` check-name strings are present in `app-release-unsigned.apk`). Only the CALL SITE is `#ifndef NDEBUG`-guarded, so the suites never run in release — but they are not dropped. Confirms the item PROJECT_STATUS listed as unverified ("proven debug-guarded, not proven absent from a release binary"). Release `.so` is ≈ 500 KB larger than needed and exports the entry points. | n/a (dead in release, never called) | reported |
| F-10 | P3 | Scene | `activeBody()` self-heals to `bodies_.front()` when `activeBodyId_` names no body — silent, unreachable in the product (every mutation keeps the id valid). A debug assert would surface a future bug instead of masking it. | none (defensive) | reported |
| F-11 | P3 | JNI | `touchEvent` makes three `Get*ArrayRegion` calls before its one `ExceptionCheck`; JNI forbids JNI calls with a pending exception. Java always passes length-6 arrays, so unreachable. | none | reported |
| F-13 | P3 | Architecture | Five representation `if/else` chains (export, roundtrip, publish, sculpt-source, capture+fingerprint) that ARCH-HEALTH-01 said the third representation should unify; the third has arrived and they were not unified. | n/a | reported |
| F-14 | P3 | Architecture | `forgeshape_jni.cpp` is 5 870 lines and is the single meeting point of arbitration, publication and lifecycle — coherent but the file every stage edits. | n/a | reported |
| F-15 | P3 | Hygiene | `buildSpherifiedBox` is defined in `forgeshape_mesh_fixtures.cpp` outside the anonymous namespace with no header declaration (accidental external linkage). | n/a | reported |
| F-12 | P3 | Docs | Root docs carried statements no longer true (opens on a body, Undo Construction-only, no sketch grid, two representations, no arcs/splines, camera has no read-back, etc.). | n/a | **FIXED** (docs commit) — see `MARKDOWN_TRUTH_MATRIX.md` |
| F-17 | P3 | Comments | Production comments carried stage journeys, superseded designs and stale claims ("Sculpt has no undo", "exactly ONE active object", "Stage 003 … one indexed cube"). | n/a | **FIXED** (comment commit) — see `COMMENT_AUDIT.md` |

## Cleared suspicions (inspected, no defect)

* Linear Construction history keeps producer/dependent order correct across Delete → Edit → Undo/Undo/Redo (snapshot per step; the whole scene is captured).
* `sanitizeImportedMeshName` boundary handling (UTF-8 continuation, shortest-form floor, surrogate/range rejection, boundary cut) is correct — read line by line.
* The GLB JSON parser is index-addressed, bounds depth 15 and 1M values; no dangling reference across a grow.
* Every GLB accessor read is bounded before use; `ExternalBuffer`/`SparseAccessor`/interleaved views all fail closed by name.
* Sculpt history bytes are bounded ≤ 4 MiB per body on the `record` path; eviction never drops the newest.
* `cadFaceRanges` cap ordering is consistent with `generateCadMesh`.
* Workplane frames and `viewFrame` are always right-handed (no view state can mirror a sketch).
* The `.forge` load is genuinely all-or-nothing: decode+validate into temporary state, then a commit section that "cannot fail" (arithmetic on built objects) — confirmed by reading and by `FSR1A_07`/`IMP01A_20`.
* Delete of a producer with dependents is refused (`RefusedHasDependents`), never cascaded.
