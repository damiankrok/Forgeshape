# The milestone aggregate, capture synchronization corrected (CAD-VS-FULLSHARDED-C4)

**Result: `PASS-CAD-VS-FULLSHARDED-C4`.**

- **The capture race is corrected, test-only.** Each of the twelve
  `SelectionOutlineVisualEvidenceTest` captures now waits for six newly
  presented renderer frames. If they do not arrive, the test fails before the
  screenshot is taken.
- **The fresh aggregate passed on attempt 1:** 57 classes / 629 tests, all five
  shards, `FULL_SHARDED_SUITE_PASS`. Shard 5 ran in a cloud aggregate for the
  first time.
- **`main` was fast-forwarded** to this closeout (§9).

## 1. Refs

| Ref | SHA |
| --- | --- |
| Branch HEAD at start (C3 docs closeout) | `5db97d00cc431d7e123ef8f9d782641cf7cdfc35` |
| `origin/main` at start, and still at the integration check | `103aa22e6367e3556bb27f2a2946cdab1bbaf2bd` |
| C3 tested candidate | `899986d25f54ad989971b1766b49037366c16833` |
| C2 corrected candidate (the last product-source change) | `5f1eccf8c336d51343aaa62fa11b9f516252acfb` |
| **C4 tested candidate (test-only)** | **`fa0b6b415f7c1b4670788e161b8eee71f10e244d`** |

**Preflight.**

- The tree was clean and every ref matched.
- `origin/main` was an ancestor of the branch.
- Everything from `899986d` to `5db97d0` was docs and evidence only:
  `PROJECT_STATUS.md` plus three files under this directory. Its diff over
  `app/src/main`, `testdata`, `DATA_PACKAGE_SPEC.md`, `third_party/manifold`
  and the Gradle files was empty.

## 2. Root cause: a timer, not a synchronization

The old `capture()` authorized every screenshot this way:

```java
settleLayout();
SystemClock.sleep(500);  // a few more frames, so the viewport has drawn the state
UiAutomation.takeScreenshot();
```

- **`settleLayout` only covers the views.** It waits for the Android layout to
  settle, and says nothing about the Vulkan viewport.
- **The sleep assumed a frame rate.** It assumed the renderer would present
  the new state within 500 ms.
- **On the CI emulator that is false.** SwiftShader presents about three frames
  a second through a four-image FIFO swapchain, so a frame recorded before the
  state change can still be on the display 500 ms later.
- **That is how C3 failed.** Its aggregate failed at `09_warm_light` after
  native had logged the ground change
  (`FORGESHAPE_VIEWPORT_BACKGROUND:WarmLight … changed=1`). The scan looked
  for the light-family outline colour in a frame still showing the dark
  ground.
- **The same code passed on replay** (`FULLSHARDED_C3.md` §7).
- **The same mechanism was finding A2.** It was measured and fixed for
  `CadVerticalSliceTest` in `db8ff6b` (`TEST_EVIDENCE.md` §3).

## 3. The correction

**One file changed:** `app/src/androidTest/java/com/forgeshape/app/SelectionOutlineVisualEvidenceTest.java`.
The fixed sleep is replaced by `awaitPresentedFrames(name)`:

| Item | Value |
| --- | --- |
| Counter | `NativeViewport.debugRendererFramesPresented()`. It is process-monotonic since C2 and continues across a render-thread restart |
| Frames required | **6** new presented frames after the state change. That covers the frame that may have been recorded just before the change, the first one after it, and the FIFO depth. It is `CadVerticalSliceTest`'s value |
| Timeout | 15 000 ms |
| Polling | `SystemClock.sleep(25)` between reads. It does not busy-spin |
| On timeout, or a counter below its start | **`AssertionError` before the screenshot.** The message carries the capture name, `start`, `latest`, `delta`, `required`, `elapsed_ms`, `timeout_ms`, `renderer_lifecycle`, `viewport_background_index` and `selected_object_id`, and the same line is written to `facts.txt` as `presented_frames_wait=FAILED …` |
| Evidence per capture | `presented_frames_start`, `presented_frames_final`, `presented_frames_delta`, `presented_frames_wait_ms` in `facts.txt`, ahead of that capture's `capture=` line |

**Why it fails closed.** `CadVerticalSliceTest` records a timeout, because its
captures are illustration and its claims are asserted from native state. This
class asserts PIXELS, so an unvouched frame would be an unvouched assertion.

**All twelve captures use it.**

