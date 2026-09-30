# Cloud CI — GitHub Actions

ForgeShape builds and tests on GitHub-hosted Linux VMs, so ordinary development
does not need the OWNER's PC to be on. This page is the working contract for
that; `CLAUDE.md` owns the Git rule it rests on, and `PROJECT_STATUS.md` records
the runs that were actually verified.

## The flow

1. `origin/main` (`https://github.com/damiankrok/Forgeshape`) is the shared
   source of truth. Start every task from the current `origin/main`.
2. Work on a task branch. Push it; GitHub Actions tests it.
3. Integrate into `main` only when the task authorizes it and the required
   checks are green on the task branch — as a fast-forward or a normal merge,
   never a force push or a rewrite.
4. After integration, confirm the same checks are green on `main`.

## The two workflows

| Workflow | File | Runs on | Mandatory? |
| --- | --- | --- | --- |
| `CI FAST` | `.github/workflows/ci-fast.yml` | `ubuntu-24.04`, JDK 17 (Temurin) | **Yes** — every push to `main`, every PR to `main` |
| `CI DEVICE` | `.github/workflows/ci-device.yml` | `ubuntu-24.04` + KVM, fresh API 36 x86_64 emulator | **Yes** — every push to `main`, every PR to `main` |
| `CI FULL SHARDED` | `.github/workflows/ci-full-sharded.yml` | the same VM, emulator and pins as `CI DEVICE` | **No** — manual only (`workflow_dispatch`), for a milestone aggregate |

The first two also trigger on pushes to `infra/ci-cloud-r1` (the branch that
bootstrapped them) and on `workflow_dispatch`. Both use a read-only token, no secrets, and
`pull_request` (never `pull_request_target`).

The toolchain is resolved explicitly by `sdkmanager` in each job, held to
`app/build.gradle`: `platforms;android-36`, `build-tools;36.1.0`,
`ndk;29.0.14206865`, `cmake;3.22.1`. The checked-in Gradle wrapper runs the
build, and its checksum is validated first.

### FAST — no device

- `:app:testDebugUnitTest`, `:app:assembleDebug`, `:app:assembleDebugAndroidTest`,
  `:app:assembleRelease`.
- **Release self-test guard** (`scripts/ci-release-selftest-guard.sh`): in the
  `libforgeshape_native.so` packaged in the release APK, 0 `SelfTests` dynamic
  symbols and 0 self-test token/check-name strings on both ABIs — and, as the
  control, more than 0 of both in the debug APK.
- **`.forge` corpus parity**: `scripts/build-forge-corpus.ps1` (the independent
  encoder) regenerates all 56 fixtures into a scratch directory under `pwsh`,
  and every committed fixture must be byte-identical.
