#!/usr/bin/env python3
"""FUNCTION-COUNCIL-R2-CLOSEOUT-R1 -- alias -> root-cause map generator.

Reads nothing from the product. The alias rows below were transcribed by hand
from the read-only FC2 dossier (artifacts/function-council-r2/SEAT_*.md,
requirements_traceability.tsv, work/VERIFICATION.md, COUNCIL_DELIBERATION.md
section 4, OPPORTUNITIES.md, PRIORITISED_BACKLOG.md) and from the closeout's own
diagnostics (artifacts/function-council-r2-closeout/diagnostics/CR-0x,
I01-panel1, PANEL-H08-1). The script only checks the transcription against the
TSV (every DEFECT-CONFIRMED row present, no stray ids) and writes
ALIAS_TO_ROOT_CAUSE.tsv.

Usage: python3 -I build_alias_map.py <repo_root>
"""
import csv
import sys
from pathlib import Path

ROOT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("/home/user/Forgeshape")
FC2 = ROOT / "artifacts/function-council-r2"
OUT = ROOT / "artifacts/function-council-r2-closeout/ALIAS_TO_ROOT_CAUSE.tsv"
CD = "artifacts/function-council-r2-closeout/diagnostics"

# ---------------------------------------------------------------------------
# Root-cause catalog.  RC-01..RC-21 are the dossier's own ids
# (COUNCIL_DELIBERATION.md section 4, reused by OPPORTUNITIES.md and
# PRIORITISED_BACKLOG.md).  Where one dossier RC lumps mechanisms that need
# different fixes at different code sites, the closeout adds a letter suffix
# (RC-13a, RC-13b, ...) and keeps the parent number.  Singletons and
# hypotheses the dossier never numbered get RC-NEW-xx.
# id: (title, mechanism file:line, default category)
# ---------------------------------------------------------------------------
P = "app/src/main/cpp/"
J = "app/src/main/java/com/forgeshape/app/"
RC = {
 "RC-01a": ("GLB export reads body.transform() instead of resolveWorldModel for a face-supported CAD body",
            P+"forgeshape_gltf_export.cpp:178-180 (captureGlbExportScene: body.transform().modelMatrix/localMatrix)", "BUG"),
 "RC-01b": ("Transform write and gizmo are not refused for isFaceSupportedCad; write records Undo and moves the fingerprint but not the drawn body",
            P+"forgeshape_jni.cpp:1335-1385 (applyBoxTransform guards NoProject/Locked only); "+P+"forgeshape_gizmo.cpp:1197-1235,1415,1428", "BUG"),
 "RC-01c": ("ARCHITECTURE.md claims isFaceSupportedCad keeps gizmo and exact-value editors off; no code does",
            "ARCHITECTURE.md:2606-2608", "DOC_DIVERGENCE"),
 "RC-02":  ("Import GLB guards check Sculpt only; no sketch/support-chooser term in JNI or chrome",
            P+"forgeshape_jni.cpp:8594-8600 (importGlbDurable: inSculptMode only); "+J+"EditorWorkspaceView.java:2823 showImportAvailable(!sculpting)", "BUG"),
 "RC-03":  ("beginSketchView frames the camera on frame(), never viewFrame(); navigator Flip/+-90 change state bits only",
            P+"forgeshape_jni.cpp:313-319 (beginSketchView); "+P+"forgeshape_sketch_session.cpp:648 (viewFrame, no production caller)", "BUG"),
 "RC-04a": ("Saved baseline (everPersisted/persistedFingerprint) is a View field lost on Activity recreation",
            J+"EditorWorkspaceView.java:271-272 (not in EditorUiState.carryAcrossRecreation); ForgeShapeActivity.java:268-280; AndroidManifest.xml:31", "BUG"),
 "RC-04b": ("Pending Save Copy / Export bytes are View fields; a recreation while the picker is open turns the result into a silent cancel over an already-created empty document",
            J+"EditorWorkspaceView.java:5778,6004 (pending bytes), 5788-5793, 6050-6055 (null -> cancel); ProjectTransfer.java:96,123", "BUG"),
 "RC-04c": ("recoveryResolved is recorded only on some outcomes; recreation re-offers recovery over the live project",
            J+"EditorWorkspaceView.java:1027-1029, 5579-5582; EditorUiState.java:151", "BUG"),
 "RC-05a": ("Autosave reads the fingerprint and the checkpoint bytes under two separate g_stateMutex scopes",
            J+"AutosaveController.java:157 (projectFingerprint), :173 (encodeProject); "+P+"forgeshape_jni.cpp:8936-8947, 8387-8410", "BUG"),
 "RC-05b": ("Save clears the checkpoint on the UI thread with no ordering against an in-flight worker write",
            J+"EditorWorkspaceView.java:5465-5467 (ProjectCheckpoint.clear) vs AutosaveController.performCheckpoint", "BUG"),
 "RC-05c": ("ProjectSlot.write delete-then-rename fallback leaves the only copy in project.forge.pending, which ProjectSlot.read never reads",
            J+"ProjectSlot.java:98-123 (write fallback), 130-150 (read)", "BUG"),
 "RC-05d": ("Two autosave controllers across a recreation can race on recovery.forge.pending (HYP-E6)",
            J+"ProjectCheckpoint.java:85-125 (static, unlocked, shared pending name), :112 fallback; AutosaveController.java:283-289", "BUG"),
 "RC-05e": ("Checkpoint read takes length() then readFully; a concurrent rename can quarantine a valid checkpoint (HYP-E7)",
            J+"ProjectCheckpoint.java:133,141; EditorWorkspaceView.java:5583-5595", "BUG"),
 "RC-06a": ("Save-and-continue before Open Saved Project writes the slot that the open then reads back",
            J+"EditorWorkspaceView.java:2665-2678 (onUnsavedSaveRequested) -> 5458-5471 (saveProjectToSlot) -> 2656-2657 (openSavedProjectSlot)", "BUG"),
 "RC-06b": ("Recover deletes the checkpoint and marks the project persisted, so recovered work is in neither slot nor checkpoint",
            J+"EditorWorkspaceView.java:5652-5687 (clear 5675, noteProjectPersisted 5676/5679)", "DESIGN_HOLD"),
 "RC-06c": ("Home offers no way to reopen the internal save slot",
            J+"HomeView.java:47-79; ProjectSlot.java:63 (getFilesDir)", "DESIGN_HOLD"),
 "RC-06d": ("Leaving a project (projectDirty fingerprint-only) discards an open sketch or staged edit without a question",
            J+"EditorWorkspaceView.java:2597-2602; "+P+"forgeshape_jni.cpp:8977-8980, 8478-8482", "DESIGN_HOLD"),
 "RC-06e": ("A clean LEAVE_NEW_PROJECT does not retire the checkpoint, so a stale recovery candidate survives",
            J+"EditorWorkspaceView.java:2638-2646", "BUG"),
 "RC-06f": ("Discard followed by a cancelled or refused Open leaves the live dirty project without crash protection",
            J+"EditorWorkspaceView.java (Discard retires checkpoint before the Open completes)", "DESIGN_HOLD"),
 "RC-07a": ("loadProject/closeProject do not end the Freeform cage edit session or clear its selection",
            P+"forgeshape_jni.cpp:8462-8480 (loadProject), 8968-8990 (closeProject)", "BUG"),
 "RC-07b": ("JNI loadProject cancels the sketch, gesture and view state BEFORE loadProjectDocument's post-decode refusals",
            P+"forgeshape_jni.cpp:8463-8478 vs "+P+"forgeshape_project_state.cpp:250-320", "BUG"),
 "RC-07c": ("closeProject does not refuse an open Construction edit (FC-R1 D7)",
            P+"forgeshape_jni.cpp:8977-8980", "DESIGN_HOLD"),
 "RC-07d": ("Load enters Sculpt over a hidden active body (Stage027 GUARD-2 asked only at the two entry points)",
            P+"forgeshape_project_state.cpp:421-425", "BUG"),
 "RC-08a": ("CAD bootstrap Back branch runs before anchored-surface/palette dismissal",
            J+"EditorWorkspaceView.java:1177-1184 (before 1196-1229)", "DESIGN_HOLD"),
 "RC-08b": ("hasDismissibleSurface omits the in-project Revolve state that dismissTopmostSurface handles; on API 33+ Back goes to the platform",
            J+"EditorWorkspaceView.java:1112-1117 vs 1166-1175; ForgeShapeActivity.java:87-98,142-155", "BUG"),
 "RC-08c": ("Back is not one step inside an in-project sketch / Freeform cage / Dimensions / recovery question",
            J+"EditorWorkspaceView.java:1112-1117, 1149-1227", "DESIGN_HOLD"),
 "RC-09":  ("Three canvas editors use DigitsKeyListener('0123456789.-') and drop the decimal comma",
            J+"BodyDimensionLabelsView.java:127; CadExtrudeCanvasView.java:644; SketchDimensionLabelView.java:112", "BUG"),
 "RC-10a": ("Clear Mask is drawn although the act is refused over the per-entry cap",
            P+"forgeshape_sculpt.cpp:1687-1690", "BUG"),
 "RC-10b": ("operationAvailable offers Add/Cut on a full 16-record sketch table; candidate fails as SketchNotFound",
            P+"forgeshape_sketch_session.cpp:2932-2933 (operationAvailable), 2895-2902 (candidateState)", "BUG"),
 "RC-10c": ("Freeform Symmetry ON over an asymmetric cage and a level over the budget are drawn and then refused",
            P+"forgeshape_freeform_session.cpp (availability vs refusal predicates)", "BUG"),
 "RC-11":  ("SculptHistory::record clears redo but keeps the undo stack on a NotRetained stroke",
            P+"forgeshape_sculpt_history.cpp:80-101", "BUG"),
 "RC-12":  ("Renderer RestartRequired is polled only from onResume; a mid-session stop is not reported or checkpointed",
            J+"ForgeShapeActivity.java:326-382; EditorWorkspaceView.java:6147-6155; "+P+"forgeshape_jni.cpp:2049-2066", "BUG"),
 "RC-13a": ("Two self-test oracles compare an FP-contracted value with ==/equal-area tie (fail under FMA, the arm64 default)",
            P+"forgeshape_history_selftest.cpp:474-480; "+P+"forgeshape_cad_feature_selftest.cpp:5392-5394", "TEST_HARNESS_DEFECT"),
 "RC-13b": ("No CI workflow runs scripts/host-native-selftests.sh or the corpus-verdict diff (platform neutrality unguarded)",
            ".github/workflows/ci-fast.yml:42-223 (no host-native step)", "TEST_HARNESS_DEFECT"),
 "RC-13c": ("Self-tests read uninitialised floats after a failed projection",
            P+"forgeshape_cad_feature_selftest.cpp:5548-5550,5594; "+P+"forgeshape_sketch_ux_selftest.cpp:1474-1480", "TEST_HARNESS_DEFECT"),
 "RC-13d": ("Device FORGESHAPE_CAD_FEATURE_PERFORMANCE line truncated at 1023 chars",
            P+"forgeshape_jni.cpp:608", "TEST_HARNESS_DEFECT"),
 "RC-13e": ("Device suite Recorder silently drops checks past its cap; PROJECT at 256/256",
            P+"forgeshape_jni.cpp:821 (kMaxProjectChecks); "+P+"forgeshape_project_selftest.cpp:21-32 (Recorder::check)", "TEST_HARNESS_DEFECT"),
 "RC-13f": ("25 self-test suites run synchronously on every Activity start (not once per process)",
            P+"forgeshape_jni.cpp:2083-2126; ForgeShapeActivity.java:33-36", "TEST_HARNESS_DEFECT"),
 "RC-13g": ("Release APK never executed by any CI tier",
            "scripts/ci-release-selftest-guard.sh (absence proof only)", "TEST_HARNESS_DEFECT"),
 "RC-13h": ("No CI step compares per-suite check counts; a suite that loses checks stays green",
            "scripts/ci-device-smoke.sh (token presence/order only)", "TEST_HARNESS_DEFECT"),
 "RC-14a": ("Device evidence older than the product tree (no Tier-5 aggregate on f3db953/70a6c14)",
            "evidence currency (PROJECT_STATUS.md Next Stage); no product file", "TEST_HARNESS_DEFECT"),
 "RC-14b": ("Coverage gaps in Sculpt/Freeform/Surface suites (D-01 scenario, Clear Mask EntryTooLarge, Freeform/Surface GLB export, ...)",
            "app/src/main/cpp/*_selftest.cpp; app/src/androidTest (absent cases)", "TEST_HARNESS_DEFECT"),
 "RC-14c": ("Device taps mostly bypass hit testing (performClick / viewport-direct); no 48 dp view-tree sweep",
            "app/src/androidTest/.../SketchTestSupport.java:79-121; EditorWorkspaceLegibilityTest.java:1116-1139", "TEST_HARNESS_DEFECT"),
 "RC-14d": ("Device tests reach state through JNI seams the product never calls; toggleFace javadoc names a non-existent product path",
            "app/src/androidTest/.../CadPlanarFaceRuntimeTest.java:521-529; CadParametricHistoryOwnerTest.java:241", "TEST_HARNESS_DEFECT"),
 "RC-15a": ("Heavy CAD derivation (Finish arrangement, candidate regeneration) synchronous under g_stateMutex on UI/render thread",
            P+"forgeshape_jni.cpp:4400-4420, 1733, 5098; "+P+"forgeshape_sketch_session.cpp:2987-3029", "PERFORMANCE_RISK"),
 "RC-15b": ("Per stroke sample whole-mesh republish, full render-mesh rebuild, full upload and GPU drain",
            P+"forgeshape_jni.cpp:10036-10040; "+P+"forgeshape_sculpt.cpp:611; "+P+"forgeshape_renderer.cpp:1119,868,749", "PERFORMANCE_RISK"),
 "RC-15c": ("Render loop never idles while a surface is attached",
            P+"forgeshape_jni.cpp (renderThreadMain loop)", "PERFORMANCE_RISK"),
 "RC-15d": ("Face-supported bodies re-derive their face frame every snapshot (FC-R1 F-07)",
            P+"forgeshape_scene.cpp:281-326", "PERFORMANCE_RISK"),
 "RC-15e": ("Save / autosave encode holds g_stateMutex that the render loop takes every frame",
            P+"forgeshape_jni.cpp:8391, 1870-1872; "+J+"EditorWorkspaceView.java saveProjectToSlot", "PERFORMANCE_RISK"),
 "RC-16a": ("validateProjectDocument skips regeneration for PlanarFaces chains with later features; decode passes, load fails",
            P+"forgeshape_project_document.cpp:927-940", "BUG"),
 "RC-16b": ("Codec accepts two encodings of one face token (edgeLocalIndex > 0xFFFF)",
            P+"forgeshape_sketch.cpp:723-749", "BUG"),
 "RC-17a": ("Native lock guard is checked only at gizmo activation; selection change/Undo leave a live gizmo over a locked body",
            P+"forgeshape_jni.cpp:3842-3850 vs 3198-3232, 7995-8030; GizmoSession::beginDrag never reads locked()", "BUG"),
 "RC-17b": ("Gizmo and placement editor stay over a hidden active body (no rule decides it)",
            J+"EditorWorkspaceView.java:3028-3033; "+P+"forgeshape_gizmo.cpp:1197", "DESIGN_HOLD"),
 "RC-17c": ("Lock means different things per representation (Freeform refuses edits, CAD/Surface do not)",
            P+"forgeshape_freeform_session.cpp:278,302; "+P+"forgeshape_surface_authoring.cpp:79-110", "DESIGN_HOLD"),
 "RC-17d": ("A hidden CAD/Surface body can be edited through History/Edit; no solid candidate drawn for a target outside the snapshot",
            P+"forgeshape_jni.cpp:7137-7250, 1758-1765; "+P+"forgeshape_scene.cpp:332-344", "DESIGN_HOLD"),
 "RC-17e": ("A locked face-supported dependent moves with its producer (derived placement ignores lock)",
            P+"forgeshape_scene.cpp resolveWorldModel", "DESIGN_HOLD"),
 "RC-18a": ("Freeform/Surface length rows seeded with unitless literals, read in the display unit, not converted on unit change",
            J+"FreeformEditorView.java:123-125,282-285; SurfaceEditorView.java:108,152,181,499-516; EditorWorkspaceView.java:6403-6416", "BUG"),
 "RC-18b": ("Freeform tap slop is density-scaled while its comment says it is the sketch's raw 24 px",
            P+"forgeshape_freeform_session.h:43-45, .cpp:640 vs "+P+"forgeshape_sketch_session.h:208-210", "DOC_DIVERGENCE"),
 "RC-19":  ("ObjectId allocator high-water mark is in the fingerprint and bytes and never rolled back",
            P+"forgeshape_project_state.cpp:138, 410-415, 753", "DESIGN_HOLD"),
 "RC-20":  ("chip() sets only a minimum height; several chrome controls are below 48 dp wide",
            J+"EditorControlStyles.java:366-382; SketchOrientationNavigatorView.java:134-137; DisplaySettingsPopoverView.java; SketchDimensionLabelView.java", "BUG"),
 "RC-21":  ("Documentation / comment drift (itemised in CONTRACT_CONFLICTS.md)",
            "PROJECT_STATUS.md, README.md, docs/CI_CLOUD.md, ARCHITECTURE.md, PRODUCT.md, code comments", "DOC_DIVERGENCE"),
 # --- closeout-minted ids (dossier left these un-numbered) ---
 "RC-NEW-01": ("Open-picker launch failure shows the Save Copy failure string",
               J+"EditorWorkspaceView.java:2090, 2652", "BUG"),
 "RC-NEW-02": ("New Freeform refusal shows a raw status token; non-Sculpt projects summarised 'in Construction'",
               J+"EditorWorkspaceView.java:2448, 6190-6198", "BUG"),
 "RC-NEW-03": ("derivedBodyName appends the suffix then sanitises to 96 bytes, cutting the suffix or emptying the name",
               P+"forgeshape_body_commands.cpp:154-181", "BUG"),
 "RC-NEW-04": ("sceneAddBody has no hasProject() guard (withdrawn: New Sculpt relies on it)",
               P+"forgeshape_jni.cpp sceneAddBody; "+J+"EditorWorkspaceView.java:2402-2406", "BUG"),
 "RC-NEW-05": ("applyLineLength maps out-of-range/negative lengths to ZeroLengthLine",
               P+"forgeshape_sketch_session.cpp:825-828", "BUG"),
 "RC-NEW-06": ("Commit regenerates the chain a second time instead of reusing the evaluated candidate",
               P+"forgeshape_sketch_session.cpp commitIntoTarget:3098; CadBody::applyState", "PERFORMANCE_RISK"),
 "RC-NEW-07": ("Near-full revolve (359.9999999 deg) yields caps 1.7e-7 m apart that kernel validation accepts",
               P+"forgeshape_cad_revolve.cpp", "BUG"),
 "RC-NEW-08": ("Resume Sculpt has no native sketch/chooser guard (chrome-only)",
               P+"forgeshape_jni.cpp (resume entry)", "BUG"),
 "RC-NEW-09": ("Writer has no byte ceiling while readers refuse files > 256 MiB",
               J+"ProjectSlot.read / ProjectCheckpoint.read / ProjectTransfer.readFrom (256 MiB) vs encoder", "BUG"),
 "RC-NEW-10": ("No parent-directory fsync after renameTo in ProjectSlot/ProjectCheckpoint",
               J+"ProjectSlot.java:98-123; ProjectCheckpoint.java:85-125", "BUG"),
 "RC-NEW-12": ("In Sculpt the status pill is overlapped by the brush column and Tool Rail",
               J+"GlobalToolbarView.java:446; BrushEdgeControlsView", "BUG"),
 "RC-NEW-13": ("Selected state uses setActivated only; status_message has no live region (TalkBack)",
               J+"EditorControlStyles.java:433-439; ToolRailView.java:242", "DESIGN_HOLD"),
 "RC-NEW-14": ("Light Charcoal focus ring 2.48:1 against the precision surface",
               "app/src/main/res (palette tokens); artifacts/ui-pref-r1/CONTRAST.md", "DESIGN_HOLD"),
 "RC-NEW-15": ("Mouse input can only orbit (no wheel/secondary button; pan and zoom need two pointers)",
               P+"forgeshape_jni.cpp:404-413; "+P+"forgeshape_camera.cpp:289-311; PointerSemantics.java:41", "FUTURE_OPPORTUNITY"),
 "RC-NEW-16": ("Settings page Back control reachable only after scrolling",
               J+"SettingsPageView.java (settings_back at y 3409-3535 px)", "FUTURE_OPPORTUNITY"),
 "RC-NEW-17": ("Status line carries first-entry instructions (bootstrap, sculpt gesture rule)",
               "app/src/main/res/values/strings.xml:54, :753; EditorWorkspaceView.java:2374", "DOC_DIVERGENCE"),
 "RC-NEW-18": ("Sketch-dimension editor flush with the right window edge (may sit in the back-gesture zone)",
               J+"SketchDimensionLabelView.java (dimension_editor x2=1080 px)", "BUG"),
 "RC-NEW-19": ("Gesture arbitration and load orchestration live inside the JNI translation unit",
               P+"forgeshape_jni.cpp:9503-10319, 8463-8479, 228-307", "FUTURE_OPPORTUNITY"),
 "RC-NEW-20": ("One document and one display density per process (static singletons)",
               P+"forgeshape_scene.cpp:406,604,638; forgeshape_gizmo.cpp:15", "FUTURE_OPPORTUNITY"),
 "RC-NEW-21": ("createSwapchain may pass an unsupported composite alpha on a desktop surface",
               P+"forgeshape_renderer.cpp:2175-2178", "FUTURE_OPPORTUNITY"),
 "RC-NEW-22": ("Triangles with twice-area <= 1e-8 m^2 cannot be picked (absolute threshold)",
               P+"forgeshape_picking.cpp", "DESIGN_HOLD"),
 "RC-NEW-23": ("Cold start rebuilds an equal-extent swapchain once (surfaceChanged always requests resize)",
               P+"forgeshape_jni.cpp:9439-9444", "PERFORMANCE_RISK"),
 "RC-NEW-24": ("Shader/C++ constant parity enforced by comments only",
               "app/src/main/cpp/shaders/* vs forgeshape_selection_outline.h / grid constants", "TEST_HARNESS_DEFECT"),
 "RC-NEW-25": ("Overlay revision counters of different producers can collide (renderer skips equal revision+count)",
               P+"forgeshape_renderer.cpp:1601-1604", "BUG"),
 "RC-NEW-26": ("Outline-mask depth attachment shared across frames in flight; WAW at early fragment tests unverified",
               P+"forgeshape_renderer.cpp:~2929-2943", "BUG"),
 "RC-NEW-27": ("Picking is a linear triangle scan (~0.3 s per tap at 8M triangles, estimated)",
               P+"forgeshape_picking.cpp", "PERFORMANCE_RISK"),
 "RC-NEW-28": ("CI triggers do not run on a task-branch push, contrary to CLAUDE.md and docs/CI_CLOUD.md:12",
               ".github/workflows/ci-fast.yml:12-17; ci-device.yml:10-15", "DOC_DIVERGENCE"),
 "RC-NEW-29": ("Fixed sleeps (settleLayout sleep(250), 884 sites) in device tests",
               "app/src/androidTest/.../WorkspaceTestSupport.java:417-421", "TEST_HARNESS_DEFECT"),
 "RC-NEW-30": ("EditorWorkspaceLegibilityTest restores animator_duration_scale outside @After",
               "app/src/androidTest/.../EditorWorkspaceLegibilityTest.java:783,798 (@After :86-96)", "TEST_HARNESS_DEFECT"),
 "RC-NEW-31": ("An assumption-failed test may be reported inside 'OK (n tests)'",
               "scripts/ci-device-smoke.sh (status -3/-4 -> skipped)", "TEST_HARNESS_DEFECT"),
 "RC-NEW-32": ("summary.json does not carry the audit ledger verdicts",
               "scripts/ci-device-smoke.sh; Ui3dStateAuditTest javadoc :34-47", "TEST_HARNESS_DEFECT"),
 "RC-NEW-33": ("SpatialSketchTest pair-order failure still latent (setUp unchanged)",
               "app/src/androidTest/.../SpatialSketchTest.java setUp; artifacts/sel-out-r1/PREEXISTING_SHARD5_FAILURE.md", "TEST_HARNESS_DEFECT"),
 "RC-NEW-34": ("encodeProject does not refuse during an open Construction edit; checkpoint can capture an uncommitted drag",
               P+"forgeshape_jni.cpp:8387-8410", "BUG"),
 "RC-NEW-35": ("8 orphan JNI natives, 2 dead helpers, 13 debug* exports stubbed in release",
               P+"forgeshape_jni.cpp; forgeshape_sketch_region.cpp:50; forgeshape_sketch.cpp:235", "FUTURE_OPPORTUNITY"),
 "RC-NEW-36": ("World-plane retained sketch (needs new durable semantics, CADB v9)",
               "n/a (future feature)", "FUTURE_OPPORTUNITY"),
 "RC-NEW-37": ("Real-stylus hover preview in Sculpt (needs an autosave-suspend concept)",
               "n/a (future feature)", "FUTURE_OPPORTUNITY"),
 "RC-NEW-38": ("Revolve Add/Cut, later Revolve, curved trim, T-Spline refinement, NURBS",
               "n/a (future features)", "FUTURE_OPPORTUNITY"),
 "NONE":      ("No finding (id never assigned)", "-", "DOC_DIVERGENCE"),
}

