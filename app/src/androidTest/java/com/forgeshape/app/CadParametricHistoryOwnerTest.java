package com.forgeshape.app;

import static com.forgeshape.app.SketchTestSupport.dragSketch;
import static com.forgeshape.app.SketchTestSupport.selectTool;
import static com.forgeshape.app.SketchTestSupport.sketchEntityCount;
import static com.forgeshape.app.SketchTestSupport.sketchState;
import static com.forgeshape.app.SketchTestSupport.tapTapWorld;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.view.View;
import android.widget.EditText;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;

/**
 * Parametric History on the device ({@code MODELING-FOUNDATIONS-R1} A), on the
 * OWNER's own path: Home → New Project → CAD, a 2 × 2 rectangle extruded 1 m,
 * then a sketch on its top face with a circle CUT 0.5 m into it — a two-feature
 * chain. The History control in the history capsule lists it; a row opened by a
 * real window touch edits that step; later steps rebuild from it.
 *
 * <p>Asserted from native truth — the derived timeline, the body measures, the
 * encoded project and its fingerprint — and from what the History surface and
 * the regeneration issue card actually show. Screenshots are evidence only.
 */
@RunWith(AndroidJUnit4.class)
public final class CadParametricHistoryOwnerTest {

    private static final double POCKET_DEPTH = 0.5;
    private static final double VOLUME_TOLERANCE = 1e-4;

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private ModelingOwnerRig rig;
    private long body;
    private long rectangle;
    private double pocketRadius;

    @Before
    public void startAtHome() {
        rig = new ModelingOwnerRig(rule.getScenario(), "modeling-parametric-history", "MFPAR_DEVICE");
        rig.startAtHome();
    }

    @After
    public void restoreAProject() {
        rig.restore();
    }

    // =======================================================================
    // DEV-PAR-01: the History lists the chain; an earlier sketch edit rebuilds
    // the later Cut; one Undo; save and reopen keep the timeline.
    // =======================================================================

    @Test
    public void devPar01_history_lists_the_chain_and_a_base_sketch_edit_rebuilds_the_cut() {
        buildBlockWithPocket();
        final double pocketArea = circleArea(pocketRadius);
        assertEquals("block minus pocket", 4.0 - pocketArea * POCKET_DEPTH, volume(),
                VOLUME_TOLERANCE);

        openHistory();
        final List<String> texts = historyRowTexts();
        rig.fact("history.rows", texts);
        assertEquals("two sketches and two features", 4, texts.size());
        assertTrue(texts.get(0), texts.get(0).contains(string(R.string.history_row_sketch, 1)));
        assertTrue(texts.get(1), texts.get(1).startsWith(FeatureHistoryPresentation.MARK_OK));
        assertTrue(texts.get(3), texts.get(3).contains(string(R.string.sketch_operation_cut)));
        final FeatureHistoryPresentation.Model committed = timeline(false);
        assertEquals("rows by durable id: S1 F1 S2 F2", "S1 F1 S2 F2 ", rowKey(committed));
        rig.capture("01_history_open");

        // A REAL touch on "Sketch 1" opens the base sketch, staged.
        touchHistoryRow(0);
        assertEquals("the base sketch is open for editing", NativeViewport.SKETCH_EDITING,
                sketchState());
        assertEquals(body, NativeViewport.sketchEditingBodyId());
        assertEquals(1L, NativeViewport.sketchEditingFeatureId());
        assertEquals(NativeViewport.CAD_OK,
                rig.applyOnUi(() -> NativeViewport.sketchApplyRectangle(rectangle, 3.0, 2.0)));
        rig.press(R.id.finish_sketch);
        assertEquals(NativeViewport.SKETCH_READY, sketchState());
        assertFalse("no regeneration issue for a valid edit", rig.shown(R.id.regeneration_issue));
        final FeatureHistoryPresentation.Model staged = timeline(true);
        assertEquals("the staged chain rebuilds", NativeViewport.CAD_OK, staged.status);
        assertEquals("nothing changed before Finish", 4.0 - pocketArea * POCKET_DEPTH, volume(),
                VOLUME_TOLERANCE);

        rig.press(R.id.extrude_sketch);
        assertEquals("the edit is committed", NativeViewport.SKETCH_INACTIVE, sketchState());
        final double widened = 6.0 - pocketArea * POCKET_DEPTH;
        assertEquals("the base widened AND the later Cut rebuilt in it", widened, volume(),
                VOLUME_TOLERANCE);
        rig.capture("02_after_upstream_edit");

        // One step: Undo restores the 2 m base with its pocket, Redo the 3 m one.
        rig.press(R.id.undo_action);
        assertEquals(4.0 - pocketArea * POCKET_DEPTH, volume(), VOLUME_TOLERANCE);
        rig.press(R.id.redo_action);
        assertEquals(widened, volume(), VOLUME_TOLERANCE);

        // Save and reopen: the same chain, the same timeline.
        final byte[] saved = NativeViewport.encodeProject();
        assertNotNull(saved);
        final FeatureHistoryPresentation.Model beforeReopen = timeline(false);
        assertEquals(NativeViewport.PROJECT_OK,
                rig.applyOnUi(() -> NativeViewport.loadProject(saved)));
        body = NativeViewport.sceneActiveBodyId();
        final FeatureHistoryPresentation.Model reopened = timeline(false);
        assertEquals("the reopened timeline is the saved one", rowKey(beforeReopen),
                rowKey(reopened));
        assertEquals(widened, volume(), VOLUME_TOLERANCE);
        assertArrayEquals("reopening wrote nothing new", saved, NativeViewport.encodeProject());
        rig.fact("par01.volume_after", volume());
    }

