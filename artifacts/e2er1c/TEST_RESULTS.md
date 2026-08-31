# E2E-R1C — test and build results

Every run below is on the ForgeShape-owned isolated AVD `ForgeShape_Stage006`,
serial `emulator-5580`, identity confirmed with `adb -s emulator-5580 emu avd
name` before use. The reserved `emulator-5554` was never contacted. Every device
call went through `scripts\run-instrumented-tests.ps1 -Serial emulator-5580` or
an explicit `adb -s emulator-5580`; no bare `adb`, no device enumeration, and no
unscoped `connected*AndroidTest`.

## Native self-tests — 16 suites, 2259 checks, 0 failures

Captured from a clean debug launch with a 64 M log buffer; the full capture is
`selftest_startup.txt` in this directory.

| suite | checks |
| --- | --- |
| `FORGESHAPE_CAMERA_SELFTEST_OK` | 119 |
| `FORGESHAPE_PICKING_SELFTEST_OK` | 174 |
| `FORGESHAPE_DYNAMIC_MESH_SELFTEST_OK` | 91 |
| `FORGESHAPE_CONSTRUCTION_BOX_SELFTEST_OK` | 100 |
| `FORGESHAPE_CONSTRUCTION_TRANSFORM_SELFTEST_OK` | 121 |
| `FORGESHAPE_CONSTRUCTION_PRIMITIVE_SELFTEST_OK` | 125 |
| `FORGESHAPE_CONSTRUCTION_SPHERE_SELFTEST_OK` | 105 |
| `FORGESHAPE_CONE_CAPSULE_SELFTEST_OK` | 163 |
| `FORGESHAPE_SCULPT_BRUSH_KERNEL_SELFTEST_OK` | 378 |
| `FORGESHAPE_RENDER_SHADING_SELFTEST_OK` | 329 |
| `FORGESHAPE_SCENE_SELFTEST_OK` | 79 |
| `FORGESHAPE_CONSTRUCTION_HISTORY_SELFTEST_OK` | 114 |
| `FORGESHAPE_GIZMO_SELFTEST_OK` | 145 |
| `FORGESHAPE_PROJECT_SELFTEST_OK` | 132 |
| `FORGESHAPE_RENDER_RECOVERY_SELFTEST_OK` | 24 |
| **`FORGESHAPE_GLTF_EXPORT_SELFTEST_OK`** (new) | **60** |

then `FORGESHAPE_NATIVE_VIEWPORT_OK`. No `_SELFTEST_FAIL` and no `_FAIL:` token
appears anywhere in the capture.

`FORGESHAPE_PROJECT_GOLDEN_SHA256` in the same capture reports
`construction=8830e7fb…` / `sculpt=112b1097…`, matching the corpus digests
below — so the fixtures these export cases load are the pinned ones.

The new suite, `FSR1C-01..12`, covers the writer from the inside: the container
framing and chunk alignment, metres and the +Y-up axis assertion on a 1 × 2 × 4 m
box, exactly one matrix for one body, an unplaced body exporting identity, the
exported matrix equalling `transform().modelMatrix()` as exact float equality,
translation in `m[12..14]` and not multiplied by scale, moving a body moving no
vertex, `Body_<ObjectId>` names in scene order, a source change reaching the
export, Sculpt and Construction exporting differently, unit-or-zero normals,
outward winding, and — `FSR1C-12` — the writer REFUSING a non-finite matrix, an
out-of-range index, a ragged index count and an empty mesh.

## Instrumented — focused

| class | result |
| --- | --- |
| `GlbExportTest` (new: `FSR1C-13..16`, `E2ER1C-01..06`, 2 evidence cases) | **OK (17 tests)** |
| `EditorWorkspaceCorrectionTest` (UI-R4B, incl. `UIR4B-16`, `-17`) | OK (37 tests) |
| `EditorWorkspaceControlsTest` | OK (23 tests) |
| `EditorWorkspaceThemeTest` | OK (19 tests) |
| `ProjectTransferTest` (E2E-R1B transfer, `FSR1B-18`) | OK (13 tests) |
| `EditorWorkspaceChromeCompositionTest` (UI-R4C) | OK (12 tests) |

