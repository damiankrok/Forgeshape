# CAD-A3 + APP-H1 - evidence index (closed by `CAD-A3-C1`)

**Result: `PASS-CAD-A3-APP-H1-OWNER-RETEST-READY`** — see `TEST_RESULTS.md`
for the per-criterion table and the aggregate line at the end of this file.

The journey the owner will retest:

`cold launch -> Home -> New Project -> CAD -> tap a world plane in the
viewport, tap it again -> exact orthographic sketch -> draw -> Finish Sketch ->
depth -> Extrude = the first durable body and the project -> New Sketch -> tap
a planar face of that body, tap it again -> draw -> Extrude = a face-supported
dependent -> Save -> Home -> Open File -> the dependency restored`, with New
Project -> Sculpt, Open File from Home, the unsaved-changes guard and a
deterministic Back beside it.

## What this pass added (Parts A-K of the brief)

- **Home, New Project, the CAD bootstrap, the dirty guard** (`HOME_FLOW.md`,
  `BOOTSTRAP_SESSION.md`): Home is not a project; the process scene starts
  empty; the first Extrude creates the project through the one validated load
  path; leaving a dirty project asks Save / Discard / Cancel.
- **The independent CADB v2 corpus** (`CORPUS.md`, `CADB_V2_DATA_CONTRACT.md`):
  six fixtures, PowerShell bytes = C++ bytes, the two corrupt ones refused for
  their planted reason — and the parity found a mistyped FNV basis in the
  production lineage token, now fixed and stated in the spec.
- **Device E2E** (`DEVICE_E2E.md`): `HomeFlowTest` (12), the widened
  `SpatialSketchTest` (8, hover included), `CadA3VisualEvidenceTest` (1), the
  reworked `SketchExtrudeTest` and the seven classes the Home change touched.
- **Screenshot evidence** (`VISUAL_EVIDENCE.md`, `OWNER_CONTACT_SHEET.png`,
  `captures/`): ten composed-display frames with measured facts.
- **New Sketch lands directly in the spatial chooser**; the by-name planes stay
  as the accessibility fallback (`SPATIAL_PLANE_PICKING.md`).

The CAD-A3 core — `FACE_TOPOLOGY.md`, `TOPOREF_CONTRACT.md`,
`DEPENDENCY_GRAPH.md`, `TRANSFORM_CONTRACT.md`, `GRID_CAMERA_UX.md`,
`PERFORMANCE.md` — is unchanged by this pass apart from the FNV basis.

| Gate | Result |
| --- | --- |
| Resume baseline | `4556f769fb424c65b717b2fac7a292aaa2b437c0` (matched, clean) |
| Native suites, clean launch | **19/19**, 2920 checks, 0 failed; `FORGESHAPE_STARTUP_NO_PROJECT bodies=0` (`NATIVE_SELFTEST_CADA3.txt`) |
| Standalone runner | 17 platform-neutral suites, 2567 checks, 0 failed |
| Golden digests | eleven v1 unchanged; six v2 pinned, PowerShell = C++ |
| JVM | 70/70 |
| Builds | debug + release, `arm64-v8a` + `x86_64` |
| Device guards | `DEV2-01..07`, `DEV3-01..06` PASS |
| Corpus | twenty-two fixtures `OnDisk True`; sixteen v1 byte-identical |
| Focused device | `HomeFlowTest` OK (12), `SpatialSketchTest` OK (8), `SketchExtrudeTest` OK (9), `CadA3VisualEvidenceTest` OK (1), seven touched classes OK (`FOCUSED_HOME_SPATIAL.txt`, `FOCUSED_VISUAL_EVIDENCE.txt`; first-fail transcripts `FOCUSED_ROUND1_FAIL.txt`, `FOCUSED_ROUND2_DEVICE_CRASH.txt`, `FOCUSED_ROUND3_FAIL.txt`) |
| Authoritative aggregate | **`FULL_SHARDED_SUITE_PASS`** — 37 classes / 504 tests / 5 shards (100 + 104 + 99 + 101 + 100), missing = duplicates = unexpected = execution_missing = 0 (`FULL_SHARDED.txt`; five earlier aggregates kept as `FULL_SHARDED_RUN1..5_FAIL.txt`, see `DEVICE_E2E.md`) |
| Device | `emulator-5580` = `ForgeShape_Stage006`; `emulator-5554` never contacted |

## Deferred beyond this stage (by the brief's own scope)

CAD → Sculpt, custom construction planes, curved-face sketches, sketching on an
imported or sculpted surface, an independently movable dependent, the
face-first contextual shortcut, projected edges, a constraint solver, and every
boolean / fillet / chamfer. Hardware stylus hover is verified only as a
synthesized hover through the real dispatch path, because the authoritative
emulator has no stylus.
