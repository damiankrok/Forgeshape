package com.forgeshape.app;

import static com.forgeshape.app.SketchTestSupport.dragSketch;
import static com.forgeshape.app.SketchTestSupport.finishAndExtrude;
import static com.forgeshape.app.SketchTestSupport.selectTool;
import static com.forgeshape.app.SketchTestSupport.sketchEntityCount;
import static com.forgeshape.app.SketchTestSupport.sketchState;
import static com.forgeshape.app.SketchTestSupport.tapSketch;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.SystemClock;
import android.view.View;
import android.widget.EditText;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

/**
 * `E2E-CADUXR1-VIS`: deterministic screenshot evidence for `SKETCH-UX-R1`,
 * captured through the real chrome on the authoritative emulator.
 *
 * <p>One journey, twelve captures, in the order the brief names them: the
 * full-screen Home; the New Project page; the immediate flat XY sketch; the
 * navigator at its default +Z; the navigator on a different principal plane;
 * the navigator rotated; a selected Line with its technical dimension; the
 * numeric length editor open; an Arc selected; a Spline selected; Edit Sketch
 * on a committed body; and the regenerated body after Finish.
 *
 * <p>Every capture is written beside a line of <b>measurable facts</b> — an
 * element's presence and bounds, a plane, a view state, a length, a body count
 * — and <b>nothing aesthetic is asserted</b>. These frames are for the owner to
 * look at; they are not an approval of how anything looks. Frames come from
 * {@code UiAutomation.takeScreenshot()}, which captures the composed display,
 * so the Vulkan viewport is in the picture rather than a black hole where a
 * {@code SurfaceView} sits.
 *
 * <p>No control is located by coordinate: viewport pixels come from
 * {@code sketchScreenPoint} and every control from its semantic id.
 */