    // =======================================================================
    // DEV-PAR-02: an edit that breaks the later Cut names it; Cancel restores
    // the exact project; Fix returns to the sketch and a good value commits.
    // =======================================================================

    @Test
    public void devPar02_a_breaking_edit_names_the_failing_cut_and_cancel_restores_exactly() {
        buildBlockWithPocket();
        final byte[] before = NativeViewport.encodeProject();
        final long fingerprint = NativeViewport.projectFingerprint();
        final double volumeBefore = volume();

        openHistory();
        touchHistoryRow(0);
        // A 0.6 m wide base: the pocket now stands outside it.
        assertEquals(NativeViewport.CAD_OK,
                rig.applyOnUi(() -> NativeViewport.sketchApplyRectangle(rectangle, 0.6, 2.0)));
        rig.press(R.id.finish_sketch);
        assertEquals(NativeViewport.SKETCH_READY, sketchState());
        final FeatureHistoryPresentation.Model staged = timeline(true);
        rig.fact("par02.staged", rowKey(staged) + " status=" + staged.status + " failed="
                + staged.failedFeature);
        assertEquals("the first failing feature is the Cut, by id", 2L, staged.failedFeature);
        assertEquals(NativeViewport.TIMELINE_STATE_FAILED, staged.rows.get(3).state);

        assertTrue("the regeneration issue card is shown", rig.shown(R.id.regeneration_issue));
        final String title = rig.on((activity, workspace) -> ((TextView) workspace.findViewById(
                R.id.regeneration_issue_title)).getText().toString());
        rig.fact("par02.issue_title", title);
        assertTrue(title, title.contains(string(R.string.sketch_operation_cut)));
        final List<String> cardRows = rig.on((activity, workspace) -> {
            final List<View> rows = new ArrayList<>();
            ModelingOwnerRig.collectById(workspace.regenerationIssue(), R.id.feature_history_row, rows);
            final List<String> out = new ArrayList<>();
            for (View row : rows) out.add(((TextView) row).getText().toString());
            return out;
        });
        rig.fact("par02.issue_rows", cardRows);
        boolean marked = false;
        for (String row : cardRows) {
            marked |= row.startsWith(FeatureHistoryPresentation.MARK_FAILED);
        }
        assertTrue("the failing row is marked by shape", marked);
        rig.capture("03_regeneration_issue");

        // The project has not moved, and a Finish attempt is refused.
        assertEquals(volumeBefore, volume(), 0.0);
        if (rig.shown(R.id.extrude_sketch)) {
            rig.press(R.id.extrude_sketch);
        }
        assertEquals("still staged", NativeViewport.SKETCH_READY, sketchState());
        assertEquals(volumeBefore, volume(), 0.0);

        // Cancel edit: exactly the project there was.
        rig.touchId(R.id.regeneration_issue_cancel);
        assertEquals(NativeViewport.SKETCH_INACTIVE, sketchState());
        assertFalse(rig.shown(R.id.regeneration_issue));
        assertArrayEquals("byte-identical after Cancel", before, NativeViewport.encodeProject());
        assertEquals("fingerprint-identical after Cancel", fingerprint,
                NativeViewport.projectFingerprint());

        // Fix: the same break, then Fix returns to the sketch, a good width commits.
        openHistory();
        touchHistoryRow(0);
        assertEquals(NativeViewport.CAD_OK,
                rig.applyOnUi(() -> NativeViewport.sketchApplyRectangle(rectangle, 0.6, 2.0)));
        rig.press(R.id.finish_sketch);
        assertTrue(rig.shown(R.id.regeneration_issue));
        rig.touchId(R.id.regeneration_issue_fix);
        assertEquals("Fix returns to the sketch", NativeViewport.SKETCH_EDITING, sketchState());
        assertFalse(rig.shown(R.id.regeneration_issue));
        assertEquals(NativeViewport.CAD_OK,
                rig.applyOnUi(() -> NativeViewport.sketchApplyRectangle(rectangle, 2.5, 2.0)));
        rig.press(R.id.finish_sketch);
        assertFalse("a valid value withdraws the card", rig.shown(R.id.regeneration_issue));
        rig.press(R.id.extrude_sketch);
        assertEquals(NativeViewport.SKETCH_INACTIVE, sketchState());
        assertEquals(5.0 - circleArea(pocketRadius) * POCKET_DEPTH, volume(), VOLUME_TOLERANCE);
        rig.capture("04_fixed");
    }

