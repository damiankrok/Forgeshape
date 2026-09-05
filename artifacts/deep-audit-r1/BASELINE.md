# Baseline preflight — DEEP-AUDIT-R1

## Git

| | |
| --- | --- |
| Baseline HEAD required | starts `be5b729c` |
| Baseline HEAD full | `be5b729c354a9876e24fbef54c630ee21fbe95d3` |
| Baseline tree | clean (`git status --short` empty) |
| Baseline check | **PASS** — no `BLOCKED-BASELINE-DRIFT` |
| `git rev-parse --abbrev-ref HEAD` | `master` |
| Recent commits at baseline | `be5b729 feat(cad): start pages, immediate flat sketch, navigator, dimensions, curves` · `c5864c6 docs(cad): commit the ten evidence frames under frames/` · `e70bdf9 feat(app): Home, New Project bootstrap, dirty guard, CADB v2 corpus (CAD-A3-C1)` |
| Final HEAD (this audit) | `d783d4b` (docs publish commit adds one more on top) |

Git is local only (no remote configured, no push). Committer identity in `.git/config` alone.

## Audit commits (on top of `be5b729c`)

1. `247c4c7` chore(audit): compress source comments and remove stale narration
2. `7b8ef02` docs: reconcile ForgeShape current truth
3. `ab5dc8d` test(audit): add targeted audit regressions
4. `d783d4b` fix(audit): close three deep-audit findings in the native domain and JNI
5. docs(audit): publish deep audit evidence — this commit

Order differs slightly from the brief's suggested numbering (tests before fix) because the two audit regression tests that already passed at baseline were separated from the three that prove the fix; both are pure additions.

## File census (tracked, baseline)

| Metric | Value |
| --- | ---: |
| Tracked files | 1201 |
| `.md` | 113 |
| `.java` | 101 (51 main, 42 androidTest, 8 test) |
| `.cpp` | 61 · `.h` 63 |
| `.png` | 586 · `.mp4` 2 · `.svg` 4 · `.glb` 4 |
| `.forge` fixtures | 36 (28 golden in `testdata/forge/v1`, 7 packaged in `androidTest/assets`, 1 in `artifacts/`) |
| `.ps1` | 15 · `.js` 9 · `.csv` 10 · shaders (`.vert`/`.frag`) 6 |

## Line counts (`git ls-files | xargs wc -l`, baseline)

| Group | Files | Lines |
| --- | ---: | ---: |
| Production C++ (`app/src/main/cpp/*.{cpp,h}` minus `*selftest*`) | 84 | 37 374 |
| Native self-tests (`*selftest*`) | 40 | 25 535 |
| Java main | 51 | 18 733 |
| Instrumented tests | 42 | 29 223 |
| JVM tests | 8 | 1 151 |
| Scripts (`scripts/*.ps1`) | 12 | 3 105 |
| Resources (`app/src/main/res/**`) | 76 | 3 583 |
| Root docs (6 `.md`) | 6 | 11 204 |

## Comment census (see `COMMENT_AUDIT.md`)

Baseline: ALL 225 code files 112 162 lines, 28 473 comment (27.9 %); production 135 files, 18 652 comment (36.3 %); Java production 45.4 %. Blocks > 15 lines: 214 (177 production).

## Tests

| Suite | Count |
| --- | ---: |
| Native self-test suites (debug launch) | 20 |
| Native checks (device, baseline) | 2970 (2981 after the 11 new audit checks) |
| Standalone-runner suites / checks | 18 / 2628 (18 platform-neutral; excludes render-shading + render-recovery which need the render/GPU seams) |
| Instrumented classes / `@Test` | 39 / 519 |
| JVM classes / `@Test` | 8 / 70 |

## APK sizes (built this audit, `d783d4b`)

| APK | Bytes | arm64-v8a `.so` | x86_64 `.so` |
| --- | ---: | ---: | ---: |
| `app-debug.apk` | 10 295 561 | 3 515 704 | 3 502 528 |
| `app-release-unsigned.apk` | 4 639 035 | 2 084 968 | 2 339 080 |

## Dependencies

Product APK: **no runtime dependency** (verified — `app/build.gradle` has only `testImplementation junit:junit:4.13.2` and `androidTestImplementation` junit + `androidx.test:core:1.6.1`/`runner:1.6.1`/`ext:junit:1.2.1`, all test-scope). Native `.so` NEEDED libraries: `libvulkan.so`, `libandroid.so`, `liblog.so`, `libm.so`, `libdl.so`, `libc.so` — platform only, no third-party. No engine, no GLM, no interchange library.

## Toolchain (pinned)

JDK 21 (Android Studio JBR), Android SDK platform 36, build-tools 36.1.0, **NDK `29.0.14206865`** (pinned, unchanged), CMake 3.22.1, Gradle 8.14.3 wrapper / AGP 8.13.2. compileSdk/targetSdk 36, minSdk 26, ABIs x86_64 + arm64-v8a, C++17 `c++_static`.

## Device

Evidence AVD `ForgeShape_Stage006` on `emulator-5580` (confirmed by `adb -s emulator-5580 emu avd name` → `ForgeShape_Stage006`). `emulator-5554` present but **never contacted** by any command in this audit. `logcat -G 64M` set before startup capture (`logcat -g` confirmed 64 MiB).
