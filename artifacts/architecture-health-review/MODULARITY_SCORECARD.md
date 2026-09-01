# Modularity scorecard — ForgeShape architecture health review (2026-09-01)

Scale 1–5. Every score below 4 carries evidence, consequence, whether it
blocks, and timing (NOW / BEFORE_IMPORT_01B / BEFORE_NEXT_MAJOR_FEATURE / LATER).

| # | Area | Score | Evidence / consequence / blocking / timing |
| --- | --- | --- | --- |
| 1 | Ownership clarity (one owner per fact) | 5 | `MODULE_MAP.md` ownership table; every fact has one owner and the docs name it. |
| 2 | Dependency direction (UI→JNI→domain→renderer/codec) | 5 | No upward include; Android types stop at `forgeshape_jni.cpp`; domain never sees renderer. |
| 3 | Platform neutrality of the domain | 5 | grep-verified; codec knows no filesystem, importer/exporter know no `Uri`. |
| 4 | Representation extensibility (adding a body kind) | 3 | 6+ dispatch sites and two duplicated geometry-extraction blocks (`gltf_export` / `glb_roundtrip`); a third kind touches ~12 files. Consequence: a forgotten `if/else` site is a silent skip (the compiler only catches `switch` sites). Not blocking. Timing: BEFORE_NEXT_MAJOR_FEATURE (a third representation). |
| 5 | Feature isolation in the Java workspace | 3 | `EditorWorkspaceView.java` 3427 lines / 113 methods hosts five contexts' wiring. Consequence: every UI stage edits one file; regression surface grows linearly. Not blocking. Timing: BEFORE_NEXT_MAJOR_FEATURE. |
| 6 | Transaction / history integrity | 5 | One history, one boundary, `ScopedConstructionEdit`; an import is one step; verified by `EditorWorkspaceHistoryTest` 20/20 and `ImportedMeshDurableTest` 12/12. |
| 7 | Persistence completeness and fail-closed | 4 | Both representations serialized and validated; decoder does not bound `nextObjectId` below the preview key range (crafted-file only). Timing: LATER, and a `.forge` decision for the coordinator. |
| 8 | Thread safety | 4 | One defect found and fixed (`applyPrimitive` unlocked write vs autosave read). Remaining: one unlocked counter read for a log line. After the fix every scene write is under `g_stateMutex`. |
| 9 | Error handling (named refusals, no silent fallbacks) | 5 | Every refusal has a named status crossing JNI and a Java constant; import and load are all-or-nothing. |
| 10 | Renderer ownership / resource lifecycle | 3 | `Renderer::bodies_` never prunes a body that left the snapshot (Undo of a creation or import). Consequence: bounded GPU memory retention for the process life; nothing wrong is drawn. Not blocking. Timing: LATER (renderer change; outside this review's policy). |
| 11 | Testability | 5 | 17 native suites build their own scene; JVM 70; 32 instrumented classes by `R.id`, no sleeps; `awaitIdle` barrier for autosave. |
| 12 | Comment accuracy | 4 | 0 markers; six stale comments found and corrected this review; the rest is accurate and explains why. |
| 13 | Documentation currency | 4 | `ARCHITECTURE.md` had five pre-persistence / pre-IMPORT-01A statements, corrected here. `PROJECT_STATUS.md` and `CLAUDE.md` were current. |
| 14 | Performance discipline (no per-frame domain work, bounded structures) | 4 | Measured: mesh build 0.034 ms, one upload at startup. Found and removed one double tessellation per publish. O(n²) duplicate-id scan in the decoder is bounded by `kMaxProjectBodies = 4096` (inferred fine, not measured). |
| 15 | Naming / vocabulary consistency | 4 | One canonical term per concept; known accepted debt: `kConstructionBoxObjectId`/`kDemoCubeObjectId` alias, dead `setChipReserved`, preview keys typed as `ObjectId`. |

**Aggregate: 4.3 / 5.** No area is below 3; the three 3s are structural debt
with a named timing, none of them blocking `IMPORT-01B`.
