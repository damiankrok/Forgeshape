package com.forgeshape.app;

import static com.forgeshape.app.SketchTestSupport.dragSketch;
import static com.forgeshape.app.SketchTestSupport.selectTool;
import static com.forgeshape.app.SketchTestSupport.sketchState;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static com.forgeshape.app.WorkspaceTestSupport.waitForLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.pm.ActivityInfo;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.SystemClock;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestName;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

/**
 * `UI-3D-STATE-C2`: the runtime half of closing `UI3D-F-005`.
 *
 * <p>The audit adjudicated the defect in a frame — a selected Line whose
 * dimension native reported, with its numeric chip drawn and no extension
 * lines, dimension line or ticks anywhere in the picture. This suite asks the
 * same question so that the answer is a NUMBER: it captures the composed
 * display through {@code UiAutomation.takeScreenshot()}, which carries the
 * Vulkan viewport, and counts the pixels drawn in the annotation's own colour
 * inside a region derived entirely from what native reports.
 *
 * <p><b>Both measurements are differential, and the difference is the style.</b>
 * For the sketch annotation the two frames differ only in whether the Line is
 * selected, and the scanned strip is the dimension line itself — offset far
 * enough from the stroke it measures that the selected stroke's own emphasis
 * cannot reach it. For the Stage 020M leader the two frames are geometrically
 * IDENTICAL: all three leaders stand in both, and the only difference is that
 * one range moved from {@code Entities} to {@code Dimension}. So the amber the
 * second frame gains is that style and nothing else.
 *
 * <p>Nothing here is an aesthetic claim. The colour, the weight and the
 * contrast of the annotation stay OWNER LATER.
 */
