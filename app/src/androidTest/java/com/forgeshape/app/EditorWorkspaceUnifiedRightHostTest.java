package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.closePrecision;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.openPrecision;
import static com.forgeshape.app.WorkspaceTestSupport.releaseOrientation;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.selectConstructionTool;
import static com.forgeshape.app.WorkspaceTestSupport.setOrientation;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static com.forgeshape.app.WorkspaceTestSupport.trailingHost;
import static com.forgeshape.app.WorkspaceTestSupport.waitForLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.pm.ActivityInfo;
import android.graphics.Insets;
import android.graphics.Rect;
import android.os.Build;
import android.os.ParcelFileDescriptor;
import android.os.SystemClock;
import android.view.View;
import android.view.ViewParent;
import android.view.WindowInsets;
import android.widget.LinearLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.IOException;

/** UILR2-01..18: static geometry contract for the unified right context. */
@RunWith(AndroidJUnit4.class)
public final class EditorWorkspaceUnifiedRightHostTest {

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
        shell("settings put global animator_duration_scale 1");
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                workspace.dispatchApplyWindowInsets(insetsBuilder(workspace)
                        .setInsets(WindowInsets.Type.ime(), Insets.NONE).build());
                return null;
            });
        }
        releaseOrientation(rule.getScenario());
    }

    /** UILR2-01. Resting host has the repository-owned fixed top/right/width. */
    @Test
    public void uilr2_01_restingHostBaseline() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final Rect host = rect(workspace, trailingHost(workspace));
            assertEquals(EditorControlStyles.dimen(activity, R.dimen.trailing_host_width),
                    host.width());
            assertEquals(EditorControlStyles.dimen(activity, R.dimen.row_gap),
                    ((LinearLayout.LayoutParams) trailingHost(workspace).getLayoutParams())
                            .topMargin);
            assertEquals(EditorControlStyles.dimen(activity, R.dimen.brush_gap),
                    ((LinearLayout.LayoutParams) trailingHost(workspace).getLayoutParams())
                            .rightMargin);
            return null;
        });
    }

    /** UILR2-02. Transform changes only height and the bottom edge. */
    @Test
    public void uilr2_02_transformExpandsDownwardOnly() {
        final Rect shape = hostRect();
        selectTransform();
        final Rect transform = hostRect();
        assertSameExternalFrame(shape, transform);
        assertTrue("Transform adds context below the same top edge",
                transform.bottom > shape.bottom && transform.height() > shape.height());
    }

    /** UILR2-03. Move/Rotate/Scale preserve top, right and width. */
    @Test
    public void uilr2_03_transformModesKeepExternalFrame() {
        selectTransform();
        final Rect baseline = hostRect();
        for (int id : new int[]{R.id.transform_mode_move, R.id.transform_mode_rotate,
                R.id.transform_mode_scale}) {
            click(id);
            assertSameExternalFrame(baseline, hostRect());
        }
    }

    /** UILR2-04. World/Local preserve top, right and width. */
    @Test
    public void uilr2_04_coordinateSpacesKeepExternalFrame() {
        selectTransform();
        final Rect baseline = hostRect();
        click(R.id.transform_space_local);
        assertSameExternalFrame(baseline, hostRect());
        click(R.id.transform_space_world);
        assertSameExternalFrame(baseline, hostRect());
    }

    /** UILR2-05. Exact is a descendant of the one visible host surface. */
    @Test
    public void uilr2_05_exactTriggerBelongsToHost() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(isDescendant(workspace.findViewById(R.id.precision_toggle),
                    trailingHost(workspace)));
            assertTrue(isDescendant(workspace.findViewById(R.id.precision_group),
                    trailingHost(workspace)));
            assertTrue("only the host carries floating depth",
                    trailingHost(workspace).getElevation() > 0.0f);
            assertEquals(0.0f, workspace.findViewById(R.id.precision_group).getElevation(), 0.0f);
            return null;
        });
    }

    /** UILR2-06. Opening Exact cannot move or resize the host externally. */
    @Test
    public void uilr2_06_exactOpenKeepsExternalFrame() {
        selectTransform();
        final Rect baseline = hostRect();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            openPrecision(workspace);
            return null;
        });
        settleLayout();
        assertSameExternalFrame(baseline, hostRect());
    }

    /** UILR2-07. A bottom Exact surface hides bottom chrome; it never translates it. */
    @Test
    public void uilr2_07_exactOpenHidesRatherThanMovesBottomControls() {
        selectTransform();
        final Rect resting = bottomRect();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            openPrecision(workspace);
            if (workspace.inspectorPlacement()
                    == WorkspaceLayoutMode.InspectorPlacement.BOTTOM_SHEET) {
                assertEquals(View.GONE, workspace.bottomRow().getVisibility());
            } else {
                assertEquals(resting, rect(workspace, workspace.bottomRow()));
            }
            return null;
        });
    }

    /** UILR2-08. Closing Exact restores the exact resting bottom bounds. */
    @Test
    public void uilr2_08_exactCloseRestoresBottomBounds() {
        selectTransform();
        final Rect resting = bottomRect();
        openAndClosePrecision();
        assertEquals(resting, bottomRect());
    }

    /** UILR2-09. Entry/exit never exposes a translated intermediate bottom row. */
    @Test
    public void uilr2_09_sheetTransitionNeverTranslatesUnrelatedChrome() {
        shell("settings put global animator_duration_scale 10");
        selectTransform();
        final Rect resting = bottomRect();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            openPrecision(workspace);
            assertEquals(View.GONE, workspace.bottomRow().getVisibility());
            closePrecision(workspace);
            assertEquals("the row stays hidden for the whole sheet exit",
                    View.GONE, workspace.bottomRow().getVisibility());
            return null;
        });
        SystemClock.sleep(1700);
        settleLayout();
        assertEquals(resting, bottomRect());
    }

    /** UILR2-10. IME keeps the right host fixed and every selector vertical. */
    @Test
    public void uilr2_10_imeKeepsVerticalFixedHost() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return;
        }
        selectTransform();
        final Rect baseline = hostRect();
        dispatchIme(true);
        final Rect ime = hostRect();
        assertSameExternalFrame(baseline, ime);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(LinearLayout.VERTICAL,
                    ((LinearLayout) workspace.findViewById(R.id.transform_mode_group))
                            .getOrientation());
            assertEquals(LinearLayout.VERTICAL,
                    ((LinearLayout) workspace.findViewById(R.id.transform_space_group))
                            .getOrientation());
            assertEquals(LinearLayout.VERTICAL,
                    ((LinearLayout) workspace.findViewById(R.id.transform_selector_row))
                            .getOrientation());
            return null;
        });
    }

    /** UILR2-11. Every visible right-host action retains the 48 dp hit floor with IME. */
    @Test
    public void uilr2_11_imeVisibleTargetsKeepTouchFloor() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return;
        }
        selectTransform();
        dispatchIme(true);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int floor = EditorControlStyles.dimen(activity, R.dimen.control_height);
            for (int id : rightActionIds()) {
                final View action = workspace.findViewById(id);
                if (action != null && action.getVisibility() == View.VISIBLE) {
                    assertTrue(id + " width", action.getWidth() >= floor);
                    assertTrue(id + " height", action.getHeight() >= floor);
                }
            }
            return null;
        });
    }

    /** UILR2-12. Signed precision remains complete and Scale remains unitless. */
    @Test
    public void uilr2_12_longSignedAndUnitlessScaleRegression() {
        selectTransform();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final String signed = "-98765.43210987654321";
            workspace.placementEditor().rowFor(R.id.field_pos_x).setText(signed);
            assertEquals(signed, workspace.placementEditor().rowFor(R.id.field_pos_x).text());
            final View units = workspace.findViewById(R.id.unit_chips);
            assertTrue(isDescendant(units, workspace.placementEditor()));
            assertFalse("Scale's numeric row owns no length-unit control",
                    isDescendant(units,
                            workspace.placementEditor().rowFor(R.id.field_scale_x)));
            assertNotNull(workspace.findViewById(R.id.apply_transform));
            return null;
        });
    }

    /** UILR2-13. Shape uses the same host family and integrated Exact action. */
    @Test
    public void uilr2_13_shapeUsesSameHostFamily() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(View.GONE, workspace.findViewById(R.id.transform_selector_row)
                    .getVisibility());
            assertTrue(isDescendant(workspace.findViewById(R.id.tool_rail_shape),
                    trailingHost(workspace)));
            assertTrue(isDescendant(workspace.findViewById(R.id.precision_toggle),
                    trailingHost(workspace)));
            return null;
        });
    }

    /** UILR2-14. Sculpt preserves the Construction host's top/right/width. */
    @Test
    public void uilr2_14_sculptUsesSameExternalGeometry() {
        final Rect construction = hostRect();
        enterSculpt();
        assertSameExternalFrame(construction, hostRect());
    }

    /** UILR2-15. Sculpt Details never pushes Objects upward. */
    @Test
    public void uilr2_15_sculptDetailsDoesNotPushBottomChrome() {
        enterSculpt();
        final Rect resting = bottomRect();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            openPrecision(workspace);
            if (workspace.inspectorPlacement()
                    == WorkspaceLayoutMode.InspectorPlacement.BOTTOM_SHEET) {
                assertEquals(View.GONE, workspace.bottomRow().getVisibility());
            } else {
                assertEquals(resting, rect(workspace, workspace.bottomRow()));
            }
            return null;
        });
    }

    /** UILR2-16. Sculpt Details close restores the exact resting bounds. */
    @Test
    public void uilr2_16_sculptDetailsCloseRestoresBounds() {
        enterSculpt();
        final Rect resting = bottomRect();
        openAndClosePrecision();
        assertEquals(resting, bottomRect());
    }

    /** UILR2-17. Compact portrait and short landscape keep one vertical host. */
    @Test
    public void uilr2_17_compactPortraitAndShortLandscape() {
        selectTransform();
        assertVerticalHost();
        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        waitForLayout(rule.getScenario(), true);
        settleLayout();
        assertVerticalHost();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(EditorControlStyles.dimen(activity, R.dimen.trailing_host_width),
                    trailingHost(workspace).getWidth());
            return null;
        });
    }

    /** UILR2-18. Expanded arithmetic and Stage 020R2/R3 selector semantics remain intact. */
    @Test
    public void uilr2_18_expandedContractAndTransformSemantics() {
        assertEquals(WorkspaceLayoutMode.EXPANDED,
                WorkspaceLayoutMode.forWindow(1066, 1706));
        selectTransform();
        click(R.id.transform_mode_rotate);
        click(R.id.transform_space_local);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] gizmo = new double[NativeViewport.GIZMO_STATE_SIZE];
            NativeViewport.gizmoState(gizmo);
            assertEquals(NativeViewport.GIZMO_MODE_ROTATE,
                    (int) gizmo[NativeViewport.GIZMO_MODE]);
            assertEquals(NativeViewport.GIZMO_SPACE_LOCAL,
                    (int) gizmo[NativeViewport.GIZMO_SPACE]);
            clickInside(workspace, R.id.transform_mode_scale);
            NativeViewport.gizmoState(gizmo);
            assertEquals(NativeViewport.GIZMO_MODE_SCALE,
                    (int) gizmo[NativeViewport.GIZMO_MODE]);
            assertEquals(View.GONE, workspace.findViewById(R.id.transform_space_group)
                    .getVisibility());
            return null;
        });
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

    private void openAndClosePrecision() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            openPrecision(workspace);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            closePrecision(workspace);
            return null;
        });
        settleLayout();
    }

    private void click(final int id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            clickInside(workspace, id);
            return null;
        });
        settleLayout();
    }

    private static void clickInside(EditorWorkspaceView workspace, int id) {
        final View view = workspace.findViewById(id);
        assertNotNull(view);
        view.performClick();
    }

    private Rect hostRect() {
        return WorkspaceTestSupport.onWorkspace(rule.getScenario(),
                (activity, workspace) -> rect(workspace, trailingHost(workspace)));
    }

    private Rect bottomRect() {
        return WorkspaceTestSupport.onWorkspace(rule.getScenario(),
                (activity, workspace) -> rect(workspace, workspace.bottomRow()));
    }

    private void dispatchIme(final boolean visible) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int bottom = visible ? Math.max(1, workspace.getHeight() / 3) : 0;
            workspace.dispatchApplyWindowInsets(insetsBuilder(workspace)
                    .setInsets(WindowInsets.Type.ime(), Insets.of(0, 0, 0, bottom)).build());
            return null;
        });
        settleLayout();
    }

    private void assertVerticalHost() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(LinearLayout.VERTICAL,
                    ((LinearLayout) workspace.findViewById(R.id.transform_selector_row))
                            .getOrientation());
            assertEquals(LinearLayout.VERTICAL,
                    ((LinearLayout) workspace.findViewById(R.id.transform_mode_group))
                            .getOrientation());
            assertEquals(LinearLayout.VERTICAL,
                    ((LinearLayout) workspace.findViewById(R.id.transform_space_group))
                            .getOrientation());
            return null;
        });
    }

    private static Rect rect(EditorWorkspaceView workspace, View view) {
        final Rect result = new Rect(0, 0, view.getWidth(), view.getHeight());
        workspace.offsetDescendantRectToMyCoords(view, result);
        return result;
    }

    private static void assertSameExternalFrame(Rect expected, Rect actual) {
        assertEquals("top", expected.top, actual.top);
        assertEquals("right", expected.right, actual.right);
        assertEquals("width", expected.width(), actual.width());
    }

    private static boolean isDescendant(View child, View ancestor) {
        for (View current = child; current != null; ) {
            if (current == ancestor) {
                return true;
            }
            final ViewParent parent = current.getParent();
            current = parent instanceof View ? (View) parent : null;
        }
        return false;
    }

    private static int[] rightActionIds() {
        return new int[]{R.id.tool_rail_shape, R.id.tool_rail_place,
                R.id.transform_mode_move, R.id.transform_mode_rotate,
                R.id.transform_mode_scale, R.id.transform_space_world,
                R.id.transform_space_local, R.id.precision_toggle};
    }

    private static WindowInsets.Builder insetsBuilder(View workspace) {
        final WindowInsets current = workspace.getRootWindowInsets();
        return current == null ? new WindowInsets.Builder() : new WindowInsets.Builder(current);
    }

    private static void shell(String command) {
        try (ParcelFileDescriptor ignored = InstrumentationRegistry.getInstrumentation()
                .getUiAutomation().executeShellCommand(command)) {
            // Closing the descriptor is enough; the command has no output to consume.
        } catch (IOException error) {
            throw new AssertionError("shell command failed: " + command, error);
        }
        SystemClock.sleep(100);
    }
}
