# The tested-tree fingerprint

## What it binds

`New-InstrumentedFingerprint` (`scripts/instrumented-runtime.ps1`) hashes a
canonical, field-per-line, fixed-order text with SHA-256:

| field | source |
| --- | --- |
| `appApkSha256` | SHA-256 of `app-debug.apk` as installed |
| `testApkSha256` | SHA-256 of `app-debug-androidTest.apk` as installed |
| `inventorySha256` | SHA-256 of the canonical inventory text |
| `partitionSha256` | SHA-256 of the canonical partition text |
| `shardCount` | the shard count in force |
| `instrumentTarget` | the package/runner instrumentation target |
| `deviceSerial` | the adb serial |
| `deviceIdentity` | `avd:<name>` from `adb -s <serial> emu avd name`, else `model:<model>`, else empty |
| `classCount`, `testCount` | the discovered totals |

The short id printed in logs is the first 12 hex characters. The full values
live in the checkpoint.

## Why not Git HEAD

ForgeShape routinely tests a dirty candidate before it is committed — the whole
of UI-PREF-R1 was verified that way. A commit hash would have called every one
of those intermediate trees the same tree, which is exactly the mistake that
makes a stale resume dangerous. **The APK byte hashes are the primary proof of
runtime and test identity**, and nothing in the fingerprint is derived from a
filename or a source path.

## The canonical texts

**Inventory** — one line per discovered test, `class#test|shard`, sorted
ordinally. Carrying the shard means a test that moves between shards changes
the hash even when the set of tests is identical.

**Partition** — one line per shard and class, `shard|class`, sorted ordinally.

Both are plain text rather than JSON, because JSON property order is a
serializer's choice and this input must be byte-stable across machines and
PowerShell versions.

## Consequences

- Rebuild after any product or test edit and the APK hash moves, so the
  fingerprint moves, so no previously passed shard can be reused.
- Change `-ShardCount` and the partition and shard count move.
- Run on another device or another AVD and the device fields move.
- The same tree, discovered twice, produces the same fingerprint — proven by
  `TESTRUNTIME-01`, which also re-plans the same discovery and checks the
  fingerprint does not move.

## Measured

On the authoritative emulator with 42 classes and 540 tests, computing the two
APK hashes, the device identity and the whole fingerprint took **0.05–0.06 s**,
against a discovery pass of 28–99 s. See `REAL_DEVICE_VALIDATION.md`.
