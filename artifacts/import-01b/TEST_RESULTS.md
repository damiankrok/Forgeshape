# IMPORT-01B — test results

Build: `gradlew.bat :app:assembleDebug :app:assembleRelease`, both supported ABIs
(`arm64-v8a`, `x86_64`) present in both APKs. Device work on `emulator-5580`,
confirmed `ForgeShape_Stage006`; `emulator-5554` never contacted.

## Native self-tests — 17/17 suites, 2628 checks, zero failures

Raw capture: `selftest_startup.txt` (64 MiB ring buffer, confirmed with
`logcat -g` before the run).

| suite | checks | delta |
| --- | ---: | ---: |
| `FORGESHAPE_CAMERA_SELFTEST_OK` | 119 | — |
| `FORGESHAPE_PICKING_SELFTEST_OK` | 174 | — |
| `FORGESHAPE_DYNAMIC_MESH_SELFTEST_OK` | 91 | — |
| `FORGESHAPE_CONSTRUCTION_BOX_SELFTEST_OK` | 100 | — |
| `FORGESHAPE_CONSTRUCTION_TRANSFORM_SELFTEST_OK` | 121 | — |
| `FORGESHAPE_CONSTRUCTION_PRIMITIVE_SELFTEST_OK` | 125 | — |
| `FORGESHAPE_CONSTRUCTION_SPHERE_SELFTEST_OK` | 105 | — |
| `FORGESHAPE_CONE_CAPSULE_SELFTEST_OK` | 163 | — |
| `FORGESHAPE_SCULPT_BRUSH_KERNEL_SELFTEST_OK` | 410 | **+32** |
| `FORGESHAPE_RENDER_SHADING_SELFTEST_OK` | 329 | — |
| `FORGESHAPE_SCENE_SELFTEST_OK` | 79 | — |
| `FORGESHAPE_CONSTRUCTION_HISTORY_SELFTEST_OK` | 147 | **+33** |
| `FORGESHAPE_GIZMO_SELFTEST_OK` | 145 | — |
| `FORGESHAPE_PROJECT_SELFTEST_OK` | 219 | **+35** |
| `FORGESHAPE_RENDER_RECOVERY_SELFTEST_OK` | 24 | — |
| `FORGESHAPE_GLTF_EXPORT_SELFTEST_OK` | 93 | — |
| `FORGESHAPE_GLTF_IMPORT_SELFTEST_OK` | 184 | — |
| **total** | **2628** | **+100** |

Then `FORGESHAPE_NATIVE_VIEWPORT_OK`. No `*_SELFTEST_FAIL` and no `_FAIL:` line.

Both golden digest lines are printed whether their cases pass or fail:

```
FORGESHAPE_PROJECT_GOLDEN_SHA256_IMPORTED_SCULPT
  imported_sculpt=b82430cf6dbb82fddf075722d7ae335460f687d2a06cde09db43817323729f76
  mixed_imported_sculpt=ab709ecea27ec29f21b6fbef126e8cdc15dc5c733d9b751bd1c8832907f27a2b
```

## JVM — 70/70

`gradlew.bat :app:testDebugUnitTest --rerun-tasks`, 70 tests, 0 failures,
0 errors.

## Device guards

`scripts\verify-device-guards.ps1`, no device attached:
`DEV2-01..07` and `DEV3-01..06` all PASS.

## `.forge` corpus

`scripts\build-forge-corpus.ps1 -VerifyOnly`: every fixture on disk matches what
the independent PowerShell encoder produces. See `CORPUS.md` for the table — ten
digests unchanged, two new.

## Instrumented — focused

| class | tests | result | log |
| --- | ---: | --- | --- |
| `ImportedMeshSculptTest` (new) | 9 | OK | `instrumented-ImportedMeshSculptTest.txt` |
| `ObjectsDeleteTest` (new) | 8 | OK | `instrumented-ObjectsDeleteTest.txt` |
| `ImportedMeshDurableTest` | 12 | OK | `instrumented-ImportedMeshDurableTest.txt` |

## Instrumented — regression gates

