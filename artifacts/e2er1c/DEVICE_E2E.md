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
| E2ER1C-02 | `e2er1c02_theFixturesTranslationsReachTheNodeAndItsScalesReachTheGeometry` | Same project | Body *i*'s node translation is exactly (0.5·i, −0.25·i, 1.25·i) m, with no node rotation, scale or matrix; and the scale is in the vertices instead — the fixture's Box, authored 1 m tall and scaled 2× on Y, exports with a baked Y extent well past that | PASS |
| E2ER1C-03 | `e2er1c03_aSculptProjectExportsTheSculptMeshForTheBodyThatHasOne` | Loaded `sculpt_mixed_v1.forge`; confirmed the product reopened in Sculpt; exported | Both bodies exported. The sculpted body is the fixture's tetrahedron and **not** the 1.5 m sphere its Construction Source describes; the never-sculpted companion is its 2 × 1 × 0.5 m Construction box. (Under `ARCH-OWNER-07` the tetrahedron's placement rotation is baked, so it is now identified by rotation-invariant properties rather than by its authored bounding box — see E2ER1C-C1-03 below.) | PASS |
| E2ER1C-04 | `e2er1c04_exportWritesTheFileTheUserChoseAndItParses` | Exported, then drove the production `onCreateGlbDocumentChosen` with a real destination `Uri` | The file exists where the user chose, is byte-for-byte what the exporter produced, and parses clean off the disk | PASS |
| E2ER1C-04 | `e2er1c04_theCreateIntentAsksForAGlbByNameAndType` | Built the create intent | `ACTION_CREATE_DOCUMENT`, `CATEGORY_OPENABLE`, type `model/gltf-binary`, suggested name ending `.glb` | PASS |
| E2ER1C-05 | `e2er1c05_cancellingWritesNothingAndChangesNothing` | Requested an export, then answered the picker with `null` | Project fingerprint unchanged; not one file written; and the staged bytes are gone, so a later answer with nothing staged writes nothing either | PASS |
| E2ER1C-06 | `e2er1c06_exportingMutatesNoProjectStateAndTouchesNoForgeSlot` | Exported a loaded project to a real destination | The whole observable native snapshot identical; mesh revision, undo depth, redo depth, active `ObjectId` and the full body-id list unchanged; project fingerprint unchanged; neither the manual `.forge` slot nor the recovery checkpoint created | PASS |
| — | `e2er1c_evidenceConstructionSentinel`, `e2er1c_evidenceSculptSentinel` | Exported each fixture and left the file for the pull | Each validates before it is written out | PASS |

## E2E-R1C-C1 — the baked static transform (`ARCH-OWNER-07`)

| # | Case | What was done | What was asserted | Result |
| --- | --- | --- | --- | --- |
| E2ER1C-C1-01 | `fsr1cC1_01to03_theNodeCarriesTranslationOnlyWithIdentityRotationAndScale` | An asymmetric Construction body — position (3.5, −1.25, 0.75), rotation (47.5, −22, 13.25), scale (2, 3, 0.5) — exported through the real path | The node's translation is exactly the authored position, unscaled; rotation, scale and matrix are ABSENT, checked on the parsed node and again on the raw JSON | PASS |
| E2ER1C-C1-02 | `fsr1cC1_04_rotationAndScaleAreBakedIntoTheGeometry` | Same body, scaled 2/3/0.5, then turned 90° about Y | The 1 × 2 × 4 m box arrives 2 × 6 × 2 m; a quarter turn moves the 4 m depth onto X and the 1 m width onto Z, leaving height alone — numbers only the baked geometry can carry | PASS |
| E2ER1C-C1-02 | `fsr1cC1_05_bakingHappensAboutTheLocalOriginAndNothingIsRecentred` | A CONE at (4, −2, 0.5), rotated on all three axes, scaled 1.5/1/0.25 | The baked bounds are lopsided about the origin — a recentre would force `min == −max` on every axis — and the node still carries the pivot | PASS |
| E2ER1C-C1-02 | `fsr1cC1_06_normalsRideTheInverseTransposeUnderNonUniformScale`, `..._bakedNormalsStayPerpendicularToTheirFaces` | A sphere exported unscaled and again at 4:1:1; a box rotated and scaled 2/3/0.5 | Every written normal equals `normalize(transpose(inverse(L)) · n)` computed in the test, differs from `normalize(L · n)` on essentially every vertex, is unit length, and stays perpendicular to its own faces; positions rode `L` | PASS |
| E2ER1C-C1-02 | `fsr1cC1_07_positionBoundsAreRecomputedFromTheBakedVertices` | Same box under a 2/3/0.5 scale | The declared `min`/`max` are the baked bounds, re-derived from the data by the independent reader, and describe the baked size rather than the authored one | PASS |
| E2ER1C-C1-03 | `e2er1c03_aSculptProjectExportsTheSculptMeshForTheBodyThatHasOne` | Loaded `sculpt_mixed_v1.forge`, exported | The sculpted body is still its 4-triangle, 4-corner tetrahedron with its longest inter-corner distance intact at 2.2079 m — identified by rotation-invariant properties, because the bake turns it — is not its Construction sphere, is visibly no longer axis-aligned, and carries no node rotation. Its never-sculpted companion is still its 2 × 1 × 0.5 m Construction box | PASS |
| E2ER1C-C1-04 | `fsr1cC1_09_eachBodyBakesItsOwnTransformIndependently`, `e2er1c01`, `e2er1c02`, `e2er1c06` | Two identical 1 m cubes with different placements; and the six-body fixture | Each body bakes only its own `L` — one stays a 1 m cube, the other becomes 3 × 7 × 0.25 m — with its own translation on its own node; the six-body fixture keeps six separate nodes, its authored translations, and leaves project, history, `ObjectId`s and both `.forge` slots untouched | PASS |
| E2ER1C-C1-05 | `e2er1c_evidenceConstructionSentinel`, `e2er1c_evidenceSculptSentinel` + `scripts\run-glb-export-evidence.ps1` | Both sentinels regenerated from the same deterministic fixtures and pulled off the device | Each validates through the independent reader before it is written; the host re-checks the GLB header on the pulled copy; both are then parsed again outside the exporter — see `BAKED_TRANSFORM_C1.md` | PASS |
| E2ER1C-C1-06 | `uir4b17_constructionChromeMeetsTheFortyEightDpFloor`, `uir4b17_theToolbarIconControlsAreStillFullyOnScreenInSculpt`, `EditorWorkspaceRightHostPlacementTest` (`UILR2C-01..12`) | The whole instrumented suite | Export keeps its placement, its 48 dp hit target and its position on screen, and the accepted UI-LAYOUT-R2 right host does not move. C1 changed no product Java, no resource and no UI file — its product delta is the exporter plus one added `ConstructionTransform` accessor — so this is a regression check rather than a new claim | PASS |

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