# ---------------------------------------------------------------------------
# Alias rows: (alias_id, kind, seat, severity_as_given, rc_ids(';'),
#              category or None for RC default, status, confidence, notes)
# confidence: E1 = reproduced in THIS closeout (diagnostics cited);
#             E2 = source file:line; E3 = recorded run of an earlier stage/FC2;
#             E4 doc only; E5 inference.
# ---------------------------------------------------------------------------
SF, RQ, OP, BL = "seat_finding", "requirement_row", "opp", "backlog_row"
cr = lambda d: f"{CD}/{d}"
A = []
def a(*r): A.append(r)

# ---- Seat A ----
a("A-01", SF, "A", "P1 (R2: P2, H1)", "RC-06a", None, "current", "E1",
  f"E1 = {cr('CR-05')}/build_and_run.out A01_SLOT_NO_LONGER_HOLDS_A=OBSERVED, A01_A_EXISTS_IN_NO_FILE=OBSERVED (host run of real ProjectSlot/ProjectCheckpoint/AutosaveController + line-cited transcription of EditorWorkspaceView, Android shims; not a device). Panel 3/3. Fix form gated by OD-06")
a("X-A01", SF, "J", "P1", "RC-06a", None, "current", "E1", "seat J cross-verification of A-01 (SEAT_J.md 'Cross-seat re-verification'); duplicate of A-01, panel 3/3")
a("A-02", SF, "A", "P1", "RC-06b", None, "current", "E1",
  f"Mechanism reproduced: {cr('CR-05')}/build_and_run.out A02_RECOVERED_WORK_LOST_ON_SECOND_DEATH=OBSERVED, A02b_NEW_PROJECT_ASKED_NOTHING_AND_CLOSED_RECOVERED_WORK=OBSERVED (host transcription). OWNER-SEMANTICS-REQUIRED: PRODUCT.md:193-194 and pinned FSR1B-05 state the behaviour on purpose; CLAUDE.md 'Autosave protects work' argues against (disagreement D-2, OD-05). Counted as DESIGN_HOLD, BUG if OWNER rules CLAUDE.md governs")
