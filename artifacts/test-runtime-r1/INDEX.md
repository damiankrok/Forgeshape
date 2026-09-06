# TEST-RUNTIME-R1 — shard-only, same-tree resume, budgets, checkpoints, classification

Baseline `7f2d234f887766390de8b4a0820ee522a5b4a75f` (clean). A **test-tooling
stage only**: no product source was touched, and no five-shard aggregate was
run.

## The problem

A failure in the last shard of an aggregate cost every shard before it. The
runner's own help said to "rerun the entire command from shard 1", so
UI-PREF-R1 spent three aggregates — one product assertion, one guest crash, one
guest-induced failure — and hours, re-proving shards that had already passed.

## What changed

| capability | where |
| --- | --- |
| tested-tree fingerprint over the installed APK bytes, inventory, partition, shard count and device | `FINGERPRINT_CONTRACT.md` |
| versioned, atomic checkpoint written after every shard | `CHECKPOINT_SCHEMA.md` |
| `-PlanOnly`, `-ShardOnly N`, `-Resume`, `-Fresh`, and `-Resume -PlanOnly` as a resume dry run | `EXAMPLE_OUTPUTS.md` |
| resume that skips only shards that passed for the identical fingerprint, and refuses by name otherwise | `FINGERPRINT_CONTRACT.md`, `TESTRUNTIME_MATRIX.md` |
| per-shard and cumulative timing, 90-minute target and 120-minute hard stop | `TIME_BUDGET_POLICY.md` |
| at most two automatic aggregate attempts per fingerprint, third needs an explicit override | `TESTRUNTIME_MATRIX.md` (20, 21) |
| conservative infrastructure / product / runner classification | `FAILURE_CLASSIFICATION.md` |
| no blind restart: a failure prints the failing shard, its classification, its log, and the two exact next commands | `EXAMPLE_OUTPUTS.md` |

## What did not change

`FULL_SHARDED_SUITE_PASS` means what it meant, plus one requirement. Discovery
validation, the deterministic class-atomic partition, the assignment proof and
the aggregate's integrity counters are the baseline's code, untouched; a PASS
still needs discovery PASS, assigned = executed, missing = duplicates =
unexpected = execution_missing = 0 and every shard PASS. The addition is that
every contributing shard must belong to the same tested fingerprint, which a
resume can only satisfy by proving the bytes did not move.

## Evidence

| document | holds |
| --- | --- |
| `BASELINE_RUNNER.md` | the runner as it was, read from the live scripts before any edit |
| `FINGERPRINT_CONTRACT.md` | what the fingerprint binds and why not Git HEAD |
| `CHECKPOINT_SCHEMA.md` | the version-1 format, its guarantees, a real example |
| `TIME_BUDGET_POLICY.md` | the two budgets, their validation, what the hard stop does not do |
| `FAILURE_CLASSIFICATION.md` | the categories and the rule that ambiguity never becomes a product verdict |
| `TESTRUNTIME_MATRIX.md` | all 24 cases, and the two real defects they were extended to catch |
| `REAL_DEVICE_VALIDATION.md` | the five bounded device runs, the fixtures, the measured overhead, and the gap |
| `EXAMPLE_OUTPUTS.md` | real output for every mode |
| `testruntime-matrix.log`, `thr1-sharding.log`, `device-guards.log`, `parse-check.log` | the tooling suites' own output |
| `plan-only*.log`, `shard-only-38.log`, `resume-plan-*.log` | the device runs |
| `fixture-checkpoint-*.json` | the same-tree and mutated checkpoints used for the resume demonstrations |

## Results

- `TESTRUNTIME-01..24`: 24 checks, 0 failures, about 5 seconds.
- `THR1-01..10`: unchanged and passing.
- `DEV2-01..07`, `DEV3-01..06`: passing, now scanning 18 executable surfaces
  including the two new scripts, 0 violations.
- Five bounded device runs, about 8 minutes total, on `ForgeShape_Stage006` /
  `emulator-5580`.

## Two defects found in this stage's own code, on the device

Both are recorded rather than quietly fixed, because each is a lesson about
PowerShell that the matrix now enforces:

1. The shard executor writes its transcript as it runs, so everything it
   emitted was captured as its return value and the shard was recorded
   `MISSING_RESULT`. The sequence now takes the last object carrying a `Lines`
   property, the runner's callbacks write straight to stdout, and
   `TESTRUNTIME-13` uses a chatty executor.
2. The checkpoint converters returned `,@($out)` while callers wrapped in
   `@()`, producing an array containing one array: 41 carried shards counted as
   1. `TESTRUNTIME-13` now carries two.

## One pre-existing defect surfaced

`THR1-09` invokes the runner as a child process; the child's refusal reaches
the parent as an error record and the parent runs under `Stop`, which
terminated the check before it could assert. The baseline runner behaves
identically — verified by running the committed version from `HEAD` — so this
was fragility in the harness, not a regression. The preference is now relaxed
around that one call and restored immediately after.
