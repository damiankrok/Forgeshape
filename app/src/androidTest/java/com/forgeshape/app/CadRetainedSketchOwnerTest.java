package com.forgeshape.app;

import static com.forgeshape.app.SketchTestSupport.dragSketch;
import static com.forgeshape.app.SketchTestSupport.drawRectangleAndExtrude;
import static com.forgeshape.app.SketchTestSupport.selectTool;
import static com.forgeshape.app.SketchTestSupport.sketchEntityCount;
import static com.forgeshape.app.SketchTestSupport.sketchState;
import static com.forgeshape.app.SketchTestSupport.sketchViewState;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.pm.ActivityInfo;
import android.graphics.Rect;
import android.os.SystemClock;
import android.util.Log;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Retained sketches and parametric hardening on the device ({@code CAD-R2}),
 * on the OWNER's own path: Home → New Project → CAD, a 2 × 2 block extruded
 * 1 m, then New Sketch on its faces, Keep Sketch, the History surface's
 * Sketches and Features segments, reuse, delete, staged edits, a large fill
 * selection, the support chooser under interruption and both orientations.
 *
 * <p>Every step a journey TESTS is a real window touch on the control or the
 * viewport (the decor view, never a direct listener call); native truth — the
 * sketch inventory, the body measures, the encoded project and its fingerprint
 * — is read to assert. A value is typed through the product's own exact-value
 * door ({@code sketchApplyRectangle}, the precision field) exactly as the
 * earlier owner classes do. Case ids are {@code D01..D12}.
 */
@RunWith(AndroidJUnit4.class)
public final class CadRetainedSketchOwnerTest {

    private static final String TAG = "ForgeShape";
    private static final double VOLUME_TOLERANCE = 1e-6;

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private ModelingOwnerRig rig;
    private long body;
    private int marks;

    @Before
    public void startAtHome() {
        rig = new ModelingOwnerRig(rule.getScenario(), "cad-r2-retained-sketch", "CADR2_DEVICE");
        rig.startAtHome();
    }

    @After
    public void restoreAProject() {
        WorkspaceTestSupport.releaseOrientation(rule.getScenario());
        settleLayout();
        rig.restore();
    }

    // =======================================================================
    // D01: a new project's Sketch Browser lists its one sketch, by id
    // =======================================================================

    @Test
    public void d01_the_sketch_browser_lists_the_new_projects_root_sketch() {
        newBlock();
        openHistory();
        touchTab(R.id.feature_history_tab_sketches);
        final List<String> rows = browserRowTexts();
        rig.fact("d01.rows", rows);
        assertEquals("one retained sketch", 1, rows.size());
        assertTrue(rows.get(0), rows.get(0).contains(string(R.string.history_row_sketch, 1)));
        final SketchBrowserPresentation.Inventory inventory = inventory();
        assertEquals(1L, inventory.rows.get(0).id);
        assertTrue("the root sketch", inventory.rows.get(0).root);

        touchBrowserRow(0);
        assertTrue("Edit Sketch is offered", rig.shown(R.id.history_act_edit_sketch));
        assertTrue("Use for Add is offered", rig.shown(R.id.history_act_reuse_add));
        assertFalse("the root sketch offers no Delete", rig.shown(R.id.history_act_delete_sketch));
        final String detail = detailText();
        rig.fact("d01.detail", detail);
        assertTrue("and says why: " + detail,
                detail.contains(string(R.string.status_cad_sketch_is_root)));
        rig.capture("d01_sketch_browser");
    }

    // =======================================================================
    // D02: a sketch kept on the top face, unused, survives save and reopen
    // =======================================================================

    @Test
    public void d02_keep_sketch_on_the_top_face_survives_save_and_reopen_with_its_id() {
        newBlock();
        final int undoBefore = NativeViewport.constructionUndoDepth();
        newSketchOn(0.7f, 0.9f, 8.0f, 0.5, 0.5, 1.0);
        drawExactSquare(0.3);
        assertTrue("Keep Sketch is offered on a body face", rig.shown(R.id.keep_sketch));
        rig.touchId(R.id.keep_sketch);
        assertEquals("kept: " + rig.statusLine(), NativeViewport.SKETCH_INACTIVE, sketchState());
        assertEquals("one Undo", undoBefore + 1, NativeViewport.constructionUndoDepth());
        assertEquals("no feature was made", 1, NativeViewport.cadFeatureCount(body));
        final SketchBrowserPresentation.Inventory kept = inventory();
        final SketchBrowserPresentation.SketchRow row = kept.find(2L);
        assertNotNull("the kept sketch is id 2", row);
        assertEquals(NativeViewport.SKETCH_ROW_UNUSED, row.state);
        assertEquals(NativeViewport.SKETCH_SUPPORT_FEATURE_FACE, row.support);
        assertEquals(1L, row.supportFeature);
        assertEquals(4.0, volume(), VOLUME_TOLERANCE);

        final byte[] saved = NativeViewport.encodeProject();
        assertNotNull(saved);
        assertEquals(NativeViewport.PROJECT_OK, rig.applyOnUi(() -> NativeViewport.loadProject(saved)));
        body = NativeViewport.sceneActiveBodyId();
        final SketchBrowserPresentation.SketchRow reopened = inventory().find(2L);
        assertNotNull("the same id after reopening", reopened);
        assertEquals(NativeViewport.SKETCH_ROW_UNUSED, reopened.state);
        assertArrayEquals("reopening wrote nothing new", saved, NativeViewport.encodeProject());

        openHistory();
        touchTab(R.id.feature_history_tab_sketches);
        final List<String> rows = browserRowTexts();
        rig.fact("d02.rows", rows);
        assertTrue(rows.get(1), rows.get(1).contains(string(R.string.sketch_usage_unused)));
        rig.capture("d02_unused_sketch");
    }

    // =======================================================================
    // D03: a sketch on an Add's face, chosen by a physical tap and confirmed
    // =======================================================================

    @Test
    public void d03_new_sketch_on_an_add_face_is_kept_on_that_feature() {
        newBlock();
        newSketchOn(0.7f, 0.9f, 8.0f, 0.0, 0.0, 1.0);
        drawExactSquare(0.6);
        finishSketch();
        commitExtrude(NativeViewport.OPERATION_ADD, "0.5");
        assertEquals(2, NativeViewport.cadFeatureCount(body));
        assertEquals(4.0 + 0.36 * 0.5, volume(), VOLUME_TOLERANCE);
        // The boss's own top cap, z = 1.5.
        newSketchOn(0.7f, 0.9f, 8.0f, 0.0, 0.0, 1.5);
        assertEquals(1.0, sketchViewState()[NativeViewport.SKETCH_VIEW_FACE_SUPPORTED], 0.0);
        drawExactSquare(0.2);
        rig.touchId(R.id.keep_sketch);
        assertEquals(NativeViewport.SKETCH_INACTIVE, sketchState());
        final SketchBrowserPresentation.SketchRow row = inventory().find(3L);
        assertNotNull(row);
        assertEquals("it stands on the Add (feature 2)", 2L, row.supportFeature);
        assertEquals(NativeViewport.SKETCH_FACE_CAP_FAR,
                (int) inventoryRaw(3L)[NativeViewport.SKETCH_INV_SUPPORT_FACE_KIND]);
    }

    // =======================================================================
    // D04: a Cut's floor and straight wall carry a sketch; a curved wall does
    // not; the Edit Sketch chip takes no tap meant for a face
    // =======================================================================

