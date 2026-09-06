# The runner as it was — baseline `7f2d234f887766390de8b4a0820ee522a5b4a75f`

Read from `scripts/run-instrumented-tests.ps1` and `scripts/instrumented-sharding.ps1`
before anything was changed. Paraphrased from the live scripts, not inferred.

## Entry points

| invocation | meaning |
| --- | --- |
| `-Serial <s>` alone | monolithic full suite: one `am instrument -w` with no filter |
| `-Serial <s> -TestClass <c>` | focused subset; prints `MODE=FOCUSED_SUBSET`; can never emit a full-suite marker |
| `-Serial <s> -FullSharded [-ShardCount n]` | the authoritative aggregate |

`-FullSharded` and `-TestClass` were mutually exclusive; `-ShardCount` was
valid only with `-FullSharded`; the default shard count was 5.

## Device guard

`emulator-5554` is refused by name before any adb command is issued. The serial
is mandatory, and readiness is checked with `adb -s <serial> get-state`, never
with a bare `adb devices` enumeration. Every adb call carries `-s <serial>`.

## Build and install

`gradlew.bat :app:assembleDebug :app:assembleDebugAndroidTest`, then
`adb -s <serial> install -r` for each of `app-debug.apk` and
`app-debug-androidTest.apk`. The APKs were used but never identified — nothing
recorded WHICH bytes a result described.

## Discovery

`adb -s <serial> shell am instrument -w -r -e listTestsForOrchestrator true <target>`,
parsed by `ConvertFrom-InstrumentedDiscoveryOutput`, which fails closed on: a
non-zero exit, any abort/failure marker, a missing `INSTRUMENTATION_CODE: -1`,
a start record missing `class`/`test`/`current`/`numtests`, an advertised
`numtests` that does not equal the number of records, a `current` sequence with
a gap or a duplicate, duplicate test ids, and anything but exactly one `OK (n
tests)` matching the discovered total.

## Partition

`New-InstrumentedShardPlan` is class-atomic and deterministic: classes are
grouped, sorted by descending test count then ascending name, and each is
assigned to the currently lightest shard (ties broken by shard index). It
refuses a shard count above the discovered class count.

## Assignment proof

`Get-InstrumentedShardAssignmentProof` computes missing, duplicate and
unexpected test ids against discovery and passes only when all three are zero.

## Execution

One `adb -s <serial> shell am instrument -w -e class <comma-joined classes>`
per shard, in index order. `Test-InstrumentedShardResult` classified each into
`PASS`, `INSTRUMENTATION_ABORT`, `ASSERTION_FAILURE`, `RUNNER_FAILURE`,
`MISSING_RESULT` or `COUNT_MISMATCH`, requiring exactly one `OK (n tests)` with
`n` equal to the shard's expected count.

**After a shard failed, every later shard was recorded `NOT_RUN` and the
aggregate failed.** The script's own help then instructed the operator to
"recover the device and rerun the entire command from shard 1". Nothing was
carried forward, so a failure in shard 5 of 5 cost the four shards that had
passed — the loop this stage exists to end.

## Aggregate and output

`New-FullShardedAggregate` requires the assignment proof, a complete result
set, no non-passing shard, zero execution-missing tests and an executed union
equal to the discovered total. `Get-FullSuiteMarker` prints
`FULL_SHARDED_SUITE_PASS` only for a passing aggregate and only in
`-FullSharded` mode. Output was a single `FULL_SHARDED_SUMMARY_BEGIN`/`_END`
block on stdout with per-shard classes, tests, filter and status, the integrity
counters, and the marker. Exit 1 on failure, 0 on pass.

## What did not exist

No timing of any kind; no checkpoint; no resume; no shard-only mode; no dry
run; no identification of the tested bytes; no time budget; no attempt
accounting; no distinction between an infrastructure failure and a product
failure beyond the shard status name; and no guidance after a failure beyond
"start again".

## Timeout behaviour

There was none in the runner. A shard ran until `adb` returned. The only limits
were the operator's own patience and whatever wrapper they had launched it
from — which is how a single aggregate reached 154 minutes and a repeated one
much more.
