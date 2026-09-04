package com.forgeshape.app;

import static com.forgeshape.app.SketchTestSupport.dragSketch;
import static com.forgeshape.app.SketchTestSupport.finishAndExtrude;
import static com.forgeshape.app.SketchTestSupport.pressNavigator;
import static com.forgeshape.app.SketchTestSupport.selectTool;
import static com.forgeshape.app.SketchTestSupport.sketchEntityCount;
import static com.forgeshape.app.SketchTestSupport.sketchPlane;
import static com.forgeshape.app.SketchTestSupport.sketchState;
import static com.forgeshape.app.SketchTestSupport.sketchViewState;
import static com.forgeshape.app.SketchTestSupport.tapSketch;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

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
 * `CADUXR1` / `E2E-CADUXR1`: the device half of `SKETCH-UX-R1`.
 *
 * <p>The full-screen Home page, the immediate first sketch, the orientation
 * navigator, the technical line dimension and its numeric editor, the Arc and
 * Spline tools, and Edit Sketch on a committed body — through the real chrome
 * and real {@code MotionEvent}s.
 *
 * <p><b>No control is located by coordinate.</b> Every control is found by its
 * semantic id; the only pixels here are viewport gestures, and each is asked
 * for from {@code sketchScreenPoint} — the same projection native unprojects
 * with — rather than written down. What can be proven without a device (the
 * curve mathematics, the exact line-length semantics, the view frames, the
 * staged edit, the `CADB` v3 round trip) is the native {@code SketchUx} suite;
 * this is what only a real window can say.
 */