a("A-03", SF, "A", "P1 (R2: P2 BLOCKED-OWNER)", "RC-06c", None, "current", "E2", "BLOCKED-OWNER OD-07 (APP-H1 'exactly two' Home acts); = IDEA-05")
a("A-04", SF, "A", "P2", "RC-02", None, "current", "E1",
  f"{cr('CR-03')}/scenario_A.out..scenario_C.out: import accepted with sketch / chooser open (bodies 1->2, active switches, undo 1); scenario_D control refused in Sculpt. Panel 3/3")
a("A-05", SF, "A", "P2 (R2: P3)", "RC-04a", None, "current", "E1",
  f"{cr('CR-05')}/build_and_run.out A05_UNSAVED_QUESTION_OVER_A_SAVED_PROJECT=OBSERVED (host transcription). = E-04 = FC-R1 D3; severity disagreement D-3 (A: P3, E: P2)")
a("A-06", SF, "A", "P2 (R2: P3)", "RC-04b", None, "current", "E2",
  f"Mechanism partly exercised: {cr('CR-05')}/build_and_run.out A06_TRANSCRIPT_RESULT_IS_TREATED_AS_CANCEL=OBSERVED; empty-document consequence of ACTION_CREATE_DOCUMENT not reproduced (E5). Kept at E2")
