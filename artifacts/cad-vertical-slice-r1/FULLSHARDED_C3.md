# The milestone aggregate, Mirror isolation corrected (CAD-VS-FULLSHARDED-C3)

**Result: `FAIL-CAD-VS-FULLSHARDED-C3`.**

- **The Mirror isolation defect is corrected.** `MirrorSmokeTest` now opens its
  own one-body Construction project and asserts the preconditions `MIRROR-01`
  requires. It passed alone, and inside the exact C2 shard-4 sequence (126/126)
  right after the class that leaves a Frozen Sculpt Mesh behind.
- **The fresh aggregate then failed at shard 2**, on a test that passed there in
  C2: `SelectionOutlineVisualEvidenceTest.e2eSelOutR1Vis_theWholeJourneyInTwelveCaptures`
  (§7). It is a fixed-delay screenshot race, not this correction's, and it was
  diagnosed and not fixed (prompt §11, C3-15).
- **Not merged:** `main` was **not** fast-forwarded, and fresh attempt 2 was
  not spent.

## 1. Refs

| Ref | SHA |
| --- | --- |
| Branch HEAD at start | `894f600653ca52c42d644ed779e3a5be3721f9f2` |
| `origin/main` at start and at the end | `103aa22e6367e3556bb27f2a2946cdab1bbaf2bd` |
| C2 corrected candidate (the last product-source change) | `5f1eccf8c336d51343aaa62fa11b9f516252acfb` |
| **C3 tested candidate (test-only)** | **`899986d25f54ad989971b1766b49037366c16833`** |

**Preflight.** The tree was clean and every ref matched. Between `5f1eccf` and
`894f600`, `git diff --name-status` over `app/`, `testdata/`,
`DATA_PACKAGE_SPEC.md` and the Gradle files was empty. The C2 product delta
(`759ed91..5f1eccf`) was the counter-mirror change in `forgeshape_jni.cpp` and
one Javadoc paragraph in `NativeViewport.java`, plus tests.

**Product neutrality.** `git diff --name-status 5f1eccf..899986d` over
`app/src/main`, `testdata`, `DATA_PACKAGE_SPEC.md`, `third_party/manifold` and
the Gradle files is **empty**. C3 changed one file,
`app/src/androidTest/java/com/forgeshape/app/MirrorSmokeTest.java`.

## 2. Root cause of the Mirror failure, proved before the fix

**Source.**

- `mirrorEligibilityOf` (`forgeshape_body_mirror.cpp`) answers `HasSculptTruth`
  for any body whose `frozenSculpt().mesh.frozen()` is true.
