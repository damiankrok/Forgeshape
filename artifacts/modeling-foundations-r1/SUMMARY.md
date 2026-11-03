# MODELING-FOUNDATIONS-R1 — SUMMARY

Parametric History, Freeform/SubD and Surface, end to end. Read `BEFORE.md`
(the pre-flight audit of the baseline) and `DESIGN.md` (the plan each phase was
built against) first; this file records what was built and what proves it.

**Result: `PASS-MODELING-FOUNDATIONS-R1`** — TECH PASS / OWNER PHYSICAL REVIEW
REQUIRED / NOT MERGED.

## Branch and baseline

| Ref | SHA |
| --- | --- |
| Task branch | `feature/modeling-v9-parametric-freeform-surface-r1` |
| Started at | `a3643b5` (drafting-toolkit closeout; tested product `df7db66`) |
| Product SHA (last product change) | `30833aa` |
| `origin/main` | `6c9f156` — not moved |
| `feature/cad-v6-sketch-face-r1` | `bbae765` — not moved |
| `feature/cad-v8-sketch-drafting-toolkit-r1` | `a3643b5` — not moved |

No merge, no integration, no FullSharded run, S3 not started.

## A. Parametric History

* `buildCadTimeline` (`forgeshape_cad_timeline.{h,cpp}`) derives a CAD body's
  rows — sketches and features in construction order, by durable id — on every
  read; with an edit staged, the rows are the candidate chain's own evaluation:
  the FIRST failing feature `Failed`, later ones `NotRegenerated`.
* `FeatureHistoryView` (History control in the history capsule; five rows,
  scrolling; state by SHAPE) and the regeneration issue card (Fix / Cancel).
* Host: PAR-01..15 (inside the `CAD_FEATURE` suite). JVM:
  `FeatureHistoryPresentationTest`. Device: `CadParametricHistoryOwnerTest` 3/3.

## B. Freeform/SubD

* `BodyRepresentation::Freeform`: a manifold all-quad control cage with strong
  ids and stored high-water marks (`forgeshape_freeform.{h,cpp}`);
  Catmull-Clark levels 0..4 with boundary and continuous-crease rules
  (`forgeshape_freeform_subdivision.{h,cpp}`); Vertex / Edge / Face selection
  with multi-select, the cage gizmo's Move / Rotate / Scale, Push/Pull and Extrude
  by a typed distance, Insert Loop at a ratio (an ambiguous ring refused), Crease,
  Delete Face, exact Symmetry X/Y/Z — each ONE Construction step
  (`forgeshape_freeform_session.{h,cpp}`). Never called T-Spline.
* Freeform context surface under Shape (`FreeformEditorView`), New Project →
  Freeform, Add → Freeform Box / Plane / Cylinder.
* `FRFM` v1 (DATA_PACKAGE_SPEC §7j, header bit4); three fixtures.
* Host: FF-01..22 (`FREEFORM`, 74 checks). JVM: `FreeformPresentationTest`.
  Device: `FreeformOwnerTest` 3/3.

## C. Surface

* `BodyRepresentation::Surface`: an ordered feature list over retained
  sketches; PlanarPatch, ExtrudedSurface, RevolvedSurface, LoftSurface,
  TrimSurface (exact, through the planar arrangement), Stitch, Thicken; ordered
  all-or-nothing regeneration naming the first failing feature
  (`forgeshape_surface.{h,cpp}`).
* Authoring (`forgeshape_surface_authoring.{h,cpp}`): a sketch in the ONE sketch
  session becomes a feature through `SurfaceSketchPurpose`; the first Surface
  project is created through `loadProjectDocument` (empty history); Stitch,
  Thicken and a typed value edit (distance, angle, thickness, sketch offset) are
  each one step or a named refusal; `buildSurfaceTimeline` feeds the same
  History surface. The sketch session gained a plane offset so a Loft's second
  section is drawn where it stands.
* UI: New Project → Surface, Add → Surface, `SurfaceFinishView` (Finish
  Sketch's choices, hung from the toolbar on the leading side, clear of the
  sketch chrome), the Surface surface under Shape (`SurfaceEditorView`), History
  rows. Start Sculpting withdrawn and refused by name.
* `SURF` v1 (DATA_PACKAGE_SPEC §7k, header bit5); three fixtures.
* Host: SURF-01..24 plus authoring and code-stability checks (`SURFACE`, 40
  checks). JVM: `SurfacePresentationTest` (7). Device: `SurfaceOwnerTest` 3/3.

Two defects the device found and this stage fixed: the Finish choices first
opened in the precision sheet, which a sketch leaves one row tall (80 px body),
and then overlapped the orientation navigator's Modify toggle; and the Surface
body tools were one nested group, which the precision body (it ends on the last
WHOLE row) could not show.

## Format

