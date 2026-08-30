package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.closePrecision;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.openPrecision;
import static com.forgeshape.app.WorkspaceTestSupport.releaseOrientation;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.selectConstructionTool;
import static com.forgeshape.app.WorkspaceTestSupport.setOrientation;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static com.forgeshape.app.WorkspaceTestSupport.trailingHost;
import static com.forgeshape.app.WorkspaceTestSupport.waitForLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.graphics.Insets;
import android.graphics.Rect;
import android.os.Build;
import android.view.View;
import android.view.WindowInsets;
import android.widget.LinearLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * UILR2C-01..12: the right context host keeps the trailing edge in every window
 * and every state.
 *
 * <p>The defect these cases exist for was one line of composition. The precision
 * surface was <b>appended</b> to the middle row, so in every window that places
 * it beside the model it became a laid-out sibling AFTER the host and pushed the
 * host inward by its own width — an 8 dp resting inset became roughly 320 dp in
 * a short landscape window and roughly 360 dp on a tablet, for as long as Exact
 * or Details was open. Everything here is written against that: the host's
 * external top, trailing inset and width in each state, and the ORDER that
 * produces them.
 *
 * <p>Where a window cannot be materialised inside an instrumentation process —
 * the two expanded cells need a 1280x800 dp window — the case asserts the
 * decision arithmetic and the composition that decide the answer, and the
 * absolute dp for those cells comes from the measured device matrix.
 */
@RunWith(AndroidJUnit4.class)
public final class EditorWorkspaceRightHostPlacementTest {

    /** Every R2 state the host must hold its external frame through. */
    private static final String[] STATES = {
            "CONSTRUCTION_SHAPE_REST", "TRANSFORM_MOVE_WORLD", "TRANSFORM_ROTATE_WORLD",
            "TRANSFORM_SCALE", "TRANSFORM_MOVE_LOCAL", "EXACT_OPEN", "EXACT_IME",
            "EXACT_CLOSE", "SCULPT_REST", "SCULPT_DETAILS", "SCULPT_CLOSE"};

    @Rule
    public final ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    @Before
    public void reset() {
        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        waitForLayout(rule.getScenario(), false);
        resetToBaselineConstruction(rule.getScenario());
    }

    @After
    public void restoreEnvironment() {
        clearIme();
        releaseOrientation(rule.getScenario());
    }

    // -----------------------------------------------------------------------
    // Compact — C10 / C13
    // -----------------------------------------------------------------------

    /** UILR2C-01. Compact portrait holds one external host frame for all 11 states. */
    @Test
    public void uilr2c_01_compactHostFrameIsInvariantAcrossEveryState() {
        assertHostFrameInvariantAcrossStates("C10 (compact portrait)");
    }

    /**
     * UILR2C-02. The compact host's external anchors take no font scale.
     *
     * <p>C13 is C10 at font 1.3, so its geometry is the same answer unless one of
     * the three anchors is text-derived. None is: they are density-backed dimens,
     * and the widest control the host carries still fits inside the fixed width,
     * so a larger font can only lengthen the column — which internal vertical
     * scrolling absorbs.
     */
    @Test
    public void uilr2c_02_compactHostAnchorsAreFontScaleIndependent() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final Context scaled = fontScaled(activity, 1.3f);
            for (int dimen : new int[]{R.dimen.trailing_host_width, R.dimen.row_gap,
                    R.dimen.brush_gap, R.dimen.rail_item_width, R.dimen.rail_padding,
                    R.dimen.control_height, R.dimen.row_gap_small}) {
                assertEquals(activity.getResources().getResourceEntryName(dimen)
                                + " must not move with the font scale",
                        EditorControlStyles.dimen(activity, dimen),
                        EditorControlStyles.dimen(scaled, dimen));
            }
            final int content = EditorControlStyles.dimen(scaled, R.dimen.rail_item_width)
                    + 2 * EditorControlStyles.dimen(scaled, R.dimen.rail_padding);
            assertTrue("the widest host control plus its padding must fit the fixed width",
                    content <= EditorControlStyles.dimen(scaled, R.dimen.trailing_host_width));
            assertNotNull("height pressure is absorbed by internal vertical scrolling",
                    workspace.findViewById(R.id.tool_rail_scroll));
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // Short landscape — L10 / L13
    // -----------------------------------------------------------------------

