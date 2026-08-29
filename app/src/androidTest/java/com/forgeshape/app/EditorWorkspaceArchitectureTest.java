package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.openPrecision;
import static com.forgeshape.app.WorkspaceTestSupport.precisionToggle;
import static com.forgeshape.app.WorkspaceTestSupport.releaseOrientation;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.selectConstructionTool;
import static com.forgeshape.app.WorkspaceTestSupport.setOrientation;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static com.forgeshape.app.WorkspaceTestSupport.toolRail;
import static com.forgeshape.app.WorkspaceTestSupport.trailingHost;
import static com.forgeshape.app.WorkspaceTestSupport.transformModeGroup;
import static com.forgeshape.app.WorkspaceTestSupport.transformSpaceGroup;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.pm.ActivityInfo;
import android.graphics.Insets;
import android.graphics.Rect;
import android.os.Build;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.view.WindowInsets;
import android.widget.LinearLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.lang.reflect.Field;
import java.lang.reflect.Method;

/** Focused ownership and parity contract for the bounded trailing-host refactor. */
@RunWith(AndroidJUnit4.class)
public final class EditorWorkspaceArchitectureTest {

    @Rule
    public final ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    @Before
    public void reset() {
        resetToBaselineConstruction(rule.getScenario());
    }

    @After
    public void restoreWindow() {
        releaseOrientation(rule.getScenario());
    }

