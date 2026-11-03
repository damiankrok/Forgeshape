# CAD-FACE-SUPPORTED-PLACEMENT-R1 — SUMMARY

**Result: `PASS-CAD-FACE-SUPPORTED-PLACEMENT-R1`.**

| Gate | Status |
| --- | --- |
| Technical | TECH PASS (emulator) |
| OWNER physical review | REQUIRED |
| Merge | NOT MERGED |

No claim of compliance with the full Master Plan is made: v4.1.61 is still
absent.

| Item | Value |
| --- | --- |
| Branch | `feature/cad-face-supported-placement-r1` |
| Started from | exactly `7c2fc91` (closeout head; product = C2 `70a6c14`) |
| Product frozen at | `912b861` |
| Preflight | `BEFORE.md` |
| Design | `DESIGN.md` |
| Acceptance | `ACCEPTANCE_MATRIX.md` |
| Every attempt | `TESTS.md` |

## What was wrong (one cause, two symptoms)

A CAD body sketched on another body's face is placed by
`ConstructionScene::resolveWorldModel` (producer × face frame). The renderer
and the picker used that placement; two other consumers did not.

- **CR-01, export.** The exporter read the body's inert stored transform, so
  the file put the body at the origin.
- **CR-02, transform and gizmo.** These wrote the stored transform. That added
  an Undo step, moved the fingerprint and wrote `.forge` bytes, while nothing
  moved on screen and the gizmo was drawn 3 m away from the body.

## What changed

| Area | Change |
| --- | --- |
| Export | Places such a body by `resolveWorldModel`: translation on the node, the linear part baked in with transpose-inverse normals. An unresolved support is refused by name (`UnresolvedPlacement`) with zero bytes. Independent bodies are byte-identical to before. |
| Typed write | `applySceneBodyTransform` refuses `DerivedPlacement` before any edit opens. JNI returns `APPLY_REJECTED_DERIVED_PLACEMENT` (11). |
| Gizmo | Neither drawn nor hit nor dragged over such a body. `setGizmoActive` refuses it. |
| UI | Transform, the gizmo and the placement editor are withdrawn. Holding Transform when such a body is selected moves the rail to Shape. |
| Stale test | `uir4a04` now asserts the exact set of 12 resting tiles. |

Unchanged: `.forge`, `CADB`, the fixtures, `DATA_PACKAGE_SPEC.md`,
`resolveWorldModel`, face identity, lineage and tokens, and the stored
transform's decoding (OD-16).

## Evidence

**RED, on the unchanged product.**
- Host: 18 checks fail (GLTF_EXPORT 14, GIZMO 4).
- The typed-write reproducer adds an Undo step and moves the fingerprint.
- `uir4a04`: `expected:<8> but was:<12>`.
- The committed fixtures: 11 MISMATCH.

**GREEN, local.**

| Check | Result |
| --- | --- |
| Host self-tests | `HOST_SELFTESTS_OK (4434 checks, 0 failed)`, 25/25 suites, oracles 17 |
| JVM | 196/0 |
| Corpus verdicts | 72 |
| Builds | debug, androidTest, release |
| Release guard | PASS |
| `git diff --check` | clean |
| E05 corpus export | 0 MISMATCH / 23 MATCH |

**CI DEVICE (emulator, sequential, under the 2700 s cap).**

| Run | Result | Note |
| --- | --- | --- |
| `41078310302` | 86/87 | The one failure was the new test's hard-coded footprint: the dependent was correctly on the cap, 0.8 wide on the snapped grid. Fixed in the test only. |
| `41084823234` | **50/50** | |
| `41090517111` | **109/109** | |

13 classes and 159 tests passed on the final test APK. This is a union of
bounded runs, not a full-suite result.

**Final CI FAST `41098028076`: success.**
- Artifact `ci-fast-evidence` `12996659575`: the downloaded ZIP's SHA-256
  `d568b891…507a` equals GitHub's digest.
- `app-debug.apk`: **17714955 bytes**, SHA-256
  **`8ec4356e30bb42195300214f42d17f91ebb4810bd576a2d0277546565c31edf9`**.
- The first attempt (`41097660891`) failed only the whitespace gate, on
  inherited raw evidence; see `TESTS.md` §4.

## Performance

Host only, Linux x86_64, `-O1`, ABBA against `7c2fc91`, median of 401 exports
per run. This is not an ARM64 or device claim.

| Scene | Before | After |
| --- | --- | --- |
| 12 independent CAD bodies | 92–99 µs | 92–96 µs (two first-run outliers of the unchanged path), bytes identical |
| 1 producer + an 11-deep face-supported chain | ~93–100 µs, wrong placement | 412–455 µs, correct placement |

The chain cost comes from resolving each dependent's producer chain once per
export.

## Open, and not decided here

| ID | Item |
| --- | --- |
| OD-01 | A2 second root sketch / `CADB` v9 |
| OD-05 | Recover / Save flow |
| OD-16 | A legacy non-identity stored dependent transform is kept inert and round-trips byte-identically; whether to normalise it is the OWNER's call |
| OD-27 | OWNER physical review |
| OD-29 | Lock on a dependent: still settable, and reported first |

Physical device, stylus, hover and TalkBack: `OWNER-PHYSICAL-UNVERIFIED`; see
`OWNER_LATER_TEST_PACK.md`. No FullSharded run (OD-03).
