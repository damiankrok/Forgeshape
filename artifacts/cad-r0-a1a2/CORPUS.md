# Corpus - the four CAD-R0-A1A2 fixtures

Written by `scripts/build-forge-corpus.ps1` (the independent PowerShell encoder)
into `testdata/forge/v1/`, and asserted by the C++ project self-test
(`CADR0_33/34/36_*_matches_the_committed_digest`) against the same digests.
Both encoders produced identical bytes on the first run; no reconciliation was
needed.

| Fixture | Bytes | SHA-256 | What it is |
| --- | ---: | --- | --- |
| `cad_rectangle_v1.forge` | 247 | `e2fd79c4070d1168ae883e064200438244c09a41bcf1682f9a0596b549d1b26b` | one CAD Body, rectangle on XZ, depth 1.5 along +Y, placement with 370 deg and non-uniform scale; **no `CONS` at all** |
| `cad_circle_v1.forge` | 239 | `886b1538a1a20113316b7badcc7c6aaca6b17ccb54d3dcfed042ff41866e4559` | one CAD Body, circle r 0.75 on YZ, depth 0.5 **against** the normal |
| `mixed_cad_v1.forge` | 780 | `94f014cdc01fe8beaa14301ef2a99c0805a7e13afe1bca0126b29026882c93db` | a Construction Box beside two CAD Bodies - a closed 5-point polyline profile with an unrelated open line in the same sketch (XY, depth 2.0), and a loop of three lines (XZ, depth 0.25) - a SPARSE `CONS` next to a `CADB` carrying every entity kind and both profile-closing rules |
| `cad_bad_plane_v1.forge` | 247 | `60476603b6c1b1e1cf6b785e863c953ee6aa63573c1a2453bd7db0935909aba9` | the rectangle fixture with its workplane code set to 9 and the `CADB` CRC **recomputed**, so every length and checksum is right and only the semantic check refuses it (`InvalidSemanticValue`) |

The twelve older fixtures are unchanged (all `OnDisk = True`, all seven prior
digests identical, printed on every debug launch as
`FORGESHAPE_PROJECT_GOLDEN_SHA256`, `..._IMPORTED`, `..._IMPORTED_SCULPT`, and
now `..._CAD`).

Device-side, `E2E-CADR0-11` proves a live CAD project encodes, validates,
reloads to the same fingerprint and bytes, and stays editable.
