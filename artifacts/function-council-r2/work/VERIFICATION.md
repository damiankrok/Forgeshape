# FUNCTION-COUNCIL-R2 — Adversarial verification record

Generated mechanically (python, no editing) from the workflow result of run `wf_8b7c4db9-993` (`work/` record; the agent that was meant to write this file failed on a provider-side safeguard flag, so the coordinator wrote it from the raw JSON). Each P0–P2 finding was judged by three independent lenses: **L1 reproduce**, **L2 refute-by-code**, **L3 scope/severity**. A finding *survives* when at least 2 of 3 lenses upheld it (did not refute).

Panel-verified: 30 of 38 P0–P2 findings (top 30 by severity); the remaining 8 lower-ranked P2 findings keep their seat grade and were NOT panel-verified.

## Findings

| ID | Seat | Sev. | Upheld | Survives | Title | Location |
| --- | --- | --- | --- | --- | --- | --- |
| A-01 | A | P1 | 3/3 | yes | Save and continue before Open Saved Project overwrites the saved project it is about to open | `app/src/main/java/com/forgeshape/app/EditorWorkspaceView.java:2665-2678 -> saveProjectToSlot 5458-5471 (ProjectSlot.write 5461, ProjectCheckpoint.clear 5467) ->` |
| A-02 | A | P1 | 3/3 | yes | Recovered work is protected nowhere: checkpoint deleted and both baselines set | `EditorWorkspaceView.java:5652-5687 (clear 5675, autosave.noteProjectPersisted 5676, noteProjectPersisted 5679); AutosaveController.performCheckpoint skip rule; ` |
| A-03 | A | P1 | 2/3 | yes | The app's saved project cannot be reopened from Home | `HomeView.java:47-79; ProjectSlot.java:63 (getFilesDir); drawer only with an open project; EditorWorkspaceView.java:2597-2602` |
| B-02 | B | P1 | 3/3 | yes | GLB export places a face-supported CAD body at its stored transform, not where it is drawn | `app/src/main/cpp/forgeshape_gltf_export.cpp:178-180 (captureGlbExportScene reads body.transform()); renderer/picker use ConstructionScene::resolveWorldModel (fo` |
| E-01 | E | P1 | 3/3 | yes | GLB export places a face-supported CAD body at its stored identity transform instead of its drawn, derived world placement | `app/src/main/cpp/forgeshape_gltf_export.cpp:176-185 (captureGlbExportScene uses body.transform(); renderer uses ConstructionScene::resolveWorldModel, forgeshape` |
| X-A01 | J | P1 | 3/3 | yes | Seat A's A-01 independently re-verified: Save-and-continue before Open Saved Project overwrites the slot it then opens | `EditorWorkspaceView.java:2665-2677, 2656, 5458-5483` |
| A-04 | A | P2 | 3/3 | yes | Import GLB is offered and accepted while a sketch is open inside a project | `EditorWorkspaceView.java:2823 (showImportAvailable(!sculpting)); forgeshape_jni.cpp:8590-8597 (Sculpt guard only); forgeshape_import_commit.cpp:61-63 (editInPro` |
| A-05 | A | P2 | 3/3 | yes | The saved baseline is lost on every Activity recreation (FUNCTION-COUNCIL-R1 D3, still present and wider) | `EditorWorkspaceView.java:271-272 (view fields); ForgeShapeActivity.java:268-280 requestTheme->recreate(); AndroidManifest.xml:31 configChanges lacks uiMode/font` |
| A-06 | A | P2 | 3/3 | yes | Save Copy / Export can silently write nothing after a recreation during the picker, leaving an empty file | `EditorWorkspaceView.java:6004 pendingCopyBytes, 5778 pendingGlbBytes; null treated as cancel 6050-6055, 5788-5793; ProjectTransfer.java:96,123 ACTION_CREATE_DOC` |
| A-07 | A | P2 | 2/3 | yes | Leaving a project discards an open sketch or staged edit without a question | `EditorWorkspaceView.java:2597-2602 (fingerprint-only dirty); forgeshape_jni.cpp:8977-8980 closeProject, 8478-8482 loadProject cancel the sketch` |
| B-01 | B | P2 | 3/3 | yes | A face-supported CAD body is transformable; the write records Undo and moves the fingerprint but changes nothing visible | `forgeshape_jni.cpp:1335-1385 (applyBoxTransform guards only NoProject/Locked); forgeshape_gizmo.cpp:1197-1235, 1415, 1428 (no face-support check); EditorWorkspa` |
| B-03 | B | P2 | 3/3 | yes | ARCHITECTURE.md claims isFaceSupportedCad keeps the gizmo and exact-value editors off; no code does | `ARCHITECTURE.md:2607-2608` |
| C-01 | C | P2 | 3/3 | yes | Orientation navigator Flip and +-90 do not move the camera | `app/src/main/cpp/forgeshape_jni.cpp:313-319 (beginSketchView reads frame()); :6611-6648; forgeshape_sketch_session.cpp:648 viewFrame has no production caller; A` |
| C-02 | C | P2 | 3/3 | yes | Import GLB accepted while a sketch is open (same as Seat A A-04) | `forgeshape_jni.cpp:8594-8600 (Sculpt guard only); EditorWorkspaceView.java:2823 showImportAvailable(!sculpting)` |
| C-06 | C | P2 | 2/3 | yes | Core sketch device classes not re-verified on the audited tree | `SketchUxTest, SpatialSketchTest (last run 36465122847/36548109667); SketchExtrudeTest, CadVerticalSliceTest, CadCanvasExtrudeTest, CadExtrudeExtentTest, CadPlan` |
| D-01 | D | P2 | 3/3 | yes | Undo after an over-cap (NotRetained) sculpt stroke lands on a never-existing state and clears hasEdits | `app/src/main/cpp/forgeshape_sculpt_history.cpp:80-101 (record keeps undo stack on NotRetained); forgeshape_sculpt.cpp:1640 undoStroke; consumers SculptContextVi` |
| D-02 | D | P2 | 3/3 | yes | Import GLB is offered and accepted while a sketch is open | `EditorWorkspaceView.java:2823 showImportAvailable(!sculpting); ProjectDrawerPolicy.java:107 markShown; forgeshape_jni.cpp:8590-8597 (Sculpt-only guard); forgesh` |
| E-02 | E | P2 | 3/3 | yes | The recovery question reappears over the live project after an Activity recreation in a process that had no candidate at launch | `EditorWorkspaceView.java:1027-1029 and 5579-5582; EditorUiState.java:151; ForgeShapeActivity.java:268-280; AndroidManifest.xml:31` |
| E-03 | E | P2 | 3/3 | yes | Import GLB is drawn and accepted below JNI while a sketch is open (FUNCTION-COUNCIL-C1 debt, CONFIRMED-OPEN) | `forgeshape_jni.cpp:8586-8604 (asks only inSculptMode); EditorWorkspaceView.java:2823 showImportAvailable(!sculpting); forgeshape_import_commit.cpp:150,175` |
| E-04 | E | P2 | 3/3 | yes | A palette change, or any recreation, forgets that the project was saved (D3, CONFIRMED-OPEN) | `EditorWorkspaceView.java:271-272, 2597-2614; ForgeShapeActivity.requestTheme 268-280` |
| F-01 | F | P2 | 3/3 | yes | Several chrome controls fall below the 48 dp hit-area floor in width | `app/src/main/java/com/forgeshape/app/EditorControlStyles.java:366-382 (chip() sets only min height); SketchOrientationNavigatorView.java:134-137; DisplaySetting` |
| F-02 | F | P2 | 3/3 | yes | Orientation navigator Flip and +/-90 change no camera; only the label changes (same defect as Seat C C-01 and FC-R1 D2) | `app/src/main/cpp/forgeshape_jni.cpp:313-320 beginSketchView frames frame(); forgeshape_sketch_session.cpp:648 viewFrame has no production caller; JNI :6611-6648` |
| F-03 | F | P2 | 3/3 | yes | In the CAD bootstrap, System Back discards the whole drawn sketch even when a panel or palette is open | `app/src/main/java/com/forgeshape/app/EditorWorkspaceView.java:1177-1184 (bootstrap branch runs before :1196-1229); SketchModifyView.paletteOpen (:97) and CadExt` |
| F-04 | F | P2 | 3/3 | yes | On API 33+, Back during an in-project Revolve backgrounds the app; below API 33 it returns to Extrude | `app/src/main/java/com/forgeshape/app/EditorWorkspaceView.java:1112-1117 hasDismissibleSurface omits the revolve state handled at :1166-1175; ForgeShapeActivity.` |
| F-05 | F | P2 | 3/3 | yes | Three canvas editors silently drop the comma decimal separator, so '2,5' becomes 25 | `BodyDimensionLabelsView.java:127, CadExtrudeCanvasView.java:644, SketchDimensionLabelView.java:112 use DigitsKeyListener('0123456789.-'). NumericPropertyRow.jav` |
| G-01 | G | P2 | 3/3 | yes | Two native self-test oracles fail under FP contraction, which is the NDK's arm64 default | `app/src/main/cpp/forgeshape_history_selftest.cpp:474-480 (eviction_is_deterministic_oldest_first); app/src/main/cpp/forgeshape_cad_feature_selftest.cpp:5392-539` |
| G-02 | G | P2 | 2/3 | yes | Domain platform-neutrality (ARCH-OWNER-01) is not guarded by any CI workflow | `.github/workflows/ci-fast.yml, ci-device.yml, ci-full-sharded.yml (0 references to scripts/host-native-selftests.sh); ARCHITECTURE.md:208 'must stay asserted'` |
| H-01 | H | P2 | 3/3 | yes | A renderer that stops mid-session is not reported (no status message, no immediate checkpoint) until the next onResume | `app/src/main/java/com/forgeshape/app/ForgeShapeActivity.java:326-382; EditorWorkspaceView.java:6147-6155; app/src/main/cpp/forgeshape_jni.cpp:2049-2066` |
| H-05 | H | P2 | 2/3 | yes | Heavy CAD derivation (Finish arrangement, entity placement, candidate regeneration) runs synchronously under g_stateMutex on the UI or render thread | `app/src/main/cpp/forgeshape_jni.cpp:4400-4420 (sketchFinish), :1733 (render-thread evaluateCandidate), :5098; forgeshape_sketch_session.cpp:2987-3029` |
| H-06 | H | P2 | 2/3 | yes | Each sculpt stroke sample costs a whole-mesh republish, a full render-mesh rebuild, a full upload and a GPU drain | `app/src/main/cpp/forgeshape_jni.cpp:10036-10040; forgeshape_sculpt.cpp:611; forgeshape_renderer.cpp:1119, 868, 749` |