a("A-07", SF, "A", "P2 (R2: P3/INFO BLOCKED-OWNER)", "RC-06d", None, "current", "E2", "Consistent with the sketch-volatility rule; OD-10. Panel 2/3")
a("A-08", SF, "A", "P3", "RC-NEW-01", None, "current", "E2", "= APP-007, OPP-27")
a("A-09", SF, "A", "P3", "RC-21", "DOC_DIVERGENCE", "current", "E2", "strings.xml:29,316; NewProjectChooserView javadoc; PRODUCT.md:1990-1992; PROJECT_STATUS.md:3379,3524")
a("A-10", SF, "A", "P3 (R2: split)", "RC-08c", None, "narrowed", "E2", "Split in R2: recovery-question half = DOC_DIVERGENCE vs PRODUCT.md:196; in-project sketch/Freeform/Dimensions half BLOCKED-OWNER under APP-H1 (OD-28)")
a("A-11", SF, "A", "P3 (R2: narrowed)", "RC-NEW-02", None, "narrowed", "E2", "Narrowed to the New Freeform path bypassing FreeformPresentation.refusalMessage; = OPP-30")
a("A-12", SF, "A", "P3", "RC-21", "DOC_DIVERGENCE", "current", "E3", "PRODUCT.md:2080-2082 overstates sculpted export; = D-06; recorded ImportedMeshSculptTest -14")
a("A-13", SF, "A", "P3 (PARTIAL)", "RC-06e", None, "narrowed", "E2", "R2: only the stale-after-clean-leave part is the gap; Open File's checkpoint is deliberate (6108-6110)")
a("H-A1", SF, "A", "hypothesis", "RC-07b", None, "hypothesis", "E2", "Ordering is E2 (= E-12 / DOC-087); a reachable refused Open over a sketch is unproven (decode already validates)")
a("H-A2", SF, "A", "hypothesis", "RC-02", None, "hypothesis", "E5", "Consequence of import-during-sketch for a staged face Add/Cut; = C H-04, D H-2, E H-E5")
a("H-A3", SF, "A", "hypothesis", "RC-08c", None, "hypothesis", "E5", "API < 31 Back in an in-project sketch finishes the Activity")
# ---- Seat B ----
a("B-01", SF, "B", "P2", "RC-01b", None, "current", "E1",
  f"{cr('CR-02')}/cr02_transform_face.out C1/C2: applyBoxTransform and a gizmo drag on the dependent -> undoDelta=1 fingerprintChanged=1 storedMoved=1 worldMoved=0; controls D1/D3 world-plane CAD move. Panel 3/3")
a("B-02", SF, "B", "P1", "RC-01a", None, "current", "E1",
  f"{cr('CR-01')}/cr01_export_face.out: 11 verdict=MISMATCH, all and only face-supported CAD bodies (e.g. S1_cap body 2 delta 3.0 m); 12 MATCH for independent bodies; S1_cap.glb == S2_side.glb byte-identical. Merged into E-01")
a("B-03", SF, "B", "P2", "RC-01c", None, "current", "E2", "ARCHITECTURE.md:2606-2608 states a guard RC-01b shows is absent; correct the doc only after T-02")
a("B-04", SF, "B", "P3 (R2: PARTIAL/BLOCKED-OWNER)", "RC-17a", None, "narrowed", "E3",
  "FC2 diag_objects case 1 (domain). Latent: Java refresh takes the gizmo down synchronously; no product path reaches the drag. Defence-in-depth gap vs 'removing a control is not removing a guard'")
a("B-05", SF, "B", "P3", "RC-NEW-03", None, "current", "E3", "FC2 diag_objects case 4; = CON-036; comment :177 'unreachable' also DOC_DIVERGENCE (CC-45); OPP-29 gated OD-31")
a("B-06", SF, "B", "owner-pending", "RC-17b", None, "current", "E3", "UNDETERMINED-SOURCE-GAP; FC2 diag_objects case 2")
a("B-07", SF, "B", "withdrawn", "RC-NEW-04", None, "withdrawn", "E2", "Withdrawn R2 (delib_E_to_B #3): New Sculpt calls sceneAddBody at Home inside the initialization bracket; kept as H-B4")
a("H-B1", SF, "B", "hypothesis", "RC-17a", None, "withdrawn", "E2", "Rejected as reachable: touch delivery and onViewportGestureSettled share the UI thread")
a("H-B2", SF, "B", "hypothesis", "RC-01b", None, "withdrawn", "E2", "Not supported: validateProjectDocument checks transformValid only, file reopens")
a("H-B3", SF, "B", "hypothesis", "RC-07d", None, "hypothesis", "E2", "Carried debt; reproduced by seat D as D-04 (counted there)")
a("H-B4", SF, "B", "hypothesis", "RC-NEW-04", None, "hypothesis", "E5", "A stray sceneAddBody caller outside the bracket; none found")
# ---- Seat C ----
a("C-01", SF, "C", "P2", "RC-03", None, "current", "E1",
  f"{cr('CR-06')}/cr06_native.out B2/B3: camera_view_identical_to_B0=1 control_differs_from_B0=1 after quarter turns. = F-02 = UI-018 = FC-R1 D2; post-fix view on Back to Sketch is OWNER (D-6, OD-09)")
