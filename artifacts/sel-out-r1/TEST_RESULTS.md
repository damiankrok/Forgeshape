# Test results — SEL-OUT-R1

Baseline `538623c26e0122d97303150ebbb52eb3ea0d32b3`, clean tree at start.
Every device gate on `emulator-5580` = AVD `ForgeShape_Stage006`, confirmed by
`emu avd name` before each run. `emulator-5554` was never contacted; the log
buffer was raised to 64 MB before the startup capture, confirmed with
`logcat -g`.

---

## 1. Native self-tests, real debug launch

```
adb -s emulator-5580 logcat -G 64M
adb -s emulator-5580 install -r app\build\outputs\apk\debug\app-debug.apk
adb -s emulator-5580 shell am start -n com.forgeshape.app/.ForgeShapeActivity
```

All **twenty** `*_SELFTEST_OK` tokens in emission order, then
`FORGESHAPE_NATIVE_VIEWPORT_OK`. **Zero** `_SELFTEST_FAIL`, `_FAIL:` or
`CASE_FAIL` lines.

`FORGESHAPE_RENDER_SHADING_SELFTEST_OK (412 checks)` — up from 345. The suite
gained 71 `seloutr1_*` checks and its four resting-tint assertions were
rewritten, because the resting tint is now zero.

The renderer also logs its outline resources once per swapchain:

```
FORGESHAPE_SELECTION_OUTLINE_RESOURCES:1080x2400 allocations=1 width=3.02px
FORGESHAPE_SELECTION_OUTLINE_RESOURCES:1080x2400 allocations=2 width=3.02px
```

Two, because the first attach is followed by one swapchain rebuild at startup —
the same shape the depth and pipeline resources have.

## 2. Native standalone runner (fast loop)

The render-shading suite compiled for x86_64 with the NDK clang and run directly
on the device, outside Gradle:

```
RENDER_SHADING 412 checks, 0 failed (71 seloutr1)
```

The 71 new checks cover: the band width policy (bounds, the floor and ceiling,
orientation independence, degenerate viewports, and that the camera is not an
input); the five per-ground contrast ratios measured with WCAG arithmetic; that
the three dark grounds share one colour, the two light grounds share another,
and the two differ; the edge-extraction kernel (interior never painted, the band
present just outside, the band's width, nothing beyond it, an empty mask
painting nothing anywhere, a half-occluded mask outlined only on its visible
half with **no** x-ray on the hidden half, the window edge, and null/zero/
out-of-range inputs); and the toggle's ownership, default, idempotence and
inertness against a real published mesh and a real two-body scene.

## 3. Focused device suite

```
scripts\run-instrumented-tests.ps1 -Serial emulator-5580 `
  -TestClass com.forgeshape.app.SelectionOutlineTest
```

→ **OK (19 tests)**, `MODE=FOCUSED_SUBSET (not full-suite evidence)`.

Two of the nineteen are driven end to end the way §19 asks — through real
`MotionEvent`s and real controls rather than through the domain call:

* `e2eSeloutr1_02_03_aRealViewportTapMovesTheOutline` dispatches a genuine
  single-finger tap to the viewport, lets CPU picking resolve it, and asserts
  the selection and the outline both moved to the tapped body and back. The
  screen point is asked of the product's own projection (the gizmo's pivot is
  the body's placement origin) rather than written down, because `CLAUDE.md`
  forbids locating anything by coordinate;
* `e2eSeloutr1_04_theObjectsListMovesTheOutline` clicks the body's row in the
  Objects list and asserts the same.

Both start from a **fresh one-body project**. A tap resolves against everything
on screen and the Objects list has a row per body, so a scene still carrying
bodies an earlier case left behind can put a third body over the point the tap
aims at — a failure about scene population, not about the outline. This was
found on the device: the tap case passed alone and failed in class order until
the scene was made to match what the case claims.

Measured performance line from that run:

```
FORGESHAPE_SELOUTR1_PERFORMANCE selectionChanges=40
  outlineAllocationsDuringLoop=0
  compositeDrawsWithOutlineOn=44 compositeDrawsWithOutlineOff=0
  meshRevisionBefore=25 meshRevisionAfter=25 fingerprintMoved=false
  maskExtent=1080x2400 bandHalfWidthPx=3.02 twentyOutlinedSwitchesMillis=814
