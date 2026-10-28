# CAD-R2 — TESTS

Raw commands and results, in the order run. Container: Linux x86_64, g++
(host suites), OpenJDK 21 driving the Gradle 8.14.3 wrapper, a local Android SDK
with the pinned NDK `29.0.14206865` (installed in the container for this stage;
`local.properties` is git-ignored).

## Phase 0 — baseline on `636797e`

| Gate | Command | Result |
| --- | --- | --- |
| Host | `bash scripts/host-native-selftests.sh` | `HOST_SELFTESTS_OK (4304 checks, 0 failed)`, 25 suites, 2 m 32 s (`host_baseline_suites.txt`) |
| JVM | `./gradlew :app:testDebugUnitTest` | 179 tests, 0 failures, 0 errors (first attempt: Maven Central HTTP 429 — environment, retried unchanged) |
