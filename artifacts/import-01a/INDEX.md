# IMPORT-01A — durable Imported Mesh objects (`ARCH-OWNER-10`)

**What this stage did:** gave the GLB reader `GLB-IMPORT-R0`/`R1` built a second
destination. *Import GLB…* no longer produces a session-only preview; it creates
real project objects with rows in the Objects list, the ordinary gizmo, one Undo
step for the whole import, and geometry the `.forge` document carries so the
project reopens without the source file.

**What this stage did NOT do:** widen the parser (unchanged), give an imported
body a Construction Source or a Frozen Sculpt Mesh (`IMPORT-01B` owns sculpt),
read any material, texture, UV, colour, animation or rig, add OBJ or FBX, or move
one accepted UI-LAYOUT-R2 control.

## Contents

| File | What it holds |
| --- | --- |
| `CORPUS.md` | the ten `.forge` fixtures — the seven legacy digests unchanged, the three new ones, their semantic inventory, and the independent-encoder agreement |
| `selftest_startup.txt` | one clean debug launch: seventeen `*_SELFTEST_OK` tokens, their check counts, and both golden-digest lines |
| `FULL_SHARDED.txt` | the authoritative exhaustive-sharded instrumented aggregate on the final tree |

## Where the rest of the evidence lives

The domain half of `IMPORT-01A` is proved by the native self-tests, which build
their own scenes and depend on no live session — `IMP01A-01..15` and `-25` in
`FORGESHAPE_GLTF_IMPORT_SELFTEST_OK`, `IMP01A-16..20` in
`FORGESHAPE_PROJECT_SELFTEST_OK`. The device half is
`ImportedMeshDurableTest` (`IMP01A-14/15/19/21..25`, `E2E-IMP01A-01..12`).
`PROJECT_STATUS.md` owns the verified-capability matrix and the suite table.

## What is NOT claimed

`E2E-IMP01A-12` is **`OWNER_REAL_FILE_01A_RETEST_PENDING`**. The owner's own
`1 lowpoly.glb` is external to this coding environment; it was never searched for
and never opened. Every structural feature the coordinator listed for it is
covered by the deterministic synthetic fixture
(`artifacts/glb-import-r1/OWNER_SAMPLE_TARGET.md` maps them), but the file itself
was never in reach. That is a pending owner step, not a technical failure.

No external importer (Godot, Blender, `gltf-validator`) was run here, exactly as
in E2E-R1C and GLB-IMPORT-R0/R1.
