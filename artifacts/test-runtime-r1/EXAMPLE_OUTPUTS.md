# Example outputs

Real lines from the runs in `REAL_DEVICE_VALIDATION.md`, trimmed to the parts
that matter.

## Plan only

```
DISCOVERY_PASS | classes=42 | tests=540 | shards=5 | missing=0 | duplicates=0 | unexpected=0
FINGERPRINT | id=d9769af86b98 | app=a673ec21ae05 | test=2d3714c81e40 | inventory=16204cf4307c | partition=19b837b782bf | device=emulator-5580 | identity=avd:ForgeShape_Stage006
MODE=PLAN_ONLY | checkpoint=artifacts\instrumented-runs\d9769af86b98\checkpoint.json
AGGREGATE_ATTEMPT | mode=PLAN_ONLY | attempt=0 | counted=False | PLAN_ONLY does not consume a full aggregate attempt
PLAN | shard=1/5 | classes=8 | tests=108 | action=RUN | selected | filter=com.forgeshape.app.CadA3VisualEvidenceTest,...
RUNNER_OVERHEAD | total=99.4s | discovery=98.8s | fingerprint=0.06s
PLAN_ONLY_COMPLETE | resume_semantics=False | shards_that_would_run=5 | shards_that_would_be_skipped=0 | no instrumentation was executed
```

No aggregate marker is printed, and the attempt is not counted.

## One shard

```
MODE=SHARD_ONLY | checkpoint=artifacts\instrumented-runs\014446b1ede7\checkpoint.json
PLAN | shard=38/42 | classes=1 | tests=3 | action=RUN | selected | filter=com.forgeshape.app.EditorWorkspaceLifecycleTest
PLAN | shard=1/42 | classes=1 | tests=46 | action=SKIP_NOT_SELECTED | shard-only run of another shard | filter=...
SHARD_START | shard=38/42 | classes=1 | tests=3 | filter=com.forgeshape.app.EditorWorkspaceLifecycleTest
OK (3 tests)
SHARD_RESULT | shard=38 | expected=3 | actual=3 | status=PASS | class=PASS | duration=24.8s | reason=valid runner result and exact expected count
SHARD_ONLY_SUMMARY_BEGIN
fingerprint=014446b1ede7
shard=38
status=PASS
classification=PASS
duration_seconds=24.8
checkpoint=artifacts\instrumented-runs\014446b1ede7\checkpoint.json
note=shard-only is subset evidence and can never complete or repair an aggregate
next=scripts\run-instrumented-tests.ps1 -Serial emulator-5580 -FullSharded -ShardCount 42 -Resume
SHARD_ONLY_SUMMARY_END
```

## A resume that is allowed

```
RESUME_ACCEPTED | fingerprint=014446b1ede7 | carried_pass_shards=41
PLAN | shard=36/42 | classes=1 | tests=6 | action=RUN | selected | filter=com.forgeshape.app.EditorWorkspaceGestureTest
PLAN | shard=38/42 | classes=1 | tests=3 | action=SKIP_PASS | already PASS for this exact fingerprint
PLAN_ONLY_COMPLETE | resume_semantics=True | shards_that_would_run=1 | shards_that_would_be_skipped=41 | no instrumentation was executed
```

## A resume that is refused

```
RESUME_REFUSED | RESUME_INVALID_TEST_APK_CHANGED
RESUME_REFUSED_EXPLANATION | the shards that passed belong to a tree these bytes are not; a final aggregate needs one fresh run (-Fresh).
RUN_ABORTED | mode=PLAN_ONLY | reason=Resume refused: RESUME_INVALID_TEST_APK_CHANGED
```

## What a failing aggregate prints instead of restarting

From the runner's own source; the block that replaces "rerun the entire command
from shard 1". It has not yet been produced by a real aggregate — see the gap
noted in `REAL_DEVICE_VALIDATION.md`.

```
FAILED_SHARD | shard=5 | status=ASSERTION_FAILURE | classification=PRODUCT_TEST_FAILURE
FAILED_SHARD_LOG | artifacts\instrumented-runs\<id>\shard-5.log
NEXT_RERUN_SHARD | scripts\run-instrumented-tests.ps1 -Serial emulator-5580 -FullSharded -ShardCount 5 -ShardOnly 5
NEXT_RESUME | scripts\run-instrumented-tests.ps1 -Serial emulator-5580 -FullSharded -ShardCount 5 -Resume
RESUME_CAVEAT | editing any product or test source rebuilds the APKs and changes the fingerprint, which refuses the resume; a final aggregate then needs one fresh run (-Fresh).
NO_AUTOMATIC_RESTART | this runner does not start a new aggregate from shard 1 by itself
FULL_SHARDED_SUITE_FAIL
```

## The aggregate summary's new fields

Added to the block that already carried the integrity counters:

```
fingerprint=<short id>
app_apk_sha256=<full>
test_apk_sha256=<full>
inventory_sha256=<full>
partition_sha256=<full>
device=<serial>          device_identity=<avd or model>
mode=<FULL_AGGREGATE|RESUME>     aggregate_attempt=<n>
shard_<n>_classification=<category>
shard_<n>_duration_seconds=<n>
shard_<n>_carried_from_checkpoint=<True|False>
elapsed_minutes=<n>  target_minutes=<n>  hard_stop_minutes=<n>
checkpoint=<path>
```

`FULL_SHARDED_SUITE_PASS` still requires every one of the original conditions:
discovery PASS, assigned = executed, missing = duplicates = unexpected =
execution_missing = 0, and every shard PASS — now with the added requirement
that every contributing shard belongs to the same fingerprint.
