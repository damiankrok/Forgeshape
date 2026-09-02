# IMPORT-01B — device end-to-end results

All device work on **`emulator-5580`**, confirmed `ForgeShape_Stage006` with
`adb -s emulator-5580 emu avd name` before anything was installed.
**`emulator-5554` was never contacted.** Log ring buffer raised to 64 MiB and
confirmed with `logcat -g` before the startup capture.

Every case below is driven by an instrumented test, by its stable semantic id or
by the existing viewport touch harness. No control is located by screen
coordinate, and nothing sleeps for the autosave debounce — `awaitIdle` is the
barrier.

| ID | What it proves | Where | Result |
| --- | --- | --- | --- |
| E2E-IMP01B-01 | A deterministic GLB imports and *Start Sculpting* is drawn for the imported body, with the *Imported Mesh* editing context beside it | `ImportedMeshSculptTest.imp01b01_*` | PASS |
| E2E-IMP01B-02 | The first pre-stroke Sculpt frame carries the imported vertex count, reports no edits, and the authored placement is untouched by the freeze | `ImportedMeshSculptTest.imp01b02and03_*` | PASS |
| E2E-IMP01B-03 | A real Grab stroke through the whole touch path mints a new `SculptRevision` and turns `SCULPT_HAS_EDITS` on | `ImportedMeshSculptTest.imp01b04and05_*` | PASS |
| E2E-IMP01B-04 | *Back to Imported Mesh* returns to the source: the published vertex count is the imported one again and the `.forge` `IMPT` section is byte-identical to before the stroke | `ImportedMeshSculptTest.imp01b04and05_*` | PASS |
| E2E-IMP01B-05 | *Resume Sculpt* returns the same revision, the same counts and the edits | `ImportedMeshSculptTest.imp01b04and05_*` | PASS |
| E2E-IMP01B-06 | Save, autosave checkpoint, a saved copy and a fresh load all carry `IMPT`+`SCUL`; the reopened project re-encodes to the same bytes and offers the way back into Sculpt | `ImportedMeshSculptTest.imp01b11and13_*` | PASS |
| E2E-IMP01B-07 | A mixed Construction + Imported-with-Sculpt document is checkpointed, decodes through the real fail-closed decoder, and loads with every body and every sculpt mesh | `ImportedMeshSculptTest.imp01b11and13_*`, `ImportedMeshDurableTest.imp01a19_*` (`mixed_imported_sculpt_v1.forge`) | PASS |
| E2E-IMP01B-08 | With two bodies, the row's Delete removes one: the row disappears, the scene loses it, and one history step is recorded | `ObjectsDeleteTest.imp01b15and23_*` | PASS |
| E2E-IMP01B-09 | Undo restores the exact body (whole-document comparison), Redo removes it again | `ObjectsDeleteTest.imp01b18and19_*` | PASS |
| E2E-IMP01B-10 | Deleting the ACTIVE body selects the next row, and the chrome follows — no stale *Resume Sculpt* from the deleted body | `ObjectsDeleteTest.imp01b20_*` | PASS |
| E2E-IMP01B-11 | A deleted body is absent from picking, from Save, from the checkpoint and from the exported GLB; Undo restores it to all four; Redo omits it again | `ObjectsDeleteTest.imp01b22_*` | PASS |
| E2E-IMP01B-12 | The owner's own `1 lowpoly.glb` | `ImportedMeshSculptTest.e2eImp01b12_*` | **`OWNER_REAL_FILE_01B_RETEST_PENDING`** |

## E2E-IMP01B-12, stated honestly

The owner's `1 lowpoly.glb` is **not in this repository and not on the device**.
No user folder was searched for it — reading arbitrary directories to find a file
nobody put here is not a test, and a fabricated pass would be worse than an
honest gap.

`ImportedMeshSculptTest.e2eImp01b12_theOwnersRealFileIsNotPresentAndIsNotSearchedFor`
records the gap as a checked fact rather than as a comment, so the report cannot
quietly claim otherwise. Closing it is a tap on **Import GLB…**, then **Start
Sculpting**, then a stroke, then **Back to Imported Mesh**.

What stands in for it technically is `glb/construction_sentinel.glb` — a real
ForgeShape export with six top-level mesh nodes, several TRIANGLES primitives per
mesh, and enough vertices per body for a brush to capture a meaningful set. It is
the same fixture `IMPORT-01A` was accepted against.

## Picking, rendering and the deleted body

`ObjectsDeleteTest.imp01b22_*` isolates the body about to be deleted at the world
origin, moves every other body 40 m away, and taps the viewport centre through
the real touch path — asserting the ObjectId native code reports, never anything
about the coordinate. Before the delete that tap selects the doomed body; after
it, it does not. That is the render/pick half; the save, checkpoint and export
halves are byte comparisons in the same case.

## What the imported-source comparison actually compares

The `.forge` document's `IMPT` section payload, extracted by walking the
envelope (28-byte header, then 24-byte section headers plus payloads) inside the
test. That is the one place an imported object's positions, normals, indices and
submesh batches are observable end to end from the Java layer, and it is
deliberately not a native accessor written for the test's convenience: it is the
bytes the product would actually save.