`FRFM` and `SURF` are dedicated REQUIRED sections written only when such a body
exists; every earlier project keeps its bytes and fingerprint. Counts are
bounded and checked against the remaining bytes before any allocation. Corpus
**72/72** byte-identical between the C++ writer and
`scripts/build-forge-corpus.ps1`; the 66 older fixtures unchanged.

| Fixture | Bytes | SHA-256 |
| --- | ---: | --- |
| `freeform_box_v1` | 800 | `2f38b285c5718a5f53cbac8e9dcac9749ae474a3ae2423dd4e7dca97157faf0e` |
| `freeform_crease_symmetry_v1` | 1504 | `b218fda7d3eaea4170827a8fece576e032969681c710b1cf9d17521f199e2029` |
| `freeform_bad_topology_v1` | 800 | `e896be40f36453d588ea22d4a0dacc6289222484d5d95db377b521d27f86c562` |
| `surface_patch_extrude_v1` | 546 | `d4cb49a6500718e1e1482a788aa897cf28d9922421ac9e1b73bffb89c3d55e14` |
| `surface_loft_trim_stitch_v1` | 936 | `006374495e44da8332cae0fc07533ebface1f59fde8e12eb5ff9981c33736c8e` |
| `surface_bad_ref_v1` | 690 | `4a0367a4a05f432633a81cd54313db07ab4a88da87d447d85dec8b93df3f640d` |

## Performance (measured, one bounded run; no startup benchmark loop)

| Measure | Host (x86_64) | CI emulator (debug) |
| --- | ---: | ---: |
| Freeform 96-face closed cage, level 1 / 2 / 3 / 4 | 279 / 374 / 1066 / 4088 µs | 1922 / 3373 / 9666 / 37688 µs |
| Freeform 96-face drag sample | 536 µs | 4495 µs |
| Freeform 500-face open cage, level 4 | 32850 µs | 203836 µs |
| Surface holed patch / loft / trim+stitch+loft / trim+thicken | 48 / 5 / 104 / 237 µs | 133 / 50 / 377 / 852 µs |
| Parametric 16-feature chain regeneration | 7399 µs | 16453 µs |
| Parametric upstream edit (staged) | 14553 µs | — (log line truncated) |

## Gates

| Gate | Result |
| --- | --- |
| Host aggregate | `HOST_SELFTESTS_OK (4287 checks, 0 failed)`, 25 suites |
| JVM full | 170/170 |
| Debug, release, androidTest builds | built locally; FAST below |
| Release self-test guard | PASS (0 self-test symbols / strings in release, both ABIs) |
| Startup tokens (device) | 25/25 in order, `NATIVE_VIEWPORT_OK`, 0 failure lines |
| CI DEVICE union `37472535341` on `30833aa` | **PASS 54/54**, 9 classes |
| CI FAST `37478194506` on `6b54843` (product `30833aa`) | success — builds (debug, androidTest, release), JVM unit tests, release self-test guard PASS, corpus parity **72/72**, device-free checks |

Union classes: `CadParametricHistoryOwnerTest` 3, `FreeformOwnerTest` 3,
`SurfaceOwnerTest` 3, `CadSketchDraftingOwnerTest` 14, `CadRevolveOwnerTest` 9,
`CadMultiFaceOwnerTest` 6, `CadFillPickR2Test` 6, `CadHud3dOwnerTest` 5,
`JniBoundaryHardeningTest` 5. Emulator CI closes no physical-device gate.

## OWNER APK

| Field | Value |
| --- | --- |
| Product SHA | `30833aa037d6eab82474663cfa0690a3cde39347` |
| Branch HEAD the FAST run built | `6b54843cd9b951f60a1dc9f3154b3efefe6de4d6` (docs-only after `30833aa`) |
| CI FAST run | `37478194506` |
| Artifact | `ci-fast-evidence`, id `11420511634` |
| Artifact ZIP SHA-256 | `d878416c0c171aaec2ae175963c569da95974db5a1f0062b75d382f069dc4997` (11,341,185 bytes) |
| Artifact expires | 2026-10-20T14:26:17Z |
| APK path in the artifact | `app/build/outputs/apk/debug/app-debug.apk` |
| APK bytes | 17,067,370 |
| APK SHA-256 | `062df5c8a769932bf85cef40877a97ef00c8a68103ac5ffae11a6319560c0862` |

## Deferred (not this stage)

True T-Spline local refinement (T-junctions); general NURBS surfaces and a trim
of a curved surface; general B-Rep fillet and draft; feature reorder,
suppression and rollback; advanced Surface repair and surface interchange with
other programs; a Revolve Add/Cut and Surface → Sculpt. UNVERIFIED on a device
(host-tested only): Freeform Rotate/Scale handles, multi-select, Delete Face,
Symmetry Y/Z and the Add entries; Surface Trim Keep inside, Thicken of a flat
patch, a partial Revolve and Add → Surface.
