# FUNCTION-COUNCIL-R2 — Feature inventory (every user-facing control and path)

Audited tree `70a6c14` (product tree `app/src/main` = `f3db953`). Synthesis
document: it collects the seats' evidence and adds a control-by-control census
taken from source in this stage. It changes nothing in the product.

Companion files: `END_TO_END_JOURNEYS.md` (journeys built on these controls),
`UI_SCREEN_STATE_MATRIX.tsv` (measured screen states), `TEST_COVERAGE_MATRIX.md`
(test classes and suites), `work/req_*.tsv` (requirement rows cited below as
`APP-`, `CON-`, `CAD-`, `MOD-`, `DOC-`, `UI-`, `PORT-`, `RND-`, `TST-`, `ADV-`),
`SEAT_A.md` … `SEAT_J.md` (findings cited as `A-01`, `F-03` …). Fresh test
executions and CI run ids are recorded by the coordinator in `TESTS.md`.

## 0. How this census was made, and what it can and cannot say

**Method (E2, reproducible).** All commands were read-only. Scratch files are in
`/tmp/claude-0/-home-user-Forgeshape/2f5454c6-63b1-52b8-b92c-5f8b69c420c6/scratchpad/`.

1. Every id declared in `app/src/main/res/values/ids*.xml` (six files:
   `ids.xml` 334, `ids_modeling.xml` 67, `ids_cad_r2.xml` 16,
   `ids_cad_revolve.xml` 14, `ids_cad_hud.xml` 9, `ids_cad_vs.xml` 6 =
   **446 ids**) was matched against `grep -n "R\.id\.<id>\b"` over
   `app/src/main/java/com/forgeshape/app/*.java`. **All 446 are referenced by
   product Java** (0 declared-but-unused, 0 used-but-undeclared). The file:line
   given in the tables is the first reference, normally the `setId` site.
2. Each handler was traced to its `NativeViewport.*` calls by an awk pass over
   `EditorWorkspaceView.java` (6655 lines) and the 15 view files that call
   JNI directly (`ObjectsSectionView`, `FreeformEditorView`,
   `SurfaceEditorView`, `SculptContextView`, `ConstructionPlacementEditorView`,
   `ConstructionShapeEditorView`, `CadFeatureEditorView`,
   `CadExtrudeCanvasView`, `RelativeScaleEditorView`, `SketchEditorView`,
   `ForgeShapeSurfaceView`, …). Native refusal names are the ones the seats
   traced into `forgeshape_jni.cpp` and the domain; they are cited with the
   seat row that proves them.
3. Test coverage per control is `grep -l "R\.id\.<id>\b"` over
   `app/src/androidTest/java/com/forgeshape/app/*.java`. **357 of 446 ids are
   referenced by at least one androidTest class; 89 are referenced by none**
   (list in §19). A reference is not an assertion: many classes press a
   control through `performClick()`, which skips hit testing and occlusion
   (`TEST_COVERAGE_MATRIX.md` FG-3, 482 call sites).

**Run tags used in the "Tests" column** (status of the class's most recent
green run, not of each method):

| Tag | Meaning | Grade |
| --- | --- | --- |
| `FC2-D1` | CI DEVICE `40797031704` dispatched by this audit: Ui3dStateAuditTest, SketchUxVisualEvidenceTest, UiPrefVisualEvidenceTest, SelectionOutlineVisualEvidenceTest, EditorWorkspaceLegibilityTest — 32/32 (`evidence/device-run-40797031704/`) | E1 |
| `FC2-D2` | CI DEVICE `40800569636` dispatched by this audit: SurfaceOwnerTest, FreeformOwnerTest, Stage027SculptWorkflowTest, SculptHistoryNavigatorTest, SculptUndoTest, SculptBrushStage025Test, ImportGlbSculptGuardTest, ImportedMeshSculptTest, CadVerticalSliceTest, CadExtrudeExtentTest, SketchUxTest — 77/77 (`evidence/device-run-40800569636/`) | E1 |
| `FC2-D3` | CI DEVICE `40805804385` dispatched by this audit: ProjectAutosaveRecoveryTest, ProjectTransferTest, ProjectProcessDeathTest, ImportedMeshDurableTest, ObjectsDeleteTest, ObjectCommandsTest, MirrorSmokeTest, BodyDimensionsSmokeTest, EditorWorkspaceLifecycleTest, JniBoundaryHardeningTest, DiagnosticsAndRendererLossTest, SettingsPreferencesTest — 90/90 (`evidence/device-run-40805804385/`) | E1 |
| `C2` | CI DEVICE `40776968165` (59/59) + `40782861799` (67/67) on product tree `f3db953` (= audited product tree): CadRetainedSketchOwnerTest, EditorWorkspaceCorrectionTest, OwnerProjectShellSketchSupportTest, CadMultiFaceOwnerTest, CadFillPickR2Test, CadHud3dOwnerTest, CadRevolveOwnerTest, CadSketchDraftingOwnerTest, CadParametricHistoryOwnerTest, HomeFlowTest, EditorWorkspaceProjectActionsTest (`artifacts/cad-r2-c2-picking-seam/TESTS.md`) | E3 |
| `OLD` | last green run predates the CAD-R2 product changes (latest aggregate `FULL_SHARDED_SUITE_PASS` = CI FULL SHARDED `36607747079` on `fa0b6b4`, `artifacts/cad-vertical-slice-r1/FULLSHARDED_C4.md`) | E3, older tree |
| `HOST` | `HOST_SELFTESTS_OK (4392 checks, 0 failed)`, 25 suites, run by this audit (`evidence/local/host_selftests.txt`) | E1 |
| `JVM` | 20 classes / 196 tests, 0 failures, run by this audit | E1 |

By this split, 28 of the 70 androidTest classes with `@Test` methods have an
E1 run in this audit (5 + 11 + 12 classes in the three FC2 DEVICE runs, 32/32,
77/77 and 90/90, **199/199** in all, each with 25/25 startup tokens in order,
emulator only), 11 have a C2 run on the identical product tree, and the other
**31** rest on `OLD` runs only. The split is the one `TESTS.md` §3 states and
§4 lists by name (31 of 70); it was re-derived for this file by listing every
`app/src/androidTest/**/*.java` with at least one `@Test` (70) and subtracting
the 28 + 11 classes named in the run lists above (E2 arithmetic). Every class
tagged `OLD` in the tables below is one of `TESTS.md` §4's 31. (Earlier drafts
cited `TEST_COVERAGE_MATRIX.md` §4 for this split; that section predates the
FC2 runs and marks most of the 28 FC2 classes "older", so it is not the source.)
The head SHA of each FC2 run (`9cfc3f5` for `40796992679`, `40797031704`,
`40800569636`; `7c05748` for `40805804385`) is recorded in `TESTS.md` §1.2,
whose product, test, script and workflow trees are byte-identical to
`70a6c14`.

**Verdicts on defects.** `work/VERIFICATION.md` records the adversarial panel:
the top 30 of 38 P0–P2 findings were each judged by three lenses (reproduce,
refute-by-code, scope/severity), and **all 30 survived** (≥ 2 of 3 upheld; 23
distinct root causes after de-duplication: A-01 = X-A01, B-02 = E-01,
A-04 = C-02 = D-02 = E-03, C-01 = F-02, A-05 = E-04). No defect cited in this
file is withdrawn by it. A defect the panel verified is labelled
**"panel-verified"** below; every other defect (all P3 findings and the 8
lower-ranked P2 findings) keeps the **seat's Round-2 verdict**, labelled
"seat R2", and was not panel-verified. Severities are the seats' Round-2
grades; where the panel record lists a different grade it is noted in §20.
Findings already withdrawn by the seats in Round 2 (B-07, C-03's Keep-Sketch
half, J-05's revolve half, F-06's column-offset fix) are not presented as
defects.

**Positive claims the panel judged overstated.** `work/VERIFICATION.md` put
the twelve strongest positive claims to two skeptics each; **10 of 12 were
judged overstated** at their stated grade (typically because the cited test
calls a native seam or `syncFromNative()` rather than the real UI path, or the
property is proven for the codec but not for every load entry point). Only
Seat D's two (CAD / Freeform / Surface never become a sculpt source; Freeform
Symmetry is exact bit for bit) stand. The rows below that restate one of the
ten are therefore **source-supported (E2) only, not reproduced**, and are
marked "panel: overstated":
the shell phase re-derived from `projectOpen()` on every refresh, and a
recreation landing where native says (§1, Home); a failed open being
all-or-nothing with nothing changed (§1.1 Open File, §2 Recover, §4 Open
Saved Project — see also E-12); hidden bodies excluded from draw and pick by one predicate
(§11 Tap); a gizmo drag being one pointer and one step with full restore
(§11 Gizmo drag); the native CAD/sketch suites and the 39 CAD corpus verdicts
as proof of the whole CAD domain (`HOST` and corpus tags stand as run
results, not as that claim); the `.forge` decoder/load fail-closed and the
encoder canonical (the same load rows; the corpus parity and verdict runs in `TESTS.md` §1 stand as results); Home / New Project as opaque pages with
New CAD entering XY at once (§1.1, §1.2 CAD); and System Back closing the
topmost surface through the real key route (§18).

**Horizons.** The canonical Master Plan and OWNER_DECISIONS are not available
(`BEFORE.md`), so every control's horizon is the one reconstructed in the
`req_*.tsv` row it cites (`PARTIAL-PLAN-SOURCE-GAP`). No H2 item is listed as
missing.

**Column legend.** *Act → native*: Java handler → `NativeViewport` call(s).
*Drawn / absent*: the predicate that sets visibility (source). *Refusal*: what
happens when the act cannot succeed, below JNI and above it.

---

## 1. Start pages (no project open)

The process starts with an EMPTY scene (`ConstructionScene(NoProjectTag)`);
`EditorWorkspaceView.refreshShellPhase` (`:1999`) derives the page from
`NativeViewport.projectOpen()` on every refresh (`APP-001`, `APP-004`; E2 —
panel: overstated as a reproduced claim, and E-02 shows a recreation can
re-raise the recovery question over a live project).

### 1.1 Home (`HomeView`, inside `StartPageView`)

| Control | id (def) | Act → native | Drawn / absent | Refusal | Req | Tests |
| --- | --- | --- | --- | --- | --- | --- |
| Home page | `home_surface`, `home_panel` (`HomeView.java:46`) | — | drawn exactly when `!projectOpen()` and no chooser / bootstrap / Settings page | — | APP-001..004, UI-001 | HomeFlowTest#e2eAppH1_01 (C2); SketchUxTest#cadUxR1_01_02_03 (FC2-D2) |
| Wordmark / headline / mark | `start_page_wordmark`, `start_page_mark`, `start_page_product_name`, `start_page_headline` (`StartPageView.java:110-129`) | none (static vector, never interactive) | with Home and New Project | — | UI-001 | SketchUxTest (FC2-D2); `start_page_mark`, `start_page_product_name` referenced by no test |
| **New Project** | `home_new_project` (`HomeView.java:49`) | opens New Project page (Java only) | Home | — | APP-002, APP-008, APP-020 (DOC-DIVERGENCE: description `strings.xml:29` names only CAD, A-09) | 14 classes incl. HomeFlowTest#e2eAppH1_02 (C2) |
| **Open File…** | `home_open_file` (`HomeView.java:58`) | `ProjectTransfer` `ACTION_OPEN_DOCUMENT` → `onOpenProjectDocumentChosen` (`:6077`) → `NativeViewport.loadProject` | Home | cancel = no-op; damaged / truncated / newer / not-a-project → refused by codec status, nothing changes (APP-006, APP-031; E2 — panel: overstated, and the JNI load wrapper resets sketch/view/gesture state before a post-decode refusal, E-12); picker launch failure message names the wrong act (A-08, APP-007 DEFECT-CONFIRMED seat R2, P3) | APP-006, APP-082 | HomeFlowTest#e2eAppH1_05/06/07 (C2); ProjectTransferTest (FC2-D3) |
| Settings | `home_settings` (`HomeView.java:68`) | opens Settings page | Home | — | APP-064, UI-054 | SettingsPreferencesTest#uiprefr1_01 (FC2-D3) |
| (absent) Open Saved Project | — | — | **not offered at Home**; the slot can only be reopened from inside a project | — | APP-032 BLOCKED-OWNER (A-03) | — |

### 1.2 New Project page (`NewProjectChooserView`)

