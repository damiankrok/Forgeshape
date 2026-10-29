# FUNCTION-COUNCIL-R2 — Screen evidence index (Seat F)

Audited tree `70a6c14f703bfda2ab689b855126c1d12a541e41`. This index lists every PNG set under
`artifacts/` as it stands on that tree, with what is recorded about where it came from. Seat F
took no new screenshots. The only captures made during this audit are the coordinator's CI
DEVICE run `40797031704` (first table). Everything else is historical (grade E3 at best), and
none of it is physical-device evidence.

How provenance was read:
`git log --diff-filter=A -- <dir>/*.png` gives the commit that added each set to this repository.
The stage dossier beside the set (`README.md`, `INDEX.md`, `EVIDENCE_INDEX.md`, `SUMMARY.md`,
`facts.txt`) gives the build or commit the dossier names. Most sets were imported in one commit
at repository bootstrap, `fd88db8` ("ci(cloud): start CI-CLOUD-R1-C1", 2026-09-27). Commit
hashes those dossiers name from before that point, such as `6d05814`, `538623c`, `dfcab1a`,
`565d400`, `717cf70` and `868995d`, are **absent from this repository's object store**. Checked
with `git cat-file -e <sha>^{commit}`. Their build provenance therefore cannot be re-verified
here. It is recorded as "dossier-stated, not resolvable".

Density on every recorded emulator run is 420 dpi, which is 2.625 px/dp, so 48 dp = 126 px.
The exception is `uiaudit1`'s expanded captures, taken at density 1.5 according to its README.

## 1. Fresh captures in this audit (coordinator CI DEVICE `40797031704`)

- Run on `ForgeShape_CI_API36` / `emulator-5580`, confirmed by `emu avd name`.
- Image: `android-36;google_apis;x86_64`, `medium_phone` profile, 1080x2400 at 420 dpi, GPU `swiftshader_indirect`.
- 25 of 25 `*_SELFTEST_OK` tokens in order, plus `NATIVE_VIEWPORT_OK`.
- Five classes ran: 32 tests, 0 failed.
- Tree: `70a6c14`. Files: `artifacts/function-council-r2/evidence/device-run-40797031704/`.

**This is emulator evidence. It is not a physical-device PASS.**

| Set (under `screens/`) | Count | Test (class.method) | Configuration | What it shows | Facts file |
| --- | --- | --- | --- | --- | --- |
| `cad-a3-c2-sketch-ux-r1/01..12` | 12 | `SketchUxVisualEvidenceTest.e2eCadUxR1Vis_theWholeJourneyInTwelveCaptures` | portrait, font 1.0, Warm Graphite | Home → New Project → immediate XY sketch → navigator +Z, other plane, +90° → Line dimension → length editor → Arc → Spline → Edit Sketch → finished | yes: rects, including `navigator_flip` 95 px wide and `dimension_label` 114 px wide |
| `sel-out-r1/01..12` | 12 | `SelectionOutlineVisualEvidenceTest.e2eSelOutR1Vis_theWholeJourneyInTwelveCaptures` | portrait; dark, Warm Light, Cool Light; left-handed | Selection outline on Construction, Imported, Sculpt and CAD bodies; outline off; occluded; two bodies; light grounds; left-handed with the Display popover; after-delete fallback | yes: band metrics, popover rects, `selection_outline_on` 97 px and `_off` 103 px |
| `ui-pref-r1/01..20` | 20 | `UiPrefVisualEvidenceTest.e2eUiPrefR1Vis_theWholeJourneyInTwentyCaptures` | portrait; capture 16 is landscape 2400x1080; 5 palettes; both handedness settings; gizmo sizes and weights | Settings entry from Home and from the drawer; Settings page; editor and Home in each palette; right and left handed; left-handed exact panel open; gizmo default, smallest-thin, largest-bold, rotate | yes: Settings row rects, `settings_back` at y=3409 px (below the fold), host and capsule rects |
| `ui-3d-state-audit-r1/ui3d03..ui3d13` | 58 | `Ui3dStateAuditTest`, 9 methods | portrait, font 1.0 | Show/hide lifecycle; Dimensions under transform and camera; selection and object state; Line dimension; staged extrude One Side, Symmetric and Two Sides (with drag, orbit, zoom); retained Edit Sketch; Sculpt leakage (Sculpt, navigator, Mask, back, resume); global dismissal | `VISIBILITY.tsv` (119 PASS), `SPATIAL_ATTACHMENTS.tsv` (68 PASS, 3 CLAMPED, 1 NOT_MEASURED), `NOTES.tsv` |
| `startup-screenshot.png`, `post-test-screenshot.png` | 2 | CI DEVICE harness | portrait | Cold launch (Home); state after the tests | `summary.json` |