a("C-02", SF, "C", "P2", "RC-02", None, "current", "E1", f"Duplicate of A-04; {cr('CR-03')}")
a("C-03", SF, "C", "P3", "RC-10b", None, "narrowed", "E2", "Keep Sketch sub-claim withdrawn (see C-03-KeepSketch); probe P6 re-implemented the code; = CAD-063, OPP-20; table-sharing policy OD-14")
a("C-03-KeepSketch", SF, "C", "P3 (sub-claim)", "RC-10b", None, "withdrawn", "E2", "keepsOnProducer (forgeshape_sketch_session.cpp:545-552) checks the table size (delib_F_to_C #3)")
a("C-04", SF, "C", "P3 (PARTIAL)", "RC-NEW-05", None, "current", "E3", "FC2 probe P4: -1 and 1e9 -> ZeroLengthLine; refusal correct, name wrong; = OPP-22")
a("C-05", SF, "C", "INFO", "RC-21", "DOC_DIVERGENCE", "current", "E2", "Three stale CadStatus comments forgeshape_sketch.h:141-145, 216-219")
a("C-06", SF, "C", "P2 (risk, narrowed)", "RC-14a", None, "narrowed", "E3",
  "Narrowed by FC2 run 40800569636 (SketchUxTest, CadVerticalSliceTest, CadExtrudeExtentTest green); five classes still without a run on this tree in the FC2 record. A coordinator CI DEVICE run is in progress and is NOT claimed here")
a("H-01(C)", SF, "C", "hypothesis", "RC-NEW-06", None, "hypothesis", "E2", "Double regeneration on commit; cost unmeasured on ARM64")
a("H-02(C)", SF, "C", "hypothesis", "RC-15a", None, "hypothesis", "E5", "Evaluation on the UI thread (FC-R1 I-4); covered by H-05")
a("H-03(C)", SF, "C", "hypothesis", "RC-NEW-07", None, "hypothesis", "E3", "FC2 P2: 260 triangles, caps ~1.7e-7 m apart, kernel accepts; downstream harm unshown")
a("H-04(C)", SF, "C", "hypothesis", "RC-02", None, "hypothesis", "E2", "Add/Cut after import still lands on its producer (commitIntoTarget:3098)")
# ---- Seat D ----
a("D-01", SF, "D", "P2", "RC-11", None, "current", "E1",
  f"{cr('CR-04')}/cr04.out SUBJECT scenarios: FAIL K3_undo_lands_on_a_state_that_existed, FAIL K4/K7 hasEdits; controls PASS. = MOD-021 = FC-R1 D4. Panel 3/3")
a("D-02", SF, "D", "P2", "RC-02", None, "current", "E1", f"Duplicate of A-04; {cr('CR-03')}")
a("D-03", SF, "D", "P3", "RC-07a", None, "current", "E3", "FC2 diag_d2 sessionActive=1 firstSel=3 after load; = E-07 = APP-087 = OPP-13")
a("D-04", SF, "D", "P3 (R2: PARTIAL)", "RC-07d", None, "current", "E3", "Recorded technical debt (Stage027 GUARD-2); = E-09 = DOC-063; crafted-file path")
a("D-05", SF, "D", "P3 (R2: PARTIAL)", "RC-10a", None, "narrowed", "E3", "= FC-R1 D6; what an over-cap Clear Mask should do is OD-23; minimal fix (withdraw) follows CLAUDE.md")
a("D-06", SF, "D", "P3", "RC-21", "DOC_DIVERGENCE", "current", "E3", "= A-12")
a("D-07", SF, "D", "P3 (PARTIAL)", "RC-NEW-08", None, "narrowed", "E2", "Latent: control withdrawn in chrome; native guard missing (= OPP-28)")
a("D-08", SF, "D", "P3 (PARTIAL)", "RC-10c", None, "narrowed", "E3", "FC2 host + E2")
a("D-09", SF, "D", "INFO", "RC-17c", None, "current", "E3", "BLOCKED-OWNER (D-OD3); FFD_locked_body_refuses_tool")
a("D-10", SF, "D", "P3", "RC-14b", None, "narrowed", "E3", "Device half withdrawn by FC2 run 40800569636; remaining: absent asserts (IMPLEMENTED-UNTESTED rows)")
a("H-1(D)", SF, "D", "hypothesis", "RC-07c", None, "hypothesis", "E5", "Split-touch drawer act during a Freeform cage drag")
a("H-2(D)", SF, "D", "hypothesis", "RC-02", None, "hypothesis", "E5", "Import during Edit Sketch of a face-supported body")
a("H-3(D)", SF, "D", "hypothesis", "RC-11", None, "hypothesis", "E5", "Mask-side variant of D-01")
# ---- Seat E ----
a("E-01", SF, "E", "P1", "RC-01a", None, "current", "E1", f"{cr('CR-01')}; duplicate of B-02 (= CON-025, DOC-067, OPP-01)")
a("E-02", SF, "E", "P2", "RC-04c", None, "current", "E1",
  f"{cr('CR-05')}/build_and_run.out E02_RECOVERY_PROMPT_SHOWN_OVER_OPEN_PROJECT=OBSERVED, E02b_SAVED_UNCHANGED_PROJECT_OFFERED_AS_UNSAVED_WORK=OBSERVED (host transcription; recreation order modelled deterministically). = DOC-044")
a("E-03", SF, "E", "P2", "RC-02", None, "current", "E1", f"Duplicate of A-04 (seat E graded E2; D-4); {cr('CR-03')}")
a("E-04", SF, "E", "P2", "RC-04a", None, "current", "E1", "Duplicate of A-05 (severity D-3)")
a("E-05", SF, "E", "P3", "RC-16a", None, "current", "E3", "FC2 pf_gap; crafted file only; = DOC-028 = ADV-003 = OPP-17")
a("E-06", SF, "E", "P3", "RC-16b", None, "current", "E3", "FC2 faceidx: equal fingerprint e5ebec3e274d33d4, different bytes; = DOC-033 = ADV-005 = OPP-18")
a("E-07", SF, "E", "P3", "RC-07a", None, "current", "E3", "Duplicate of D-03")
a("E-08", SF, "E", "P3 (R2: BLOCKED-OWNER)", "RC-06f", None, "current", "E2", "OWNER decision on protection after Discard")
a("E-09", SF, "E", "P3", "RC-07d", None, "current", "E3", "Duplicate of D-04")
a("E-10", SF, "E", "P3 (R2: PARTIAL)", "RC-07c", None, "current", "E2", "= FC-R1 D7; no reachable wrong outcome (all closeProject callers after the dirty guard); disagreement D-8")
a("E-11", SF, "E", "INFO", "RC-21", "DOC_DIVERGENCE", "current", "E2", "D5 listed open; PRODUCT.md:1990-1992; stale PlanarFaces comments")
a("E-12", SF, "E", "P3", "RC-07b", None, "current", "E2", "= DOC-087 = OPP-16 = CC-17")
a("E-13", SF, "E", "P3", "RC-05a", None, "current", "E2", "= H-02 = DOC-086 = RND-052 = ADV-022; timing E5")
a("E-14", SF, "E", "P3", "RC-05b", None, "current", "E2", "Promoted from H-E1; = DOC-084; timing E5")
a("H-E1", SF, "E", "hypothesis", "RC-05b", None, "hypothesis", "E5", "Promoted to E-14 in round 2 (counted there)")
a("H-E2", SF, "E", "hypothesis", "RC-NEW-09", None, "hypothesis", "E2", "Asymmetry E2, reachability E5; = IDEA-10")
a("H-E3", SF, "E", "hypothesis", "RC-NEW-10", None, "hypothesis", "E5", "= HJ-2 = H-G-03")
a("H-E4", SF, "E", "hypothesis", "RC-15e", None, "hypothesis", "E2", "Mechanism E2; large-size reachability E5, OWNER-PHYSICAL-UNVERIFIED")
a("HYP-E6", SF, "E", "hypothesis", "RC-05d", None, "hypothesis", "E2", "Mechanism E2, reachability E5")
a("HYP-E7", SF, "E", "hypothesis", "RC-05e", None, "hypothesis", "E2", "Mechanism E2, reachability E5")
a("H-E5", SF, "E", "hypothesis", "RC-02", None, "hypothesis", "E5", "Next commit after import misplaced")
# ---- Seat F ----
a("F-01", SF, "F", "P2", "RC-20", None, "current", "E3", "FC2 measured rects (Flip 36.2 dp, Outline chips 37/39 dp, dimension label 43.4 dp); = UI-011..UI-014 = CAD-024; panel 3/3")
a("F-02", SF, "F", "P2", "RC-03", None, "current", "E1", "Duplicate of C-01")
a("F-03", SF, "F", "P2", "RC-08a", None, "current", "E2",
  "OWNER-SEMANTICS-REQUIRED (D-5): ForgeShapeActivity.java:68-72 innermost-first vs PRODUCT.md:196-198 and the deliberate bootstrap order comment; OPP-G3 OWNER-FIRST (OD-08)")