| class | tests | result |
| --- | ---: | --- |
| `EditorWorkspaceSculptRetentionTest` | 2 | OK |
| `EditorWorkspaceHistoryTest` | 20 | OK |
| `EditorWorkspaceObjectsTest` | 10 | OK |
| `ProjectAutosaveRecoveryTest` | 14 | OK |
| `GlbExportTest` | 24 | OK |
| `GlbImportExternalR1Test` | 14 | OK |
| `GlbImportPreviewTest` | 23 | OK |
| `EditorWorkspaceGizmoTest` | 46 | OK |
| `EditorWorkspaceCorrectionTest` | 37 | OK |
| `EditorWorkspaceCompositionTest` | 10 | OK (run during development; also covered by the aggregate) |
| `EditorWorkspaceLayoutTest` | 10 | OK (run during development; also covered by the aggregate) |

Raw runner output for each is `instrumented-<class>.txt`.

## Full sharded aggregate — PASS

The authoritative exhaustive-sharded full-suite run, through
`scripts\run-instrumented-tests.ps1 -Serial emulator-5580 -FullSharded`, on the
final runtime/test tree. Raw log: `FULL_SHARDED.txt`.

```
DISCOVERY_PASS | classes=33 | tests=472 | shards=5 | missing=0 | duplicates=0 | unexpected=0
shard 1  93/93  PASS      shard 2  96/96  PASS      shard 3  92/92  PASS
shard 4  95/95  PASS      shard 5  96/96  PASS
assigned_union=472  executed_union=472
missing=0  duplicates=0  unexpected=0  execution_missing=0
failed_shards=0  aborted_shards=0
aggregate=PASS
FULL_SHARDED_SUITE_PASS
```

`ImportedMeshSculptTest` (9) ran in shard 4 and `ObjectsDeleteTest` (8) in shard
3 — both inside the aggregate, beside six other classes each, which is the
condition that caught the process-scoped-state defect.

**It ran against exactly the candidate tree.** The runtime/test fingerprint —
SHA-256 over every `.cpp`, `.h`, `.java` and `.xml` under `app/src/main/cpp`,
`app/src/main/java`, `app/src/androidTest` and `app/src/main/res` — was
`2b80120f8fa5cbd8c71eabe4dfab6bf553c9d307b0649a4323b72606aa9dbab8` before the
run and unchanged after it, and `sha256(git diff --binary HEAD)` was
`7ec4b478a3e68e7a59aa0c13845111d778d34c874cfa8b2306c35729fd81719d` on both
sides. Nothing but untracked evidence files moved.

### What it took to get there

Six attempts, and the history is kept because it is what justifies the aggregate
existing at all. Two were real defects it caught; four were device failures.

| # | Outcome | Classification | Action |
| --- | --- | --- | --- |
| 1 | shard 1 FAIL | **product defect** — unbounded GPU fence wait hung the render thread | bounded the wait; `ProjectAutosaveRecoveryTest` 14/14 |
| 2 | shard 3 FAIL | **test defect** — assumed process-scoped state | asked `validateProject` instead; shard 3's exact composition 92/92 |
| 3 | shard 4 ABORT | **device** — `system_server` died; no ForgeShape tombstone or crash-buffer entry | AVD restart |
| 4 | no discovery | **device** — installer wedged | diagnosed (see below) |
| 5 | no discovery | **device** — same wedge | recovery cycle 1: AVD restart, ineffective |
| 6 | **PASS** | — | recovery cycle 2: real guest reboot |

Both defects are written up in `DELETE_CONTRACT.md`. Each correction was proved
against the exact condition that exposed it before the aggregate was rerun, and
every rerun started **from shard 1** on the corrected tree — a class-only or
shard-only rerun is supplementary and repairs no aggregate.

## Focused acceptance tests — IMP01B-01..24