    @Test
    public void d04_cut_floor_and_straight_wall_are_supports_a_curved_wall_is_not() {
        newBlock();
        // A slot through the +x side: x in [0.4, 1.2], y in [-0.3, 0.3], 0.4 deep.
        newSketchOn(0.7f, 0.9f, 8.0f, 0.0, 0.0, 1.0);
        drawExactRectangleAt(0.8, 0.0, 0.8, 0.6);
        finishSketch();
        commitExtrude(NativeViewport.OPERATION_CUT, "0.4");
        // A round pocket at (-0.4, 0), r 0.3, 0.4 deep, on the same top cap.
        // Seen from the -x side: from +x, the chooser's translucent YZ plane
        // stands in front of a point with x < 0 and takes the tap (D04 run 1).
        newSketchOn(-0.7f, 0.9f, 8.0f, -0.4, 0.5, 1.0);
        drawExactCircleAt(-0.4, 0.0, 0.3);
        finishSketch();
        commitExtrude(NativeViewport.OPERATION_CUT, "0.4");
        assertEquals(3, NativeViewport.cadFeatureCount(body));

        // The slot's floor, z = 0.6, seen from above (+z, a little +y).
        newSketchOn(0.0f, 0.2f, 7.0f, 0.8, 0.0, 0.6);
        drawExactSquare(0.1);
        rig.touchId(R.id.keep_sketch);
        assertEquals(NativeViewport.SKETCH_INACTIVE, sketchState());
        final double[] floor = inventoryRaw(4L);
        rig.fact("d04.floor", Arrays.toString(floor));
        assertEquals("on the slot Cut", 2.0, floor[NativeViewport.SKETCH_INV_SUPPORT_FEATURE], 0.0);
        assertEquals(NativeViewport.SKETCH_FACE_CAP_FAR,
                (int) floor[NativeViewport.SKETCH_INV_SUPPORT_FACE_KIND]);

        // The slot's -x wall (x = 0.4, facing +x), seen from +x.
        newSketchOn((float) (Math.PI / 2), 0.3f, 6.0f, 0.4, 0.0, 0.8);
        drawExactSquare(0.1);
        rig.touchId(R.id.keep_sketch);
        final double[] wall = inventoryRaw(5L);
        rig.fact("d04.wall", Arrays.toString(wall));
        assertEquals(2.0, wall[NativeViewport.SKETCH_INV_SUPPORT_FEATURE], 0.0);
        assertEquals(NativeViewport.SKETCH_FACE_SIDE,
                (int) wall[NativeViewport.SKETCH_INV_SUPPORT_FACE_KIND]);

        // The round pocket's curved wall: the chooser does not choose it.
        openNewSketch();
        setCamera(-1.0f, 0.1f, 6.0f);
        final float[] at = new float[2];
        assertTrue(NativeViewport.debugProjectWorld(-0.1, 0.0, 0.8, at));
        final float[] window = viewportToWindow(at);
        realTap(window);
        final int kind = NativeViewport.supportChooserSelectedKind();
        rig.fact("d04.curved_kind", kind);
        realTap(window);
        if (sketchState() == NativeViewport.SKETCH_EDITING) {
            // If anything opened, it must not stand on the curved wall.
            final double[] view = sketchViewState();
            rig.fact("d04.curved_opened", Arrays.toString(view));
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                NativeViewport.sketchCancel();
                workspace.onNativeStateChanged();
                return null;
            });
            settleLayout();
            throw new AssertionError("a sketch opened at a curved Cut wall");
        }
        assertNotEquals("the curved wall is never chosen", NativeViewport.SUPPORT_KIND_FACE, kind);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.supportChooserCancel();
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        assertEquals("nothing was kept there", 5, inventory().rows.size());
    }

    // =======================================================================
    // D05: one sketch drives an Add and a Cut; editing it rebuilds both
    // =======================================================================

    @Test
    public void d05_a_shared_sketch_drives_an_add_and_a_cut_and_one_edit_rebuilds_both() {
        newBlock();
        newSketchOn(0.7f, 0.9f, 8.0f, 0.5, 0.5, 1.0);
        final double[] centre = drawExactSquare(0.4);
        rig.touchId(R.id.keep_sketch);
        assertEquals(NativeViewport.SKETCH_INACTIVE, sketchState());

        reuse(2L, R.id.history_act_reuse_add, "0.3");
        assertEquals(4.0 + 0.16 * 0.3, volume(), VOLUME_TOLERANCE);
        reuse(2L, R.id.history_act_reuse_cut, "0.2");
        assertEquals(4.0 + 0.16 * 0.3 - 0.16 * 0.2, volume(), VOLUME_TOLERANCE);
        final long[] consumers = new long[4];
        assertEquals("one sketch, two features", 2,
                NativeViewport.cadSketchConsumers(body, 2L, consumers));
        assertEquals(2L, consumers[0]);
        assertEquals(3L, consumers[1]);
        assertEquals("no copy", 2, inventory().rows.size());

        // Edit the SHARED sketch from the browser: both features rebuild.
        openHistory();
        touchTab(R.id.feature_history_tab_sketches);
        touchBrowserRow(1);
        assertTrue(detailText(), detailText().contains(string(R.string.sketch_shared_edit_warning, 2)));
        rig.touchId(R.id.history_act_edit_sketch);
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
        assertEquals("through its first consumer", 2L, NativeViewport.sketchEditingFeatureId());
        final long square = selectedOrOnlyEntity();
        assertEquals(NativeViewport.CAD_OK, rig.applyOnUi(
                () -> NativeViewport.sketchApplyRectangle(square, 0.5, 0.5)));
        rig.press(R.id.finish_sketch);
        assertEquals(NativeViewport.SKETCH_READY, sketchState());
        rig.press(R.id.extrude_sketch);
        assertEquals(NativeViewport.SKETCH_INACTIVE, sketchState());
        assertEquals("both consumers rebuilt from the edited sketch",
                4.0 + 0.25 * 0.3 - 0.25 * 0.2, volume(), VOLUME_TOLERANCE);
        rig.fact("d05.centre", Arrays.toString(centre));

        // Save and reopen: still one sketch, two consumers.
        final byte[] saved = NativeViewport.encodeProject();
        assertEquals(NativeViewport.PROJECT_OK, rig.applyOnUi(() -> NativeViewport.loadProject(saved)));
        body = NativeViewport.sceneActiveBodyId();
        assertEquals(2, NativeViewport.cadSketchConsumers(body, 2L, consumers));
        assertEquals(4.0 + 0.25 * 0.3 - 0.25 * 0.2, volume(), VOLUME_TOLERANCE);
    }

    // =======================================================================
    // D06: delete an unused sketch, Undo / Redo; a used or root one cannot be
    // =======================================================================

    @Test
    public void d06_delete_an_unused_sketch_and_undo_redo_restore_the_same_id() {
        newBlock();
        newSketchOn(0.7f, 0.9f, 8.0f, 0.5, 0.5, 1.0);
        drawExactSquare(0.3);
        rig.touchId(R.id.keep_sketch);
        final long fingerprintKept = NativeViewport.projectFingerprint();
        openHistory();
        touchTab(R.id.feature_history_tab_sketches);
        touchBrowserRow(1);
        assertTrue("the unused sketch offers Delete", rig.shown(R.id.history_act_delete_sketch));
        rig.touchId(R.id.history_act_delete_sketch);
        assertEquals("deleted", 1, inventory().rows.size());
        assertEquals("the browser re-read native", 1, browserRowTexts().size());
        rig.capture("d06_after_delete");

        rig.press(R.id.undo_action);
        assertNotNull("Undo restores the same id", inventory().find(2L));
        assertEquals(fingerprintKept, NativeViewport.projectFingerprint());
        rig.press(R.id.redo_action);
        assertEquals(1, inventory().rows.size());
        rig.press(R.id.undo_action);

        // A NEW sketch now takes a fresh id: 3, never the freed... the restored 2 stands.
        newSketchOn(-0.7f, 0.9f, 8.0f, -0.5, -0.5, 1.0);
        drawExactSquare(0.3);
        rig.touchId(R.id.keep_sketch);
        assertNotNull(inventory().find(3L));

        // The root sketch offers no Delete, and native refuses one by name.
        openHistory();
        touchTab(R.id.feature_history_tab_sketches);
        touchBrowserRow(0);
        assertFalse(rig.shown(R.id.history_act_delete_sketch));
        assertEquals(NativeViewport.CAD_SKETCH_IS_ROOT,
                rig.applyOnUi(() -> NativeViewport.cadDeleteSketch(body, 1L)));
    }

    // =======================================================================
    // D07: a LATER feature's typed depth from the Features segment
    // =======================================================================

    @Test
    public void d07_edit_feature_opens_the_exact_later_extrude_and_a_typed_depth_commits() {
        newBlock();
        newSketchOn(0.7f, 0.9f, 8.0f, 0.0, 0.0, 1.0);
        drawExactSquare(0.6);
        finishSketch();
        commitExtrude(NativeViewport.OPERATION_CUT, "0.4");
        assertEquals(4.0 - 0.36 * 0.4, volume(), VOLUME_TOLERANCE);

        openHistory();
        touchTab(R.id.feature_history_tab_features);
        final List<String> rows = browserRowTexts();
        rig.fact("d07.features", rows);
        assertEquals(2, rows.size());
        touchBrowserRow(1);
        assertTrue(rig.shown(R.id.history_act_edit_feature));
        assertTrue(rig.shown(R.id.history_act_edit_sketch));
        rig.touchId(R.id.history_act_edit_feature);
        assertEquals("the Cut, by id, in Ready", NativeViewport.SKETCH_READY, sketchState());
        assertEquals(2L, NativeViewport.sketchEditingFeatureId());
        commitTypedDepthEdit("0.25");
        assertEquals(4.0 - 0.36 * 0.25, volume(), VOLUME_TOLERANCE);
        final double[] measure = measure();
        rig.fact("d07.measure", Arrays.toString(measure));
        assertEquals("bounds unchanged by a pocket", -1.0,
                measure[NativeViewport.CAD_MEASURE_MIN_X], 1e-6);
        assertEquals(1.0, measure[NativeViewport.CAD_MEASURE_MAX_X], 1e-6);
        rig.press(R.id.undo_action);
        assertEquals(4.0 - 0.36 * 0.4, volume(), VOLUME_TOLERANCE);
    }

    // =======================================================================
    // D08: an earlier sketch edit that strands a later Cut; Cancel restores
    // =======================================================================

    @Test
    public void d08_an_upstream_sketch_edit_names_the_first_failure_and_cancel_restores() {
        newBlock();
        newSketchOn(0.7f, 0.9f, 8.0f, 0.5, 0.5, 1.0);
        // The top cap's sketch (u, v) is the world (x, y): the pocket stands at
        // (0.6, 0.6), well clear of a 0.4 wide base (D08 run 1 drew it at the
        // sketch origin, inside it).
        drawExactRectangleAt(0.6, 0.6, 0.3, 0.3);
        finishSketch();
        commitExtrude(NativeViewport.OPERATION_CUT, "0.4");
        assertEquals(4.0 - 0.09 * 0.4, volume(), VOLUME_TOLERANCE);
        final byte[] before = NativeViewport.encodeProject();
        final long fingerprint = NativeViewport.projectFingerprint();

        openHistory();
        touchTab(R.id.feature_history_tab_sketches);
        touchBrowserRow(0);
        rig.touchId(R.id.history_act_edit_sketch);
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
        final long rectangle = selectedOrOnlyEntity();
        // 0.4 wide: the pocket near (0.5, 0.5) now misses the block entirely.
        assertEquals(NativeViewport.CAD_OK,
                rig.applyOnUi(() -> NativeViewport.sketchApplyRectangle(rectangle, 0.4, 2.0)));
        rig.press(R.id.finish_sketch);
        assertTrue("the issue card names the failing Cut", rig.shown(R.id.regeneration_issue));
        final String title = rig.on((activity, workspace) -> ((TextView) workspace.findViewById(
                R.id.regeneration_issue_title)).getText().toString());
        rig.fact("d08.issue", title);
        assertTrue(title, title.contains(string(R.string.sketch_operation_cut)));
        rig.capture("d08_issue");
        rig.touchId(R.id.regeneration_issue_cancel);
        assertEquals(NativeViewport.SKETCH_INACTIVE, sketchState());
        assertArrayEquals("byte-identical after Cancel", before, NativeViewport.encodeProject());
        assertEquals(fingerprint, NativeViewport.projectFingerprint());
    }

    // =======================================================================
    // D09: 22 grid cells by real taps around a pinched pair of holes; commit,
    // save, reopen, exact volume
    // =======================================================================

    @Test
    public void d09_twenty_two_grid_cells_by_real_taps_extrude_one_exact_solid() {
        rig.press(R.id.home_new_project);
        rig.press(R.id.new_project_cad);
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
        // Five vertical and three horizontal lines, typed exact on the outline.
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        for (int i = -2; i <= 2; i++) {
            final double x = i;
            dragSketch(rule.getScenario(), x, -1.5, x, 1.5);
            final long id = selectedOrLastEntity();
            assertEquals(NativeViewport.CAD_OK, rig.applyOnUi(
                    () -> NativeViewport.sketchApplyLine(id, x, -2.0, x, 2.0)));
        }
        for (int j = -1; j <= 1; j++) {
            final double y = j;
            dragSketch(rule.getScenario(), -2.5, y, 2.5, y);
            final long id = selectedOrLastEntity();
            assertEquals(NativeViewport.CAD_OK, rig.applyOnUi(
                    () -> NativeViewport.sketchApplyLine(id, -3.0, y, 3.0, y)));
        }
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), -3.0, -2.0, 3.0, 2.0);
        final long outline = selectedOrLastEntity();
        assertEquals(NativeViewport.CAD_OK,
                rig.applyOnUi(() -> NativeViewport.sketchApplyRectangle(outline, 6.0, 4.0)));
        assertEquals(9, sketchEntityCount());
        finishSketch();
        assertEquals("fill mode", 1, NativeViewport.sketchSelectionKind());
        final long[] all = faceHandles();
        rig.fact("d09.cells", all.length);
        assertEquals("a 6 x 4 grid", 24, all.length);
        setCamera(0.25f, 0.25f, 14.0f);

        // Every cell but (-0.5, -0.5) and (0.5, 0.5): two holes touching at (0, 0).
        int taps = 0;
        for (int column = 0; column < 6; column++) {
            for (int row = 0; row < 4; row++) {
                final double u = -2.5 + column;
                final double v = -1.5 + row;
                if ((column == 2 && row == 1) || (column == 3 && row == 2)) {
                    continue;
                }
                final long handle = cellAt(u, v);
                tapCell(handle, u, v);
                taps++;
                assertEquals("tap " + taps + " adds exactly its cell", taps, selected().length);
            }
        }
        assertEquals(22, selected().length);
        rig.capture("d09_selection");
        final double[] preview = candidateMeasure();
        rig.fact("d09.preview", Arrays.toString(preview));
        assertEquals("one solid with its pinched holes", 1.0, preview[1], 0.0);
        commitExtrude(-1, "0.5");
        assertTrue(NativeViewport.projectOpen());
        body = NativeViewport.sceneActiveBodyId();
        assertEquals("22 unit cells x 0.5", 11.0, volume(), VOLUME_TOLERANCE);
        final double[] m = measure();
        assertEquals(1.0, m[NativeViewport.CAD_MEASURE_COMPONENTS], 0.0);
        final byte[] saved = NativeViewport.encodeProject();
        assertEquals(NativeViewport.PROJECT_OK, rig.applyOnUi(() -> NativeViewport.loadProject(saved)));
        body = NativeViewport.sceneActiveBodyId();
        assertEquals(11.0, volume(), VOLUME_TOLERANCE);
        assertArrayEquals(saved, NativeViewport.encodeProject());
    }

    // =======================================================================
    // D10: the support chooser interrupted by Undo and by a rotation
    // =======================================================================

    @Test
    public void d10_an_interrupted_support_choice_is_refused_never_retargeted() {
        newBlock();
        newSketchOn(0.7f, 0.9f, 8.0f, 0.0, 0.0, 1.0);
        drawExactSquare(0.6);
        finishSketch();
        commitExtrude(NativeViewport.OPERATION_ADD, "0.5");
        assertEquals(2, NativeViewport.cadFeatureCount(body));

        // Aim at the boss's top, then Undo removes the boss: the confirm tap
        // at the same pixel must not open a sketch on the vanished face.
        openNewSketch();
        setCamera(0.7f, 0.9f, 8.0f);
        final float[] at = new float[2];
        assertTrue(NativeViewport.debugProjectWorld(0.0, 0.0, 1.5, at));
        final float[] window = viewportToWindow(at);
        realTap(window);
        assertEquals(NativeViewport.SUPPORT_KIND_FACE, NativeViewport.supportChooserSelectedKind());
        final boolean undoShown = rig.shown(R.id.undo_action);
        rig.fact("d10.undo_shown", undoShown);
        if (undoShown) {
            rig.touchId(R.id.undo_action);
        } else {
            rig.applyOnUi(NativeViewport::historyUndo);
        }
        assertEquals("the boss is gone", 1, NativeViewport.cadFeatureCount(body));
        final String mark = mark();
        if (NativeViewport.supportChooserActive()) {
            realTap(window);
        }
        final List<String> tokens = tokensSince(mark);
        rig.fact("d10.after_undo", tokens + " state=" + sketchState());
        if (sketchState() == NativeViewport.SKETCH_EDITING) {
            // Whatever opened must stand on a face that EXISTS: the base's top
            // (feature 1) at z = 1, never the vanished boss.
            final float[] origin = new float[2];
            assertTrue(NativeViewport.sketchScreenPoint(0.0, 0.0, origin));
            final float[] baseTop = new float[2];
            assertTrue(NativeViewport.debugProjectWorld(0.0, 0.0, 1.0, baseTop));
            assertEquals(baseTop[0], origin[0], 2.0f);
            assertEquals(baseTop[1], origin[1], 2.0f);
            cancelSketch();
        } else {
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                NativeViewport.supportChooserCancel();
                workspace.onNativeStateChanged();
                return null;
            });
            settleLayout();
        }

        // Rotation mid-choice: the chooser keeps its native state, the confirm
        // re-projects in the new window and lands on the face it names.
        openNewSketch();
        setCamera(0.7f, 0.9f, 8.0f);
        assertTrue(NativeViewport.debugProjectWorld(0.5, 0.5, 1.0, at));
        realTap(viewportToWindow(at));
        WorkspaceTestSupport.setOrientation(rule.getScenario(),
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        settleLayout();
        SystemClock.sleep(600);
        settleLayout();
        rig.fact("d10.rotated_chooser", NativeViewport.supportChooserActive());
        if (NativeViewport.supportChooserActive()) {
            assertTrue(NativeViewport.debugProjectWorld(0.5, 0.5, 1.0, at));
            final float[] rotated = viewportToWindow(at);
            realTap(rotated);
            if (sketchState() != NativeViewport.SKETCH_EDITING) {
                realTap(rotated);
            }
            assertEquals("the confirm after rotation opens on the base's top",
                    NativeViewport.SKETCH_EDITING, sketchState());
            assertSketchOriginAtFaceOf(1.0);
            cancelSketch();
        }
        assertEquals("nothing was written", 1, NativeViewport.cadFeatureCount(body));
        assertEquals(1, inventory().rows.size());
    }

    // =======================================================================
    // D11: a Revolve's angle from the Features segment; no reuse on it
    // =======================================================================

    @Test
    public void d11_a_revolve_feature_edits_its_exact_angle_and_offers_no_add_or_cut() {
        rig.press(R.id.home_new_project);
        rig.press(R.id.new_project_cad);
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), 0.4, -0.4, 1.2, 0.4);
        final long square = selectedOrLastEntity();
        assertEquals(NativeViewport.CAD_OK,
                rig.applyOnUi(() -> NativeViewport.sketchApplyRectangle(square, 0.8, 0.8)));
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        dragSketch(rule.getScenario(), 0.0, -0.6, 0.0, 0.6);
        final long line = selectedOrLastEntity();
        assertEquals(NativeViewport.CAD_OK,
                rig.applyOnUi(() -> NativeViewport.sketchApplyLine(line, 0.0, -0.6, 0.0, 0.6)));
        rig.press(R.id.finish_sketch);
        assertEquals(NativeViewport.CAD_OK, rig.applyOnUi(NativeViewport::sketchBeginRevolve));
        assertEquals(NativeViewport.CAD_OK,
                rig.applyOnUi(() -> NativeViewport.sketchSetRevolveAxis(line, 0)));
        rig.press(R.id.revolve_sketch);
        assertTrue(NativeViewport.projectOpen());
        body = NativeViewport.sceneActiveBodyId();

        openHistory();
        touchTab(R.id.feature_history_tab_sketches);
        touchBrowserRow(0);
        assertFalse("a revolved body takes no Add", rig.shown(R.id.history_act_reuse_add));
        assertFalse("nor a Cut", rig.shown(R.id.history_act_reuse_cut));
        assertTrue(detailText(), detailText().contains(string(R.string.sketch_cannot_reuse, "")
                .trim().replace(":", "").trim()));
        touchTab(R.id.feature_history_tab_features);
        touchBrowserRow(0);
        rig.touchId(R.id.history_act_edit_feature);
        assertEquals(NativeViewport.SKETCH_READY, sketchState());
        assertEquals(NativeViewport.CAD_OK,
                rig.applyOnUi(() -> NativeViewport.sketchSetRevolveAngle(212.5)));
        rig.press(R.id.revolve_sketch);
        assertEquals(NativeViewport.SKETCH_INACTIVE, sketchState());
        assertEquals(212.5, featureAngle(), 0.0);
        rig.press(R.id.undo_action);
        assertEquals(360.0, featureAngle(), 0.0);
    }

    // =======================================================================
    // D12: both orientations, every act a whole 48 dp target that owns its
    // touch, a stylus press, and the chip never under the chooser
    // =======================================================================

    @Test
    public void d12_landscape_and_portrait_acts_are_48dp_and_own_their_touch() {
        newBlock();
        newSketchOn(0.7f, 0.9f, 8.0f, 0.5, 0.5, 1.0);
        drawExactSquare(0.3);
        rig.touchId(R.id.keep_sketch);
        for (int orientation : new int[]{ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE,
                ActivityInfo.SCREEN_ORIENTATION_PORTRAIT}) {
            WorkspaceTestSupport.setOrientation(rule.getScenario(), orientation);
            settleLayout();
            SystemClock.sleep(600);
            settleLayout();
            openHistory();
            stylusTap(R.id.feature_history_tab_sketches);
            assertEquals("a stylus press chose the segment", SketchBrowserPresentation.SEGMENT_SKETCHES,
                    (int) rig.on((activity, workspace) -> workspace.featureHistory().segment()));
            touchBrowserRow(1);
            final List<Rect> chips = actChipRects();
            rig.fact("d12.chips." + orientation, chips.toString());
            assertEquals("Edit, Add, Cut and Delete", 4, chips.size());
            final float density = rig.on((activity, workspace) ->
                    activity.getResources().getDisplayMetrics().density);
            for (Rect r : chips) {
                assertTrue("48 dp high: " + r, r.height() >= Math.round(48 * density) - 1);
                assertTrue("48 dp wide: " + r, r.width() >= Math.round(48 * density) - 1);
            }
            for (int i = 0; i < chips.size(); i++) {
                for (int j = i + 1; j < chips.size(); j++) {
                    assertFalse("acts never overlap: " + chips.get(i) + " " + chips.get(j),
                            Rect.intersects(chips.get(i), chips.get(j)));
                }
            }
            // A real touch on each lands on that chip and nothing else.
            for (int id : new int[]{R.id.history_act_edit_sketch, R.id.history_act_reuse_add,
                    R.id.history_act_reuse_cut, R.id.history_act_delete_sketch}) {
                assertTrue(ownsItsCentre(id));
            }
            rig.capture("d12_acts_" + orientation);
            closeHistory();
        }
        // While choosing a support, the Edit Sketch chip stands nowhere.
        openNewSketch();
        assertFalse(rig.shown(R.id.cad_canvas_edit_sketch));
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.supportChooserCancel();
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    // -----------------------------------------------------------------------
    // The project and the chooser
    // -----------------------------------------------------------------------

    /** Home -> New Project -> CAD -> a 2 x 2 rectangle extruded 1. */
    private void newBlock() {
        rig.press(R.id.home_new_project);
        rig.press(R.id.new_project_cad);
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
        body = drawRectangleAndExtrude(rule.getScenario(), 2.0, 2.0, "1");
        assertTrue(NativeViewport.projectOpen());
        assertEquals(4.0, volume(), VOLUME_TOLERANCE);
    }

    /** The ForgeShape mark, then New Sketch, both real window taps. */
    private void openNewSketch() {
        rig.touchId(R.id.project_actions_button);
        assertTrue("New Sketch is offered", rig.shown(R.id.project_new_sketch));
        rig.touchId(R.id.project_new_sketch);
        assertTrue("the support chooser is up", NativeViewport.supportChooserActive());
        assertFalse("no Edit Sketch chip during the choice", rig.shown(R.id.cad_canvas_edit_sketch));
    }

    /** New Sketch, a camera pose, then aim + confirm on a world point. */
    private void newSketchOn(float yaw, float pitch, float distance, double x, double y, double z) {
        openNewSketch();
        setCamera(yaw, pitch, distance);
        final float[] at = new float[2];
        assertTrue("the target projects", NativeViewport.debugProjectWorld(x, y, z, at));
        final float[] window = viewportToWindow(at);
        realTap(window);
        assertEquals("the tap chose a FACE at (" + x + ", " + y + ", " + z + ")",
                NativeViewport.SUPPORT_KIND_FACE, NativeViewport.supportChooserSelectedKind());
        realTap(window);
        assertEquals("a face sketch is open: " + rig.statusLine(), NativeViewport.SKETCH_EDITING,
                sketchState());
    }

    private void cancelSketch() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onCancelSketchRequested();
            return null;
        });
        settleLayout();
        assertEquals(NativeViewport.SKETCH_INACTIVE, sketchState());
    }

    /** The open sketch's origin projects onto the body's top at height z. */
    private void assertSketchOriginAtFaceOf(double z) {
        final float[] origin = new float[2];
        assertTrue(NativeViewport.sketchScreenPoint(0.0, 0.0, origin));
        final float[] world = new float[2];
        assertTrue(NativeViewport.debugProjectWorld(0.0, 0.0, z, world));
        rig.fact("origin", Arrays.toString(origin) + " world=" + Arrays.toString(world));
        assertEquals(world[0], origin[0], 2.0f);
        assertEquals(world[1], origin[1], 2.0f);
    }

    /** A square of side s about the sketch origin: a real drag, then typed exact. */
    private double[] drawExactSquare(double side) {
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        final double drag = Math.max(side, 1.0) / 2;
        dragSketch(rule.getScenario(), -drag, -drag, drag, drag);
        assertEquals(1, sketchEntityCount());
        final double[] drawn = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        assertTrue(NativeViewport.sketchSelectedEntity(drawn));
        final long id = (long) drawn[NativeViewport.SKETCH_ENTITY_ID];
        assertEquals(NativeViewport.CAD_OK,
                rig.applyOnUi(() -> NativeViewport.sketchApplyRectangle(id, side, side)));
        assertTrue(NativeViewport.sketchSelectedEntity(drawn));
        return new double[]{drawn[NativeViewport.SKETCH_ENTITY_VALUES],
                drawn[NativeViewport.SKETCH_ENTITY_VALUES + 1]};
    }

    /**
     * A w x h rectangle centred on (x, y) of the base's top-cap sketch, whose
     * (u, v) is the world (x, y) about (0, 0, 1) (OSS-03): a real drag
     * corner to corner, then the exact size typed about the drawn centre.
     */
    private void drawExactRectangleAt(double x, double y, double w, double h) {
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), x - w / 2, y - h / 2, x + w / 2, y + h / 2);
        final double[] drawn = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        assertTrue(NativeViewport.sketchSelectedEntity(drawn));
        assertEquals(NativeViewport.SKETCH_ENTITY_KIND_RECTANGLE,
                (int) drawn[NativeViewport.SKETCH_ENTITY_KIND]);
        final long id = (long) drawn[NativeViewport.SKETCH_ENTITY_ID];
        assertEquals(NativeViewport.CAD_OK,
                rig.applyOnUi(() -> NativeViewport.sketchApplyRectangle(id, w, h)));
        assertTrue(NativeViewport.sketchSelectedEntity(drawn));
        rig.fact("rect", Arrays.toString(drawn));
        assertEquals("the drag's centre (u)", x, drawn[NativeViewport.SKETCH_ENTITY_VALUES], 0.05);
        assertEquals("the drag's centre (v)", y, drawn[NativeViewport.SKETCH_ENTITY_VALUES + 1], 0.05);
    }

    /** A circle of radius r about (x, y) of the top-cap sketch: a real drag, then typed. */
    private void drawExactCircleAt(double x, double y, double r) {
        selectTool(rule.getScenario(), R.id.tool_rail_circle);
        dragSketch(rule.getScenario(), x, y, x + r, y);
        final double[] drawn = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        assertTrue(NativeViewport.sketchSelectedEntity(drawn));
        assertEquals(NativeViewport.SKETCH_ENTITY_KIND_CIRCLE,
                (int) drawn[NativeViewport.SKETCH_ENTITY_KIND]);
        final long id = (long) drawn[NativeViewport.SKETCH_ENTITY_ID];
        assertEquals(NativeViewport.CAD_OK,
                rig.applyOnUi(() -> NativeViewport.sketchApplyCircle(id, r)));
        assertTrue(NativeViewport.sketchSelectedEntity(drawn));
        rig.fact("circle", Arrays.toString(drawn));
        assertEquals(x, drawn[NativeViewport.SKETCH_ENTITY_VALUES], 0.05);
        assertEquals(y, drawn[NativeViewport.SKETCH_ENTITY_VALUES + 1], 0.05);
    }

    private void finishSketch() {
        rig.press(R.id.finish_sketch);
        assertEquals("Finish succeeds: " + NativeViewport.sketchLastStatus(),
                NativeViewport.SKETCH_READY, sketchState());
    }

    /** The precision surface: an operation (or -1 to keep it), a depth, Extrude. */
    private void commitExtrude(int operation, String depth) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openPrecision(workspace);
            return null;
        });
        settleLayout();
        if (operation >= 0) {
            final int id = operation == NativeViewport.OPERATION_CUT ? R.id.sketch_operation_cut
                    : operation == NativeViewport.OPERATION_ADD ? R.id.sketch_operation_add
                    : R.id.sketch_operation_new_body;
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                final View chip = workspace.sketchEditor().findViewById(id);
                assertNotNull(chip);
                assertTrue("the operation is offered", chip.isShown());
                chip.performClick();
                return null;
            });
            settleLayout();
        }
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final EditText field = workspace.sketchEditor().findViewById(R.id.field_extrude_depth);
            field.setText(depth);
            workspace.findViewById(R.id.sketch_extrude_commit).performClick();
            return null;
        });
        settleLayout();
        assertEquals("the extrusion committed: " + rig.statusLine() + " last="
                + NativeViewport.sketchLastStatus(), NativeViewport.SKETCH_INACTIVE, sketchState());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.closePrecision(workspace);
            return null;
        });
        settleLayout();
    }

    /** A staged edit open in Ready: a typed depth, then the edit commit. */
    private void commitTypedDepthEdit(String depth) {
        commitExtrude(-1, depth);
    }

    /** Sketches segment: select row `index`, its reuse act, a depth, Extrude. */
    private void reuse(long sketchId, int actId, String depth) {
        openHistory();
        touchTab(R.id.feature_history_tab_sketches);
        final int index = rowIndexOf(sketchId);
        touchBrowserRow(index);
        assertTrue("the act is offered", rig.shown(actId));
        rig.touchId(actId);
        assertEquals("the reuse opens in Ready", NativeViewport.SKETCH_READY, sketchState());
        assertEquals(sketchId, NativeViewport.sketchReuseSketchId());
        assertFalse("Back to Sketch is withdrawn over a shared sketch",
                rig.shown(R.id.back_to_sketch));
        commitExtrude(-1, depth);
    }

    // -----------------------------------------------------------------------
    // History surface
    // -----------------------------------------------------------------------

    private void openHistory() {
        rig.refresh();
        if (!rig.on((activity, workspace) -> workspace.featureHistory().isOpen())) {
            assertTrue("the History control is drawn", rig.shown(R.id.feature_history_action));
            rig.touchId(R.id.feature_history_action);
        }
        assertTrue("the History surface opened", rig.on((activity, workspace) ->
                workspace.featureHistory().isOpen()));
    }

    private void closeHistory() {
        if (rig.on((activity, workspace) -> workspace.featureHistory().isOpen())) {
            rig.touchId(R.id.feature_history_action);
        }
    }

    private void touchTab(int id) {
        rig.touchId(id);
    }

    private List<String> browserRowTexts() {
        return rig.on((activity, workspace) -> {
            final List<String> out = new ArrayList<>();
            final android.widget.LinearLayout rows = workspace.featureHistory().rowsContainer();
            for (int i = 0; i < rows.getChildCount(); i++) {
                out.add(((TextView) rows.getChildAt(i)).getText().toString());
            }
            return out;
        });
    }

    private void touchBrowserRow(final int index) {
        rig.touchView((activity, workspace) ->
                workspace.featureHistory().rowsContainer().getChildAt(index));
    }

    private int rowIndexOf(long sketchId) {
        final SketchBrowserPresentation.Inventory inventory = inventory();
        for (int i = 0; i < inventory.rows.size(); i++) {
            if (inventory.rows.get(i).id == sketchId) {
                return i;
            }
        }
        throw new AssertionError("no sketch " + sketchId);
    }

    private String detailText() {
        return rig.on((activity, workspace) ->
                workspace.featureHistory().detailText().getText().toString());
    }

    private List<Rect> actChipRects() {
        return rig.on((activity, workspace) -> {
            final List<Rect> out = new ArrayList<>();
            collectRects(workspace.featureHistory().actionsContainer(), out);
            return out;
        });
    }

    private static void collectRects(View view, List<Rect> out) {
        if (view instanceof ViewGroup) {
            final ViewGroup group = (ViewGroup) view;
            for (int i = 0; i < group.getChildCount(); i++) {
                collectRects(group.getChildAt(i), out);
            }
            return;
        }
        if (view.isShown() && view.isClickable()) {
            final int[] at = new int[2];
            view.getLocationInWindow(at);
            out.add(new Rect(at[0], at[1], at[0] + view.getWidth(), at[1] + view.getHeight()));
        }
    }

    /** Whether the clickable view under a control's centre is that control. */
    private boolean ownsItsCentre(final int id) {
        return rig.on((activity, workspace) -> {
            final View target = workspace.findViewById(id);
            if (target == null || !target.isShown()) {
                return false;
            }
            final int[] at = new int[2];
            target.getLocationInWindow(at);
            final float cx = at[0] + target.getWidth() / 2f;
            final float cy = at[1] + target.getHeight() / 2f;
            return clickableAt(activity.getWindow().getDecorView(), cx, cy) == target;
        });
    }

    // -----------------------------------------------------------------------
    // Native reads
    // -----------------------------------------------------------------------

    private SketchBrowserPresentation.Inventory inventory() {
        final double[] header = new double[NativeViewport.SKETCH_INV_HEADER_SIZE];
        final double[] rows = new double[NativeViewport.SKETCH_INV_MAX_ROWS
                * NativeViewport.SKETCH_INV_ROW_SIZE];
        final int count = NativeViewport.cadSketchInventory(body, header, rows);
        return SketchBrowserPresentation.fromNative(header, rows, count);
    }

    private double[] inventoryRaw(long sketchId) {
        final double[] header = new double[NativeViewport.SKETCH_INV_HEADER_SIZE];
        final double[] rows = new double[NativeViewport.SKETCH_INV_MAX_ROWS
                * NativeViewport.SKETCH_INV_ROW_SIZE];
        final int count = NativeViewport.cadSketchInventory(body, header, rows);
        for (int i = 0; i < count; i++) {
            final int at = i * NativeViewport.SKETCH_INV_ROW_SIZE;
            if ((long) rows[at + NativeViewport.SKETCH_INV_ID] == sketchId) {
                return Arrays.copyOfRange(rows, at, at + NativeViewport.SKETCH_INV_ROW_SIZE);
            }
        }
        throw new AssertionError("no sketch " + sketchId + " in " + count + " rows");
    }

    private double volume() {
        return measure()[NativeViewport.CAD_MEASURE_VOLUME];
    }

    private double[] measure() {
        final double[] out = new double[NativeViewport.CAD_MEASURE_SIZE];
        assertTrue("the body measures", NativeViewport.cadBodyMeasure(body, out));
        return out;
    }

    private double featureAngle() {
        final double[] info = new double[NativeViewport.CAD_FEATURE_INFO_SIZE];
        assertTrue(NativeViewport.cadFeatureInfo(body, 0, info));
        return info[NativeViewport.CAD_FEATURE_ANGLE];
    }

    private static long selectedOrLastEntity() {
        final double[] drawn = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        assertTrue("an entity is selected", NativeViewport.sketchSelectedEntity(drawn));
        return (long) drawn[NativeViewport.SKETCH_ENTITY_ID];
    }

    /** The edited sketch's one entity: the selection, or the only one. */
    private long selectedOrOnlyEntity() {
        final double[] drawn = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        if (NativeViewport.sketchSelectedEntity(drawn)) {
            return (long) drawn[NativeViewport.SKETCH_ENTITY_ID];
        }
        assertEquals("one entity", 1, sketchEntityCount());
        final long[] ids = new long[4];
        assertEquals(1, NativeViewport.sketchEntityIds(ids));
        return ids[0];
    }

    private static long[] faceHandles() {
        final int count = NativeViewport.sketchProfiles(null);
        final long[] out = new long[Math.max(count, 0)];
        assertEquals(count, NativeViewport.sketchProfiles(out));
        return out;
    }

    private static long[] selected() {
        final List<Long> out = new ArrayList<>();
        final double[] info = new double[NativeViewport.SKETCH_REGION_INFO_SIZE];
        for (long handle : faceHandles()) {
            assertTrue(NativeViewport.sketchProfileInfo(handle, info));
            if (info[NativeViewport.SKETCH_REGION_SELECTED] != 0.0) {
                out.add(handle);
            }
        }
        final long[] result = new long[out.size()];
        for (int i = 0; i < result.length; i++) {
            result[i] = out.get(i);
        }
        Arrays.sort(result);
        return result;
    }

    /** The cell whose native interior point lies within half a unit of (u, v). */
    private long cellAt(double u, double v) {
        final float[] want = new float[2];
        assertTrue(NativeViewport.sketchScreenPoint(u, v, want));
        final float[] edge = new float[2];
        assertTrue(NativeViewport.sketchScreenPoint(u + 0.5, v, edge));
        final double half = Math.hypot(edge[0] - want[0], edge[1] - want[1]);
        final double[] info = new double[NativeViewport.SKETCH_REGION_INFO_SIZE];
        long best = -1;
        double bestDistance = Double.MAX_VALUE;
        for (long handle : faceHandles()) {
            assertTrue(NativeViewport.sketchProfileInfo(handle, info));
            final double d = Math.hypot(info[NativeViewport.SKETCH_REGION_SCREEN_X] - want[0],
                    info[NativeViewport.SKETCH_REGION_SCREEN_Y] - want[1]);
            if (d < bestDistance) {
                bestDistance = d;
                best = handle;
            }
        }
        assertTrue("a cell's interior point stands inside its square", bestDistance < half);
        return best;
    }

    private static double[] candidateMeasure() {
        final double[] out = new double[NativeViewport.CANDIDATE_MEASURE_SIZE];
        assertTrue("the candidate measures", NativeViewport.sketchCandidateMeasure(out));
        assertEquals("the candidate is valid", NativeViewport.CAD_OK,
                (int) out[NativeViewport.CANDIDATE_MEASURE_STATUS]);
        return new double[]{out[NativeViewport.CANDIDATE_MEASURE_VOLUME],
                out[NativeViewport.CANDIDATE_MEASURE_COMPONENTS]};
    }

    // -----------------------------------------------------------------------
    // Real window touches
    // -----------------------------------------------------------------------

    /**
     * One real tap that toggles exactly `handle`: at the cell's own centre
     * (u, v) first, re-projected through the live camera, and -- only when the
     * extrude arrow's head or chrome stands on that one pixel -- the nearest of
     * the cell's own quarter points. A tap that toggled another cell fails.
     */
    private void tapCell(long handle, double u, double v) {
        // A cell under chrome or off the viewport is first brought into the
        // open by a real two-finger pan -- the navigation Ready offers -- and
        // only then tapped at its own centre (D09 run 1: the lower-left cell
        // stood under the chrome at every one of its points).
        bringOnScreen(u, v);
        final double[][] points = {{u, v}, {u - 0.25, v - 0.25}, {u + 0.25, v - 0.25},
                {u - 0.25, v + 0.25}, {u + 0.25, v + 0.25}};
        for (double[] p : points) {
            final float[] at = new float[2];
            assertTrue(NativeViewport.sketchScreenPoint(p[0], p[1], at));
            if (headDistance(at[0], at[1]) < 48.0f) {
                continue;
            }
            final float[] window = viewportToWindow(at);
            if (blockerAt(window) != null) {
                continue;
            }
            final long[] before = selected();
            realTap(window);
            final long[] after = selected();
            assertTrue("the tap at (" + p[0] + ", " + p[1] + ") toggled cell " + handle,
                    Arrays.binarySearch(after, handle) >= 0 && after.length == before.length + 1);
            return;
        }
        throw new AssertionError("cell " + handle + " at (" + u + ", " + v
                + ") is under the arrow head or chrome at every point of its own");
    }

    private void bringOnScreen(double u, double v) {
        final float w = onWorkspace(rule.getScenario(), (activity, workspace) ->
                (float) workspace.findViewById(R.id.viewport_surface).getWidth());
        final float h = onWorkspace(rule.getScenario(), (activity, workspace) ->
                (float) workspace.findViewById(R.id.viewport_surface).getHeight());
        float sign = 1.0f;
        for (int pan = 0; pan < 8; pan++) {
            final float[] at = new float[2];
            assertTrue(NativeViewport.sketchScreenPoint(u, v, at));
            if (blockerAt(viewportToWindow(at)) == null && at[0] > 160f && at[0] < w - 160f
                    && at[1] > 200f && at[1] < h - 260f) {
                return;
            }
            final float dx = Math.max(-350f, Math.min(350f, w * 0.5f - at[0]));
            final float dy = Math.max(-350f, Math.min(350f, h * 0.45f - at[1]));
            final double before = Math.hypot(w * 0.5f - at[0], h * 0.45f - at[1]);
            twoFingerPan(sign * dx, sign * dy);
            final float[] now = new float[2];
            assertTrue(NativeViewport.sketchScreenPoint(u, v, now));
            rig.fact("pan", "(" + u + "," + v + ") " + Arrays.toString(at) + " -> "
                    + Arrays.toString(now));
            if (Math.hypot(w * 0.5f - now[0], h * 0.45f - now[1]) > before) {
                sign = -sign;
            }
        }
    }

    private void twoFingerPan(final float dx, final float dy) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final float density = activity.getResources().getDisplayMetrics().density;
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final View root = activity.getWindow().getDecorView();
            final int[] vp = new int[2];
            viewport.getLocationInWindow(vp);
            final float cx = vp[0] + viewport.getWidth() * 0.5f;
            final float cy = vp[1] + viewport.getHeight() * 0.6f;
            final float half = 60f * density;
            final long down = SystemClock.uptimeMillis();
            final int steps = 8;
            final MotionEvent.PointerProperties[] props = new MotionEvent.PointerProperties[2];
            for (int i = 0; i < 2; i++) {
                props[i] = new MotionEvent.PointerProperties();
                props[i].id = i;
                props[i].toolType = MotionEvent.TOOL_TYPE_FINGER;
            }
            final MotionEvent.PointerCoords[] coords = new MotionEvent.PointerCoords[2];
            for (int step = 0; step <= steps; step++) {
                final float t = (float) step / steps;
                for (int i = 0; i < 2; i++) {
                    coords[i] = new MotionEvent.PointerCoords();
                    coords[i].x = cx + dx * t + (i == 0 ? -half : half);
                    coords[i].y = cy + dy * t;
                    coords[i].pressure = 1f;
                    coords[i].size = 1f;
                }
                final long time = down + 16L * step;
                if (step == 0) {
                    sendMulti(root, down, time, MotionEvent.ACTION_DOWN, 1, props, coords);
                    sendMulti(root, down, time, MotionEvent.ACTION_POINTER_DOWN
                            | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT), 2, props, coords);
                } else {
                    sendMulti(root, down, time, MotionEvent.ACTION_MOVE, 2, props, coords);
                }
            }
            final long end = down + 16L * (steps + 1);
            sendMulti(root, down, end, MotionEvent.ACTION_POINTER_UP
                    | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT), 2, props, coords);
            sendMulti(root, down, end + 8L, MotionEvent.ACTION_UP, 1, props, coords);
            return null;
        });
        settleLayout();
    }

    private static void sendMulti(View root, long downTime, long time, int action, int count,
                                  MotionEvent.PointerProperties[] props,
                                  MotionEvent.PointerCoords[] coords) {
        final MotionEvent event = MotionEvent.obtain(downTime, time, action, count, props, coords,
                0, 0, 1f, 1f, 0, 0, InputDevice.SOURCE_TOUCHSCREEN, 0);
        root.dispatchTouchEvent(event);
        event.recycle();
    }

    private String blockerAt(final float[] window) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final float density = activity.getResources().getDisplayMetrics().density;
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final View root = activity.getWindow().getDecorView();
            final int[] vp = new int[2];
            final int[] rp = new int[2];
            viewport.getLocationInWindow(vp);
            root.getLocationInWindow(rp);
            final float vx = window[0] - (vp[0] - rp[0]);
            final float vy = window[1] - (vp[1] - rp[1]);
            if (vx < 8 * density || vy < 8 * density || vx > viewport.getWidth() - 8 * density
                    || vy > viewport.getHeight() - 8 * density) {
                return "off-viewport";
            }
            final View owner = clickableAt(root, window[0] + rp[0], window[1] + rp[1]);
            if (owner == null || owner == viewport) {
                return null;
            }
            return owner.getId() != View.NO_ID
                    ? activity.getResources().getResourceEntryName(owner.getId())
                    : owner.getClass().getSimpleName();
        });
    }

    private static View clickableAt(View view, float wx, float wy) {
        if (view.getVisibility() != View.VISIBLE) {
            return null;
        }
        final int[] at = new int[2];
        view.getLocationInWindow(at);
        if (wx < at[0] || wy < at[1] || wx >= at[0] + view.getWidth()
                || wy >= at[1] + view.getHeight()) {
            return null;
        }
        if (view instanceof ViewGroup) {
            final ViewGroup group = (ViewGroup) view;
            for (int i = group.getChildCount() - 1; i >= 0; i--) {
                final View hit = clickableAt(group.getChildAt(i), wx, wy);
                if (hit != null) {
                    return hit;
                }
            }
        }
        return view.isClickable() || HUD_DOWN_TAKERS.contains(view.getId())
                || view.getId() == R.id.viewport_surface ? view : null;
    }

    private static final Set<Integer> HUD_DOWN_TAKERS = new HashSet<>(
            Arrays.asList(R.id.cad_extrude_depth_value, R.id.cad_extrude_second_value,
                    R.id.cad_extrude_depth_editor, R.id.cad_extrude_second_editor,
                    R.id.cad_extrude_panel, R.id.cad_canvas_edit_sketch));

    private static float headDistance(float x, float y) {
        final double[] tool = new double[NativeViewport.CAD_EXTRUDE_SIZE];
        NativeViewport.cadExtrudeToolState(tool);
        if (tool[NativeViewport.CAD_EXTRUDE_ACTIVE] == 0.0
                || tool[NativeViewport.CAD_EXTRUDE_HEAD_ON_SCREEN] == 0.0) {
            return Float.POSITIVE_INFINITY;
        }
        return (float) Math.hypot(x - tool[NativeViewport.CAD_EXTRUDE_HEAD_X],
                y - tool[NativeViewport.CAD_EXTRUDE_HEAD_Y]);
    }

    private void realTap(final float[] window) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View root = activity.getWindow().getDecorView();
            final long down = SystemClock.uptimeMillis();
            dispatch(root, down, down, MotionEvent.ACTION_DOWN, window[0], window[1],
                    MotionEvent.TOOL_TYPE_FINGER);
            dispatch(root, down, down + 40L, MotionEvent.ACTION_UP, window[0], window[1],
                    MotionEvent.TOOL_TYPE_FINGER);
            return null;
        });
        settleLayout();
    }

    /** A STYLUS press and release at a control's centre, through the window. */
    private void stylusTap(final int id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View target = workspace.findViewById(id);
            assertNotNull(target);
            final View root = activity.getWindow().getDecorView();
            final int[] at = new int[2];
            final int[] rp = new int[2];
            target.getLocationInWindow(at);
            root.getLocationInWindow(rp);
            final float x = at[0] - rp[0] + target.getWidth() / 2f;
            final float y = at[1] - rp[1] + target.getHeight() / 2f;
            final long down = SystemClock.uptimeMillis();
            dispatch(root, down, down, MotionEvent.ACTION_DOWN, x, y, MotionEvent.TOOL_TYPE_STYLUS);
            dispatch(root, down, down + 50L, MotionEvent.ACTION_UP, x, y,
                    MotionEvent.TOOL_TYPE_STYLUS);
            return null;
        });
        settleLayout();
    }

    private static void dispatch(View root, long downTime, long eventTime, int action, float x,
                                 float y, int toolType) {
        final MotionEvent.PointerProperties[] props = {new MotionEvent.PointerProperties()};
        props[0].id = 0;
        props[0].toolType = toolType;
        final MotionEvent.PointerCoords[] coords = {new MotionEvent.PointerCoords()};
        coords[0].x = x;
        coords[0].y = y;
        coords[0].pressure = 1f;
        coords[0].size = 1f;
        final int source = toolType == MotionEvent.TOOL_TYPE_STYLUS
                ? InputDevice.SOURCE_STYLUS | InputDevice.SOURCE_TOUCHSCREEN
                : InputDevice.SOURCE_TOUCHSCREEN;
        final MotionEvent event = MotionEvent.obtain(downTime, eventTime, action, 1, props, coords,
                0, 0, 1f, 1f, 0, 0, source, 0);
        root.dispatchTouchEvent(event);
        event.recycle();
    }

    private float[] viewportToWindow(float[] at) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final View root = activity.getWindow().getDecorView();
            final int[] vp = new int[2];
            final int[] rp = new int[2];
            viewport.getLocationInWindow(vp);
            root.getLocationInWindow(rp);
            return new float[]{at[0] + vp[0] - rp[0], at[1] + vp[1] - rp[1]};
        });
    }

    private void setCamera(float yaw, float pitch, float distance) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(NativeViewport.debugSetCameraPose(yaw, pitch, distance));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    private String string(int id, Object... args) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> activity.getString(id, args));
    }

    // -----------------------------------------------------------------------
    // Debug attribution tokens, read back from this process's own log
    // -----------------------------------------------------------------------

    private String mark() {
        final String marker = "CADR2_MARK_" + System.nanoTime() + "_" + (marks++);
        Log.i(TAG, marker);
        return marker;
    }

    private static List<String> tokensSince(String marker) {
        final List<String> tokens = new ArrayList<>();
        try {
            final Process process = Runtime.getRuntime().exec(
                    new String[]{"logcat", "-d", "-v", "raw", "-s", "ForgeShape:I"});
            boolean after = false;
            try (BufferedReader in = new BufferedReader(
                    new InputStreamReader(process.getInputStream(), "UTF-8"))) {
                String line;
                while ((line = in.readLine()) != null) {
                    if (line.contains(marker)) {
                        after = true;
                        tokens.clear();
                        continue;
                    }
                    if (after && line.trim().startsWith("FORGESHAPE_SUPPORT_CHOOSER")) {
                        tokens.add(line.trim());
                    }
                }
            }
            process.waitFor();
        } catch (IOException | InterruptedException error) {
            tokens.add("logcat_unreadable:" + error);
        }
        return tokens;
    }
}