- **One path.** The two `capture(...)` overloads end in the one
  `capture(String, boolean)`, whose first statements are `settleLayout()` and
  then `awaitPresentedFrames(name)`.
- **Nothing bypasses it.** No other screenshot call exists in the class
  (`takeScreenshot` appears once).
- **Proved on device.** Both device runs recorded a 6-frame (or 7-frame) delta
  for all twelve (§5).

**What did not change.**

- The two authored colours and `MATCH_TOLERANCE = 26`.
- `matchedPixels > 0` where a band is expected, and `== 0` for capture 02.
- The median band thickness bound `1..8` px.
- `otherFamilyPixels == 0`.
- All twelve captures and their order, and every scene fact.
- The left-handed View/Overlay facts and the Delete-fallback facts.
- No `@Ignore`, `Assume`, retry, tolerance change or larger fixed sleep.

`git diff 899986d..fa0b6b4` touches no line of `measureBand`, `isNear` or the
journey.

## 4. Device-free checks (on `fa0b6b4`)

- `git diff --check`: clean.
- `:app:compileDebugAndroidTestJavaWithJavac`: green (local).
- `test-instrumented-runtime.ps1`: TESTRUNTIME-01..26 PASS.
- `test-instrumented-sharding.ps1`: THR1-01..10 PASS.
- `verify-device-guards.ps1`: all PASS. It ran through the same
  `powershell.exe` → `pwsh` shim as C3.
- **`CI FAST` `36603387133`, attempt 1: success.** Its steps:
  - whitespace;
  - the debug, release and androidTest builds;
  - JVM;
  - the release self-test guard;
  - `.forge` corpus parity;
  - the runner and device-guard checks.

## 5. Focused device gates (on `fa0b6b4`)

### 5a. `SelectionOutlineVisualEvidenceTest` alone: `CI DEVICE` `36603390799`

**PASS:** 23/23 startup tokens in order, `NATIVE_VIEWPORT_OK`, 0 failure
lines, **OK (1 test)**, 61 s. `captures=12`, 12 PNGs.

| Capture | Ground | Frames (Δ) | Wait | Median band | Matched px | Other family |
| --- | --- | --- | --- | --- | --- | --- |
| 01 construction, dark | 0 | 6 | 1663 ms | 3 | 1146 | 0 |
| 02 outline off | 0 | 6 | 157 ms | 0 | **0** | 0 |
| 03 imported | 0 | 6 | 1619 ms | 3 | 3286 | 0 |
| 04 sculpt | 0 | 6 | 1565 ms | 3 | 1143 | 0 |
| 05 CAD | 0 | 6 | 1540 ms | 3 | 1292 | 0 |
| 06 occluded | 0 | 6 | 1776 ms | 3 | 1012 | 0 |
| 07 two bodies, A | 0 | 6 | 1512 ms | 3 | 2107 | 0 |
| 08 two bodies, B | 0 | 6 | 1754 ms | 3 | 1169 | 0 |
| **09 Warm Light** | **3** | 6 | **1687 ms** | 3 | 1169 | **0** |
| **10 Cool Light** | **4** | 6 | **1478 ms** | 3 | 1169 | **0** |
| 11 left-handed overlay | 0 | 6 | 1610 ms | 3 | 1132 | 0 |
| 12 after Delete | 0 | 6 | 2184 ms | 3 | 2094 | 0 |

**What the table shows.**

- **No wait timed out.**
- **The typical wait is 1.5–2.2 s, three to four times the old fixed delay.**
- **09 and 10 carry the light-family band.** It is 3 px wide, with zero
  dark-family pixels.
- **02 still proves the outline OFF.**
- **One short wait.** 02's 157 ms follows a toggle made while frames were
  already queued, and it still counted six frames presented after its start.

### 5b. The exact C3 shard-2 sequence: `CI DEVICE` `36604540195`

The filter was recovered from the C3 aggregate's own `runner-output.txt`
(`SHARD_START | shard=2/5`), not from memory. These are the 10 classes in the
runner's order:

`CadVerticalSliceTest`, `EditorWorkspaceArchitectureTest`,
`EditorWorkspaceCorrectionTest`, `GlbImportExternalR1Test`,
`JniBoundaryHardeningTest`, `SculptUndoTest`, `SelectionOutlineTest`,
`SelectionOutlineVisualEvidenceTest`, `Ui3dStateAuditTest`,
`Ui3dStateCorrectionTest`.

**PASS:** 23/23 startup tokens, 0 failure lines, **OK (126 tests)**, 1066 s,
first attempt.