| ID | Requirement | Where | Result |
| --- | --- | --- | --- |
| IMP01B-01 | Imported Mesh exposes Start Sculpting | native sculpt suite (2), `ImportedMeshSculptTest.imp01b01_*`, `ImportedMeshDurableTest.imp01a14_*` | PASS |
| IMP01B-02 | initial Sculpt seed equals Imported source world geometry | native sculpt suite (10): bit-exact positions, exact indices, not the doubled draw buffer, two-sided collapse, counts, identity, real normals | PASS |
| IMP01B-03 | authored transform is not double-baked | native sculpt suite (2): transform untouched, and `model*imported[v] == model*sculpt[v]` for every vertex; `ImportedMeshSculptTest.imp01b02and03_*` | PASS |
| IMP01B-04 | real sculpt stroke changes Sculpt truth | native sculpt suite (2), `ImportedMeshSculptTest.imp01b04and05_*` through the whole touch path | PASS |
| IMP01B-05 | Imported source remains unchanged | native sculpt suite (5): positions, normals, indices, batches, transform — all `operator==`; on device via the `.forge` `IMPT` bytes | PASS |
| IMP01B-06 | sculpt stroke Undo/Redo | `ImportedMeshSculptTest.imp01b06_*`: the pair is withdrawn in Sculpt, refused below JNI, and a Construction undo moves no sculpted vertex | PASS |
| IMP01B-07 | Back to Imported Mesh restores source | native sculpt suite (3), `ImportedMeshSculptTest.imp01b04and05_*` incl. the label and content description | PASS |
| IMP01B-08 | Resume Sculpt restores retained sculpt | native sculpt suite (3), `ImportedMeshSculptTest.imp01b04and05_*`, and after a `.forge` reload | PASS |
| IMP01B-09 | destructive reset confirmation protects edits | native sculpt suite (5), `ImportedMeshSculptTest.imp01b09_*`: representation-aware label, the dialog, the consequence wording, cancel changes nothing | PASS |
| IMP01B-10 | no Construction Source fabricated/accessed | native sculpt suite (2), `ImportedMeshSculptTest.imp01b10and24_*`: still imported, Shape still refused below JNI, rail entry still absent | PASS |
| IMP01B-11 | IMPT+SCUL deterministic `.forge` roundtrip | native project suite (22): validity, digest, bit-for-bit roundtrip, live load, recapture, fingerprint; `ImportedMeshSculptTest.imp01b11and13_*` | PASS |
| IMP01B-12 | mixed source/sculpt project roundtrip | native project suite (12): all four combinations in one document, loaded and recaptured to identical bytes | PASS |
| IMP01B-13 | Save Copy/autosave/recovery preserve imported sculpt | `ImportedMeshSculptTest.imp01b11and13_*`: checkpoint is the same canonical document, copy is the same bytes, reload re-encodes identically | PASS |
| IMP01B-14 | export uses correct effective current geometry | `ImportedMeshSculptTest.imp01b14_*`: sculpted in Sculpt, source outside it, and exporting records no history step and moves no slot | PASS |
| IMP01B-15 | Delete Construction body is one real mutation | native history suite (7), `ObjectsDeleteTest.imp01b15and23_*` | PASS |
| IMP01B-16 | Delete Imported body is one real mutation | native history suite (3), `ObjectsDeleteTest.imp01b16and17_*` | PASS |
| IMP01B-17 | Delete retained-Sculpt body removes all active truth | native history suite (2), `ObjectsDeleteTest.imp01b16and17_*`: no orphan `IMPT` and no orphan `SCUL` survive | PASS |
| IMP01B-18 | Delete Undo restores exact body | native history suite (9): same sculpt revision/counts, same published revision, same imported geometry and name; on device by whole-document comparison | PASS |
| IMP01B-19 | Delete Redo removes same body | native history suite (3), `ObjectsDeleteTest.imp01b18and19_*` | PASS |
| IMP01B-20 | active fallback deterministic | native history suite (4): next row, previous when last, unchanged when inactive; `ObjectsDeleteTest.imp01b20_*` incl. no stale Resume Sculpt | PASS |
| IMP01B-21 | last-body behavior safe and named | native history suite (5): `RefusedLastBody` by name, nothing changed, plus the unknown-id and open-edit refusals; `ObjectsDeleteTest.imp01b21_*` ×2 incl. the Sculpt withdrawal | PASS |
| IMP01B-22 | deleted body absent from render/pick/save/export/recovery | `ObjectsDeleteTest.imp01b22_*`: pick before/after, `.forge`, checkpoint, GLB, and Undo restores all four | PASS |
| IMP01B-23 | Delete UI IDs/hit target/R2 geometry correct | `ObjectsDeleteTest.imp01b15and23_*`: semantic id, ObjectId tag, distinct from the label, 48 dp in both dimensions, label still reachable | PASS |
| IMP01B-24 | forbidden scope remains absent | `ImportedMeshSculptTest.imp01b10and24_*` (four brushes, no OBJ/FBX, one import control), `ObjectsDeleteTest.imp01b24_*` (no rename/duplicate/hide/lock/group) | PASS |

