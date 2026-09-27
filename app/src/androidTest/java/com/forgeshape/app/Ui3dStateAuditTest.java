package com.forgeshape.app;

import static com.forgeshape.app.SketchTestSupport.dragSketch;
import static com.forgeshape.app.SketchTestSupport.selectTool;
import static com.forgeshape.app.SketchTestSupport.sketchState;
import static com.forgeshape.app.Ui3dAuditRecorder.MAY_SHOW;
import static com.forgeshape.app.Ui3dAuditRecorder.MUST_HIDE;
import static com.forgeshape.app.Ui3dAuditRecorder.MUST_SHOW;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;
import android.widget.EditText;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * `UI-3D-STATE-AUDIT-R1`: the whole-app interactive UI / 3D-state audit.
 *
 * <p>This suite is an AUDIT and not a product gate. It drives the shipped
 * controls and the real viewport through the state transitions the audit
 * contract names, and for every dynamic surface it records two things: whether
 * the surface was on screen when the contract matrix said it must be, and — for
 * a world- or feature-anchored surface — how far it stood from the anchor
 * native reports for it at that instant.
 *
 * <p><b>It records rather than asserts.</b> A visibility mismatch or a stale
 * anchor is a finding, and a finding is a row in
 * {@link Ui3dAuditRecorder}, not a thrown assertion — an audit that stopped at
 * the first defect would deliver a truncated matrix, which is the one outcome
 * the contract forbids. What IS asserted is the harness itself: that the
 * journey step actually happened, so no row is measured from a state that was
 * never reached.
 *
 * <p><b>The freshness rule.</b> Every expected anchor is re-read from native
 * AFTER the action, never remembered from before it. A row whose actual
 * placement matches the anchor from BEFORE the action and misses the one from
 * after it is exactly what a stale cached screen coordinate looks like.
 *
 * <p>No production behaviour is changed by anything here, and no observability
 * seam was added: every anchor comes from a read-only debug seam the repository
 * already ships ({@code bodyDimensionLabelPoint}, {@code cadExtrudeToolState},
 * {@code cadBodySketchAnchor}, {@code sketchLineDimension} plus
 * {@code sketchScreenPoint}, {@code debugProjectWorld}, {@code debugCameraPose}).
 */
