# IMPORT-01B — the `.forge` golden corpus

Regenerated and verified with the independent PowerShell encoder, which is
written from `DATA_PACKAGE_SPEC.md` and shares no line with the production codec:

```
powershell -ExecutionPolicy Bypass -File scripts\build-forge-corpus.ps1
powershell -ExecutionPolicy Bypass -File scripts\build-forge-corpus.ps1 -VerifyOnly
```

## Every fixture, after the change

| Fixture | Bytes | SHA-256 | Status |
| --- | ---: | --- | --- |
| `construction_multibody_v1.forge` | 1264 | `8830e7fbd8dcb803535d5c4f91e410dfca4c7a0cecc1202c9d2b1aa8553b9aaf` | UNCHANGED |
| `sculpt_mixed_v1.forge` | 629 | `112b109731a43bf57a0f77b34794e7ce2529e056d9b18f061cd3c891f51a2784` | UNCHANGED |
| `corrupt_crc_v1.forge` | 1264 | `df353c419215864d8de3aca6e2d43733c29740e2a1be4daeeb188af7c9728b96` | UNCHANGED |
| `truncated_v1.forge` | 1224 | `af80dae6c3bc9f8854a326d46090aa54f93ef59c193a22893c335135e6b04204` | UNCHANGED |
| `unsupported_major_v1.forge` | 1264 | `27df45ad6bff0577514df65aa8773024203197c6323cb9c625003a67a7b6d77a` | UNCHANGED |
| `unknown_optional_v1.forge` | 1296 | `85286d94592b783644d2dc00744538325f6d91557de32235a6038595d56a57ca` | UNCHANGED |
| `unknown_required_v1.forge` | 1296 | `c71553ef0d4f0be28a8972418b981b47ee0c19ae715058719ad7f0a3cd0db5cd` | UNCHANGED |
| `imported_only_v1.forge` | 348 | `0f42be318da9faf4aa780e152b8550171085a267882d5e6e69cc9ef29a1539a8` | UNCHANGED |
| `construction_imported_v1.forge` | 570 | `539e10e7a9e388ab1ca72867b78c1e461d3bbe0ef5876d87fa54bc6bd6ae7a51` | UNCHANGED |
| `mixed_imported_v1.forge` | 905 | `3fdc82a099da69b93552d7c84c56002ed8ae24ba7086ddbc6a69a9bbf671f1fb` | UNCHANGED |
| `imported_sculpt_v1.forge` | 489 | `b82430cf6dbb82fddf075722d7ae335460f687d2a06cde09db43817323729f76` | **NEW** |
| `mixed_imported_sculpt_v1.forge` | 1266 | `ab709ecea27ec29f21b6fbef126e8cdc15dc5c733d9b751bd1c8832907f27a2b` | **NEW** |

**All ten pre-existing digests are byte-identical.** Generalizing `SCUL` changed
no byte of any file that already existed, because it removed a refusal rather
than adding a field.

## What the two new fixtures are

### `imported_sculpt_v1.forge` — `SCNE + IMPT + SCUL`, no `CONS` at all

One body. The Imported Mesh every imported fixture shares (`head_low`, four
vertices, two submeshes with different `doubleSided`, at
`(1.5, -0.25, 4.0)` unrotated and unscaled) plus a Frozen Sculpt Mesh on it.
`projectKind = Sculpt`, header flags `6` = `hasSCUL | hasIMPT`.

It is the fixture that pins the generalized rule, and it pins it in the strongest
form available: there is no `CONS` section anywhere in the file, so a reader that
still required a Construction Source beside a sculpt mesh refuses it.

### `mixed_imported_sculpt_v1.forge` — all four combinations at once

Four bodies, interleaved rather than grouped:

| body | source | sculpt |
| ---: | --- | --- |
| 1 | `CONS` (Box) | — |
| 2 | `CONS` (Sphere) | yes, stale, edited |
| 3 | `IMPT` | — |
| 4 | `IMPT` | yes, edited |

`projectKind = Sculpt`, active body 4, header flags `7` = all three. The `SCUL`
section carries entries over bodies of BOTH source kinds, so a reader that keyed
a sculpt entry to a Construction body fails here, and both subsequences —
`CONS` over 1,2 and `IMPT` over 3,4 — must be read as subsequences of `SCNE`
rather than as contiguous blocks.

## The sculpt mesh both new fixtures use

A four-vertex tetrahedron with twelve indices, `renderBothSides = true` (the
imported source has a two-sided submesh), `sourceStale = false` (an Imported Mesh
cannot go stale), `hasEdits = true`.

It is deliberately **not** the imported geometry it was frozen from. A fixture
where the two matched could not tell a decoder that confused them apart. Every
number is an exact binary fraction — `0.25`, `1.75`, `2.5`, `0.5`, `0.75`,
`3.25` — so neither implementation has a rounding argument to make.

## Where they are asserted

- **Digest**, against the committed value:
  `IMP01B_11_imported_sculpt_fixture_matches_the_committed_digest` and
  `IMP01B_12_mixed_imported_sculpt_fixture_matches_the_committed_digest`.
- **Round trip**, bit for bit: `IMP01B_11_the_imported_sculpt_fixture_roundtrips_bit_for_bit`
  and `IMP01B_12_the_mixed_imported_sculpt_fixture_roundtrips_bit_for_bit`
  (decode → `sameProjectDocument` → re-encode → identical bytes).
- **Live round trip**: loaded into a real `ConstructionScene`, then
  `captureProjectDocument` re-encoded and compared to the same bytes —
  `IMP01B_11_capturing_it_again_produces_the_same_document` and
  `IMP01B_12_recapturing_the_mixed_project_produces_the_same_bytes`.
- **On a device**: both are packaged into the test APK's assets and loaded by
  `ImportedMeshDurableTest.imp01a19_theCommittedImportedFixturesLoadOnTheDevice`,
  which proves the independent encoder's bytes actually open rather than only
  hashing the same.

Both are logged at every debug launch as
`FORGESHAPE_PROJECT_GOLDEN_SHA256_IMPORTED_SCULPT`, so drift is a value a human
can read out of logcat.
