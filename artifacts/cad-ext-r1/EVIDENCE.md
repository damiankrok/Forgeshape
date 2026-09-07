# EVIDENCE — `CAD-EXT-R1` (One Side / Symmetric / Two Sides, `CADB` v4)

**Baseline:** `18c782a9e39b5f440704502ecc61975e3cf6629a`, clean tree, no remote.
**Device:** `emulator-5580`, confirmed `ForgeShape_Stage006` by
`adb -s emulator-5580 emu avd name`. `emulator-5554` was never contacted.
**Automated elapsed:** 9.9 minutes (target ≤20, hard stop 30).

---

## 1. Startup self-tests

Log ring buffer enlarged to 64 MiB and confirmed (`logcat -g`) before capture.

```
22 *_SELFTEST_OK tokens, 0 *_SELFTEST_FAIL / *_FAIL:
FORGESHAPE_NATIVE_VIEWPORT_OK
FORGESHAPE_CAD_SELFTEST_OK (155 checks)     <- was 122; +33 CADEXT_* cases
FORGESHAPE_PROJECT_SELFTEST_OK (256 checks)
FORGESHAPE_SKETCH_UX_SELFTEST_OK (105 checks)
FORGESHAPE_CAD_A3_SELFTEST_OK (66 checks)
```

The `CADEXT_*` cases cover the two-distance reduction, the mesh offsets on all
three world planes, A/B independence, every transition of the §3.4 policy, the
canonical form and its refusals, `sameCadBodyState` and the fingerprint, Undo /
Redo of the whole extent, the face tokens and lineage signature surviving an
extent edit, the version-selection rule, the v4 round trip and the fail-closed
decoder.

## 2. The golden corpus — two independent implementations

`scripts\build-forge-corpus.ps1` is a second encoder written from
`DATA_PACKAGE_SPEC.md`. It produced the six new v4 fixtures **byte-identically
to the C++ encoder on the first run**, and every older fixture is unchanged.

```
-VerifyOnly : 36 fixtures listed, 0 with OnDisk=False
```

| Fixture | Bytes | SHA-256 |
| --- | ---: | --- |
| `cad_symmetric_v4.forge` | 257 | `6674e7225933ee2195292b6e43a091d3e327b7189ebd35fa1519191ecf92ab5a` |
| `cad_two_sides_v4.forge` | 249 | `8c49e09cca8133b81ef9bbfab75549ed111dc08b4057777ed077191cf25cab47` |
| `cad_face_extent_v4.forge` | 629 | `73739a226a30f8d015fe19663cd06062eeab7dab541f90795282ab5660510673` |
| `mixed_cad_extent_v4.forge` | 785 | `32ee99fccc5be71ed15003dc5b6856f7ed9c57b764c1032394b2c0b2f814f076` |
| `cad_bad_extent_v4.forge` | 257 | `15f1cecae31287643467d4b127e92918aca9677c5a1016b1def41a30d493b995` |
| `cad_bad_two_sides_v4.forge` | 249 | `aeb1b6784c546dde05a8e3227227aa19f2b396bee5d0f40981ad1e115414ea39` |

Printed on every debug launch as `FORGESHAPE_CAD_GOLDEN_SHA256_V4`, and asserted
by `CADEXT_10_c..h`. The two corrupt ones are refused
(`InvalidSemanticValue`) with nothing written, asserted by `CADEXT_10_j`.

All thirty pre-existing digests are byte-for-byte what they were, printed
unchanged as `FORGESHAPE_PROJECT_GOLDEN_SHA256*`.

## 3. Device suites

```
scripts\run-instrumented-tests.ps1 -Serial emulator-5580 -TestClass com.forgeshape.app.CadExtrudeExtentTest
  -> OK (5 tests), 48.5 s
scripts\run-instrumented-tests.ps1 -Serial emulator-5580 -TestClass com.forgeshape.app.CadCanvasExtrudeTest
  -> OK (9 tests), 82.9 s
```

Both are `MODE=FOCUSED_SUBSET`; no aggregate marker was emitted in either
direction, and `-FullSharded` was not run.

**Two real defects were found by the new suite on its first run**, both fixed:

1. A Two Sides side of **zero** left its value with no screen anchor, so the
   number could not be typed back up — the JNI projection was gated on the
   side's `present` flag. `present` now gates only the ARROW; a value with
   nowhere to GRAB is not a value with nowhere to STAND.
2. The suite's own expectation that a zero side is refused was wrong against the
   stated contract (a Two Sides side is non-negative, with at least one side
   positive). Corrected to assert acceptance, plus that a negative IS refused.

## 4. Builds and release symbol sanity

```
gradlew.bat :app:assembleDebug            -> exit 0
gradlew.bat :app:assembleDebugAndroidTest -> exit 0
gradlew.bat :app:assembleRelease          -> exit 0

release arm64-v8a : selftest dyn-syms = 0 ; extrude dyn-syms = 22
release x86_64    : selftest dyn-syms = 0 ; extrude dyn-syms = 22
debug   x86_64    : selftest dyn-syms = 24   (contrast)
```

## 5. Device guards

`scripts\verify-device-guards.ps1` — `DEV2-01..07` and `DEV3-01..06` all PASS,
`All device-guard checks PASS.` No device was contacted by it.

## 6. What did NOT change

* No `.forge` byte of any One Side project. A One Side extrusion is a direction
  and a positive depth, which every `CADB` version since v1 has carried, so the
  version-selection rule leaves such a project at v1, v2 or v3 exactly.
* No face token and no `CAD-A3` lineage signature: the signature takes no
  distance and no mode, asserted by `CADEXT_11_a`.
* No `SCNE`, `CONS`, `SCUL` or `IMPT` field, and no file major/minor.
* No boolean, no feature list, no `Add`, no `Cut`, no `CADB` v5.
