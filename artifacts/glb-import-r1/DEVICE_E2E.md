# GLB-IMPORT-R1 — device end-to-end results

Device: `ForgeShape_Stage006` / `emulator-5580`, identity confirmed with
`adb -s emulator-5580 emu avd name` before any case ran. The reserved
`emulator-5554` was never contacted.

Every case below drives the **real** picker-result seam:
`EditorWorkspaceView.onOpenGlbDocumentChosen(Uri)` — the production handler —
with a real file on the device. Only the system's own document UI is skipped,
because it is another app's surface and cannot be driven reliably from
instrumentation. What is under test either way is the parse, the refusal, the
switch and the untouched project.

| ID | What it proves | Result |
| --- | --- | --- |
| `E2E-GLBIR1-01` | The Nomad-like fixture imports through the real Import GLB result seam and **all seven primitives** render: one mesh, 1978 vertices, 3780 triangles, seven draw batches. The shared POSITION accessor is decoded once, not seven times | **PASS** |
| `E2E-GLBIR1-02` | The fixture states no NORMAL; the generated normals are finite, the world bounds carry no NaN or infinity, and the preview keeps drawing over several frames | **PASS** |
| `E2E-GLBIR1-03` | The matrix-baked asymmetric geometry has the expected world AABB — computed for the comparison by an **independent third reader** from the same bytes, so a transposed, dropped or mirrored matrix would show as a number | **PASS** |
| `E2E-GLBIR1-04` | The double-sided material reaches culling: every batch's published mesh reports `renderBothSides` | **PASS** |
| `E2E-GLBIR1-05` | A real one-finger orbit through the platform-neutral input seam turns the camera while the preview is shown, and the preview survives it | **PASS** |
| `E2E-GLBIR1-06` | Source → Imported → Source leaves the project snapshot, scene body ids, active body, mesh revision, undo/redo depth, encoded `.forge` bytes and autosave fingerprint **bit-identical** | **PASS** |
| `E2E-GLBIR1-07` | A malformed/unsupported external file refuses with the right stable token and bounded category and leaves no preview or project residue — tested with a required compression extension (`UnsupportedExtension`, *unsupported*) and a `.forge` document offered as a GLB (`NotGlb`, *unreadable*) | **PASS** |
| `E2E-GLBIR1-08` | Native `.forge` Open and external Import GLB are distinct functional acts: different intents, different MIME advertisements, and a `.glb` handed to the project path changes nothing and becomes no preview | **PASS** |
| `E2E-GLBIR1-09` | Clear releases the preview and the source stays editable — an edit after Clear moves the fingerprint, so the project is live and not merely intact | **PASS** |
| `E2E-GLBIR1-10` | The **exact owner sample** `1 lowpoly.glb`, SHA-256 `59fd1be…3bd6` | **`OWNER_SAMPLE_RUNTIME_TEST_PENDING`** |

## On `E2E-GLBIR1-10`

The owner's binary is not in this repository and was not present on any path
this stage was given. No search of the machine was made for it, and no copy of
it exists in the tree. The case is pending for that reason alone.

Every structural feature the coordinator listed for that file is covered — see
`OWNER_SAMPLE_TARGET.md` for the feature-by-feature map — against a synthetic
fixture of the same shape and scale. What cannot be claimed from here is the
file itself, and this bundle claims nothing about it.

Closing it is one manual step: open ForgeShape, tap the project control, choose
**Import GLB…**, and pick the real file.
