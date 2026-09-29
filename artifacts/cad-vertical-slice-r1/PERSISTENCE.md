# Persistence: `CADB` version 5

`DATA_PACKAGE_SPEC.md` §7f owns the byte layout; this page records the
decisions behind it and how it was proven. The spec was written first, then:

- the C++ codec (`forgeshape_project_document.cpp`);
- the fingerprint (`forgeshape_project_state.cpp`);
- the independent PowerShell encoder (`scripts/build-forge-corpus.ps1`),
  implemented from the spec text.

## 1. When v5 is written

`CADB` v5 is written only when at least one CAD body needs it, which means one
of two things:

- its extrusion selects something that is not the R0 profile: a region with
  holes, or more than one region;
- it carries later (Add/Cut) features.

Every other project writes v1, v2, v3 or v4 exactly as before.

- **Old fixtures unchanged.** All 36 pre-existing fixtures are byte-for-byte
  unchanged. `CI FAST` regenerates all of them with the PowerShell encoder and
  compares bytes.
- **Old digests unchanged.** The C++ self-tests still assert every old digest.
  The project suite is green on the host and on the device.
- **Old builds refuse v5.** An older build refuses a required `CADB` at an
  unknown version. It does not open a body with its holes silently filled or
  its features silently missing.
- **Legacy mapping.** A legacy one-feature state is the new in-memory form with
  an empty later-feature list and no extra regions. No migration exists.

## 2. What is stored, and what never is

- **Stored:** the authored truth.
  - the first feature's region selection: outer anchor plus hole anchors, and
    any additional regions;
  - per later feature: its id, its operation code (2 Add, 3 Cut), its support
    (earlier feature id, face kind, edge, lineage token), its sketch entities,
    its extent and distances, and its region selection.
- **Never stored:**
  - a vertex, a triangle or a face tag;
  - a kernel result;
  - a render index;
  - the candidate preview;
  - the Tool Labels preference.

## 3. Bounds

| Bound | Value | Why |
| --- | --- | --- |
| Features per body | 16 (1 base + 15 later) | a history step, a record and a regeneration stay finite |
| Regions per feature | 16 | same |
| Holes per region | 64 | same |
| Entities per sketch | 256 (unchanged) | existing R0 bound |
| Kernel result | 262 144 triangles | no chain of edits can grow a body without bound |

- **Checked before allocating.** Counts are checked **before any allocation**
  (`ImpossibleCount`).
- **Record size.** Per later feature the fixed record is 56 bytes, plus its
  entities and its `REGIONS` block.
- **Worst case.** 16 features × 256 entities is well under a megabyte.

## 4. Fail closed

The decoder refuses each of the following:

- an unknown operation or face code;
- ids that are not strictly ascending;
- a support that names no earlier feature;
- a stored hole set that is not EXACTLY the one the sketch derives;
- overlapping or touching regions;
- non-canonical order;
- anything `validateCadBodyState` refuses.

A body carrying later features is then **regenerated through the boolean kernel**
as part of validation. A file whose Add would be disjoint, or whose Cut would
miss, is therefore refused rather than opened as a body the editor could never
have produced (`CADVS_IO_18`).

## 5. Fixtures

There are eight new fixtures, and the corpus is now **44**.

| Fixture | Bytes | SHA-256 | Proves |
| --- | ---: | --- | --- |
| `cad_region_hole_v5` | 302 | `c6d269425cfa02d749e00b6fe922edf1d6896e6d488a1bad52efbfb59ece91f5` | the owner's rectangle-with-a-hole |
| `cad_feature_add_v5` | 370 | `2cb25af694e26a1d186374ec7e691d500201068eeb0606e08dc486c2d637f264` | base + one Add |
| `cad_feature_cut_v5` | 362 | `9f4efdb626fea351790a0063308b422954aa49f1162e199027d5d04b43b66cbc` | base + one Cut |
| `cad_feature_chain_v5` | 496 | `a4a65681d08c093a9179d5c6be22a56fe2d7b2347670d2231229c2ef0230f10f` | holed base + Symmetric Add + Cut, applied in order |
| `cad_bad_operation_v5` | 370 | `b04b776b29189ffe7262b5ba95f9d106e5a5f37fa6f6c8a8566a5b31de002a23` | operation code 9 refused |
| `cad_bad_feature_ref_v5` | 370 | `79436a25e7177fa818b7ac4dac1476254658c9e09f1c3a49b0f042f560b738cd` | support names a missing feature |
| `cad_bad_feature_order_v5` | 370 | `7d50931ddf3f7a734454beaed51576225aae0cc1d990b94ba5bc3ab8d7c4e951` | a later feature wearing id 1 |
| `cad_bad_region_v5` | 302 | `17d27c5c46b9b9f7d606d278d524aff6cfbc48647504497825c8438f2493f7a0` | stored holes that the sketch does not derive |

The full digests are in `DATA_PACKAGE_SPEC.md` §11.

**Two independent implementations.** The C++ encoder, driven by the self-test
(`cadFeatureFixtureDigests()`), produces the same SHA-256 for all eight as the
PowerShell encoder. `CADVS_IO_22` pins that. `CADVS_IO_23` pins the two lineage
tokens:

- plain rectangle: `0x958F78AF1C70BAA1`, unchanged from the v2 fixtures;
- holed rectangle: `0x937BA1514FAF7381`.

**Round trip.** Each v5 document decodes to the same state, re-encodes
byte-identically and validates (`CADVS_IO_01..12`). On the device,
`CadVerticalSliceTest.feature_edit_roundtrip` saves a two-feature body, reopens
it, and checks three things: the same volume, the same triangle count, and
byte-identical re-encoding.

## 6. Fingerprint

`projectSemanticFingerprint` mixes the new fields ONLY when they are present:

- regions under a `REGN` marker;
- later features under a `FEAT` marker.

So the fingerprint of every pre-existing project is unchanged, and a later
feature moves it (`CADVS_IO_20`).

## 7. Cost

Encoding and decoding a body with later features regenerates its chain once to
validate it, which is one kernel run per later feature. The host measurement
(`PERF_NOTES.md`) is a codec round trip of about 4–7 ms for the three-feature
acceptance model. Autosave runs on its own worker thread, so no frame pays it.
