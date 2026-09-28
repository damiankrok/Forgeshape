# The milestone aggregate, corrected (CAD-VS-FULLSHARDED-C2)

**Result: PENDING.** The fresh aggregate, run `36494252771` on `5f1eccf`, is
in progress. This record is finished when it ends.

C1 stopped the cloud aggregate at shard 2 on seven failing tests
(`FULLSHARDED_C1.md`). C2 found one cause per failure and corrected each at its
owning seam. No assertion was weakened, and no sleep, retry or skip was added.
One fresh aggregate then ran on the corrected candidate.

## 1. Refs

| Ref | SHA |
| --- | --- |
| Branch HEAD at start | `5f6ed11f336c199489101d2f964f6b384792f241` |
| `origin/main` at start | `103aa22e6367e3556bb27f2a2946cdab1bbaf2bd` |
| Tested CAD product candidate before C2 | `759ed9192b03a0d35d3f3e938fdb210ed39e4aa7` |
| C2 diagnostic commit (tests only) | `01f24836d515cb167ae149ed6734b48188fcffee` |
| **C2 corrected, tested candidate** | **`5f1eccf8c336d51343aaa62fa11b9f516252acfb`** |

**Preflight.** The tree was clean and every ref matched. Between `759ed91` and
`5f6ed11`, `git diff --name-status` over `app/`, `testdata/`,
`DATA_PACKAGE_SPEC.md` and the Gradle files was empty.

**What C2 changed under `app/src/main`.** Two files, `forgeshape_jni.cpp` (the
counter mirrors, §4) and `NativeViewport.java` (one Javadoc line). Nothing under
`testdata/`, the format spec or the build files moved. The corrected candidate
is therefore a **new product SHA**, and every C2 gate below cites
`5f1eccf`.

## 2. The seven C1 failures, one by one

| # | Test | C1 symptom | Root cause | Classification | Correction | After |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | `EditorWorkspaceArchitectureTest.uiar110_sculptRailParity` | expected 4 rail entries, was 7 | `SCULPT-FCM-R1` (`6c09c7c`, 2026-09-07) made the Sculpt rail seven brushes; the test was not updated | `STALE_TEST_EXPECTATION` | asserts the seven ids **in the rail's reading order** (Grab, Clay, Smooth, Flatten, Inflate, Crease, Mask), each inside the trailing host, and the child count | green |
| 2 | `EditorWorkspaceCorrectionTest.uir4b08_everyAnchoredSurfaceSharesOneMotionContract` | expected 5 surfaces, was 6 | `SCULPT-H1` (`ee98c98`, 2026-09-07) registered the History navigator in `anchoredSurfaces()`; the test was not updated | `STALE_TEST_EXPECTATION` | asserts the six surfaces **by identity and order** (`objectsPopover`, `addPrimitivePalette`, `propertyInspector`, `displayPopover`, `projectPopover`, `historyNavigator`), then each is settled and the one shared motion contract, as before | green |
| 3 | `SelectionOutlineTest.seloutr1_35_…` | 2 composite draws not seen in 3 s | the renderer counter mirrors ran backwards across a render-thread restart (§3) | `TEST_OBSERVATION_SEAM_DEFECT` (pre-existing; fails on `main`) | the mirrors continue across a restart (§4) | green |
| 4 | `SelectionOutlineTest.seloutr1_15_16_theToggleStops…` | same | same | `TEST_OBSERVATION_SEAM_DEFECT` (pre-existing; fails on `main`) | same | green |
| 5 | `SelectionOutlineTest.seloutr1_23_24_25_36_…` | selection fell to 17, expected 12 | the case assumed a two-body scene it never established (§5) | `TEST_ISOLATION_DEFECT` (pre-existing; fails on `main`) | opens a fresh project, asserts exactly two bodies, keeps the exact expectation | green |
| 6 | `SelectionOutlineTest.seloutr1_02_…` | 2 composite draws not seen in 3 s | same mechanism as #3 (§3, §6) | `TEST_OBSERVATION_SEAM_DEFECT` (pre-existing; the probe fails identically on `main`) | the mirror fix | green |
| 7 | `SelectionOutlineTest.seloutr1_01_…` | same, only inside shard 2 | same mechanism; how high the stale value is depends on the previous class (§3) | `TEST_OBSERVATION_SEAM_DEFECT` | the mirror fix | green, in the exact shard-2 order |

None of the seven is a `CAD_VERTICAL_SLICE_REGRESSION`. The only one that is
not a test defect lives in a diagnostic seam, and it is on `main` too.

## 3. Root cause of the outline timing failures (#3, #4, #6, #7)

**The seam.** The renderer's counters live in the `Renderer` object that each
render thread creates. This covers presented frames, device rebuilds, and the
outline's mask allocations, mask-pass frames and composite draws. The UI thread
reads them through process-scoped atomics that the render loop writes after
every frame (`forgeshape_jni.cpp`). The outline allocation slot is documented
as a count "for the life of the process".

