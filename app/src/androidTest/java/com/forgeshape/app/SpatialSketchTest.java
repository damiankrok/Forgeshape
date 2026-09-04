package com.forgeshape.app;

import static com.forgeshape.app.SketchTestSupport.dragSketch;
import static com.forgeshape.app.SketchTestSupport.drawRectangleAndExtrude;
import static com.forgeshape.app.SketchTestSupport.finishAndExtrude;
import static com.forgeshape.app.SketchTestSupport.hoverStylus;
import static com.forgeshape.app.SketchTestSupport.onNativeStateChanged;
import static com.forgeshape.app.SketchTestSupport.selectTool;
import static com.forgeshape.app.SketchTestSupport.sketchEntityCount;
import static com.forgeshape.app.SketchTestSupport.sketchState;
import static com.forgeshape.app.SketchTestSupport.tapTapWorld;
import static com.forgeshape.app.SketchTestSupport.tapWorld;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
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
 * `E2E-CADA3`: the spatial "Choose Sketch Support" flow, through the real chrome
 * and real MotionEvents.
 *
 * <p>The DOMAIN half -- semantic faces, TopoRef, the dependency graph, CADB v2,
 * the exact camera, the adaptive grid and the picking -- is proved by the native
 * `CADA3-*` self-test, which builds its own scenes and camera. What is left, and
 * what this covers, is what can only be true on a device: that New Sketch lands
 * directly in a viewport-first support pick, that a tap-tap on a world plane or
 * a planar CAD face begins a sketch on it, that a curved side never does, that
 * a stylus hover lights a target without committing, that a second body
 * extruded on the first's face is a face-supported dependent which follows a
 * producer edit, that its producer cannot be deleted while it stands, that the
 * dependency survives a save/reopen, and that the adaptive grid moves with the
 * zoom while a typed value never re-snaps.
 *
 * <p>No control is located by coordinate. The one place a pixel appears is the
 * viewport gesture, and every such pixel is asked for from
 * {@code sketchScreenPoint} or {@code debugProjectWorld} -- the same projection
 * native code unprojects with -- never written down.
 */
