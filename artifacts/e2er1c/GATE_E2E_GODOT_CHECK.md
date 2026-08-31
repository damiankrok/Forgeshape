# External-importer check — prepared, NOT performed

This file exists so the owner can run the one check this repository cannot, and
so that nobody reading the evidence mistakes what was proven for what was not.

## What was NOT done

**No external program opened these files.** Godot was not run. Blender was not
run. `gltf-validator` was not run. No third-party importer of any kind was
invoked, in this stage or any other, and no result from one is claimed anywhere
in the E2E-R1C evidence, in `PROJECT_STATUS.md` or in `PRODUCT.md`.

None of those tools is installed in this environment, and installing one is not
authorized. That is not a failure of the stage: the stage's claim is that the
bytes conform to glTF 2.0, which is asserted against the specification by an
independent in-repo reader. Whether a particular third-party program is happy
with them is a different question, and it is the owner's to answer.

## The two files to open

Both are in this directory, pulled off the isolated AVD by
`scripts\run-glb-export-evidence.ps1`, and are byte-identical to what
`GlbExportTest` asserted about.

| File | Bytes | SHA-256 |
| --- | --- | --- |
| `construction_sentinel.glb` | 60312 | `266355a5dc2e58af626c2fb60b66420ad2c98e0d6eb1446925d8c5a3a5eaaf5f` |
| `sculpt_sentinel.glb` | 2684 | `16805a054491b3e65c4a0cec86dc3860667bee500b13a9a33d71eb04e1a517f1` |

## What to look for

`construction_sentinel.glb` — the six-body Construction fixture:

1. **Six separate objects**, one per primitive kind: box, cylinder, sphere,
   cone, capsule and plane.
2. **Up is up.** Nothing lies on its side, and no object needs a −90° X rotation
   to look right. An importer that inserts one is telling you the file is Z-up;
   it should not be.
3. **Scale reads in metres.** Body *i* is placed at (0.5·i, −0.25·i, 1.25·i)
   metres with scale (1 + 0.25·i, 2, 0.5) — so the six form a rising, receding
   line, each one taller than it is deep, each a little wider than the last.
4. **Surfaces are lit, not inside-out.** The box has hard 90° edges, the sphere
   and capsule are smooth. A shape that renders black or hollow with backface
   culling on would mean the winding is reversed.
5. The plane is the only double-sided object.

`sculpt_sentinel.glb` — the Sculpt fixture:

6. **Two objects**, and they are different kinds of thing: one is a 2 × 1 × 0.5 m
   box (a body that was never sculpted, exported from its Construction shape),
   the other a lopsided four-faced tetrahedron (a body that WAS sculpted,
   exported from its Frozen Sculpt Mesh). If the second one is a sphere, the
   exporter fell back to the Construction Source and that is a defect.

## Recording a result

Leave the verdict to the owner. This stage records no PASS or FAIL for it, and
the OWNER checkpoint GATE-E2E is where that decision belongs.

| | |
| --- | --- |
| Tool and version | *(owner)* |
| Result | *(owner)* |
| Notes | *(owner)* |