a("F-04", SF, "F", "P2", "RC-08b", None, "current", "E2", "= UI-010 = CAD-115; API 33-35 UNVERIFIED; not run on a device")
a("F-05", SF, "F", "P2", "RC-09", None, "current", "E1",
  f"{cr('CR-06')}/CommaDiag.out: CANVAS filter '0123456789.-' turns '1 000,5' into 10005 while the row filter yields 1000.5 (real LengthUnit). = UI-051 = ADV-067")
a("F-06", SF, "F", "P3", "RC-NEW-12", None, "current", "E3", "FC2 captures ui3d11_01/05; = UI-040")
a("F-07", SF, "F", "no severity (BLOCKED-OWNER, H2)", "RC-NEW-13", None, "current", "E2", "No repo accessibility rule (OD-21); = OPP-45")
a("F-08", SF, "F", "P3 (OUT-OF-SCOPE)", "RC-NEW-14", None, "current", "E3", "#4C8FD6 on #4B4E50 = 2.48:1; palette anchors owner-approved")
a("F-09", SF, "F", "P3 (PARTIAL)", "RC-14c", None, "current", "E2", "Merged with I-08")
a("F-10", SF, "F", "INFO (FUTURE-NOT-DUE)", "RC-NEW-15", None, "current", "E2", "= G-09")
a("F-11", SF, "F", "INFO (OUT-OF-SCOPE)", "RC-NEW-16", None, "current", "E3", "screens/ui-pref-r1/facts.txt")
a("F-12", SF, "F", "P3 (DOC-DIVERGENCE)", "RC-NEW-17", None, "current", "E3", "OWNER: are first-entry hints permitted (APP-073)")
a("H-F1", SF, "F", "hypothesis", "RC-20", None, "hypothesis", "E5", "Navigator plane chips ~45 dp (computed)")
a("H-F2", SF, "F", "hypothesis", "RC-09", None, "hypothesis", "E5", "IMEs emit ',' for TYPE_NUMBER_FLAG_DECIMAL")
a("H-F3", SF, "F", "hypothesis (promoted)", "RC-08c", "DOC_DIVERGENCE", "current", "E2", "Promoted in R2 to DOC-DIVERGENCE UI-083 (Back backgrounds the app in an in-project sketch)")
a("H-F4", SF, "F", "hypothesis", "RC-20", None, "hypothesis", "E5", "Flip chip label clips at font 1.5/2.0")
a("H-F5", SF, "F", "hypothesis", "RC-NEW-18", None, "hypothesis", "E5", "Dimension editor Apply in back-gesture zone; not measured")
# ---- Seat G ----
a("G-01", SF, "G", "P2", "RC-13a", None, "current", "E1",
  f"{cr('CR-07')}/host-runs/clang-fma-on.out HOST_SELFTESTS_FAIL (4392 checks, 2 failed); fp_oracle_diag.out fed=0.56999999999999995 oracle=0.57000000000000006; = PORT-031; arm64 device outcome E5 (H-G-01a/b); product unaffected")
a("G-02", SF, "G", "P2", "RC-13b", None, "current", "E2", "= I-05 = OPP-31; panel 2/3")
a("G-03", SF, "G", "P3", "RC-21", "DOC_DIVERGENCE", "current", "E2", "ARCHITECTURE.md:214 ANativeWindow ownership")
a("G-04", SF, "G", "P3", "RC-13c", None, "current", "E3", "FC2 clang++ -Wall -Wextra 10 warnings; no verdict flips; = PORT-034 = OPP-25")
a("G-05", SF, "G", "P3 (R2: DOC-DIVERGENCE)", "RC-18b", None, "narrowed", "E2", "Gizmo comparison removed in R2; desktop-DPI part FUTURE-NOT-DUE")
a("G-06", SF, "G", "FUTURE-NOT-DUE", "RC-19", "FUTURE_OPPORTUNITY", "current", "E3", "Regraded R2; same root as J-03 (OD-J3 / IDEA-04)")
a("G-07", SF, "G", "INFO", "RC-NEW-19", None, "current", "E2", "FUTURE-NOT-DUE")
a("G-08", SF, "G", "INFO", "RC-NEW-20", None, "current", "E2", "FUTURE-NOT-DUE")
a("G-09", SF, "G", "INFO", "RC-NEW-15", None, "current", "E2", "= F-10")
a("H-G-01a", SF, "G", "hypothesis", "RC-13a", None, "hypothesis", "E2", "arm64 debug launch prints CONSTRUCTION_HISTORY_SELFTEST_FAIL; OWNER-PHYSICAL-UNVERIFIED")
a("H-G-01b", SF, "G", "hypothesis", "RC-13a", None, "hypothesis", "E5", "arm64 launch prints CAD_FEATURE_SELFTEST_FAIL")
a("H-G-02", SF, "G", "hypothesis", "RC-04a", None, "hypothesis", "E5", "Hardware keyboard attach recreates the Activity (configChanges lacks keyboard/navigation): a further RC-04 trigger")
a("H-G-03", SF, "G", "hypothesis", "RC-NEW-10", None, "hypothesis", "E5", "= HJ-2 = H-E3")
a("H-G-04", SF, "G", "hypothesis", "RC-NEW-21", None, "hypothesis", "E5", "No desktop surface to test")
a("H-G-05", SF, "G", "hypothesis", "RC-13a", None, "hypothesis", "E3", "Probed: 72 fixture verdicts identical under -mfma; user geometry near tolerance unproven")
# ---- Seat H ----
a("H-01", SF, "H", "P2", "RC-12", None, "current", "E2", "= RND-006 = DOC-077b = OPP-10; panel 3/3")
a("H-02", SF, "H", "P3", "RC-05a", None, "current", "E2", "Duplicate of E-13")
a("H-03", SF, "H", "P3", "RC-13d", None, "current", "E3", "CI DEVICE 40797031704 line ends 'parametric_ch'; = I-07 = RND-058 = TST-012 = OPP-24")
a("H-04", SF, "H", "P3", "RC-21", "DOC_DIVERGENCE", "current", "E2", "PROJECT_STATUS.md lists FC-R1 D5 open (fixed in 5da5d85)")
a("H-05", SF, "H", "P2 (risk)", "RC-15a", None, "current", "E3", "Host at-cap Finish 275-322 ms (FC2); no budget recorded (D-7, OD-19); panel 2/3")
a("H-06", SF, "H", "P2 (risk)", "RC-15b", None, "current", "E3", "Host 20 ms at 100k vertices (FC2); OD-19; panel 2/3")
a("H-07", SF, "H", "P2 (risk)", "RC-15c", None, "current", "E2", "Not panel-verified (seat grade only)")
a("H-08", SF, "H", "P2 (debt)", "RC-15d", None, "current", "E1",
  f"{cr('PANEL-H08-1')}/h08_diag.out: CHAIN n=32 snapshot_median_us=2551.4, n=64 10427.1 vs PLAIN 12.9/25.9 (host). Not panel-verified in FC2")