**The lifecycle.** `ForgeShapeActivity.onDestroy` calls
`NativeViewport.stop()` whenever the Activity is not changing configuration.
That happens at the end of **every** instrumentation test method, because
`ActivityScenarioRule` finishes its Activity. The next test's `onCreate` starts
a **new** render thread with a new `Renderer`, whose counters begin at zero.

**The defect.** Each mirror used to be a straight copy of the live renderer's
count, so two things happened:

- **Before the new renderer's first frame,** the mirror still held the dead
  renderer's total.
- **At the first frame,** it dropped to 1.

`assertOutlineIsBeingDrawn` reads a baseline and waits up to 3 s for
`baseline + 2`. A baseline read before the first frame is the previous test's
total (10 to 27 in the C2 diagnostic runs). The new renderer then has to count past that
total, from 1, within 3 s. SwiftShader presents about 3–5 frames a second
there, so the wait runs out.

**Proof, not argument.** The diagnostic commit `01f2483` added the failure
values to every outline assertion. It also added `RendererCounterContinuityTest`,
which lets one renderer draw, finishes the Activity, relaunches it, and asserts
that no counter read falls below the read before it. The same test diff ran
with the same class list on both sides:

| Run | Side | Probe | `seloutr1_35` | `seloutr1_15_16` toggle | `seloutr1_02` |
| --- | --- | --- | --- | --- | --- |
| `36488591133` | `main` `103aa22` + probe (`05f2f8c`) | **FAIL** `7.0 -> 1.0`, read 273 ms after the relaunch | FAIL `composite 27 -> 13` | FAIL `composite 16 -> 12` | pass |
| `36488594032` | candidate `01f2483` (unfixed) | **FAIL** `7.0 -> 1.0`, read 340 ms after the relaunch | FAIL `composite 27 -> 10` | FAIL `composite 12 -> 10` | FAIL `composite 10 -> 10` |

- **The counter visibly ran backwards within one assertion** (`27 -> 13`,
  `16 -> 12`). A counter that only ever counts frames cannot do that.
- **The probe fails the same way on `main` and on the candidate.**

