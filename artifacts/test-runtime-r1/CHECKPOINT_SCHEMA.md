# Checkpoint schema, version 1

Written by `Save-InstrumentedCheckpoint`, read by `Read-InstrumentedCheckpoint`,
in the run directory — by default `artifacts\instrumented-runs\<short-id>\checkpoint.json`,
overridable with `-RunDirectory` or `-CheckpointPath`. It is deliberately not a
global temp file: the checkpoint belongs to the run whose logs sit beside it.

## Fields

| field | meaning |
| --- | --- |
| `schemaVersion` | 1. A higher value is refused, not guessed at |
| `testedTreeFingerprint` | the full fingerprint hash |
| `fingerprintShortId` | its first 12 characters, as printed in logs |
| `appApkSha256`, `testApkSha256` | the installed bytes |
| `inventorySha256`, `partitionSha256` | the canonical texts' hashes |
| `shardCount` | shards in the plan |
| `deviceSerial`, `deviceIdentity` | where the result was produced |
| `instrumentTarget` | package/runner |
| `discoveredClasses`, `discoveredTests` | the totals discovery proved |
| `mode` | `FULL_AGGREGATE`, `RESUME` or `SHARD_ONLY` |
| `aggregateAttempt` | which automatic attempt this is for this fingerprint |
| `startedAt`, `updatedAt` | round-trip UTC timestamps |
| `shardResults[]` | one record per shard touched |

Each `shardResults` record carries `shardNumber`, `assignedCount`,
`executedCount`, `status`, `failureClassification`, `durationSeconds`,
`startedAt`, `finishedAt` and `logPath`.

## Guarantees

- **Atomic.** The JSON is written to `<path>.writing` and then moved over the
  target with `-Force`, so a killed run never leaves a half-written checkpoint
  to be read as authority (`TESTRUNTIME-10`, which also asserts no temporary
  file survives and the directory holds exactly one file).
- **Written at meaningful transitions only**: once before the first shard, and
  once after each shard completes. Never inside a test, never per line of
  output.
- **Fails closed.** Unreadable JSON, a missing required field, a schema below 1
  or above this runner's version are all refused by name
  (`TESTRUNTIME-08`, `TESTRUNTIME-09`).
- **Not silently discarded.** An unreadable checkpoint is archived beside itself
  as `checkpoint.unreadable.<timestamp>.json` rather than deleted, and `-Fresh`
  copies the current one to `checkpoint.attempt<N>.<timestamp>.json` before the
  new attempt starts.
- **Preserved on failure.** A failed run keeps its checkpoint and its per-shard
  logs; that is the entire point.

## Example

The real checkpoint from the device validation, after a shard-only run of one
class (`REAL_DEVICE_VALIDATION.md`):

```json
{
    "schemaVersion":  1,
    "testedTreeFingerprint":  "014446b1ede7175cce3ad2fbdb1d177724e1558ce4b60ce27c7b7626741bfbd2",
    "fingerprintShortId":  "014446b1ede7",
    "appApkSha256":  "a673ec21ae0593b93c1cbf7164cc60c6d349e57d35fe4b1f2084fb4fc49a3394",
    "testApkSha256":  "2d3714c81e40a3b2b41af2bd112647d97450548dde36ae943601cd2c454be0da",
    "inventorySha256":  "24ba17623a03b4afd77fbcb1ba0ac49deddf27a2660ddf35b612f93936b2aae1",
    "partitionSha256":  "0d9d9dcec876fba1d3f1afb52dd154367fbcc6ce2bd49936796b396ae4787597",
    "shardCount":  42,
    "deviceSerial":  "emulator-5580",
    "deviceIdentity":  "avd:ForgeShape_Stage006",
    "instrumentTarget":  "com.forgeshape.app.test/androidx.test.runner.AndroidJUnitRunner",
    "discoveredClasses":  42,
    "discoveredTests":  540,
    "mode":  "SHARD_ONLY",
    "aggregateAttempt":  0,
    "shardResults":  [ { "shardNumber": 38, "assignedCount": 3, "executedCount": 3,
                         "status": "PASS", "failureClassification": "PASS",
                         "durationSeconds": 24.8 } ]
}
```

Run directories are transient tool state and are ignored by Git; the two
fixtures in this folder are committed copies used as evidence.
