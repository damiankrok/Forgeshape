# CAD Vertical Slice R1 — summary

What the milestone set out to do, what it delivered, and where the proof lives.
The result token and the gate state are in `TEST_EVIDENCE.md` §6 and in
`PROJECT_STATUS.md`: **`PARTIAL-CAD-VERTICAL-SLICE-R1-TEST-BUDGET`**. Every
cloud gate is green on the tested candidate `759ed91` (`CI FAST` plus 100
device tests). The milestone aggregate has since run in the cloud twice:

- **C1** failed on 7 tests, five of which fail on `main` without this slice
  (`FULLSHARDED_C1.md`).
- **C2** corrected all seven on the new candidate `5f1eccf`, where the only
  product-source change is a diagnostic counter mirror. The aggregate then
  stopped at shard 4 on a pre-existing test isolation defect,
  `MirrorSmokeTest`, which fails on `main` too (`FULLSHARDED_C2.md`).
- **C3** corrected that Mirror isolation with a test-only change (`899986d`),
  green alone and in the exact shard-4 sequence. The aggregate then stopped at
  shard 2 on a nondeterministic screenshot race in
  `SelectionOutlineVisualEvidenceTest`, which passed on replay
  (`FULLSHARDED_C3.md`).

`main` was not merged.

## 1. The owner's problem, in one paragraph

After Finish Sketch, the extrude controls were three wide text pills that
covered 6.78 % of the viewport, the value stood 99.5 dp away from the arrow it
described, and the drawing chrome and a precision panel covering 28.8 % stayed
up. A rectangle drawn around a circle silently extruded the circle. A sketch on
a body's face could only make a SECOND body beside it: there was no Add, no Cut
and no way to change a body after it was made. All of this was measured on the
unfixed product first (`before/`, `CURRENT_TRUTH.md`).

## 2. What changed

| Area | Delivered | Proof |
| --- | --- | --- |
| Compact HUD | One icon row on the arrow: extent control (with a three-icon palette), the value centred on the shaft, the operation badge, Flip. 48 dp hit areas, 24–32 dp glyphs. 2.70 % of the viewport. | `UX_CONTRACT.md`, `OWNER_FINDINGS.md` F1–F4 |
| Tool Labels | Settings → Interface, default OFF. Short captions under the HUD icons. Application state only. | `AppPreferencesTest`, `SettingsPreferencesTest`, `compact_extrude_hud` |
| Ready chrome | The drawing tools, the orientation navigator and the Line dimension are absent in Ready. The precision surface is closed until asked for. | `SketchChromePolicyTest`, `ui_context_withdrawal` |
| Regions | A loop minus its direct clean children. Nothing is auto-chosen when there is more than one region. A tap toggles a region; the hatch leaves a hole empty. | `PROFILE_REGIONS.md`, `owner_rectangle_circle_region` |
| Kernel | Manifold 3.5.4 (Apache-2.0), vendored unmodified behind `forgeshape_cad_kernel.h`, reproducible and verified byte-identical. | `KERNEL_GATE.md` (PASS) |
| Feature chain | New Body, then up to 15 Add or Cut features on the SAME `SceneObject`. Each is one Undo. Ordered, atomic regeneration. Per-triangle face tags. | `FEATURE_CHAIN.md`, `new_body_then_add_same_body`, `same_body_cut` |
| Preview | The renderer draws the candidate the commit will apply, latest-only. An invalid candidate is named and Extrude is withdrawn. | `PERF_NOTES.md`, `operation_refusal` |
| Feature list and edit | A CAD Body's features are listed in its Shape panel and reopen in place. An upstream edit carries a downstream feature. | `feature_edit_roundtrip` |
| Persistence | `CADB` v5, written only when a body needs it. 8 new fixtures; the 36 older ones are byte-identical. The PowerShell encoder agrees byte for byte. | `PERSISTENCE.md`, `DATA_PACKAGE_SPEC.md` §7f |

## 3. What did not change

- No older `.forge` fixture, no older digest, no older section version.
- No renderer pipeline, shader or style. The preview reuses the draw item and
  the selection-tint slot.
- No NDK, AGP, Gradle, JDK or CMake version.
- CAD → Sculpt stays refused. There is no Revolve, Intersect, Through All,
  Hole, fillet, chamfer, shell or constraint solver.

## 4. Where to look

| Question | File |
| --- | --- |
| What was true before | `CURRENT_TRUTH.md`, `before/` |
| What the owner will see | `OWNER_FINDINGS.md`, `after/` |
| Why this kernel | `KERNEL_GATE.md` |
| How regions and the chain work | `PROFILE_REGIONS.md`, `FEATURE_CHAIN.md` |
| The file format | `PERSISTENCE.md` |
| Cost | `PERF_NOTES.md` |
| What was tested, and the runs | `TEST_PLAN.md`, `TEST_EVIDENCE.md` |
| What is still wrong or missing | `POST_AUDIT.md` |
