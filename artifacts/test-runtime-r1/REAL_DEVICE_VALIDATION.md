# Bounded real-device validation

All on the isolated `ForgeShape_Stage006` / `emulator-5580` AVD, identity
confirmed by the runner before any device call; `emulator-5554` never
contacted. **No five-shard aggregate was run**, by instruction.

Total device time **about 8 minutes** against the 30-minute target and
45-minute hard stop of this stage's own budget.

| # | run | command | elapsed | outcome |
| --- | --- | --- | ---: | --- |
| 1 | plan only, production shape | `-FullSharded -ShardCount 5 -PlanOnly` | 2:21 | `DISCOVERY_PASS classes=42 tests=540`; fingerprint `d9769af86b98`; five shards of 108 tests each planned `RUN`; `PLAN_ONLY_COMPLETE`; no instrumentation, no marker (`plan-only.log`) |
| 2 | plan only, one class per shard | `-FullSharded -ShardCount 42 -PlanOnly` | 0:46 | fingerprint `014446b1ede7` — a different shard count is a different tested tree; each shard's class named on its `PLAN` line (`plan-only-42.log`) |
| 3 | one class through the new path | `-FullSharded -ShardCount 42 -ShardOnly 38` | 1:04 | `EditorWorkspaceLifecycleTest`, `OK (3 tests)`, `SHARD_RESULT status=PASS class=PASS duration=24.8s`, checkpoint written, `SHARD_ONLY_SUMMARY` with the exact resume command, and **no aggregate marker in either direction** (`shard-only-38.log`) |
| 4 | same-tree resume, dry run | `-FullSharded -ShardCount 42 -Resume -PlanOnly -CheckpointPath <fixture>` | 0:33 | `RESUME_ACCEPTED carried_pass_shards=41`; `shards_that_would_run=1`, `shards_that_would_be_skipped=41`; the one `RUN` line is shard 36, `EditorWorkspaceGestureTest`; nothing executed (`resume-plan-same-tree.log`) |
| 5 | invalidated resume | same, with the fixture's `testApkSha256` mutated | 0:39 | `RESUME_REFUSED | RESUME_INVALID_TEST_APK_CHANGED`, the explanation line, `RUN_ABORTED`, exit 1 (`resume-plan-changed-test-apk.log`) |

## The two fixtures

`fixture-checkpoint-same-tree.json` is the REAL checkpoint that run 3 wrote,
with its `shardResults` replaced by 41 PASS records covering every shard except
36. `fixture-checkpoint-changed-test-apk.json` is that file with one field
changed — `testApkSha256` set to zeros — which is the brief's "copied/mutated
synthetic checkpoint" and is why no product APK had to be tampered with to
create a mismatch.

Runs 4 and 5 are dry runs on purpose. Executing a resume that carried 41
synthetic PASS records would have printed a `FULL_SHARDED_SUITE_PASS` into an
evidence directory for an aggregate that never ran — a token somebody could
later cite in good faith. The skip/refuse decision is what these runs are for,
and the execution semantics behind it are proven deterministically by
`TESTRUNTIME-13/14/15`.

## What this did NOT exercise on the device

The `FULL_SHARDED_SUMMARY` block of a real multi-shard aggregate, and the
`FAILED_SHARD` / `NEXT_RERUN_SHARD` / `NEXT_RESUME` guidance block that follows
a real failure. Running either needs a full aggregate, which this stage is
forbidden to run. What stands in for it: the aggregate computation itself is
unchanged from the baseline and is covered by `THR1-06/07/08/10`; the summary
block is largely the baseline's, extended with fields; every script parses with
zero errors (`parse-check.log`); and the first real aggregate run under this
runner will exercise it. That gap is stated rather than papered over.

## Runner overhead measured here

| run | total pre-instrumentation | discovery | fingerprint |
| --- | ---: | ---: | ---: |
| 1 | 99.4 s | 98.8 s | 0.06 s |
| 2 | 37.9 s | 37.7 s | 0.05 s |
| 3 | 28.2 s | 28.0 s | 0.05 s |

The work this stage added — hashing both APKs, asking the device its identity,
canonicalising the inventory and partition, hashing them, and writing the
checkpoint — is the difference between the total and the discovery, about
**0.2–0.6 seconds**. Discovery, which existed before, is 99 % of the
pre-instrumentation time. Nothing is hashed or discovered per test or per
class, and the checkpoint is written once before the first shard and once after
each shard.
