# IMPORT-01A — the `.forge` corpus after the Imported Mesh branch

`testdata/forge/v1/`, written by `scripts/build-forge-corpus.ps1` — a second,
independent implementation of the v1 encoder written from `DATA_PACKAGE_SPEC.md`
that shares no line with the C++ codec. The two agree exactly when these digests
match the ones `forgeshape_project_selftest.cpp` asserts and a debug launch
prints as `FORGESHAPE_PROJECT_GOLDEN_SHA256` and
`FORGESHAPE_PROJECT_GOLDEN_SHA256_IMPORTED`.

## The seven legacy fixtures are UNCHANGED

The imported branch costs a project that has none exactly nothing: no `IMPT`
section, no `hasIMPT` header flag, and therefore not one byte. That is the claim
`IMP01A-19` makes and these digests are the evidence for it.

| Fixture | Bytes | SHA-256 | Changed by IMPORT-01A? |
| --- | ---: | --- | --- |
| `construction_multibody_v1.forge` | 1264 | `8830e7fbd8dcb803535d5c4f91e410dfca4c7a0cecc1202c9d2b1aa8553b9aaf` | no |
| `sculpt_mixed_v1.forge` | 629 | `112b109731a43bf57a0f77b34794e7ce2529e056d9b18f061cd3c891f51a2784` | no |
| `corrupt_crc_v1.forge` | 1264 | `df353c419215864d8de3aca6e2d43733c29740e2a1be4daeeb188af7c9728b96` | no |
| `truncated_v1.forge` | 1224 | `af80dae6c3bc9f8854a326d46090aa54f93ef59c193a22893c335135e6b04204` | no |
| `unsupported_major_v1.forge` | 1264 | `27df45ad6bff0577514df65aa8773024203197c6323cb9c625003a67a7b6d77a` | no |
| `unknown_optional_v1.forge` | 1296 | `85286d94592b783644d2dc00744538325f6d91557de32235a6038595d56a57ca` | no |
| `unknown_required_v1.forge` | 1296 | `c71553ef0d4f0be28a8972418b981b47ee0c19ae715058719ad7f0a3cd0db5cd` | no |

## The three new fixtures

| Fixture | Bytes | SHA-256 | Semantic inventory |
| --- | ---: | --- | --- |
| `imported_only_v1.forge` | 348 | `0f42be318da9faf4aa780e152b8550171085a267882d5e6e69cc9ef29a1539a8` | ONE body, Imported Mesh, **no `CONS` at all**. `projectKind = Construction`, `headerFlags = 0x04`, sections `SCNE` + `IMPT`, `nextObjectId = 2`, active = 1. The fixture that proves an imported object needs no Construction Source standing in for it |
| `construction_imported_v1.forge` | 570 | `539e10e7a9e388ab1ca72867b78c1e461d3bbe0ef5876d87fa54bc6bd6ae7a51` | TWO bodies: id 1 a Box at the identity, id 2 the Imported Mesh. `headerFlags = 0x05`, sections `SCNE` + a **sparse** `CONS` (one entry, not two) + `IMPT`, `nextObjectId = 3`, active = 2 |
| `mixed_imported_v1.forge` | 905 | `3fdc82a099da69b93552d7c84c56002ed8ae24ba7086ddbc6a69a9bbf671f1fb` | THREE bodies, representations INTERLEAVED: id 1 a Box, id 2 a Sphere carrying an edited Frozen Sculpt Mesh, id 3 the Imported Mesh. `projectKind = Sculpt`, `headerFlags = 0x07`, sections `SCNE` + `CONS` (optional companion) + `SCUL` + `IMPT`, `nextObjectId = 4`, active = 2 |

All three carry the same Imported Mesh, chosen so nothing can pass by symmetry:

* name `head_low` (8 bytes UTF-8);
* 4 vertices at `(0,0,0)`, `(1.5,0,0)`, `(0,2.25,0)`, `(0,0,3.5)`, no two alike;
* 4 unit normals, each along a different axis, one of them negative;
* 6 indices — `0,1,2` then `0,2,3`;
* **2 submeshes with DIFFERENT `doubleSided` answers** — `{0,3,false}` then
  `{3,3,true}`. One answer for the whole mesh could not tell them apart;
* placement `(1.5, -0.25, 4.0)` with rotation `0,0,0` and scale `1,1,1`, which is
  what an import produces: the node's translation kept, its linear part baked.

Every number is an exact binary fraction, so neither implementation has a
rounding argument to make.

## Determinism

`encode → decode → encode` is byte-identical for all three
(`IMP01A_19_the_imported_fixtures_roundtrip_bit_for_bit` and its two siblings),
and `sameProjectDocument` compares positions AND normals by their float BITS, so
a value that came back one ULP away is a failure rather than a rounding
difference.

Five of the ten fixtures are packaged into the test APK's assets, so
`ImportedMeshDurableTest.imp01a19_theCommittedImportedFixturesLoadOnTheDevice`
proves the independent encoder's bytes actually LOAD — with the representations
the specification states — rather than only hashing the same.

## Regenerate and verify

```
powershell -ExecutionPolicy Bypass -File scripts\build-forge-corpus.ps1
powershell -ExecutionPolicy Bypass -File scripts\build-forge-corpus.ps1 -VerifyOnly
```