| Control | id (def) | Act → native | Drawn / absent | Refusal | Req | Tests |
| --- | --- | --- | --- | --- | --- | --- |
| CAD | `new_project_cad` (`:51`) | `beginCadBootstrap` (`EditorWorkspaceView.java:2363`) → `sketchBegin(WORKPLANE_XY)` — no `ObjectId`, no body (E2; panel: the "opaque pages, New CAD enters XY at once" claim is overstated as reproduced) | New Project page | sketch begin failure stays on page | APP-011, CAD-001 (PARTIAL), UI-004 | HomeFlowTest#e2eAppH1_03 (C2); SketchUxTest#cadUxR1_05_06_07 (FC2-D2) |
| Sculpt | `new_project_sculpt` (`:60`) | `onNewSculptProjectChosen` (`:2403`): `beginSessionInitialization` → `sceneAddBody` → `applyConstructionSphere` → `freezeToSculpt` → `endSessionInitialization`; on refusal `closeProject` | page | any refusal closes the project again (APP-085 IMPLEMENTED-UNTESTED) | APP-015, CON-067, CON-089 | HomeFlowTest#e2eAppH1_04 (C2); SketchUxTest#cadUxR1_39 (FC2-D2) |
| Freeform | `new_project_freeform` (`:69`) | `onNewFreeformProjectChosen` (`:2440`): bracket → `freeformCreateBody(FREEFORM_FORM_BOX)`; refusal → `closeProject`, status `freeformStatusToken` | page | refusal text bypasses `FreeformPresentation.refusalMessage` (A-11, APP-017 PARTIAL) | APP-016, MOD-069 | FreeformOwnerTest (FC2-D2); HomeFlowTest (C2) |
| Surface | `new_project_surface` (`:78`) | `beginSurfaceSketch(NO_OBJECT, 0)` (`:2483`) → `surfaceBeginSketch(body, WORKPLANE_XY, offset)` | page | Back to Home before the first Finish costs nothing (APP-019 IMPLEMENTED-UNTESTED) | APP-018, MOD-102 | SurfaceOwnerTest (FC2-D2) |
| Back | `new_project_cancel` (`:88`) | back to Home | page | — | APP-009 | HomeFlowTest#e2eAppH1_02 (C2) |

New Project opened from the drawer over a project closes the project before the
page appears (`APP-010` DOC-DIVERGENCE: PRODUCT.md wording).

### 1.3 Settings page (`SettingsPageView`)

All rows write `AppPreferences` through `AppPreferencesStore`; none reaches a
`.forge` byte, the fingerprint, a history step or a checkpoint (`APP-065`
PARTIAL — the *saved baseline* is lost on the recreation a palette change
causes, A-05; `UI-060`). Rows set `setSelected` + description.

| Control | id (def) | Act → native | Drawn / absent | Refusal | Req | Tests |
| --- | --- | --- | --- | --- | --- | --- |
| Palette ×5 (Warm Graphite, Neutral Charcoal, Light Charcoal, Warm Light, Cool Light) | `appearance_warm_graphite`, `appearance_neutral_charcoal`, `appearance_light_charcoal`, `appearance_warm_light`, `appearance_cool_light` (`SettingsPageView.java:55-57`) | `ForgeShapeActivity.requestTheme` → `recreate()`; native colours via `viewportBackgroundIsLight` | always on the page | — | APP-066, UI-056 | SettingsPreferencesTest#uiprefr1_22_26 (FC2-D3); EditorWorkspaceThemeTest (OLD) |
| Handedness Right / Left | `handedness_right`, `handedness_left` (`:59`) | `applyHandedness` (Java edge anchoring only) | always | — | APP-067, UI-058 | SettingsPreferencesTest#uiprefr1_13_14, #uiprefr1_16_18 (FC2-D3) |
| Gizmo size 90/100/125/150 % | `gizmo_size_small/default/large/largest` (`:65-66`) | `pushGizmoPreferences` (`:2220`) → `setGizmoVisualScale` | always | out-of-range refused below JNI, never clamped (CON-019) | APP-068, UI-059 | SettingsPreferencesTest#uiprefr1_28_33, #uiprefr1_30_35_36 (FC2-D3) |
| Gizmo stroke Thin / Regular / Bold | `gizmo_weight_thin/regular/bold` (`:72`) | `setGizmoStrokeWeight` | always | — | APP-068, CON-094 | SettingsPreferencesTest#uiprefr1_33 (FC2-D3) |
| Tool labels Off / On | `tool_labels_off`, `tool_labels_on` (`:75`) | Java preference (captions the CAD HUD palette) | always | — | APP-069 (DOC-DIVERGENCE: Home description) | SettingsPreferencesTest (FC2-D3) |
| Back | `settings_back` (`:164`) | return to where opened (Home or editor) | bottom of the scrolling page (reachable only after scrolling, F-11 INFO) | — | APP-064, UI-055 | SettingsPreferencesTest#uiprefr1_01/02 (FC2-D3) |
| (absent) handle style | — | — | absent by renderer contract (`GIZMO_STYLE_DEFERRED_BY_RENDERER_CONTRACT`) | — | APP-068 | SettingsPreferencesTest#uiprefr1_40 (FC2-D3) |

## 2. Questions asked over a live project (`ChooserSurfaceView` modals)

| Control | id (def) | Act → native | Drawn / absent | Refusal | Req | Tests |
| --- | --- | --- | --- | --- | --- | --- |
| Unsaved changes question | `unsaved_prompt`, `unsaved_prompt_panel` (`UnsavedChangesPromptView.java:35`) | shown when `projectDirty()` (fingerprint ≠ last Save/Open/Recover baseline, `EditorWorkspaceView.java:2597-2602`) before New Project…, Open Saved Project, Open File… | only when dirty | — | APP-033 | HomeFlowTest#e2eAppH1_08/09/10 (C2) |
| Save and continue | `unsaved_save` (`:39`) | `saveProjectToSlot` (`:5458`: `encodeProject` → `ProjectSlot.write`, checkpoint cleared) → `proceedToLeave` | in question | failed save keeps the project (APP-036). **Before Open Saved Project this overwrites the slot it is about to open (A-01, APP-037, ADV-027 DEFECT-CONFIRMED, panel-verified 3/3, P2)** | APP-036, APP-037 | HomeFlowTest#e2eAppH1_10 covers Open File only (C2) |
| Discard changes | `unsaved_discard` (`:48`) | retire checkpoint → `proceedToLeave` → `closeProject` | in question | Discard then a cancelled/refused Open leaves the dirty live project without a checkpoint (E-08, DOC-048 PARTIAL) | APP-035 | HomeFlowTest#e2eAppH1_09 (C2) |
| Cancel — stay | `unsaved_cancel` (`:58`) | nothing | in question | System Back = Cancel | APP-034 | HomeFlowTest#e2eAppH1_08/10 (C2) |
| Recovery question | `recovery_prompt`, `recovery_prompt_panel` (`RecoveryPromptView.java:49-54`) | `offerRecoveryIfPresent` (`:5579`) → `validateProject` (decodes, applies nothing) | once per process when `recovery.forge` decodes; corrupt/newer candidate quarantined | can reappear over the live project after a recreation (E-02, DOC-044 DEFECT-CONFIRMED, panel-verified 3/3, P2); `validateProject` accepts a PlanarFaces file `loadProject` refuses (E-05, DOC-028/ADV-003 DEFECT-CONFIRMED, P3) | APP-055, APP-081 | ProjectAutosaveRecoveryTest#fsr1b04_*, #fsr1b08_* (FC2-D3) |
| Recover | `recovery_recover` (`:80`) | `onRecoverRequested` (`:5652`) → `loadProject` | in question | failed load changes nothing (E2; panel: overstated, see E-12). After success the checkpoint is deleted and the project is marked persisted, so recovered work is protected nowhere (A-02, APP-057/ADV-026 DEFECT-CONFIRMED, panel-verified 3/3, P1; inverts pinned FSR1B-05 → owner decision) | APP-056, APP-057 | ProjectAutosaveRecoveryTest#fsr1b05/06; ProjectProcessDeathTest#stage6/8 (FC2-D3) |
| Discard (recovery) | `recovery_discard` (`:92`) | delete candidate only; slot untouched | in question | System Back over the question: DOC-DIVERGENCE (APP-060) | APP-058 | ProjectAutosaveRecoveryTest#fsr1b07 (FC2-D3) |

## 3. Global Toolbar (`GlobalToolbarView`)

| Control | id (def) | Act → native | Drawn / absent | Refusal | Req | Tests |
| --- | --- | --- | --- | --- | --- | --- |
| Toolbar, groups | `global_toolbar` (`:147`), `toolbar_mark_group` (`:174`), `toolbar_editing_group` (`:193`), `toolbar_utility_group` (`:373`) | containers | editor phases | — | UI-005 | EditorWorkspaceLayoutTest etc. (OLD) |
| **ForgeShape mark** | `project_actions_button` (`:176`) | opens the project drawer (§4) | `ProjectDrawerPolicy.markShown = projectOpen && !bootstrap` (`ProjectDrawerPolicy.java`); withdrawn in the CAD bootstrap (`GlobalToolbarView.java:685`) | — | APP-021, UI-005 | OwnerProjectShellSketchSupportTest#devOss01 (C2); 19 classes |
| Context label | `editing_context_label` (`:204`) | none (reads Sketch on XY/XZ/YZ, New CAD, Sculpt, Imported Mesh, CAD Body, Freeform, Surface, Construction, `:870-880`) | when the editing group is shown | — | UI-041 | ImportedMeshSculptTest (FC2-D2); SketchExtrudeTest (OLD) |
| Start Sculpting | `freeze_to_sculpt` (`:236`) | `onFreezeToSculpt` (`EditorWorkspaceView.java:5267`) → `freezeToSculpt` | `!sculpting && !hasFrozenMesh && !cad && !freeform && !surface && !sketching && !bootstrap && !activeHidden` (`:896`) | native: `CadBodyNotSculptable`, Freeform/Surface by name, hidden body `SCULPT_REFUSED_HIDDEN_BODY` (MOD-041, MOD-061..063); **in-sketch refusal untested** (MOD-045 IMPLEMENTED-UNTESTED) | MOD-033, MOD-034 | 28 classes; Stage027SculptWorkflowTest#guard2_* (FC2-D2) |
| Resume Sculpt | `resume_sculpt` (`:252`) | `onResumeSculpt` (`:5309`) → `enterSculptMode` | `!sculpting && hasFrozenMesh && !sketching && !bootstrap && !activeHidden` (`:898`) | hidden body refused (MOD-041); **no native sketch guard** (D-07, MOD-046 PARTIAL seat R2, P3) | MOD-035 | Stage027SculptWorkflowTest#guard2_aHiddenSculptBodyCannotResume (FC2-D2) |
| Back to Construction / Back to Imported Mesh (short `← Construction` / `← Imported Mesh`) | `back_to_construction` (`:275`) | `onBackToConstruction` (`:5340`) → `enterConstructionMode` | `sculpting` (`:883`); description always the full wording | — | MOD-035, MOD-051, UI-042 | 15 classes; SculptUndoTest#e2eScundo05 (FC2-D2) |
| Finish Sketch | `finish_sketch` (`:296`) | `onFinishSketchRequested` (`:4506`) → `sketchFinish` | `sketchState == SKETCH_EDITING` (`:900`); withdrawn over a sketch-only (Keep Sketch) edit (`showSketchOnly`) | native: `OpenProfile`, `AmbiguousProfile` is not a Finish refusal (Finish auto-selects only one region, CAD-042); open chain refused by name (CAD-026) | CAD-042, CAD-026 | 22 classes; SketchUxTest#cadUxR1_24 (FC2-D2) |
| Extrude | `extrude_sketch` (`:309`) | `onExtrudeRequested` (`:4781`) → `sketchCommit` (or first project: `commitFirstCadProject` path) | `sketchState == SKETCH_READY && extrudeReady && !revolving` (`:972`) — **withdrawn while the candidate is invalid** | native refuses an invalid commit all the same (`AmbiguousProfile`, `AddDisjoint`, `AddNoEffect`, `CutRemovesBody`, `CutNoIntersection`, CAD-061) | CAD-060, CAD-066, UI-082 | 11 classes; CadVerticalSliceTest#operation_refusal (FC2-D2); CadFillPickR2Test#devR2_05 (C2) |
| Revolve | `revolve_sketch` (`:322`) | same `onExtrudeRequested` path; commit of the Revolve candidate | `SKETCH_READY && extrudeReady && revolving` (`:973`) | `RevolveProfileCrossesAxis`, `RevolveAxisUnresolved`, `RevolveAngleInvalid` … (CAD-078..081) | CAD-077..083, CAD-111 | CadRevolveOwnerTest#devRev01/06 (C2) |
| Back to Home | `back_to_home` (`:343`) | `onBackToHomeRequested` (`:2576`) → `closeProject` (bootstrap: sketch discarded) | only in the CAD/Surface bootstrap (`:683`) | — | APP-014, APP-061 | HomeFlowTest#e2eAppH1_11 (C2) |
| Export | `export_action` (`:394`) | `onExportGlbRequested` (`EditorWorkspaceView.java:5749`) → `exportGlb` bytes → `ACTION_CREATE_DOCUMENT` | over every open project; withdrawn in the bootstrap (`:684`) | `SingularTransform` / `MirroredTransform`; cancel writes nothing; a picker result after recreation writes nothing and is logged as cancel (A-06, APP-043 DEFECT-CONFIRMED, panel-verified 3/3, P3). **A face-supported CAD body is exported at its stored transform, not where it is drawn (E-01 = B-02, DOC-067/CON-025 DEFECT-CONFIRMED, panel-verified 3/3, P1)** | APP-046, APP-047, APP-048 (DOC-DIVERGENCE, D-06) | GlbExportTest (OLD); ImportedMeshSculptTest#imp01b14 (FC2-D2) |
| Display | `display_settings_button` (`:425`) | opens Display popover (§10) | editor phases | — | UI-061 | EditorWorkspaceDisplayTest (OLD); 12 classes |
| Hide UI | `hide_ui_toggle` (`:436`) | `onChromeHideRequested` → `setChromeHidden(true)` (UI state only) | editor | — | UI-039 | EditorWorkspaceControlsTest (OLD) |
| Show UI chip | `restore_ui_chip` (`EditorWorkspaceView.java:813`) | `setChromeHidden(false)` | only while chrome is hidden | — | UI-039 | EditorWorkspaceControlsTest (OLD) |
| Status line | `status_message` (`:446`) | none (verdicts, standing faults) | when a message is posted | carries some always-true instructions (F-12, UI-041 DOC-DIVERGENCE); overlapped by the brush column and Tool Rail in Sculpt (F-06, UI-040 DEFECT-CONFIRMED seat R2, P3, E1 capture `ui3d11_01_sculpt.png`); not announced to TalkBack (F-07, UI-074 BLOCKED-OWNER) | UI-040, UI-041 | 11 classes incl. CadMultiFaceOwnerTest#devMf05 (C2) |

