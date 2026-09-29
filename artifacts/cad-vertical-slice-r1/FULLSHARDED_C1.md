# The milestone aggregate in the cloud (CAD-VS-FULLSHARDED-C1)

**Result: `FAIL-CAD-VS-FULLSHARDED-C1`.**

- **What was built.** The repository's own `-FullSharded` runner now runs in
  GitHub Actions, and its PlanOnly proof is valid.
- **Attempt 1.** Fresh attempt 1 on the vertical-slice candidate stopped at
  shard 2 with 7 real test failures, and the runner printed
  `FULL_SHARDED_SUITE_FAIL`.
- **Why it stops here.** Fixing the failures is outside this closeout (prompt
  §9), and a second fresh attempt would fail the same way. `main` was **not**
  merged.

## 1. What was tested

- **Tested product SHA:** `759ed91`.
- **Runs were on:** `fa4e6f1`. Its only non-documentation changes since
  `759ed91` are the runner portability change, the boot-only smoke mode and the
  new workflow (§5).
- **APK inputs unchanged:** `git diff 759ed91..fa4e6f1` touches nothing under
  `app/`, `testdata/` or the Gradle files, so the APK inputs are the tested
  candidate's.

## 2. Runs

| Run | Mode | Commit | Outcome |
| --- | --- | --- | --- |
| `36469669832` | registration push (§5) | `e3e8c7c` | job **skipped**, by design |
| `36469711583` | plan | `9ad02a5` | the SDK install step failed transiently before any runner code ran; its log was not printed, which `fa4e6f1` fixed |
| `36470848992` | **plan** | `fa4e6f1` | **valid**: 56 classes / 628 tests, 5 shards (126/126/126/125/125), missing 0, duplicates 0, unexpected 0, fingerprint `d961e8150662`, attempt 0 (not counted), no instrumentation |
| `36472045843` | **fresh, attempt 1** | `fa4e6f1` | **`FULL_SHARDED_SUITE_FAIL`**: shard 1 PASS (126, 921 s); shard 2 `ASSERTION_FAILURE`, classified `PRODUCT_TEST_FAILURE` (7 of 126, 1088 s); the runner stopped; elapsed 33.48 min |
| `36478586309` | shard 2 only | `fa4e6f1` | the same 7 failures, reproduced on a new VM; attempt 0; no aggregate marker |
| `36478589747` | CI DEVICE, **`main` `103aa22`** (no slice) | `103aa22` | the three failing classes alone: **5 of 68 fail** |
| `36482442687` | CI DEVICE, candidate | `fa4e6f1` | the same three classes alone: **6 of 68 fail** |

**Budget.**

- Aggregate attempt 1 used 33.5 of the 120 minutes.
- Fresh attempt 2 was **not** spent. The failing shard reproduces
  deterministically, so a second attempt would be a blind rerun (prompt §8).

## 3. The failure, test by test

| Test | Assertion | Main (no slice) | Candidate, classes alone | Candidate, shard 2 (twice) |
| --- | --- | --- | --- | --- |
| `EditorWorkspaceArchitectureTest.uiar110_sculptRailParity` | Sculpt rail has 4 entries; it has 7 | FAIL | FAIL | FAIL |
| `EditorWorkspaceCorrectionTest.uir4b08_everyAnchoredSurfaceSharesOneMotionContract` | 5 anchored surfaces; there are 6 | FAIL | FAIL | FAIL |
| `SelectionOutlineTest.seloutr1_35_…` | 2 composite draws within 3 s | FAIL | FAIL | FAIL |
| `SelectionOutlineTest.seloutr1_15_16_…` | same | FAIL | FAIL | FAIL |
| `SelectionOutlineTest.seloutr1_23_24_25_36_…` | selection falls to the surviving body | FAIL | FAIL | FAIL |
| `SelectionOutlineTest.seloutr1_02_…` | 2 composite draws within 3 s | pass | FAIL | FAIL |
| `SelectionOutlineTest.seloutr1_01_…` | same | pass | pass | FAIL |