### The third attempt — an infrastructure abort, and what proved it was one

The third aggregate reached shard 4 and stopped with:

```
com.forgeshape.app.ImportedMeshSculptTest:........INSTRUMENTATION_ABORTED: System has crashed.
SHARD_RESULT | shard=4 | expected=95 | actual=0 | status=INSTRUMENTATION_ABORT
```

Shards 1, 2 and 3 had passed 93/93, 96/96 and 92/92 — including the two shards
that had caught the earlier defects.

**"System has crashed" is not "the app crashed", and the difference was checked
rather than assumed:**

- `logcat -b crash` showed `DeadSystemException: The system died` for
  `com.android.phone` and `com.android.systemui` — both are *victims* of a
  `system_server` death, not causes;
- **no ForgeShape entry in the crash buffer at all**;
- **no tombstone from that day.** The newest was `tombstone_22`, dated four days
  earlier. A native crash in `libforgeshape_native.so` would have left one.

So `system_server` died on its own, after roughly four hours of continuous
instrumentation on one emulator, and took the instrumentation with it.

`CLAUDE.md` prescribes exactly one response, and it was followed literally:
**recover the isolated AVD and rerun the whole `-FullSharded` from shard 1.** A
shard-only or class-only rerun is supplementary and cannot repair an aggregate,
so none was attempted as a substitute.

Recovery, in order: `emu avd name` confirmed `ForgeShape_Stage006` **before** the
kill; `emu kill` scoped to `-s emulator-5580`; `emulator-5554` never contacted;
restart through `scripts\start-forgeshape-emulator.ps1`, which re-confirmed the
AVD by name rather than by port; `sys.boot_completed=1` checked; the 64 MiB log
ring restored. Then the whole command again from shard 1.

### The install hang, and its actual root cause

Attempts 4 and 5 never reached discovery: the runner installed the app APK and
then blocked indefinitely. The device was idle throughout — no instrumentation
process, no ForgeShape process, no log output. It was diagnosed rather than
guessed at, with bounded probes so no single call could block for an hour again:

| probe | result | what it ruled out |
| --- | --- | --- |
| `adb shell ps`, `getprop`, `pm list packages` | instant | the transport is alive |
| host RAM / disk | 39.5 GB free, 44 GB free | host exhaustion |
| `/data` usage | 90% used, 614 MB free; tombstones 19 MB | disk space |
| `adb push` of the same APK | **64 MB/s, 0.031 s** | bulk transfer |
| `adb install` | hangs > 180 s | — |
| `adb shell pm install` on the pushed file | hangs, **no PackageManager log at all** | it is device-side, in the installer |
| restart the AVD via the sanctioned launcher | install still hangs | a plain restart |

The decisive observation came from the process table after that restart:
**`system_server` still had pid 12886 — the same pid as before it.** The
sanctioned launcher passes no `-no-snapshot-load`, so the emulator had *resumed a
snapshot* carrying the wedged system, PIDs and all. The restart had not rebooted
anything.

**Recovery:** `adb -s emulator-5580 reboot` — a real guest reboot, scoped to our
serial, using no launcher and touching nothing shared. `system_server` came back
as **pid 753**, and the same install that had hung four times returned
`Performing Streamed Install / Success` immediately.

`adb kill-server` was never used: another program owns a reserved emulator
session on the same host daemon, and restarting it would have disrupted that.
`emulator-5554` was never contacted at any point.