- `sceneBodyCanMirror` (`forgeshape_jni.cpp`) returns that answer, and
  `ObjectsSectionView` adds `object_row_mirror` only when it is true. The
  control is ABSENT for such a body (`CLAUDE.md`, "A control that cannot
  succeed is not drawn").
- `WorkspaceTestSupport.resetToBaselineConstruction` applies a box and a
  placement to the first Construction body. Its own comment says it cannot
  un-freeze, and no product operation can.

**Runtime.** In the C2 shard-4 run `36499781424` (process 5348), at the start
of `e2eMirror0101`:

```
FORGESHAPE_STARTUP_NO_PROJECT bodies=21
FORGESHAPE_SCULPT_STATE:mode_construction mode=construction ... frozen=1 ... objectId=1 ... stale=1
FORGESHAPE_SCENE_ACTIVE_BODY:1
FORGESHAPE_PROJECT_LOADED:21 bodies sculptMeshes=3 active=1 kind=Construction
```

- **The inherited body is active body 1.** It is a Construction box, the mode
  is Construction, and it carries a Frozen Sculpt Mesh (`frozen=1`).
- **The baseline apply kept the frozen mesh.** It only marked the source stale
  (`stale=1`).
- **Where the mesh came from.** `EditorWorkspaceControlsTest.ui06`, the second
  class in shard 4, starts sculpting on body 1
  (`FORGESHAPE_SCULPT_STATE:freeze … objectId=1`).

The C3 shard-4 replay repeated this with the new diagnostic line (§5):
`inherited open=true bodies=21 active=1 representation=1 mode=0 frozen=1
canMirror=false`. **Classification: `TEST_ISOLATION_DEFECT`.** The product
withdraws Mirror correctly.

## 3. The correction

`MirrorSmokeTest.setUp` no longer relies on `resetToBaselineConstruction` alone:

1. It logs what it inherited (`FORGESHAPE_TEST_MIRROR_START inherited …`).
2. It cancels any sketch and support chooser, enters Construction, and
   **closes the project**, which writes nothing and is what Back to Home does.
3. `ensureConstructionProjectForTest` opens a fresh project with one seeded
   body inside the session-initialization bracket.
4. It clears the Construction history and syncs the workspace.
5. `resetToBaselineConstruction` then applies the usual baseline.
6. It **asserts the preconditions** (§4) and only then encodes
   `baselineProject`.

The shape is `SelectionOutlineTest.freshProject()`, which C2 already used for
the same kind of defect.

**What did not change:**

- `WorkspaceTestSupport` is untouched.
- No product source, no unfreeze operation, and no change to Mirror
  eligibility.
- No `@Ignore`, `assume*`, sleep, retry or timeout was added.
- The journey is unchanged. It still uses the real Objects row overflow, the
  Mirror control, the three-chip chooser, the YZ commit, and Construction
  Undo/Redo, with every assertion it had.

## 4. Preconditions now asserted

All are read from native truth before the journey starts:

- a project is open;
- `sceneBodyCount() == 1`, and that one body is `sceneActiveBodyId()`;
- `sceneBodyRepresentation == REPRESENTATION_CONSTRUCTION`;
- `sculptState[SCULPT_MODE] == MODE_CONSTRUCTION`;
- `sculptState[SCULPT_HAS_MESH] == 0` for the active body, so there is no
  Frozen Sculpt Mesh (the state behind `HasSculptTruth`);
- `sceneBodyCanMirror(body)`, the same query that decides whether the row draws
  Mirror;
- `constructionUndoDepth() == 0`, so the setup left no history step.

The journey's own `clickRowControl(…, "Mirror")` still asserts that Mirror is
offered.

## 5. Focused validation on `899986d`

| Gate | Run | Result |
| --- | --- | --- |
| `CI FAST` | `36547144378`, attempt 1 | **green**: whitespace, debug + release + androidTest builds, JVM, release guard (0 self-test symbols and strings), `.forge` corpus parity, runner and device-guard checks. No retry |
| `CI DEVICE`, `MirrorSmokeTest` alone | `36547148093` | **PASS**: 23/23 startup tokens, 0 failure lines, **OK (1 test)**. `fresh open=true bodies=1 active=1 representation=1 mode=0 frozen=0 canMirror=true`, then `BODY_MIRRORED:2 from=1 plane=YZ`, then Undo (`removed=1`) and Redo (`restored=1`) |
| `CI DEVICE`, the exact C2 shard-4 sequence | `36548109667` | **PASS**: the 12 classes in the runner's order (`CadExtrudeExtentTest`, `EditorWorkspaceControlsTest`, `EditorWorkspaceDisplayTest`, `EditorWorkspaceGestureTest`, `EditorWorkspaceHistoryTest`, `EditorWorkspaceObjectsTest`, `ImportedMeshDurableTest`, `MirrorSmokeTest`, `ProjectProcessDeathTest`, `SculptHistoryNavigatorTest`, `SketchUxTest`, `UiPrefVisualEvidenceTest`): **OK (126 tests)**, each test once, 830 s. `inherited … bodies=21 active=1 … frozen=1 canMirror=false`, then `fresh … bodies=1 … frozen=0 canMirror=true`, then `BODY_MIRRORED:86 from=85 plane=YZ` |

**Device-free checks** (local `pwsh` 7, and again in `CI FAST`):

- `test-instrumented-runtime.ps1`: 26/26.
- `test-instrumented-sharding.ps1`: THR1-01..10.
- `verify-device-guards.ps1`: all PASS. `DEV2-02` needed a `powershell.exe` →
  `pwsh` shim on the Linux host, because the script names the Windows binary.
- `git diff --check`: clean.
- `:app:compileDebugAndroidTestJavaWithJavac`: green.

## 6. The authoritative aggregate

### 6a. Run `36550524879`: infrastructure, before the runner

- **The boot proof failed.** It returned `DEVICE_STARTUP_UNRESOLVED`: each of
  three startup captures had 22 of 23 tokens, zero failure lines,
  `viewport_ok=true` and liblog-reported drops (56–80). The host was still
  loaded (`load1=4.13`; settling hit its 181 s cap). This is the
  dropped-capture signature `CLAUDE.md` describes, not a failing self-test.
- **The runner step was skipped.** No discovery, fingerprint, attempt or shard
  exists for this run.
- **Then the job hung.** The `if: always()` step "Capture the device log and
  stop the emulator" ran `adb logcat -d` against the wedged emulator and did
  not return for 2 h 31 min (09:46 → 12:17). It was cancelled, because the step
  has no timeout of its own and the job's 180-minute limit was the only bound.
- **Classification: `INFRASTRUCTURE`.** The runner's fresh attempt 1 was not
  spent, so the aggregate was dispatched once more.

### 6b. Run `36567175533`: fresh attempt 1

| Field | Value |
| --- | --- |
| Discovery | `DISCOVERY_PASS`, 57 classes / 629 tests, 5 shards, missing 0, duplicates 0, unexpected 0 |
| Inventory change | none against C2 (57 / 629; inventory `06ecaec7c14b`, partition `a4a0b57ed4b7`, both identical to C2) |
| Fingerprint | `84de3b50ff25` (app `1bfc1212b316`, test `21a3b7d0e67c`, device `emulator-5580`, identity `avd:ForgeShape_CI_API36`) |
| App APK SHA-256 | `1bfc1212b316ce3eb066e4488679e2f5f2d31a5f78cf26952dfe50ec5b96ef2a` |
| Test APK SHA-256 | `21a3b7d0e67c00769cd1ae5ce516981ebf25d5a086bb05ccffddf40c169d4209` |
| Attempt | 1 of 2 for this fingerprint, counted |
| Elapsed | 25.5 of 120 minutes |

| Shard | Classes / tests | Result | Duration |
| --- | --- | --- | --- |
| 1 | 11 / 126 | **PASS** 126/126 | 687.4 s |
| 2 | 10 / 126 | **`ASSERTION_FAILURE`**, `PRODUCT_TEST_FAILURE`: 1 of 126 | 845.0 s |
| 3 | 12 / 126 | NOT_RUN (the runner stops at a failed shard) | — |
| 4 | 12 / 126 | NOT_RUN | — |
| 5 | 12 / 125 | NOT_RUN | — |

- **Union counts:** assigned union 629, executed union 126, execution missing
  503, failed shards 4 (shard 2 and the three unrun shards), aborted 0.
- **Marker: `FULL_SHARDED_SUITE_FAIL`.** No `FULL_SHARDED_SUITE_PASS` exists
  for this candidate, and none is claimed.

## 7. The new failure: `SelectionOutlineVisualEvidenceTest`

**What fails.** `e2eSelOutR1Vis_theWholeJourneyInTwelveCaptures`, at capture
09:

```
java.lang.AssertionError: 09_warm_light: a band must be present in this frame, and none was found
```

**What happened, from the device log.**

- **Captures 01–08 passed.** That includes 07 and 08, whose bands are measured
  on the same two-body scene.
- **The palette change reached native.** The case then set the Warm Light
  ground, and native logged
  `FORGESHAPE_VIEWPORT_BACKGROUND:WarmLight requested=3 known=1 changed=1`.
- **The screenshot was taken on a timer.** `capture()`
  (`SelectionOutlineVisualEvidenceTest.java:643`) waits for layout, sleeps a
  fixed **500 ms**, and takes it. It measures pixels of the LIGHT-family
  outline colour, and found none.

**Why this is a capture race and not a product change.**

- **Nothing it touches changed.** Neither the product source (unchanged since
  `5f1eccf`) nor this class changed.
- **Same sequence, same partition, opposite result.** The same shard-2 sequence
  and partition `a4a0b57ed4b7` passed 126/126 in the C2 aggregate `36494252771`,
  this test included.
- **The mechanism is already documented.** On this emulator the renderer
  presents about three frames a second through a four-image FIFO swapchain,
  so a capture 400–500 ms after a change can show a frame recorded before it.
  `TEST_EVIDENCE.md` §3 measured this as finding A2. There, `db8ff6b` moved
  `CadVerticalSliceTest` to wait for presented frames: 6 frames took
  1.49–2.12 s, more than four times the fixed delay.
- **Frame 09 is the first after a ground change.** A stale frame would still
  show the dark family's outline colour, which is what "none was found" in the
  light family's colour means.

**Diagnostic replay of the shard-2 sequence** (subset evidence, no attempt
counted): see §7a.

**Classification: `TEST_OBSERVATION_SEAM_DEFECT`, pre-existing.** The capture
waits a fixed time instead of an observable event.

**Proposed correction, not applied here (C3-15).** A test-only fix, like
`db8ff6b`: `SelectionOutlineVisualEvidenceTest.capture` should wait for the
renderer to present a bounded number of frames after each state change
(`NativeViewport.debugRendererFramesPresented`, monotonic since C2) instead of
`SystemClock.sleep(500)`. It rebuilds the test APK, so the next authoritative
run is a new fingerprint's fresh attempt 1.

**Why attempt 2 was not spent.** A second run of the same bytes could pass only
by losing the same race, which is retry-until-green (prompt §7). The failing
shard is not repaired by a resume either, because a fresh VM signs with a new
debug key (`FULLSHARDED_C1.md` §4).

### 7a. Diagnostic replay

PENDING

## 8. APK

No handoff APK is issued, because the handoff follows an integration PASS
(prompt §13). A local debug build of `899986d` exists only in the session
scratchpad (`app-debug.apk`, SHA-256
`3ec4d276253d8bf556582e150b9dbdd794a7f196389ff6b19357e14bb8e868e1`, signed with
that host's debug key), and it is not a tested artifact.

## 9. Scope and remaining debt

- **No new capability.** No CAD, Mirror, Sculpt, format, fixture, kernel or
  build change.
- **Debt carried forward:**
  - A3, A4, A5 and A6 are still owed.
  - The cloud resume is still refused by fingerprint, because every VM signs
    with a fresh debug key.
  - The `CI FULL SHARDED` cleanup step can hang on a wedged emulator (§6a).
  - The `SelectionOutlineVisualEvidenceTest` capture race (§7).
  - Shards 3–5 were not reached by this aggregate, and shard 5 has still never
    run in a cloud aggregate.