## 4. Project drawer (`ProjectActionsPopoverView`, rows from `ProjectDrawerPolicy.ROWS`)

Every row goes through `ProjectDrawerPolicy.perform`: the drawer is closed
first, then the one existing handler runs. Tapping the mark again or System
Back closes it (`APP-024`, `UI-007`).

| Group / row | id (def) | Act → native | Drawn / absent | Refusal | Req | Tests |
| --- | --- | --- | --- | --- | --- | --- |
| Drawer, header, scroll | `project_actions_popover` (`:87`), `project_drawer_header` (`:101`), `project_drawer_scroll` (`:121`) | grows from the mark's leading top corner | while open | — | UI-006 | OwnerProjectShellSketchSupportTest#devOss01 (C2) |
| Create · New Sketch | `project_new_sketch` (`ProjectDrawerPolicy.java:72`) | `onNewSketchSpatial` (`EditorWorkspaceView.java:3902`) → `supportChooserBegin` | `newSketchShown = projectOpen && !sculpting && !sketching && !chooserActive` | native refuses a begin in Sculpt (MOD-047) | APP-023, CAD-003 | OwnerProjectShellSketchSupportTest#devOss02/03; CadRetainedSketchOwnerTest (C2) |
| Create · New Project… | `project_new` (`:73`) | unsaved guard → `closeProject` → New Project page | always in the drawer | unsaved question when dirty | APP-010, APP-033 | HomeFlowTest#e2eAppH1_08/09 (C2) |
| Project · Save Project | `project_save` (`:74`) | `saveProjectToSlot` (`:5458`) → `encodeProject` → `ProjectSlot.write` (temp + fsync + rename) | always | failure reported, project stays live (APP-028); rename-fallback leaves a `.pending` only copy nothing reads (J-04, ADV-013 DEFECT-CONFIRMED seat R2, P3) | APP-027 | EditorWorkspaceProjectActionsTest#e2er1a01 (C2); ImportedMeshDurableTest#e2eImp01a06 (FC2-D3) |
| Project · Open Saved Project | `project_open` (`:75`) | unsaved guard → `openSavedProjectSlot` (`:5483`) → `loadProject` | always; reads "No saved project yet" and disabled with no slot (APP-029) | refused load changes nothing (APP-031; E2, panel: overstated); **load does not end a Freeform edit session (D-03 = E-07, MOD-083/APP-087/DOC-052 DEFECT-CONFIRMED seat R2, P3)**; **JNI load wrapper mutates sketch/view/gesture state before a post-decode refusal (E-12, DOC-087 DEFECT-CONFIRMED seat R2, P3)** | APP-030 | EditorWorkspaceProjectActionsTest#e2er1a04..06 (C2); ImportedMeshDurableTest#e2eImp01a06 (FC2-D3) |
| Transfer · Save Copy… | `project_save_copy` (`:76`) | `onSaveCopyRequested` (`:5709`): `encodeProject` BEFORE the picker → `ACTION_CREATE_DOCUMENT` → write truncating | always | cancel no-op; recreation while picker open writes nothing silently (A-06, APP-043) | APP-042, APP-045 | ProjectTransferTest#fsr1b10_* (FC2-D3) |
| Transfer · Open File… | `project_open_file` (`:78`) | unsaved guard → `ACTION_OPEN_DOCUMENT` → `onOpenProjectDocumentChosen` (`:6077`) → `loadProject`; never writes the slot | always | as Open Saved Project | APP-044, APP-045 | ProjectTransferTest#fsr1b11_*, #fsr1b12_* (FC2-D3); HomeFlowTest#e2eAppH1_10 (C2) |
| Transfer · Share Diagnostics… | `project_share_diagnostics` (`:80`) | bounded local report → `ACTION_CREATE_DOCUMENT` | always | — | APP-071 | DiagnosticsAndRendererLossTest#fsr1b15_*, #e2er1b08_* (FC2-D3) |
| Import · Import GLB… | `import_glb` (`:82`) | picker → `applyImportedGlbBytes` (`:5877`) → `importGlbDurable` (ONE Undo for all objects) | `showImportAvailable(!sculpting)` (`EditorWorkspaceView.java:2823`) | Sculpt: absent, late result refused, native refuses (APP-025). **While a sketch or the support chooser is open the row is drawn and the import is accepted, switching the active body and recording a step (A-04 = C-02 = D-02 = E-03; APP-026/CAD-099/MOD-049/DOC-072/ADV-046 DEFECT-CONFIRMED, panel-verified 3/3, P2, E1 host diag `diag_import_during_sketch`)** | APP-049, APP-050 | ImportedMeshDurableTest (FC2-D3); ImportGlbSculptGuardTest#d1_02..04 (FC2-D2) |
| Application · Settings… | `project_settings` (`:83`) | opens Settings page | always | — | APP-064 | SettingsPreferencesTest#uiprefr1_02 (FC2-D3) |