    /** UIAR1-01. One host owns every expected semantic child. */
    @Test
    public void uiar101_hostOwnsExpectedSemanticChildren() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View host = trailingHost(workspace);
            for (int id : new int[]{R.id.tool_rail_scroll, R.id.tool_rail,
                    R.id.precision_group, R.id.precision_toggle,
                    R.id.transform_selector_row, R.id.transform_mode_group,
                    R.id.transform_space_group}) {
                final View child = workspace.findViewById(id);
                assertNotNull(child);
                assertTrue("semantic child " + id + " belongs to the trailing host",
                        isDescendantOf(child, host));
            }
            assertEquals("the workspace row contains one trailing ownership boundary",
                    workspace.findViewById(R.id.workspace_trailing_host), host);
            return null;
        });
    }

    /** UIAR1-02. Root exposes neither trailing fields nor positional test accessors. */
    @Test
    public void uiar102_rootNoLongerExposesRightClusterImplementation() {
        int hostFields = 0;
        for (Field field : EditorWorkspaceView.class.getDeclaredFields()) {
            if (field.getType() == WorkspaceTrailingHostView.class) {
                hostFields++;
            }
            final String name = field.getName();
            assertFalse("right-cluster leaf leaked back into root: " + name,
                    name.equals("toolRail") || name.equals("toolRailScroll")
                            || name.equals("precisionToggle") || name.equals("railColumn")
                            || name.startsWith("transformMode")
                            || name.startsWith("transformSpace"));
        }
        assertEquals("root owns exactly one trailing host reference", 1, hostFields);
        for (Method method : EditorWorkspaceView.class.getDeclaredMethods()) {
            final String name = method.getName();
            assertFalse("root-only positional accessor returned: " + name,
                    name.equals("railColumn") || name.equals("toolRailScroll")
                            || name.equals("precisionToggle")
                            || name.equals("transformModeGroup")
                            || name.equals("transformSpaceGroup"));
        }
    }

    /** UIAR1-03. Move, Rotate and Scale still render native read-back exactly. */
    @Test
    public void uiar103_transformModeParity() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            selectConstructionTool(workspace, R.id.tool_rail_place);
            assertMode(workspace, R.id.transform_mode_move,
                    NativeViewport.GIZMO_MODE_MOVE, true);
            assertMode(workspace, R.id.transform_mode_rotate,
                    NativeViewport.GIZMO_MODE_ROTATE, true);
            assertMode(workspace, R.id.transform_mode_scale,
                    NativeViewport.GIZMO_MODE_SCALE, false);
            return null;
        });
    }

    /** UIAR1-04. World/Local remains a semantic request followed by native read-back. */
    @Test
    public void uiar104_coordinateSpaceParity() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            selectConstructionTool(workspace, R.id.tool_rail_place);
            workspace.findViewById(R.id.transform_mode_move).performClick();
            workspace.findViewById(R.id.transform_space_local).performClick();
            assertSpace(workspace, NativeViewport.GIZMO_SPACE_LOCAL);
            workspace.findViewById(R.id.transform_space_world).performClick();
            assertSpace(workspace, NativeViewport.GIZMO_SPACE_WORLD);
            return null;
        });
    }

    /** UIAR1-05. Exact remains presentation state and opens through its semantic trigger. */
    @Test
    public void uiar105_exactTriggerParity() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertFalse(workspace.propertyInspector().isOpen());
            precisionToggle(workspace).performClick();
            assertTrue(workspace.propertyInspector().isOpen());
            assertTrue(precisionToggle(workspace).isActivated());
            precisionToggle(workspace).performClick();
            assertFalse(workspace.propertyInspector().isOpen());
            assertFalse(precisionToggle(workspace).isActivated());
            return null;
        });
    }

    /** UIAR1-06. Display suppresses and restores the host without moving its anchor. */
    @Test
    public void uiar106_displaySuppressionParity() {
        final Rect before = boundsOfHost();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.display_settings_button).performClick();
            assertEquals(View.GONE, trailingHost(workspace).getVisibility());
            workspace.findViewById(R.id.display_settings_button).performClick();
            return null;
        });
        settleLayout();
        assertEquals(before, boundsOfHost());
    }

    /** UIAR1-07. A short window keeps one vertical host and full hit targets. */
    @Test
    public void uiar107_compactShortParity() {
        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        WorkspaceTestSupport.waitForLayout(rule.getScenario(), true);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            selectConstructionTool(workspace, R.id.tool_rail_place);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(LinearLayout.VERTICAL,
                    transformModeGroup(workspace).getOrientation());
            assertTouchFloor(activity, workspace.findViewById(R.id.precision_toggle));
            assertTouchFloor(activity, workspace.findViewById(R.id.transform_mode_move));
            assertTopRightContract(activity, workspace);
            return null;
        });
    }

    /** UIAR1-08. IME input keeps the host vertical and fixed externally. */
    @Test
    public void uiar108_imeParity() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return;
        }
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            selectConstructionTool(workspace, R.id.tool_rail_place);
            openPrecision(workspace);
            final Rect before = rectInWorkspace(workspace, trailingHost(workspace));
            final int inset = Math.max(1, workspace.getHeight() / 3);
            final WindowInsets current = workspace.getRootWindowInsets();
            final WindowInsets.Builder withIme = current == null
                    ? new WindowInsets.Builder() : new WindowInsets.Builder(current);
            workspace.dispatchApplyWindowInsets(withIme
                    .setInsets(WindowInsets.Type.ime(), Insets.of(0, 0, 0, inset)).build());
            assertEquals(LinearLayout.VERTICAL,
                    transformModeGroup(workspace).getOrientation());
            final Rect after = rectInWorkspace(workspace, trailingHost(workspace));
            assertEquals(before.top, after.top);
            assertEquals(before.right, after.right);
            assertEquals(before.width(), after.width());
            assertTouchFloor(activity, workspace.findViewById(R.id.precision_toggle));
            final WindowInsets afterIme = workspace.getRootWindowInsets();
            final WindowInsets.Builder withoutIme = afterIme == null
                    ? new WindowInsets.Builder() : new WindowInsets.Builder(afterIme);
            workspace.dispatchApplyWindowInsets(withoutIme
                    .setInsets(WindowInsets.Type.ime(), Insets.NONE).build());
            return null;
        });
    }

    /** UIAR1-09. The same host contract applies on the expanded/tablet branch. */
    @Test
    public void uiar109_expandedTabletParity() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTopRightContract(activity, workspace);
            if (workspace.layoutMode() == WorkspaceLayoutMode.EXPANDED) {
                assertEquals(View.VISIBLE, trailingHost(workspace).getVisibility());
                assertTrue(trailingHost(workspace).getElevation() > 0.0f);
                assertEquals(View.VISIBLE, workspace.findViewById(R.id.objects_dock).getVisibility());
            }
            assertEquals(WorkspaceLayoutMode.EXPANDED,
                    WorkspaceLayoutMode.forWindow(1066, 1706));
            return null;
        });
    }

    /** UIAR1-10. Sculpt uses the same host with four semantic brush entries. */
    @Test
    public void uiar110_sculptRailParity() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View start = workspace.findViewById(R.id.freeze_to_sculpt);
            final View resume = workspace.findViewById(R.id.resume_sculpt);
            if (start != null && start.getVisibility() == View.VISIBLE) {
                start.performClick();
            } else {
                assertNotNull(resume);
                resume.performClick();
            }
            workspace.syncFromNative();
            assertEquals(NativeViewport.MODE_SCULPT, NativeViewport.productMode());
            for (int id : new int[]{R.id.tool_rail_grab, R.id.tool_rail_clay,
                    R.id.tool_rail_smooth, R.id.tool_rail_inflate}) {
                final View entry = workspace.findViewById(id);
                assertNotNull(entry);
                assertTrue(isDescendantOf(entry, trailingHost(workspace)));
            }
            assertEquals(4, toolRail(workspace).getChildCount());
            assertEquals(View.GONE, transformModeGroup(workspace).getVisibility());
            return null;
        });
    }

    /** UIAR1-11. The host stores no native/domain authority. */
    @Test
    public void uiar111_noDuplicateStateAuthority() {
        for (Field field : WorkspaceTrailingHostView.class.getDeclaredFields()) {
            assertFalse("host must not own EditorUiState",
                    field.getType() == EditorUiState.class);
            assertFalse("host must not cache native numeric state",
                    field.getType() == double[].class || field.getType() == float[].class);
        }
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.tool_rail_place).performClick();
            assertEquals(EditorUiState.CONSTRUCTION_TOOL_TRANSFORM,
                    workspace.uiState().constructionTool());
            assertEquals(Integer.valueOf(EditorUiState.CONSTRUCTION_TOOL_TRANSFORM),
                    toolRail(workspace).activeKey());
            return null;
        });
    }

    /** UIAR1-12. Re-rendering one unchanged snapshot preserves bounds and visibility. */
    @Test
    public void uiar112_geometryVisibilitySnapshotIsStable() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            selectConstructionTool(workspace, R.id.tool_rail_place);
            workspace.findViewById(R.id.transform_mode_rotate).performClick();
            return null;
        });
        settleLayout();
        final int[] ids = {R.id.workspace_trailing_host, R.id.tool_rail,
                R.id.precision_toggle, R.id.transform_mode_group,
                R.id.transform_space_group};
        final Rect[] before = boundsOf(ids);
        final int[] visibility = visibilityOf(ids);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.syncFromNative();
            return null;
        });
        settleLayout();
        final Rect[] after = boundsOf(ids);
        final int[] afterVisibility = visibilityOf(ids);
        for (int i = 0; i < ids.length; i++) {
            assertEquals("bounds for semantic id " + ids[i], before[i], after[i]);
            assertEquals("visibility for semantic id " + ids[i],
                    visibility[i], afterVisibility[i]);
        }
    }

    private void assertMode(EditorWorkspaceView workspace, int actionId, int expected,
                            boolean expectSpace) {
        workspace.findViewById(actionId).performClick();
        final double[] state = new double[NativeViewport.GIZMO_STATE_SIZE];
        NativeViewport.gizmoState(state);
        assertEquals(expected, (int) state[NativeViewport.GIZMO_MODE]);
        assertTrue(workspace.findViewById(actionId).isActivated());
        assertEquals(expectSpace ? View.VISIBLE : View.GONE,
                transformSpaceGroup(workspace).getVisibility());
    }

    private void assertSpace(EditorWorkspaceView workspace, int expected) {
        final double[] state = new double[NativeViewport.GIZMO_STATE_SIZE];
        NativeViewport.gizmoState(state);
        assertEquals(expected, (int) state[NativeViewport.GIZMO_SPACE]);
        final int id = expected == NativeViewport.GIZMO_SPACE_WORLD
                ? R.id.transform_space_world : R.id.transform_space_local;
        assertTrue(workspace.findViewById(id).isActivated());
    }

    private void assertTouchFloor(ForgeShapeActivity activity, View view) {
        final int floor = EditorControlStyles.dimen(activity, R.dimen.control_height);
        assertTrue(view.getWidth() >= floor);
        assertTrue(view.getHeight() >= floor);
    }

    private void assertTopRightContract(ForgeShapeActivity activity,
                                        EditorWorkspaceView workspace) {
        final View host = trailingHost(workspace);
        final ViewGroup.LayoutParams raw = host.getLayoutParams();
        assertTrue(raw instanceof LinearLayout.LayoutParams);
        final LinearLayout.LayoutParams params = (LinearLayout.LayoutParams) raw;
        assertEquals(EditorControlStyles.dimen(activity, R.dimen.row_gap), params.topMargin);
        assertEquals(EditorControlStyles.dimen(activity, R.dimen.brush_gap), params.rightMargin);
        assertEquals(android.view.Gravity.TOP, params.gravity);
    }

    private Rect boundsOfHost() {
        return WorkspaceTestSupport.onWorkspace(rule.getScenario(),
                (activity, workspace) -> rectInWorkspace(workspace, trailingHost(workspace)));
    }

    private Rect[] boundsOf(final int[] ids) {
        return WorkspaceTestSupport.onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final Rect[] result = new Rect[ids.length];
            for (int i = 0; i < ids.length; i++) {
                result[i] = rectInWorkspace(workspace, workspace.findViewById(ids[i]));
            }
            return result;
        });
    }

    private int[] visibilityOf(final int[] ids) {
        return WorkspaceTestSupport.onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int[] result = new int[ids.length];
            for (int i = 0; i < ids.length; i++) {
                result[i] = workspace.findViewById(ids[i]).getVisibility();
            }
            return result;
        });
    }

    private static Rect rectInWorkspace(EditorWorkspaceView workspace, View view) {
        final Rect rect = new Rect(0, 0, view.getWidth(), view.getHeight());
        workspace.offsetDescendantRectToMyCoords(view, rect);
        return rect;
    }

    private static boolean isDescendantOf(View child, View ancestor) {
        for (View current = child; current != null; ) {
            if (current == ancestor) {
                return true;
            }
            final ViewParent parent = current.getParent();
            current = parent instanceof View ? (View) parent : null;
        }
        return false;
    }
}
