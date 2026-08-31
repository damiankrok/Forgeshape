# E2E-R1C / Stage 023 — early GLB export evidence

Baseline this stage started from, resolved in full:
`678af45d19779ff5af96cc3e5db5850171ba2bdb` (short `678af45d`), working tree
clean at preflight.

Device: `ForgeShape_Stage006` / `emulator-5580`, identity confirmed with
`adb -s emulator-5580 emu avd name`. The reserved `emulator-5554` was never
contacted.

## What is in this directory

| File | What it is |
| --- | --- |
| `COORDINATE_AUTHORITY.md` | The export-coordinate authority gate: up axis, handedness, axis ownership, body-local convention, transform order and winding, each from named repository truth, and the resulting **zero-conversion** verdict |
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
crease-policy normals and placement matrix, in metres, +Y up, with **no
coordinate conversion of any kind** — ForgeShape's world convention and glTF's
are the same convention, which was proved from repository truth before any
exporter code was written. A body with a Frozen Sculpt Mesh exports that mesh; a
body without one exports re-evaluated Construction geometry, and there is no
fallback in the other direction. Exporting is a read: it mints no revision,
opens no transaction, moves no `ObjectId` or sculpt vertex, and writes neither
`.forge` slot.

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