```

## 4. Visual evidence journey

```
scripts\collect-sel-out-evidence.ps1 -Serial emulator-5580
```

→ **OK (1 test)**, twelve frames, `VISUAL_EVIDENCE.md` and
`OWNER_CONTACT_SHEET_SEL_OUT_R1.png` composed.

The suite **asserts** as well as captures: a band must be present in the eleven
frames where one should be, absent in `02_construction_outline_off.png`, its
measured thickness between 1 and 8 px, and **zero** pixels of the other ground
family's colour in any frame. Median measured thickness: **3 px** in every
outlined frame, against the renderer's reported 3.02 px half-width.

## 5. JVM

```
gradlew.bat :app:testDebugUnitTest      →  BUILD SUCCESSFUL
```

## 6. Builds

```
gradlew.bat :app:assembleDebug          →  BUILD SUCCESSFUL (arm64-v8a, x86_64)
gradlew.bat :app:assembleDebugAndroidTest → BUILD SUCCESSFUL
gradlew.bat :app:assembleRelease        →  BUILD SUCCESSFUL (arm64-v8a, x86_64)
```

**F-16 still holds.** In the release `arm64-v8a` `.so`: `llvm-nm --defined-only`
finds **0** `run*SelfTests` symbols, and `llvm-strings` finds **0** `seloutr1_`
check-name strings. The stripped release libraries are 1 196 232 (arm64) and
1 263 416 (x86_64) bytes.

## 7. Device guards

```
scripts\verify-device-guards.ps1        →  All device-guard checks PASS.
```

`DEV2-01..07` and `DEV3-01..06`, over **19** executable surfaces including the
new `scripts\collect-sel-out-evidence.ps1`. `DEV2-06` confirms no bare, unscoped
`adb` invocation exists across the other 15 scripts.

## 8. `.forge` corpus

```
scripts\build-forge-corpus.ps1 -VerifyOnly
```

All twenty-eight fixtures listed with digests matching the values the C++
self-tests assert, and `git status` over the corpus is **clean**. This stage
touched no project-state code — no codec, no document, no history, no scene, no
CAD and no sculpt file appears in its diff — so the corpus could not have moved,
and this confirms it did not.

## 9. Delete / history non-collision

Not one file behind Delete, the Construction history, the project codec, CAD or
Sculpt is in this stage's diff:

```
git status --short | grep -E "body_delete|history|project_document|project_bytes|
                              project_state|cad_|sketch|scene.cpp|
                              construction.cpp|sculpt"
→ (no matches)
```

`SelectionOutlineTest.seloutr1_23_24_25_36_...` additionally drives the real
thing on the device: Delete removes exactly one body and records exactly one
Undo step, selection falls to the surviving body by the **existing** rule, Undo
restores it, Redo removes it again, and the outline renders whatever selection
is at each point. **This is non-collision evidence and is explicitly NOT the
pending OWNER Delete verdict of `IMPORT-01B` / `UI-OWNER-45`.**

## 10. Full-suite aggregate (`TEST-RUNTIME-R1`)

Run once, fresh, on the stable tree after every focused gate above was green:

```
scripts\run-instrumented-tests.ps1 -Serial emulator-5580 `
  -FullSharded -ShardCount 5 -Fresh
```

Fingerprint `9208c9fa0ac3` (app `f0deeb5d13d0`, test `9a378b27f08a`, device
`emulator-5580` / `avd:ForgeShape_Stage006`), `AGGREGATE_ATTEMPT | attempt=1 of
2 for this fingerprint`.

**Two earlier aggregates were started and deliberately stopped before
completing, and neither is claimed as evidence.** Both were abandoned because
the source tree was still moving under them, which is exactly what
`TEST-RUNTIME-R1` says invalidates a run:

1. the first reached `SHARD_RESULT | shard=1 | 112/112 | PASS | 729.4s` and was
   stopped when the documentation pass found two comments in
   `forgeshape_renderer.h` asserting an invariant the outline breaks ("the only
   sampled image", "the single descriptor set"). Correcting them changes the app
   APK and therefore the fingerprint;
2. the second was stopped almost immediately, when reviewing §19 showed the
   suite switched selection through the domain call rather than through a real
   `MotionEvent` — a genuine gap, closed by the two `e2eSeloutr1_*` cases.

Each stop reset the attempt counter, because a changed APK is a new
fingerprint — so this run is attempt 1 on the final tree, not a third try at the
same one. The lesson is recorded plainly: the aggregate is the **last** gate and
the tree must be frozen before it starts.

### Result

```
SHARD_RESULT | shard=1 | 112/112 | PASS | 725.1s
SHARD_RESULT | shard=2 | 112/112 | PASS | 617.6s
SHARD_RESULT | shard=3 | 112/112 | PASS | 711.4s
SHARD_RESULT | shard=4 | 112/112 | PASS | 699.2s
SHARD_RESULT | shard=5 | ASSERTION_FAILURE | PRODUCT_TEST_FAILURE | 694.1s
FULL_SHARDED_SUITE_FAIL
```

Elapsed ≈ **58 minutes**, inside the 90-minute target and well inside the
120-minute stop. Shard 4 carries `SelectionOutlineTest`,
`EditorWorkspaceDisplayTest`, `ObjectsDeleteTest`, `ImportedMeshDurableTest` and
`CadA3VisualEvidenceTest`; shard 1 carries
`SelectionOutlineVisualEvidenceTest`. **All of this stage's own tests are in
shards that passed.**

Shard 5's two failures are both in `SpatialSketchTest` and are **proven to
pre-date this stage**: the minimal repro `GlbImportPreviewTest,SpatialSketchTest`
reproduces them identically at the clean baseline `538623c2` with all of this
stage's work stashed and the APKs rebuilt from it. The nine-step bisection is in
`PREEXISTING_SHARD5_FAILURE.md`.

`FULL_SHARDED_SUITE_PASS` is therefore **not claimed and is not obtainable by
this stage** — it requires every shard, and shard 5 fails without this stage's
code. No shard log was concatenated into a PASS and no result criterion was
weakened, which is what `TEST-RUNTIME-R1` and the stage brief both require.

Full transcript: `FULL_SHARDED_RUN1.txt`.
