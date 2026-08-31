# E2E-R1C — early GLB export evidence

Two passes, both recorded here:

- **Stage 023** built the exporter. Baseline
  `678af45d19779ff5af96cc3e5db5850171ba2bdb`, delivered at
  `cdc184b999bae716809f632c11fe771ebbb98c2b`.
- **E2E-R1C-C1** changed the static transform contract after the owner reviewed
  the first files in Blender (`ARCH-OWNER-07`): rotation and scale are now baked
  into the geometry and only the translation stays on the node. Baseline
  `cdc184b999bae716809f632c11fe771ebbb98c2b`.

Both started from a clean working tree at preflight. The sentinel `.glb` files
in this directory are the **corrected, baked** ones; the pre-bake digests are
kept as history in `GATE_E2E_GODOT_CHECK.md` and are not presented as current.

Device: `ForgeShape_Stage006` / `emulator-5580`, identity confirmed with
`adb -s emulator-5580 emu avd name`. The reserved `emulator-5554` was never
contacted.

## What is in this directory

| File | What it is |
| --- | --- |
| `COORDINATE_AUTHORITY.md` | The export-coordinate authority gate: up axis, handedness, axis ownership, body-local convention, transform order and winding, each from named repository truth, and the resulting **zero-conversion** verdict. Section 5 carries a superseded-by note for `ARCH-OWNER-07`; the conventions themselves are unchanged |
| `BAKED_TRANSFORM_C1.md` | `ARCH-OWNER-07`: the split `Model = T · L`, the normal matrix and why it is not a second answer, the no-recentre proof and why it needs a cone, the determinant rule, and both corrected sentinels read back independently |
| `TEST_RESULTS.md` | Native self-tests, focused instrumented classes, host builds, guards, corpus digests, and the one design the run rejected |
| `DEVICE_E2E.md` | The device end-to-end case table: what was done, what was asserted, and the result |
| `FULL_SHARDED.txt` | The authoritative exhaustive-sharded instrumented aggregate, as the runner printed it |
| `selftest_startup.txt` | The clean debug launch capture behind the 16-suite / 2259-check claim |
| `construction_sentinel.glb` | The six-body Construction fixture exported, pulled off the device |
| `sculpt_sentinel.glb` | The Sculpt fixture exported, pulled off the device |
| `GATE_E2E_GODOT_CHECK.md` | The external-importer check, **prepared and NOT performed**, with what to look for. No verdict is recorded — that is the owner's at GATE-E2E |

## The claim, in one paragraph

ForgeShape writes the current model as a single glTF 2.0 binary to a destination
the user picks through the Storage Access Framework, using the Export control
that was already in the Global Toolbar. The file carries every body's triangles,
crease-policy normals and placement, in metres, +Y up, with **no coordinate
conversion of any kind** — ForgeShape's world convention and glTF's are the same
convention, which was proved from repository truth before any exporter code was
written. Each body's **rotation and scale are baked into its vertices** and only
its translation stays on the node, so the object arrives already the shape it
was made as, standing where it stood, on the pivot it was rotated about. A body
with a Frozen Sculpt Mesh exports that mesh; a body without one exports
re-evaluated Construction geometry, and there is no fallback in the other
direction. Exporting is a read: it mints no revision, opens no transaction,
moves no `ObjectId` or sculpt vertex, and writes neither `.forge` slot.

## What is NOT claimed

- **No external program opened these files.** Godot, Blender and
  `gltf-validator` were not run; none is installed and installing one is not
  authorized. See `GATE_E2E_GODOT_CHECK.md`.
- This is the **early vertical slice**, not the Stage 033 exporter. No UVs, no
  textures, no chosen materials, no hierarchy, no merge or unit options, no
  compression.
- There is **no importer**, and no OBJ or FBX in either direction.
- No third-party interchange library is used anywhere.

## Why the evidence is worth anything

The input is pinned by a digest from a second, independent `.forge` encoder; the
output is read back by `GlbDocument`, a test-only glTF 2.0 reader written from
the specification that shares no line with the exporter and re-derives every
chunk boundary, accessor offset and declared bound rather than trusting a number
the writer stated. `fsr1c13_aTruncatedExportIsRejectedByTheIndependentReader`
exists so that the reader is known to be able to fail.
