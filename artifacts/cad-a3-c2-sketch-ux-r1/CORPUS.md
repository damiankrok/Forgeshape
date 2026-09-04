# The independent corpus (`SKETCH-UX-R1` H)

`scripts/build-forge-corpus.ps1` writes every fixture from
`DATA_PACKAGE_SPEC.md` and **shares no line with the production codec**. The
C++ self-test builds the same documents through `encodeProjectV1` and asserts
the digests. Agreement is the format being a specification rather than whatever
one encoder happens to emit.

## The six new `CADB` v3 fixtures

| Fixture | Bytes | SHA-256 | What it pins |
| --- | --- | --- | --- |
| `cad_arc_profile_v3.forge` | 301 | `580a47dcfa305ddec34c2ed7831ba88b7a05659008e7944285ca2a70fc02b8cd` | The three-point Arc encoding. One body: a semicircle closed by a line — the smallest v3 file there is |
| `cad_spline_profile_v3.forge` | 321 | `e3ff4f7be2529a30f040bd3d699b46df9792473b88755494144c15697b19261a` | The Spline's `pointCount` + points, and nothing derived |
| `cad_mixed_curve_profile_v3.forge` | 892 | `3e2fa16f05e353f1745a36e165aedadbb5d5378db0c039167293aececad7078d` | An Arc body, a Spline body and a Rectangle body in one v3 section, with a **sparse** `CONS` beside them |
| `cad_face_curve_v3.forge` | 478 | `0a8218f0aa86cfb7cdcf7781c72864e76e9065e2f7a788d5b2cf4e6fc1250ad0` | A curve profile on a producer's far cap: **v3 carrying a v2 support block**, which is what makes a version a superset |
| `cad_bad_arc_v3.forge` | 301 | `628d74fdbf3cf9082ef4869f9b948acef81c6a14517703b707896602e7c4b418` | Three **collinear** points. No circle passes through them |
| `cad_bad_spline_v3.forge` | 321 | `f5437366d894032e97b2e49d3027726fa2bc739bda747f05f3b1eaf8c9ed714d` | A spline whose two **ends coincide** — a loop the chain walker cannot read |

Every length, count and CRC in the two corrupt files is **correct**. Only the
semantic check can refuse them, which is exactly what makes them worth having:
they prove the refusal is the domain's and not an accident of a broken length.

## The two routes to the corrupt pair

- **PowerShell** constructs them with the bad value in place, never generated
  and then mutated — the rule `CAD-A3-C1` established for its own corrupt pair.
- **C++** encodes the valid counterpart and writes the bad values over their own
  fixed-width fields, then recomputes the payload CRC. Every length, count and
  offset is unchanged by construction.

Two routes, one file. The digests agreeing is what proves it.

## The twenty-two older fixtures

**All byte-for-byte unchanged.** Re-verified by running the builder and
comparing against the digests `DATA_PACKAGE_SPEC.md` already recorded:

```
cad_face_sketch_cap_v2   4e4bdacc20954b063ef38d81a0c2c1c4bb4faff97255aea1e11d161bec560a86
cad_face_sketch_side_v2  9afa0ae2036cc1c03ee2e49f42fe45c19e79a8f050c9c87a2202205e10ed5a3d
cad_face_chain_v2        24ad47b5b5d600a47860ccafc3a644fa22e3d0bde994c43d969567cb299580b7
mixed_cad_face_v2        4b20f3c8ea05876850043dff28591f19855efa4c0566bebd43df11f1c0ff1529
cad_bad_face_ref_v2      2f728f27393ff19b9dfa327be8b6598f26eb595dfe9a8c399a0db5e8cd50f564
cad_dependency_cycle_v2  88072d355efa54f95e6d68c10d15c81a9cfebc0e2ed1f231255ebe5a42b117f0
cad_rectangle_v1         e2fd79c4070d1168ae883e064200438244c09a41bcf1682f9a0596b549d1b26b
cad_circle_v1            886b1538a1a20113316b7badcc7c6aaca6b17ccb54d3dcfed042ff41866e4559
mixed_cad_v1             94f014cdc01fe8beaa14301ef2a99c0805a7e13afe1bca0126b29026882c93db
cad_bad_plane_v1         60476603b6c1b1e1cf6b785e863c953ee6aa63573c1a2453bd7db0935909aba9
construction_multibody   8830e7fbd8dcb803535d5c4f91e410dfca4c7a0cecc1202c9d2b1aa8553b9aaf
sculpt_mixed             112b109731a43bf57a0f77b34794e7ce2529e056d9b18f061cd3c891f51a2784
imported_only            0f42be318da9faf4aa780e152b8550171085a267882d5e6e69cc9ef29a1539a8
construction_imported    539e10e7a9e388ab1ca72867b78c1e461d3bbe0ef5876d87fa54bc6bd6ae7a51
mixed_imported           3fdc82a099da69b93552d7c84c56002ed8ae24ba7086ddbc6a69a9bbf671f1fb
imported_sculpt          b82430cf6dbb82fddf075722d7ae335460f687d2a06cde09db43817323729f76
mixed_imported_sculpt    ab709ecea27ec29f21b6fbef126e8cdc15dc5c733d9b751bd1c8832907f27a2b
```

plus the five malformed-file fixtures, unchanged.

**Twenty-eight fixtures in all.**

## What the parity found

One real defect, caught before it reached a fixture: the C++ helper for the
spline corpus body used `0.8` where the PowerShell one used `0.75`. `0.75` is an
exact binary fraction and `0.8` is not — a value the two encoders could round
differently is a byte-parity argument waiting to happen. The C++ helper was
corrected, and the fixture comment now states the rule.

(The equivalent parity in `CAD-A3-C1` found a mistyped FNV basis in production
code. This one found a test-fixture value, which is the less serious of the two
and still worth having found.)

## Reproducing

```
powershell -ExecutionPolicy Bypass -File scripts\build-forge-corpus.ps1
powershell -ExecutionPolicy Bypass -File scripts\build-forge-corpus.ps1 -VerifyOnly
```

and the C++ side, on any launch, as `FORGESHAPE_SKETCH_UX_SELFTEST_OK`.
