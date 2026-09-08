# UI-3D-STATE-C2 — builds, release symbols, device guards

All on the final tree.

## Builds

```
> .\gradlew.bat :app:assembleDebug --offline -q
exit=0  elapsed=22.4 s

> .\gradlew.bat :app:assembleRelease --offline -q
exit=0  elapsed=21.7 s
```

`assembleDebugAndroidTest` is built and run by
`scripts\run-instrumented-tests.ps1` on each of the three focused runs;
`BUILD SUCCESSFUL` in each (`logs/run-01`, `-02`, `-03`).

ABIs are `x86_64` and `arm64-v8a` in both configurations, and the NDK stays
pinned at `29.0.14206865`.

## Release symbol check

The new translation unit is a PRODUCT source, so it must be present in the
release `.so`; the self-tests that exercise it must not be.

```
llvm-readelf --dyn-syms lib/arm64-v8a/libforgeshape_native.so | grep -ci selftest  -> 0
llvm-readelf --dyn-syms lib/x86_64/libforgeshape_native.so    | grep -ci selftest  -> 0
grep -c "SELFTEST_OK"           lib/arm64-v8a/libforgeshape_native.so  -> 0
grep -c "SELFTEST_OK"           lib/x86_64/libforgeshape_native.so     -> 0
grep -c "sketchOverlayStyleWeights" lib/arm64-v8a/libforgeshape_native.so -> 1
grep -c "sketchOverlayStyleWeights" lib/x86_64/libforgeshape_native.so    -> 1
```

Read out of `app-release-unsigned.apk`, copied to a `.zip` and expanded in the
scratchpad — no `.so` survives in `app/build/intermediates` or `app/.cxx`.

## Startup self-tests, debug

```
adb -s emulator-5580 logcat -G 64M
adb -s emulator-5580 logcat -g      -> main: ring buffer is 64 MiB
```

22 `*_SELFTEST_OK` tokens, then `FORGESHAPE_NATIVE_VIEWPORT_OK`, zero
`_SELFTEST_FAIL` and zero `_FAIL:`. **3596 checks**, from 3582 at `CAD-EXT-R1`
plus C1's own; the gizmo suite went **167 → 176** with this correction's nine.
Full capture in `startup-selftests.log`.

## Device guards

```
> .\scripts\verify-device-guards.ps1
DEV2-01..07 PASS, DEV3-01..06 PASS, 19 executable surfaces scanned, 0 violations
All device-guard checks PASS.
```

## Devices attached during the whole correction

```
emulator-5560   CarPassport_API_36        (foreign, never targeted)
emulator-5570   BuildPlan_API_36_16K      (foreign, never targeted)
emulator-5580   ForgeShape_Stage006       (the one target, confirmed by `emu avd name`)
```

`emulator-5554` was **not attached and was never contacted**.