    // =======================================================================
    // DEV-PAR-03: a Revolve's angle from its History row, exact.
    // =======================================================================

    @Test
    public void devPar03_a_revolve_row_edits_the_exact_angle_and_undo_restores_it() {
        rig.press(R.id.home_new_project);
        rig.press(R.id.new_project_cad);
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), 0.4, -0.4, 1.2, 0.4);
        final long square = selectedEntity();
        assertEquals(NativeViewport.CAD_OK,
                rig.applyOnUi(() -> NativeViewport.sketchApplyRectangle(square, 0.8, 0.8)));
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        dragSketch(rule.getScenario(), 0.0, -0.6, 0.0, 0.6);
        final long line = selectedEntity();
        assertEquals(NativeViewport.CAD_OK, rig.applyOnUi(
                () -> NativeViewport.sketchApplyLine(line, 0.0, -0.6, 0.0, 0.6)));
        assertEquals(2, sketchEntityCount());
        rig.press(R.id.finish_sketch);
        assertEquals(NativeViewport.CAD_OK, rig.applyOnUi(NativeViewport::sketchBeginRevolve));
        assertEquals(NativeViewport.CAD_OK,
                rig.applyOnUi(() -> NativeViewport.sketchSetRevolveAxis(line, 0)));
        rig.press(R.id.revolve_sketch);
        assertTrue("the revolve created the project: " + rig.statusLine(), NativeViewport.projectOpen());
        body = NativeViewport.sceneActiveBodyId();
        assertEquals(360.0, featureAngle(), 0.0);

        openHistory();
        final List<String> texts = historyRowTexts();
        rig.fact("par03.rows", texts);
        assertEquals(2, texts.size());
        touchHistoryRow(1);
        assertEquals("the Revolve opens staged in Ready", NativeViewport.SKETCH_READY, sketchState());
        assertEquals(NativeViewport.CAD_OK,
                rig.applyOnUi(() -> NativeViewport.sketchSetRevolveAngle(137.5)));
        rig.press(R.id.revolve_sketch);
        assertEquals(NativeViewport.SKETCH_INACTIVE, sketchState());
        assertEquals("the exact angle", 137.5, featureAngle(), 0.0);
        final FeatureHistoryPresentation.Model timeline = timeline(false);
        assertEquals(137.5, timeline.rows.get(1).angle, 0.0);
        rig.capture("05_revolve_edited");
        rig.press(R.id.undo_action);
        assertEquals(360.0, featureAngle(), 0.0);
        rig.press(R.id.redo_action);
        assertEquals(137.5, featureAngle(), 0.0);
    }

    // -----------------------------------------------------------------------
    // The chain, built the way the OWNER builds it
    // -----------------------------------------------------------------------

    private void buildBlockWithPocket() {
        rig.press(R.id.home_new_project);
        rig.press(R.id.new_project_cad);
        assertEquals("New CAD lands in a sketch", NativeViewport.SKETCH_EDITING, sketchState());
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), -1.0, -1.0, 1.0, 1.0);
        rectangle = selectedEntity();
        assertEquals(NativeViewport.CAD_OK,
                rig.applyOnUi(() -> NativeViewport.sketchApplyRectangle(rectangle, 2.0, 2.0)));
        rig.press(R.id.finish_sketch);
        assertEquals(NativeViewport.SKETCH_READY, sketchState());
        rig.press(R.id.extrude_sketch);
        assertTrue("the first Extrude created the project", NativeViewport.projectOpen());
        body = NativeViewport.sceneActiveBodyId();
        assertEquals(4.0, volume(), VOLUME_TOLERANCE);

        // A sketch on the top face: New Sketch, then a double tap on the cap.
        assertTrue(NativeViewport.debugSetCameraPose(0.35f, 0.6f, 9.0f));
        rig.refresh();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openAddPrimitive(workspace);
            workspace.addPrimitivePalette().findViewById(R.id.add_sketch).performClick();
            return null;
        });
        settleLayout();
        tapTapWorld(rule.getScenario(), 0.0, 0.0, 1.0);
        assertEquals("a face sketch is open", NativeViewport.SKETCH_EDITING, sketchState());
        selectTool(rule.getScenario(), R.id.tool_rail_circle);
        dragSketch(rule.getScenario(), 0.5, 0.5, 0.65, 0.5);
        final long circle = selectedEntity();
        assertEquals(NativeViewport.CAD_OK,
                rig.applyOnUi(() -> NativeViewport.sketchApplyCircle(circle, 0.15)));
        final double[] drawn = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        assertTrue(NativeViewport.sketchSelectedEntity(drawn));
        pocketRadius = 0.15;
        rig.fact("pocket.centre", drawn[NativeViewport.SKETCH_ENTITY_VALUES] + ","
                + drawn[NativeViewport.SKETCH_ENTITY_VALUES + 1]);
        rig.press(R.id.finish_sketch);
        assertEquals(NativeViewport.SKETCH_READY, sketchState());
        assertEquals(NativeViewport.CAD_OK,
                rig.applyOnUi(() -> NativeViewport.sketchSetOperation(NativeViewport.OPERATION_CUT)));
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openPrecision(workspace);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final EditText field = workspace.sketchEditor().findViewById(R.id.field_extrude_depth);
            field.setText(Double.toString(POCKET_DEPTH));
            final View commit = workspace.findViewById(R.id.sketch_extrude_commit);
            assertNotNull("the precision surface's Extrude", commit);
            commit.performClick();
            return null;
        });
        settleLayout();
        assertEquals("the Cut committed: " + rig.statusLine(), NativeViewport.SKETCH_INACTIVE,
                sketchState());
        assertEquals("the Cut changed the SAME body", body, NativeViewport.sceneActiveBodyId());
        assertEquals(2, NativeViewport.cadFeatureCount(body));
    }

    // -----------------------------------------------------------------------
    // History surface
    // -----------------------------------------------------------------------

    private void openHistory() {
        rig.refresh();
        assertTrue("the History control is drawn for a CAD body",
                rig.shown(R.id.feature_history_action));
        rig.touchId(R.id.feature_history_action);
        assertTrue("the History surface opened", rig.on((activity, workspace) ->
                workspace.featureHistory().isOpen()));
    }

    private List<String> historyRowTexts() {
        return rig.on((activity, workspace) -> {
            final List<String> out = new ArrayList<>();
            final android.widget.LinearLayout rows = workspace.featureHistory().rowsContainer();
            for (int i = 0; i < rows.getChildCount(); i++) {
                out.add(((TextView) rows.getChildAt(i)).getText().toString());
            }
            return out;
        });
    }

    /** A REAL window touch on one History row. */
    private void touchHistoryRow(final int index) {
        rig.touchView((activity, workspace) ->
                workspace.featureHistory().rowsContainer().getChildAt(index));
    }

    private FeatureHistoryPresentation.Model timeline(boolean staged) {
        final double[] header = new double[NativeViewport.TIMELINE_HEADER_SIZE];
        final double[] rows =
                new double[NativeViewport.TIMELINE_MAX_ROWS * NativeViewport.TIMELINE_ROW_SIZE];
        final int count = NativeViewport.cadTimeline(body, staged, header, rows);
        return FeatureHistoryPresentation.fromNative(FeatureHistoryPresentation.DOMAIN_CAD, header,
                rows, count);
    }

    private static String rowKey(FeatureHistoryPresentation.Model model) {
        final StringBuilder key = new StringBuilder();
        for (FeatureHistoryPresentation.Row row : model.rows) {
            key.append(row.isSketch() ? 'S' : 'F').append(row.id).append(' ');
        }
        return key.toString();
    }

    // -----------------------------------------------------------------------
    // Plumbing
    // -----------------------------------------------------------------------

    private static long selectedEntity() {
        final double[] drawn = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        assertTrue("an entity is selected", NativeViewport.sketchSelectedEntity(drawn));
        return (long) drawn[NativeViewport.SKETCH_ENTITY_ID];
    }

    private double volume() {
        final double[] out = new double[NativeViewport.CAD_MEASURE_SIZE];
        assertTrue("the body measures", NativeViewport.cadBodyMeasure(body, out));
        return out[NativeViewport.CAD_MEASURE_VOLUME];
    }

    private double featureAngle() {
        final double[] info = new double[NativeViewport.CAD_FEATURE_INFO_SIZE];
        assertTrue(NativeViewport.cadFeatureInfo(body, 0, info));
        return info[NativeViewport.CAD_FEATURE_ANGLE];
    }

    /** The 32-gon a circle is extruded as — the product's own polygon, not pi r^2. */
    private static double circleArea(double radius) {
        return 0.5 * 32 * radius * radius * Math.sin(2.0 * Math.PI / 32);
    }

    private String string(int id, Object... args) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> activity.getString(id, args));
    }
}
