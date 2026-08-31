# GLB-IMPORT-R1 — external static GLB preview compatibility

Baseline: `27ec0bcc377d90fb69daa3b7b1332723ca4fe94b`, working tree clean at
preflight. Owner authority: `ARCH-OWNER-09`, approved 2026-08-31.

Device: `ForgeShape_Stage006` / `emulator-5580`, identity confirmed with
`adb -s emulator-5580 emu avd name`. The reserved `emulator-5554` was never
contacted.

## What this stage did

R0 gave ForgeShape a GLB reader that shares no line with its writer, so the file
and the scene could be compared. It read **exactly** what the exporter emits and
refused everything else by name — which is right for answering "is my exporter
wrong?" and useless for answering "what does my character look like in this
app?".

R1 widens the readable subset to the class of **static** file another sculpting
tool writes, so the owner can open their own low-poly character:

- a node **`matrix`** or ordinary **TRS**, composed `T·R·S` and **baked** into
  the preview's positions, so every draw item carries an identity model matrix
  and the placement exists in exactly one place;
- **several TRIANGLES primitives per mesh**, kept as separate draw batches so
  per-primitive `doubleSided` survives; primitives **sharing one POSITION
  accessor** decode once, so a seven-material character is the vertex count the
  file states and not seven copies of it;
- a **missing NORMAL**, generated area-weighted from the baked positions;
- **COLOR_0 / COLOR_1 / TEXCOORD_0 / TEXCOORD_1**, structurally validated and
  then deliberately not decoded;
- a material's **`doubleSided`**, reaching preview culling and nothing else;
- **`extras`**, ignored at every level.

Normals ride `transpose(inverse(L))`, not `L`. A negative determinant corrects
winding **for the preview only**. A zero determinant or a non-affine node matrix
is `SingularNodeTransform`. There is **no coordinate conversion of any kind** —
glTF and ForgeShape are both right-handed, +Y-up and metric.

## What this stage did NOT do

**It is still not production import.** What the preview produces has no
`ObjectId`, no Construction Source, no Frozen Sculpt Mesh, no `MeshStore`
publish; it never enters `ConstructionHistory`, `.forge`, the checkpoint or
`projectSemanticFingerprint`; it cannot be selected, picked, edited or
re-exported; and it is gone with the process. Most of the R1 suite exists to
hold exactly that line while the parser gets more permissive.

Durable import — materials, hierarchy, and what an imported object even *is* in
a Construction/Sculpt product — is `IMPORT-01` and stays post-MVP. OBJ and FBX
remain absent in both directions. No material or texture pipeline was started:
`doubleSided` is the only material member read anywhere.

## The owner's file

**`1 lowpoly.glb` is not in this repository and was not opened here.** No search
was made for it and no copy of it exists in the tree. What was exercised instead
is a deterministic **synthetic** fixture with the same structural feature set —
see `OWNER_SAMPLE_TARGET.md`, which maps every property the coordinator listed
for the real file to the case that covers it, and `nomad_like_fixture.txt` for
the fixture's own digest and counts.

`E2E-GLBIR1-10` is **`OWNER_SAMPLE_RUNTIME_TEST_PENDING`**, for that reason
alone. Closing it is one manual step: tap **Import GLB…** and pick the file.

## What is in this directory

| File | What it is |
| --- | --- |
| `SUPPORTED_SUBSET.md` | What R1 reads, what it does with the transform and the normals, and the named refusal for everything else |
| `OWNER_SAMPLE_TARGET.md` | The coordinator's fingerprint of the owner sample, and a feature-by-feature map to the cases that cover it |
| `TEST_RESULTS.md` | The `GLBIR1-01..21` table, and the R0 cases this stage deliberately rewrote |
| `DEVICE_E2E.md` | The `E2E-GLBIR1-01..10` device table |
| `nomad_like_fixture.txt` | The fixture's byte count, SHA-256, decoded counts and baked world bounds, written by the suite itself |
| `nomad_like_preview.png` | The fixture imported and drawn in the viewport, from a fixed camera pose |
| `selftest_startup.txt` | The clean debug launch behind the 17-suite claim |
| `FULL_SHARDED.txt` | The authoritative exhaustive-sharded instrumented aggregate |

The R0 package at [`../glb-import-r0/`](../glb-import-r0/) is unchanged and
remains the record of the roundtrip finding: the file and the scene describe the
same world geometry to exactly 0.0 m. Those cases still pass under R1.
