# Build, release and ABI — DEEP-AUDIT-R1 (Part O)

## Configuration (unchanged by this audit)

`app/build.gradle`: compileSdk 36, minSdk 26, targetSdk 36, versionName `0.3.0`, versionCode 1, **ndkVersion `29.0.14206865`** (pinned, not bumped), abiFilters `x86_64` + `arm64-v8a`, `c++_static`, C++17, JDK 17 source/target. AGP 8.13.2, Gradle 8.14.3 wrapper. No release `buildType` block (release is the default minify-off, unsigned). `CMakeLists.txt`: `glslc` AOT shaders `#include`d as C initializers, single `forgeshape_native` lib, links vulkan/android/log. None of AGP/Gradle/JDK/CMake/NDK changed.

## Built this audit (`d783d4b`)

| Target | Result | ABI `.so` (bytes) | LOAD align |
| --- | --- | --- | --- |
| debug | BUILD SUCCESSFUL | arm64 3 515 704 / x86_64 3 502 528 | 0x4000 (both) |
| release (unsigned) | BUILD SUCCESSFUL | arm64 2 084 968 / x86_64 2 339 080 | 0x4000 (both) |
| `:app:testDebugUnitTest` | 70/70, 0 failures | — | — |

Both ABIs, debug and release, all four `.so`s. 16 KB-page ELF `LOAD` alignment 0x4000 confirmed on every one (readelf) — the requirement for 16 KB-page devices holds.

## Native symbol / hardening inspection

* 155 `Java_com_forgeshape_app_NativeViewport_*` exported dynamic symbols in each `.so`, matching the 155 `static native` Java declarations (F: `NATIVE_CPP_JNI.md`).
* RELRO + `BIND_NOW`/`NOW` set; non-executable stack (`GNU_STACK RW`); NEEDED = platform libraries only.
* No `.symtab` (stripped); the debug `.so` carries 21 self-test-related strings and the release `.so` carries the self-test **check-name** strings and the exported `run*SelfTests` symbols — **F-16**: the `*selftest*` translation units are on the CMake source list unconditionally, so their code links into release even though the call site is `#ifndef NDEBUG`. Release `.so` is ~500 KB larger than needed and exports those entry points. Confirms PROJECT_STATUS's "proven debug-guarded, not proven absent from a release binary" as CONFIRMED-PRESENT. Recommended follow-up: a CMake condition dropping the suites from a release build.

## Manifest / packaging

Debuggable flag present in debug only (`aapt2 dump badging`: `application-debuggable` on debug, absent on release). `native-code: 'arm64-v8a' 'x86_64'` on both. No permission, one exported launcher activity, no build output committed (`.gitignore` covers `.cxx`/`.gradle`/APK). Product APK has no runtime dependency.

## Findings

| ID | Sev | Finding | Status |
| --- | --- | --- | --- |
| F-16 | P2 | Self-test code links into the release `.so` (call site guarded, translation units not). | reported |
| — | OK | NDK pinned, ABIs and page alignment correct, no dependency, release hardened (RELRO/BIND_NOW/NX). |
