# TESTRUNTIME-01..24

`scripts\test-instrumented-runtime.ps1`. Synthetic fixtures, an injected shard
executor and an injected clock; **24 checks, 0 failures, about 5 seconds**. Raw
output in `testruntime-matrix.log`.

| id | requirement | how it is proven |
| --- | --- | --- |
| 01 | canonical fingerprint stable for the same APKs and inventory | two fingerprints from identical inputs compare equal ordinally, and re-planning the same discovery does not move it |
| 02 | app APK change invalidates the checkpoint | `RESUME_INVALID_APP_APK_CHANGED` |
| 03 | test APK change invalidates | `RESUME_INVALID_TEST_APK_CHANGED` |
| 04 | inventory change invalidates | a fifth class is discovered; `RESUME_INVALID_TEST_INVENTORY_CHANGED` |
| 05 | partition change invalidates | the same tests assigned the other way round; `RESUME_INVALID_PARTITION_CHANGED` |
| 06 | shard-count change invalidates | a 3-shard plan against a 2-shard checkpoint; `RESUME_INVALID_SHARD_COUNT_CHANGED` |
| 07 | device mismatch invalidates | a different serial, and the same serial on a different AVD; `RESUME_INVALID_DEVICE_CHANGED` for both |
| 08 | corrupt checkpoint refused | unparseable JSON and valid JSON missing required fields; `RESUME_INVALID_CHECKPOINT_CORRUPT` |
| 09 | future checkpoint schema refused | `schemaVersion` 99; `RESUME_INVALID_CHECKPOINT_SCHEMA` |
| 10 | atomic checkpoint replacement | a second save replaces the first, no `.writing` file survives, one file in the directory |
| 11 | shard-only executes one shard plan | exactly one `RUN`, the right index, the other `SKIP_NOT_SELECTED`, one result, and a shard outside the plan refused |
| 12 | shard-only cannot emit aggregate PASS | the real runner script invoked with the reserved serial for `-ShardOnly` and for `-PlanOnly`: neither prints `FULL_SHARDED_SUITE_PASS` nor `FULL_SHARDED_SUITE_FAIL` |
| 13 | same-tree resume skips PASS shards | two carried PASS shards on a 3-shard plan skip and one runs; the executor is deliberately chatty; the carried duration survives; a complete resume aggregates to PASS |
| 14 | resume reruns the failed shard | an `ASSERTION_FAILURE` shard is planned `RUN` while the passed one is `SKIP_PASS`, and the non-PASS record is not carried forward |
| 15 | resume reruns a not-run shard | a `NOT_RUN` shard is planned `RUN` |
| 16 | changed tree cannot reuse a PASS shard | the critical case: a rebuilt test APK refuses reuse by name and the fingerprint has moved |
| 17 | target warning logic | 95 minutes against 90/120 warns and does not stop; a run crossing its target still executes every shard |
| 18 | hard-stop logic | a 40-minute step against a 60-minute stop starts exactly one shard; the rest are `TIME_BUDGET_EXCEEDED`; the aggregate cannot pass |
| 19 | invalid budget parameters refused | zero, negative, hard-below-target, non-numeric, above 1440, NaN; and the documented defaults and an equal pair accepted |
| 20 | attempts 1 and 2 allowed | attempt 1 with no checkpoint, attempt 2 after one; shard-only, plan-only and resume consume none; a different fingerprint starts at 1 |
| 21 | attempt 3 refused without override | `ATTEMPT_LIMIT_REACHED`, and the explicit override allows it and is named `ATTEMPT_LIMIT_OVERRIDDEN` |
| 22 | infrastructure signature classified INFRA | an instrumentation abort and four device-level signatures |
| 23 | assertion failure classified PRODUCT | `FAILURES!!!` / `Tests run: … Failures:`; a passing shard stays PASS |
| 24 | ambiguous failure fails closed | bare non-zero exit, missing result, count mismatch, unknown status: all `RUNNER_ERROR` |

## Two defects these cases were extended to catch

Both were found on the device, during this stage, in code written for it:

1. **The executor's transcript was taken as its return value.** The real
   executor writes its shard transcript as it goes, so everything it emitted
   landed in the same pipeline as its result object and the shard was recorded
   `MISSING_RESULT`. The sequence now takes the last object carrying a `Lines`
   property, the runner's callbacks write to stdout directly, and
   `TESTRUNTIME-13` uses a deliberately chatty executor.
2. **A nested-array return counted 41 carried shards as one.** The checkpoint
   converters returned `,@($out)` while every caller wrapped in `@()`, which
   produces an array containing one array. `TESTRUNTIME-13` now carries two
   PASS shards, which fails on the old code.