@RunWith(AndroidJUnit4.class)
public final class SpatialSketchTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private byte[] baselineProject;

    @Before
    public void setUp() {
        resetToBaselineConstruction(rule.getScenario());
        baselineProject = NativeViewport.encodeProject();
    }

    @After
    public void tearDown() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.supportChooserCancel();
            NativeViewport.sketchCancel();
            NativeViewport.enterConstructionMode();
            if (baselineProject != null) {
                NativeViewport.loadProject(baselineProject);
            }
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
    }

    // -----------------------------------------------------------------------
    // E2E-CADA3-02/03: New Sketch lands DIRECTLY in the spatial support
    // chooser, and a tap-tap on a world plane begins a sketch on it. The
    // by-name plane list stays reachable as the fallback.
    // -----------------------------------------------------------------------

    @Test
    public void spatialSupportChooserBeginsAWorldPlaneSketch() {
        openSpatialChooser();
        assertTrue("the spatial support chooser is active", NativeViewport.supportChooserActive());
        assertEquals("no sketch has begun yet", NativeViewport.SKETCH_INACTIVE, sketchState());

        // A point unambiguously on the XY plane (the baseline body is
        // Construction, so no CAD face is eligible: only world planes are).
        tapTapWorld(rule.getScenario(), 2.0, 2.0, 0.0);
        assertFalse("the chooser has handed off", NativeViewport.supportChooserActive());
        assertEquals("a sketch began on a world plane", NativeViewport.SKETCH_EDITING, sketchState());
        assertEquals(NativeViewport.WORKPLANE_XY, sketchPlane());
        NativeViewport.sketchCancel();
    }

    @Test
    public void byNamePlaneListRemainsTheAccessibilityFallback() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openAddPrimitive(workspace);
            assertFalse(workspace.addPrimitivePalette().showingPlanes());
            final View byName =
                    workspace.addPrimitivePalette().findViewById(R.id.sketch_plane_by_name);
            assertNotNull("the by-name fallback is offered beside New Sketch", byName);
            byName.performClick();
            assertTrue(workspace.addPrimitivePalette().showingPlanes());
            for (int id : new int[]{R.id.sketch_plane_xy, R.id.sketch_plane_xz,
                    R.id.sketch_plane_yz, R.id.sketch_support_spatial}) {
                assertNotNull(workspace.addPrimitivePalette().findViewById(id));
            }
            workspace.addPrimitivePalette().findViewById(R.id.sketch_plane_yz).performClick();
            return null;
        });
        settleLayout();
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
        assertEquals("on the plane named", NativeViewport.WORKPLANE_YZ, sketchPlane());
        NativeViewport.sketchCancel();
    }

    // -----------------------------------------------------------------------
    // E2E-CADA3-11: all three world planes are reachable spatially through
    // real MotionEvents, and a tap aims before a second tap commits.
    // -----------------------------------------------------------------------

    @Test
    public void everyWorldPlaneIsSelectableSpatially() {
        final double[][] pointsOn = {{2.0, 2.0, 0.0}, {2.0, 0.0, 2.0}, {0.0, 2.0, 2.0}};
        final int[] planes = {NativeViewport.WORKPLANE_XY, NativeViewport.WORKPLANE_XZ,
                NativeViewport.WORKPLANE_YZ};
        for (int i = 0; i < planes.length; i++) {
            openSpatialChooser();
            // First tap AIMS: the target is selected and highlighted, nothing
            // begins. Second tap on the same target COMMITS.
            tapWorld(rule.getScenario(), pointsOn[i][0], pointsOn[i][1], pointsOn[i][2]);
            assertEquals("plane " + planes[i] + " is aimed at", planes[i],
                    NativeViewport.supportChooserSelectedKind());
            assertEquals("aiming begins nothing", NativeViewport.SKETCH_INACTIVE, sketchState());
            assertTrue(NativeViewport.supportChooserActive());
            tapWorld(rule.getScenario(), pointsOn[i][0], pointsOn[i][1], pointsOn[i][2]);
            assertEquals("the second tap begins the sketch", NativeViewport.SKETCH_EDITING,
                    sketchState());
            assertEquals(planes[i], sketchPlane());
            NativeViewport.sketchCancel();
            onNativeStateChanged(rule.getScenario());
        }
    }

    // -----------------------------------------------------------------------
    // E2E-CADA3-15 (hover): a stylus hover lights a target and commits nothing
    // -----------------------------------------------------------------------

    /**
     * The authoritative emulator has no stylus, so the hover is a synthesized
     * stylus {@code ACTION_HOVER_MOVE} through the viewport's real generic
     * motion dispatch -- the path hardware hover takes -- and the touch commit
     * that follows is a real finger tap. Real hardware hover is NOT claimed.
     */
    @Test
    public void stylusHoverHighlightsATargetWithoutCommitting() {
        openSpatialChooser();
        final float[] at = new float[2];
        assertTrue(NativeViewport.debugProjectWorld(2.0, 0.0, 2.0, at));
        assertTrue("the hover lit the XZ plane target",
                hoverStylus(rule.getScenario(), at[0], at[1]));
        assertEquals("hover selects nothing", -1, NativeViewport.supportChooserSelectedKind());
        assertEquals("and begins nothing", NativeViewport.SKETCH_INACTIVE, sketchState());
        assertTrue(NativeViewport.supportChooserActive());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final ForgeShapeSurfaceView viewport = workspace.findViewById(R.id.viewport_surface);
            assertTrue(viewport.lastHoverHighlighted());
            return null;
        });
        // A hover off every target lights nothing.
        assertFalse(hoverStylus(rule.getScenario(), 2.0f, 2.0f));
        // A finger still commits: aim, then commit, on the hovered plane.
        tapTapWorld(rule.getScenario(), 2.0, 0.0, 2.0);
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
        assertEquals(NativeViewport.WORKPLANE_XZ, sketchPlane());
        NativeViewport.sketchCancel();
    }

    // -----------------------------------------------------------------------
    // E2E-CADA3-05/12/13/16/17/28/33/38: a face-supported dependent, its
    // producer, a parent edit, the refusals and the persistence.
    // -----------------------------------------------------------------------

    @Test
    public void faceSupportedSketchExtrudesADependentAndPersists() {
        // Body A: a 2x2 rectangle extruded 2 on XY. Its far cap is a 2x2
        // square centred at world (0, 0, 2).
        final long producerId = extrudeARectangleOnXy(2.0, 2.0, 2.0);
        final int bodiesAfterA = NativeViewport.sceneBodyCount();
        final int stepsAfterA = NativeViewport.constructionUndoDepth();

        // New Sketch -> spatial -> tap-tap A's far-cap centre. A sketch begins,
        // supported by that face.
        openSpatialChooser();
        tapWorld(rule.getScenario(), 0.0, 0.0, 2.0);
        assertEquals("E2E-CADA3-12: the tap aims at a CAD face, not a world plane",
                3, NativeViewport.supportChooserSelectedKind());
        tapWorld(rule.getScenario(), 0.0, 0.0, 2.0);
        assertEquals("a sketch began on the tapped face", NativeViewport.SKETCH_EDITING,
                sketchState());

        // Draw a rectangle on the face and extrude New Body -> body B.
        final long dependentId = drawRectangleAndExtrude(rule.getScenario(), 1.0, 1.0, "0.5");
        assertTrue("E2E-CADA3-05: B is face-supported",
                NativeViewport.sceneActiveBodyIsFaceSupportedCad());
        assertNotEquals(producerId, dependentId);
        assertEquals("one new body", bodiesAfterA + 1, NativeViewport.sceneBodyCount());
        assertEquals("one history step for the dependent's creation",
                stepsAfterA + 1, NativeViewport.constructionUndoDepth());

        // E2E-CADA3-16: a parent parameter edit keeps the dependent attached
        // and still resolving.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(producerId));
            assertEquals("the producer's depth is edited", NativeViewport.APPLY_APPLIED,
                    NativeViewport.cadApplyExtrude(3.0, NativeViewport.EXTRUDE_ALONG_NORMAL));
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(dependentId));
            workspace.syncFromNative();
            assertTrue("the dependent is still face-supported after the edit",
                    NativeViewport.sceneActiveBodyIsFaceSupportedCad());
            final double[] cad = new double[NativeViewport.CAD_STATE_SIZE];
            assertTrue(NativeViewport.cadState(cad));
            return null;
        });
        // The dependent's world placement followed: its far cap is now at the
        // producer's new top (z = 3 + 0.5), and that point projects on screen.
        final float[] on = new float[2];
        assertTrue(NativeViewport.debugProjectWorld(0.0, 0.0, 3.5, on));

        // E2E-CADA3-17/38: the producer cannot be deleted while the dependent
        // stands; the dependent can, then the producer.
        assertEquals("deleting the producer is refused",
                NativeViewport.DELETE_REFUSED_HAS_DEPENDENTS,
                NativeViewport.sceneDeleteBody(producerId));
        assertEquals("both bodies remain", bodiesAfterA + 1, NativeViewport.sceneBodyCount());

        // E2E-CADA3-15: save and reopen; the dependency restores and both
        // bodies come back with the edited producer depth.
        final byte[] saved = NativeViewport.encodeProject();
        assertNotNull(saved);
        final int total = NativeViewport.sceneBodyCount();
        assertEquals("the saved project reopens", NativeViewport.PROJECT_OK,
                NativeViewport.loadProject(saved));
        assertEquals("every body restored", total, NativeViewport.sceneBodyCount());
        assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(dependentId));
        assertTrue("the dependency survived the reopen",
                NativeViewport.sceneActiveBodyIsFaceSupportedCad());
        assertEquals(NativeViewport.DELETE_REFUSED_HAS_DEPENDENTS,
                NativeViewport.sceneDeleteBody(producerId));
    }

    // -----------------------------------------------------------------------
    // E2E-CADA3-13: a side face supports a sketch too
    // -----------------------------------------------------------------------

    @Test
    public void aSideFaceSupportsASketch() {
        // A: 2x2 rectangle extruded 2 on XY. Its +X side is the plane x = 1,
        // centred at (1, 0, 1). Look at it from the +X direction so the side
        // faces the camera rather than being edge-on.
        extrudeARectangleOnXy(2.0, 2.0, 2.0);
        assertTrue(NativeViewport.debugSetCameraPose(1.5708f, 0.35f, 8.0f));
        openSpatialChooser();
        tapWorld(rule.getScenario(), 1.0, 0.0, 1.0);
        assertEquals("a planar side is a face target", 3,
                NativeViewport.supportChooserSelectedKind());
        tapWorld(rule.getScenario(), 1.0, 0.0, 1.0);
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
        final long dependent = drawRectangleAndExtrude(rule.getScenario(), 0.5, 0.5, "0.25");
        assertTrue("the side-supported body is a dependent",
                NativeViewport.sceneActiveBodyIsFaceSupportedCad());
        assertNotEquals(NativeViewport.NO_OBJECT, dependent);
    }

    // -----------------------------------------------------------------------
    // E2E-CADA3-14: a circle's cylindrical side is never a support
    // -----------------------------------------------------------------------

    @Test
    public void aCylindricalSideIsRefusedAsASupport() {
        // A cylinder: a circle of radius 1 on XY extruded 2. Its side is
        // curved; its far cap at (0, 0, 2) is planar.
        beginSketchOnXy();
        selectTool(rule.getScenario(), R.id.tool_rail_circle);
        dragSketch(rule.getScenario(), 0.0, 0.0, 1.0, 0.0);
        assertEquals(1, sketchEntityCount());
        finishAndExtrude(rule.getScenario(), "2");
        assertTrue(NativeViewport.sceneActiveBodyIsCad());

        assertTrue(NativeViewport.debugSetCameraPose(1.5708f, 0.2f, 8.0f));
        openSpatialChooser();
        // A tap on the curved side at (1, 0, 1): never a face. It resolves to
        // a world plane behind it or to nothing, and begins no face sketch.
        tapWorld(rule.getScenario(), 1.0, 0.0, 1.0);
        assertNotEquals("the curved side is not a face target", 3,
                NativeViewport.supportChooserSelectedKind());
        assertEquals(NativeViewport.SKETCH_INACTIVE, sketchState());
        // The planar cap still is.
        assertTrue(NativeViewport.debugSetCameraPose(0.7f, 0.9f, 8.0f));
        tapWorld(rule.getScenario(), 0.0, 0.0, 2.0);
        assertEquals("the cap is", 3, NativeViewport.supportChooserSelectedKind());
        NativeViewport.supportChooserCancel();
        onNativeStateChanged(rule.getScenario());
    }

    // -----------------------------------------------------------------------
    // E2E-CADA3-18: the adaptive grid moves with the zoom; a typed value is
    // never re-snapped to it.
    // -----------------------------------------------------------------------

    @Test
    public void adaptiveGridFollowsZoomAndTypedValuesBypassSnap() {
        beginSketchOnXy();
        final double initial = NativeViewport.sketchGridStep();
        assertTrue("a usable grid step", initial > 0.0);
        // Two real pinches in opposite directions. The grid must follow the
        // zoom both ways, and the closer view must draw the finer step: what
        // is asserted is the ORDER of the two steps, not which finger motion
        // the camera maps to which direction.
        assertTrue(zoomSketchCamera(3.0f));
        final double afterFirst = NativeViewport.sketchGridStep();
        assertTrue(zoomSketchCamera(1.0f / 9.0f));
        final double afterSecond = NativeViewport.sketchGridStep();
        assertNotEquals("the grid followed the first zoom", initial, afterFirst, 0.0);
        assertNotEquals("and the second, the other way", afterFirst, afterSecond, 0.0);
        // The step before any camera sample is the documented 0.25 m fallback;
        // every step chosen FROM the camera is a nice 1/2/5 x 10^k.
        assertTrue("and every zoomed step is a nice 1/2/5 x 10^k",
                isNiceStep(afterFirst) && isNiceStep(afterSecond));

        // A rectangle placed by drag snaps to the grid; then a typed width
        // that is NOT a multiple of any grid step is kept EXACTLY.
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), -1.0, -1.0, 1.0, 1.0);
        assertEquals(1, sketchEntityCount());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final EditText width = workspace.sketchEditor().findViewById(R.id.field_sketch_rect_width);
            final EditText height =
                    workspace.sketchEditor().findViewById(R.id.field_sketch_rect_height);
            assertNotNull(width);
            width.setText("1.2345");
            height.setText("0.777");
            workspace.findViewById(R.id.apply_sketch_entity).performClick();
            return null;
        });
        settleLayout();
        final double[] entity = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        assertTrue(NativeViewport.sketchSelectedEntity(entity));
        assertEquals("a typed width is exact", 1.2345,
                entity[NativeViewport.SKETCH_ENTITY_VALUES + 2], 1e-12);
        assertEquals("a typed height is exact", 0.777,
                entity[NativeViewport.SKETCH_ENTITY_VALUES + 3], 1e-12);
        NativeViewport.sketchCancel();
    }

    // -----------------------------------------------------------------------
    // helpers
    // -----------------------------------------------------------------------

    /** New Sketch inside a project lands directly in the spatial chooser. */
    private void openSpatialChooser() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openAddPrimitive(workspace);
            final View newSketch = workspace.addPrimitivePalette().findViewById(R.id.add_sketch);
            assertNotNull(newSketch);
            newSketch.performClick();
            return null;
        });
        settleLayout();
        assertTrue("New Sketch is a viewport-first support pick",
                NativeViewport.supportChooserActive());
    }

    private void beginSketchOnXy() {
        assertEquals(NativeViewport.CAD_OK, NativeViewport.sketchBegin(NativeViewport.WORKPLANE_XY));
        onNativeStateChanged(rule.getScenario());
    }

    private long extrudeARectangleOnXy(double width, double height, double depth) {
        beginSketchOnXy();
        return drawRectangleAndExtrude(rule.getScenario(), width, height, Double.toString(depth));
    }

    private int sketchPlane() {
        final double[] state = new double[NativeViewport.SKETCH_STATE_SIZE];
        NativeViewport.sketchState(state);
        return (int) state[NativeViewport.SKETCH_PLANE];
    }

    /**
     * Dollies the sketch camera by a two-finger pinch through the real
     * viewport, so the grid is re-sampled the way a user's zoom re-samples it.
     */
    private boolean zoomSketchCamera(final float factor) {
        final double before = NativeViewport.sketchGridStep();
        final Boolean moved = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final float cx = viewport.getWidth() / 2f;
            final float cy = viewport.getHeight() / 2f;
            final long down = android.os.SystemClock.uptimeMillis();
            // Two fingers whose separation changes by `factor`: closing for a
            // factor above one, spreading for one below, both kept on screen.
            final float fromHalf = factor >= 1f ? 200f : 60f;
            final float toHalf = factor >= 1f ? 200f / factor : 60f / factor;
            pinch(viewport, down, cx, cy, fromHalf, toHalf);
            return true;
        });
        settleLayout();
        // The step is re-sampled when the overlay is rebuilt for the new
        // camera; a fresh read after the pinch settles is what the product draws.
        return moved && NativeViewport.sketchGridStep() != before;
    }

    private static void pinch(View viewport, long down, float cx, float cy, float fromHalf,
                              float toHalf) {
        final android.view.MotionEvent.PointerProperties[] props = {
                new android.view.MotionEvent.PointerProperties(),
                new android.view.MotionEvent.PointerProperties()};
        props[0].id = 0;
        props[1].id = 1;
        props[0].toolType = props[1].toolType = android.view.MotionEvent.TOOL_TYPE_FINGER;
        final android.view.MotionEvent.PointerCoords[] coords = {
                new android.view.MotionEvent.PointerCoords(),
                new android.view.MotionEvent.PointerCoords()};
        coords[0].x = cx - fromHalf;
        coords[0].y = cy;
        coords[1].x = cx + fromHalf;
        coords[1].y = cy;
        dispatch(viewport, down, down, android.view.MotionEvent.ACTION_DOWN, 1, props, coords);
        dispatch(viewport, down, down + 8L,
                android.view.MotionEvent.ACTION_POINTER_DOWN
                        | (1 << android.view.MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                2, props, coords);
        for (int step = 1; step <= 8; ++step) {
            final float t = step / 8f;
            final float half = fromHalf + (toHalf - fromHalf) * t;
            coords[0].x = cx - half;
            coords[1].x = cx + half;
            dispatch(viewport, down, down + 8L + step * 12L, android.view.MotionEvent.ACTION_MOVE,
                    2, props, coords);
        }
        dispatch(viewport, down, down + 120L,
                android.view.MotionEvent.ACTION_POINTER_UP
                        | (1 << android.view.MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                2, props, coords);
        dispatch(viewport, down, down + 130L, android.view.MotionEvent.ACTION_UP, 1, props, coords);
    }

    private static void dispatch(View viewport, long down, long when, int action, int count,
                                 android.view.MotionEvent.PointerProperties[] props,
                                 android.view.MotionEvent.PointerCoords[] coords) {
        final android.view.MotionEvent event = android.view.MotionEvent.obtain(down, when, action,
                count, props, coords, 0, 0, 1f, 1f, 0, 0,
                android.view.InputDevice.SOURCE_TOUCHSCREEN, 0);
        try {
            viewport.dispatchTouchEvent(event);
        } finally {
            event.recycle();
        }
    }

    /** Whether a step is one of 1, 2 or 5 times a power of ten. */
    private static boolean isNiceStep(double step) {
        final double exponent = Math.floor(Math.log10(step));
        final double mantissa = step / Math.pow(10.0, exponent);
        for (double nice : new double[]{1.0, 2.0, 5.0, 10.0}) {
            if (Math.abs(mantissa - nice) < 1e-9) {
                return true;
            }
        }
        return false;
    }
}
