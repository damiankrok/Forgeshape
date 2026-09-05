# Build, symbols and size — before / after

"Before" libraries: the stripped `.so` files the deep audit built at `d783d4b`,
whose native sources are byte-identical to the baseline `af6a719`
(`BASELINE.md`). "After": this stage's tree, built 2026-09-05 14:09
(`app/build/intermediates/stripped_native_libs/<config>/.../libforgeshape_native.so`,
the same bytes that are packaged into the APKs). Tooling: NDK 29.0.14206865
`llvm-readelf --dyn-syms`, `llvm-strings`, `llvm-readelf -l`.

## Release (PAH-R1-09, PAH-R1-10, PAH-R1-11)

| ABI | before (bytes) | after (bytes) | delta | `*SelfTests*` dynamic symbols before → after | check-name strings (`FSR1A_*`, `CADUXR1_*`, `DAR1_*`, `*_SELFTEST_*`) before → after | JNI exports before → after |
| --- | --- | --- | --- | --- | --- | --- |
| arm64-v8a | 2 084 968 | 1 171 552 | −913 416 (−43.8 %) | 20 → **0** | 181 → **0** | 155 → 155 |
| x86_64 | 2 339 080 | 1 236 296 | −1 102 784 (−47.1 %) | 20 → **0** | 181 → **0** | 155 → 155 |

`app-release-unsigned.apk`: 2 617 971 bytes, containing exactly those two
libraries (`unzip -l`). ELF `LOAD` alignment 0x4000 on both (16 KB-page
requirement unchanged). The audit's "≈ 500 KB" estimate undershot: the suites
carried their fixtures and the CAD/sketch geometry they build.

## Debug (PAH-R1-08 — unchanged composition)

| ABI | before (bytes) | after (bytes) | delta | `*SelfTests*` dynamic symbols | check-name strings | JNI exports |
| --- | --- | --- | --- | --- | --- | --- |
| arm64-v8a | 3 515 704 | 3 521 816 | +6 112 | 22 → 22 | 241 → 241 | 155 → 155 |
| x86_64 | 3 502 528 | 3 508 608 | +6 080 | 22 → 22 | 241 → 241 | 155 → 155 |

The debug delta is the F-08 locking (a dozen extra lock scopes, two local copy
buffers) and the F-11 checks; the self-tests are all still there and all still
run (`DEVICE_STARTUP.txt`).

## F-15 symbol (PAH-R1-14)

`buildSpherifiedBox` in the dynamic symbol table: before **1** (release AND
debug, both ABIs — it was exported, not merely external), after **0** in all
four. `buildFixtureLarge`/`buildStressMesh`, its two callers, still link
(release and debug both build; the debug stress harness that uses them is
reachable only from `debugMeshCommand`).

## Per-configuration source lists

| Configuration | self-test TUs in `compile_commands.json` | total TUs |
| --- | --- | --- |
| Debug | 20 | 61 |
| RelWithDebInfo (release) | 0 | 41 |

Nothing unrelated to the self-tests was removed from the release build: the 41
release TUs are the baseline's 61 minus exactly the 20 `*_selftest.cpp` files.