**Why the order mattered (#7).** The stale value is whatever the previous
test's renderer counted. Whether a case fails therefore depends on two things:

- how long the case before it ran with the outline on;
- whether the case reads its baseline before its own renderer's first frame.

In shard 2, `seloutr1_01` (the class's first case) followed the six
classes before it, and so the last case of `SculptUndoTest`. In the three-class C1 comparison it followed
`EditorWorkspaceCorrectionTest`'s last case, which leaves a lower total.

## 4. The correction, and why it is release-neutral

**The fix.** `renderThreadMain` now reads each counter mirror once, before its
loop: presented frames, device rebuilds and the three outline counts. It then
stores `base + renderer count` instead of the bare count.

- **The base is safe to read.** It is the value the previous thread left.
  `stop()` joins that thread before `start()` creates the next, and the join
  and the thread creation order the two, so no other writer exists.
- **The mirrors keep their documented meaning.** They are now process-lifetime
  counts that never decrease, which the allocation slot already claimed to be.
- **The mask-pass/composite equality still holds.** Both counters start from
  equal bases and advance together.
- **Width, height and band width are unchanged.** They are current values, not
  counts.

**What does not change:**

- **Nothing drawn or saved.** No rendering, pass scheduling, resource,
  selection, project truth, `.forge` byte, history step or fingerprint moves.
- **Only diagnostic readers see a difference.** The mirrors are read only by
  the diagnostic accessors (`selectionOutlineStats`,
  `debugRendererFramesPresented`, `debugRendererDeviceRebuilds`), and each
  reader now sees a non-decreasing count.
- **No release-build difference.** The change adds no symbol and no string;
  it adds five loads and five additions per thread.
- **The release guard agrees.** `CI FAST` `36488851113` reports 0 `SelfTests`
  symbols and 0 self-test strings in both release ABIs, with debug unchanged
  at 23 / 391.

**Regression assertion.** `RendererCounterContinuityTest` fails on the unfixed
product on both sides (above). On `5f1eccf` it also covers `framesPresented`
and `deviceRebuilds`, and it passed:
`stoppedComposite=447 liveComposite=450`,
`stoppedFramesPresented=522 liveFramesPresented=525`.

**Test waits.** Nothing in the outline suite was lengthened. With the mirror
monotonic, the existing wait (`baseline + 2` within 3 s) is a bounded event
wait: whichever renderer is live must record two more composite draws. The one
residual race is theoretical and was present before. Two separate atomics are
read one after the other, so a read that straddles a render-thread store could
see the mask pass one frame ahead of the composite. The window is a few
nanoseconds per frame, and the outline suite passed in every C2 run.

`CadVerticalSliceTest.awaitPresentedFrames` already carried a fallback for a
counter that "counts from zero again". The code is unchanged; only its comment
now says the fallback is defensive.

## 5. `seloutr1_23_24_25_36`: the selection rule (#5)

**The rule in source.** `deleteSceneBody` (`forgeshape_body_delete.cpp`,
`UI-OWNER-45`, `CLAUDE.md`) moves selection to the NEXT body in scene order, or
to the PREVIOUS one when the deleted body was last.

**What the test did.** It took `first = ids[0]`, appended a second body, which
is therefore last, and deleted it. It then expected the selection to go to
`ids[0]`. That is the previous body only when the scene holds exactly two.

**What actually happened.** The scene is process-scoped, and the earlier cases
in the class leave bodies behind. The diagnostic runs show 7 bodies before
the Delete on both sides, so the selection went, correctly, to body 17: the one
immediately before the deleted body.

**Classification and fix.** The product obeys the documented contract, so this
is a `TEST_ISOLATION_DEFECT`. The case now calls the suite's own
`freshProject()`, which the two E2E cases already used for the same reason. It
asserts `sceneBodyCount() == 2` and keeps the exact expectation.

## 6. `seloutr1_02`, symmetrically (#6)

**The evidence.** Two runs per side, each with the same classes in the same
order, and no aggregate retries:

| Side | Run | `seloutr1_02` |
| --- | --- | --- |
| `main` | C1 `36478589747` | pass |
| `main` | C2 `36488591133` | pass |
| candidate | C1 `36482442687` | FAIL |
| candidate | C2 `36488594032` | FAIL `composite 10 -> 10` |

**Why `main` passed and the candidate failed.** The case reads its baseline
after `seloutr1_04_05`, a Sculpt case.

- **On the candidate,** the baseline was that case's total of **10**. The new
  renderer counted from 1 to exactly **10** in the 3 s; the target was 12.
- **On `main`,** the case before ran for less time (6.0 s against 8.1 s in
  C2) and left a total the new renderer could pass in 3 s.

**The same defect on both sides.** The probe, which removes the timing from
the question, fails identically on both (`7.0 -> 1.0`). The slice did not
touch the mirrors, the outline pass or the lifecycle.

**Classification: `TEST_OBSERVATION_SEAM_DEFECT`, pre-existing.** It is
corrected by §4, and `seloutr1_02` passed on `5f1eccf` in both device runs
below.

## 7. Focused validation on `5f1eccf`

| Gate | Run | Result |
| --- | --- | --- |
| `CI FAST` | `36488851113`, attempt 2 | **green**: build (debug, release, androidTest), JVM, release guard (0/0), corpus parity 44/44, runner and guard checks. Attempt 1 failed in `sdkmanager --install` 7 s in, before any build or test ran, the transient C1 already recorded, so it was re-run once |
| `CI DEVICE` | `36490257169` | **PASS**: 23/23 startup tokens in order, `FORGESHAPE_NATIVE_VIEWPORT_OK`, 0 failure lines. `EditorWorkspaceArchitectureTest`, `EditorWorkspaceCorrectionTest`, `SelectionOutlineTest`, `RendererCounterContinuityTest`: **OK (69 tests)**, 457 s |
| `CI DEVICE`, the exact shard-2 sequence | `36491864271` | **PASS**: the ten C1 shard-2 classes in the runner's order (`CadVerticalSliceTest`, `EditorWorkspaceArchitectureTest`, `EditorWorkspaceCorrectionTest`, `GlbImportExternalR1Test`, `JniBoundaryHardeningTest`, `SculptUndoTest`, `SelectionOutlineTest`, `SketchUxVisualEvidenceTest`, `Ui3dStateAuditTest`, `Ui3dStateCorrectionTest`): **OK (126 tests)**, 932 s, `seloutr1_01` included |

**Device-free checks, run locally under `pwsh` 7:**

- `test-instrumented-runtime.ps1`: 26/26.
- `test-instrumented-sharding.ps1`: THR1-01..10.
- `verify-device-guards.ps1`: all PASS.
- `git diff --check`: clean.

`CI FAST` repeated all four.

## 8. The authoritative aggregate

PENDING: `CI FULL SHARDED` run `36494252771`, `mode=fresh`, attempt 1 on
`5f1eccf`.

## 9. Cloud resume

A fresh VM still signs with a new debug key, so a cross-run `-Resume` would
still be refused by fingerprint (`FULLSHARDED_C1.md` §4).

PENDING the aggregate's outcome.

## 10. Scope

- **No new capability.** No CAD feature, format, fixture, kernel, region,
  operation, UI or Sculpt behaviour changed.
- **Still not delivered, and unchanged:** A3, A4, A5 and A6, Through All,
  Revolve, projected edges, fillet and shell.