a("H-09", SF, "H", "unassigned", "NONE", None, "withdrawn", "E0", "Id never assigned (SEAT_H.md:8; delib_A_to_H O-17)")
a("H-10", SF, "H", "P3 (risk)", "RC-NEW-22", None, "current", "E2", "Threshold policy is an OD-19 sibling (minimum feature size)")
a("H-11", SF, "H", "P3", "RC-21", "DOC_DIVERGENCE", "current", "E2", "surfaceDestroyed bounded 5 s wait (jni:9487) vs CLAUDE.md 'must block'")
a("H-12", SF, "H", "unassigned", "NONE", None, "withdrawn", "E0", "Id never assigned")
a("H-13", SF, "H", "P3 (R2: PARTIAL)", "RC-13e", None, "current", "E1",
  f"{cr('CR-07')}/cap_probe.out PROJECT host=256 device_cap=256 headroom=0 AT_CAP; SIM 257th check failing still yields SELFTEST_OK (256 checks). Duplicate of I-01; latent, no check dropped today")
a("H-14", SF, "H", "P3", "RC-NEW-23", None, "current", "E3", "PASS-REPRODUCED with improvement; ~0.5 s on SwiftShader (CI DEVICE 40797031704)")
a("H-15", SF, "H", "INFO", "RC-13f", "PERFORMANCE_RISK", "current", "E3", "Debug only (~8.4 s, CAD_FEATURE 7.5 s, 40797031704); the cost of the I-02 mechanism")
a("H-16", SF, "H", "P3", "RC-21", "DOC_DIVERGENCE", "current", "E2", "forgeshape_selection.cpp:42-47, 90-94 'rigid' comments")
a("H-17", SF, "H", "P3", "RC-NEW-24", None, "current", "E2", "OPP-44")
a("HYP-H1", SF, "H", "hypothesis", "RC-NEW-25", None, "hypothesis", "E5", "Equal counts unlikely")
a("HYP-H2", SF, "H", "hypothesis (split)", "RC-12", None, "hypothesis", "E2", "E2 fact: attach never consults recovery_; visible return after RestartRequired unproven")
a("HYP-H3", SF, "H", "hypothesis", "RC-NEW-26", None, "hypothesis", "E5", "No validation-layer session since SEL-OUT-R1")
a("HYP-H4", SF, "H", "hypothesis", "RC-NEW-27", None, "hypothesis", "E5", "~38 ns/triangle host extrapolated")
# ---- Seat I ----
a("I-01", SF, "I", "P2", "RC-13e", None, "current", "E1",
  f"{cr('CR-07')}/cap_probe.out and {cr('I01-panel1')}/cap_probe.out (cap=255 drops DAR1_02_rebinding... silently, SELFTEST_OK). = TST-005 = OPP-12; latent: nothing dropped at 256. Not panel-verified in FC2")
a("I-02", SF, "I", "P2 (PARTIAL + DOC-DIVERGENCE)", "RC-13f", None, "current", "E3", "~8.4 s per Activity start on CI emulator (40797031704); OPP-32 gated OD-26")
a("I-03", SF, "I", "P2 (DOC-DIVERGENCE)", "RC-NEW-28", None, "current", "E2", "OWNER chooses triggers vs text (OD-18)")
a("I-04", SF, "I", "P3", "RC-21", "DOC_DIVERGENCE", "current", "E2", "Stale counts in six places")
a("I-05", SF, "I", "P2 (PARTIAL)", "RC-13b", None, "current", "E2", "Duplicate of G-02")
a("I-06", SF, "I", "P2 (PARTIAL)", "RC-14a", None, "current", "E3", "54/70 classes without a run on the product tree in the FC2 record; Tier-5 BLOCKED-OWNER")
a("I-07", SF, "I", "P3", "RC-13d", None, "current", "E3", "Duplicate of H-03")
a("I-08", SF, "I", "P2 (PARTIAL)", "RC-14c", None, "current", "E2", "= F-09")
a("I-09", SF, "I", "P3 (PARTIAL)", "RC-NEW-29", None, "current", "E2", "= OPP-43")
a("I-10", SF, "I", "P3 (IMPLEMENTED-UNTESTED)", "RC-13g", None, "current", "E2", "= IDEA-08")
a("I-11", SF, "I", "P3", "RC-NEW-30", None, "current", "E2", "= TST-048 = OPP-26")
a("I-12", SF, "I", "see H-I-1", "RC-NEW-31", None, "hypothesis", "E5", "Moved to Hypotheses as H-I-1")
a("I-13", SF, "I", "P3 (PARTIAL)", "RC-NEW-32", None, "current", "E3", "No false green in 40797031704; = OPP-34 part")
a("I-14", SF, "I", "INFO (PARTIAL)", "RC-13h", None, "current", "E2", "= OPP-34 part")
a("H-I-1", SF, "I", "hypothesis", "RC-NEW-31", None, "hypothesis", "E5", "assumeTrue skip inside OK (n)")
a("H-I-2", SF, "I", "hypothesis", "RC-13f", None, "hypothesis", "E5", "ANR during ~9 s synchronous self-tests")
a("H-I-3", SF, "I", "hypothesis", "RC-NEW-33", None, "hypothesis", "E3", "PREEXISTING_SHARD5_FAILURE.md")
# ---- Seat J ----
a("J-01", SF, "J", "P3 (R2: BLOCKED-OWNER + PARTIAL)", "RC-17d", None, "narrowed", "E3",
  "FC2 j1_hidden_edit; refusal question OD-J1 (H2); locked half documented behaviour (not a defect); missing solid preview PARTIAL P3")
a("J-02", SF, "J", "P3", "RC-18a", None, "current", "E3", "FC2 j3_unit_probe (0.25 -> 2.5E-4 m in mm); = ADV-066 = OPP-23; SketchModifyView part narrowed")
a("J-03", SF, "J", "P3 (R2: BLOCKED-OWNER)", "RC-19", None, "current", "E3", "FC2 j4_replay fingerprint DIFF with semantic MATCH; follows the stated rules; OD-J3")
a("J-04", SF, "J", "P3", "RC-05c", None, "current", "E2", "= ADV-013; path likelihood E5")
a("J-05", SF, "J", "P3 (R2: narrowed)", "RC-14d", None, "narrowed", "E2", "Seam-as-setup coverage PARTIAL; toggleFace javadoc DOC_DIVERGENCE; orphan natives -> OPP-42 (RC-NEW-35)")
a("J-05-RevolveAxis", SF, "J", "P3 (sub-claim)", "RC-14d", None, "withdrawn", "E3", "CadRevolveOwnerTest taps the axis through the decor view (40782861799); delib_C_to_J C-1")
a("HJ-1", SF, "J", "hypothesis", "RC-NEW-34", None, "hypothesis", "E2", "Guard absence E2, user consequence E5; OD-30")
a("HJ-2", SF, "J", "hypothesis", "RC-NEW-10", None, "hypothesis", "E5", "= H-E3 = H-G-03")
a("HJ-3", SF, "J", "hypothesis", "RC-17e", None, "hypothesis", "E2", "OD-J2")

