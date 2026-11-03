# CAD-V6-S2-OWNER-FEEDBACK-MULTIFACE-E2E-R1 — `PASS-CAD-V6-S2-OWNER-FEEDBACK-MULTIFACE-E2E`

TECH PASS / OWNER PHYSICAL REVIEW REQUIRED / NOT MERGED. Task branch
`feature/cad-v6-s2-owner-feedback-multiface-r1`, created at `d66dd89` (the
previous task's head). `main` stays `6c9f156`; `feature/cad-v6-sketch-face-r1`
stays `bbae765`. No FullSharded; S3 not started. Read `OWNER_FEEDBACK.md` and
`BEFORE.md` first.

## Final candidate

| fact | value |
| --- | --- |
| final product SHA | `e9960fb` (later commits add only artifacts and docs: `app/`, `scripts/`, `.github/` identical) |
| final focused DEVICE | `CI DEVICE` `37365134653` attempt 2 on `e9960fb` — **PASS 69/69** (9 classes) |
| final CI FAST | `CI FAST` `37365137933` on `e9960fb` — success (build, JVM, guards, corpus 57/57, `git diff --check`) |
| APK | artifact `ci-fast-evidence` id `11368352590`, path `app/build/outputs/apk/debug/app-debug.apk`; artifact zip 9,376,915 bytes, zip SHA-256 `c91a62b28decf84b7480ef82459276a37a532d59d18f2e66e174c6382e001cc5`; expires 2026-10-19T19:58:22Z. The APK's own byte count and SHA-256 are inside that zip: this session's GitHub client cannot follow the artifact download redirect, and no handoff APK was rebuilt. |

## Root causes (proven on the baseline, test-only commit `9c9e3c5`)

1. **A 16-face cap inherited from the loop-region cap.**
   `kMaxPlanarFaceSelection = kMaxProfileRegions` (16), enforced in
   `togglePlanarFace` (`TooManyRegions`), in `resolvePlanarFaceSelection`, and
   in the `CADB` v6 decoder. The v6 field was always a u32, so 16 was a
   validation bound only. Host `MF_B02`: 16 select, the 17th is refused,
   the set stays 16, the candidate stays `Ok`.
2. **The first project's Extrude refused every fill selection.**
   `commitFirstCadProject` tested `selectedProfileId()` — the LOOP-REGION
   anchor that `setPlanarSelection` clears — so in New Project → CAD every
   PlanarFaces selection was refused: `AmbiguousProfile` when the sketch
   closed several loops (the owner's rectangle + circles, host `MF_B04B`:
   three faces chosen, candidate `Ok`, refusal `AmbiguousProfile`), and
   `ProfileNotFound` with one loop (`MF_B04`). The same selection commits in a
   live project (`MF_B05`). This is the screenshot's message: not stale, not a
   UI desync, but the CURRENT commit's own refusal from the one commit path
   that read the wrong field.

## The new bound

`kMaxArrangementFragments = kMaxArrangementSourceEdges + 2 × kMaxArrangementContacts`
= 1024 + 8192 = **9216**; `kMaxArrangementFaces = kMaxArrangementFragments`;
`kMaxPlanarFaceSelection = kMaxArrangementFaces`.

- Fragments: a contact cuts at most its two source edges once each (≤ 2K
  cuts); an open edge with c cuts makes ≤ c + 1 fragments, a closed one
  max(c, 1) ≤ c + 1; so fragments ≤ S + 2K.
- Faces: on the fragment graph (E edges, V nodes, C components) Euler gives
  E − V + C bounded faces; every component has a node, so C ≤ V and bounded
  faces ≤ E ≤ S + 2K. Pruned fragments only remove edges.
- The densest arrangement the caps admit (64 × 64 lines, exactly 4096
  contacts) has 3969 faces, inside the bound (`MF_01`).
- A selection names distinct faces of one derived arrangement, so it cannot
  reach the bound; it stays a bound because the decoder reads a count before
  it has an arrangement. `kMaxProfileRegions` stays 16 for `LoopRegions`
  (`MF_13`).

## Format / codec

No `CADB` layout or version change. The count is the existing v6 u32; the
decoder holds it to the new bound (`ImpossibleCount` above it) and, before
allocating, to the bytes present (a face is ≥ 19 bytes; `Truncated`).
v1–v5 bytes unchanged; fixtures 57/57 byte-identical (CI FAST parity) and
57/57 verdicts identical to the baseline (`host-forge-corpus-verdicts.sh`,
diffed). A 36-face selection round-trips exactly (`MF_11`); patched counts:
bound + 1 → `ImpossibleCount`, bound with no bytes → `Truncated`, 0 →
`ImpossibleCount`, 36 → `Ok` (`MF_12`).

## Files changed (product)

- `forgeshape_sketch_arrangement.h` — `kMaxArrangementFragments`,
  `kMaxArrangementFaces` and their derivation.
- `forgeshape_cad_body.h` — `kMaxPlanarFaceSelection = kMaxArrangementFaces`.
- `forgeshape_project_bootstrap.cpp` — `selectionChosen()` / `unchosenStatus()`.
- `forgeshape_sketch_session.{h,cpp}` — `unchosenStatus()` public; toggle and
  membership by binary search over the canonical selection.
- `forgeshape_sketch_arrangement.cpp` — `resolvePlanarFaceRef` by binary
  search; O(n) duplicate-index checks in partition and merge.
- `forgeshape_cad_body.cpp` — duplicate check by one sort.
- `forgeshape_project_document.cpp` — count-versus-bytes guard.
- `EditorWorkspaceView.java` — `reportCandidateVerdict(null)` reports the
  count for a valid non-empty selection, so no earlier refusal stands.
- Tests: `forgeshape_cad_multiface_selftest.{h,cpp}` (in `CAD_FEATURE`),
  `FILL_R2_13` updated (deliberate rule change: every cell of a 20-cell grid
  selects), `CadMultiFaceOwnerTest.java`.
- CI: `scripts/ci-device-smoke.sh` prints failed tests and fact files into the
  job log (this session cannot download artifacts).

## Host tests (`MULTIFACE`, inside `CAD_FEATURE`)

`MF_01` derived bound · `MF_02` 17th/18th by real Ready taps, no cap ·
`MF_03` 32 · `MF_04` all 48 of an 8 × 6 grid, one slab · `MF_05` reverse,
shuffled and 6 × 120 random toggles = parity, > 16 · `MF_06` remove/re-add at
positions 15/16/17/32 · `MF_07` contiguous 20 → one component, 18 perimeter
fragments, area 20 · `MF_08` 36 mixed (16-cell block, empty row, 20-cell
checkerboard) → 21 deterministic components · `MF_09` New Body 20, one Undo ·
`MF_10` New Body 36, 21 shells, volume 36 × depth · `MF_11` v6 round trip +
reload · `MF_12` codec bound · `MF_13` LoopRegions cap 16 unchanged ·
`MF_14` non-empty never "choose one"; first project commits three owner-sketch
faces · `MF_15`/`15B` refused commit names the candidate's own reason
(`PlanarFacesTouchAtPoint`) in the first project and a live one; the fix
commits · `MF_15C` the device's pinch pattern · `MF_PERF_01/02`. `MF-16`:
`FILL_R2_*` and `PICK_R2_*` still pass in the same suite.

## Performance (host, single-shot, no startup loop)

| measure | value |
| --- | --- |
| tap + candidate evaluation, 32 faces (8 × 6 grid) | mean 182 µs, max 243 µs |
| 128 toggles (12 × 12 grid) | 49 µs total |
| evaluation at 128 faces | 776 µs |
| mixed 36-face evaluation (21 components) | 396 µs |
| near-bound: 63 × 63 grid, 3969 faces, every face selected | toggles 900 µs total, one evaluation 32 ms (Finish of the 125-entity sketch through gestures 277 ms) |

## Host aggregate and local gates (on `e9960fb`)

`HOST_SELFTESTS_OK (4024 checks, 0 failed)`; JVM 120/120;
`assembleDebug`, `assembleDebugAndroidTest`, `assembleRelease` green; release
guard PASS (release 0 symbols / 0 strings on both ABIs; debug 27 / 392);
`git diff --check` clean over `d66dd89..HEAD`.

## DEVICE attempts

| run | candidate | classes | result | cause / action |
| --- | --- | --- | --- | --- |
| `37352304851` | `548b3ce` | 9 | FAIL 65/69 | names not in the job log; added the failed-test echo |
| `37356742992` | `32a54a6` | 9 | FAIL 65/69 | all 4 in `CadMultiFaceOwnerTest`, harness: 2 × 3 m grid drawn 2 × 2.8 m (volumes), taps on the HUD dimension value (`HUD_TOUCH:value`); the 8 regression classes 63/63 |
| `37360833700` | `e3ec332` | 1 | FAIL 0/6 | 0.25 m cells snapped off the 0.2 m touch grid → no cells at Finish |
| `37362368699` | `721f619` | 1 | FAIL 0/6 | `sketchGridStep()` 0.25 vs touch snap 0.2: rectangle read back 2.8 m |
| `37363828318` | `e9960fb` | 1 | **PASS 6/6** | each entity dragged, then typed exact (`sketchApply*`) |
| `37365134653` #1 | `e9960fb` | 9 | cancelled, 0 tests | pre-test infra: "job was not acquired by Runner of type hosted" → same-run rerun |
| `37365134653` #2 | `e9960fb` | 9 | **PASS 69/69** | final union |

Final union: `CadMultiFaceOwnerTest`, `CadFillPickR2Test`,
`CadPlanarFaceOwnerCorrectionTest`, `CadPlanarFaceRuntimeTest`,
`CadHud3dOwnerTest`, `CadCanvasExtrudeTest`, `SketchExtrudeTest`,
`HomeFlowTest` (the first-project path changed), `CadVerticalSliceTest`.
No JNI signature changed.

Device facts (`device-facts-37363828318.txt`, repeated in the final union):
taps 1..18 each `FORGESHAPE_SKETCH_TAP:resolved`, status "Regions in the
extrusion: k." (no `selection_cap`); 24 cells → preview 3.84 m³, one
component; the first Extrude commits 20 cells → "Project created", body
3.2 m³, one component; the pinched 17-cell set's refused commit → last status
61, "Those areas meet only at a point — …"; a 19th cell tapped from below the
plane, jittered, resolves without orbit. R2-04 on the final candidate: shaft
0.64 px, `resolved`; head `arrow_head`; drag 1.5 → 1.88 m.

## FAST attempts

| run | candidate | result |
| --- | --- | --- |
| `37352307992` | `548b3ce` | success |
| `37365137933` | `e9960fb` | **success** (final) |

## OWNER re-review checklist

1. New Project → CAD; draw a rectangle crossed by circles and a spline (or a
   grid). Tap more than 16 cells: every tap adds its cell and the status line
   counts it.
2. Choose every cell; Extrude is offered; the preview is the whole shape.
3. Remove cells around the 16th–18th and add them back in another order: the
   same selection returns.
4. With many cells chosen, tap **Extrude** in a brand-new project: the body is
   created (no "Several profiles are closed").
5. With a few cells chosen: Extrude also works.
6. Choose cells that touch only at a corner inside one connected group: the
   status says they meet at a point; drop one cell and Extrude works.
7. "Several profiles are closed — choose one" appears only with nothing chosen.
8. The previous task's HUD3D checks still hold (dock tilt, hide, palette,
   nearby-cell taps).