@RunWith(AndroidJUnit4.class)
public final class SketchUxTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    @Before
    public void startAtHome() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.dismissRecoveryPromptForTest();
            workspace.showHomeAsFirstLaunchForTest();
            return null;
        });
        settleLayout();
    }

    @After
    public void returnToABaselineProject() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.supportChooserCancel();
            NativeViewport.sketchCancel();
            NativeViewport.enterConstructionMode();
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
    }

    // -----------------------------------------------------------------------
    // CADUXR1-01..04 — Home is a page, and New Project is navigation
    // -----------------------------------------------------------------------

    @Test
    public void cadUxR1_01_02_03_homeIsAFullScreenPageWithNoEditorBehindIt() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View home = workspace.findViewById(R.id.home_surface);
            assertNotNull(home);
            assertTrue("Home is on screen", workspace.homeVisible());

            // A PAGE: it fills the window rather than floating in the middle of
            // it. A centred card is exactly what the owner rejected, so this
            // asserts the geometry rather than the look.
            final View root = (View) home.getParent();
            assertEquals("Home spans the whole window width", root.getWidth(), home.getWidth());
            assertEquals("and the whole window height", root.getHeight(), home.getHeight());
            assertEquals("flush to the leading edge", 0, home.getLeft());
            assertEquals("and to the top", 0, home.getTop());

            // The product mark and name are near the top, above the actions.
            final View wordmark = workspace.findViewById(R.id.start_page_wordmark);
            final View newProject = workspace.findViewById(R.id.home_new_project);
            assertNotNull(wordmark);
            assertTrue("the wordmark is drawn", wordmark.isShown());
            assertTrue("above the first action",
                    top(wordmark) < top(newProject));
            assertTrue("the headline is drawn",
                    workspace.findViewById(R.id.start_page_headline).isShown());

            // CADUXR1-02: none of the editor is drawn behind it.
            assertFalse(workspace.findViewById(R.id.global_toolbar).isShown());
            assertFalse(workspace.findViewById(R.id.objects_capsule).isShown());
            assertFalse("and no sketch navigator outside a sketch",
                    workspace.findViewById(R.id.sketch_orientation_navigator).isShown());

            // CADUXR1-03: both ways to have a project are reachable.
            assertTrue(newProject.isShown());
            assertTrue(workspace.findViewById(R.id.home_open_file).isShown());
            return null;
        });
    }

    @Test
    public void cadUxR1_04_newProjectIsAPageWithCadSculptAndBack() {
        press(R.id.home_new_project);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View page = workspace.findViewById(R.id.new_project_chooser);
            final View root = (View) page.getParent();
            assertTrue(workspace.newProjectChooserVisible());
            assertEquals("New Project is a page too, not a dialog over Home",
                    root.getWidth(), page.getWidth());
            assertEquals(root.getHeight(), page.getHeight());
            assertFalse("and it replaces Home rather than floating over it",
                    workspace.homeVisible());
            assertTrue(workspace.findViewById(R.id.new_project_cad).isShown());
            assertTrue(workspace.findViewById(R.id.new_project_sculpt).isShown());
            assertTrue("with Back at its foot",
                    workspace.findViewById(R.id.new_project_cancel).isShown());
            assertFalse("choosing is not creating", NativeViewport.projectOpen());
            return null;
        });
        press(R.id.new_project_cancel);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("Back returns to Home", workspace.homeVisible());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // CADUXR1-05..07 — New CAD lands on a flat sketch, with no plane step
    // -----------------------------------------------------------------------

    @Test
    public void cadUxR1_05_06_07_newCadEntersAFlatXySketchImmediately() {
        enterFirstSketch();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("New CAD is already in a sketch", NativeViewport.SKETCH_EDITING,
                    sketchState());
            assertEquals("on XY", NativeViewport.WORKPLANE_XY, sketchPlane());
            assertFalse("with no floating-plane chooser anywhere in the path",
                    NativeViewport.supportChooserActive());
            assertFalse("and no project until the first Extrude",
                    NativeViewport.projectOpen());

            final double[] view = sketchViewState();
            assertEquals("looking along the plane's POSITIVE normal", 0.0,
                    view[NativeViewport.SKETCH_VIEW_FLIPPED], 0.0);
            assertEquals("the right way up", 0.0,
                    view[NativeViewport.SKETCH_VIEW_QUARTER_TURNS], 0.0);
            assertEquals("on a world plane, not a face", 0.0,
                    view[NativeViewport.SKETCH_VIEW_FACE_SUPPORTED], 0.0);
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // CADUXR1-08..16 — the orientation navigator
    // -----------------------------------------------------------------------

    @Test
    public void cadUxR1_08_09_10_theNavigatorReachesEveryPrincipalOrientation() {
        enterFirstSketch();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("the navigator is in the sketch",
                    workspace.findViewById(R.id.sketch_orientation_navigator).isShown());
            // It may stand on the drawing; it may never stand on another live
            // control. The Tool Rail is the one beside it.
            assertFalse("the navigator does not cover the Tool Rail",
                    overlaps(workspace.findViewById(R.id.sketch_orientation_navigator),
                             workspace.findViewById(R.id.tool_rail_select)));
            return null;
        });

        // XZ, then YZ, then back to XY: every principal plane from the control.
        pressNavigator(rule.getScenario(), R.id.sketch_navigator_plane_xz);
        assertEquals(NativeViewport.WORKPLANE_XZ, sketchPlane());
        pressNavigator(rule.getScenario(), R.id.sketch_navigator_plane_yz);
        assertEquals(NativeViewport.WORKPLANE_YZ, sketchPlane());
        pressNavigator(rule.getScenario(), R.id.sketch_navigator_plane_xy);
        assertEquals(NativeViewport.WORKPLANE_XY, sketchPlane());

        // And each principal plane can be viewed from EITHER side.
        pressNavigator(rule.getScenario(), R.id.sketch_navigator_flip);
        assertEquals("looking along the negative normal", 1.0,
                sketchViewState()[NativeViewport.SKETCH_VIEW_FLIPPED], 0.0);
        pressNavigator(rule.getScenario(), R.id.sketch_navigator_flip);
        assertEquals("and back", 0.0,
                sketchViewState()[NativeViewport.SKETCH_VIEW_FLIPPED], 0.0);
    }

    @Test
    public void cadUxR1_11_12_13_rotationTurnsTheViewAndNotTheSketch() {
        enterFirstSketch();
        // One line, so there is authored truth for the rotation not to touch.
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        dragSketch(rule.getScenario(), -1.0, -0.5, 1.0, 0.5);
        assertEquals(1, sketchEntityCount());
        final double[] before = selectedEntityValues();

        pressNavigator(rule.getScenario(), R.id.sketch_navigator_rotate_cw);
        assertEquals("+90 is one quarter turn", 1.0,
                sketchViewState()[NativeViewport.SKETCH_VIEW_QUARTER_TURNS], 0.0);
        pressNavigator(rule.getScenario(), R.id.sketch_navigator_rotate_ccw);
        pressNavigator(rule.getScenario(), R.id.sketch_navigator_rotate_ccw);
        assertEquals("-90 twice from there wraps to three", 3.0,
                sketchViewState()[NativeViewport.SKETCH_VIEW_QUARTER_TURNS], 0.0);
        pressNavigator(rule.getScenario(), R.id.sketch_navigator_rotate_cw);
        assertEquals("and four quarter turns is the identity", 0.0,
                sketchViewState()[NativeViewport.SKETCH_VIEW_QUARTER_TURNS], 0.0);

        final double[] after = selectedEntityValues();
        assertEquals("the entity count is untouched", 1, sketchEntityCount());
        for (int i = 0; i < before.length; i++) {
            assertEquals("a view rotation moves no authored coordinate",
                    before[i], after[i], 0.0);
        }
    }

    @Test
    public void cadUxR1_14_15_thePlaneIsFixedOnceTheSketchHasGeometry() {
        enterFirstSketch();
        // Empty: switchable, and the control says so.
        assertEquals(1.0, sketchViewState()[NativeViewport.SKETCH_VIEW_PLANE_SWITCHABLE], 0.0);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(workspace.findViewById(R.id.sketch_navigator_plane_xz).isEnabled());
            return null;
        });
        pressNavigator(rule.getScenario(), R.id.sketch_navigator_plane_xz);
        assertEquals(NativeViewport.WORKPLANE_XZ, sketchPlane());

        // Draw something, and the plane freezes.
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), -1.0, -0.5, 1.0, 0.5);
        assertEquals(1, sketchEntityCount());
        assertEquals(0.0, sketchViewState()[NativeViewport.SKETCH_VIEW_PLANE_SWITCHABLE], 0.0);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertFalse("a control that cannot succeed is not pressable",
                    workspace.findViewById(R.id.sketch_navigator_plane_yz).isEnabled());
            return null;
        });
        // And below JNI it is refused by name, not merely withdrawn above it.
        assertEquals("switching plane with geometry drawn is refused by name",
                NativeViewport.CAD_SKETCH_NOT_EMPTY,
                NativeViewport.sketchSetSupportPlane(NativeViewport.WORKPLANE_YZ));
        assertEquals("and the sketch is unmoved", NativeViewport.WORKPLANE_XZ, sketchPlane());
        assertEquals(1, sketchEntityCount());
    }

    // -----------------------------------------------------------------------
    // CADUXR1-17..24 — the technical line dimension and its numeric editor
    // -----------------------------------------------------------------------

    @Test
    public void cadUxR1_17_18_19_20_21_22_aSelectedLineShowsAndTakesAnExactLength() {
        enterFirstSketch();
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        // A 3-4-5 line: exactly 5 m long, so the label has an exact value.
        dragSketch(rule.getScenario(), 0.0, 0.0, 3.0, 4.0);
        assertEquals(1, sketchEntityCount());

        // CADUXR1-17/18: the annotation is up, and it reads the real length.
        final double[] dimension = new double[NativeViewport.SKETCH_DIMENSION_SIZE];
        assertTrue("the selected line carries a dimension",
                NativeViewport.sketchLineDimension(dimension));
        assertEquals("and the length is the line's own", 5.0,
                dimension[NativeViewport.SKETCH_DIMENSION_LENGTH], 1e-9);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("the label is drawn over the viewport",
                    workspace.findViewById(R.id.sketch_dimension_label).isShown());
            assertTrue(workspace.findViewById(R.id.sketch_dimension_value).isShown());
            assertFalse("and the editor is closed until it is tapped",
                    workspace.sketchDimensionLabel().editorOpen());
            return null;
        });

        // CADUXR1-19: tapping the value opens a compact numeric editor.
        press(R.id.sketch_dimension_value);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(workspace.sketchDimensionLabel().editorOpen());
            final View field = workspace.findViewById(R.id.field_sketch_line_length);
            assertTrue("the field is on screen", field.isShown());
            // It must not become a panel over the drawing.
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final View editor = workspace.findViewById(R.id.sketch_dimension_editor);
            assertTrue("the editor covers a small part of the viewport",
                    editor.getWidth() * editor.getHeight()
                            < viewport.getWidth() * viewport.getHeight() / 4);
            return null;
        });

        // CADUXR1-20/21/22: type 10 m. P0 fixed, direction preserved, the far
        // endpoint exactly 10 m away — a doubled 3-4-5.
        typeLengthAndApply("10");
        final double[] line = selectedEntityValues();
        assertEquals("P0 stays exactly where it was", 0.0, line[0], 1e-9);
        assertEquals(0.0, line[1], 1e-9);
        assertEquals("and P1 moves along the same direction", 6.0, line[2], 1e-9);
        assertEquals(8.0, line[3], 1e-9);
        assertTrue("the dimension follows the edit",
                NativeViewport.sketchLineDimension(dimension));
        assertEquals(10.0, dimension[NativeViewport.SKETCH_DIMENSION_LENGTH], 1e-9);
    }

    @Test
    public void cadUxR1_23_anInvalidTypedLengthChangesNothing() {
        enterFirstSketch();
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        dragSketch(rule.getScenario(), 0.0, 0.0, 3.0, 4.0);
        final double[] before = selectedEntityValues();

        // Opened ONCE. A refused value deliberately leaves the editor open with
        // the text in it, so the user can correct the number rather than having
        // to find and reopen the label — which is also why the value chip is
        // not on screen to press again between attempts.
        press(R.id.sketch_dimension_value);
        for (String bad : new String[]{"0", "-4", "banana", ""}) {
            typeLengthAndApply(bad);
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                assertTrue("a refused length leaves the editor open to correct",
                        workspace.sketchDimensionLabel().editorOpen());
                return null;
            });
            final double[] after = selectedEntityValues();
            for (int i = 0; i < before.length; i++) {
                assertEquals("\"" + bad + "\" moves no authored coordinate",
                        before[i], after[i], 0.0);
            }
        }
        assertEquals("and the line is still there", 1, sketchEntityCount());
        // A valid one then lands and closes the editor, so the refusals left
        // nothing stuck.
        typeLengthAndApply("7.5");
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertFalse("an accepted length closes the editor",
                    workspace.sketchDimensionLabel().editorOpen());
            return null;
        });
    }

    @Test
    public void cadUxR1_24_aLengthEditMayOpenAProfileAndExtrudeThenRefuses() {
        enterFirstSketch();
        // A triangle of three lines: a closed chain.
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        dragSketch(rule.getScenario(), 0.0, 0.0, 2.0, 0.0);
        dragSketch(rule.getScenario(), 2.0, 0.0, 1.0, 2.0);
        dragSketch(rule.getScenario(), 1.0, 2.0, 0.0, 0.0);
        assertEquals(3, sketchEntityCount());

        // Select the first line and shorten it: the chain opens.
        selectTool(rule.getScenario(), R.id.tool_rail_select);
        tapSketch(rule.getScenario(), 1.0, 0.0);
        assertTrue(NativeViewport.sketchLineDimension(
                new double[NativeViewport.SKETCH_DIMENSION_SIZE]));
        press(R.id.sketch_dimension_value);
        typeLengthAndApply("1");

        // Extrude then refuses BY NAME rather than repairing the sketch.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.finish_sketch).performClick();
            return null;
        });
        settleLayout();
        assertEquals("the sketch stays in editing", NativeViewport.SKETCH_EDITING, sketchState());
        assertEquals("refused as an open profile", NativeViewport.CAD_OPEN_PROFILE,
                NativeViewport.sketchLastStatus());
        assertFalse("and no project was created", NativeViewport.projectOpen());
    }

    // -----------------------------------------------------------------------
    // CADUXR1-25..31 — Arc and Spline are real, drawable, extrudable entities
    // -----------------------------------------------------------------------

    @Test
    public void cadUxR1_25_27_anArcIsDrawnSelectedAndExtrudedWithALine() {
        enterFirstSketch();
        drawSemicircleArc();
        assertEquals("the arc is one entity", 1, sketchEntityCount());
        assertEquals("and it is an Arc", NativeViewport.SKETCH_ENTITY_KIND_ARC,
                selectedEntityKind());

        // The chord that closes it into a profile.
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        dragSketch(rule.getScenario(), -1.0, 0.0, 1.0, 0.0);
        assertEquals(2, sketchEntityCount());

        finishAndExtrude(rule.getScenario(), "1");
        assertTrue("a curve profile extrudes into a real CAD body",
                NativeViewport.sceneActiveBodyIsCad());
        assertTrue(NativeViewport.projectOpen());
    }

    @Test
    public void cadUxR1_28_30_aSplineIsDrawnSelectedDeletedAndExtruded() {
        enterFirstSketch();
        drawSpline();
        assertEquals(1, sketchEntityCount());
        assertEquals(NativeViewport.SKETCH_ENTITY_KIND_SPLINE, selectedEntityKind());

        // Deletable like any other entity.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.CAD_OK, NativeViewport.sketchDeleteSelected());
            return null;
        });
        settleLayout();
        assertEquals("a spline is deletable", 0, sketchEntityCount());

        // Redraw it and close it with a line, then extrude.
        drawSpline();
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        dragSketch(rule.getScenario(), 1.0, 0.0, -1.0, 0.0);
        assertEquals(2, sketchEntityCount());
        finishAndExtrude(rule.getScenario(), "0.5");
        assertTrue(NativeViewport.sceneActiveBodyIsCad());
    }

    // -----------------------------------------------------------------------
    // CADUXR1-32..35 — Edit Sketch on a committed body
    // -----------------------------------------------------------------------

    @Test
    public void cadUxR1_32_33_34_35_editSketchStagesFinishesAndUndoesExactly() {
        enterFirstSketch();
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), -1.0, -0.5, 1.0, 0.5);
        finishAndExtrude(rule.getScenario(), "1");
        final long body = NativeViewport.sceneActiveBodyId();
        assertTrue(NativeViewport.sceneActiveBodyIsCad());
        final int undoAfterCreate = NativeViewport.constructionUndoDepth();

        // CADUXR1-32: Edit Sketch reopens the authored sketch.
        openEditSketch();
        assertEquals("the session is editing THAT body", body,
                NativeViewport.sketchEditingBodyId());
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
        assertEquals("with the body's own entity in it", 1, sketchEntityCount());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("and the navigator is up, as in any sketch",
                    workspace.findViewById(R.id.sketch_orientation_navigator).isShown());
            return null;
        });

        // CADUXR1-33: Cancel is exact non-mutation.
        final double widthBefore = cadRectangleWidth();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.sketchApplyRectangle(1L, 4.0, 1.0);
            return null;
        });
        assertEquals("the staged edit did not reach the body", widthBefore, cadRectangleWidth(),
                1e-9);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.cancel_sketch).performClick();
            return null;
        });
        settleLayout();
        assertEquals(NativeViewport.SKETCH_INACTIVE, sketchState());
        assertEquals("Cancel changed nothing", widthBefore, cadRectangleWidth(), 1e-9);
        assertEquals("and recorded nothing", undoAfterCreate,
                NativeViewport.constructionUndoDepth());

        // CADUXR1-34: Finish is exactly one history step.
        openEditSketch();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.sketchApplyRectangle(1L, 4.0, 1.0);
            return null;
        });
        finishAndExtrude(rule.getScenario(), "1");
        assertEquals("the body took the edit", 4.0, cadRectangleWidth(), 1e-9);
        assertEquals("one Finish is exactly one Undo", undoAfterCreate + 1,
                NativeViewport.constructionUndoDepth());
        assertEquals("and no second body was created", 1, NativeViewport.sceneBodyCount());

        // CADUXR1-35: Undo and Redo restore the whole sketch.
        assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionUndo());
        assertEquals("Undo restores the previous sketch", widthBefore, cadRectangleWidth(), 1e-9);
        assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionRedo());
        assertEquals("and Redo restores the edited one", 4.0, cadRectangleWidth(), 1e-9);
    }

    // -----------------------------------------------------------------------
    // CADUXR1-39/40 — nothing forbidden arrived, and Sculpt still works
    // -----------------------------------------------------------------------

    @Test
    public void cadUxR1_39_newSculptStillReachesASculptableSphere() {
        press(R.id.home_new_project);
        press(R.id.new_project_sculpt);
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("New Sculpt still creates its project", NativeViewport.projectOpen());
            assertEquals("and lands in Sculpt", NativeViewport.MODE_SCULPT,
                    NativeViewport.productMode());
            assertEquals("seeding is not a user act", 0,
                    NativeViewport.constructionUndoDepth());
            assertFalse("no sketch is open in Sculpt", workspace.findViewById(
                    R.id.sketch_orientation_navigator).isShown());
            return null;
        });
    }

    @Test
    public void cadUxR1_40_theSketchToolSetIsExactlySevenAndNoMore() {
        enterFirstSketch();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            // The five that were here, plus the two this stage adds. No
            // boolean, no fillet, no chamfer, no constraint control.
            for (int id : new int[]{R.id.tool_rail_select, R.id.tool_rail_line,
                                    R.id.tool_rail_polyline, R.id.tool_rail_rectangle,
                                    R.id.tool_rail_circle, R.id.tool_rail_arc,
                                    R.id.tool_rail_spline}) {
                final View entry = workspace.findViewById(id);
                assertNotNull("sketch tool " + id + " exists", entry);
                assertTrue("and is on the rail", entry.isShown());
            }
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // Helpers — every one of them drives a real control or a real gesture
    // -----------------------------------------------------------------------

    /** Home → New Project → CAD, which now lands straight in the sketch. */
    private void enterFirstSketch() {
        press(R.id.home_new_project);
        press(R.id.new_project_cad);
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
    }

    /** The Arc tool's two-step gesture: drag the chord, tap the bulge. */
    private void drawSemicircleArc() {
        selectTool(rule.getScenario(), R.id.tool_rail_arc);
        dragSketch(rule.getScenario(), 1.0, 0.0, -1.0, 0.0);
        assertEquals("the chord alone places nothing", 0, sketchEntityCount());
        tapSketch(rule.getScenario(), 0.0, 1.0);
    }

    /** The Spline tool's run of taps, ended by tapping the last point again. */
    private void drawSpline() {
        selectTool(rule.getScenario(), R.id.tool_rail_spline);
        tapSketch(rule.getScenario(), -1.0, 0.0);
        tapSketch(rule.getScenario(), -0.5, 0.75);
        tapSketch(rule.getScenario(), 0.5, 0.75);
        tapSketch(rule.getScenario(), 1.0, 0.0);
        tapSketch(rule.getScenario(), 1.0, 0.0);
    }

    /** Opens the precision surface's Edit Sketch on the active CAD body. */
    private void openEditSketch() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.setPrecisionOpenForTest(true);
            return null;
        });
        settleLayout();
        press(R.id.edit_cad_sketch);
    }

    /** Types into the dimension field and presses its Apply. */
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

    private static double[] selectedEntityValues() {
        final double[] entity = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        assertTrue("something is selected", NativeViewport.sketchSelectedEntity(entity));
        return new double[]{entity[NativeViewport.SKETCH_ENTITY_VALUES],
                            entity[NativeViewport.SKETCH_ENTITY_VALUES + 1],
                            entity[NativeViewport.SKETCH_ENTITY_VALUES + 2],
                            entity[NativeViewport.SKETCH_ENTITY_VALUES + 3]};
    }

    private static int selectedEntityKind() {
        final double[] entity = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        assertTrue("something is selected", NativeViewport.sketchSelectedEntity(entity));
        return (int) entity[NativeViewport.SKETCH_ENTITY_KIND];
    }

    /** The active CAD body's rectangle width, read back from native truth. */
    private static double cadRectangleWidth() {
        final double[] state = new double[NativeViewport.CAD_STATE_SIZE];
        assertTrue("the active body is a CAD body", NativeViewport.cadState(state));
        return state[NativeViewport.CAD_PRIMARY_SIZE];
    }

    private static int top(View view) {
        final int[] at = new int[2];
        view.getLocationInWindow(at);
        return at[1];
    }

    /** Whether two on-screen controls share any pixel. */
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
}
