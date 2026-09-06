# Time budgets — TEST-OWNER-02 in the runner

## Defaults

| budget | default | behaviour |
| --- | --- | --- |
| target | **90 minutes** | a WARNING. The run continues. `TIME_BUDGET_WARNING` names the elapsed time and the remaining hard budget |
| hard stop | **120 minutes** | a STOP. No further shard is launched |

Both are parameters: `-TargetMinutes` and `-HardStopMinutes`.

## Validation

`Assert-InstrumentedBudget` refuses, by name, before anything is built or
installed: a non-numeric value, NaN, an infinity, zero or negative, anything
above 1440 minutes, and a hard stop below the target. An equal target and hard
stop is allowed — that is a run the operator wants stopped exactly at the
target. `TESTRUNTIME-19` covers all six refusals and both accepted forms.

## What the hard stop does, and does not do

At the hard stop the runner stops **launching** shards. The shard already in
flight is not killed mid-instrumentation: the remaining shards are recorded
`TIME_BUDGET_EXCEEDED`, the checkpoint is written, and the aggregate fails.

**It does not reboot the device.** A wall-clock budget expiring says nothing
about the device's health, and rebooting on a timer would destroy the state an
operator needs to diagnose why the run was slow.

Because the shards it did not reach are recorded rather than lost, the natural
next step is `-Resume` on the same fingerprint — which is the whole reason the
budget can be enforced without wasting what already passed.

## Reported every run

- per shard: `duration=<n>s` on its `SHARD_RESULT` line and `durationSeconds`
  in the checkpoint;
- cumulative: `elapsed_minutes` in the summary;
- remaining: `RemainingMinutes` in the budget state, and the warning line names
  the hard budget.

No ETA is estimated from one sample. A prediction drawn from a single shard of
a suite whose shards differ by an order of magnitude would be worse than
silence.

## Proven by

- `TESTRUNTIME-17`: at 95 minutes against 90/120 the state is target-exceeded
  and not hard-stopped, remaining is 25 minutes, and a run that crosses its
  target still executes every shard.
- `TESTRUNTIME-18`: with a clock stepping 40 minutes and a 60-minute hard stop,
  exactly one shard starts, the rest are `TIME_BUDGET_EXCEEDED`, that status
  classifies as `TIME_BUDGET_EXCEEDED`, and the aggregate does not pass.

Both use an injected clock, so they take milliseconds rather than two hours.
