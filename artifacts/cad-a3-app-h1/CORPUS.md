# The `.forge` corpus after `CAD-A3-C1` — twenty-two fixtures

`scripts/build-forge-corpus.ps1` is a second, independent implementation of the
encoder written from `DATA_PACKAGE_SPEC.md`. This pass extended it to `CADB`
v2 (`New-CadPayloadV2`) and to the lineage token (`Get-CadTopologySignature`,
reimplemented from §7c's text), and added six fixtures. Every one of the
sixteen v1 fixtures is byte-for-byte unchanged (`-VerifyOnly` reports every
digest matching the committed file, and the native `FSR1A-12`, `IMP01A-19`,
`IMP01B-11/12` and `CADR0-33/34/36` pins are untouched).

## The six v2 fixtures

| Fixture | Bytes | SHA-256 (PowerShell = C++) | Decoder |
| --- | ---: | --- | --- |
| `cad_face_sketch_cap_v2.forge` | 425 | `4e4bdacc20954b063ef38d81a0c2c1c4bb4faff97255aea1e11d161bec560a86` | Ok, round-trips bit for bit |
| `cad_face_sketch_side_v2.forge` | 425 | `9afa0ae2036cc1c03ee2e49f42fe45c19e79a8f050c9c87a2202205e10ed5a3d` | Ok, round-trips bit for bit |
| `cad_face_chain_v2.forge` | 594 | `24ad47b5b5d600a47860ccafc3a644fa22e3d0bde994c43d969567cb299580b7` | Ok, round-trips bit for bit |
| `mixed_cad_face_v2.forge` | 923 | `4b20f3c8ea05876850043dff28591f19855efa4c0566bebd43df11f1c0ff1529` | Ok, round-trips bit for bit |
| `cad_bad_face_ref_v2.forge` | 425 | `2f728f27393ff19b9dfa327be8b6598f26eb595dfe9a8c399a0db5e8cd50f564` | **refused `InvalidSemanticValue`** |
| `cad_dependency_cycle_v2.forge` | 594 | `88072d355efa54f95e6d68c10d15c81a9cfebc0e2ed1f231255ebe5a42b117f0` | **refused `UnresolvedReference`** |

What each is, exactly, is in `DATA_PACKAGE_SPEC.md` §11. The two corrupt
fixtures are CONSTRUCTED by the PowerShell builder with the bad value in place
(side index 7 of a four-sided rectangle; B supported on C and C on B with B's
lineage set to C's own signature), never generated and then mutated. The C++
self-test reaches the same bytes by patching its valid parent's one field and
the CRC — and the digests agreeing is the proof that the two routes describe
one file, while the C++ decoder refusing them for exactly the planted reason
(`CADA3_50`, `CADA3_51`) is the proof they are refused semantically and not
structurally.

## Parity found a production defect

The first comparison disagreed on all six fixtures. Feeding the PowerShell files
to the production decoder (a scratch runner over the same object files) refused
every one as `InvalidSemanticValue`, and printing both sides' rectangle
signature isolated it: `forgeshape_cad_face.cpp` spelled the FNV-1a offset
basis as `1469598103934665603` — one digit short of the basis its own comment
named (`14695981039346656037`, `0xCBF29CE484222325`). Because a reader
recomputes the producer's signature and compares it with the stored token, that
constant IS the format, and a spec written from the code's stated intent could
not match the code. The contract was fixed to the stated algorithm; the six
digests were then identical on both sides with no change to the PowerShell
arithmetic. No shipped `.forge` carried a v2 token before this pass, so nothing
existing was invalidated. `projectSemanticFingerprint` (an in-process change
detector, never a file field) keeps its own constant and was not touched.

A second, PowerShell-side pitfall was closed on the way: Windows PowerShell
5.1's infix bitwise operators are not defined for `BigInteger`, so the
signature now uses the explicit `op_*` operator methods and a 64-bit mask.

## Verify

```
powershell -ExecutionPolicy Bypass -File scripts\build-forge-corpus.ps1 -VerifyOnly
```

prints every fixture with `OnDisk True`, and a debug launch prints the same six
digests as `FORGESHAPE_PROJECT_GOLDEN_SHA256_CAD_V2`. `CADA3-46..51` assert them
on device.