`MODE=FOCUSED_SUBSET` is printed by the runner for each of these; none of them
can emit a full-suite marker, and none is offered as one.

### One rejected design, recorded because the run rejected it

The Export control was first labelled **"Export GLB…"**. That is wider than
"Export", the extra width comes out of the same Global Toolbar row as the
transition button, and `UIR4B-16` failed: **Back to Construction** had
abbreviated to `← Construction`. Critical navigation outranks naming the format
in a label, so the drawn label stayed "Export" and the format moved to the
content description (`Export GLB — save the model as a .glb file`) and to the
`.glb` filename the system picker opens with. `UIR4B-17` was extended to hold
Export to the 48 dp floor and to keep it fully on screen, and passes.

## Instrumented — authoritative exhaustive-sharded full suite

`scripts\run-instrumented-tests.ps1 -Serial emulator-5580 -FullSharded`. The
runner's complete output is `FULL_SHARDED.txt` in this directory; its aggregate
block is:

```
discovered_classes=28
discovered_tests=399
assigned_union=399
executed_union=399
missing=0
duplicates=0
unexpected=0
execution_missing=0
failed_shards=0
aborted_shards=0
aggregate=PASS
FULL_SHARDED_SUITE_PASS
```

Five shards, every one `status=PASS` with `actual` equal to `expected`. The union
comes from live AndroidJUnitRunner discovery, not from a list in a document, and
it is exactly-once: nothing missing, nothing duplicated, nothing unexpected.

**Run once, on the shipped tree.** An earlier aggregate also passed, and was
discarded rather than reported: it was taken before a two-line correction to the
export failure message, so it did not cover the code that ships. This one does.
No green aggregate was re-run for ceremony.

**Nothing was left running.** After the aggregate reached its terminal state,
the device showed no `com.forgeshape.app.test` process and no instrumentation
process, and the host showed no `run-instrumented-tests.ps1`, shard or
instrumentation process — only the adb fork-server and the Gradle daemon, which
belong to the environment rather than to this stage.

## JVM — 70 tests

`:app:testDebugUnitTest`, counted from the task's own result XML across 8
classes.

## Host-side

| check | result |
| --- | --- |
| `:app:testDebugUnitTest` (JVM suites) | BUILD SUCCESSFUL |
| `:app:assembleDebug` — `arm64-v8a` + `x86_64` | BUILD SUCCESSFUL, both ABIs present |
| `:app:assembleRelease` — `arm64-v8a` + `x86_64` | BUILD SUCCESSFUL, both ABIs present |
| `scripts\verify-device-guards.ps1` (DEV2-01..07, DEV3-01..06) | All device-guard checks PASS |
| `scripts\build-forge-corpus.ps1 -VerifyOnly` | all 7 fixtures match their pinned digests |

## The `.forge` inputs, pinned

The two fixtures `GlbExportTest` loads ship in the androidTest APK at
`app/src/androidTest/assets/forge/` and are byte-identical copies of the
permanent golden corpus, which the independent PowerShell encoder produces from
`DATA_PACKAGE_SPEC.md`:

| fixture | bytes | SHA-256 |
| --- | --- | --- |
| `construction_multibody_v1.forge` | 1264 | `8830e7fbd8dcb803535d5c4f91e410dfca4c7a0cecc1202c9d2b1aa8553b9aaf` |
| `sculpt_mixed_v1.forge` | 629 | `112b109731a43bf57a0f77b34794e7ce2529e056d9b18f061cd3c891f51a2784` |

So both ends are pinned: the input by a digest from a second encoder, the output
by a reader that is not the writer.

## The exported sentinels

| file | bytes | SHA-256 |
| --- | --- | --- |
| `construction_sentinel.glb` | 60312 | `266355a5dc2e58af626c2fb60b66420ad2c98e0d6eb1446925d8c5a3a5eaaf5f` |
| `sculpt_sentinel.glb` | 2684 | `16805a054491b3e65c4a0cec86dc3860667bee500b13a9a33d71eb04e1a517f1` |

**Deterministic.** `scripts\run-glb-export-evidence.ps1` was run twice, deleting
the on-device files first each time; both runs produced byte-identical files
(verified with `cmp`, not only by digest).