@RunWith(AndroidJUnit4.class)
public final class Ui3dStateAuditTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private float density;

    @Before
    public void startFromTheBaseline() {
        resetToBaselineConstruction(rule.getScenario());
        density = onWorkspace(rule.getScenario(),
                (activity, workspace) -> activity.getResources().getDisplayMetrics().density);
    }

    @After
    public void leaveTheBaselineBehind() {
        Ui3dAuditRecorder.flush();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.setBodyDimensionsMode(false);
            NativeViewport.sketchCancel();
            NativeViewport.supportChooserCancel();
            NativeViewport.enterConstructionMode();
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
    }

    // =======================================================================
    // UI3D-03 — the show/hide lifecycle matrix
    // =======================================================================

    @Test
    public void ui3d03_showHideLifecycleMatrix() {
        // --- no project -> Home ------------------------------------------
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.showHomeAsFirstLaunchForTest();
            return null;
        });
        settleLayout();
        recordEditorChrome("UI3D-03", "home_no_project", true);
        Ui3dAuditRecorder.capture("ui3d03_01_home");

        // --- Home -> New Project chooser ---------------------------------
        press(R.id.home_new_project);
        record("UI3D-03", "new_project_chooser", "S02 new_project_chooser", MUST_SHOW,
                shown(R.id.new_project_chooser));
        record("UI3D-03", "new_project_chooser", "S01 home_surface", MAY_SHOW,
                shown(R.id.home_surface));
        Ui3dAuditRecorder.capture("ui3d03_02_new_project");

        // --- New CAD -> a volatile sketch over the empty scene ------------
        press(R.id.new_project_cad);
        recordPrecisionBody("UI3D-03", "new_cad_sketch", "S28 sketch_editor", MUST_SHOW,
                R.id.sketch_editor);
        record("UI3D-03", "new_cad_sketch", "S30 sketch_orientation_navigator", MUST_SHOW,
                shown(R.id.sketch_orientation_navigator));
        record("UI3D-03", "new_cad_sketch", "S02 new_project_chooser", MUST_HIDE,
                shown(R.id.new_project_chooser));
        record("UI3D-03", "new_cad_sketch", "S01 home_surface", MUST_HIDE,
                shown(R.id.home_surface));
        record("UI3D-03", "new_cad_sketch", "S12 add_primitive_palette", MUST_HIDE,
                shown(R.id.add_primitive_palette));
        record("UI3D-03", "new_cad_sketch", "S16 object_row_commands", MUST_HIDE,
                shown(R.id.object_row_commands));
        record("UI3D-03", "new_cad_sketch", "S35 cad_extrude cluster", MUST_HIDE,
                shown(R.id.cad_extrude_depth_value));
        record("UI3D-03", "new_cad_sketch", "S19 body_dimension_label_x", MUST_HIDE,
                shown(R.id.body_dimension_label_x));
        Ui3dAuditRecorder.capture("ui3d03_03_first_sketch");

        // --- a rectangle, then Finish Sketch -> the staged extrusion ------
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), -0.6, -0.4, 0.6, 0.4);
        press(R.id.finish_sketch);
        assertEquals("Finish Sketch reached Ready", NativeViewport.SKETCH_READY, sketchState());
        record("UI3D-03", "sketch_ready", "S35 cad_extrude cluster", MUST_SHOW,
                shown(R.id.cad_extrude_depth_value));
        // `CAD-VERTICAL-SLICE-R1`: ONE extent control stands at rest and opens
        // the three choices; Ready withdraws the drawing chrome.
        record("UI3D-03", "sketch_ready", "S35 cad_extrude_extent", MUST_SHOW,
                shown(R.id.cad_extrude_extent));
        record("UI3D-03", "sketch_ready", "S30 sketch_orientation_navigator", MUST_HIDE,
                shown(R.id.sketch_orientation_navigator));
        record("UI3D-03", "sketch_ready", "S07 tool_rail_rectangle", MUST_HIDE,
                shown(R.id.tool_rail_rectangle));
        record("UI3D-03", "sketch_ready", "S35 cad_extrude_flip", MUST_SHOW,
                shown(R.id.cad_extrude_flip));
        record("UI3D-03", "sketch_ready", "S36 cad_extrude_second_value", MUST_HIDE,
                shown(R.id.cad_extrude_second_value));
        record("UI3D-03", "sketch_ready", "S38 cad_canvas_edit_sketch", MUST_HIDE,
                shown(R.id.cad_canvas_edit_sketch));
        record("UI3D-03", "sketch_ready", "S29 sketch_profile_chooser", MAY_SHOW,
                shown(R.id.sketch_profile_chooser));
        Ui3dAuditRecorder.capture("ui3d03_04_staged_one_side");

        // --- One Side -> Symmetric -> Two Sides ---------------------------
        press(R.id.cad_extrude_extent_symmetric);
        record("UI3D-03", "extent_symmetric", "S35 cad_extrude_flip", MUST_HIDE,
                shown(R.id.cad_extrude_flip));
        record("UI3D-03", "extent_symmetric", "S36 cad_extrude_second_value", MUST_HIDE,
                shown(R.id.cad_extrude_second_value));
        Ui3dAuditRecorder.capture("ui3d03_05_symmetric");

        press(R.id.cad_extrude_extent_two_sides);
        record("UI3D-03", "extent_two_sides", "S36 cad_extrude_second_value", MUST_SHOW,
                shown(R.id.cad_extrude_second_value));
        record("UI3D-03", "extent_two_sides", "S35 cad_extrude_flip", MUST_HIDE,
                shown(R.id.cad_extrude_flip));
        Ui3dAuditRecorder.capture("ui3d03_06_two_sides");

        press(R.id.cad_extrude_extent_one_side);
        record("UI3D-03", "extent_back_to_one_side", "S36 cad_extrude_second_value", MUST_HIDE,
                shown(R.id.cad_extrude_second_value));
        record("UI3D-03", "extent_back_to_one_side", "S35 cad_extrude_flip", MUST_SHOW,
                shown(R.id.cad_extrude_flip));

        // --- Apply Extrude -> a committed body ----------------------------
        commitExtrude("0.4");
        assertTrue("the extrusion created a CAD body", NativeViewport.sceneActiveBodyIsCad());
        record("UI3D-03", "extrude_committed", "S35 cad_extrude cluster", MUST_HIDE,
                shown(R.id.cad_extrude_depth_value));
        record("UI3D-03", "extrude_committed", "S36 cad_extrude_second_value", MUST_HIDE,
                shown(R.id.cad_extrude_second_value));
        record("UI3D-03", "extrude_committed", "S28 sketch_editor", MUST_HIDE,
                shown(R.id.sketch_editor));
        record("UI3D-03", "extrude_committed", "S30 sketch_orientation_navigator", MUST_HIDE,
                shown(R.id.sketch_orientation_navigator));
        record("UI3D-03", "extrude_committed", "S31 sketch_dimension_label", MUST_HIDE,
                shown(R.id.sketch_dimension_label));
        record("UI3D-03", "extrude_committed", "S38 cad_canvas_edit_sketch", MUST_SHOW,
                shown(R.id.cad_canvas_edit_sketch));
        Ui3dAuditRecorder.capture("ui3d03_07_committed_body");

        // --- Edit Sketch -> the staged sketch again ------------------------
        press(R.id.cad_canvas_edit_sketch);
        recordPrecisionBody("UI3D-03", "edit_sketch", "S28 sketch_editor", MUST_SHOW,
                R.id.sketch_editor);
        record("UI3D-03", "edit_sketch", "S38 cad_canvas_edit_sketch", MUST_HIDE,
                shown(R.id.cad_canvas_edit_sketch));
        record("UI3D-03", "edit_sketch", "S16 object_row_commands", MUST_HIDE,
                shown(R.id.object_row_commands));
        Ui3dAuditRecorder.capture("ui3d03_08_edit_sketch");

        // --- Cancel the edit: nothing of it may survive --------------------
        press(R.id.cancel_sketch);
        record("UI3D-03", "edit_sketch_cancelled", "S28 sketch_editor", MUST_HIDE,
                shown(R.id.sketch_editor));
        record("UI3D-03", "edit_sketch_cancelled", "S30 sketch_orientation_navigator", MUST_HIDE,
                shown(R.id.sketch_orientation_navigator));
        record("UI3D-03", "edit_sketch_cancelled", "S31 sketch_dimension_label", MUST_HIDE,
                shown(R.id.sketch_dimension_label));
        record("UI3D-03", "edit_sketch_cancelled", "S35 cad_extrude cluster", MUST_HIDE,
                shown(R.id.cad_extrude_depth_value));
        record("UI3D-03", "edit_sketch_cancelled", "S38 cad_canvas_edit_sketch", MUST_SHOW,
                shown(R.id.cad_canvas_edit_sketch));
        Ui3dAuditRecorder.capture("ui3d03_09_after_cancel");
    }

    // =======================================================================
    // UI3D-04 — Construction Body Dimensions under a body transform
    // =======================================================================

    @Test
    public void ui3d04_dimensionsUnderBodyTransform() {
        enterTransformTool();
        press(R.id.body_dimensions);
        assertTrue("Dimensions mode opened", dimensionsModeOpen());
        record("UI3D-04", "dimensions_open", "S19 body_dimension_label_x", MUST_SHOW,
                shown(R.id.body_dimension_label_x));
        record("UI3D-04", "dimensions_open", "S22 dimension_anchor_group", MUST_SHOW,
                shown(R.id.dimension_anchor_group));
        record("UI3D-04", "dimensions_open", "S24 transform gizmo (native)", MUST_HIDE,
                gizmoActive());
        measureDimensionLabels("UI3D-04", "dimensions_open", "ui3d04_01_dimensions_open");

        // --- Move, through the exact placement editor (a real control) -----
        applyTransform(1.4, 0.6, -0.9, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
        record("UI3D-04", "after_move", "S19 body_dimension_label_x", MUST_SHOW,
                shown(R.id.body_dimension_label_x));
        measureDimensionLabels("UI3D-04", "after_move", "ui3d04_02_after_move");

        // --- Rotate --------------------------------------------------------
        applyTransform(1.4, 0.6, -0.9, 25.0, 40.0, 15.0, 1.0, 1.0, 1.0);
        measureDimensionLabels("UI3D-04", "after_rotate", "ui3d04_03_after_rotate");

        // --- Scale ---------------------------------------------------------
        applyTransform(1.4, 0.6, -0.9, 25.0, 40.0, 15.0, 1.8, 0.7, 2.2);
        measureDimensionLabels("UI3D-04", "after_scale", "ui3d04_04_after_scale");

        // --- A resize typed at the label itself, on a one-sided anchor,
        //     which is the one path in this mode that MOVES the body ---------
        press(R.id.dimension_anchor_negative);
        typeDimension(0, "3.5");
        record("UI3D-04", "after_typed_dimension", "S19 body_dimension_label_x", MUST_SHOW,
                shown(R.id.body_dimension_label_x));
        measureDimensionLabels("UI3D-04", "after_typed_dimension",
                "ui3d04_05_after_typed_dimension");

        // --- Undo, which puts the body back --------------------------------
        press(R.id.undo_action);
        measureDimensionLabels("UI3D-04", "after_undo", "ui3d04_06_after_undo");

        press(R.id.body_dimensions);
        record("UI3D-04", "dimensions_closed", "S19 body_dimension_label_x", MUST_HIDE,
                shown(R.id.body_dimension_label_x));
        record("UI3D-04", "dimensions_closed", "S22 dimension_anchor_group", MUST_HIDE,
                shown(R.id.dimension_anchor_group));
        record("UI3D-04", "dimensions_closed", "S24 transform gizmo (native)", MUST_SHOW,
                gizmoActive());
    }

    // =======================================================================
    // UI3D-05 / UI3D-14 — the same labels under camera motion
    // =======================================================================

    @Test
    public void ui3d05_dimensionsUnderCameraMotion() {
        enterTransformTool();
        press(R.id.body_dimensions);
        assertTrue("Dimensions mode opened", dimensionsModeOpen());
        settleLayout();
        measureDimensionLabels("UI3D-05", "before_camera", "ui3d05_01_before_camera");

        // --- ORBIT, through a real one-finger viewport drag ----------------
        //
        // The gizmo is withdrawn in this mode, so a single finger in the
        // viewport is unambiguously an orbit — which is exactly the gesture a
        // user makes to look at the body they are measuring.
        final float[] poseBefore = cameraPose();
        final float[] anchorBefore = dimensionAnchor(0);
        orbitViewport();
        final float[] poseAfter = cameraPose();
        assertTrue("the orbit actually moved the camera",
                Math.abs(poseAfter[0] - poseBefore[0]) > 0.01f
                        || Math.abs(poseAfter[1] - poseBefore[1]) > 0.01f);
        final float[] anchorAfter = dimensionAnchor(0);
        Ui3dAuditRecorder.note("UI3D-05", "orbit moved the X anchor from ("
                + anchorBefore[0] + ", " + anchorBefore[1] + ") to (" + anchorAfter[0] + ", "
                + anchorAfter[1] + ")");
        record("UI3D-05", "after_orbit", "S19 body_dimension_label_x", MUST_SHOW,
                shown(R.id.body_dimension_label_x));
        measureDimensionLabels("UI3D-05", "after_orbit", "ui3d05_02_after_orbit");

        // Does a later chrome act repair it? That separates "never refreshed"
        // from "refreshed by the wrong driver", which is a different repair.
        syncChrome();
        measureDimensionLabels("UI3D-05", "after_orbit_then_chrome_sync",
                "ui3d05_03_after_chrome_sync");

        // --- PAN, two fingers ----------------------------------------------
        panViewport();
        measureDimensionLabels("UI3D-05", "after_pan", "ui3d05_04_after_pan");
        syncChrome();
        measureDimensionLabels("UI3D-05", "after_pan_then_chrome_sync", null);

        // --- ZOOM out, then in ---------------------------------------------
        pinchViewport(true);
        measureDimensionLabels("UI3D-05", "after_zoom_out", "ui3d05_05_after_zoom_out");
        syncChrome();
        pinchViewport(false);
        measureDimensionLabels("UI3D-05", "after_zoom_in", "ui3d05_06_after_zoom_in");
        syncChrome();
        measureDimensionLabels("UI3D-05", "after_zoom_then_chrome_sync", null);

        press(R.id.body_dimensions);
    }

    // =======================================================================
    // UI3D-06 / UI3D-12 — selection, visibility, lock, delete, undo, redo
    // =======================================================================

    @Test
    public void ui3d06_selectionAndObjectStateInvalidation() {
        final long first = NativeViewport.sceneActiveBodyId();
        final long second = addBody();
        assertTrue("a second body exists", second != NativeViewport.NO_OBJECT
                && second != first);

        enterTransformTool();
        press(R.id.body_dimensions);
        final boolean openOnSecond = dimensionsModeOpen();
        record("UI3D-06", "dimensions_on_body_B", "S19 body_dimension_label_x",
                openOnSecond ? MUST_SHOW : MUST_HIDE, shown(R.id.body_dimension_label_x));
        measureDimensionLabels("UI3D-06", "dimensions_on_body_B", "ui3d06_01_body_b");

        // --- switch A <- B: nothing of B may stay attached to A -------------
        selectBody(first);
        record("UI3D-06", "switched_to_body_A", "S19 body_dimension_label_x", MAY_SHOW,
                shown(R.id.body_dimension_label_x));
        Ui3dAuditRecorder.note("UI3D-06", "after switching to body A the Dimensions mode is "
                + (dimensionsModeOpen() ? "still open" : "closed"));
        measureDimensionLabels("UI3D-06", "switched_to_body_A", "ui3d06_02_body_a");

        // --- hide the active body ------------------------------------------
        setVisible(first, false);
        record("UI3D-06", "active_body_hidden", "S19 body_dimension_label_x", MUST_HIDE,
                shown(R.id.body_dimension_label_x));
        Ui3dAuditRecorder.note("UI3D-06", "hidden body: Dimensions mode is "
                + (dimensionsModeOpen() ? "still open" : "closed"));
        Ui3dAuditRecorder.capture("ui3d06_03_hidden");
        setVisible(first, true);

        // --- lock the active body -------------------------------------------
        setLocked(first, true);
        record("UI3D-06", "active_body_locked", "S24 transform gizmo (native)", MUST_HIDE,
                gizmoActiveInTransform());
        record("UI3D-06", "active_body_locked", "S19 body_dimension_label_x", MUST_HIDE,
                shown(R.id.body_dimension_label_x));
        Ui3dAuditRecorder.capture("ui3d06_04_locked");
        setLocked(first, false);

        // --- Delete -> Undo -> Redo -----------------------------------------
        selectBody(second);
        enterTransformTool();
        press(R.id.body_dimensions);
        final boolean labelsBeforeDelete = shown(R.id.body_dimension_label_x);
        deleteBody(second);
        record("UI3D-06", "after_delete", "S19 body_dimension_label_x", MUST_HIDE,
                shown(R.id.body_dimension_label_x));
        Ui3dAuditRecorder.note("UI3D-06", "labels were "
                + (labelsBeforeDelete ? "shown" : "hidden") + " before the Delete");
        Ui3dAuditRecorder.capture("ui3d06_05_after_delete");

        press(R.id.undo_action);
        record("UI3D-06", "after_undo_of_delete", "S08 objects_capsule", MUST_SHOW,
                shown(R.id.objects_capsule));
        Ui3dAuditRecorder.capture("ui3d06_06_after_undo");
        press(R.id.redo_action);
        record("UI3D-06", "after_redo_of_delete", "S19 body_dimension_label_x", MUST_HIDE,
                shown(R.id.body_dimension_label_x));
    }

    // =======================================================================
    // UI3D-07 — the CAD selected-Line dimension
    // =======================================================================

    @Test
    public void ui3d07_cadSelectedLineDimension() {
        beginSketchXy();
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        dragSketch(rule.getScenario(), -0.8, -0.5, 0.8, -0.5);
        settleLayout();

        record("UI3D-07", "line_drawn", "S31 sketch_dimension_label", MAY_SHOW,
                shown(R.id.sketch_dimension_label));

        // Select the line the way a finger does: the Select tool, then a tap at
        // the pixel the line's own midpoint projects to.
        selectTool(rule.getScenario(), R.id.tool_rail_select);
        SketchTestSupport.tapSketch(rule.getScenario(), 0.0, -0.5);
        settleLayout();

        final boolean labelShown = shown(R.id.sketch_dimension_label);
        record("UI3D-07", "line_selected", "S31 sketch_dimension_label", MUST_SHOW, labelShown);
        Ui3dAuditRecorder.note("UI3D-07", "native reports a line dimension: "
                + NativeViewport.sketchLineDimension(new double[
                        NativeViewport.SKETCH_DIMENSION_SIZE]));
        measureSketchDimension("UI3D-07", "line_selected", "ui3d07_01_line_selected");

        if (labelShown) {
            // Edit the length through the label's own field.
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                workspace.findViewById(R.id.sketch_dimension_value).performClick();
                return null;
            });
            settleLayout();
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                final EditText field = workspace.findViewById(R.id.field_sketch_line_length);
                if (field != null) {
                    field.setText("1.0");
                    workspace.findViewById(R.id.apply_sketch_line_length).performClick();
                }
                return null;
            });
            settleLayout();
            record("UI3D-07", "length_applied", "S31 sketch_dimension_label", MUST_SHOW,
                    shown(R.id.sketch_dimension_label));
            measureSketchDimension("UI3D-07", "length_applied", "ui3d07_02_length_applied");

            // Pan the sketch view: the anchor moves, and the label must follow.
            panViewport();
            measureSketchDimension("UI3D-07", "after_pan", "ui3d07_03_after_pan");
        }

        // Deselect by choosing another tool: the annotation belongs to the
        // selection and to nothing else.
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        record("UI3D-07", "tool_changed", "S31 sketch_dimension_label", MAY_SHOW,
                shown(R.id.sketch_dimension_label));
        Ui3dAuditRecorder.capture("ui3d07_04_tool_changed");

        press(R.id.cancel_sketch);
        record("UI3D-07", "sketch_cancelled", "S31 sketch_dimension_label", MUST_HIDE,
                shown(R.id.sketch_dimension_label));
    }

    // =======================================================================
    // UI3D-08 / UI3D-09 — the staged extrusion under drag and camera motion
    // =======================================================================

    @Test
    public void ui3d0809_stagedExtrudeAttachment() {
        beginSketchXy();
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), -0.7, -0.5, 0.7, 0.5);
        press(R.id.finish_sketch);
        assertEquals("Finish Sketch reached Ready", NativeViewport.SKETCH_READY, sketchState());

        measureExtrudeCluster("UI3D-08", "one_side_staged", "ui3d08_01_one_side");

        // --- a REAL drag of the arrow, from native's own projected tip -----
        final double[] before = extrudeState();
        final double depthBefore = before[NativeViewport.CAD_EXTRUDE_DEPTH];
        dragExtrudeArrow(1);
        final double[] afterDrag = extrudeState();
        Ui3dAuditRecorder.note("UI3D-08", "arrow drag moved the depth from " + depthBefore
                + " to " + afterDrag[NativeViewport.CAD_EXTRUDE_DEPTH]);
        measureExtrudeCluster("UI3D-08", "after_arrow_drag", "ui3d08_02_after_drag");

        // --- camera motion in the feature-preview view ---------------------
        orbitViewport();
        measureExtrudeCluster("UI3D-08", "after_orbit", "ui3d08_03_after_orbit");
        pinchViewport(true);
        measureExtrudeCluster("UI3D-08", "after_zoom_out", "ui3d08_04_after_zoom_out");
        final double scaleFar = extrudeState()[NativeViewport.CAD_EXTRUDE_SCALE];
        pinchViewport(false);
        measureExtrudeCluster("UI3D-08", "after_zoom_in", "ui3d08_05_after_zoom_in");
        final double scaleNear = extrudeState()[NativeViewport.CAD_EXTRUDE_SCALE];
        Ui3dAuditRecorder.note("UI3D-08", "camera-attached scale far=" + scaleFar + " near="
                + scaleNear + " (it must shrink as the camera pulls back and saturate in"
                + " [0.80, 1.60])");

        // --- Symmetric ------------------------------------------------------
        press(R.id.cad_extrude_extent_symmetric);
        record("UI3D-09", "symmetric", "S36 cad_extrude_second_value", MUST_HIDE,
                shown(R.id.cad_extrude_second_value));
        measureExtrudeCluster("UI3D-09", "symmetric", "ui3d09_01_symmetric");

        // --- Two Sides -------------------------------------------------------
        press(R.id.cad_extrude_extent_two_sides);
        record("UI3D-09", "two_sides", "S36 cad_extrude_second_value", MUST_SHOW,
                shown(R.id.cad_extrude_second_value));
        measureExtrudeCluster("UI3D-09", "two_sides", "ui3d09_02_two_sides");
        measureSecondCluster("UI3D-09", "two_sides", "ui3d09_03_two_sides_side_b");

        // Drag each side, then look again: the two values must not swap places.
        dragExtrudeArrow(1);
        measureExtrudeCluster("UI3D-09", "two_sides_after_side_a_drag", null);
        measureSecondCluster("UI3D-09", "two_sides_after_side_a_drag", null);
        dragExtrudeArrow(-1);
        measureExtrudeCluster("UI3D-09", "two_sides_after_side_b_drag",
                "ui3d09_04_after_side_b_drag");
        measureSecondCluster("UI3D-09", "two_sides_after_side_b_drag", null);

        // Zero one side by typing it, if the contract accepts it.
        typeSecondDistance("0");
        final double[] zeroed = extrudeState();
        Ui3dAuditRecorder.note("UI3D-09", "after typing 0 into Side B: positive="
                + zeroed[NativeViewport.CAD_EXTRUDE_POSITIVE] + " negative="
                + zeroed[NativeViewport.CAD_EXTRUDE_NEGATIVE] + " extent="
                + zeroed[NativeViewport.CAD_EXTRUDE_EXTENT]);
        record("UI3D-09", "side_b_zeroed", "S36 cad_extrude_second_value", MAY_SHOW,
                shown(R.id.cad_extrude_second_value));
        measureSecondCluster("UI3D-09", "side_b_zeroed", "ui3d09_05_side_b_zero");

        // Back to One Side: no stale second arrow or second value may survive.
        press(R.id.cad_extrude_extent_one_side);
        record("UI3D-09", "back_to_one_side", "S36 cad_extrude_second_value", MUST_HIDE,
                shown(R.id.cad_extrude_second_value));
        record("UI3D-09", "back_to_one_side", "S37 cad_extrude_second_editor", MUST_HIDE,
                shown(R.id.cad_extrude_second_editor));
        Ui3dAuditRecorder.capture("ui3d09_06_back_to_one_side");

        press(R.id.cancel_sketch);
    }

    // =======================================================================
    // UI3D-10 — the retained Edit Sketch chip
    // =======================================================================

    @Test
    public void ui3d10_retainedEditSketchLifecycle() {
        beginSketchXy();
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), -0.6, -0.4, 0.6, 0.4);
        press(R.id.finish_sketch);
        commitExtrude("0.5");
        assertTrue("a CAD body was created", NativeViewport.sceneActiveBodyIsCad());

        record("UI3D-10", "committed", "S35 cad_extrude cluster", MUST_HIDE,
                shown(R.id.cad_extrude_depth_value));
        record("UI3D-10", "committed", "S38 cad_canvas_edit_sketch", MUST_SHOW,
                shown(R.id.cad_canvas_edit_sketch));
        measureEditSketchChip("UI3D-10", "committed", "ui3d10_01_committed");

        // --- move / rotate / scale the committed body ----------------------
        enterTransformTool();
        applyTransform(1.1, 0.4, -0.7, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
        measureEditSketchChip("UI3D-10", "after_move", "ui3d10_02_after_move");
        applyTransform(1.1, 0.4, -0.7, 20.0, 35.0, 10.0, 1.0, 1.0, 1.0);
        measureEditSketchChip("UI3D-10", "after_rotate", "ui3d10_03_after_rotate");
        applyTransform(1.1, 0.4, -0.7, 20.0, 35.0, 10.0, 1.6, 1.3, 0.8);
        measureEditSketchChip("UI3D-10", "after_scale", "ui3d10_04_after_scale");

        // --- a REAL gizmo move drag, and then camera motion ----------------
        dragGizmoAxis(NativeViewport.GIZMO_HANDLE_AXIS_X);
        measureEditSketchChip("UI3D-10", "after_gizmo_drag", "ui3d10_05_after_gizmo_drag");
        orbitViewport();
        measureEditSketchChip("UI3D-10", "after_orbit", "ui3d10_06_after_orbit");
        pinchViewport(true);
        measureEditSketchChip("UI3D-10", "after_zoom_out", "ui3d10_07_after_zoom");
        pinchViewport(false);

        // --- Edit Sketch removes committed-body context --------------------
        press(R.id.cad_canvas_edit_sketch);
        recordPrecisionBody("UI3D-10", "edit_sketch", "S28 sketch_editor", MUST_SHOW,
                R.id.sketch_editor);
        record("UI3D-10", "edit_sketch", "S38 cad_canvas_edit_sketch", MUST_HIDE,
                shown(R.id.cad_canvas_edit_sketch));
        record("UI3D-10", "edit_sketch", "S39 cad_editor", MUST_HIDE, shown(R.id.cad_editor));
        record("UI3D-10", "edit_sketch", "S24 transform gizmo (native)", MUST_HIDE,
                gizmoActive());
        Ui3dAuditRecorder.capture("ui3d10_08_edit_sketch");

        // --- Finish again restores the feature preview for the SAME body ---
        final long editing = NativeViewport.sketchEditingBodyId();
        press(R.id.finish_sketch);
        record("UI3D-10", "finish_again", "S35 cad_extrude cluster", MUST_SHOW,
                shown(R.id.cad_extrude_depth_value));
        Ui3dAuditRecorder.note("UI3D-10", "the sketch being edited belongs to body " + editing);
        measureExtrudeCluster("UI3D-10", "finish_again", "ui3d10_09_finish_again");

        press(R.id.cancel_sketch);
        record("UI3D-10", "cancelled_edit", "S38 cad_canvas_edit_sketch", MUST_SHOW,
                shown(R.id.cad_canvas_edit_sketch));
        record("UI3D-10", "cancelled_edit", "S35 cad_extrude cluster", MUST_HIDE,
                shown(R.id.cad_extrude_depth_value));
    }

    // =======================================================================
    // UI3D-11 — Sculpt, and what may not leak into or out of it
    // =======================================================================

    @Test
    public void ui3d11_sculptModeLeakage() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionSphere(1.2);
            NativeViewport.setSculptBrush(320.0, 1.0);
            workspace.syncFromNative();
            return null;
        });
        settleLayout();

        enterTransformTool();
        press(R.id.body_dimensions);
        final boolean dimsOpenBefore = dimensionsModeOpen();
        Ui3dAuditRecorder.note("UI3D-11", "Dimensions mode before entering Sculpt: "
                + dimsOpenBefore);

        press(R.id.freeze_to_sculpt);
        assertEquals("Sculpt was entered", NativeViewport.MODE_SCULPT, NativeViewport.productMode());
        record("UI3D-11", "sculpt_entered", "S19 body_dimension_label_x", MUST_HIDE,
                shown(R.id.body_dimension_label_x));
        diagnoseDimensionLabels("UI3D-11", "sculpt_entered");
        record("UI3D-11", "sculpt_entered", "S22 dimension_anchor_group", MUST_HIDE,
                shown(R.id.dimension_anchor_group));
        record("UI3D-11", "sculpt_entered", "S24 transform gizmo (native)", MUST_HIDE,
                gizmoActive());
        record("UI3D-11", "sculpt_entered", "S25 transform_mode_group", MUST_HIDE,
                shown(R.id.transform_mode_group));
        record("UI3D-11", "sculpt_entered", "S12 add_primitive_palette", MUST_HIDE,
                shown(R.id.add_primitive_palette));
        record("UI3D-11", "sculpt_entered", "S35 cad_extrude cluster", MUST_HIDE,
                shown(R.id.cad_extrude_depth_value));
        record("UI3D-11", "sculpt_entered", "S38 cad_canvas_edit_sketch", MUST_HIDE,
                shown(R.id.cad_canvas_edit_sketch));
        record("UI3D-11", "sculpt_entered", "S40 brush_edge_controls", MUST_SHOW,
                shown(R.id.brush_edge_controls));
        record("UI3D-11", "sculpt_entered", "S07 tool_rail_grab", MUST_SHOW,
                shown(R.id.tool_rail_grab));
        record("UI3D-11", "sculpt_entered", "S07 tool_rail_mask", MUST_SHOW,
                shown(R.id.tool_rail_mask));
        Ui3dAuditRecorder.capture("ui3d11_01_sculpt");

        // --- a real stroke, then the History navigator ----------------------
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        record("UI3D-11", "after_stroke", "S09 history_navigator_action", MUST_SHOW,
                shown(R.id.history_navigator_action));
        press(R.id.history_navigator_action);
        record("UI3D-11", "navigator_open", "S42 history_navigator", MUST_SHOW,
                shown(R.id.history_navigator));
        Ui3dAuditRecorder.capture("ui3d11_02_navigator");
        press(R.id.history_navigator_action);
        record("UI3D-11", "navigator_closed", "S42 history_navigator", MUST_HIDE,
                shown(R.id.history_navigator));

        // --- Mask / Clear Mask ---------------------------------------------
        record("UI3D-11", "before_mask", "S41 clear_mask", MAY_SHOW, shown(R.id.clear_mask));
        selectTool(rule.getScenario(), R.id.tool_rail_mask);
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        syncChrome();
        Ui3dAuditRecorder.note("UI3D-11", "native says Clear Mask is available: "
                + NativeViewport.sculptCanClearMask());
        recordPrecisionBody("UI3D-11", "after_mask_paint", "S41 clear_mask",
                NativeViewport.sculptCanClearMask() ? MUST_SHOW : MAY_SHOW, R.id.clear_mask);
        Ui3dAuditRecorder.capture("ui3d11_03_mask");
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openPrecision(workspace);
            return null;
        });
        settleLayout();
        if (shown(R.id.clear_mask)) {
            press(R.id.clear_mask);
            syncChrome();
            record("UI3D-11", "after_clear_mask+precision_open", "S41 clear_mask",
                    NativeViewport.sculptCanClearMask() ? MAY_SHOW : MUST_HIDE,
                    shown(R.id.clear_mask));
        }
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.closePrecision(workspace);
            return null;
        });
        settleLayout();

        // --- Back to Construction, then Resume -----------------------------
        selectTool(rule.getScenario(), R.id.tool_rail_grab);
        press(R.id.back_to_construction);
        assertEquals("Construction was re-entered", NativeViewport.MODE_CONSTRUCTION,
                NativeViewport.productMode());
        record("UI3D-11", "back_to_construction", "S40 brush_edge_controls", MUST_HIDE,
                shown(R.id.brush_edge_controls));
        record("UI3D-11", "back_to_construction", "S42 history_navigator", MUST_HIDE,
                shown(R.id.history_navigator));
        record("UI3D-11", "back_to_construction", "S19 body_dimension_label_x", MUST_HIDE,
                shown(R.id.body_dimension_label_x));
        record("UI3D-11", "back_to_construction", "S41 clear_mask", MUST_HIDE,
                shown(R.id.clear_mask));
        Ui3dAuditRecorder.capture("ui3d11_04_back_to_construction");

        press(R.id.resume_sculpt);
        record("UI3D-11", "resume_sculpt", "S40 brush_edge_controls", MUST_SHOW,
                shown(R.id.brush_edge_controls));
        record("UI3D-11", "resume_sculpt", "S19 body_dimension_label_x", MUST_HIDE,
                shown(R.id.body_dimension_label_x));
        record("UI3D-11", "resume_sculpt", "S24 transform gizmo (native)", MUST_HIDE,
                gizmoActive());
        Ui3dAuditRecorder.capture("ui3d11_05_resume");
        press(R.id.back_to_construction);
    }

    // =======================================================================
    // UI3D-13 — global dismissal: Back, Cancel, Apply, context switches
    // =======================================================================

    @Test
    public void ui3d13_globalDismissalAndPrimarySurfaceExclusivity() {
        enterTransformTool();

        // --- only one primary surface at a time ----------------------------
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openObjectsPanel(workspace);
            return null;
        });
        settleLayout();
        record("UI3D-13", "objects_open", "S11 objects_popover", MAY_SHOW,
                shown(R.id.objects_popover));

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openPrecision(workspace);
            return null;
        });
        settleLayout();
        record("UI3D-13", "precision_open", "S13 property_inspector", MUST_SHOW,
                shown(R.id.property_inspector));
        record("UI3D-13", "precision_open", "S11 objects_popover", MUST_HIDE,
                shown(R.id.objects_popover));

        press(R.id.display_settings_button);
        record("UI3D-13", "display_open", "S14 display_settings_popover", MUST_SHOW,
                shown(R.id.display_settings_popover));
        record("UI3D-13", "display_open", "S13 property_inspector", MUST_HIDE,
                shown(R.id.property_inspector));
        Ui3dAuditRecorder.capture("ui3d13_01_one_primary");

        // --- System Back closes the topmost, and the app stays --------------
        systemBack();
        record("UI3D-13", "after_system_back", "S14 display_settings_popover", MUST_HIDE,
                shown(R.id.display_settings_popover));

        // --- Dimensions dismisses the precision surface ---------------------
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openPrecision(workspace);
            return null;
        });
        settleLayout();
        press(R.id.body_dimensions);
        record("UI3D-13", "dimensions_open", "S13 property_inspector", MUST_HIDE,
                shown(R.id.property_inspector));
        record("UI3D-13", "dimensions_open", "S19 body_dimension_label_x", MUST_SHOW,
                shown(R.id.body_dimension_label_x));

        // --- Relative Scale closes Dimensions -------------------------------
        press(R.id.body_relative_scale);
        record("UI3D-13", "relative_scale_open", "S19 body_dimension_label_x", MUST_HIDE,
                shown(R.id.body_dimension_label_x));
        record("UI3D-13", "relative_scale_open", "S22 dimension_anchor_group", MUST_HIDE,
                shown(R.id.dimension_anchor_group));
        record("UI3D-13", "relative_scale_open", "S23 relative_scale_editor", MUST_SHOW,
                shown(R.id.relative_scale_editor));
        Ui3dAuditRecorder.capture("ui3d13_02_relative_scale");
        press(R.id.body_relative_scale);

        // --- the Shape entry withdraws the Transform members ---------------
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.tool_rail_shape).performClick();
            return null;
        });
        settleLayout();
        record("UI3D-13", "shape_tool", "S25 transform_mode_group", MUST_HIDE,
                shown(R.id.transform_mode_group));
        record("UI3D-13", "shape_tool", "S22 body_dimensions entry", MUST_HIDE,
                shown(R.id.body_dimensions));
        record("UI3D-13", "shape_tool", "S24 transform gizmo (native)", MUST_HIDE,
                gizmoActive());
        Ui3dAuditRecorder.capture("ui3d13_03_shape_tool");
    }

    // =======================================================================
    // Measurement
    // =======================================================================

    /**
     * Records why a dimension label is or is not on screen, and what the
     * overlay container's own padding is.
     *
     * <p>The padding matters because every world-anchored chrome view is a
     * child of that padded container while the anchor native reports is in
     * VIEWPORT pixels: if the two disagree, the difference is a constant, and a
     * constant is worth naming rather than describing as "about 128 px".
     */
    private void diagnoseDimensionLabels(final String caseId, final String action) {
        final double[] state = new double[NativeViewport.BODY_DIM_SIZE];
        NativeViewport.bodyDimensionsState(state);
        final float[] point = new float[2];
        final StringBuilder projects = new StringBuilder();
        for (int axis = 0; axis < 3; axis++) {
            projects.append(NativeViewport.bodyDimensionLabelPoint(axis, point) ? "1" : "0");
        }
        final String chrome = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View labels = workspace.findViewById(R.id.body_dimension_labels);
            final View x = workspace.findViewById(R.id.body_dimension_label_x);
            final View parent = labels == null ? null : (View) labels.getParent();
            return "container=" + (labels == null ? "absent"
                    : (labels.getVisibility() == View.VISIBLE ? "VISIBLE" : "GONE"))
                    + " containerShown=" + (labels != null && labels.isShown())
                    + " labelX=" + (x == null ? "absent"
                    : (x.getVisibility() == View.VISIBLE ? "VISIBLE" : "GONE"))
                    + " overlayPadding=" + (parent == null ? "?"
                    : parent.getPaddingLeft() + "," + parent.getPaddingTop() + ","
                            + parent.getPaddingRight() + "," + parent.getPaddingBottom());
        });
        Ui3dAuditRecorder.note(caseId, action + ": modeActive="
                + state[NativeViewport.BODY_DIM_MODE_ACTIVE] + " measurable="
                + state[NativeViewport.BODY_DIM_MEASURABLE] + " anchorProjectsXYZ=" + projects
                + " " + chrome);
    }

    /** Measures all three dimension labels against the anchors native reports NOW. */
    private void measureDimensionLabels(final String caseId, final String action,
                                        final String captureName) {
        settleLayout();
        diagnoseDimensionLabels(caseId, action);
        final int[] labels = {R.id.body_dimension_label_x, R.id.body_dimension_label_y,
                R.id.body_dimension_label_z};
        final String[] names = {"S19 body_dimension_label_x", "S19 body_dimension_label_y",
                "S19 body_dimension_label_z"};
        final float[][] pair = new float[3][];
        for (int axis = 0; axis < 3; axis++) {
            final int which = axis;
            final float[] expected = new float[2];
            final boolean projects = NativeViewport.bodyDimensionLabelPoint(axis, expected);
            final float[] measured = onWorkspace(rule.getScenario(), (activity, workspace) -> {
                final View label = workspace.findViewById(labels[which]);
                final View viewport = workspace.findViewById(R.id.viewport_surface);
                if (label == null || !label.isShown() || viewport == null) {
                    return null;
                }
                final float[] centre = Ui3dAuditRecorder.centreOf(label, viewport);
                final boolean clamp = projects
                        && Ui3dAuditRecorder.wouldClamp(label, viewport, expected[0], expected[1]);
                return new float[]{centre[0], centre[1], clamp ? 1f : 0f};
            });
            if (!projects) {
                Ui3dAuditRecorder.spatialUnmeasured(caseId, action, names[which], "WORLD_ANCHORED",
                        measured == null ? "anchor does not project and the label is withdrawn"
                                : "anchor does not project but the label is STILL SHOWN");
                continue;
            }
            if (measured == null) {
                Ui3dAuditRecorder.spatialUnmeasured(caseId, action, names[which], "WORLD_ANCHORED",
                        "the anchor projects but the label is not on screen");
                continue;
            }
            Ui3dAuditRecorder.spatial(caseId, action, names[which], "WORLD_ANCHORED",
                    expected[0], expected[1], measured[0], measured[1], density,
                    measured[2] != 0f, "");
            if (which == 0) {
                pair[0] = new float[]{expected[0], expected[1], measured[0], measured[1]};
            }
        }
        if (captureName != null) {
            if (pair[0] != null) {
                Ui3dAuditRecorder.captureWithOverlay(captureName, pair[0][0], pair[0][1],
                        pair[0][2], pair[0][3], viewportOrigin());
            } else {
                Ui3dAuditRecorder.capture(captureName);
            }
        }
    }

    private void measureSketchDimension(String caseId, String action, String captureName) {
        settleLayout();
        final double[] dimension = new double[NativeViewport.SKETCH_DIMENSION_SIZE];
        if (!NativeViewport.sketchLineDimension(dimension)) {
            Ui3dAuditRecorder.spatialUnmeasured(caseId, action, "S31 sketch_dimension_label",
                    "WORLD_ANCHORED", "no straight Line is selected");
            return;
        }
        final float[] expected = new float[2];
        if (!NativeViewport.sketchScreenPoint(dimension[NativeViewport.SKETCH_DIMENSION_ANCHOR_U],
                dimension[NativeViewport.SKETCH_DIMENSION_ANCHOR_V], expected)) {
            Ui3dAuditRecorder.spatialUnmeasured(caseId, action, "S31 sketch_dimension_label",
                    "WORLD_ANCHORED", "the anchor does not project");
            return;
        }
        final float[] measured = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View shown = workspace.findViewById(R.id.sketch_dimension_value);
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            if (shown == null || !shown.isShown() || viewport == null) {
                return null;
            }
            final float[] centre = Ui3dAuditRecorder.centreOf(shown, viewport);
            final boolean clamp = Ui3dAuditRecorder.wouldClamp(shown, viewport, expected[0],
                    expected[1]);
            return new float[]{centre[0], centre[1], clamp ? 1f : 0f};
        });
        if (measured == null) {
            Ui3dAuditRecorder.spatialUnmeasured(caseId, action, "S31 sketch_dimension_label",
                    "WORLD_ANCHORED", "the anchor projects but the label is not on screen");
            return;
        }
        Ui3dAuditRecorder.spatial(caseId, action, "S31 sketch_dimension_label", "WORLD_ANCHORED",
                expected[0], expected[1], measured[0], measured[1], density, measured[2] != 0f,
                "length=" + dimension[NativeViewport.SKETCH_DIMENSION_LENGTH] + "m");
        if (captureName != null) {
            Ui3dAuditRecorder.captureWithOverlay(captureName, expected[0], expected[1],
                    measured[0], measured[1], viewportOrigin());
        }
    }

    private void measureExtrudeCluster(String caseId, String action, String captureName) {
        settleLayout();
        final double[] tool = extrudeState();
        if (tool[NativeViewport.CAD_EXTRUDE_ACTIVE] == 0.0
                || tool[NativeViewport.CAD_EXTRUDE_ON_SCREEN] == 0.0) {
            Ui3dAuditRecorder.spatialUnmeasured(caseId, action, "S35 cad_extrude cluster",
                    "FEATURE_ANCHORED", "the manipulator is inactive or its anchor is off screen");
            return;
        }
        final float expectX = (float) tool[NativeViewport.CAD_EXTRUDE_LABEL_X];
        final float expectY = (float) tool[NativeViewport.CAD_EXTRUDE_LABEL_Y];
        final float[] measured = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View leaf = workspace.findViewById(R.id.cad_extrude_depth_value);
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            if (leaf == null || !leaf.isShown() || viewport == null) {
                return null;
            }
            final View placed = Ui3dAuditRecorder.placedAncestor(leaf,
                    workspace.findViewById(R.id.cad_extrude_canvas));
            final float[] centre = Ui3dAuditRecorder.centreOf(placed, viewport);
            final boolean clamp = Ui3dAuditRecorder.wouldClamp(placed, viewport, expectX, expectY);
            return new float[]{centre[0], centre[1], clamp ? 1f : 0f};
        });
        if (measured == null) {
            Ui3dAuditRecorder.spatialUnmeasured(caseId, action, "S35 cad_extrude cluster",
                    "FEATURE_ANCHORED", "the anchor projects but the cluster is not on screen");
            return;
        }
        Ui3dAuditRecorder.spatial(caseId, action, "S35 cad_extrude cluster", "FEATURE_ANCHORED",
                expectX, expectY, measured[0], measured[1], density, measured[2] != 0f,
                "scale=" + tool[NativeViewport.CAD_EXTRUDE_SCALE]
                        + " extent=" + tool[NativeViewport.CAD_EXTRUDE_EXTENT]);
        if (captureName != null) {
            Ui3dAuditRecorder.captureWithOverlay(captureName, expectX, expectY, measured[0],
                    measured[1], viewportOrigin());
        }
    }

    private void measureSecondCluster(String caseId, String action, String captureName) {
        settleLayout();
        final double[] tool = extrudeState();
        if (tool[NativeViewport.CAD_EXTRUDE_ACTIVE] == 0.0
                || tool[NativeViewport.CAD_EXTRUDE_SECOND_ON_SCREEN] == 0.0) {
            Ui3dAuditRecorder.spatialUnmeasured(caseId, action, "S36 cad_extrude_second_value",
                    "FEATURE_ANCHORED", "the second anchor is absent or off screen; shown="
                            + shown(R.id.cad_extrude_second_value));
            return;
        }
        final float expectX = (float) tool[NativeViewport.CAD_EXTRUDE_SECOND_LABEL_X];
        final float expectY = (float) tool[NativeViewport.CAD_EXTRUDE_SECOND_LABEL_Y];
        final float[] measured = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View leaf = workspace.findViewById(R.id.cad_extrude_second_value);
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            if (leaf == null || !leaf.isShown() || viewport == null) {
                return null;
            }
            final View placed = Ui3dAuditRecorder.placedAncestor(leaf,
                    workspace.findViewById(R.id.cad_extrude_canvas));
            final float[] centre = Ui3dAuditRecorder.centreOf(placed, viewport);
            final boolean clamp = Ui3dAuditRecorder.wouldClamp(placed, viewport, expectX, expectY);
            return new float[]{centre[0], centre[1], clamp ? 1f : 0f};
        });
        if (measured == null) {
            Ui3dAuditRecorder.spatialUnmeasured(caseId, action, "S36 cad_extrude_second_value",
                    "FEATURE_ANCHORED", "the second anchor projects but the value is not shown");
            return;
        }
        Ui3dAuditRecorder.spatial(caseId, action, "S36 cad_extrude_second_value",
                "FEATURE_ANCHORED", expectX, expectY, measured[0], measured[1], density,
                measured[2] != 0f, "negative=" + tool[NativeViewport.CAD_EXTRUDE_NEGATIVE]);
        if (captureName != null) {
            Ui3dAuditRecorder.captureWithOverlay(captureName, expectX, expectY, measured[0],
                    measured[1], viewportOrigin());
        }
    }

    private void measureEditSketchChip(String caseId, String action, String captureName) {
        settleLayout();
        final long body = NativeViewport.sceneActiveBodyId();
        final float[] anchor = new float[3];
        if (body == NativeViewport.NO_OBJECT
                || !NativeViewport.cadBodySketchAnchor(body, anchor)) {
            Ui3dAuditRecorder.spatialUnmeasured(caseId, action, "S38 cad_canvas_edit_sketch",
                    "FEATURE_ANCHORED", "native reports no anchor; chip shown="
                            + shown(R.id.cad_canvas_edit_sketch));
            return;
        }
        final float[] measured = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View chip = workspace.findViewById(R.id.cad_canvas_edit_sketch);
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            if (chip == null || !chip.isShown() || viewport == null) {
                return null;
            }
            final float[] centre = Ui3dAuditRecorder.centreOf(chip, viewport);
            final boolean clamp = Ui3dAuditRecorder.wouldClamp(chip, viewport, anchor[0],
                    anchor[1]);
            return new float[]{centre[0], centre[1], clamp ? 1f : 0f};
        });
        if (measured == null) {
            Ui3dAuditRecorder.spatialUnmeasured(caseId, action, "S38 cad_canvas_edit_sketch",
                    "FEATURE_ANCHORED", "the anchor projects but the chip is not on screen");
            return;
        }
        Ui3dAuditRecorder.spatial(caseId, action, "S38 cad_canvas_edit_sketch",
                "FEATURE_ANCHORED", anchor[0], anchor[1], measured[0], measured[1], density,
                measured[2] != 0f, "scale=" + anchor[2]);
        if (captureName != null) {
            Ui3dAuditRecorder.captureWithOverlay(captureName, anchor[0], anchor[1], measured[0],
                    measured[1], viewportOrigin());
        }
    }

    // =======================================================================
    // Journey helpers — every one drives a shipped control
    // =======================================================================

    private void record(String caseId, String state, String surface, String expect,
                        boolean shownNow) {
        Ui3dAuditRecorder.visibility(caseId, state, surface, expect, shownNow);
    }

    /** Records the whole editor chrome as absent behind a full-window page. */
    private void recordEditorChrome(String caseId, String state, boolean atHome) {
        final String expect = atHome ? MUST_HIDE : MUST_SHOW;
        record(caseId, state, "S01 home_surface", atHome ? MUST_SHOW : MUST_HIDE,
                shown(R.id.home_surface));
        record(caseId, state, "S06 global_toolbar", expect, shown(R.id.global_toolbar));
        record(caseId, state, "S07 tool_rail", expect, shown(R.id.tool_rail));
        record(caseId, state, "S08 objects_capsule", expect, shown(R.id.objects_capsule));
        record(caseId, state, "S19 body_dimension_label_x", MUST_HIDE,
                shown(R.id.body_dimension_label_x));
        record(caseId, state, "S35 cad_extrude cluster", MUST_HIDE,
                shown(R.id.cad_extrude_depth_value));
        record(caseId, state, "S38 cad_canvas_edit_sketch", MUST_HIDE,
                shown(R.id.cad_canvas_edit_sketch));
        record(caseId, state, "S13 property_inspector", MUST_HIDE, shown(R.id.property_inspector));
    }

    /**
     * Records whether the precision surface is currently showing a given body.
     *
     * <p>`sketch_editor`, `cad_editor`, `sculpt_mesh_summary` and
     * `relative_scale_editor` are BODIES of the one precision surface
     * (`inspector.setBody`), not surfaces of their own, so asking whether one is
     * on screen while the panel is closed asks the wrong question. The audit
     * opens the panel from its own toggle — the control a user presses — and
     * then asks which body is in it, which is the question the contract
     * actually settles.
     */
    private void recordPrecisionBody(String caseId, String state, String surface, String expect,
                                     int bodyId) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openPrecision(workspace);
            return null;
        });
        settleLayout();
        record(caseId, state + "+precision_open", surface, expect, shown(bodyId));
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.closePrecision(workspace);
            return null;
        });
        settleLayout();
    }

    private boolean shown(final int id) {
        final Boolean value = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View view = workspace.findViewById(id);
            return view != null && view.isShown();
        });
        return Boolean.TRUE.equals(value);
    }

    private void press(final int id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View control = workspace.findViewById(id);
            assertNotNull("the audit journey needs control " + id, control);
            control.performClick();
            return null;
        });
        settleLayout();
    }

    /**
     * A System Back press, through the ONE decision method both platform routes
     * call: {@code ForgeShapeActivity.onBackPressed} below API 33 and the
     * {@code OnBackInvokedCallback} above it each call this and nothing else.
     */
    private void systemBack() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.dismissTopmostSurface();
            return null;
        });
        settleLayout();
    }

    private void syncChrome() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.syncFromNative();
            return null;
        });
        settleLayout();
    }

    private void enterTransformTool() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.uiState().setConstructionTool(EditorUiState.CONSTRUCTION_TOOL_TRANSFORM);
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    private boolean dimensionsModeOpen() {
        final double[] state = new double[NativeViewport.BODY_DIM_SIZE];
        NativeViewport.bodyDimensionsState(state);
        return state[NativeViewport.BODY_DIM_MODE_ACTIVE] != 0.0;
    }

    private boolean gizmoActive() {
        final double[] state = new double[NativeViewport.GIZMO_STATE_SIZE];
        NativeViewport.gizmoState(state);
        return state[NativeViewport.GIZMO_ACTIVE] != 0.0;
    }

    /** The gizmo state after making sure Transform is the held entry. */
    private boolean gizmoActiveInTransform() {
        enterTransformTool();
        return gizmoActive();
    }

    private double[] extrudeState() {
        final double[] tool = new double[NativeViewport.CAD_EXTRUDE_SIZE];
        NativeViewport.cadExtrudeToolState(tool);
        return tool;
    }

    private float[] cameraPose() {
        final float[] pose = new float[NativeViewport.CAMERA_POSE_SIZE];
        NativeViewport.debugCameraPose(pose);
        return pose;
    }

    private float[] dimensionAnchor(int axis) {
        final float[] out = new float[2];
        if (!NativeViewport.bodyDimensionLabelPoint(axis, out)) {
            return new float[]{Float.NaN, Float.NaN};
        }
        return out;
    }

    private int[] viewportOrigin() {
        final int[] origin = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int[] at = new int[2];
            workspace.findViewById(R.id.viewport_surface).getLocationOnScreen(at);
            return at;
        });
        return origin == null ? new int[]{0, 0} : origin;
    }

    private void applyTransform(final double px, final double py, final double pz,
                                final double rx, final double ry, final double rz,
                                final double sx, final double sy, final double sz) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int status =
                    NativeViewport.applyBoxTransform(px, py, pz, rx, ry, rz, sx, sy, sz);
            assertTrue("the placement was accepted rather than refused: " + status,
                    status == NativeViewport.APPLY_APPLIED
                            || status == NativeViewport.APPLY_UNCHANGED);
            workspace.syncFromNative();
            return null;
        });
        settleLayout();
    }

    private void typeDimension(final int axis, final String value) {
        final int labelId = axis == 0 ? R.id.body_dimension_label_x
                : axis == 1 ? R.id.body_dimension_label_y : R.id.body_dimension_label_z;
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View label = workspace.findViewById(labelId);
            if (label != null && label.isShown()) {
                label.performClick();
            }
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final EditText field = workspace.findViewById(R.id.field_body_dimension);
            if (field != null) {
                field.setText(value);
                workspace.findViewById(R.id.apply_body_dimension).performClick();
            }
            return null;
        });
        settleLayout();
    }

    private long addBody() {
        final Long id = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final long created = NativeViewport.sceneAddBody();
            NativeViewport.applyConstructionBox(1.0, 1.0, 1.0);
            NativeViewport.applyBoxTransform(2.5, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
            workspace.syncFromNative();
            return created;
        });
        settleLayout();
        return id == null ? NativeViewport.NO_OBJECT : id;
    }

    private void selectBody(final long id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.sceneSelectBody(id);
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    private void setVisible(final long id, final boolean visible) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.sceneSetBodyVisible(id, visible);
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    private void setLocked(final long id, final boolean locked) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.sceneSetBodyLocked(id, locked);
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    private void deleteBody(final long id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.sceneDeleteBody(id);
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    private void beginSketchXy() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openAddPrimitive(workspace);
            workspace.addPrimitivePalette().findViewById(R.id.sketch_plane_by_name)
                    .performClick();
            workspace.addPrimitivePalette().findViewById(R.id.sketch_plane_xy).performClick();
            return null;
        });
        settleLayout();
        assertEquals("a sketch is open", NativeViewport.SKETCH_EDITING, sketchState());
    }

    private void commitExtrude(final String depth) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final EditText field = workspace.sketchEditor()
                    .findViewById(R.id.field_extrude_depth);
            if (field != null) {
                field.setText(depth);
            }
            workspace.findViewById(R.id.sketch_extrude_commit).performClick();
            return null;
        });
        settleLayout();
        assertEquals("the extrusion committed", NativeViewport.SKETCH_INACTIVE, sketchState());
    }

    private void typeSecondDistance(final String value) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View reading = workspace.findViewById(R.id.cad_extrude_second_value);
            if (reading != null && reading.isShown()) {
                reading.performClick();
            }
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final EditText field = workspace.findViewById(R.id.field_cad_extrude_second);
            if (field != null) {
                field.setText(value);
                workspace.findViewById(R.id.apply_cad_extrude_second).performClick();
            }
            return null;
        });
        settleLayout();
    }

    // -----------------------------------------------------------------------
    // Real viewport gestures
    // -----------------------------------------------------------------------

    /** Drags the extrude arrow from native's own projected tip, along the axis. */
    private void dragExtrudeArrow(final int side) {
        final double[] tool = extrudeState();
        if (tool[NativeViewport.CAD_EXTRUDE_ACTIVE] == 0.0) {
            return;
        }
        final boolean second = side < 0;
        final float tipX = (float) tool[second ? NativeViewport.CAD_EXTRUDE_SECOND_TIP_X
                : NativeViewport.CAD_EXTRUDE_TIP_X];
        final float tipY = (float) tool[second ? NativeViewport.CAD_EXTRUDE_SECOND_TIP_Y
                : NativeViewport.CAD_EXTRUDE_TIP_Y];
        final float baseX = (float) tool[NativeViewport.CAD_EXTRUDE_LABEL_X];
        final float baseY = (float) tool[NativeViewport.CAD_EXTRUDE_LABEL_Y];
        // Along the shaft, away from the profile: the direction the tip already
        // states, so no screen convention is written down here.
        float ax = tipX - baseX;
        float ay = tipY - baseY;
        final float length = (float) Math.sqrt(ax * ax + ay * ay);
        if (!(length > 1f)) {
            Ui3dAuditRecorder.note("UI3D-08", "the arrow has no screen extent to drag along");
            return;
        }
        ax /= length;
        ay /= length;
        dragViewport(tipX, tipY, tipX + ax * 120f, tipY + ay * 120f);
    }

    private void dragGizmoAxis(final int handle) {
        final float[] start = new float[2];
        if (!NativeViewport.gizmoHandlePoint(handle, start)) {
            Ui3dAuditRecorder.note("UI3D-10", "the gizmo handle does not project; no drag made");
            return;
        }
        dragViewport(start[0], start[1], start[0] + 90f, start[1] - 40f);
    }

    private void orbitViewport() {
        final float[] centre = viewportCentre();
        dragViewport(centre[0] - 120f, centre[1] - 60f, centre[0] + 120f, centre[1] + 60f);
    }

    private void dragViewport(final float fromX, final float fromY, final float toX,
                              final float toY) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final long down = SystemClock.uptimeMillis();
            send(viewport, down, down, MotionEvent.ACTION_DOWN, fromX, fromY);
            for (int step = 1; step <= 8; ++step) {
                final float t = step / 8f;
                send(viewport, down, down + step * 12L, MotionEvent.ACTION_MOVE,
                        fromX + (toX - fromX) * t, fromY + (toY - fromY) * t);
            }
            send(viewport, down, down + 128L, MotionEvent.ACTION_UP, toX, toY);
            return null;
        });
        settleLayout();
    }

    /** A two-finger translation, which is the product's pan. */
    private void panViewport() {
        final float[] c = viewportCentre();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final long down = SystemClock.uptimeMillis();
            send(viewport, down, down, MotionEvent.ACTION_DOWN, c[0] - 120f, c[1]);
            sendTwo(viewport, down, down + 8L,
                    MotionEvent.ACTION_POINTER_DOWN | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                    c[0] - 120f, c[1], c[0] + 120f, c[1]);
            for (int step = 1; step <= 8; ++step) {
                final float dx = 12f * step;
                final float dy = 8f * step;
                sendTwo(viewport, down, down + 8L + step * 12L, MotionEvent.ACTION_MOVE,
                        c[0] - 120f + dx, c[1] + dy, c[0] + 120f + dx, c[1] + dy);
            }
            sendTwo(viewport, down, down + 140L,
                    MotionEvent.ACTION_POINTER_UP | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                    c[0] - 24f, c[1] + 64f, c[0] + 216f, c[1] + 64f);
            send(viewport, down, down + 150L, MotionEvent.ACTION_UP, c[0] - 24f, c[1] + 64f);
            return null;
        });
        settleLayout();
    }

    /** A pinch. {@code out} pulls the camera back; otherwise it pushes in. */
    private void pinchViewport(final boolean out) {
        final float[] c = viewportCentre();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final long down = SystemClock.uptimeMillis();
            final float start = out ? 300f : 80f;
            final float end = out ? 80f : 300f;
            send(viewport, down, down, MotionEvent.ACTION_DOWN, c[0] - start, c[1]);
            sendTwo(viewport, down, down + 8L,
                    MotionEvent.ACTION_POINTER_DOWN | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                    c[0] - start, c[1], c[0] + start, c[1]);
            for (int step = 1; step <= 8; ++step) {
                final float span = start + (end - start) * (step / 8f);
                sendTwo(viewport, down, down + 8L + step * 12L, MotionEvent.ACTION_MOVE,
                        c[0] - span, c[1], c[0] + span, c[1]);
            }
            sendTwo(viewport, down, down + 140L,
                    MotionEvent.ACTION_POINTER_UP | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                    c[0] - end, c[1], c[0] + end, c[1]);
            send(viewport, down, down + 150L, MotionEvent.ACTION_UP, c[0] - end, c[1]);
            return null;
        });
        settleLayout();
    }

    private float[] viewportCentre() {
        final float[] centre = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            return new float[]{viewport.getWidth() * 0.5f, viewport.getHeight() * 0.5f};
        });
        return centre == null ? new float[]{200f, 400f} : centre;
    }

    private static void send(View target, long downTime, long eventTime, int action, float x,
                             float y) {
        final MotionEvent event = MotionEvent.obtain(downTime, eventTime, action, x, y, 0);
        try {
            target.dispatchTouchEvent(event);
        } finally {
            event.recycle();
        }
    }

    private static void sendTwo(View target, long downTime, long eventTime, int action,
                                float x0, float y0, float x1, float y1) {
        final MotionEvent.PointerProperties[] props = {
                pointer(0), pointer(1)};
        final MotionEvent.PointerCoords[] coords = {coords(x0, y0), coords(x1, y1)};
        final MotionEvent event = MotionEvent.obtain(downTime, eventTime, action, 2, props,
                coords, 0, 0, 1f, 1f, 0, 0, android.view.InputDevice.SOURCE_TOUCHSCREEN, 0);
        try {
            target.dispatchTouchEvent(event);
        } finally {
            event.recycle();
        }
    }

    private static MotionEvent.PointerProperties pointer(int id) {
        final MotionEvent.PointerProperties props = new MotionEvent.PointerProperties();
        props.id = id;
        props.toolType = MotionEvent.TOOL_TYPE_FINGER;
        return props;
    }

    private static MotionEvent.PointerCoords coords(float x, float y) {
        final MotionEvent.PointerCoords out = new MotionEvent.PointerCoords();
        out.x = x;
        out.y = y;
        out.pressure = 1f;
        out.size = 1f;
        return out;
    }
}
