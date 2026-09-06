# Device E2E — `E2E-UIPREFR1-01..18` (UI-PREF-R1 Part M)

All on the isolated `ForgeShape_Stage006` / `emulator-5580` AVD (1080 × 2400 @
420 dpi, API 36), through the real chrome, real `MotionEvent`s where a gesture
is involved, and native truth read back. No control is located by coordinate.
Results are in `TEST_RESULTS.md`; the frames are in `frames/` and
`VISUAL_EVIDENCE.md`.

| # | journey | where it is proven |
| --- | --- | --- |
| 01 | cold launch → Home → Settings | `SettingsPreferencesTest.uiprefr1_01` (Home's Settings row, the page full-window, Home replaced, no project created, Back returns Home); frames `01_home_settings_entry`, `02_settings_page` |
| 02 | choose Warm Light → return Home → visible light theme | `uiprefr1_22_26` (WARM_LIGHT in force, viewport index 3, window luminance > 0.6, dark bar icons); frame `11_home_warm_light` (Home captured in Warm Light after the editor frame `10_editor_warm_light`) |
| 03 | kill / relaunch → Warm Light persists | in-process half: `uiprefr1_06` (`existsOnDisk`, cache dropped, fresh read equal). Process-death half: the relaunch transcript in `RELAUNCH.md` — the app force-stopped, a preference file with `COOL_LIGHT / LEFT / 1.5 / BOLD` in place, a cold `am start`, and logcat showing `FORGESHAPE_VIEWPORT_BACKGROUND:CoolLight`, `FORGESHAPE_GIZMO_VISUAL_SCALE:1.500 … accepted=1`, `FORGESHAPE_GIZMO_STROKE_WEIGHT:Bold … changed=1` and `FORGESHAPE_GIZMO_UPLOAD_OK weight=Bold` from the very first frame |
| 04 | choose Cool Light | `uiprefr1_22_26` (second iteration), `uiprefr1_06`; frames `12_editor_cool_light`, `13_home_cool_light` |
| 05 | open project → Settings → each of the three dark palettes | `uiprefr1_02` (the Project surface door) and `EditorWorkspaceThemeTest.r1b2_03/04/05` (every palette, through the page, the default restored exactly); frames `04`–`09` (editor and Home in Warm Graphite, Neutral Charcoal, Light Charcoal) |
| 06 | Handedness Left → rail moves left | `uiprefr1_13_14` (8 dp off the left edge, same width and top, first in the row, nothing on the right); frame `15_left_handed_editor` (`rail_left_inset_dp=8.0`) |
| 07 | Exact/Details in left-handed mode → opens inward, no overlap | `uiprefr1_15_17` (short landscape, seated after the host, `panel.left ≥ host.right + gap`, no intersection); frame `16_left_handed_exact_open` (`exact_overlaps_rail=false`) |
| 08 | rotate / recreate → Left persists | `uiprefr1_05` (`ActivityScenario.recreate()`), `uiprefr1_15_17` (portrait → landscape with Left in force) |
| 09 | Handedness Right → the accepted right layout | `uiprefr1_13_14` (the right-handed `Rect` restored exactly), `uiprefr1_03_04` / `assertRightHandedFrame`, and `EditorWorkspaceRightHostPlacementTest` UILR2C-01..12 unchanged on the default |
| 10 | gizmo size minimum → Move / Rotate / Scale still pickable | `uiprefr1_30_35_36` at 0.9: every handle of every mode grabbed at its own native pixel; frame `18_gizmo_smallest_thin` (handle pixels with `hit=` per handle) |
| 11 | gizmo size maximum → usable, no overlap catastrophe | `uiprefr1_30_35_36` at 1.5; frames `19_gizmo_largest_bold`, `20_gizmo_largest_bold_rotate` |
| 12 | thickness min / max | `uiprefr1_28_33` (Thin and Bold reach native), `uiprefr1_33` (no handle moved); frames `18`, `19` |
| 13 | handle style | not implemented — `GIZMO_STYLE_DEFERRED_BY_RENDERER_CONTRACT`; `uiprefr1_40` asserts no such row is drawn |
| 14 | switch palette with an object selected → selection and project unchanged | `uiprefr1_09_10_11_37` (two palette switches with a turned cone selected; bytes, fingerprint, selection, revision identical); `EditorWorkspaceThemeTest.r1b2_06/07` |
| 15 | switch Settings in a CAD project → saved bytes semantically identical | `uiprefr1_09_10_11_37` compares `encodeProject()` byte for byte — the same bytes Save writes — after every preference changed. The baseline case is the Construction box the suites share; the CAD bootstrap and `SketchExtrudeTest` are unchanged and green in the aggregate, and nothing in `forgeshape_project_*.cpp` or the CAD domain changed (`DELETE_HOLD.md`) |
| 16 | New CAD / Sketch navigator usable with a left-handed rail | the navigator is anchored to the upper TRAILING corner of the overlay and the left rail stands at the leading edge, so the two never share an edge; `SketchUxTest` and `SpatialSketchTest` are unchanged and green in the aggregate. No dedicated left-handed sketch case was added: the navigator's contract is its own (Part C3) and no overlap exists to adjust for |
| 17 | Sculpt-mode gizmo / settings regression | Sculpt has no gizmo (refused by name, unchanged); the brush controls mirror with the rail (`HANDEDNESS.md`); `EditorWorkspaceSculptRetentionTest`, `SculptUndoTest`, `ImportedMeshSculptTest` unchanged and green in the aggregate |
| 18 | Delete present and unchanged — NOT the OWNER verdict | `ObjectsDeleteTest` unchanged and green in the aggregate; `DELETE_HOLD.md` proves no Delete file changed. This is not, and is not claimed to be, the OWNER's manual Delete → Undo → Redo acceptance |