@RunWith(AndroidJUnit4.class)
public final class SketchUxVisualEvidenceTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private File outDir;
    private final List<String> facts = new ArrayList<>();
    private int captureIndex;

    @Before
    public void startAtHome() {
        outDir = new File(InstrumentationRegistry.getInstrumentation().getTargetContext()
                .getExternalFilesDir(null), "evidence/cad-a3-c2-sketch-ux-r1");
        //noinspection ResultOfMethodCallIgnored
        outDir.mkdirs();
        for (File stale : orEmpty(outDir.listFiles())) {
            //noinspection ResultOfMethodCallIgnored
            stale.delete();
        }
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.dismissRecoveryPromptForTest();
            workspace.showHomeAsFirstLaunchForTest();
            return null;
        });
        settleLayout();
    }

    @After
    public void leaveAProjectBehind() {
        writeFacts();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.supportChooserCancel();
            NativeViewport.sketchCancel();
            NativeViewport.enterConstructionMode();
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
    }

    @Test
    public void e2eCadUxR1Vis_theWholeJourneyInTwelveCaptures() {
        // 1. The full-screen Home page.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View home = workspace.findViewById(R.id.home_surface);
            final View root = (View) home.getParent();
            assertTrue(workspace.homeVisible());
            fact("home_visible", true);
            fact("home_is_full_window",
                    home.getWidth() == root.getWidth() && home.getHeight() == root.getHeight());
            fact("home_size_px", home.getWidth() + "x" + home.getHeight());
            fact("window_size_px", root.getWidth() + "x" + root.getHeight());
            fact("editor_toolbar_shown", workspace.findViewById(R.id.global_toolbar).isShown());
            fact("project_open", NativeViewport.projectOpen());
            fact("body_count", NativeViewport.sceneBodyCount());
            bounds(workspace, "wordmark", R.id.start_page_wordmark);
            bounds(workspace, "headline", R.id.start_page_headline);
            bounds(workspace, "home_new_project", R.id.home_new_project);
            bounds(workspace, "home_open_file", R.id.home_open_file);
            return null;
        });
        capture("01_home_full_screen");

        // 2. The New Project page.
        press(R.id.home_new_project);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View page = workspace.findViewById(R.id.new_project_chooser);
            final View root = (View) page.getParent();
            assertTrue(workspace.newProjectChooserVisible());
            fact("new_project_is_full_window",
                    page.getWidth() == root.getWidth() && page.getHeight() == root.getHeight());
            fact("home_still_visible", workspace.homeVisible());
            bounds(workspace, "new_project_cad", R.id.new_project_cad);
            bounds(workspace, "new_project_sculpt", R.id.new_project_sculpt);
            bounds(workspace, "new_project_back", R.id.new_project_cancel);
            return null;
        });
        capture("02_new_project_page");

        // 3. The immediate flat XY sketch.
        press(R.id.new_project_cad);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
            fact("support_chooser_active", NativeViewport.supportChooserActive());
            fact("sketch_state", sketchState());
            fact("project_open", NativeViewport.projectOpen());
            fact("projection_mode", NativeViewport.projectionMode());
            fact("grid_step_m", NativeViewport.sketchGridStep());
            bounds(workspace, "tool_rail_arc", R.id.tool_rail_arc);
            bounds(workspace, "tool_rail_spline", R.id.tool_rail_spline);
            bounds(workspace, "finish_sketch", R.id.finish_sketch);
            return null;
        });
        capture("03_immediate_flat_sketch");

        // 4. The navigator at its default: XY along +Z, unrotated.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            viewFacts();
            bounds(workspace, "navigator", R.id.sketch_orientation_navigator);
            bounds(workspace, "navigator_label", R.id.sketch_navigator_view_label);
            bounds(workspace, "navigator_flip", R.id.sketch_navigator_flip);
            // The one geometric claim worth pinning: it stands on the drawing,
            // never on the Tool Rail.
            fact("navigator_overlaps_tool_rail",
                    overlaps(workspace.findViewById(R.id.sketch_orientation_navigator),
                             workspace.findViewById(R.id.tool_rail_select)));
            return null;
        });
        capture("04_navigator_default_plus_z");

        // 5. The navigator on a different principal plane.
        press(R.id.sketch_navigator_plane_yz);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            viewFacts();
            return null;
        });
        capture("05_navigator_other_plane");

        // 6. The navigator rotated +90 degrees, back on XY.
        press(R.id.sketch_navigator_plane_xy);
        press(R.id.sketch_navigator_rotate_cw);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            viewFacts();
            return null;
        });
        capture("06_navigator_rotated_plus_90");
        press(R.id.sketch_navigator_rotate_ccw);  // square again for the drawing below

        // 7. A selected Line with its technical dimension.
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        dragSketch(rule.getScenario(), 0.0, 0.0, 3.0, 4.0);
        assertEquals(1, sketchEntityCount());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] dimension = new double[NativeViewport.SKETCH_DIMENSION_SIZE];
            assertTrue(NativeViewport.sketchLineDimension(dimension));
            fact("dimension_entity", dimension[NativeViewport.SKETCH_DIMENSION_ENTITY]);
            fact("dimension_length_m", dimension[NativeViewport.SKETCH_DIMENSION_LENGTH]);
            fact("dimension_label_text",
                    ((android.widget.TextView) workspace.findViewById(R.id.sketch_dimension_value))
                            .getText());
            bounds(workspace, "dimension_label", R.id.sketch_dimension_label);
            return null;
        });
        capture("07_line_dimension");

        // 8. The numeric length editor open.
        press(R.id.sketch_dimension_value);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(workspace.sketchDimensionLabel().editorOpen());
            fact("dimension_editor_open", true);
            bounds(workspace, "dimension_editor", R.id.sketch_dimension_editor);
            bounds(workspace, "dimension_field", R.id.field_sketch_line_length);
            return null;
        });
        capture("08_length_editor_open");

        // The typed length lands: 5 m becomes exactly 10 m, P0 unmoved.
        typeLengthAndApply("10");
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] dimension = new double[NativeViewport.SKETCH_DIMENSION_SIZE];
            assertTrue(NativeViewport.sketchLineDimension(dimension));
            fact("length_after_typing_m", dimension[NativeViewport.SKETCH_DIMENSION_LENGTH]);
            return null;
        });

        // Start a clean sketch for the curves, so each capture shows one thing.
        restartSketch();

        // 9. An Arc selected.
        selectTool(rule.getScenario(), R.id.tool_rail_arc);
        dragSketch(rule.getScenario(), 1.0, 0.0, -1.0, 0.0);
        tapSketch(rule.getScenario(), 0.0, 1.0);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(1, sketchEntityCount());
            fact("entity_kind", selectedEntityKind());
            fact("entity_is_arc",
                    selectedEntityKind() == NativeViewport.SKETCH_ENTITY_KIND_ARC);
            fact("sketch_entities", sketchEntityCount());
            return null;
        });
        capture("09_arc_selected");

        // 10. A Spline selected, beside the arc.
        selectTool(rule.getScenario(), R.id.tool_rail_spline);
        tapSketch(rule.getScenario(), -1.5, -1.5);
        tapSketch(rule.getScenario(), -0.5, -0.75);
        tapSketch(rule.getScenario(), 0.5, -1.5);
        tapSketch(rule.getScenario(), 1.5, -0.75);
        tapSketch(rule.getScenario(), 1.5, -0.75);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("the spline was placed beside the arc; last status was "
                            + NativeViewport.cadStatusToken(NativeViewport.sketchLastStatus()),
                    2, sketchEntityCount());
            fact("entity_kind", selectedEntityKind());
            fact("entity_is_spline",
                    selectedEntityKind() == NativeViewport.SKETCH_ENTITY_KIND_SPLINE);
            fact("sketch_entities", sketchEntityCount());
            return null;
        });
        capture("10_spline_selected");

        // A curve profile extruded into the first body: the arc closed by a
        // line. The spline is deleted first, so the sketch closes exactly one.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.CAD_OK, NativeViewport.sketchDeleteSelected());
            return null;
        });
        settleLayout();
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        dragSketch(rule.getScenario(), -1.0, 0.0, 1.0, 0.0);
        finishAndExtrude(rule.getScenario(), "1");
        final long body = NativeViewport.sceneActiveBodyId();
        assertTrue(NativeViewport.sceneActiveBodyIsCad());

        // 11. Edit Sketch on the committed body.
        openEditSketch();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(body, NativeViewport.sketchEditingBodyId());
            fact("editing_body_id", NativeViewport.sketchEditingBodyId());
            fact("sketch_state", sketchState());
            fact("sketch_entities", sketchEntityCount());
            fact("undo_depth", NativeViewport.constructionUndoDepth());
            fact("body_count", NativeViewport.sceneBodyCount());
            bounds(workspace, "navigator", R.id.sketch_orientation_navigator);
            return null;
        });
        capture("11_edit_sketch_open");

        // 12. Finish the edit: the body regenerated, one history step.
        //
        // The edit is a BIGGER ARC on the same chord. A line-length edit is
        // deliberately NOT used here: on a closed chain it opens the profile,
        // and Extrude then refuses by name — which is the designed behaviour
        // (`CADUXR1-24` proves it) and not what a "regenerated body" frame is
        // meant to show. Redrawing the arc keeps the chain closed on the same
        // two endpoints, because the new arc's ends snap to the line's own.
        final int undoBefore = NativeViewport.constructionUndoDepth();
        selectTool(rule.getScenario(), R.id.tool_rail_select);
        tapSketch(rule.getScenario(), 0.0, 1.0);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("the arc is selected", NativeViewport.SKETCH_ENTITY_KIND_ARC,
                    selectedEntityKind());
            assertEquals(NativeViewport.CAD_OK, NativeViewport.sketchDeleteSelected());
            return null;
        });
        settleLayout();
        selectTool(rule.getScenario(), R.id.tool_rail_arc);
        dragSketch(rule.getScenario(), 1.0, 0.0, -1.0, 0.0);
        tapSketch(rule.getScenario(), 0.0, 1.75);
        assertEquals("the chord and the new arc", 2, sketchEntityCount());
        finishAndExtrude(rule.getScenario(), "1");
        assertTrue(NativeViewport.debugSetCameraPose(0.7f, 0.5f, 7.0f));
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            fact("sketch_state", sketchState());
            fact("body_count", NativeViewport.sceneBodyCount());
            fact("undo_depth_before_finish", undoBefore);
            fact("undo_depth_after_finish", NativeViewport.constructionUndoDepth());
            fact("one_finish_is_one_undo",
                    NativeViewport.constructionUndoDepth() == undoBefore + 1);
            fact("body_is_cad", NativeViewport.sceneActiveBodyIsCad());
            return null;
        });
        capture("12_edit_finished_regenerated");
    }

    // -----------------------------------------------------------------------
    // Support
    // -----------------------------------------------------------------------

    /** Cancels the sketch and opens a fresh one, still with no project. */
    private void restartSketch() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.cancel_sketch).performClick();
            return null;
        });
        settleLayout();
        press(R.id.home_new_project);
        press(R.id.new_project_cad);
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
    }

    private void openEditSketch() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.setPrecisionOpenForTest(true);
            return null;
        });
        settleLayout();
        press(R.id.edit_cad_sketch);
    }

    private void typeLengthAndApply(final String text) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final EditText field = workspace.findViewById(R.id.field_sketch_line_length);
            field.setText(text);
            workspace.findViewById(R.id.apply_sketch_line_length).performClick();
            return null;
        });
        settleLayout();
    }

    private void press(final int id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View control = workspace.findViewById(id);
            assertNotNull("control " + id + " must exist", control);
            assertTrue("control " + id + " must be on screen", control.isShown());
            control.performClick();
            return null;
        });
        settleLayout();
    }

    private static int selectedEntityKind() {
        final double[] entity = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        assertTrue(NativeViewport.sketchSelectedEntity(entity));
        return (int) entity[NativeViewport.SKETCH_ENTITY_KIND];
    }

    /** The navigator's whole state, in one place, for every navigator capture. */
    private void viewFacts() {
        final double[] view = new double[NativeViewport.SKETCH_VIEW_SIZE];
        NativeViewport.sketchViewState(view);
        fact("view_plane", (int) view[NativeViewport.SKETCH_VIEW_PLANE]);
        fact("view_flipped", (int) view[NativeViewport.SKETCH_VIEW_FLIPPED]);
        fact("view_quarter_turns", (int) view[NativeViewport.SKETCH_VIEW_QUARTER_TURNS]);
        fact("view_face_supported", (int) view[NativeViewport.SKETCH_VIEW_FACE_SUPPORTED]);
        fact("view_plane_switchable", (int) view[NativeViewport.SKETCH_VIEW_PLANE_SWITCHABLE]);
        fact("projection_mode", NativeViewport.projectionMode());
    }

    /** Takes one composed-display frame after a short settle. */
    private void capture(String name) {
        settleLayout();
        SystemClock.sleep(400);  // a few more frames, so the viewport has drawn the state
        final Bitmap frame =
                InstrumentationRegistry.getInstrumentation().getUiAutomation().takeScreenshot();
        assertNotNull("the display could be captured", frame);
        captureIndex++;
        final File png = new File(outDir, name + ".png");
        try (FileOutputStream out = new FileOutputStream(png)) {
            assertTrue(frame.compress(Bitmap.CompressFormat.PNG, 100, out));
        } catch (IOException error) {
            throw new AssertionError("could not write " + png, error);
        }
        facts.add("capture=" + name + ".png width=" + frame.getWidth() + " height="
                + frame.getHeight());
        facts.add("");
    }

    private void fact(String key, Object value) {
        facts.add("  " + key + "=" + value);
    }

    private void bounds(EditorWorkspaceView workspace, String label, int id) {
        final View view = workspace.findViewById(id);
        if (view == null || !view.isShown()) {
            facts.add("  " + label + "=absent");
            return;
        }
        final int[] at = new int[2];
        view.getLocationOnScreen(at);
        final Rect rect = new Rect(at[0], at[1], at[0] + view.getWidth(), at[1] + view.getHeight());
        facts.add("  " + label + "=" + rect.toShortString() + " width_px=" + view.getWidth()
                + " height_px=" + view.getHeight());
    }

    private static boolean overlaps(View a, View b) {
        if (a == null || b == null || !a.isShown() || !b.isShown()) {
            return false;
        }
        final int[] pa = new int[2];
        final int[] pb = new int[2];
        a.getLocationInWindow(pa);
        b.getLocationInWindow(pb);
        return pa[0] < pb[0] + b.getWidth() && pb[0] < pa[0] + a.getWidth()
                && pa[1] < pb[1] + b.getHeight() && pb[1] < pa[1] + a.getHeight();
    }

    private void writeFacts() {
        final File file = new File(outDir, "facts.txt");
        try (PrintWriter out = new PrintWriter(file, "UTF-8")) {
            out.println("captures=" + captureIndex);
            for (String line : facts) {
                out.println(line);
            }
        } catch (IOException error) {
            throw new AssertionError("could not write " + file, error);
        }
    }

    private static File[] orEmpty(File[] files) {
        return files == null ? new File[0] : files;
    }
}
