# CAD-FACE-SUPPORTED-PLACEMENT-R1 — ACCEPTANCE MATRIX

"RED" means the check failed on the unchanged product (`evidence/red/`), or the
closeout reproducer showed the defect. "GREEN" means it passes on this
branch's product.

- Host: `HOST_SELFTESTS_OK (4434 checks, 0 failed)`.
- Device: see `TESTS.md`.

## Export (T-01)

| ID | Requirement | Evidence | RED | GREEN |
| --- | --- | --- | --- | --- |
| E01 | Dependent on a far CAP exports on the cap; node translation = the resolved origin; every vertex and normal = the renderer's | `GLTF_EXPORT FSP_E01_*` (5); device `CadFaceSupportedPlacementTest` (AABB [-0.5,-0.5,2]..[0.5,0.5,2.5]) | 3 of 5 fail | pass |
| E02 | Dependent on a SIDE exports rotated onto the side; cap and side files differ | `FSP_E02_*` (3) | 3 fail (cap and side were byte-identical) | pass |
| E03 | Chain A→B→C: every level on its producer | `FSP_E03_*` (3) | 3 fail | pass |
| E04 | Moved + rotated producer carries its dependents into the file, and again after a second move | `FSP_E04_*` (2); device: typed producer move → dependent [2.5,3.5] in x | 2 fail | pass |
| E05 | The committed `CADB` v2 fixtures (`cad_face_sketch_cap_v2`, `cad_face_sketch_side_v2`, `cad_face_chain_v2`, `mixed_cad_face_v2`) export where the renderer draws, parsed by an independent Python reader | `evidence/e05-corpus` | closeout: 11 MISMATCH / 12 MATCH | 0 MISMATCH / 23 MATCH (+1 Imported Mesh, translation equal) |
| E06 | Mixed scene keeps body order and names; every body where the renderer draws it | `FSP_E06_*` (2) | 1 fails | pass |
| E07 | Stale support → zero bytes, `UnresolvedPlacement` by name, capture refuses, fingerprint and document unchanged | `FSP_E07_*` (4) | 2 fail | pass |
| E08 | Deterministic; independent bodies byte-identical to the base exporter | `FSP_E08_*` (2), pinned digest `0x7b23f2a918aa3e3f` taken on `7c2fc91`; E05: C1/C2/`cad_rectangle_v1` GLB sha256 unchanged | pass (no regression to catch before) | pass |

## Transform (T-02)

| ID | Requirement | Evidence | RED | GREEN |
| --- | --- | --- | --- | --- |
| T01 | Typed write to a dependent refused by name; no Undo step, no fingerprint move, no `.forge` byte; drawn placement unchanged; a locked dependent still reports Locked | `CAD_A3 FSP_T01_*` (4); device: `applyBoxTransform` → 11, witness equal | closeout CR-02 C1: +1 Undo, fingerprint moved, stored (5,0,0)/rotZ 30 saved and decoded (`evidence/red/cr02_typed_and_gizmo_RED.txt`) | pass |
| T02 | Gizmo neither drawn, hit nor dragged over a dependent; nothing recorded | `GIZMO FSP_T02_*` (5); device: `setGizmoActive(true)` leaves it inactive and invisible, real drags through the window change nothing | 4 fail; CR-02 C2: gizmo drawn at the origin 3 m from the body, drag recorded | pass |
| T03 | Producer still transformable (typed and gizmo) as ONE step; the dependent follows | `CAD_A3 FSP_T03_*` (2), `GIZMO FSP_T03_*` (2); device | — | pass |
| T04 | An independent CAD body still takes typed and gizmo writes | `CAD_A3 FSP_T04`, `GIZMO FSP_T04`; device (producer offers Transform, gizmo active) | — | pass |
| T05 | UI withdraws Transform, the gizmo and the placement editor while the native refusal still holds on a direct call | device `CadFaceSupportedPlacementTest` (rail without `tool_rail_place`, Transform→Shape on selection, direct `applyBoxTransform`/`setGizmoActive`) | before: rail offered Transform (`WorkspaceTrailingHostView` had no such input) | device run |
| T06 | Legacy non-identity stored dependent transform: encode/decode/encode byte-identical, same stored values and same drawn placement after reload, export by the face (OD-16 untouched) | `CAD_A3 FSP_T06_*` (3) | — | pass |

## UI / device

| ID | Requirement | Evidence |
| --- | --- | --- |
| UI-1 | The dependent is created through New Sketch → spatial chooser by real taps | device `aFaceSupportedBodyIsNotTransformableAndExportsOnItsFace` |
| UI-2 | Real `MotionEvent` drags through the decor root, over the drawn dependent and over its inert stored pivot, change no Undo/Redo depth, fingerprint or `.forge` byte | same |
| UI-3 | GLB written through the app's export write path (`onCreateGlbDocumentChosen` → `ProjectTransfer.writeTo`), read back from disk byte-identical, parsed, world AABB on the face | same |
| UI-4 | Save → reopen: still face-supported, still refused, still exported on the face | same |
| UI-5 | Portrait and landscape × font scale 1.0 and 1.5: the dependent never offers Transform, the producer always does | device `theWithdrawalHoldsAcrossOrientationAndFontScale` |
| UI-6 | The stale `uir4a04` asserts the exact set of 12 resting tiles | device `EditorWorkspaceMobileTest` (RED: run `40983965648`, `expected:<8> but was:<12>`) |

Physical device, stylus and TalkBack: `OWNER-PHYSICAL-UNVERIFIED`
(`OWNER_LATER_TEST_PACK.md`).