**Five of the seven fail on `main` without the vertical slice.**

- **`uiar110` and `uir4b08` are stale tests.** `SCULPT-FCM-R1` gave the Sculpt
  rail seven tools and `SCULPT-H1` added an anchored surface, both on
  2026-09-07. Neither test was updated, and no aggregate has run since: the
  status file records `FULL_SHARDED_SUITE_PASS` last at 34 classes / 482
  tests.
- **None of the three test classes was touched by the slice,** and the slice
  did not touch the rail or `anchoredSurfaces()`.

**`seloutr1_01` depends on its context.** It passes when its class runs with
only the other two, and fails inside shard 2, after seven more classes in the
same process.

**`seloutr1_02` is UNRESOLVED.**

- It failed in both candidate contexts and passed in the one `main` run.
- It fails the same composite-draw timing assertion that three tests fail on
  `main`.
- The slice's only renderer change is the preview tint in the body push
  constant, which is zero unless a CAD preview sets it. It does not touch the
  outline passes.
- That is an argument, not a proof. Three runs cannot clear the slice, so it is
  recorded as open, not as a flake.

## 4. Fingerprints, and why a cloud resume is refused

The same commit produced three different fingerprints, because a fresh VM
generates a new debug signing key and the APK bytes change with it:

- plan: `d961e8150662` (app `08e722e1…`);
- attempt 1: `e9d3d605597e` (app `7b5e4fc3…`, test `fe648257…`);
- shard rerun: `5c2142e36a6a` (app `58d5e2f0…`).

A cross-run `-Resume` is therefore refused by the runner's own check. This is
the limitation prompt §6.3 allows, reported rather than worked around. The run
directory still travels between runs as an artifact. The inventory and partition
hashes (`75a10f08…`, `5a927490…`) were identical in all three.

## 5. What this closeout changed

- **`scripts/run-instrumented-tests.ps1`:** two host-dependent choices, now
  pure functions in `instrumented-runtime.ps1`. They are proven by
  `TESTRUNTIME-25`/`26` (26/26 green).
  - **Gradle:** `gradlew.bat` on Windows, `bash ./gradlew` elsewhere (the
    checked-in wrapper has no executable bit).
  - **APK paths:** built with the host separator, so Windows gets the same
    backslash strings as before.
  - **Unchanged:** discovery, partition, fingerprint inputs, checkpoint schema,
    attempts, budgets, classification and markers.
- **`scripts/ci-device-smoke.sh`:** `FORGESHAPE_CI_BOOT_ONLY=1` stops after the
  startup evidence and leaves `emulator-5580` running. The default mode is
  unchanged.
- **`.github/workflows/ci-full-sharded.yml`:** `workflow_dispatch` only, with
  the modes plan, fresh, resume and shard.
  - It uses the CI DEVICE pins and AVD, with an `emu avd name` check.
  - It uploads the run directory every time and can restore it from
    `previous_run_id`.
- **Registration deviation.** GitHub returns 404 to a dispatch of a workflow
  that lives only on a non-default branch and has never run.
  - Commit `e3e8c7c` added a push trigger for this branch and this one file,
    with a job guard, so its single push run was skipped.
  - `9ad02a5` removed the trigger again.
  - The committed file is dispatch-only, and it keeps the guard.
- **No product, test, fixture or schema change.**

## 6. What unblocks the aggregate

None of this was done here, because prompt §9 forbids fixing a failure inside
this closeout:

1. **Update the two stale tests** to the product as it is: seven Sculpt tools,
   six anchored surfaces. This is test-only and fails on `main` today.
2. **Root-cause the `SelectionOutlineTest` composite-draw failures on the CI
   emulator**, including `seloutr1_02`. It fails on the candidate and not on
   `main` in one run, so it needs more than one comparison before it can be
   classified. `seloutr1_23_24_25_36` also fails on `main`, on a selection
   rule, and needs its own look.
3. **Then run one fresh aggregate** with CI FULL SHARDED (attempt 2 of 2 for
   this candidate, or attempt 1 of a new one if a test change moves the
   fingerprint).