# ---- DEFECT-CONFIRMED requirement rows (37) ----
R = [
 ("APP-007","A","P3","RC-NEW-01","current","E2","A-08"),
 ("APP-026","A","P2","RC-02","current","E1","A-04; merged MOD-049"),
 ("APP-037","A","P2","RC-06a","current","E1","A-01; merged ADV-027"),
 ("APP-039","A","P3","RC-04a","current","E1","A-05; merged DOC-049"),
 ("APP-043","A","P3","RC-04b","current","E2","A-06"),
 ("APP-057","A","P1","RC-06b","current","E1","A-02; merged ADV-026; DESIGN_HOLD pending OD-05"),
 ("APP-087","A","P3","RC-07a","current","E3","D-03/E-07; merged DOC-052, MOD-083"),
 ("CON-024","B","P2","RC-01b","current","E1","B-01; merged TST-076"),
 ("CON-025","B","P1","RC-01a","current","E1","B-02/E-01; merged DOC-067"),
 ("CON-036","B","P3","RC-NEW-03","current","E3","B-05"),
 ("CAD-013","C","P2","RC-03","current","E1","C-01; merged UI-018"),
 ("CAD-063","C","P3","RC-10b","current","E2","C-03"),
 ("MOD-021","D","P2","RC-11","current","E1","D-01"),
 ("DOC-028","E","P3","RC-16a","current","E3","E-05; merged ADV-003"),
 ("DOC-033","E","P3","RC-16b","current","E3","E-06; merged ADV-005"),
 ("DOC-044","E","P2","RC-04c","current","E1","E-02"),
 ("DOC-063","E","P3","RC-07d","current","E3","E-09/D-04 (seat D graded PARTIAL)"),
 ("DOC-084","E","P3","RC-05b","current","E2","E-14"),
 ("DOC-086","E","P3","RC-05a","current","E2","E-13/H-02; merged RND-052, ADV-022"),
 ("DOC-087","E","P3","RC-07b","current","E2","E-12"),
 ("UI-009","F","P2","RC-08a","current","E2","F-03; OWNER-SEMANTICS-REQUIRED (D-5)"),
 ("UI-010","F","P2","RC-08b","current","E2","F-04; merged CAD-115"),
 ("UI-011","F","P2","RC-20","current","E3","F-01"),
 ("UI-012","F","P2","RC-20","current","E3","F-01 (navigator Flip)"),
 ("UI-013","F","P2","RC-20","current","E3","F-01 (Outline chips)"),
 ("UI-014","F","P2","RC-20","current","E3","F-01 (dimension value chip)"),
 ("UI-024","F","P2","RC-08a","current","E2","F-03 (palettes in the Back chain)"),
 ("UI-040","F","P3","RC-NEW-12","current","E3","F-06"),
 ("UI-051","F","P2","RC-09","current","E1","F-05; merged ADV-067"),
 ("PORT-031","G","P2","RC-13a","current","E1","G-01"),
 ("PORT-034","G","P3","RC-13c","current","E3","G-04"),
 ("RND-006","H","P2","RC-12","current","E2","H-01"),
 ("RND-058","H","P3","RC-13d","current","E3","H-03; merged TST-012"),
 ("TST-005","I","P2","RC-13e","current","E1","I-01 (latent)"),
 ("TST-048","I","P3","RC-NEW-30","current","E2","I-11"),
 ("ADV-013","J","P3","RC-05c","current","E2","J-04"),
 ("ADV-066","J","P3","RC-18a","current","E3","J-02"),
]
for rid, seat, sev, rc, st, conf, note in R:
    cat = "DESIGN_HOLD" if rc in ("RC-06b", "RC-08a") else None
    a(rid, RQ, seat, sev, rc, cat, st, conf, "DEFECT-CONFIRMED row; seat alias " + note)

# ---- OPP rows (OPPORTUNITIES.md) and backlog rows without an OPP id ----
O = [
 ("OPP-01","P1","RC-01a",None),("OPP-02","P2","RC-01b;RC-01c",None),("OPP-03","P2","RC-02",None),
 ("OPP-04","P2","RC-11",None),("OPP-05","P2","RC-09",None),("OPP-06","P2","RC-03",None),
 ("OPP-07","P2","RC-20",None),("OPP-08","P2","RC-08b",None),("OPP-09","P2","RC-04c",None),
 ("OPP-10","P2","RC-12",None),("OPP-11","P2 (evidence)","RC-13a",None),("OPP-12","P2 (latent)","RC-13e",None),
 ("OPP-13","P3","RC-07a",None),("OPP-14","P3/P2","RC-04a",None),("OPP-15","P3","RC-04b",None),
 ("OPP-16","P3","RC-07b",None),("OPP-17","P3","RC-16a",None),("OPP-18","P3","RC-16b",None),
 ("OPP-19","P3","RC-05a;RC-05b;RC-05c",None),("OPP-20","P3","RC-10b",None),("OPP-21","P3","RC-10a;RC-10c",None),
 ("OPP-22","P3","RC-NEW-05",None),("OPP-23","P3","RC-18a",None),("OPP-24","P3","RC-13d",None),
 ("OPP-25","P3","RC-13c",None),("OPP-26","P3","RC-NEW-30",None),("OPP-27","P3","RC-NEW-01",None),
 ("OPP-28","P3","RC-NEW-08",None),("OPP-29","P3","RC-NEW-03",None),("OPP-30","P3","RC-NEW-02",None),
 ("OPP-G1","P1 (OWNER-FIRST OD-05)","RC-06b",None),("OPP-G2","P2 (OWNER-FIRST OD-06)","RC-06a",None),
 ("OPP-G3","P2 (OWNER-FIRST OD-08)","RC-08a",None),
 ("OPP-31","structural","RC-13b",None),("OPP-32","structural","RC-13f",None),("OPP-33","structural","RC-14c",None),
 ("OPP-34","structural","RC-13h;RC-NEW-32",None),("OPP-35","structural","RC-07a;RC-07b;RC-07c","FUTURE_OPPORTUNITY"),
 ("OPP-36","structural","RC-10a;RC-10b;RC-10c","FUTURE_OPPORTUNITY"),("OPP-37","structural","RC-08a;RC-08b","FUTURE_OPPORTUNITY"),
 ("OPP-38","structural","RC-15d",None),("OPP-39","structural","RC-15b",None),("OPP-40","structural","RC-15a",None),
 ("OPP-41","structural","RC-21",None),("OPP-42","structural","RC-NEW-35",None),("OPP-43","structural","RC-NEW-29",None),
 ("OPP-44","structural","RC-NEW-24;RC-NEW-22;RC-NEW-23","FUTURE_OPPORTUNITY"),("OPP-45","structural","RC-NEW-13",None),
]
for oid, sev, rc, cat in O:
    a(oid, OP, "-", sev, rc, cat, "current", "E2", "OPPORTUNITIES.md / PRIORITISED_BACKLOG.md entry; confidence is that of the mechanism rows it names")
for bid, sev, rc, note in [
    ("BACKLOG-C1-R17", "P2 (coverage)", "RC-14a", "PRIORITISED_BACKLOG.md:57 (C-06)"),
    ("BACKLOG-C1-R18", "P2 BLOCKED-OWNER", "RC-06c", "PRIORITISED_BACKLOG.md:58 (A-03 -> IDEA-05)"),
    ("BACKLOG-C1-R19", "P3/INFO BLOCKED-OWNER", "RC-06d", "PRIORITISED_BACKLOG.md:59 (A-07)"),
    ("IDEA-01", "idea", "RC-NEW-36", "OD-01; CADB v9 not authorised"),
    ("IDEA-02", "idea", "RC-NEW-19;RC-NEW-20", "desktop research spike"),
    ("IDEA-03", "idea", "RC-NEW-15", "Android desktop-mode input"),
    ("IDEA-04", "idea", "RC-19", "per-document allocator"),
    ("IDEA-05", "idea", "RC-06c", "= A-03"),
    ("IDEA-06", "idea", "RC-NEW-37", "stylus hover preview"),
    ("IDEA-07", "idea", "RC-15c", "draw-on-change frame pacing"),
    ("IDEA-08", "idea", "RC-13g", "release smoke / arm64 run"),
    ("IDEA-09", "idea", "RC-NEW-13", "accessibility specification"),
    ("IDEA-10", "idea", "RC-NEW-09", "write-side size limit"),
    ("IDEA-11", "idea", "RC-NEW-38", "future modelling features"),
]:
    cat = "FUTURE_OPPORTUNITY" if bid.startswith("IDEA") else None
    a(bid, BL, "-", sev, rc, cat, "current", "E4" if bid.startswith("IDEA") else "E2", note)

# ---------------------------------------------------------------------------
# Cross-checks against the dossier, then write.
# ---------------------------------------------------------------------------
tsv = list(csv.DictReader(open(FC2 / "requirements_traceability.tsv"), delimiter="\t"))
dc = {r["ID"] for r in tsv if r["verdict"] == "DEFECT-CONFIRMED"}
mapped = {r[0] for r in A if r[1] == RQ}
assert dc == mapped, ("TSV mismatch", dc ^ mapped)
ids = [r[0] for r in A]
assert len(ids) == len(set(ids)), "duplicate alias id"
for r in A:
    for rc in r[4].split(";"):
        assert rc in RC, rc

HDR = ["alias_id", "alias_kind", "seat", "severity_as_given", "root_cause_id", "root_cause_title",
       "mechanism_file_line", "category", "status", "confidence", "notes"]
with open(OUT, "w", newline="") as f:
    w = csv.writer(f, delimiter="\t", lineterminator="\n", quoting=csv.QUOTE_MINIMAL)
    w.writerow(HDR)
    for alias, kind, seat, sev, rcs, cat, st, conf, notes in A:
        rcl = rcs.split(";")
        title = " | ".join(RC[x][0] for x in rcl)
        mech = " | ".join(RC[x][1] for x in rcl)
        category = cat or RC[rcl[0]][2]
        w.writerow([alias, kind, seat, sev, rcs, title, mech, category, st, conf, notes])
print(f"wrote {OUT} rows={len(A)} defect_confirmed_rows_mapped={len(mapped)}")