@RunWith(AndroidJUnit4.class)
public final class Ui3dDimensionVisibilityTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    /** Names the ledger after the case, so two cases cannot overwrite one file. */
    @Rule
    public TestName testName = new TestName();

    /**
     * What counts as a pixel of the annotation, stated as a direction rather
     * than as an exact colour: a Dimension range is blended over whatever it
     * stands on, so its value depends on the ground.
     *
     * <p>Every vertex of both producers carries the emphasis tag, so
     * {@code gizmo.vert} draws them in {@code pc.highlight.rgb} — the gizmo's
     * held-handle amber, (1.00, 0.84, 0.28) on the dark grounds. Nothing else
     * the viewport draws is warm in this way: the X axis is red with no green
     * lift, the entity and grid levels are pure greys, and the one other amber
     * in the product, the selection outline, is switched off for both frames of
     * every measurement below.
     */
    private static boolean isAnnotationAmber(int pixel) {
        final int r = (pixel >> 16) & 0xFF;
        final int g = (pixel >> 8) & 0xFF;
        final int b = pixel & 0xFF;
        return r >= 120 && r - b >= 60 && g - b >= 40 && r >= g && g > b;
    }

    private File outDir;
    private final List<String> facts = new ArrayList<>();
    private boolean outlineWasVisible;

    @Before
    public void startFromABaselineProject() {
        outDir = new File(InstrumentationRegistry.getInstrumentation().getTargetContext()
                .getExternalFilesDir(null), "evidence/ui-3d-state-c2");
        //noinspection ResultOfMethodCallIgnored
        outDir.mkdirs();
        WorkspaceTestSupport.setOrientation(rule.getScenario(),
                ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        waitForLayout(rule.getScenario(), false);
        resetToBaselineConstruction(rule.getScenario());
        // The selection outline is the ONE other amber the viewport draws. It
        // is a session-only display setting, so switching it off costs the
        // project nothing and it is restored below.
        outlineWasVisible = NativeViewport.selectionOutlineVisible();
        NativeViewport.setSelectionOutlineVisible(false);
        settleLayout();
    }

    @After
    public void leaveTheDefaultsBehind() {
        NativeViewport.sketchCancel();
        NativeViewport.setBodyDimensionsMode(false);
        NativeViewport.setSelectionOutlineVisible(outlineWasVisible);
        writeFacts();
        resetToBaselineConstruction(rule.getScenario());
    }

    // =======================================================================
    // UI3DC2-04 — the SKETCH-UX-R1 selected-Line dimension
    // =======================================================================

    @Test
    public void ui3dc204_theSelectedLineDimensionIsDrawn() {
        beginSketchXy();
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        dragSketch(rule.getScenario(), -0.8, -0.5, 0.8, -0.5);
        assertEquals("one line placed", 1, SketchTestSupport.sketchEntityCount());

        selectTool(rule.getScenario(), R.id.tool_rail_select);
        SketchTestSupport.tapSketch(rule.getScenario(), 0.0, -0.5);
        settleLayout();

        final double[] dimension = new double[NativeViewport.SKETCH_DIMENSION_SIZE];
        assertTrue("native reports a dimension for the selected Line",
                NativeViewport.sketchLineDimension(dimension));
        final double[] entity = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        assertTrue("native reports the selected entity own values",
                NativeViewport.sketchSelectedEntity(entity));
        assertEquals("the selection is a straight Line",
                NativeViewport.SKETCH_ENTITY_KIND_LINE,
                (int) entity[NativeViewport.SKETCH_ENTITY_KIND]);
        fact("UI3DC2-04 selected entity id="
                + (long) entity[NativeViewport.SKETCH_ENTITY_ID]
                + " length=" + dimension[NativeViewport.SKETCH_DIMENSION_LENGTH]
                + " anchor=(" + dimension[NativeViewport.SKETCH_DIMENSION_ANCHOR_U]
                + ", " + dimension[NativeViewport.SKETCH_DIMENSION_ANCHOR_V] + ")");

        // The dimension LINE, in sketch coordinates, from values native gave:
        // its midpoint is the anchor, it is parallel to the stroke and it is as
        // long as the stroke. No constant of the annotation geometry is
        // mirrored here — only the two facts the seams already report.
        final double x0 = entity[NativeViewport.SKETCH_ENTITY_VALUES];
        final double y0 = entity[NativeViewport.SKETCH_ENTITY_VALUES + 1];
        final double x1 = entity[NativeViewport.SKETCH_ENTITY_VALUES + 2];
        final double y1 = entity[NativeViewport.SKETCH_ENTITY_VALUES + 3];
        final double span = Math.hypot(x1 - x0, y1 - y0);
        assertTrue("the stroke has a length to measure", span > 1e-6);
        final double du = (x1 - x0) / span;
        final double dv = (y1 - y0) / span;
        final double au = dimension[NativeViewport.SKETCH_DIMENSION_ANCHOR_U];
        final double av = dimension[NativeViewport.SKETCH_DIMENSION_ANCHOR_V];

        final float[] a = new float[2];
        final float[] b = new float[2];
        final float[] strokeMid = new float[2];
        assertTrue(NativeViewport.sketchScreenPoint(au - du * span / 2, av - dv * span / 2, a));
        assertTrue(NativeViewport.sketchScreenPoint(au + du * span / 2, av + dv * span / 2, b));
        assertTrue(NativeViewport.sketchScreenPoint((x0 + x1) / 2, (y0 + y1) / 2, strokeMid));

        // The strip is worth scanning only because it stands CLEAR of the
        // stroke: a selected stroke is drawn in the same amber, so a strip that
        // touched it would prove nothing about the annotation.
        final double standOff = Math.hypot(
                (a[0] + b[0]) / 2f - strokeMid[0], (a[1] + b[1]) / 2f - strokeMid[1]);
        fact("UI3DC2-04 dimension line px a=(" + a[0] + ", " + a[1] + ") b=(" + b[0] + ", "
                + b[1] + ") stand-off from the stroke=" + standOff + " px");
        assertTrue("the annotation stands clear of the stroke it measures", standOff > 30.0);

        final int[] origin = viewportOriginOnScreen();
        final List<Rect> excluded = boundsOnScreen(R.id.sketch_dimension_label);

        final Bitmap after = capture("ui3dc2_04_after_line_selected");
        final int amberSelected = amberAlong(after, a, b, origin, excluded);

        // The SAME strip with nothing selected. The stroke, the grid and the
        // axes are untouched by the selection; only the annotation goes.
        assertTrue("the selection clears", NativeViewport.sketchSelectEntity(0));
        SketchTestSupport.onNativeStateChanged(rule.getScenario());
        assertFalse("native reports no dimension once nothing is selected",
                NativeViewport.sketchLineDimension(
                        new double[NativeViewport.SKETCH_DIMENSION_SIZE]));
        final Bitmap before = capture("ui3dc2_04_before_nothing_selected");
        final int amberDeselected = amberAlong(before, a, b, origin, excluded);

        // The overlay is drawing at all: the grid, the axes and the stroke are
        // ink on the ground in the very frame the annotation is absent from.
        final int overlayInk = inkInViewport(before, origin);
        fact("UI3DC2-04 amber along the dimension line: selected=" + amberSelected
                + " deselected=" + amberDeselected + "; overlay ink in the deselected frame="
                + overlayInk);

        assertEquals("the annotation is absent with nothing selected", 0, amberDeselected);
        assertTrue("the annotation is DRAWN for the selected Line (0 before UI-3D-STATE-C2,"
                        + " measured " + amberSelected + ")", amberSelected >= 20);
        assertTrue("the rest of the overlay is still drawn in the same frame", overlayInk > 1000);

        NativeViewport.sketchCancel();
        settleLayout();
        assertEquals("the sketch is closed", NativeViewport.SKETCH_INACTIVE, sketchState());
    }

    // =======================================================================
    // UI3DC2-05 — the Stage 020M active-axis leader
    // =======================================================================

    @Test
    public void ui3dc205_theActiveAxisDimensionLeaderIsDrawn() {
        enterTransform();
        press(R.id.body_dimensions);
        final double[] state = new double[NativeViewport.BODY_DIM_SIZE];
        NativeViewport.bodyDimensionsState(state);
        assertTrue("Dimensions is open", state[NativeViewport.BODY_DIM_MODE_ACTIVE] != 0.0);
        assertEquals("it opens with no axis being read",
                NativeViewport.BODY_DIM_AXIS_NONE,
                (int) state[NativeViewport.BODY_DIM_ACTIVE_AXIS]);

        final int[] origin = viewportOriginOnScreen();
        final List<Rect> excluded = labelBounds();

        // BEFORE: all three leaders stand, all three in the Entities style.
        final Bitmap before = capture("ui3dc2_05_before_no_active_axis");
        final int amberNoAxis = amberInViewport(before, origin, excluded);
        final int inkNoAxis = inkInViewport(before, origin);

        // The PRODUCT path that makes an axis active: tapping its own viewport
        // label. Captured as it stands, editor and soft keyboard included,
        // because that is the state a user is actually in.
        openAxisEditor(0);
        NativeViewport.bodyDimensionsState(state);
        assertEquals("tapping the X label makes X the axis being read", 0,
                (int) state[NativeViewport.BODY_DIM_ACTIVE_AXIS]);
        final float[] anchor = new float[2];
        assertTrue("the X leader reports an anchor",
                NativeViewport.bodyDimensionLabelPoint(0, anchor));
        capture("ui3dc2_05_product_path_x_editor_open");

        // The MEASURED frame closes that editor again and asks native alone for
        // the same active axis, so the two scanned frames carry byte-identical
        // chrome — no compact editor, no soft keyboard — and differ in exactly
        // one thing: which range the X leader is drawn in. The product path
        // above is what proves the state is reachable; this is what makes the
        // difference between the two numbers attributable to the style.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.bodyDimensionLabels().closeEditor();
            return null;
        });
        settleLayout();
        assertTrue("native takes the active axis on its own",
                NativeViewport.setBodyDimensionAxis(0));
        NativeViewport.bodyDimensionsState(state);
        assertEquals("the X axis is the one being read", 0,
                (int) state[NativeViewport.BODY_DIM_ACTIVE_AXIS]);
        final List<Rect> excludedAfter = labelBounds();

        final Bitmap after = capture("ui3dc2_05_after_x_axis_active");
        final int amberActive = amberInViewport(after, origin, excludedAfter);
        final int inkActive = inkInViewport(after, origin);

        fact("UI3DC2-05 amber in the viewport: no active axis=" + amberNoAxis
                + " X active=" + amberActive + "; overlay ink " + inkNoAxis + " -> " + inkActive
                + "; X anchor=(" + anchor[0] + ", " + anchor[1] + ")");

        assertTrue("the active-axis leader is DRAWN (invisible before UI-3D-STATE-C2;"
                        + " measured " + amberNoAxis + " -> " + amberActive + ")",
                amberActive - amberNoAxis >= 40);
        // The other two leaders did not go anywhere: the frame keeps the ink it
        // had, so this is a style gaining a weight rather than a range moving.
        assertTrue("the neutral leaders are still drawn beside it",
                inkActive > inkNoAxis / 2);

        press(R.id.body_dimensions);
    }

    // -----------------------------------------------------------------------
    // Measurement
    // -----------------------------------------------------------------------

    /** Counts annotation-amber pixels in a plus/minus 4 px strip along one segment. */
    private int amberAlong(Bitmap frame, float[] a, float[] b, int[] origin, List<Rect> excluded) {
        assertNotNull("the display was captured", frame);
        final float ax = a[0] + origin[0];
        final float ay = a[1] + origin[1];
        final float bx = b[0] + origin[0];
        final float by = b[1] + origin[1];
        final double length = Math.hypot(bx - ax, by - ay);
        assertTrue("the dimension line has a length on screen", length > 8.0);
        final double nx = -(by - ay) / length;
        final double ny = (bx - ax) / length;
        int count = 0;
        final int steps = (int) Math.ceil(length);
        for (int i = 0; i <= steps; ++i) {
            final double t = (double) i / steps;
            for (int off = -4; off <= 4; ++off) {
                final int x = (int) Math.round(ax + (bx - ax) * t + nx * off);
                final int y = (int) Math.round(ay + (by - ay) * t + ny * off);
                if (!inBitmap(frame, x, y) || isExcluded(excluded, x, y)) {
                    continue;
                }
                if (isAnnotationAmber(frame.getPixel(x, y))) {
                    ++count;
                }
            }
        }
        return count;
    }

    /** Counts annotation-amber pixels over the whole viewport. */
    private int amberInViewport(Bitmap frame, int[] origin, List<Rect> excluded) {
        assertNotNull("the display was captured", frame);
        final Rect viewport = viewportRectOnScreen(origin);
        int count = 0;
        for (int y = viewport.top; y < viewport.bottom; y += 2) {
            for (int x = viewport.left; x < viewport.right; x += 2) {
                if (!inBitmap(frame, x, y) || isExcluded(excluded, x, y)) {
                    continue;
                }
                if (isAnnotationAmber(frame.getPixel(x, y))) {
                    ++count;
                }
            }
        }
        return count;
    }

    /**
     * How much of the viewport is NOT its own flat ground: the grid, the axes,
     * the entities, the leaders and the body together. A crude number, and its
     * only job is to say the renderer drew something in the frame the
     * annotation is missing from — so that "the annotation is absent" is never
     * confused with "the capture is blank".
     */
    private int inkInViewport(Bitmap frame, int[] origin) {
        assertNotNull("the display was captured", frame);
        final Rect viewport = viewportRectOnScreen(origin);
        final int ground = frame.getPixel(
                Math.min(viewport.left + 6, frame.getWidth() - 1),
                Math.min(viewport.top + 6, frame.getHeight() - 1));
        int count = 0;
        for (int y = viewport.top; y < viewport.bottom; y += 2) {
            for (int x = viewport.left; x < viewport.right; x += 2) {
                if (!inBitmap(frame, x, y)) {
                    continue;
                }
                if (channelDistance(frame.getPixel(x, y), ground) > 12) {
                    ++count;
                }
            }
        }
        return count;
    }

    private static int channelDistance(int a, int b) {
        return Math.max(Math.max(Math.abs(((a >> 16) & 0xFF) - ((b >> 16) & 0xFF)),
                        Math.abs(((a >> 8) & 0xFF) - ((b >> 8) & 0xFF))),
                Math.abs((a & 0xFF) - (b & 0xFF)));
    }

    private static boolean inBitmap(Bitmap frame, int x, int y) {
        return x >= 0 && y >= 0 && x < frame.getWidth() && y < frame.getHeight();
    }

    private static boolean isExcluded(List<Rect> excluded, int x, int y) {
        for (Rect rect : excluded) {
            if (rect.contains(x, y)) {
                return true;
            }
        }
        return false;
    }

    // -----------------------------------------------------------------------
    // Driving, all of it by semantic id
    // -----------------------------------------------------------------------

    private void beginSketchXy() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openAddPrimitive(workspace);
            workspace.addPrimitivePalette().findViewById(R.id.sketch_plane_by_name).performClick();
            workspace.addPrimitivePalette().findViewById(R.id.sketch_plane_xy).performClick();
            return null;
        });
        settleLayout();
        assertEquals("a sketch is open", NativeViewport.SKETCH_EDITING, sketchState());
    }

    private void enterTransform() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.uiState().setConstructionTool(EditorUiState.CONSTRUCTION_TOOL_TRANSFORM);
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    private void press(final int id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View view = workspace.findViewById(id);
            assertNotNull("the control is present", view);
            view.performClick();
            return null;
        });
        settleLayout();
    }

    /** Taps one axis viewport label, which is how a user makes it active. */
    private void openAxisEditor(final int axis) {
        press(axis == 0 ? R.id.body_dimension_label_x
                : axis == 1 ? R.id.body_dimension_label_y : R.id.body_dimension_label_z);
    }

    private List<Rect> labelBounds() {
        final List<Rect> out = new ArrayList<>();
        out.addAll(boundsOnScreen(R.id.body_dimension_label_x));
        out.addAll(boundsOnScreen(R.id.body_dimension_label_y));
        out.addAll(boundsOnScreen(R.id.body_dimension_label_z));
        return out;
    }

    private int[] viewportOriginOnScreen() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int[] at = new int[2];
            workspace.findViewById(R.id.viewport_surface).getLocationOnScreen(at);
            return at;
        });
    }

    private Rect viewportRectOnScreen(int[] origin) {
        final int[] size = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            return new int[]{viewport.getWidth(), viewport.getHeight()};
        });
        return new Rect(origin[0], origin[1], origin[0] + size[0], origin[1] + size[1]);
    }

    /** A shown view bounds on screen, inflated, or nothing when it is not up. */
    private List<Rect> boundsOnScreen(final int id) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final List<Rect> out = new ArrayList<>();
            final View view = workspace.findViewById(id);
            if (view != null && view.isShown()) {
                final int[] at = new int[2];
                view.getLocationOnScreen(at);
                out.add(new Rect(at[0] - 8, at[1] - 8,
                        at[0] + view.getWidth() + 8, at[1] + view.getHeight() + 8));
            }
            return out;
        });
    }

    // -----------------------------------------------------------------------
    // Evidence
    // -----------------------------------------------------------------------

    private Bitmap capture(String name) {
        SystemClock.sleep(500);
        final Bitmap frame =
                InstrumentationRegistry.getInstrumentation().getUiAutomation().takeScreenshot();
        if (frame == null) {
            fact(name + ": the display could not be captured");
            return null;
        }
        final File file = new File(outDir, name + ".png");
        try (FileOutputStream out = new FileOutputStream(file)) {
            frame.compress(Bitmap.CompressFormat.PNG, 100, out);
        } catch (IOException e) {
            fact(name + ": the frame could not be written — " + e);
        }
        fact(name + ".png written, " + frame.getWidth() + "x" + frame.getHeight());
        return frame;
    }

    private void fact(String line) {
        facts.add(line);
    }

    private void writeFacts() {
        if (facts.isEmpty()) {
            return;
        }
        try (PrintWriter writer = new PrintWriter(new File(outDir, "FACTS_" + testName.getMethodName() + ".txt"))) {
            for (String line : facts) {
                writer.println(line);
            }
        } catch (IOException e) {
            // Evidence, not a product path: a failure to write it is reported
            // through the run log rather than by failing a case that passed.
            System.out.println("ui-3d-state-c2: could not write the facts " + e);
        }
        facts.clear();
    }
}