No OBJ, FBX or other interchange row exists (`APP-052`;
ProjectTransferTest#fsr1b18, FC2-D3).

## 5. Objects capsule, popover and rows

| Control | id (def) | Act → native | Drawn / absent | Refusal | Req | Tests |
| --- | --- | --- | --- | --- | --- | --- |
| Objects dock / capsule | `objects_dock` (`EditorWorkspaceView.java:677`), `objects_capsule` (`ObjectsCapsuleView.java:59`) | resting scene control | project open (not in bootstrap) | — | UI-034 | HomeFlowTest (C2); EditorWorkspaceRightHostPlacementTest (OLD) |
| Active body name | `objects_capsule_active` (`:75`) | opens Objects popover | as capsule | — | UI-034 | EditorWorkspaceLegibilityTest (FC2-D1) |
| + Add Primitive | `objects_capsule_add` (`:99`); `add_body` in the popover (`ObjectsSectionView.java:122`) | opens Add Primitive palette (§7) | `showCreationAvailable(projectOpen && !sculpting && !sketching)` (`EditorWorkspaceView.java:2879-2880`) | native `sceneAddBody` refuses `in_sketch` (jni 3246-3265) | CON-001, MOD-038 | EditorWorkspaceMobileTest (OLD) |
| Objects popover / section / list | `objects_popover` (`ObjectsPopoverView.java:47`), `objects_section` (`ObjectsSectionView.java:103`), `objects_list` (`:110`) | lists `sceneBodyIds` | from capsule | — | CON-079 | Ui3dStateAuditTest#ui3d06 (FC2-D1); `objects_list` referenced by no test |
| Row (select) | `object_row` (`:178`) | `onBodySelected` (`:397`) → `sceneSelectBody` | one per body | refused in Sculpt and in a sketch (CON-077) | CON-076, CON-077 | EditorWorkspaceObjectsTest (OLD); `object_row` referenced by no test by id |
| Delete | `object_row_delete` (`:221`) | `onBodyDeleted` (`:421`) → `sceneDeleteBody` (ONE Undo) | `showDeletionAvailable(!sculpting && !sketching)` (`:2891`) and not on the only row | `RefusedLastBody`, `RefusedHasDependents`, refused in Sculpt/sketch (CON-044..046, CAD-009) | CON-042..047 | ObjectsDeleteTest (FC2-D3) |
| More (overflow) | `object_row_more` (`:199`) | grows the row command strip inline | `showObjectCommandsAvailable(!sculpting && !sketching)` (`:2897`) | — | CON-038 | ObjectCommandsTest, MirrorSmokeTest (FC2-D3) |
| Command strip | `object_row_commands` (`:503`) | container (two lines) | after More | Back closes strip, then panel (UI-036) | UI-036 | Ui3dStateAuditTest (FC2-D1) |
| Rename | `object_row_rename` (`:512`), `object_rename_field` (`:666`), `object_rename_commit` (`:696`) | `onRenamed` (`:818`) → `sceneRenameBody` (UTF-16 via `utf16ToUtf8`) | in strip | empty / unstorable refused, same name records nothing (CON-033, CON-039) | CON-033 | ObjectCommandsTest#e2eObj018a01 (FC2-D3) |
| Hide / Show | `object_row_visibility` (`:527`) | `onVisibilityToggled` (`:733`) → `sceneSetBodyVisible` | in strip | — (hiding the active body keeps selection; gizmo stays over a hidden active body, B-06 owner-pending, CON-032) | CON-026, CON-027 | ObjectCommandsTest#e2eObj018a01 (FC2-D3); Stage027SculptWorkflowTest (FC2-D2) |
| Lock / Unlock | `object_row_lock` (`:540`) | `onLockToggled` (`:748`) → `sceneSetBodyLocked` | in strip | gizmo refused at activation `FORGESHAPE_GIZMO_REFUSED:body_locked`; typed write `APPLY_REJECTED_LOCKED`; a selection change / Undo can leave a live gizmo over a locked body until Java refreshes (B-04, CON-030 PARTIAL seat R2, P3) | CON-028..030 | ObjectCommandsTest#e2eObj018a01 (FC2-D3) |
| Duplicate | `object_row_duplicate` (`:553`) | `onDuplicated` (`:763`) → `sceneDuplicateBody` | in strip | `RefusedFaceSupportedCad`; derived name truncated mid-suffix at 96 bytes (B-05, CON-036 DEFECT-CONFIRMED seat R2, P3) | CON-035..037, MOD-057, MOD-089, MOD-110 | ObjectCommandsTest (FC2-D3) |
| Mirror | `object_row_mirror` (`:566`) | opens mirror plane chooser | only when `sceneBodyCanMirror(id)` (`:564`): Construction body with no Frozen Sculpt Mesh | Imported, CAD, Freeform, Surface, sculpt-carrying refused by name (CON-050, MOD-058) | CON-048..052 | MirrorSmokeTest (FC2-D3) |
| Mirror plane chooser XY / XZ / YZ | `object_mirror_planes` (`:611`), `object_mirror_plane_xy/xz/yz` (`:617-618`) | `onMirrored` (`:796`) → `sceneMirrorBody` (ONE Undo, fresh id) | after Mirror | opening creates nothing; Back closes with no mutation (CON-052 Back IMPLEMENTED-UNTESTED) | CON-049, CON-051 | MirrorSmokeTest#e2eMirror0101 (FC2-D3) |

## 6. History capsule, Sculpt History navigator, body History

| Control | id (def) | Act → native | Drawn / absent | Refusal | Req | Tests |
| --- | --- | --- | --- | --- | --- | --- |
| History capsule | `history_group` (`EditorWorkspaceView.java:750`) | container | editor | — | UI-069 | EditorWorkspaceLegibilityTest (FC2-D1) |
| Undo / Redo | `undo_action` (`:751`), `redo_action` (`:760`) | `onHistoryStepRequested` (`:3591`) → `historyUndo` / `historyRedo`; native routes to `ConstructionHistory` or the active body's `SculptHistory` by product mode; enabled = `historyUndoAvailable/RedoAvailable` | drawn in both modes | Construction Undo refused in a sketch (CON-068). **Sculpt Undo after a NotRetained (> 1 MiB) stroke lands on a state that never existed and clears `hasEdits` (D-01, MOD-021 DEFECT-CONFIRMED, panel-verified 3/3, P2, E1 `diag_d3`)** | CON-063..070, MOD-018..027 | EditorWorkspaceHistoryTest (OLD); SculptUndoTest (FC2-D2); FreeformOwnerTest, SurfaceOwnerTest (FC2-D2) |
| Sculpt History (navigator toggle) | `history_navigator_action` (`:775`) | opens navigator | Sculpt only (`refreshHistoryNavigator` `:3502`) | — | MOD-031, UI-063 | SculptHistoryNavigatorTest#e2eSchnav01 (FC2-D2) |
| Navigator / list / row | `history_navigator` (`SculptHistoryNavigatorView.java:79`), `history_navigator_list` (`:104`), `history_navigator_row` (`:169`) | `onSculptHistoryStateChosen` (`:3560`) → `sculptJumpToHistoryCursor` (= repeated `undoStroke`/`redoStroke`) | while open; five rows visible, scrolls | `NothingToDo` on the current row, `OutOfRange` never clamped (MOD-030) | MOD-028..030 | SculptHistoryNavigatorTest#e2eSchnav02..09 (FC2-D2); list/row ids referenced by no test |
| History (body) | `feature_history_action` (`:790`) | opens the History surface | `FeatureHistoryPresentation.controlShown = projectOpen && !sketching && !sculpting && bodyHasHistory` (CAD or Surface body) — **drawn over a hidden body too (J-01, BLOCKED-OWNER OD-J1)** | — | CAD-093, UI-030 | CadParametricHistoryOwnerTest (C2); SurfaceOwnerTest (FC2-D2) |
| History surface, segments Steps / Sketches / Features | `feature_history` (`FeatureHistoryView.java:88`), `feature_history_segments` (`:99`), `feature_history_tab_steps/sketches/features` (`:101-108`), `feature_history_list` (`:133`) | rows from `cadTimeline` / `surfaceTimeline` (`:3175`) and `cadSketchInventory` (`:3257`) | while open; segments for a CAD body | — | CAD-087, CAD-094 | CadRetainedSketchOwnerTest#d01, #d17 (C2) |
| Row | `feature_history_row` (`:388`), `sketch_browser_row` (`:337`) | `onFeatureHistoryRowChosen` (`:3383`) → `sketchBeginEditFeature`; Surface rows open the staged value edit (§17) | per sketch / feature, by durable id | `sketchBeginEdit*` do not check visibility (J-01) | CAD-094, MOD-104 | CadParametricHistoryOwnerTest#devPar01 (C2) |
| Row acts | `feature_history_actions` (`:123`), `feature_history_detail` (`:128`), `history_act_edit_sketch`, `history_act_edit_feature`, `history_act_reuse_add`, `history_act_reuse_cut`, `history_act_delete_sketch` (`SketchBrowserText.java:144-148`) | `onHistoryAct` (`:3284`) → `sketchBeginEditSketch` / `sketchBeginEditFeature` / `sketchBeginReuseSketch` / `cadDeleteSketch` | each only when native says the act is available (`SketchBrowserPresentation`) | `SketchInUse`, `SketchIsRoot`, `SketchSupportInvalid` (CAD-089, CAD-091) | CAD-088..092 | CadRetainedSketchOwnerTest#d05, #d06 (C2) |
| Regeneration issue card | `regeneration_issue` (`RegenerationIssueView.java:52`), `_title` (`:63`), `_reason` (`:66`), `_rows` (`:70`) | read-only: first failing feature by id | `issueShown = editSession && downstreamFailure && !dismissed` | — | CAD-094 | CadParametricHistoryOwnerTest#devPar02 (C2) |
| Fix / Cancel edit | `regeneration_issue_fix` (`:83`), `regeneration_issue_cancel` (`:91`) | Fix: back to sketch editing, keeps the edit; Cancel: `sketchCancel`, writes nothing | with the card | — | CAD-094 | CadParametricHistoryOwnerTest#devPar02 (C2) |

## 7. Add Primitive palette and the sketch support chooser (`AddPrimitivePaletteView`)

| Control | id (def) | Act → native | Drawn / absent | Refusal | Req | Tests |
| --- | --- | --- | --- | --- | --- | --- |
| Palette | `add_primitive_palette` (`:128`) | anchored to the + that opened it | creation available | Back closes (uilr106) | UI-038 | Ui3dStateAuditTest (FC2-D1) |
| Box, Cylinder, Sphere, Cone, Capsule, Plane | `add_primitive_box/cylinder/sphere/cone/capsule/plane` (`:86-88`) | `onAddPrimitiveChosen` (`EditorWorkspaceView.java:1597`) → `beginConstructionEdit` → `sceneAddBody` → `commitConstructionEdit` (ONE Undo) | shapes section | `sceneAddBody` refuses in a sketch / Sculpt | CON-001, CON-006 | EditorWorkspaceHistoryTest#s01909/s01910 (OLD) |
| Freeform Box / Plane / Cylinder | `add_freeform_box/plane/cylinder` (`:102`) | `onAddFreeformChosen` (`:1560`) → `freeformCreateBody(form)` | shapes section | status `freeformStatusToken` | MOD-068 | OwnerProjectShellSketchSupportTest#devOss05 (box only, C2); plane/cylinder referenced by no test |
| New Sketch | `add_sketch` (`:196`) | `onNewSketchSpatial` (`:3902`) → `supportChooserBegin` | shapes section | refused in Sculpt | CAD-003, UI-032 | HomeFlowTest (C2) |
| Surface | `add_surface` (`:206`) | `onAddSurfaceChosen` → `beginSurfaceSketch(NO_OBJECT)` → `surfaceBeginSketch` | shapes section | — | MOD-102 | OwnerProjectShellSketchSupportTest#devOss05 (C2) |
| Support chooser (spatial) | `sketch_support_spatial` (`:241`) | viewport: first tap aims (highlight), second tap on the same target confirms (`supportChooserBegin` …); nearest target owns a tap; a stylus hover highlights only (`ForgeShapeSurfaceView.onHoverEvent` `:143` → `supportChooserHover`) | while the chooser is active | a surface that cannot be a support chooses nothing (Freeform, Surface, curved side, Revolve face); interrupted choice refused, never retargeted (CAD-004, CAD-011, MOD-064) | CAD-003..005, CAD-011, UI-033 | CadRetainedSketchOwnerTest#d04, #d10, #d15, #d16, #d19..d22 (C2); SpatialSketchTest#stylusHoverHighlightsATargetWithoutCommitting (OLD; emulator, not hardware hover) |
| Choose a plane by name / XY / XZ / YZ / Back to shapes | `sketch_plane_by_name` (`:218`), `sketch_plane_chooser` (`:232`), `sketch_plane_xy/xz/yz` (`:113`), `sketch_plane_back` (`:269`) | `onNewSketchChosen` (`:3862`) → `sketchBegin(plane)` | accessibility fallback, under the spatial chooser | — | CAD-003 | SpatialSketchTest#byNamePlaneListRemainsTheAccessibilityFallback (OLD); 14 classes use `sketch_plane_by_name` |

## 8. Trailing host and Tool Rail (Construction)

`WorkspaceTrailingHostView`; the rail's entries depend on mode
(`ensureEntries`, `:532-628`).

| Control | id (def) | Act → native | Drawn / absent | Refusal | Req | Tests |
| --- | --- | --- | --- | --- | --- | --- |
| Host / rail / scroll | `workspace_trailing_host` (`:174`), `tool_rail_scroll` (`:180`), `tool_rail` (`ToolRailView.java:97`) | containers | withdrawn in the bootstrap before a sketch begins (`renderTrailingHost` `:3106`) and while Display suspends it | — | UI-039, UI-058 | EditorWorkspaceLegibilityTest (FC2-D1) |
| Shape | `tool_rail_shape` (`:618`) | `onToolSelected` (`:5148`) → opens Shape context (precision surface §9) / Freeform cage editing for a Freeform body (`refreshFreeformEdit` `:6354` → `freeformBeginEdit`) | not for an Imported Mesh (`shapeOffered = !sceneActiveBodyIsImported()`) | native refuses shape Apply on an imported body (CON-007) | CON-007, MOD-090 | ImportedMeshDurableTest#imp01a14 (FC2-D3) |
| Transform | `tool_rail_place` (`:605`) | `uiState` tool → gizmo via `refreshTransformGizmo` (`:3007`) → `setGizmoActive` | Construction, every representation | gizmo not offered over a locked body (`transformOffered` `:3046`). **Offered over a face-supported CAD body whose placement is derived, so a typed/dragged write records a step and changes nothing visible (B-01, CON-024/TST-076 DEFECT-CONFIRMED, panel-verified 3/3, P2)** | CON-008..023 | EditorWorkspaceGizmoTest (OLD); EditorWorkspaceLegibilityTest#uilr113/114 (FC2-D1) |
| Move / Rotate / Scale | `transform_mode_group` (`:198`), `transform_mode_move/rotate/scale` (`:199-208`) | `onTransformModeRequested` (`:3118`) → `setGizmoMode` (no history, no publish) | `transformOffered` | — | CON-015, CON-021 | EditorWorkspaceLegibilityTest#uilr101/103 (FC2-D1) |
| World / Local | `transform_space_group` (`:214`), `transform_space_world/local` (`:215-219`) | `onTransformSpaceRequested` (`:3130`) → `setGizmoSpace` | `transformOffered && GIZMO_SPACE_SELECTABLE` (absent in Scale) | World in Scale refused (CON-015) | CON-015 | EditorWorkspaceLegibilityTest#uilr109 (FC2-D1) |
| Dimensions / Relative scale | `body_size_group` (`:241`), `body_dimensions` (`:242`), `body_relative_scale` (`:246`) | `onBodyDimensionsRequested` (`:4992`) / `onRelativeScaleRequested` (`:5021`) → `setBodyDimensionsMode` | `transformOffered && BODY_DIM_SUPPORTED` (Construction body only) | locked, hidden, Imported, CAD refused (CON-058); entering Dimensions withdraws the gizmo below JNI (CON-059) | CON-054..060 | BodyDimensionsSmokeTest#e2eDim020m01 (FC2-D3) |
| Anchor −/centre/+ | `dimension_anchor_group` (`:258`), `dimension_anchor_negative/center/positive` (`:259-270`) | `onDimensionAnchorRequested` (`:5046`) → `setBodyDimensionAnchor` | with Dimensions open | — | CON-055 | BodyDimensionsSmokeTest (FC2-D3); centre/positive referenced by no test by id |
| Precision toggle | `precision_group` (`:312`), `precision_toggle` (`:313`) | opens/closes the precision surface (§9) | every editor phase with the host | — | UI-015 | EditorWorkspaceLegibilityTest#uilr101/103 (FC2-D1) |
| Transform selector row | `transform_selector_row` (`:226`) | container | `transformOffered` | — | — | EditorWorkspaceUnifiedRightHostTest (OLD) |

## 9. Precision surface (`PropertyInspectorView`) and its bodies

| Control | id (def) | Act → native | Drawn / absent | Refusal | Req | Tests |
| --- | --- | --- | --- | --- | --- | --- |
| Inspector, title, scroll, body, footer, close | `property_inspector` (`:90`), `inspector_title` (`:112`), `inspector_toggle` (`:117`), `inspector_scroll` (`:128`), `inspector_body` (`:151`), `inspector_footer` (`:159`) | containers; close | opened from a control, absent otherwise; bottom sheet in compact windows | Back closes it via its own exit (uilr116); **in the CAD bootstrap Back discards the sketch instead (F-03, UI-009 DEFECT-CONFIRMED, panel-verified 3/3, P2)** | UI-039, UI-047 | EditorWorkspaceLegibilityTest#uilr107..110, #uilr116 (FC2-D1) |
| Shape: primitive chooser and rows | `primitive_chooser` (`ConstructionShapeEditorView.java:153`), `primitive_option_*` (`:40-42`), `primitive_row_*` (`:87-108`) | draft only; selecting a kind publishes nothing (CON-005) | Shape tool, Construction body | — | CON-005 | EditorWorkspaceControlsTest (OLD) |
| Shape fields | `field_box_width/height/depth`, `field_cylinder_diameter/height`, `field_sphere_diameter`, `field_cone_diameter/height`, `field_capsule_diameter/total_height`, `field_plane_width/depth` (`:89-109`) | draft values (comma accepted, `NumericPropertyRow.java:109`) | per kind | non-finite, ≤ 0, `RelationInvalid` (capsule height < diameter) refused, never clamped (CON-002, CON-003) | CON-002, CON-003 | EditorWorkspaceControlsTest (OLD); HOST CONSTRUCTION_* suites |
| Apply Shape | `apply_shape` (`:135`) | `submitDraftShape` (`:396`) → `applyConstruction{Box,Cylinder,Sphere,Cone,Capsule,Plane}` (ONE Undo) | Shape tool | refused values change nothing (EditorWorkspaceHistoryTest#s01905) | CON-002..006 | EditorWorkspaceHistoryTest#s01902 (OLD) |
| Placement fields Pos/Rot/Scale X/Y/Z | `field_pos_x..z`, `field_rot_x..z`, `field_scale_x..z` (`ConstructionPlacementEditorView.java:33-42`) | draft | Transform tool, precision open | scale ≤ 1e-6 / negative / non-finite refused (CON-014) | CON-014, CON-023 | EditorWorkspaceGizmoTest#s02025/s02026 (OLD) |
| Apply Transform | `apply_transform` (`:120`) | `onApplyTransform` (`:245`) → `applyBoxTransform` (ONE atomic step; identical values Unchanged) | as fields | `APPLY_REJECTED_LOCKED`; `NoProject` (CON-074); **no face-supported guard (B-01)** | CON-023, CON-029 | EditorWorkspaceLegibilityTest#uilr110 (FC2-D1); EditorWorkspaceGizmoTest (OLD) |
| Unit chips mm / cm / m | `unit_chips` (`UnitChipsView.java:36`), `unit_chip_mm/cm/m` (`:29`) | presentation only (CON-086) | with Position | — | CON-086, UI-052 | EditorWorkspaceControlsTest (OLD) |
| Relative Scale fields + Apply | `relative_scale_editor` (`RelativeScaleEditorView.java:58`), `field_relative_scale_x/y/z` (`:40-41`), `apply_relative_scale` (`:83`) | `onApply` (`:139`) → `applyBodyRelativeScale` (opens at 1/1/1 every time) | Relative Scale mode | identity is Unchanged; refusals as Dimensions (CON-057, CON-058) | CON-057 | BodyDimensionsSmokeTest (FC2-D3); `apply_relative_scale` referenced by no test by id |
| CAD body editor (legacy exact editor) | `cad_editor` (`CadFeatureEditorView.java:72`), `cad_plane_summary` (`:78`), `cad_profile_summary` (`:80`), `cad_row_rectangle/circle` (`:83-86`), `field_cad_rect_width/height` (`:84`), `field_cad_circle_radius` (`:87`), `field_cad_depth` (`:94`), `cad_direction_along/against` (`:99-101`), `apply_cad` (`:156`) | `onApply` (`:330`) → `cadApplyRectangle` / `cadApplyCircle` / `cadApplyExtrude` (one step each) | Shape tool over a CAD body | `applyState` validates the whole state; no closed profile refused by name (CAD-100) | CAD-097, CAD-100 | SketchExtrudeTest#e2eCadr0_09and10 (OLD); CadVerticalSliceTest (FC2-D2) |
| Edit Sketch (inspector) | `edit_cad_sketch` (`:127`) | `onEditCadSketchRequested` (`:4851`) → `sketchBeginEdit` (staged copy) | CAD body, not sketching | not refused over a hidden body (J-01) | CAD-069, CAD-070 | SketchUxTest#cadUxR1_32_33_34_35 (FC2-D2) |
| Feature list / row | `cad_feature_list` (`:142`), `cad_feature_row` (`:284`) | `onEditCadFeatureRequested` (`:4874`) → `sketchBeginEditFeature` | CAD body with later features | — | CAD-068 | CadVerticalSliceTest#feature_edit_roundtrip (FC2-D2); CadRevolveOwnerTest (C2) |
| Sculpt context: summary, Isolate | `sculpt_mesh_summary` (`SculptContextView.java:72`), `sculpt_isolate` (`:87`) | `onIsolateToggled` (`:289`) → `setSculptIsolate` (view restriction, never truth) | Isolate only in Sculpt (`:166`) | — | MOD-043, UI-062 | Stage027SculptWorkflowTest#isolate_* (FC2-D2) |
| Sculpt context: mask summary, Clear Mask | `sculpt_mask_summary` (`:107`), `clear_mask` (`:111`) | `onClearMaskRequested` (`:295`) → `sculptClearMask` (ONE sculpt-history entry) | `sculptCanClearMask` (`:179-180`) — that predicate omits the size test | over the 1 MiB entry cap refused `EntryTooLarge` (deliberate), but the control is drawn and the shown text gives no reason (D-05, MOD-014 PARTIAL seat R2, P3) | MOD-012..014 | SculptBrushStage025Test#e2eFcm04and05 (FC2-D2) |
| Stale-source warning, Reset Sculpt from Shape… / from Imported Mesh… | `stale_source_warning` (`:128`), `freeze_again` (`:137`) | `onResetSculptRequested` (`:241`): asks first exactly when `SCULPT_HAS_EDITS` → `rebuildSculptMeshFromShape` (`:307`) → `freezeToSculpt` | warning when `SCULPT_SOURCE_STALE`; Reset in Sculpt context | after D-01 `hasEdits` can be false over edited geometry, so Reset discards without asking | MOD-036, MOD-037, MOD-052 | ImportedMeshSculptTest#imp01b09 (FC2-D2); EditorWorkspaceControlsTest (OLD) |
| Sketch editor (selected entity, profiles, depth, operation) | §12.6 | | | | | |

## 10. Display popover (`DisplaySettingsPopoverView`)

All presentation; session-only, native-owned (`DisplaySettingsStore`); no
truth, no dirty flag. Read back by `refreshDisplaySettings` (`:6312`).

| Control | id (def) | Act → native | Drawn / absent | Refusal | Req | Tests |
| --- | --- | --- | --- | --- | --- | --- |
| Popover | `display_settings_popover` (`:76`) | — | from Display | Back closes | UI-061 | EditorWorkspaceDisplayTest (OLD) |
| Studio / MatCap / Debug | `display_mode_studio/matcap/debug` (`:99-128`) | `onShadingModelRequested` (`:6239`) → `setShadingModel` | always in popover | — | RND rows | EditorWorkspaceDisplayTest (OLD); `display_mode_debug` referenced by no test |
| Smooth / Faceted | `surface_shading_smooth/faceted` (`:150-160`) | `setSurfaceShading` | always | — | — | EditorWorkspaceDisplayTest (OLD) |
| Perspective / Orthographic | `projection_perspective/orthographic` (`:183-193`) | `setProjectionMode` | always | — | — | EditorWorkspaceDisplayTest (OLD); CadRetainedSketchOwnerTest#d19 (C2) |
| Grid On / Off | `view_grid_on/off` (`:229-244`) | `setGridVisible` | always | chips built like the outline chips (width not measured; F-01) | — | EditorWorkspaceDisplayTest (OLD) |
| Selection Outline On / Off | `view_selection_outline_on/off` (`:277-292`) | `setSelectionOutlineVisible` | always | **hit area 37.0 / 39.2 dp wide, below the 48 dp floor (F-01, UI-013 DEFECT-CONFIRMED, panel-verified 3/3, P2, E1 measured)** | CON-083, UI-061 | SelectionOutlineTest (OLD); SelectionOutlineVisualEvidenceTest (FC2-D1) |

## 11. Viewport affordances without a semantic id (`ForgeShapeSurfaceView`)

The SurfaceView (`viewport_surface`, `ForgeShapeActivity.java:50`) owns the
whole window (`UI-045`). `onTouchEvent` (`:58`) → `NativeViewport.touchEvent`;
input crosses JNI as platform-neutral samples (`forgeshape_input.h`).

| Gesture | Mode | Native meaning | Refusal / guard | Req | Tests |
| --- | --- | --- | --- | --- | --- |
| One-finger drag off any handle | Construction, Ready | orbit | — | CAD-016, CON-011 | EditorWorkspaceGizmoTest#s02029and30 (OLD) |
| Two-finger pan / pinch | all (incl. sketch drawing) | pan / zoom; a gesture that becomes multi-touch mutates no sculpt vertex | — | MOD-044, UI-078 | SculptUndoTest (FC2-D2) |
| Tap | Construction | pick → `sceneSelectBody` (hidden bodies not pickable; E2 — panel: the single-predicate claim is overstated as reproduced) | in Sculpt refused before the pick (`FORGESHAPE_SCENE_SELECT_REFUSED:in_sculpt_mode:viewport_tap`) | CON-026, CON-076, MOD-040 | SelectionOutlineTest#e2eSeloutr1_02_03 (OLD); Stage027SculptWorkflowTest#guard1 (FC2-D2) |
| Gizmo handle drag | Transform | writes `ConstructionTransform` throughout; one pointer, ONE step; second pointer / cancel restores (E2; panel: overstated as reproduced) | locked refused; face-supported not refused (B-01) | CON-008..013 | EditorWorkspaceGizmoTest (OLD) |
| Dimension labels X/Y/Z + editor | Dimensions mode | `body_dimension_labels` (`BodyDimensionLabelsView.java:91`), `body_dimension_label_x/y/z` (`:55-56`), `body_dimension_editor` (`:109`), `field_body_dimension` (`:118`), `apply_body_dimension` (`:148`) → `onBodyDimensionEntered` (`:5062`) → `applyBodyDimension` | zero / negative / non-finite / degenerate axis refused; **field drops the comma: "2,5" → 25 (F-05, UI-051/ADV-067 DEFECT-CONFIRMED, panel-verified 3/3, P2, E2)** | CON-054..056 | BodyDimensionsSmokeTest (FC2-D3); Ui3dStateCorrectionTest (OLD) |
| Sculpt stroke | Sculpt | one completed stroke = one `SculptHistory` entry | — | MOD-018 | SculptUndoTest (FC2-D2) |
| Sketch draw / select / snap | Sketch Editing | per rail tool; one finger never orbits | — | UI-078, CAD-028 | CadSketchDraftingOwnerTest#devDr04 (C2) |
| Ready region / face tap | Sketch Ready | pure toggle of the cell under the Down pixel, through the camera captured at Down; inside 24 px slop no orbit | `ray_parallel`, `invalid_face`, `selection_cap` (debug token `FORGESHAPE_SKETCH_TAP:*`) | CAD-043, CAD-044 | CadFillPickR2Test, CadMultiFaceOwnerTest (C2) |
| Extrude arrow drag / head tap | Ready | §13 | | | |
| Revolve ring handle drag | Revolve | §14 | | | |
| Freeform cage tap / drag | Freeform cage edit | select vertex/edge/face by id; drag one step | hidden / locked body not cage-editable (MOD-081) | MOD-072, MOD-073 | FreeformOwnerTest#devFf01 (FC2-D2) |
| Stylus hover | support chooser | highlight only | — | UI-033 | SpatialSketchTest (OLD, emulator) — hardware hover OWNER-PHYSICAL-UNVERIFIED (UI-081) |
| Mouse | all | orbit only; no wheel zoom or pan (F-10 / G-09 INFO) | — | UI-079 FUTURE-NOT-DUE | — |

## 12. Sketch mode (CAD sketch, Surface sketch, Edit Sketch)

`SketchChromePolicy`: Tool Rail drawn unless Ready; navigator and Line
dimension only while Editing; Back to Sketch only in Ready; Cancel Sketch
whenever a sketch is active; Finish never opens the precision surface.

### 12.1 Sketch Tool Rail and sketch group

| Control | id (def) | Act → native | Drawn / absent | Refusal | Req | Tests |
| --- | --- | --- | --- | --- | --- | --- |
| Select, Line, Polyline, Rectangle, Circle, Arc, Spline | `tool_rail_select/line/polyline/rectangle/circle/arc/spline` (`WorkspaceTrailingHostView.java:549-566`) | `onToolSelected` (`:5148`) → `sketchSetTool` | `sketching && state != READY` | — | CAD-018..023, UI-065 | SketchUxTest#cadUxR1_40 (FC2-D2); 22 classes use rectangle |
| Cancel Sketch | `sketch_group` (`:289`), `cancel_sketch` (`:290`) | `onCancelSketchRequested` (`:4927`) → `sketchCancel` (bootstrap: also Home) | any active sketch | costs nothing (CAD-098) | CAD-098 | CadRevolveOwnerTest#devRev08 (C2) |
| Back to Sketch | `back_to_sketch` (`:294`) | `onBackToSketchRequested` (`:4949`) → `sketchBackToEditing` | Ready; not over a reuse session | — | CAD-073 | CadRetainedSketchOwnerTest (C2) |
| Keep Sketch | `keep_sketch` (`:302`) | `onKeepSketchRequested` (`:3359`) → `sketchCommitSketch` (ONE Undo, no feature) | `sketchCanKeepSketch()` — a face-of-own-feature sketch | world-plane second root absent and refused `SketchSupportInvalid` (CAD-089) | CAD-088, CAD-089 | CadRetainedSketchOwnerTest#d02, #d18 (C2) |

### 12.2 Orientation navigator (`SketchOrientationNavigatorView`)

| Control | id (def) | Act → native | Drawn / absent | Refusal | Req | Tests |
| --- | --- | --- | --- | --- | --- | --- |
| Navigator, view label | `sketch_orientation_navigator` (`:77`), `sketch_navigator_view_label` (`:90`) | reads `sketchViewState` every refresh | Editing only | — | UI-019, UI-020 | SketchUxTest (FC2-D2) |
| Plane XY / XZ / YZ | `sketch_navigator_plane_xy/xz/yz` (`:66-68`) | `onSketchPlaneChosen` (`:4043`) → `sketchSetSupportPlane` | enabled only while the sketch is EMPTY and world-supported; hidden/disabled otherwise | `SketchNotEmpty`, face-supported `InvalidWorkplane` (CAD-012); chip width ~45 dp computed, not measured (UI-021 UNDETERMINED) | CAD-012 | SketchUxTest#cadUxR1_14_15 (FC2-D2) |
| Flip | `sketch_navigator_flip` (`:118`) | `onSketchViewFlipRequested` (`:4060`) → `sketchSetViewFlipped` | Editing | **changes the label and the stored view state but not the camera (`beginSketchView` frames `frame()`, `viewFrame()` has no production caller) — C-01 = F-02, CAD-013/UI-018 DEFECT-CONFIRMED, panel-verified 3/3, P2, E1 probe P1; hit area 36.2 dp wide (F-01, UI-012, E1)** | CAD-013, CAD-014 | SketchUxTest#cadUxR1_08_09_10 asserts only the stored flag (false-green, F-02) |
| −90° / +90° | `sketch_navigator_rotate_ccw` (`:115`), `sketch_navigator_rotate_cw` (`:126`) | `onSketchViewRotateRequested` (`:4066`) → `sketchRotateView` | Editing | same as Flip: no camera change (C-01) | CAD-013 | SketchUxTest#cadUxR1_11_12_13 asserts only `SKETCH_VIEW_QUARTER_TURNS` |

### 12.3 Selected Line dimension (`SketchDimensionLabelView`)

| Control | id (def) | Act → native | Drawn / absent | Refusal | Req | Tests |
| --- | --- | --- | --- | --- | --- | --- |
| Label, value, editor, field, Apply | `sketch_dimension_label` (`:79`), `sketch_dimension_value` (`:82`), `sketch_dimension_editor` (`:93`), `field_sketch_line_length` (`:102`), `apply_sketch_line_length` (`:133`) | `onSketchLineLengthEntered` (`:4079`) → `sketchApplyLineLength` (P0 fixed, direction kept) | a straight Line selected, Editing; label hidden when its point does not project | zero / negative / > 1e5 / non-finite refused, but negative and over-range are named `ZeroLengthLine` (C-04, CAD-025 PARTIAL seat R2, P3); field drops the comma (F-05); label hit area 43.4 dp wide (F-01, UI-014) | CAD-024..026 | SketchUxTest#cadUxR1_17..22, #cadUxR1_23, #cadUxR1_24 (FC2-D2) |

### 12.4 Sketch actions palette (`SketchModifyView`, "Modify")

Drawn by `SketchDraftingPresentation.modifyEntryShown`; draws only what can
succeed; Trim / Extend are tap modes offered whatever is selected (`CAD-037`).

| Control | id (def) | Act → native | Refusal | Req | Tests |
| --- | --- | --- | --- | --- | --- |
| Modify toggle, palette | `sketch_modify` (`:106`), `sketch_modify_toggle` (`:112`), `sketch_actions_palette` (`:122`) | open/close | **not in Back's dismissal chain: Back in the bootstrap discards the sketch (F-03, UI-024 DEFECT-CONFIRMED, panel-verified 3/3)** | CAD-037, UI-023 | CadSketchDraftingOwnerTest (C2) |
| Select multiple | `sketch_action_select_multiple` (`:56`) | `onSketchMultiSelectToggled` (`:4473`) → `sketchSetMultiSelect` | — | CAD-035 | devDr09 (C2) |
| Dimension + kind chips (length, angle, horizontal, vertical, width, height, radius, diameter, edge length, arc radius, sweep, edge angle) | `sketch_action_dimension` (`:56`), `sketch_dimension_kind_*` (`:65-70`) | `onSketchDimensionKindChosen` (`:4435`) → `sketchAddDimension`; angle: `sketchBeginDimensionAngle` (`:4448`) | `SketchDimensionConflict` (CAD-030) | CAD-029, CAD-030 | devDr02 (C2); horizontal/vertical/edge_length/edge_angle referenced by no test |
| Make Construction / Make Regular | `sketch_action_construction` (`:57`) | `onSketchModifyAction` (`:4338`) → `sketchToggleConstruction` | — | CAD-027 | devDr01 (C2); SurfaceOwnerTest (FC2-D2) |
| Trim / Extend (tap modes) | `sketch_action_trim`, `sketch_action_extend` (`:57`) | `enterModifyMode` (`:4393`) → `sketchSetModifyMode`; tap on stroke | `SketchDimensionDependency`, `SketchDimensionLocked` (CAD-031, CAD-032) | CAD-031, CAD-032 | devDr05..07 (C2) |
| Offset + distance + Confirm / Done | `sketch_action_offset` (`:58`), `field_sketch_offset_distance` (`:204`), `sketch_modify_confirm` (`:232`), `sketch_modify_done` (`:244`), `sketch_modify_mode` (`:166`), `sketch_modify_mode_title` (`:170`) | `onSketchOffsetDistanceEntered` (`:4457`) → `sketchSetOffsetDistance`; `onSketchModifyModeEnd` (`:4402`) → `sketchConfirmOffset` / `sketchConfirmMirror` | `OffsetSplineUnsupported`, `OffsetInvalid…` (CAD-033) | CAD-033 | devDr08 (C2) |
| Mirror (sketch entities) | `sketch_action_mirror` (`:58`) | mirror mode → `sketchConfirmMirror` | axis must be a straight edge | CAD-034 | devDr09 (C2) |
| Delete | `sketch_action_delete` (`:58`) | `sketchDeleteSelected` | — | CAD-035 | referenced by no test by id |
| Dimensions Selected / All / Off | `sketch_dimensions_selected/all/off` (`:149-150`) | `onSketchDimensionVisibilityChosen` (`:4467`) → `sketchSetDimensionVisibility` | — | CAD-036 | devDr03 (C2) |
| Driving toggle | `sketch_dimension_driving` (`:191`) | part of add-dimension | `SketchDimensionConflict` | CAD-029 | devDr02 (C2) |

### 12.5 Persistent dimension labels (`SketchDimensionLabelsView`)

| Control | id (def) | Act → native | Drawn / absent | Refusal | Req | Tests |
| --- | --- | --- | --- | --- | --- | --- |
| Labels, item | `sketch_dimension_labels` (`:104`), `sketch_dimension_label_item` (`:175`) | open value editor | stood off 8 dp from the geometry; overlapping labels hidden by priority; never clamped back | — | CAD-036, UI-022 | devDr13 (C2) |
| Value editor, field, Apply, Delete dimension | `sketch_dimension_value_editor` (`:110`), `field_sketch_dimension_value` (`:119`), `apply_sketch_dimension_value` (`:146`), `sketch_dimension_delete` (`:151`) | `onSketchDimensionValueEntered` (`:4479`) → `sketchApplyDimensionValue`; `onSketchDimensionDeleteRequested` (`:4493`) → `sketchRemoveDimension` | while a label is open | Reference dimension not editable; comma accepted (`:126`) | CAD-029 | devDr02 (C2) |

### 12.6 Sketch editor inside the precision surface (`SketchEditorView`)

| Control | id (def) | Act → native | Drawn / absent | Refusal | Req | Tests |
| --- | --- | --- | --- | --- | --- | --- |
| Editor, entity summary, rows | `sketch_editor` (`:144`), `sketch_entity_summary` (`:156`), `sketch_entity_row_rectangle/circle/line` (`:159-165`) | reads `sketchSelectedEntity` | precision open while sketching | — | CAD-019 | Ui3dStateAuditTest (FC2-D1); rows referenced by no test by id |
| Rectangle W/H, Circle radius, Line endpoints, Apply | `field_sketch_rect_width/height` (`:160`), `field_sketch_circle_radius` (`:163`), `field_sketch_line_x0/y0/x1/y1` (`:170-174`), `apply_sketch_entity` (`:199`) | `onApplyEntity` (`:773`) → `sketchApplyRectangle` / `sketchApplyCircle` / `sketchApplyLine` | entity selected | bounds 1e5 m (CAD-023) | CAD-019 | SpatialSketchTest (OLD); circle/line fields referenced by no test |
| Delete entity | `delete_sketch_entity` (`:185`) | `onDeleteEntity` (`:821`) → `sketchDeleteSelected` | entity selected | — | CAD-035 | SketchExtrudeTest (OLD) |
| Profile chooser / option | `sketch_profile_chooser` (`:215`), `sketch_profile_option` (`:598`) | `onProfileChosen` (`:731`) → `sketchSelectProfile` | Ready, loop regions | `AmbiguousProfile` over an empty selection (CAD-042) | CAD-039..042 | CadVerticalSliceTest (FC2-D2) |
| Select All / Clear areas | `sketch_area_shortcuts` (`:222`), `sketch_select_all_areas` (`:225`), `sketch_clear_areas` (`:233`) | `sketchSelectAllAreas` / `sketchClearAreaSelection` | Ready | `TooManyRegions` past the cap | CAD-049 | CadRetainedSketchOwnerTest#d14 (C2) |
| Operation New Body / Add / Cut | `sketch_operation_row` (`:252`), `sketch_operation_new_body/add/cut` (`:259-263`) | `onExtrudeOperationRequested` (`:4734`) → `sketchSetOperation` | offered as native says: a world-plane sketch offers New Body alone (CAD-059) | **with a full sketch table (16) Add / Cut are still offered and the refusal is misnamed (C-03, CAD-063 DEFECT-CONFIRMED seat R2, P3, E2)** | CAD-059..063 | CadRetainedSketchOwnerTest, OwnerProjectShellSketchSupportTest (C2) |
| Depth, Along / Against, Extrude | `field_extrude_depth` (`:289`), `extrude_direction_along/against` (`:294-296`), `sketch_extrude_commit` (`:383`) | `submitDepth` (`:758`) → `sketchSetExtrude`; commit as toolbar Extrude | Ready; the pinned Extrude stays even while the candidate is invalid (it submits a typed depth first; native refuses) | as Extrude | CAD-055..057, CAD-066 | 12 classes; CadCanvasExtrudeTest (OLD) |
| Revolve section | `sketch_revolve_begin` (`:272`), `sketch_revolve_section` (`:318`), `sketch_revolve_axis_summary` (`:324`), `field_revolve_angle` (`:326`), `sketch_revolve_flip` (`:331`), `sketch_revolve_change_axis` (`:343`), `sketch_revolve_back_to_extrude` (`:355`), `sketch_revolve_commit` (`:369`) | §14 | | | | |

## 13. CAD extrude HUD (`CadExtrudeCanvasView`, `CadExtrudeDockView`)

Anchors, scale and dock geometry are derived natively (`cadExtrudeToolState`
slots 32..58); nothing here is truth. Every touch target ≥ 48 dp and never
`setScale`d (`CadHudPresentationTest`, `CadHud3dPresentationTest`, JVM).

| Control | id (def) | Act → native | Drawn / absent | Refusal | Req | Tests |
| --- | --- | --- | --- | --- | --- | --- |
| Canvas | `cad_extrude_canvas` (`:247`) | container | Ready | — | CAD-072 | Stage027SculptWorkflowTest (FC2-D2, absence) |
| Extrude arrow (no id; overlay) | — | drag: one captured pointer, basis frozen at Down → `setExtrude` (same door as a typed value), millimetre floor; still tap on the drawn HEAD is the arrow's; a still tap on the shaft toggles the cell | Ready with a selection | second pointer / Cancel restores the pre-drag depth and records nothing (CAD-052) | CAD-051..053 | CadFillPickR2Test#devR2_04 (C2); CadCanvasExtrudeTest (OLD) |
| Depth value + editor + field + Apply | `cad_extrude_depth_value` (`:271`), `cad_extrude_depth_editor` (`:363`), `field_cad_extrude_depth` (`:364`), `apply_cad_extrude_depth` (`:376`) | `onExtrudeSideEntered` (`:4110`) → `sketchSetExtrudeSide` | value above its leader at display precision; hidden when its point is off-screen | zero / negative typed refused; **field drops the comma (F-05)** | CAD-055, CAD-075, UI-027, UI-028 | CadExtrudeExtentTest#cadext06b (FC2-D2) |
| Second value (Two Sides) + editor | `cad_extrude_second_value` (`:387`), `cad_extrude_second_editor` (`:397`), `field_cad_extrude_second` (`:398`), `apply_cad_extrude_second` (`:410`) | `sketchSetExtrudeSide(B)` | Two Sides only | as above | CAD-055 | CadExtrudeExtentTest (FC2-D2) |
| Action dock / panel | `cad_extrude_panel` (`:282`) | opens the ONE palette | projected world rectangle; hidden whole at axis sine ≤ 0.35, behind the eye or off-viewport | claims only its quad ∪ 48 dp square (others pass to the viewport) | CAD-054, UI-025 | CadHud3dOwnerTest#devHud3d01..05 (C2) |
| Palette, extent palette, operation palette | `cad_extrude_actions_palette` (`:297`), `cad_extrude_extent_palette` (`:306`), `cad_extrude_operation_palette` (`:325`) | containers | after a dock tap | **not in Back's dismissal chain (F-03, UI-024)** | — | CadHud3dOwnerTest#devHud3d04 (C2); actions/operation palette ids referenced by no test |
| One Side / Symmetric / Two Sides | `cad_extrude_extent_one_side/symmetric/two_sides` (`:103-104`) | `onExtrudeExtentRequested` (`:4145`) → `sketchSetExtrudeExtent` (`extrudeFeatureWithExtent` transition) | palette | non-canonical refused `InvalidExtrudeExtent` (CAD-055) | CAD-055, CAD-056 | CadExtrudeExtentTest#cadext06a (FC2-D2) |
| New Body / Add / Cut | `cad_extrude_operation_new_body/add/cut` (`:107-108`) | `sketchSetOperation` | only operations native offers | — | CAD-059 | CadVerticalSliceTest (FC2-D2); `new_body` referenced by no test |
| Flip | `cad_extrude_flip` (`:346`) | `onExtrudeFlipRequested` (`:4170`) → `sketchFlipExtrudeDirection` | One Side only (`flipPresent`) | refused below JNI in Symmetric / Two Sides (CAD-057) | CAD-057, UI-026 | CadCanvasExtrudeTest#e2eCaduxs1_02 (OLD); CadExtrudeExtentTest (FC2-D2) |
| Edit Sketch chip | `cad_canvas_edit_sketch` (`:422`) | `onEditCadSketchRequested` → `sketchBeginEdit` | committed CAD body, anchor projects; withdrawn while the chooser or the History surface is open | hidden when the anchor does not project | CAD-071, UI-031 | CadRetainedSketchOwnerTest, CadMultiFaceOwnerTest (C2) |

## 14. Revolve (New Body only)

| Control | id (def) | Act → native | Drawn / absent | Refusal | Req | Tests |
| --- | --- | --- | --- | --- | --- | --- |
| Revolve… | `sketch_revolve_begin` (`SketchEditorView.java:272`) | `onRevolveBeginRequested` (`:4619`) → `sketchBeginRevolve` | `CadRevolvePresentation.entryShown`: not active, available, ≥ 1 area selected | — | CAD-077 | CadRevolveOwnerTest#devRev01 (C2) |
| Axis pick (viewport tap on a straight edge) / Change axis | `sketch_revolve_change_axis` (`:343`) | `onRevolveAxisPickRequested` (`:4645`) → `sketchBeginRevolveAxisPick` | while revolving | `RevolveAxisNotStraight`, `RevolveAxisDegenerate`, `RevolveAxisUnresolved`; never re-aimed (CAD-078) | CAD-078 | CadRevolveOwnerTest#devRev05 (C2) |
| Angle field / canvas label + editor | `field_revolve_angle` (`:326`); `cad_revolve_angle_label` (`CadRevolveAngleLabelView.java:57`), `cad_revolve_angle_value` (`:60`), `cad_revolve_angle_editor` (`:71`), `field_revolve_angle_canvas` (`:80`), `apply_revolve_angle` (`:107`) | `onRevolveAngleEntered` (`:4667`) → `sketchSetRevolveAngle` | label at the projected half-angle point; hidden when it does not project | outside [0.001, 360] or non-finite `RevolveAngleInvalid`, never clamped; comma accepted (`:87`) | CAD-079, UI-029 | CadRevolveOwnerTest#devRev02 (C2) |
| Ring handle drag (no id) | — | whole degrees via `setRevolveAngle`; never orbits | — | second pointer / Cancel restores | CAD-082 | CadRevolveOwnerTest#devRev03 (C2) |
| Flip direction | `sketch_revolve_flip` (`:331`) | `onRevolveFlipRequested` (`:4655`) → `sketchFlipRevolve` | while revolving | reverses direction, never the angle's sign | CAD-079 | CadRevolveOwnerTest#devRev04 (C2) |
| Extrude instead | `sketch_revolve_back_to_extrude` (`:355`) | `onRevolveEndRequested` (`:4630`) → `sketchEndRevolve` | `extrudeInsteadShown` | **System Back in this state on API 36 backgrounds the app (F-04, CAD-115/UI-010 DEFECT-CONFIRMED, panel-verified 3/3, P2, E2; API 33-35 UNVERIFIED)** | CAD-115 | CadRevolveOwnerTest presses the control, sends no Back key |
| Revolve (commit) | `sketch_revolve_commit` (`:369`), toolbar `revolve_sketch` | commit | `commitShown`: candidate Ok | `RevolveProfileCrossesAxis`, `RevolveComponentsOverlap` (CAD-081) | CAD-080..083, CAD-111 | CadRevolveOwnerTest#devRev06..08 (C2) |

## 15. Sculpt mode

| Control | id (def) | Act → native | Drawn / absent | Refusal | Req | Tests |
| --- | --- | --- | --- | --- | --- | --- |
| Grab, Clay, Smooth, Flatten, Inflate, Crease, Mask | `tool_rail_grab/clay/smooth/flatten/inflate/crease/mask` (`WorkspaceTrailingHostView.java:585-600`) | `onToolSelected` → `setSculptTool` | Sculpt only | — | MOD-001..008, UI-064 | SculptBrushStage025Test#e2eFcm01 (FC2-D2) |
| Brush column, Radius, Strength | `brush_edge_controls` (`BrushEdgeControlsView.java:65`), `brush_radius_slider` (`:77`), `brush_strength_slider` (`:84`), `brush_radius_value` (`:76`), `brush_strength_value` (`:83`) | radius px [24, 600], strength [0.05, 1] | Sculpt | — (covers the status line, F-06) | MOD-006 | EditorWorkspaceLegibilityTest#uilr120 (FC2-D1) |
| Back to Construction, Sculpt History, Isolate, Clear Mask, Reset | §3, §6, §9 | | | | | |
| Absent in Sculpt (control removed, guard kept) | Add Primitive, New Sketch, Delete, row commands, Import GLB, Construction Undo meaning, Start Sculpting | | | native refusal for each (MOD-038, MOD-039, MOD-047, MOD-048, CON-068) | | ObjectsDeleteTest#imp01b21_deleteIsWithdrawnAndRefusedWhileSculpting (FC2-D3); ImportGlbSculptGuardTest (FC2-D2) |

## 16. Freeform context surface (`FreeformEditorView`)

Drawn when `FreeformPresentation.editWanted = projectOpen && !sculpting &&
!sketching && shapeToolHeld && activeIsFreeform && editable`; tool rows by
`showPushPull`/`showExtrude` (face selection), `showDeleteFaces`
(face selection < all faces), `showInsertLoop` (exactly one edge),
`showCrease` (edges), `showTransformModes` (any selection). Each typed tool
and each drag is ONE Construction step (`MOD-080`). Refusals are shown via
`freeformStatusToken` (`:268-277`).

| Control | id (def) | Act → native | Refusal | Req | Tests |
| --- | --- | --- | --- | --- | --- |
| Editor, counts | `freeform_editor` (`:69`), `freeform_counts` (`:74`) | reads `freeformState` | — | MOD-090 | FreeformOwnerTest (FC2-D2) |
| Vertex / Edge / Face | `freeform_element_vertex/edge/face` (`:49-50`) | `freeformSetElement` (`:87`) | — | MOD-072 | devFf01..03 (FC2-D2) |
| Select multiple | `freeform_multi_select` (`:93`) | `freeformSetMultiSelect` (`:99`) | — | MOD-072 | referenced by no test |
| Move / Rotate / Scale | `freeform_mode_move/rotate/scale` (`:53-54`) | `freeformSetTransformMode` (`:113`) | — | MOD-073 | referenced by no test by id |
| Distance + Push/Pull, Extrude Face | `field_freeform_distance` (`:123`), `freeform_push_pull` (`:128`), `freeform_extrude` (`:137`) | `freeformPushPull` (`:134`), `freeformExtrude` (`:143`) | zero / sub-micron / non-finite refused (MOD-074). **Distance seeded as a unitless literal and read in the session display unit (J-02, ADV-066 DEFECT-CONFIRMED seat R2, P3)** | MOD-074, MOD-075 | devFf02, devFf03 (FC2-D2) |
| Delete Face | `freeform_delete_faces` (`:149`) | `freeformDeleteFaces` (`:154`) | deleting every face refused by name (MOD-077) | MOD-077 | referenced by no test by id |
| Loop ratio + Insert Loop | `field_freeform_ratio` (`:165`), `freeform_insert_loop` (`:169`) | `freeformInsertLoop` (`:175`) | ratio ∉ (0, 1), ambiguous ring refused (MOD-076) | MOD-076 | devFf02 (FC2-D2) |
| Crease weight + Crease | `field_freeform_crease` (`:180`), `freeform_set_crease` (`:184`) | `freeformSetCrease` (`:190`) | outside [0, 1] / NaN refused (MOD-071) | MOD-071 | devFf02 (FC2-D2) |
| Symmetry X / Y / Z | `freeform_symmetry_x/y/z` (`:57-58`) | `freeformSetSymmetry` (`:209`) | ON over an asymmetric cage refused `CageNotSymmetric`, but the chip is drawn (D-08, MOD-079 PARTIAL seat R2, P3) | MOD-078, MOD-079 | devFf03 (X only, FC2-D2) |
| Level 0..4 | `freeform_level_0..4` (`:63-64`) | `freeformSetLevel` (`:226`) | `SubdivisionBudgetExceeded` (> 131072 derived quads) drawn then refused (D-08) | MOD-070 | devFf03 (levels 0 and 3, FC2-D2) |
| Cage tap / handle drag | viewport (§11) | select by id; drag one step | hidden / locked refused | MOD-072, MOD-073, MOD-081 | devFf01 (FC2-D2) |

Not drawn: T-Spline / SubD wording (MOD-087 IMPLEMENTED-UNTESTED), Sculpt over
Freeform (MOD-062).

## 17. Surface surfaces (`SurfaceFinishView`, `SurfaceEditorView`)

| Control | id (def) | Act → native | Drawn / absent | Refusal | Req | Tests |
| --- | --- | --- | --- | --- | --- | --- |
| Finish surface, list, body | `surface_finish` (`SurfaceFinishView.java:31`), `surface_finish_list` (`:38`), `surface_finish_body` (`SurfaceEditorView.java:95`) | after a Surface sketch's Finish | `SurfacePresentation.anyCreateShown` | — | MOD-100, UI-067 | SurfaceOwnerTest (FC2-D2); ids referenced by no test |
| Patch / Extrude / Revolve / Loft / Trim / Section | `surface_create_patch/extrude/revolve/loft/trim/section` (`SurfacePresentation.java:157-162`) | `commitSketch` (`:427`) → `surfaceCommitSketch(kind, value, keepInside)` (first one creates the project through `loadProjectDocument`) | `createShown(sketchState, kind)`: only kinds whose candidate commits | — (blocked kinds absent; `surface_create_blocked` (`:136`) explains) | MOD-093..097, MOD-102 | devSurf01..03 (FC2-D2) |
| Distance, Angle, Keep inside, Offset | `field_surface_distance` (`:106`), `field_surface_angle` (`:110`), `surface_trim_keep_inside` (`:114`), `field_surface_offset` (`:150`) | parameters of the commit / next sketch | per kind | distance seeded unitless (J-02) | MOD-094, MOD-095 | devSurf01..03 (FC2-D2) |
| Editor, summary | `surface_editor` (`:95`), `surface_summary` (`:100`) | `refreshBody` (`:323`) reads `surfaceState`, `surfaceLiveFeatures`, `surfaceTimeline` | Surface body, Shape held | — | MOD-092 | SurfaceOwnerTest; ids referenced by no test |
| New Surface Sketch | `surface_new_sketch` (`:154`) | `onSurfaceSketchRequested` (`:2479`) → `surfaceBeginSketch(body, XY, offset)` | Surface body | while `SurfaceSketchPurpose` is set the CAD commit refuses (MOD-101) | MOD-101 | devSurf01 (FC2-D2) |
| Stitch | `surface_stitch` (`:169`) | `surfaceStitch(body, true)` (`:174`) | `stitchShown(surfaceStitch(body, false))` | gap, non-manifold, incompatible refused by name (MOD-098) | MOD-098 | devSurf01 (FC2-D2) |
| Thickness + Thicken (per live feature) | `field_surface_thickness` (`:179`), `surface_thicken` (`:354`) | `surfaceThicken(body, featureId, t, true)` (`:363`) | `thickenShown(surfaceThicken(..., false))` | `ThickenUnsupportedForSurfaceType` (MOD-099) | MOD-099 | devSurf01/02 (FC2-D2) |
| Staged value edit: title, value, verdict, Apply, Fix, Cancel | `surface_edit_title` (`:184`), `field_surface_edit_value` (`:186`), `surface_edit_verdict` (`:189`), `surface_edit_apply` (`:195`), `surface_edit_fix` (`:203`), `surface_edit_cancel` (`:211`) | `beginEdit` (`:259`) → `surfaceValue`; `applyEdit` (`:452`) → `surfaceApplyValue` (ONE step) | from a History row; Apply drawn only when the whole chain rebuilds (`applyShown`) | first failing feature named; Cancel writes nothing (MOD-104). **No visibility check: a hidden Surface body can be edited (J-01)** | MOD-103, MOD-104 | devSurf02 (FC2-D2) |

## 18. System and hardware paths

| Path | Behaviour (source) | Refusal / gap | Req | Tests |
| --- | --- | --- | --- | --- |
| System Back at Home | platform's own (leaves the app) | — | APP-061 | HomeFlowTest#e2eAppH1_11 (C2) |
| System Back on New Project / Settings | to Home / to where opened | — | APP-061, APP-064 | HomeFlowTest#e2eAppH1_11 (C2); SettingsPreferencesTest (FC2-D3) |
| System Back in the CAD bootstrap | `dismissTopmostSurface` (`EditorWorkspaceView.java:1177-1184`) cancels the sketch and returns Home **before** closing an open precision surface or palette | F-03 (UI-009, UI-024) | APP-061, UI-009 | HomeFlowTest#e2eAppH1_11 tests with nothing open only |
| System Back with an anchored surface open | closes the topmost (Objects, Add Primitive, precision, Display, strip, navigator); E2 — panel: overstated as a whole-product claim (F-03, F-04 are counter-cases) | — | APP-062, UI-036 | EditorWorkspaceLegibilityTest#uilr106 (FC2-D1, real `KEYCODE_BACK`) |
| System Back inside an in-project sketch / cage edit / Dimensions with nothing open | owner records disagree (APP-063 PARTIAL, UI-083 DOC-DIVERGENCE, A-10) | BLOCKED-OWNER | APP-063 | — |
| System Back during an in-project Revolve | F-04 (API 36: backgrounds the app) | F-04 | CAD-115 | none |
| Activity recreation (palette, dark mode, font scale, locale) | `configChanges` lacks `uiMode`, `fontScale`, `locale` (`AndroidManifest.xml:31`); saved baseline lost (A-05), recovery question can reappear (E-02), pending picker bytes lost (A-06) | DEFECT-CONFIRMED, panel-verified 3/3 (A-05 = E-04, E-02, A-06) | APP-039, DOC-044, DOC-049 | SettingsPreferencesTest#uiprefr1_05 (FC2-D3) does not assert the baseline |
| Rotation | not a recreation (configChanges); layout re-derived | — | UI-046 | CadRetainedSketchOwnerTest#d18 (C2) |
| Process death | autosave checkpoint; recovery question on next cold launch | after Recover, nothing protects the work (A-02) | APP-053..056 | ProjectProcessDeathTest#stage1..8 (FC2-D3) |
| Renderer stops mid-session | reported only at the next resume (H-01, RND-006 DEFECT-CONFIRMED, panel-verified 3/3, P2, E2) | — | APP-072, RND-006 | DiagnosticsAndRendererLossTest#e2er1b07 (FC2-D3) uses the injection seam |
| Number / letter keys | debug fixtures in a debug build only; no-op in release | — | APP-076 | release guard PASS (`evidence/local/release_guard.txt`) |
| Ctrl+Z / Ctrl+S / Escape | not mapped | — | APP-077, UI-080 FUTURE-NOT-DUE | — |

## 19. Ids referenced by no androidTest class (89, E2 grep)

Containers and read-only text are expected here; the act-bearing ones are
marked **bold** and are the control-level test gaps.

`editor_workspace`, `start_page_mark`, `start_page_product_name`,
`unsaved_prompt`, `unsaved_prompt_panel`, `recovery_prompt`,
`recovery_prompt_panel`, **`display_mode_debug`**, `history_navigator_list`,
**`history_navigator_row`**, `sketch_plane_chooser`, **`sketch_plane_back`**,
`project_drawer_scroll`, `sketch_group`, `brush_strength_value`,
`inspector_footer`, `sculpt_mesh_summary`, `sculpt_mask_summary`,
`objects_list`, **`object_row`**, `sketch_entity_row_rectangle`,
`sketch_entity_row_circle`, `sketch_entity_row_line`, `sketch_entity_summary`,
**`field_sketch_circle_radius`**, **`field_sketch_line_x0`**, **`field_sketch_line_y0`**,
**`field_sketch_line_x1`**, **`field_sketch_line_y1`**, `cad_plane_summary`,
`cad_profile_summary`, `cad_row_rectangle`, `cad_row_circle`,
**`field_cad_circle_radius`**, **`cad_direction_along`**, **`cad_direction_against`**,
`body_size_group`, **`dimension_anchor_center`**, **`dimension_anchor_positive`**,
**`apply_relative_scale`**, `sketch_actions_palette`, **`sketch_action_delete`**,
`sketch_modify_mode`, `sketch_modify_mode_title`,
**`sketch_dimension_kind_horizontal`**, **`sketch_dimension_kind_vertical`**,
**`sketch_dimension_kind_edge_length`**, **`sketch_dimension_kind_edge_angle`**,
`sketch_dimension_labels`, `sketch_dimension_value_editor`,
`cad_extrude_actions_palette`, `cad_extrude_operation_palette`,
**`cad_extrude_operation_new_body`**, `feature_history_segments`,
`feature_history_detail`, `feature_history_actions`, **`sketch_browser_row`**,
`sketch_area_shortcuts`, `sketch_revolve_section`,
`sketch_revolve_axis_summary`, **`field_revolve_angle`**,
`cad_revolve_angle_label`, `cad_revolve_angle_editor`, `sketch_operation_row`,
`feature_history`, `feature_history_list`, `regeneration_issue_reason`,
`regeneration_issue_rows`, **`add_freeform_plane`**, **`add_freeform_cylinder`**,
`freeform_counts`, **`freeform_multi_select`**, **`freeform_mode_move`**,
**`freeform_mode_rotate`**, **`freeform_mode_scale`**, **`freeform_delete_faces`**,
**`freeform_symmetry_y`**, **`freeform_symmetry_z`**, **`freeform_level_1`**,
**`freeform_level_2`**, **`freeform_level_4`**, `surface_editor`,
`surface_finish`, `surface_finish_list`, `surface_finish_body`,
`surface_summary`, **`surface_trim_keep_inside`**, `surface_create_blocked`,
`surface_edit_title`.

Some of these acts are exercised by a test through JNI or through a helper
that finds the view another way (e.g. `object_row` via `rowFor`, row acts in
`FeatureHistoryView` via the rows container); a missing id reference is a
lead, not a proof that the act is untested.

## 20. Control-level findings collected here (seat R2 verdicts and the `work/VERIFICATION.md` panel)

| Finding | Control(s) | Verdict (seat R2) | Grade | Severity | Panel (`work/VERIFICATION.md`) |
| --- | --- | --- | --- | --- | --- |
| E-01 = B-02 | `export_action` over a face-supported CAD body | DEFECT-CONFIRMED | E1 (seat diag `export_face`) | P1 | survived 3/3 (both) |
| A-02 | `recovery_recover` | DEFECT-CONFIRMED (owner decision: inverts FSR1B-05) | E2 | P1 | survived 3/3 |
| A-01 | `unsaved_save` before `project_open` | DEFECT-CONFIRMED | E2 | P2 | survived 3/3 (= X-A01, 3/3) |
| A-04 = C-02 = D-02 = E-03 | `import_glb` while sketching | DEFECT-CONFIRMED | E1 + E2 | P2 | survived 3/3 (all four) |
| B-01 | `tool_rail_place`, `apply_transform` on a face-supported body | DEFECT-CONFIRMED | E1 | P2 | survived 3/3 |
| C-01 = F-02 | `sketch_navigator_flip`, `_rotate_ccw`, `_rotate_cw` | DEFECT-CONFIRMED | E1 + E2 | P2 | survived 3/3 (both) |
| F-01 | `sketch_navigator_flip`, `view_selection_outline_on/off`, `sketch_dimension_label` | DEFECT-CONFIRMED | E1 | P2 | survived 3/3 |
| F-03 | System Back in the bootstrap with a panel or palette open | DEFECT-CONFIRMED | E2 | P2 | survived 3/3 |
| F-04 | System Back during an in-project Revolve (API 36) | DEFECT-CONFIRMED | E2 | P2 | survived 3/3 |
| F-05 | `field_body_dimension`, `field_cad_extrude_depth/second`, `field_sketch_line_length` | DEFECT-CONFIRMED | E2 | P2 | survived 3/3 |
| D-01 | `undo_action` in Sculpt after a NotRetained stroke | DEFECT-CONFIRMED | E1 | P2 | survived 3/3 |
| E-02 | `recovery_prompt` after a recreation | DEFECT-CONFIRMED | E2 | P2 | survived 3/3 |
| H-01 | (no control) renderer stop not reported | DEFECT-CONFIRMED | E2 | P2 | survived 3/3 |
| A-05 | Settings palette rows → saved baseline | DEFECT-CONFIRMED | E2 | P3 | survived 3/3 (= E-04, 3/3); panel record lists P2 |
| A-06 | `project_save_copy`, `export_action` across a recreation | DEFECT-CONFIRMED | E2/E5 | P3 | survived 3/3; panel record lists P2 |
| A-08 | `home_open_file` picker launch failure text | DEFECT-CONFIRMED | E2 | P3 | not panel-verified (seat grade only) |
| B-04 | `object_row_lock` + selection change / Undo | PARTIAL | E2 | P3 | not panel-verified (seat grade only) |
| B-05 | `object_row_duplicate` / Mirror names at 96 bytes | DEFECT-CONFIRMED | E1 | P3 | not panel-verified (seat grade only) |
| C-03 | `sketch_operation_add/cut` with a full sketch table | DEFECT-CONFIRMED | E2 (E1 for the domain refusal) | P3 | not panel-verified (seat grade only) |
| C-04 | `apply_sketch_line_length` refusal name | PARTIAL | E1 | P3 | not panel-verified (seat grade only) |
| D-03 = E-07 | `project_open` / `project_open_file` over a Freeform cage edit | DEFECT-CONFIRMED | E1 | P3 | not panel-verified (seat grade only) |
| D-05 | `clear_mask` | PARTIAL | E1 | P3 | not panel-verified (seat grade only) |
| D-07 | `resume_sculpt` while sketching (native) | PARTIAL | E2 | P3 | not panel-verified (seat grade only) |
| D-08 | `freeform_symmetry_*`, `freeform_level_*` | PARTIAL | E2 | P3 | not panel-verified (seat grade only) |
| E-12 | `project_open` / `project_open_file` post-decode refusal | DEFECT-CONFIRMED | E2 | P3 | not panel-verified (seat grade only) |
| F-06 | `status_message` in Sculpt | DEFECT-CONFIRMED | E1 | P3 | not panel-verified (seat grade only) |
| J-01 | `feature_history_action`, `edit_cad_sketch`, Surface edit over a hidden body | BLOCKED-OWNER (refusal) / PARTIAL (no preview) | E1 | P3 | not panel-verified (seat grade only) |
| J-02 | `field_freeform_distance`, `field_surface_*` unit | DEFECT-CONFIRMED | E1 (arithmetic) + E2 (wiring) | P3 | not panel-verified (seat grade only) |
| J-04 | `project_save` rename fallback | DEFECT-CONFIRMED | E2 | P3 | not panel-verified (seat grade only) |
| A-03 | (absent) Open Saved Project at Home | BLOCKED-OWNER | E2 | P2 | survived 2/3 |
| A-07 | `unsaved_prompt` when leaving with an open sketch or staged edit (fingerprint-only dirty test) | DEFECT-CONFIRMED | E2 | P2 | survived 2/3 |