Duplicates by root cause (from the verdict notes and the seat records): A-01 = X-A01; B-02 = E-01; A-04 = C-02 = D-02 = E-03; C-01 = F-02; A-05 = E-04. De-duplicated, the panel confirmed **23 distinct root causes** (4 ranked P1 by their seats: A-01, A-02, A-03, B-02, see severity notes in `COUNCIL_DELIBERATION.md` for the round-2 regrades, e.g. A-01 → P2/H1, A-03 → P2 BLOCKED-OWNER).

## Positive-claim challenges (two skeptics each; a claim *stands* only if neither refuted it)

| Seat | Claim | Stands |
| --- | --- | --- |
| A | Home and the shell phase are re-derived from native projectOpen() on every refresh, and a recreation lands where native truth says | NO — overstated at its stated grade |
| A | A failed open is all-or-nothing: decode happens before any mutation and the load is staged | NO — overstated at its stated grade |
| B | Hidden bodies are excluded from draw and pick by a single predicate in ConstructionScene::snapshot | NO — overstated at its stated grade |
| B | A gizmo drag is one captured pointer and exactly one history step; cancel or a second pointer restores all nine values | NO — overstated at its stated grade |
| C | The whole CAD/sketch native domain passes: CAD 155, CAD_A3 66, SKETCH_UX 118 and CAD_FEATURE 638 checks, including 17 geometry oracles and 17 PICK_C2 seam checks | NO — overstated at its stated grade |
| C | All 39 CAD corpus fixtures decode to their recorded verdicts, and the refusal fixtures are refused by name | NO — overstated at its stated grade |
| D | CAD, Freeform and Surface bodies can never become a sculpt source. Start Sculpting refuses each by name and the control is absent. SCUL over CADB/FRFM/SURF is refused by the codec. | yes |
| D | Freeform Symmetry is exact bit for bit, and an on-plane vertex stays on the plane. | yes |
| E | The .forge decoder and load are fail-closed: no crash, no partial load, and a refused load never mutates the live scene | NO — overstated at its stated grade |
| E | The encoder is canonical and matches the committed corpus | NO — overstated at its stated grade |
| F | Home and New Project are opaque full-window pages; New Project replaces Home; New CAD enters an XY sketch at once with no project and no chooser | NO — overstated at its stated grade |
| F | System Back closes the topmost anchored surface and the app stays, through the real platform key route on API 36 | NO — overstated at its stated grade |

**Reading:** 10 of 12 strongest positive claims were judged overstated by at least one skeptic — typically because the cited test calls a native seam or `syncFromNative()` directly rather than the real UI path, or because a property is proven for the codec but not for every load entry point. They remain *supported* at E2 (source) but are **not** upgraded to PASS-REPRODUCED. See the per-seat Round 2 sections for the specific objections.

## Not panel-verified (seat grade only)

8 P2 findings ranked below the top 30; listed in the seat files with their own grades.
