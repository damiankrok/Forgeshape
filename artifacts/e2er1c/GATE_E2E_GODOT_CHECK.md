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
| `construction_sentinel.glb` | 59884 | `8ac4fb6abb509361b911c9b4bbc2dd8385910668808b40fbd9faeab244f2c3ff` |
| `sculpt_sentinel.glb` | 2588 | `ae82a0720d9f503f77d0237edf40ab5c1152bae6938df08166e1ea9b31014f2c` |

### Superseded — PRE_ARCH_OWNER_07 history, NOT the current files

The first pair, written before the owner's Blender review changed the static
transform contract. They wrote the whole `T·R·S` as a node matrix. They are kept
here only so a reader who has one of them can tell which it is; they are in Git
history at `cdc184b999bae716809f632c11fe771ebbb98c2b` and **the files in this
directory no longer have these digests.**

| File | Bytes | SHA-256 |
| --- | --- | --- |
| `construction_sentinel.glb` (pre-bake) | 60312 | `266355a5dc2e58af626c2fb60b66420ad2c98e0d6eb1446925d8c5a3a5eaaf5f` |
| `sculpt_sentinel.glb` (pre-bake) | 2684 | `16805a054491b3e65c4a0cec86dc3860667bee500b13a9a33d71eb04e1a517f1` |

## The check that is new, and the reason for it

`ARCH-OWNER-07`: a static export now **bakes each body's rotation and scale into
its vertices** and leaves only the translation on the node. So before anything
else, in the importer's object inspector:

- **Rotation reads 0, 0, 0** for every imported object.
- **Scale reads 1, 1, 1** for every imported object.
- **Location** is the ForgeShape position, after whatever coordinate-basis
  conversion the importer ordinarily applies.
- The object still *looks* turned and sized, because that is now the mesh.

A non-identity rotation or a non-unit scale on any object means the bake did not
happen and this correction failed.

## What to look for

`construction_sentinel.glb` — the six-body Construction fixture:

1. **Six separate objects**, one per primitive kind: box, cylinder, sphere,
   cone, capsule and plane.
2. **Up is up.** Nothing lies on its side, and no object needs a −90° X rotation
   to look right. An importer that inserts one is telling you the file is Z-up;
   it should not be.
3. **Scale reads in metres.** Body *i* stands at (0.5·i, −0.25·i, 1.25·i)
   metres — so the six form a **descending**, receding line: each one steps
   further along +X, further away along +Z, and a little LOWER, because the Y
   term is negative. Each was authored with scale (1 + 0.25·i, 2, 0.5), which
   now lives in its mesh: each is twice as tall as it was drawn, half as deep,
   and a little wider than the one before.
4. **Surfaces are lit, not inside-out.** The box has hard 90° edges, the sphere
   and capsule are smooth. A shape that renders black or hollow with backface
   culling on would mean the winding is reversed.
5. The plane is the only double-sided object.

`sculpt_sentinel.glb` — the Sculpt fixture:

6. **Two objects**, and they are different kinds of thing: one is a 2 × 1 × 0.5 m
   box (a body that was never sculpted, exported from its Construction shape,
   and at the origin unrotated so its mesh is exactly its authored size), the
   other a lopsided four-faced tetrahedron (a body that WAS sculpted, exported
   from its Frozen Sculpt Mesh, and turned — so its mesh is the turned shape).
   If the second one is a sphere, the exporter fell back to the Construction
   Source and that is a defect.
7. **Both read rotation 0,0,0 and scale 1,1,1**, like everything in the other
   file.

## Recording a result

Leave the verdict to the owner. This stage records no PASS or FAIL for it, and
the OWNER checkpoint GATE-E2E is where that decision belongs.

| | |
| --- | --- |
| Tool and version | *(owner)* |
| Result | *(owner)* |
| Notes | *(owner)* |