- `scripts/test-instrumented-runtime.ps1`, `scripts/test-instrumented-sharding.ps1`
  and `scripts/verify-device-guards.ps1` (the latter through a CI-only
  `powershell.exe` → `pwsh` shim). On Linux, DEV3's scan of the executable
  surface — including the CI helper scripts — is fully meaningful; DEV2-02's
  "occupied port is BLOCKED" passes on Linux for a different reason (the
  launcher's Windows-only port query and `emulator.exe` lookup), so it proves
  nothing there and is only real evidence on Windows.
- `git diff --check` over the pushed range (or the PR's range).

### DEVICE — a real emulator, focused

`scripts/ci-device-smoke.sh`:

1. Refuses port 5554 / `emulator-5554` before anything starts, exactly as the
   local launcher does.
2. Creates a fresh AVD `ForgeShape_CI_API36` from
   `system-images;android-36;google_apis;x86_64` (`medium_phone` profile) and
   boots it headless on **port 5580** — `-no-window -no-audio -no-boot-anim
   -no-snapshot -wipe-data -gpu swiftshader_indirect -accel on`. The exact
   command is kept in the evidence.
3. Confirms identity with `adb -s emulator-5580 emu avd name`; every adb call
   carries `-s`.
4. Sets the log buffer to 64M, installs the debug APK, lets the freshly booted
   system settle (1-minute load below 2.5, at most 180 s), launches it, and
   requires ONE capture holding all **23** `*_SELFTEST_OK` tokens in order,
   `FORGESHAPE_NATIVE_VIEWPORT_OK`, and zero `_SELFTEST_FAIL` / `_FAIL:` lines —
   read from `ForgeShape`-tagged lines only, so a system line containing
   `_FAIL:` is never mistaken for one of ours.

   **A dropped capture is proven, not assumed.** The suites write ~3600 lines
   in under a second; on a busy emulator liblog drops lines on the WRITER side
   before they reach logd, which no ring-buffer size can fix, and it records
   how many in the `events` buffer (`liblog : <count>`). A capture with any
   failure line fails at once. A capture that is short of tokens with zero
   failure lines is relaunched only when liblog reports a drop for that
   process — at most three captures, each kept as `startup-logcat-<n>.txt` —
   and a gap with no reported drop fails as `DEVICE_STARTUP_UNRESOLVED`.
5. Runs ONE instrumentation class — `com.forgeshape.app.Ui3dStateCorrectionTest`
   by default (12 tests: real `MotionEvent` input, native anchor projection
   through the live camera, Construction / Sculpt / CAD modes) — and requires
   `OK (n tests)`.

A result is named in `summary.json`: `PASS`,
`BLOCKED-CI-CLOUD-DEVICE-CAPABILITY` (the emulator gives no usable Vulkan path —
nothing is weakened, mocked or replaced by an OpenGL fallback),
`FAIL-CI-CLOUD-DEVICE-PRODUCT` (a self-test or the focused class failed after
ForgeShape started), `DEVICE_INFRASTRUCTURE_FAILURE` (boot, install, timeout),
or `DEVICE_STARTUP_UNRESOLVED` (evidence ambiguous — read the logcat).

## Running a workflow by hand

GitHub → **Actions** → `CI FAST` or `CI DEVICE` → **Run workflow**, pick the
branch. `CI DEVICE` takes an optional `test_class` input for a different
focused class; it is still focused evidence.

### FULL SHARDED — the milestone aggregate, in the cloud

`CI FULL SHARDED` runs `scripts/run-instrumented-tests.ps1 -FullSharded`, the
same runner a Windows workstation runs, under PowerShell 7 on the Linux VM.
Everything that makes an aggregate authoritative stays the runner's:

- live AndroidJUnitRunner discovery and the exhaustive class-atomic partition;
- the fingerprint (both APK hashes, inventory, partition, shard count, device);
- the checkpoint after every shard;
- the attempt count and the 90/120-minute budgets;
- the `FULL_SHARDED_SUITE_PASS` marker.

The workflow only supplies the device. It boots it with
`FORGESHAPE_CI_BOOT_ONLY=1 scripts/ci-device-smoke.sh`, which provides the same
AVD, the explicit `emulator-5580` and the `emu avd name` identity check, plus
the 23 startup tokens. It then maps its `mode` input to one runner switch:

| `mode` | runner switch | aggregate attempt |
| --- | --- | --- |
| `plan` | `-PlanOnly` | none; no instrumentation runs |
| `fresh` | `-Fresh` | counted |
| `shard` (with `shard`) | `-ShardOnly <n>` | none; subset evidence only |
| `resume` (with `previous_run_id`) | `-Resume` | continues the checkpoint's attempt |

**Run a `plan` first.** Every run uploads its run directory (checkpoint and
shard logs) as `ci-full-sharded-run-directory`, and `previous_run_id` restores
one into the next run unchanged.

**Resume across runs in practice.** A fresh VM generates a new debug signing
key, so the rebuilt APKs usually differ by their signature. The fingerprint
then differs too, and the runner refuses the resume by name
(`RESUME_INVALID_APP_APK_CHANGED`). This is the runner working as designed. The
honest way on after a failure is:

1. run the failing shard focused (`mode=shard`);
2. fix what it shows;
3. use a second `fresh` attempt, within the two-attempt rule.

The runner cannot see attempts made on another VM, so the two-attempt limit
across runs is the operator's to keep. Record each attempt's run id.

## Where the evidence is

Each run's page → **Artifacts** (kept 14 days):

- `ci-fast-evidence` — toolchain manifest, release-guard table, corpus parity,
  runner/guard check output, JUnit reports, and the debug, androidTest and
  unsigned release APKs. `ci-fast-failure-reports` on failure.
- `ci-device-evidence` — `summary.json`, `environment-manifest.txt` (emulator
  build, system image, KVM, GPU mode, Vulkan features, ForgeShape's own Vulkan
  lines), `emulator-command.txt`, `emulator-boot.log`, `startup-logcat.txt`,
  `selftest-tokens-found.txt`, `instrumentation-raw.txt`,
  `instrumentation-junit.xml`, `test-logcat.txt`, startup/post-test
  screenshots, and `test-evidence/` — whatever the focused instrumented class
  wrote under the app's own external `files/evidence` directory (captured
  frames and measured facts; pulled with an explicit `-s`, and absent when the
  class wrote none). `ci-device-gradle-reports` on failure.

The job summary on the run page repeats the key numbers.

## What cloud CI does not replace

- **FullSharded is not the default** and never runs on a push or a pull
  request. The exhaustive gate is still
  `scripts\run-instrumented-tests.ps1 -Serial <serial> -FullSharded`, run
  deliberately. It runs either on a ForgeShape-owned AVD or, by hand, through
  `CI FULL SHARDED` on the CI emulator. Both paths use the same runner.
- **Emulator evidence closes no physical-device gate**: stylus, pressure,
  hover, palm rejection, real hardware GPUs, 16 KB-page devices and real-device
  performance all still need a physical device. The CI emulator renders
  through SwiftShader, so its frame times say nothing about a phone's.
- The OWNER's visual review of evidence frames is not replaced either.
