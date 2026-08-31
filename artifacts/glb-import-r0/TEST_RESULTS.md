# GLB-IMPORT-R0 — test and build results

Every run is on the ForgeShape-owned isolated AVD `ForgeShape_Stage006`, serial
`emulator-5580`, identity confirmed with `adb -s emulator-5580 emu avd name`.
The reserved `emulator-5554` was never contacted. Every device call went through
`scripts\run-instrumented-tests.ps1 -Serial emulator-5580` or an explicit
`adb -s emulator-5580`; no bare `adb`, no device enumeration, no unscoped
`connected*AndroidTest`.

## The corrected sentinels these cases read

Unchanged from E2E-R1C-C1, and confirmed at preflight:

| file | bytes | SHA-256 |
| --- | --- | --- |
| `construction_sentinel.glb` | 59884 | `8ac4fb6abb509361b911c9b4bbc2dd8385910668808b40fbd9faeab244f2c3ff` |
| `sculpt_sentinel.glb` | 2588 | `ae82a0720d9f503f77d0237edf40ab5c1152bae6938df08166e1ea9b31014f2c` |

They ship in the androidTest APK at `app/src/androidTest/assets/glb/` as
byte-identical copies, so the cases read the exact bytes in the evidence bundle.

## Native self-tests — 17 suites, 2379 checks, 0 failures

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
| `FORGESHAPE_GLTF_EXPORT_SELFTEST_OK` | 93 |
| **`FORGESHAPE_GLTF_IMPORT_SELFTEST_OK`** (new) | **87** |

then `FORGESHAPE_NATIVE_VIEWPORT_OK`. No `_SELFTEST_FAIL` and no `_FAIL:` token
appears in the capture (`selftest_startup.txt`).

The new suite covers three things. The **JSON reader** on its own: ordinary
documents, missing and wrong-typed members, and the refusals that matter —
`nan`, `Infinity`, `1e400`, a trailing comma, a leading zero, an unterminated
string, a second document, and nesting bounded at depth 15 rather than allowed
to recurse the stack away. The **parser**, against a real export: the container
walked from its own headers, POSITION/NORMAL/index decoding checked against the
authored box, node translation applied exactly with no axis conversion, and
sixteen unsupported features each edited into that real export one at a time and
refused by their own names. The **roundtrip and the preview**: equivalence for
Construction and Sculpt, a deliberate disagreement reported as a mismatch, and
the preview's boundaries — reserved renderer keys, no body added, no
representation, a refused load leaving the previous preview whole, and Clear
releasing everything.

## Instrumented — focused

| class | result |
| --- | --- |
| `GlbImportPreviewTest` (new: `GLBIR0-01..20`, `E2E-GLBIR0-01..08`, 4 evidence cases) | **OK (22 tests)** |
| `EditorWorkspaceCorrectionTest` (UI-R4B) | OK (37 tests) |
| `EditorWorkspaceChromeCompositionTest` (UI-R4C) | OK (12 tests) |
| `EditorWorkspaceControlsTest` | OK (23 tests) |
| `ProjectTransferTest` (E2E-R1B transfer, `FSR1B-18`) | OK (13 tests) |
| `EditorWorkspaceGestureTest` | OK (6 tests) |
| Shard 5's exact class set, run together | OK (85 tests) |

`MODE=FOCUSED_SUBSET` is printed for each; none can emit a full-suite marker and
none is offered as one.

### One defect the full suite caught, the wrong fix, and the right one

The first full-suite run failed one case:
`EditorWorkspaceGestureTest.ui11_theImeLeavesTheFieldAndTheCommitPathUsableAndTheSurfaceUntouched`,
"the inspector must sit clear of the keyboard".

The cause was mine and real. Preview mode withdraws every editing control, and
the Property Inspector was withdrawn with `inspector.setVisibility(GONE)` — but
the inspector is an `AnchoredSurfaceView` that owns its own visibility **as its
open/closed state**, and nothing else in the workspace ever writes that field.
So the GONE was an override with no owner to undo it: once a preview had been
shown, the inspector was stuck gone for the rest of the process, and a later
case that expected it to move with the IME found it in the wrong place.

**The first fix was wrong and the second run said so.** Restoring
`inspector.setVisibility(VISIBLE)` on the non-preview path made it worse: that
runs on every sync, in every window, and forcing an anchored surface visible is
forcing it OPEN. Shard 1 — which contains no preview case at all — then failed
three ways, in `EditorWorkspaceGizmoTest` and
`EditorWorkspaceRightHostPlacementTest`, with the Objects capsule in the wrong
place and the placement editor unreadable. A fix that breaks a shard which never
touches the feature is a fix aimed at the symptom.

The right fix is to stop overriding a field this view owns. The preview branch
CLOSES the inspector — `setPrecisionOpen(false)`, the same call the product uses
everywhere else — and sets no visibility on it. Every other view the method
hides is one `syncFromNative` rewrites on the way past, which is why hiding
those is safe and hiding this one was not; the comment in the code says so, so
the next person does not repeat it.

`GlbImportPreviewTest`, `EditorWorkspaceGizmoTest`,
`EditorWorkspaceRightHostPlacementTest` and `EditorWorkspaceGestureTest` then
passed together (87 tests) before the aggregate was re-run.

## Instrumented — authoritative exhaustive-sharded full suite

`scripts\run-instrumented-tests.ps1 -Serial emulator-5580 -FullSharded`. The
runner's complete output is `FULL_SHARDED.txt`. The reported aggregate is the
one taken **after** the fix above, on the shipped tree; the failing first run is
described rather than hidden, and was not reported as a result.

## Host-side

| check | result |
| --- | --- |
| `:app:testDebugUnitTest` (JVM, 70 tests) | BUILD SUCCESSFUL |
| `:app:assembleDebug` — `arm64-v8a` + `x86_64` | BUILD SUCCESSFUL, both ABIs present |
| `:app:assembleRelease` — `arm64-v8a` + `x86_64` | BUILD SUCCESSFUL, both ABIs present |
| `scripts\verify-device-guards.ps1` (DEV2-01..07, DEV3-01..06) | All PASS — 12 executable surfaces scanned, including the new evidence script |
| `scripts\build-forge-corpus.ps1 -VerifyOnly` | all 7 fixtures match their pinned digests |
