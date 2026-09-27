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

Both also trigger on pushes to `infra/ci-cloud-r1` (the branch that bootstrapped
them) and on `workflow_dispatch`. Both use a read-only token, no secrets, and
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
  encoder) regenerates all 36 fixtures into a scratch directory under `pwsh`,
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
4. Sets the log buffer to 64M, installs the debug APK, launches it, and requires
   all **22** `*_SELFTEST_OK` tokens in order, `FORGESHAPE_NATIVE_VIEWPORT_OK`,
   and zero `_SELFTEST_FAIL` / `_FAIL:` lines.
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

## Where the evidence is

Each run's page → **Artifacts** (kept 14 days):

- `ci-fast-evidence` — toolchain manifest, release-guard table, corpus parity,
  runner/guard check output, JUnit reports, and the debug, androidTest and
  unsigned release APKs. `ci-fast-failure-reports` on failure.
- `ci-device-evidence` — `summary.json`, `environment-manifest.txt` (emulator
  build, system image, KVM, GPU mode, Vulkan features, ForgeShape's own Vulkan
  lines), `emulator-command.txt`, `emulator-boot.log`, `startup-logcat.txt`,
  `selftest-tokens-found.txt`, `instrumentation-raw.txt`,
  `instrumentation-junit.xml`, `test-logcat.txt`, and startup/post-test
  screenshots. `ci-device-gradle-reports` on failure.

The job summary on the run page repeats the key numbers.

## What cloud CI does not replace

- **FullSharded is not the default** and is not run by CI. The exhaustive
  gate stays `scripts\run-instrumented-tests.ps1 -Serial <serial> -FullSharded`
  on a ForgeShape-owned AVD, run deliberately.
- **Emulator evidence closes no physical-device gate**: stylus, pressure,
  hover, palm rejection, real hardware GPUs, 16 KB-page devices and real-device
  performance all still need a physical device. The CI emulator renders
  through SwiftShader, so its frame times say nothing about a phone's.
- The OWNER's visual review of evidence frames is not replaced either.