- **Per class:** 7 + 12 + 37 + 14 + 5 + 10 + 19 + 1 + 9 + 12 = 126 in the
  JUnit XML, each test once.
- **The visual case** recorded a 6- or 7-frame delta for every capture, in
  187–1644 ms, and no timeout.

## 6. The authoritative aggregate: `CI FULL SHARDED` `36607747079`, mode `fresh`

| Field | Value |
| --- | --- |
| Tested SHA | `fa0b6b415f7c1b4670788e161b8eee71f10e244d` |
| Boot proof | `BOOTED`: 23/23 tokens in order, `NATIVE_VIEWPORT_OK`, 0 failure lines. Capture 2 was used: the first was a liblog-proven drop and was relaunched by the documented procedure |
| Discovery | `DISCOVERY_PASS`: 57 classes / 629 tests, 5 shards, missing 0, duplicates 0, unexpected 0 |
| Inventory / partition | `06ecaec7c14b` / `a4a0b57ed4b7`, identical to C2 and C3 (only a method body changed) |
| **Fingerprint** | **`e387ad721be4`**: a new one, because the test APK changed |
| App APK SHA-256 | `03899ce005f463d25031ca604c1202cb09ee6f62d6553bbb05255dc89fe754b6` |
| Test APK SHA-256 | `42b19f48d7e4d795a150d27dcf3d3d1a4bc6986a116a2f9e8cbc428e998d33bf` |
| Device | `emulator-5580`, `avd:ForgeShape_CI_API36` |
| Attempt | **1 of 2** for this fingerprint, counted; no checkpoint was carried |
| Elapsed | **65.65 min** (target 90, hard stop 120) |

| Shard | Classes / tests | Result | Duration |
| --- | --- | --- | --- |
| 1 | 11 / 126 | PASS 126/126 | 710.3 s |
| 2 | 10 / 126 | PASS 126/126 | 909.4 s |
| 3 | 12 / 126 | PASS 126/126 | 811 s |
| 4 | 12 / 126 | PASS 126/126 | 751.5 s |
| 5 | 12 / 125 | PASS 125/125 | 757 s |

- **Union:** assigned 629, executed 629.
- **Integrity:** missing 0, duplicates 0, unexpected 0, execution missing 0,
  failed shards 0, aborted shards 0.
- **Verdict:** `aggregate=PASS`.
- **Marker: `FULL_SHARDED_SUITE_PASS`.**
- **The job:** every step succeeded, including the log-capture cleanup that
  hung in C3 §6a.

## 7. Product and data neutrality

`git diff --stat 899986d..fa0b6b4` lists:

- the one test file above;
- the four C3 docs and evidence files.

`git diff 899986d..fa0b6b4` over the product paths is **empty (0 bytes)**:
`app/src/main`, `testdata`, `DATA_PACKAGE_SPEC.md`, `third_party/manifold`,
`build.gradle`, `settings.gradle`, `gradle.properties` and `app/build.gradle`.
The same range changes nothing under `scripts/`, `.github/` or `gradle/`.

It follows that nothing product-side moved:

- no `CADB` fixture;
- no Manifold source;
- no renderer, shader or palette value;
- no `.forge` format;
- no Sculpt or stylus work;
- no CAD feature.

## 8. Incidents

**None this run.**

- No infrastructure retry was needed: every run passed on its first attempt.
- The only irregularity was the boot proof's first startup capture. liblog
  proved it a drop, and the procedure relaunched it.

## 9. Integration

- **The closeout.** This record and the status updates are ONE docs/evidence
  commit (`[skip ci]`) on top of `fa0b6b4`. Its diff against `fa0b6b4`
  touches only `PROJECT_STATUS.md` and `artifacts/cad-vertical-slice-r1/`.
- **The fast-forward.** `origin/main` was re-fetched and was still `103aa22`,
  an ancestor of the branch, and `main` was fast-forwarded to the closeout
  commit with a normal push: no force, no rebase, no merge commit.

## 10. Remaining debt

- **Owed:** A3, A4, A5 and A6 (`TEST_EVIDENCE.md`).
- **Cloud resume.** It is still refused by fingerprint, because every VM
  signs with a fresh debug key.
- **The `CI FULL SHARDED` cleanup step has no timeout of its own.** It did not
  hang this time, but C3 §6a is unchanged.
- **OWNER review.** The OWNER-review APK is the coordinator's next task, not
  this one's.

No next feature was started.
