package com.forgeshape.app;

import static com.forgeshape.app.SketchTestSupport.dragSketch;
import static com.forgeshape.app.SketchTestSupport.drawRectangleAndExtrude;
import static com.forgeshape.app.SketchTestSupport.selectTool;
import static com.forgeshape.app.SketchTestSupport.sketchEntityCount;
import static com.forgeshape.app.SketchTestSupport.sketchState;
import static com.forgeshape.app.SketchTestSupport.tapWorld;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.pm.ActivityInfo;
import android.os.SystemClock;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;

/**
 * `SKETCH-NAVIGATOR-OUTCOME-R1`: the sketch orientation navigator turns the
 * REAL view.
 *
 * <p>Every navigator act here is a REAL tap through the window
 * ({@link ModelingOwnerRig#touchId}), and every assertion about the view reads
 * where sketch points actually land on screen through the live camera
 * ({@code sketchScreenPoint} and {@code debugProjectWorld} project through the
 * same camera the renderer and the picker use) -- never only the navigator's
 * own state. The control points are ASYMMETRIC: U = (1, 0) and V = (0, 1) in
 * sketch coordinates, so a symmetric drawing cannot hide a wrong turn.
 *
 * <p>Expected screen directions follow one rule, the navigator's own: the view
 * starts with sketch U to the right and V up; Flip looks at the back of the
 * plane (up becomes -V, nothing mirrored); +90 rolls the VIEW clockwise, so the
 * drawing appears to turn counter-clockwise (U goes up).
 *
 * <p>The exact eye side, the handedness and the view matrix are proved on the
 * host (`SKETCH_UX` NAVCAM checks); this proves what a user sees and touches.
 */
@RunWith(AndroidJUnit4.class)
public final class SketchNavigatorCameraTest {

    /** Pixels: what two projections of one point may differ by. */
    private static final float SAME_PIXEL = 1.5f;
    /** Pixels: how far a control point must be from the origin along an axis. */
    private static final float CLEAR = 20f;
    /** Metres: a drawn point after grid snap. */
    private static final double DRAWN = 0.06;

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private ModelingOwnerRig rig;
    private byte[] baselineProject;

