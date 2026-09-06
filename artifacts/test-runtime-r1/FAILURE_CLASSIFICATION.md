# Failure classification

`Get-InstrumentedFailureClassification` maps a shard's raw status and output
onto one conservative category. The categories are the brief's:

| category | when |
| --- | --- |
| `PASS` | exactly one `OK (n tests)` with `n` equal to the shard's assigned count |
| `PRODUCT_TEST_FAILURE` | instrumentation produced attributable assertion evidence: `FAILURES!!!` or a `Tests run: … Failures:` line |
| `INFRASTRUCTURE_FAILURE` | `INSTRUMENTATION_ABORTED`, `System has crashed`, `Process crashed`, `shortMsg=`, a device offline/not-found/unauthorized line, `error: closed`, `INSTALL_FAILED`, a lost instrumentation process |
| `TIME_BUDGET_EXCEEDED` | the hard budget stopped this shard from starting |
| `USER_ABORTED` | the operator stopped the run |
| `NOT_RUN` | an earlier shard ended the progression, or the shard was not selected |
| `RUNNER_ERROR` | everything else, including a bare non-zero exit, a missing final result, a count mismatch, and any status this runner does not recognise |

## The rule that matters

**A product failure is claimed only when instrumentation actually produced
attributable assertion evidence.** A non-zero shell exit is not product
evidence. Neither is a missing `OK` line, nor a count that does not match: both
mean the run did not tell us what happened, which is a runner or environment
problem. Calling either a product failure sends somebody to read code that
never ran — and UI-PREF-R1 spent three aggregate attempts on exactly that
confusion.

Ambiguity therefore fails closed to `RUNNER_ERROR`, never to a verdict about
the product.

## Evidence is retained, never rewritten

Each shard's raw output is written to `<run directory>\shard-<n>.log`, the
`FAILED_SHARD_LOG` line names it, and the checkpoint stores the classification
beside the status. An infrastructure failure is never rewritten into a PASS,
and a classification never changes what the aggregate concluded — the integrity
counters remain the authority.

## Proven by

- `TESTRUNTIME-22`: an `INSTRUMENTATION_ABORTED: System has crashed.` line and
  four device-level signatures each classify as `INFRASTRUCTURE_FAILURE`.
- `TESTRUNTIME-23`: `FAILURES!!!` with `Tests run: 108,  Failures: 1`
  classifies as `PRODUCT_TEST_FAILURE`, and a passing shard stays `PASS`.
- `TESTRUNTIME-24`: a bare non-zero exit, a missing result, a count mismatch
  and an unknown status all classify as `RUNNER_ERROR`.

The three real UI-PREF-R1 aggregate failures map cleanly onto this: run 1 was
`PRODUCT_TEST_FAILURE` (a stale assertion in a test), run 2 was
`INFRASTRUCTURE_FAILURE` (the guest's system server crashed), and run 3 was
recorded as an assertion failure that a later clean restart showed to be the
guest — which is exactly why the classification retains the log path rather
than deciding the question by itself.