`EditorWorkspaceLegibilityTest` (20 tests) ran in the same job with no captures. Its assertions
are cited in `UI_SCREEN_STATE_MATRIX.tsv`.

## 2. Historical sets (pre-audit, E3; trees differ from `70a6c14` unless stated)

| Directory | Count | Stage (from dossier) | Build / commit as recorded | Configuration | What it shows |
| --- | --- | --- | --- | --- | --- |
| `artifacts/*.png` (root) | 190 | Stages 003–016R2, platform-fix P2, gate P0/P1 (`stage0NN_*`, `platformfixp2_*`, `gatep0_*`, `gatep1_16k_*`) | added at `fd88db8`; per-file build not recorded | `ForgeShape_Stage006` era; portrait, landscape and wide | Early viewport, camera, selection, box state, invalid-input refusals, primitive editors, sculpt, 16 KB-page landscape |
| `uiaudit1/` | 22 | UI audit 1 (read-only) | dossier: 13/13 self-tests; commit not stated | `emulator-5580`; compact at 2.625 px/dp; expanded at density 1.5 | Resting Construction, transform modes, exact transform scroll, Add Primitive, Objects, Sculpt, Display, palettes, expanded (E01–E04) |
| `uiaudit2a/` | 49 | UI audit 2a | dossier names `bd3f7d5`, not resolvable | `emulator-5580` | Start chooser, transform, exact transform, Add Primitive, Objects, Display popover |
| `uiaudit2b/` | 15 | UI audit 2b (motion) | `bd3f7d5`, not resolvable | motion and reduced-motion probes | Chrome motion frames |
| `uilayoutr1/`, `uilayoutr2/` | 14, 16 | UI-LAYOUT-R1/R2 | R2 dossier: `54c3dfa…`, `8d9ebbf…`, not resolvable; "59 tests" | compact, short landscape, expanded | Right-host layout |
| `uilayoutr2-correction/{raw,overlays}` | 36 + 36 | UI-LAYOUT-R2 correction | `6d05814…`, not resolvable | `emulator-5580`, `ForgeShape_Stage006` | Raw and bounds-overlay captures per window class |
| `uilayoutr2-spec-closeout/{raw,overlays}` | 36 + 36 | UI-SPEC-R0 closeout | accepted spec SHA-256 `9f1e8667…` (owner acceptance 2026-08-29 quoted) | `emulator-5580` | Captures compared with the accepted spec |
| `uilayoutr2-owner-review/` | 1 | Owner review package | packaging only; dossier records no verdict | — | Review sheet |
| `uiarchr1/{before,after}` | 11 + 11 | UI architecture refactor R1 | `717cf70…`, not resolvable | `emulator-5580` | Before/after of the right-cluster refactor (geometry equivalence) |
| `uir4a/`, `uir4b/`, `uir4c/`, `uir5a/` | 15, 12, 10, 16 | UI-R4A/B/C, UI-R5A | not stated (uir5a mentions `3251093`, not verified) | compact and landscape | Objects capsule, Add Primitive, chrome composition, trailing cluster |
| `stage019/` | 6 | Stage 019 Construction history | not stated | compact, short landscape, expanded | Undo/Redo affordances; no Construction Undo in Sculpt |
| `stage020/`, `stage020r2/`, `stage020r3/` | 11, 10, 6 | Stage 020 gizmo (R2, R3) | not stated (R3 README: `emulator-5580`) | compact, short landscape, expanded, three palettes | Move/rotate gizmo, axis active, exact transform after gizmo, multi-object, no gizmo in Sculpt; R3 sculpt stroke |
| `ui-pref-r1/frames` (+1 root) | 20 + 1 | UI-PREF-R1 | `565d400…`, not resolvable; "OK (108 tests)" | as in §1 | Same journey as the fresh `ui-pref-r1` set; the predecessor run |
| `sel-out-r1/frames` (+1) | 12 + 1 | SEL-OUT-R1 | baseline `538623c…`, not resolvable; "OK (19 tests)" | as in §1 | Predecessor of the fresh `sel-out-r1` set |
| `cad-a3-app-h1/frames` (+1) | 10 + 1 | CAD-A3 / APP-H1 | dossier names `1169628`, not resolvable; "OK, 22 tests" | portrait | Home, New Project, world-plane chooser, highlight, orthographic sketch, face highlight, face sketch, producer/dependent, reopened dependency |
| `cad-a3-c2-sketch-ux-r1/frames` (+1) | 12 + 1 | SKETCH-UX-R1 | not stated; "OK (14 tests)" | portrait | Predecessor of the fresh `cad-a3-c2-sketch-ux-r1` set |
| `ui-3d-state-audit-r1/screenshots` | 59 | UI-3D-STATE-AUDIT-R1 | `dfcab1a…`, not resolvable; `OK (9 tests)` | `ForgeShape_Stage006`, 1080x2400 | Predecessor of the fresh `ui3d*` set |
| `ui-3d-state-c1/screenshots` | 12 | UI-3D-STATE-C1 | not stated; 117/117 state rows | `emulator-5580` | Finding closure captures (dimension overlay padding) |
| `ui-3d-state-c2/screenshots` | 5 | UI-3D-STATE-C2 | not stated | `emulator-5580`, 1080x2400 | Line dimension drawn; active axis leader before/after; product-path X editor open |
| `cad-foundation-c2/` | 1 | CAD-FOUNDATION-C2 | `3872120` (present in repo) | — | HUD at close/normal/far/very-far zoom (composite) |
| `cad-v6-s2-owner-correction-e2e/screens` | 5 | CAD-V6-S2 owner correction | `8ca3e6b` (present); CI DEVICE `36999488652`, 82/82 | CI emulator | Dock near axis, near edge, oblique left/right, palette open |
| `cad-v6-revolve-newbody/device-37391067580` | 6 | CAD-V6-REVOLVE-NEWBODY | CI DEVICE `37391067580`; dossier names `3e2443f` (present) | CI emulator, portrait | 360° preview, 90° preview, axis highlight, angle label, committed body, reopened revolve (`facts.txt` holds axis taps and drag) |
| `glb-import-r0/`, `glb-import-r1/` | 2, 1 | GLB-IMPORT-R0/R1 | R0 names `868995d…`, not resolvable; "OK (87 tests)"; R1 "OK (90 tests)" | — | Import preview diagnostic (now has no user control) |

## 3. Gaps in screen evidence on this tree

No committed screenshot exists for:

- the Freeform context surface;
- the Surface context and Surface Finish surfaces;
- the History surface (Steps/Sketches/Features). Its acts are asserted at font 1.3 and 1.5 by `CadRetainedSketchOwnerTest.d17`, with no capture;
- the Mirror plane chooser;
- the unsaved-changes and recovery questions;
- any font scale above 1.0;
- any tablet or foldable window on this tree.

Expanded layouts exist only from historical audits with a density override (`uiaudit1` E01–E04,
`stage019`/`stage020` "expanded"). Revolve exists only as the CI run `37391067580` dev captures.

## Fresh CI DEVICE evidence (appended by coordinator)

_Reserved for the coordinator. Seat F did not write below this line._