    /**
     * UILR2C-03. Short landscape holds one external host frame for all 11 states.
     *
     * <p>This is the cell the correction is for: a 411 dp-tall window takes the
     * side placement, which is where the panel used to translate the host.
     */
    @Test
    public void uilr2c_03_shortLandscapeHostFrameIsInvariantAcrossEveryState() {
        goLandscape();
        assertHostFrameInvariantAcrossStates("L10 (short landscape)");
    }

    /**
     * UILR2C-04. Nothing in the short-landscape host widens with the font scale.
     *
     * <p>L13 differs from L10 only in text size, and the host's width is a fixed
     * dp whose widest internal control already fits. A longer column is height,
     * and height is content-driven and scrolls.
     */
    @Test
    public void uilr2c_04_shortLandscapeHostWidthTakesNoFontScale() {
        goLandscape();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final Context scaled = fontScaled(activity, 1.3f);
            assertEquals(EditorControlStyles.dimen(activity, R.dimen.trailing_host_width),
                    EditorControlStyles.dimen(scaled, R.dimen.trailing_host_width));
            assertEquals(EditorControlStyles.dimen(activity, R.dimen.brush_gap),
                    EditorControlStyles.dimen(scaled, R.dimen.brush_gap));
            assertEquals("the laid-out host is exactly its fixed width",
                    EditorControlStyles.dimen(activity, R.dimen.trailing_host_width),
                    trailingHost(workspace).getWidth());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // Expanded — E10 / E13
    // -----------------------------------------------------------------------

    /**
     * UILR2C-05. The expanded window docks the panel and still leaves the host
     * the trailing edge.
     *
     * <p>The dock's width is what used to be added to the host's inset. Here the
     * arithmetic that produces it is asserted together with the composition that
     * keeps it out of that inset, because a 1280x800 dp window cannot be created
     * from inside the process; the measured dp for E10 is in the matrix.
     */
    @Test
    public void uilr2c_05_expandedDocksThePanelInboardOfTheHost() {
        assertEquals(WorkspaceLayoutMode.EXPANDED, WorkspaceLayoutMode.forWindow(1280, 800));
        assertEquals(WorkspaceLayoutMode.InspectorPlacement.SIDE_DOCK,
                WorkspaceLayoutMode.EXPANDED.inspectorPlacement(800));
        assertEquals(340, WorkspaceLayoutMode.sideDockWidthDp(1280));
        assertTrue(WorkspaceLayoutMode.EXPANDED.objectsDocked(1280));
        goLandscape();
        selectTransform();
        openExact();
        assertSideSurfaceIsSeatedBeforeTheHost();
    }

    /**
     * UILR2C-06. The expanded decision takes no font scale either.
     *
     * <p>{@link WorkspaceLayoutMode} is a pure function of window dp and has no
     * text input at all, so E13 resolves to exactly E10's placement, dock width
     * and Objects decision. The host's own anchors were shown font-independent in
     * UILR2C-02.
     */
    @Test
    public void uilr2c_06_expandedGeometryIsFontScaleIndependent() {
        assertEquals(WorkspaceLayoutMode.InspectorPlacement.SIDE_DOCK,
                WorkspaceLayoutMode.forWindow(1280, 800).inspectorPlacement(800));
        assertEquals(340, WorkspaceLayoutMode.sideDockWidthDp(1280));
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final Context scaled = fontScaled(activity, 1.3f);
            assertEquals(EditorControlStyles.dimen(activity, R.dimen.trailing_host_width),
                    EditorControlStyles.dimen(scaled, R.dimen.trailing_host_width));
            assertEquals(EditorControlStyles.dimen(activity, R.dimen.brush_gap),
                    EditorControlStyles.dimen(scaled, R.dimen.brush_gap));
            assertEquals(EditorControlStyles.dimen(activity, R.dimen.overlay_anchor_gap),
                    EditorControlStyles.dimen(scaled, R.dimen.overlay_anchor_gap));
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // The correction itself
    // -----------------------------------------------------------------------

    /** UILR2C-07. Short landscape: Exact, Exact+IME and Details never translate the host. */
    @Test
    public void uilr2c_07_shortLandscapeExactAndDetailsNeverTranslateTheHost() {
        goLandscape();
        final int inset = onWorkspace(rule.getScenario(), (activity, workspace) ->
                EditorControlStyles.dimen(activity, R.dimen.brush_gap));
        selectTransform();
        final Rect resting = hostRect();
        assertEquals("resting trailing inset", inset, trailingInset(resting));

        openExact();
        assertSameExternalFrame("Exact open", resting, hostRect());
        assertEquals("Exact open trailing inset", inset, trailingInset(hostRect()));

        dispatchIme(true);
        assertSameExternalFrame("Exact + IME", resting, hostRect());
        assertEquals("Exact + IME trailing inset", inset, trailingInset(hostRect()));
        clearIme();
        closeExact();

        enterSculpt();
        final Rect sculptResting = hostRect();
        assertEquals("Sculpt resting trailing inset", inset, trailingInset(sculptResting));
        openExact();
        assertSameExternalFrame("Sculpt Details", sculptResting, hostRect());
        assertEquals("Sculpt Details trailing inset", inset, trailingInset(hostRect()));
        closeExact();
        assertSameExternalFrame("Details closed", sculptResting, hostRect());
    }

    /**
     * UILR2C-08. A side-placed panel is always seated BEFORE the host in the row.
     *
     * <p>This is the general form of UILR2C-07 and the case that covers the
     * expanded cells: whatever the panel's width, it cannot reach the host's
     * inset while the host is the last child of the row they share.
     */
    @Test
    public void uilr2c_08_sidePanelNeverTakesTheTrailingEdgeFromTheHost() {
        goLandscape();
        selectTransform();
        openExact();
        assertSideSurfaceIsSeatedBeforeTheHost();
        closeExact();
        enterSculpt();
        openExact();
        assertSideSurfaceIsSeatedBeforeTheHost();
        closeExact();
    }

    /** UILR2C-09. Compact Exact hide/restore returns the exact resting bounds. */
    @Test
    public void uilr2c_09_compactExactHideRestoreRetainsRestingBounds() {
        selectTransform();
        final Rect host = hostRect();
        final Rect capsule = viewRect(R.id.objects_capsule);
        final Rect history = viewRect(R.id.history_group);
        openExact();
        closeExact();
        assertEquals("Objects capsule", capsule, viewRect(R.id.objects_capsule));
        assertEquals("history capsule", history, viewRect(R.id.history_group));
        assertEquals("right host", host, hostRect());
    }

    /** UILR2C-10. Short-window persistent chrome returns with every delta at zero. */
    @Test
    public void uilr2c_10_shortWindowPersistentChromeReturnsUnchanged() {
        goLandscape();
        selectTransform();
        final int[] persistent = {R.id.global_toolbar, R.id.objects_capsule,
                R.id.history_group, R.id.workspace_trailing_host};
        final Rect[] before = new Rect[persistent.length];
        for (int i = 0; i < persistent.length; i++) {
            before[i] = viewRect(persistent[i]);
        }
        openExact();
        closeExact();
        for (int i = 0; i < persistent.length; i++) {
            assertEquals("delta must be 0 dp for view " + i,
                    before[i], viewRect(persistent[i]));
        }
    }

    /**
     * UILR2C-11. Intrinsic hit geometry meets the 48 dp floor.
     *
     * <p>Intrinsic size is the control's own laid-out box, which is what the floor
     * is about. The clipped intersection a scrolling container reports is measured
     * too and carried in the failure message, so a control that is merely partly
     * scrolled out is never confused with one that is too small.
     */
    @Test
    public void uilr2c_11_intrinsicTargetsMeetTheTouchFloor() {
        selectTransform();
        assertIntrinsicTouchFloor("compact portrait");
        goLandscape();
        selectTransform();
        assertIntrinsicTouchFloor("short landscape");
    }

    /** UILR2C-12. The IME leaves the surface full-window and the host anchored. */
    @Test
    public void uilr2c_12_imeKeepsSurfaceFullWindowAndHostAnchored() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return;
        }
        goLandscape();
        selectTransform();
        openExact();
        final Rect resting = hostRect();
        final Rect surface = viewRect(R.id.viewport_surface);
        dispatchIme(true);
        final Rect withIme = hostRect();
        assertSameExternalFrame("IME", resting, withIme);
        assertEquals("the Vulkan surface stays the whole window under the IME",
                surface, viewRect(R.id.viewport_surface));
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            assertEquals("surface width is the window width",
                    workspace.getWidth(), viewport.getWidth());
            assertEquals("surface height is the window height",
                    workspace.getHeight(), viewport.getHeight());
            return null;
        });
        assertTrue("available host height may shrink, never its anchors",
                withIme.height() <= resting.height());
        clearIme();
    }

    // -----------------------------------------------------------------------
    // State walk
    // -----------------------------------------------------------------------

    private void assertHostFrameInvariantAcrossStates(String cell) {
        final Rect baseline = enterState(STATES[0]);
        final int inset = onWorkspace(rule.getScenario(), (activity, workspace) ->
                EditorControlStyles.dimen(activity, R.dimen.brush_gap));
        assertEquals(cell + " resting trailing inset", inset, trailingInset(baseline));
        for (int i = 1; i < STATES.length; i++) {
            final Rect actual = enterState(STATES[i]);
            assertSameExternalFrame(cell + " / " + STATES[i], baseline, actual);
            assertEquals(cell + " / " + STATES[i] + " trailing inset",
                    inset, trailingInset(actual));
        }
    }

    /** Drives the product to one named R2 state and reports the host's frame. */
    private Rect enterState(String state) {
        switch (state) {
            case "CONSTRUCTION_SHAPE_REST":
                click(R.id.tool_rail_shape);
                break;
            case "TRANSFORM_MOVE_WORLD":
                click(R.id.tool_rail_place);
                click(R.id.transform_mode_move);
                click(R.id.transform_space_world);
                break;
            case "TRANSFORM_ROTATE_WORLD":
                click(R.id.transform_mode_rotate);
                break;
            case "TRANSFORM_SCALE":
                click(R.id.transform_mode_scale);
                break;
            case "TRANSFORM_MOVE_LOCAL":
                click(R.id.transform_mode_move);
                click(R.id.transform_space_local);
                break;
            case "EXACT_OPEN":
                openExact();
                break;
            case "EXACT_IME":
                dispatchIme(true);
                break;
            case "EXACT_CLOSE":
                clearIme();
                closeExact();
                break;
            case "SCULPT_REST":
                enterSculpt();
                break;
            case "SCULPT_DETAILS":
                openExact();
                break;
            case "SCULPT_CLOSE":
                closeExact();
                break;
            default:
                throw new AssertionError("unknown state " + state);
        }
        return hostRect();
    }

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    private void assertSideSurfaceIsSeatedBeforeTheHost() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (workspace.inspectorPlacement()
                    == WorkspaceLayoutMode.InspectorPlacement.BOTTOM_SHEET) {
                return null;
            }
            final LinearLayout row = workspace.workspaceMiddleRow();
            final int panel = row.indexOfChild(workspace.propertyInspector());
            final int host = row.indexOfChild(trailingHost(workspace));
            assertTrue("the side panel must share the host's row", panel >= 0);
            assertTrue("the side panel is seated BEFORE the host, never appended after it",
                    panel < host);
            assertEquals("the host is the row's trailing child",
                    row.getChildCount() - 1, host);
            return null;
        });
    }

    private void assertIntrinsicTouchFloor(String window) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int floor = EditorControlStyles.dimen(activity, R.dimen.control_height);
            for (int id : new int[]{R.id.tool_rail_shape, R.id.tool_rail_place,
                    R.id.transform_mode_move, R.id.transform_mode_rotate,
                    R.id.transform_mode_scale, R.id.transform_space_world,
                    R.id.transform_space_local, R.id.precision_toggle}) {
                final View action = workspace.findViewById(id);
                if (action == null || action.getVisibility() != View.VISIBLE) {
                    continue;
                }
                final Rect visible = new Rect();
                final boolean onScreen = action.getGlobalVisibleRect(visible);
                final String detail = window + " / "
                        + activity.getResources().getResourceEntryName(id)
                        + " intrinsic=" + action.getWidth() + "x" + action.getHeight()
                        + " visible=" + (onScreen
                                ? visible.width() + "x" + visible.height() : "scrolled out");
                assertTrue("intrinsic width " + detail, action.getWidth() >= floor);
                assertTrue("intrinsic height " + detail, action.getHeight() >= floor);
            }
            return null;
        });
    }

    private static Context fontScaled(Context context, float scale) {
        final Configuration configuration =
                new Configuration(context.getResources().getConfiguration());
        configuration.fontScale = scale;
        return context.createConfigurationContext(configuration);
    }

    private void goLandscape() {
        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        waitForLayout(rule.getScenario(), true);
        settleLayout();
    }

    private void selectTransform() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            selectConstructionTool(workspace, R.id.tool_rail_place);
            return null;
        });
        settleLayout();
    }

    private void enterSculpt() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] sculpt = new double[NativeViewport.SCULPT_STATE_SIZE];
            NativeViewport.sculptState(sculpt);
            final int action = sculpt[NativeViewport.SCULPT_HAS_MESH] != 0.0
                    ? R.id.resume_sculpt : R.id.freeze_to_sculpt;
            final View control = workspace.findViewById(action);
            assertNotNull(control);
            control.performClick();
            workspace.syncFromNative();
            assertEquals(NativeViewport.MODE_SCULPT, NativeViewport.productMode());
            return null;
        });
        settleLayout();
    }

    private void openExact() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            openPrecision(workspace);
            return null;
        });
        settleLayout();
    }

    private void closeExact() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            closePrecision(workspace);
            return null;
        });
        settleLayout();
    }

    private void click(final int id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View view = workspace.findViewById(id);
            assertNotNull("semantic control " + id, view);
            view.performClick();
            return null;
        });
        settleLayout();
    }

    private void dispatchIme(final boolean visible) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return;
        }
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int bottom = visible ? Math.max(1, workspace.getHeight() / 3) : 0;
            final WindowInsets current = workspace.getRootWindowInsets();
            final WindowInsets.Builder builder = current == null
                    ? new WindowInsets.Builder() : new WindowInsets.Builder(current);
            workspace.dispatchApplyWindowInsets(builder
                    .setInsets(WindowInsets.Type.ime(), Insets.of(0, 0, 0, bottom)).build());
            return null;
        });
        settleLayout();
    }

    private void clearIme() {
        dispatchIme(false);
    }

    private Rect hostRect() {
        return viewRect(R.id.workspace_trailing_host);
    }

    private Rect viewRect(final int id) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View view = workspace.findViewById(id);
            assertNotNull("semantic view " + id, view);
            final Rect bounds = new Rect(0, 0, view.getWidth(), view.getHeight());
            workspace.offsetDescendantRectToMyCoords(view, bounds);
            return bounds;
        });
    }

    /** The host's distance from the trailing window edge, in pixels. */
    private int trailingInset(final Rect host) {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.getWidth() - host.right);
    }

    private static void assertSameExternalFrame(String label, Rect expected, Rect actual) {
        assertEquals(label + " top", expected.top, actual.top);
        assertEquals(label + " right", expected.right, actual.right);
        assertEquals(label + " width", expected.width(), actual.width());
    }
}
