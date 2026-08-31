# GLB-IMPORT-R0 — device end-to-end and focused case table

All on `ForgeShape_Stage006` / `emulator-5580`. Every device call went through
`scripts\run-instrumented-tests.ps1 -Serial emulator-5580` or an explicit
`adb -s emulator-5580`. The reserved `emulator-5554` was never contacted.

The system's document picker is another app's surface and cannot be driven
reliably from instrumentation, so the split is the one E2E-R1B and E2E-R1C used
and stated: what happens to the BYTES is exercised for real, end to end, against
a real `ContentResolver` and a real `Uri`; what is ASKED of the system is
asserted as the exact `Intent`. Between them that covers everything ForgeShape
is responsible for.

## Focused — `GLBIR0-01..20`

| ID | Where | Result |
| --- | --- | --- |
| `GLBIR0-01` valid corrected Construction sentinel parses | `glbir0_01_…` (6 meshes) + native `GLBIR0_01_*` | PASS |
| `GLBIR0-02` valid corrected Sculpt sentinel parses | `glbir0_02_…` (2 meshes, 16 triangles) | PASS |
| `GLBIR0-03` header/chunk/range validation independent and fail-closed | `glbir0_03_…`; native `GLBIR0_03_*` — no data, wrong magic, wrong version, truncation, a file that lies about its own length, a header-length file, malformed JSON, an accessor past its view | PASS |
| `GLBIR0-04` POSITION/NORMAL/index decoding exact | native `GLBIR0_04_*` — a 2×1×0.5 m box decodes to those bounds and 12 triangles, one unit normal per position, every index in range | PASS |
| `GLBIR0-05` node translation applied exactly; no axis conversion | native `GLBIR0_05_*` — the translation is read verbatim, the node matrix is a pure translation with an identity basis, and world = local + translation. A Blender-style Z-up fix would put the Y and Z terms in each other's place | PASS |
| `GLBIR0-06` unsupported matrix / non-identity R/S fails closed | native `GLBIR0_06_*` — `NodeMatrix`, `NodeRotation`, `NodeScale`, `NodeHierarchy`; and an EXPLICIT identity `[0,0,0,1]`/`[1,1,1]` is accepted, because that is what the specification says the default is | PASS |
| `GLBIR0-07` external buffer / extension / animation / skinning / sparse fails closed | native `GLBIR0_07_*` — 12 features, each edited into a real export one at a time and refused by its own name | PASS |
| `GLBIR0-08` preview has no ObjectId/Construction/Sculpt/history ownership | `glbir0_08_…`; native `GLBIR0_08_*` — no body added, active body unchanged, no revision, no history step, every renderer key in the reserved range and unreachable by the allocator | PASS |
| `GLBIR0-09` manual `.forge` Save bytes unchanged | `glbir0_09_…` — identical bytes with and without a preview, written through the production Save Copy path and still accepted by the decoder | PASS |
| `GLBIR0-10` autosave fingerprint/checkpoint unchanged | `glbir0_10_…` — the fingerprint does not move for load, show or clear, and no checkpoint is written | PASS |
| `GLBIR0-11` Source↔Imported switch mutates nothing | `glbir0_11_…` — the whole observable native snapshot, the fingerprint and the re-encoded project bytes are all identical across a round trip | PASS |
| `GLBIR0-12` Clear releases and restores | `glbir0_12_…`; native `GLBIR0_12_*` — nothing loaded, nothing shown, zero counts, the workspace back; and `visible` cannot be true with nothing loaded | PASS |
| `GLBIR0-13` Construction source→GLB→import equivalence | `glbir0_13and15and16_…`; native `GLBIR0_13_*` | PASS — `ROUNDTRIP_EQUIVALENT` |
| `GLBIR0-14` Sculpt source→GLB→import equivalence | `glbir0_14_…`; native `GLBIR0_14_*` — and the Construction reading of the same scene is a different geometry, which is what proves the Sculpt comparison was about the sculpt mesh | PASS — `ROUNDTRIP_EQUIVALENT` |
| `GLBIR0-15` max world-position delta within float32 tolerance, reported | the reports — `max_position_delta_m=0` | PASS |
| `GLBIR0-16` normals and world bounds agree within tolerance | the reports — `max_normal_delta_deg=1.20741827e-06`, every per-body `source_aabb` equal to its `import_aabb` | PASS |
| `GLBIR0-17` picker cancel / read failure no-op | `glbir0_17_…` ×2 — the Intent is `ACTION_OPEN_DOCUMENT` + `CATEGORY_OPENABLE` with a type; a null answer and an unreadable `Uri` both load nothing and change nothing | PASS |
| `GLBIR0-18` controls ≥48 dp, semantic ids, R2 host unchanged | `glbir0_18_…`, plus `uir4b17_*` and `EditorWorkspaceRightHostPlacementTest` in the aggregate | PASS |
| `GLBIR0-19` preview mode exposes no edit control that cannot succeed | `glbir0_19_…` — the trailing host and Undo/Redo withdrawn, and Apply Shape, Apply Transform, Add body, Start Sculpting and both Construction rail entries all off screen (`isShown()`, so an ancestor's absence counts) | PASS |
| `GLBIR0-20` no OBJ/FBX/material/UV/animation/durable-import scope added | `glbir0_20_…`; `fsr1b18_…`; native `GLBIR0_20_*` | PASS |

## Device end-to-end — `E2E-GLBIR0-01..08`

| # | Case | Result |
| --- | --- | --- |
| 01 | The current Construction sentinel loaded through the real SAF result seam; the preview renders **six** meshes | PASS (`glbir0_01_…`, `glbir0_17_…` for the seam) |
| 02 | Source → Imported → Source: project values, history, `ObjectId`s, manual slot and recovery fingerprint all unchanged | PASS (`glbir0_11_…`, `-08`, `-09`, `-10`) |
| 03 | The real current Construction source exported and independently reimported; machine result **`ROUNDTRIP_EQUIVALENT`**, max delta **0.0 m** | PASS (`glbir0_13and15and16_…`) |
| 04 | The current Sculpt source exported and reimported; the preview reflects the **sculpt** geometry, not the Construction companion | PASS (`glbir0_14_…` — `source=sculpt` for the sculpted body, `source=construction` for its companion) |
| 05 | A malformed or unsupported GLB refusal leaves Source untouched with no preview residue | PASS (`glbir0_03_…`, `glbir0_06and07_…`) |
| 06 | Preview UI hit targets and R2 geometry regression | PASS (`glbir0_18_…`, plus the aggregate's `UIR4B-17` and `UILR2C-01..12`) |
| 07 | Clear frees the preview and the app continues editing Source | PASS (`glbir0_12_…`) |
| 08 | A preview never becomes anything that outlives the process | PASS (`e2eGlbir0_08_…`) — structural: the preview is written nowhere, so there is nothing for a restart to restore. The project bytes are identical with and without it, and no `.forge` slot is created |

## The visual pair

`source_scene.png` and `imported_preview.png` are the same project from the
**same camera pose**, pinned with `debugSetCameraPose(0.6, 0.35, 16.0)` before
the first capture and asserted unchanged before the second — the camera is
process-scoped native state that loading, showing and hiding a preview do not
touch, so the two share it by construction rather than by being re-aimed.

They illustrate; they are not the authority. Nothing is overlaid or blended, and
the machine comparison above is what the verdict rests on. What the pair shows
is that the silhouettes coincide, that the preview is drawn in its own flat
neutral rather than the project's per-body colours, and that the editing chrome
is correctly absent over it.
