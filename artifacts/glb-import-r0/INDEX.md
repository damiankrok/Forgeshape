# GLB-IMPORT-R0 — bounded diagnostic imported mesh preview

Baseline: `868995d135b4582693b10cd486abc7418b45c5a9`, working tree clean at
preflight. Owner authority: `ARCH-OWNER-08`, approved 2026-08-31.

Device: `ForgeShape_Stage006` / `emulator-5580`, identity confirmed with
`adb -s emulator-5580 emu avd name`. The reserved `emulator-5554` was never
contacted.

## The question, and the answer

The owner saw a discrepancy between the ForgeShape scene and Blender that the
corrected node scale of 1/1/1 did not explain. Three things could produce that:
**(a)** ForgeShape's exporter is wrong, **(b)** a reader is wrong, or **(c)** the
external tool presents the same geometry differently. This stage separates (a)
from (b) and (c) — the split that decides whether ForgeShape has a defect.

> ### Verdict: `ROUNDTRIP_EQUIVALENT`, with a maximum world-position delta of **exactly 0.0 metres**.
>
> Every vertex of every body, in world space, is **bit-identical** between the
> ForgeShape scene and the same scene decoded out of the `.glb` by a parser that
> shares no line with the writer. Vertex counts, triangle counts, index order
> and per-body world bounds all match exactly. The largest world-space normal
> disagreement is `1.2e-06` degrees, which is float32 noise.
>
> **So the discrepancy the owner is seeing is not an exporter or parser defect
> in ForgeShape.** It is (b)-free and (a)-free; what remains is (c) — how the
> external tool presents, frames or orients the same geometry. This stage does
> not and cannot say which of those it is, and it makes no claim about Blender.

## What is in this directory

| File | What it is |
| --- | --- |
| `construction_roundtrip.txt` | The six-body Construction project: exported through the real writer, reimported by the independent parser, compared per body against domain truth |
| `sculpt_roundtrip.txt` | The Sculpt project, same treatment — including that the sculpted body's compared representation is `source=sculpt` |
| `committed_sentinels.txt` | The **committed** sentinel bytes from `artifacts/e2er1c/`, compared against the projects they were exported from. Not a fresh export |
| `SUBSET.md` | What the R0 parser supports, and the named refusal for everything it does not |
| `source_scene.png` | The Construction project drawn from a pinned camera |
| `imported_preview.png` | The same project's exported `.glb`, decoded and drawn from the **same** camera pose |
| `FULL_SHARDED.txt` | The authoritative exhaustive-sharded instrumented aggregate |
| `selftest_startup.txt` | The clean debug launch behind the 17-suite claim |

## What this is NOT

- **Not production import.** It reads a file ForgeShape itself wrote, in a
  deliberately narrow subset, to answer one question. Arbitrary files,
  materials, hierarchy and the whole question of what an imported object even
  *is* in a Construction/Sculpt product are `IMPORT-01`, and stay post-MVP.
- **Not project truth.** The preview has no `ObjectId` from the scene, no
  Construction Source, no sculpt representation and no history. It is never
  saved, autosaved, checkpointed or re-exported, cannot be selected or edited,
  and is gone when the process is.
- **Not a claim about Blender.** No external tool was run here.
- **No OBJ, no FBX**, in either direction. No materials, textures, UVs,
  animation or skinning — each is a named refusal, not an omission.

## Why the comparison is worth anything

The two sides come from different code on purpose.

- **Expected** is re-derived from DOMAIN truth: the primitive generator or the
  Frozen Sculpt Mesh, through the product's own crease-policy render-mesh
  derivation, placed by `ConstructionTransform::modelMatrix()` — the matrix the
  *renderer* consumes. It is not the exporter's captured arrays. If the
  exporter captured the wrong geometry, this side still holds the right
  geometry and the comparison fails.
- **Actual** is the real bytes through `forgeshape_gltf_import`, which shares
  nothing with the writer and re-derives every offset, length, stride and bound
  from the file.

The identity under test is `modelMatrix() · p_local == nodeTranslation · p_baked`,
which is `T·L·p == T·(L·p)`. It holds only if the bake, the node transform and
the file all agree.

**And the comparison can fail.** `GLBIR0_13_a_real_disagreement_is_reported_as_a_mismatch`
moves a body 8 m after exporting and checks that the diagnostic reports
`ROUNDTRIP_MISMATCH` naming that body and that distance; `glbir0_13_theComparisonCanAlsoFail`
compares a project against another project's file. A diagnostic that could only
say "equivalent" would be worthless.
