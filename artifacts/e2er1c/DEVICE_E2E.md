# E2E-R1C — device end-to-end cases

All on `ForgeShape_Stage006` / `emulator-5580`. Each case loads a pinned
`.forge` fixture through the **production** Open File handler, exports through
the **production** export path, and reads the result back through `GlbDocument`,
the independent glTF 2.0 reader.

The system's document picker is another app's surface and cannot be driven
reliably from instrumentation, so the split is the same one E2E-R1B used and
stated: what happens to the BYTES is exercised for real, end to end, against a
real `ContentResolver` and a real destination `Uri`; what is ASKED of the system
is asserted as the exact `Intent` (`e2er1c04_theCreateIntentAsksForAGlbByNameAndType`).
Between them that covers everything ForgeShape is responsible for.

| # | Case | What was done | What was asserted | Result |
| --- | --- | --- | --- | --- |
| E2ER1C-01 | `e2er1c01_aSixBodyProjectExportsAsOneValidGlbWithSixDistinctNodes` | Loaded `construction_multibody_v1.forge` (six bodies, one per primitive kind); exported | Zero structural problems from the independent reader; six scene bodies, six nodes, six *different* matrices, every one carrying triangles | PASS |
| E2ER1C-02 | `e2er1c02_theFixturesTranslationsRotationsAndScalesReachTheFile` | Same project | Body *i* at exactly (0.5·i, −0.25·i, 1.25·i) m in matrix elements 12–14; column lengths exactly (1+0.25·i, 2, 0.5) — the non-uniform scale; and no column axis-aligned, so the asymmetric rotation is really there | PASS |
| E2ER1C-03 | `e2er1c03_aSculptProjectExportsTheSculptMeshForTheBodyThatHasOne` | Loaded `sculpt_mixed_v1.forge`; confirmed the product reopened in Sculpt; exported | Both bodies exported. The sculpted body is the fixture's tetrahedron — local bounds exactly [0, 1.5] × [0, 1.25] × [0, 1.75], four triangles — and **not** the 1.5 m sphere its Construction Source describes. The never-sculpted companion is its 2 × 1 × 0.5 m Construction box | PASS |
| E2ER1C-04 | `e2er1c04_exportWritesTheFileTheUserChoseAndItParses` | Exported, then drove the production `onCreateGlbDocumentChosen` with a real destination `Uri` | The file exists where the user chose, is byte-for-byte what the exporter produced, and parses clean off the disk | PASS |
| E2ER1C-04 | `e2er1c04_theCreateIntentAsksForAGlbByNameAndType` | Built the create intent | `ACTION_CREATE_DOCUMENT`, `CATEGORY_OPENABLE`, type `model/gltf-binary`, suggested name ending `.glb` | PASS |
| E2ER1C-05 | `e2er1c05_cancellingWritesNothingAndChangesNothing` | Requested an export, then answered the picker with `null` | Project fingerprint unchanged; not one file written; and the staged bytes are gone, so a later answer with nothing staged writes nothing either | PASS |
| E2ER1C-06 | `e2er1c06_exportingMutatesNoProjectStateAndTouchesNoForgeSlot` | Exported a loaded project to a real destination | The whole observable native snapshot identical; mesh revision, undo depth, redo depth, active `ObjectId` and the full body-id list unchanged; project fingerprint unchanged; neither the manual `.forge` slot nor the recovery checkpoint created | PASS |
| — | `e2er1c_evidenceConstructionSentinel`, `e2er1c_evidenceSculptSentinel` | Exported each fixture and left the file for the pull | Each validates before it is written out | PASS |

## Determinism

`scripts\run-glb-export-evidence.ps1 -Serial emulator-5580` was run twice, with
the on-device files deleted before each run. Both runs produced byte-identical
sentinels — compared with `cmp`, not only by digest.

## Device isolation

`emulator-5554` was never contacted, in this stage or by any script it runs:
`run-instrumented-tests.ps1` and `run-glb-export-evidence.ps1` both refuse that
serial before issuing any adb command, both require an explicit `-Serial`, and
both confirm the AVD's own name rather than trusting the port.
`scripts\verify-device-guards.ps1` re-checks this mechanically (DEV2-01..07,
DEV3-01..06) and passes, including DEV3-06's scan of every executable script
surface — which now includes the new evidence script.