    @Before
    public void setUp() {
        rig = new ModelingOwnerRig(rule.getScenario(), "sketch-navigator-outcome", "NAVCAM_DEVICE");
        WorkspaceTestSupport.resetToBaselineConstruction(rule.getScenario());
        baselineProject = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.encodeProject());
    }

    @After
    public void tearDown() {
        WorkspaceTestSupport.releaseOrientation(rule.getScenario());
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.supportChooserCancel();
            NativeViewport.sketchCancel();
            NativeViewport.enterConstructionMode();
            if (baselineProject != null) {
                NativeViewport.loadProject(baselineProject);
            }
            workspace.onNativeStateChanged();
            return null;
        });
        rig.restore();
    }

    // -----------------------------------------------------------------------
    // NAV-01: a new CAD project opens straight on XY, seen along +Z, ortho.
    // -----------------------------------------------------------------------

    @Test
    public void nav01_aNewCadProjectOpensOnXyAlongPlusZInOrtho() {
        rig.startAtHome();
        rig.touchId(R.id.home_new_project);
        rig.touchId(R.id.new_project_cad);
        assertEquals("the first sketch is open at once", NativeViewport.SKETCH_EDITING,
                sketchState());
        final double[] view = viewState();
        assertEquals(NativeViewport.WORKPLANE_XY, (int) view[NativeViewport.SKETCH_VIEW_PLANE]);
        assertEquals(0.0, view[NativeViewport.SKETCH_VIEW_FLIPPED], 0.0);
        assertEquals(0.0, view[NativeViewport.SKETCH_VIEW_QUARTER_TURNS], 0.0);
        assertEquals("exact orthographic", NativeViewport.PROJECTION_ORTHOGRAPHIC,
                (int) onWorkspace(rule.getScenario(),
                        (activity, workspace) -> NativeViewport.projectionMode()));
        assertScreenDirections("new CAD project", 0, false);
        // XY seen along +Z: the world X axis runs right and world Y runs up.
        final float[] o = world(0, 0, 0);
        final float[] x = world(1, 0, 0);
        final float[] y = world(0, 1, 0);
        assertTrue("world +X is to the right", x[0] - o[0] > CLEAR && Math.abs(x[1] - o[1]) < SAME_PIXEL);
        assertTrue("world +Y is up", o[1] - y[1] > CLEAR && Math.abs(y[0] - o[0]) < SAME_PIXEL);
        rig.capture("nav01_new_cad_xy");
    }

    // -----------------------------------------------------------------------
    // NAV-02: +90 / -90 by real taps turn the real view; inverses and 4 x 90.
    // -----------------------------------------------------------------------

    @Test
    public void nav02_quarterTurnsTurnTheRealViewAndUndoEachOther() {
        beginWorldSketch();
        final float[][] start = controlPixels();
        assertScreenDirections("start", 0, false);

        rig.touchId(R.id.sketch_navigator_rotate_cw);
        assertEquals(1.0, viewState()[NativeViewport.SKETCH_VIEW_QUARTER_TURNS], 0.0);
        assertScreenDirections("+90", 1, false);
        rig.capture("nav02_plus_90");
        rig.touchId(R.id.sketch_navigator_rotate_ccw);
        assertSamePixels("+90 then -90 is the start", start, controlPixels());

        rig.touchId(R.id.sketch_navigator_rotate_ccw);
        assertEquals(3.0, viewState()[NativeViewport.SKETCH_VIEW_QUARTER_TURNS], 0.0);
        assertScreenDirections("-90", 3, false);
        rig.touchId(R.id.sketch_navigator_rotate_cw);
        assertSamePixels("-90 then +90 is the start", start, controlPixels());

        for (int i = 0; i < 4; i++) {
            rig.touchId(R.id.sketch_navigator_rotate_cw);
            assertScreenDirections("+90 x" + (i + 1), (i + 1) % 4, false);
        }
        assertSamePixels("four quarter turns are the identity", start, controlPixels());
    }

    // -----------------------------------------------------------------------
    // NAV-03: Flip looks at the back of the plane; again, and it is back.
    // -----------------------------------------------------------------------

    @Test
    public void nav03_flipShowsTheBackOfThePlaneAndFlipsBack() {
        beginWorldSketch();
        final float[][] start = controlPixels();
        rig.touchId(R.id.sketch_navigator_flip);
        assertEquals(1.0, viewState()[NativeViewport.SKETCH_VIEW_FLIPPED], 0.0);
        assertScreenDirections("flipped", 0, true);
        rig.capture("nav03_flipped");
        rig.touchId(R.id.sketch_navigator_flip);
        assertEquals(0.0, viewState()[NativeViewport.SKETCH_VIEW_FLIPPED], 0.0);
        assertSamePixels("flip twice is the start", start, controlPixels());
    }

    // -----------------------------------------------------------------------
    // NAV-04 / NAV-09: XY, XZ, YZ by the plane chips, every Flip x roll
    // combination, and the plane frozen once geometry exists.
    // -----------------------------------------------------------------------

    @Test
    public void nav04_everyPlaneAndEveryFlipRollCombinationIsTheRealView() {
        beginWorldSketch();
        final int[] chips = {R.id.sketch_navigator_plane_xy, R.id.sketch_navigator_plane_xz,
                R.id.sketch_navigator_plane_yz};
        final int[] planes = {NativeViewport.WORKPLANE_XY, NativeViewport.WORKPLANE_XZ,
                NativeViewport.WORKPLANE_YZ};
        for (int p = 0; p < 3; p++) {
            rig.touchId(chips[p]);
            assertEquals("plane chip " + p, planes[p],
                    (int) viewState()[NativeViewport.SKETCH_VIEW_PLANE]);
            assertEquals("the projection stays orthographic",
                    NativeViewport.PROJECTION_ORTHOGRAPHIC, (int) onWorkspace(rule.getScenario(),
                            (activity, workspace) -> NativeViewport.projectionMode()));
            for (int flip = 0; flip < 2; flip++) {
                for (int turn = 0; turn < 4; turn++) {
                    final double[] v = viewState();
                    assertScreenDirections("plane " + p + " flip " + flip + " turns " + turn,
                            (int) v[NativeViewport.SKETCH_VIEW_QUARTER_TURNS],
                            v[NativeViewport.SKETCH_VIEW_FLIPPED] != 0.0);
                    rig.touchId(R.id.sketch_navigator_rotate_cw);
                }
                rig.touchId(R.id.sketch_navigator_flip);
            }
        }
        // Once the sketch has geometry the plane is frozen, and the chip says
        // so rather than appearing to switch.
        rig.touchId(R.id.sketch_navigator_plane_xy);
        drawLine(0.5, 0.25, 1.5, 1.0);
        assertEquals(0.0, viewState()[NativeViewport.SKETCH_VIEW_PLANE_SWITCHABLE], 0.0);
        assertEquals(NativeViewport.CAD_SKETCH_NOT_EMPTY, (int) onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sketchSetSupportPlane(
                        NativeViewport.WORKPLANE_XZ)));
        assertEquals(NativeViewport.WORKPLANE_XY,
                (int) viewState()[NativeViewport.SKETCH_VIEW_PLANE]);
    }

    // -----------------------------------------------------------------------
    // NAV-06 / NAV-07: after a turn and a flip, drawing lands where the user
    // sees it, and the view acts change no authored or project truth.
    // -----------------------------------------------------------------------

    @Test
    public void nav06_afterTurningTheViewADragDrawsWhereTheUserSeesIt() {
        beginWorldSketch();
        rig.touchId(R.id.sketch_navigator_rotate_cw);
        rig.touchId(R.id.sketch_navigator_flip);
        assertScreenDirections("turned and flipped", 1, true);
        // The drag goes between the pixels where the RENDERED world points
        // are (XY world sketch: world (u, v, 0)), not through sketchScreenPoint.
        // Whole-metre points, so the view-adaptive grid snap (0.2 m here, run
        // 41509947297) leaves them exact; a wrong mapping would land metres
        // away, far outside the tolerance.
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        final float[] from = world(1.0, 0.0, 0.0);
        final float[] to = world(2.0, 1.0, 0.0);
        rig.realDrag(from[0], from[1], to[0], to[1], 12);
        assertEquals("one line was drawn", 1, sketchEntityCount());
        final double[] line = onlyEntity();
        rig.fact("drawn_line_after_turn_and_flip", Arrays.toString(line));
        assertEquals(NativeViewport.SKETCH_ENTITY_KIND_LINE,
                (int) line[NativeViewport.SKETCH_ENTITY_KIND]);
        assertEquals("start u", 1.0, line[2], DRAWN);
        assertEquals("start v", 0.0, line[3], DRAWN);
        assertEquals("end u", 2.0, line[4], DRAWN);
        assertEquals("end v", 1.0, line[5], DRAWN);

        // The view acts change nothing true: the line, the project, history.
        final String before = truth();
        rig.touchId(R.id.sketch_navigator_rotate_cw);
        rig.touchId(R.id.sketch_navigator_flip);
        rig.touchId(R.id.sketch_navigator_rotate_ccw);
        rig.touchId(R.id.sketch_navigator_rotate_ccw);
        assertEquals("view acts move no authored or project truth", before, truth());
        rig.fact("truth_after_view_acts", truth());
    }

    // -----------------------------------------------------------------------
    // NAV-08: leaving the sketch gives back the camera the user had, and Back
    // to Sketch shows the aligned view with a navigator that says so.
    // -----------------------------------------------------------------------

    @Test
    public void nav08_cancelAndCommitGiveBackThePreSketchCameraAndBackToSketchIsHonest() {
        // A perspective, orbited camera before the sketch.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.setProjectionMode(NativeViewport.PROJECTION_PERSPECTIVE);
            assertTrue(NativeViewport.debugSetCameraPose(0.6f, 0.4f, 9.0f));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        final float[][] preSketch = worldPixels();
        final int preProjection = projection();

        // Cancel.
        beginWorldSketch();
        rig.touchId(R.id.sketch_navigator_rotate_cw);
        rig.touchId(R.id.sketch_navigator_flip);
        rig.touchId(R.id.cancel_sketch);
        assertEquals(NativeViewport.SKETCH_INACTIVE, sketchState());
        assertEquals("Cancel restores the projection", preProjection, projection());
        assertSamePixels("Cancel restores the pre-sketch camera", preSketch, worldPixels());

        // Finish, Back to Sketch: the aligned view, and a navigator that says
        // the view is the aligned one (reset-on-re-entry, `CR-06` OD-09 (a)).
        beginWorldSketch();
        rig.touchId(R.id.sketch_navigator_rotate_cw);
        rig.touchId(R.id.sketch_navigator_flip);
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), -0.5, -0.5, 0.5, 0.5);
        rig.touchId(R.id.finish_sketch);
        assertEquals(NativeViewport.SKETCH_READY, sketchState());
        rig.touchId(R.id.back_to_sketch);
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
        final double[] back = viewState();
        rig.fact("navigator_after_back_to_sketch", Arrays.toString(back));
        assertEquals("Back to Sketch: not flipped", 0.0,
                back[NativeViewport.SKETCH_VIEW_FLIPPED], 0.0);
        assertEquals("Back to Sketch: no roll", 0.0,
                back[NativeViewport.SKETCH_VIEW_QUARTER_TURNS], 0.0);
        assertScreenDirections("Back to Sketch", 0, false);

        // Commit (Finish, Extrude): one Construction step, pre-sketch camera.
        final int undoBefore = undo();
        rig.touchId(R.id.sketch_navigator_rotate_ccw);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.finish_sketch).performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final android.widget.EditText field =
                    workspace.sketchEditor().findViewById(R.id.field_extrude_depth);
            field.setText("0.5");
            workspace.findViewById(R.id.sketch_extrude_commit).performClick();
            return null;
        });
        settleLayout();
        assertEquals(NativeViewport.SKETCH_INACTIVE, sketchState());
        assertEquals("one Construction step for the commit", undoBefore + 1, undo());
        assertEquals("Commit restores the projection", preProjection, projection());
        assertSamePixels("Commit restores the pre-sketch camera", preSketch, worldPixels());
    }

    // -----------------------------------------------------------------------
    // NAV-05: a face sketch -- Flip is not offered and refused below JNI; the
    // roll turns the real view; the support reference is untouched.
    // -----------------------------------------------------------------------

    @Test
    public void nav05_aFaceSketchRollsButNeverFlipsAndKeepsItsSupport() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.CAD_OK,
                    NativeViewport.sketchBegin(NativeViewport.WORKPLANE_XY));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        drawRectangleAndExtrude(rule.getScenario(), 2.0, 2.0, "2.0");
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openAddPrimitive(workspace);
            final View newSketch = workspace.addPrimitivePalette().findViewById(R.id.add_sketch);
            assertNotNull(newSketch);
            newSketch.performClick();
            return null;
        });
        settleLayout();
        tapWorld(rule.getScenario(), 0.0, 0.0, 2.0);
        tapWorld(rule.getScenario(), 0.0, 0.0, 2.0);
        assertEquals("a sketch on the far cap", NativeViewport.SKETCH_EDITING, sketchState());
        assertEquals(1.0, viewState()[NativeViewport.SKETCH_VIEW_FACE_SUPPORTED], 0.0);

        assertFalse("Flip is not offered on a face", onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.findViewById(R.id.sketch_navigator_flip)
                        .isEnabled()));
        final int flip = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sketchSetViewFlipped(true));
        assertNotEquals("and refused below JNI", NativeViewport.CAD_OK, flip);
        assertEquals(0.0, viewState()[NativeViewport.SKETCH_VIEW_FLIPPED], 0.0);
        assertScreenDirections("face, aligned", 0, false);

        rig.touchId(R.id.sketch_navigator_rotate_cw);
        assertScreenDirections("face, +90", 1, false);
        assertEquals("still on its face", 1.0,
                viewState()[NativeViewport.SKETCH_VIEW_FACE_SUPPORTED], 0.0);
        rig.capture("nav05_face_plus_90");
    }

    // -----------------------------------------------------------------------
    // NAV-09: with no sketch the navigator's acts are refused and the camera
    // does not move.
    // -----------------------------------------------------------------------

    @Test
    public void nav09_withNoSketchTheViewActsAreRefusedAndMoveNothing() {
        final float[][] before = worldPixels();
        final String truth = truth();
        assertEquals(NativeViewport.CAD_NOT_SKETCHING, (int) onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sketchSetViewFlipped(true)));
        assertEquals(NativeViewport.CAD_NOT_SKETCHING, (int) onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sketchRotateView(1)));
        settleLayout();
        assertSamePixels("no sketch: the camera did not move", before, worldPixels());
        assertEquals(truth, truth());
    }

    // -----------------------------------------------------------------------
    // NAV-10: portrait and landscape, font scale 1.0 and 1.5.
    // -----------------------------------------------------------------------

    @Test
    public void nav10_theNavigatorTurnsTheViewAcrossOrientationAndFontScale() {
        try {
            for (String scale : new String[]{"1.0", "1.5"}) {
                setFontScale(scale);
                for (int orientation : new int[]{ActivityInfo.SCREEN_ORIENTATION_PORTRAIT,
                        ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE}) {
                    WorkspaceTestSupport.setOrientation(rule.getScenario(), orientation);
                    settleLayout();
                    SystemClock.sleep(600);
                    settleLayout();
                    final String where = "font " + scale + " orientation " + orientation;
                    beginWorldSketch();
                    rig.touchId(R.id.sketch_navigator_rotate_cw);
                    assertScreenDirections(where + " +90", 1, false);
                    rig.touchId(R.id.sketch_navigator_flip);
                    assertScreenDirections(where + " +90 flipped", 1, true);
                    rig.capture("nav10_font_" + scale.replace('.', '_') + "_" + orientation);
                    rig.touchId(R.id.cancel_sketch);
                    assertEquals(NativeViewport.SKETCH_INACTIVE, sketchState());
                }
            }
        } finally {
            setFontScale("1.0");
        }
    }

    // -----------------------------------------------------------------------
    // The view rule, and the measurements
    // -----------------------------------------------------------------------

    /**
     * Where sketch U = (1, 0) and V = (0, 1) must appear relative to the
     * origin, from the navigator's own rule: the view's right and up axes, in
     * sketch coordinates, start as U and V; Flip makes up -V; each quarter turn
     * maps (right, up) to (-up, right).
     */
    private void assertScreenDirections(String where, int turns, boolean flipped) {
        double[] right = {1, 0};
        double[] up = {0, flipped ? -1 : 1};
        for (int i = 0; i < turns; i++) {
            final double[] r = {-up[0], -up[1]};
            up = right;
            right = r;
        }
        final float[][] px = controlPixels();
        final float[] o = px[0];
        final float[][] axes = {px[1], px[2]};
        final String[] names = {"U", "V"};
        for (int a = 0; a < 2; a++) {
            // Screen x grows right, screen y grows DOWN.
            final double wantRight = a == 0 ? right[0] : right[1];
            final double wantUp = a == 0 ? up[0] : up[1];
            final float dx = axes[a][0] - o[0];
            final float dy = o[1] - axes[a][1];
            final String got = where + ": " + names[a] + " at dx=" + dx + " dy(up)=" + dy
                    + " expected right=" + wantRight + " up=" + wantUp;
            if (wantRight != 0) {
                assertTrue(got, Math.signum(dx) == Math.signum(wantRight) && Math.abs(dx) > CLEAR);
                assertTrue(got, Math.abs(dy) < SAME_PIXEL);
            } else {
                assertTrue(got, Math.signum(dy) == Math.signum(wantUp) && Math.abs(dy) > CLEAR);
                assertTrue(got, Math.abs(dx) < SAME_PIXEL);
            }
        }
    }

    /** Origin, U and V of the sketch on screen, through the live camera. */
    private float[][] controlPixels() {
        final double[][] points = {{0, 0}, {1, 0}, {0, 1}};
        final float[][] out = new float[3][];
        for (int i = 0; i < 3; i++) {
            final float[] at = new float[2];
            final double u = points[i][0];
            final double v = points[i][1];
            assertTrue("sketch point projects", onWorkspace(rule.getScenario(),
                    (activity, workspace) -> NativeViewport.sketchScreenPoint(u, v, at)));
            out[i] = at;
        }
        return out;
    }

    /** Three asymmetric world points on screen, for camera comparisons. */
    private float[][] worldPixels() {
        return new float[][]{world(0, 0, 0), world(1, 0, 0), world(0, 2, 0), world(0, 0, 3)};
    }

    private float[] world(double x, double y, double z) {
        final float[] at = new float[2];
        assertTrue("world point projects", onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.debugProjectWorld(x, y, z, at)));
        return at;
    }

    private static void assertSamePixels(String what, float[][] want, float[][] got) {
        for (int i = 0; i < want.length; i++) {
            for (int c = 0; c < 2; c++) {
                assertEquals(what + " point " + i + " " + Arrays.toString(got[i]) + " vs "
                        + Arrays.toString(want[i]), want[i][c], got[i][c], SAME_PIXEL);
            }
        }
    }

    private void beginWorldSketch() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.CAD_OK,
                    NativeViewport.sketchBegin(NativeViewport.WORKPLANE_XY));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
    }

    private void drawLine(double u0, double v0, double u1, double v1) {
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        dragSketch(rule.getScenario(), u0, v0, u1, v1);
    }

    private double[] viewState() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] out = new double[NativeViewport.SKETCH_VIEW_SIZE];
            NativeViewport.sketchViewState(out);
            return out;
        });
    }

    private double[] onlyEntity() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final long[] ids = new long[16];
            assertEquals(1, NativeViewport.sketchEntityIds(ids));
            final double[] out = new double[NativeViewport.SKETCH_ENTITY_VALUES_SIZE];
            assertTrue(NativeViewport.sketchEntityValues(ids[0], out));
            return out;
        });
    }

    /** The sketch's entities, the project bytes, history and fingerprint. */
    private String truth() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final StringBuilder s = new StringBuilder();
            final long[] ids = new long[64];
            final int n = NativeViewport.sketchEntityIds(ids);
            for (int i = 0; i < n; i++) {
                final double[] v = new double[NativeViewport.SKETCH_ENTITY_VALUES_SIZE];
                NativeViewport.sketchEntityValues(ids[i], v);
                s.append(Arrays.toString(v));
            }
            final byte[] project = NativeViewport.encodeProject();
            s.append(" project=").append(project == null ? -1 : Arrays.hashCode(project))
                    .append('/').append(project == null ? -1 : project.length)
                    .append(" fp=").append(Long.toHexString(NativeViewport.projectFingerprint()))
                    .append(" undo=").append(NativeViewport.constructionUndoDepth())
                    .append(" redo=").append(NativeViewport.constructionRedoDepth())
                    .append(" bodies=").append(NativeViewport.sceneBodyCount());
            return s.toString();
        });
    }

    private int undo() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.constructionUndoDepth());
    }

    private int projection() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.projectionMode());
    }

    private void setFontScale(String scale) {
        shell("settings put system font_scale " + scale);
        final float want = Float.parseFloat(scale);
        boolean applied = false;
        for (int attempt = 0; attempt < 60 && !applied; attempt++) {
            final Float now = onWorkspace(rule.getScenario(), (activity, workspace) ->
                    activity.getResources().getConfiguration().fontScale);
            applied = now != null && Math.abs(now - want) < 0.01f;
            if (!applied) SystemClock.sleep(100);
        }
        assertTrue("font scale " + scale + " took effect", applied);
        settleLayout();
        SystemClock.sleep(400);
        settleLayout();
        rig.fact("font_scale", scale);
    }

    private static void shell(String command) {
        final android.os.ParcelFileDescriptor pipe = InstrumentationRegistry
                .getInstrumentation().getUiAutomation().executeShellCommand(command);
        try (InputStream stream = new android.os.ParcelFileDescriptor.AutoCloseInputStream(pipe)) {
            final byte[] drain = new byte[64];
            while (stream.read(drain) >= 0) {
                // Drained so the command completes before the case continues.
            }
        } catch (IOException ignored) {
            // The command still ran; nothing here reads its output.
        }
        SystemClock.sleep(200);
    }
}
