package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.os.SystemClock;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.IOException;
import java.io.InputStream;

/**
 * `TABLET-WORKBENCH-R1` on the device, in a REAL tablet window: the emulator's
 * display is set to 2560 x 1600 at 240 dpi (a 1706 x 1066 dp landscape tablet)
 * for this class and reset afterwards, so every branch below runs under the
 * window the decision is about rather than a simulated one.
 *
 * <p>In a workbench the Browser (Objects, then the active CAD body's History)
 * and the Properties panel are docked at rest; each collapses with one control
 * and comes back with one control; another surface opening does not dismiss
 * the pinned Properties. A hardware keyboard's Ctrl+Z / Ctrl+Y and Esc and a
 * mouse's wheel and right-drag reach the one handler each act already has.
 *
 * <p>Asserted from native truth and the view tree. Emulator evidence closes no
 * physical-device gate: the owner's Galaxy Tab S9 Ultra, its keyboard cover
 * and a real mouse are UNVERIFIED until run there.
 */
@RunWith(AndroidJUnit4.class)
public final class TabletWorkbenchTest {

    @BeforeClass
    public static void aTabletWindow() {
        shell("wm size 2560x1600");
        shell("wm density 240");
        SystemClock.sleep(1500);
    }

    @AfterClass
    public static void backToThePhoneWindow() {
        shell("wm size reset");
        shell("wm density reset");
        SystemClock.sleep(1500);
    }

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    @Before
    public void aCadPartInAWorkbench() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.sketchCancel();
            NativeViewport.supportChooserCancel();
            NativeViewport.enterConstructionMode();
            NativeViewport.closeProject();
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
        final int status = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int s = NativeViewport.sceneConvertToCadPart(NativeViewport.sceneActiveBodyId());
            workspace.onNativeStateChanged();
            return s;
        });
        assertEquals("the baseline box becomes a CAD part", NativeViewport.CAD_OK, status);
        settleLayout();
        SystemClock.sleep(300);
        settleLayout();
        final boolean workbench = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int widthDp = EditorControlStyles.toDp(activity, workspace.getWidth());
            return workspace.layoutMode() == WorkspaceLayoutMode.EXPANDED
                    && workspace.layoutMode().workbench(widthDp);
        });
        assertTrue("the emulator window is a tablet workbench", workbench);
    }

    @Test
    public void a_workbench_docks_the_browser_and_the_properties_at_rest() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("the Browser column stands", workspace.objectsDocked());
            assertTrue("its collapse control is drawn",
                    activity.findViewById(R.id.browser_panel_toggle).isShown());
            assertTrue("the History is docked in the Browser", workspace.historyDocked());
            assertTrue("and drawn there for a CAD body",
                    activity.findViewById(R.id.feature_history).isShown());
            assertEquals("the History stands under the scene list",
                    workspace.browserContent(),
                    activity.findViewById(R.id.feature_history).getParent());
            assertFalse("the capsule draws no second door to it",
                    activity.findViewById(R.id.feature_history_action).isShown());
            assertTrue("the Properties panel is pinned", workspace.propertiesPinned());
            assertTrue("and open at rest", workspace.propertyInspector().isOpen());
            return null;
        });

        // Another surface opening does not dismiss the pinned panel.
        click(R.id.display_settings_button);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("the Display popover opened", workspace.displayPopover().isOpen());
            assertTrue("the pinned Properties stayed", workspace.propertyInspector().isOpen());
            // Esc closes the topmost surface, one step as Back is.
            assertTrue(activity.onKeyDown(KeyEvent.KEYCODE_ESCAPE,
                    new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ESCAPE)));
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertFalse("Esc closed the popover", workspace.displayPopover().isOpen());
            assertTrue("and left the pinned panel", workspace.propertyInspector().isOpen());
            return null;
        });
    }

    @Test
    public void each_panel_collapses_with_one_control_and_comes_back() {
        click(R.id.browser_panel_toggle);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertFalse("the Browser collapsed", workspace.objectsDocked());
            assertEquals("the Objects capsule is back", View.VISIBLE,
                    workspace.objectsCapsule().getVisibility());
            assertFalse("the History floats again", workspace.historyDocked());
            assertTrue("its capsule control is back",
                    activity.findViewById(R.id.feature_history_action).isShown());
            assertTrue("the collapse control stays to restore it",
                    activity.findViewById(R.id.browser_panel_toggle).isShown());
            return null;
        });
        click(R.id.browser_panel_toggle);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("the Browser is back", workspace.objectsDocked());
            assertTrue("with the History in it", workspace.historyDocked());
            return null;
        });

        click(R.id.precision_toggle);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertFalse("the Properties panel collapsed", workspace.propertyInspector().isOpen());
            assertTrue(workspace.propertiesCollapsedForTest());
            // A refresh does not undo the user's choice.
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertFalse("the collapse holds across a refresh", workspace.propertyInspector().isOpen());
            return null;
        });
        click(R.id.precision_toggle);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("the Properties panel is back", workspace.propertyInspector().isOpen());
            assertTrue(workspace.propertiesPinned());
            return null;
        });
    }

    @Test
    public void a_keyboard_and_a_mouse_reach_the_ones_handlers() {
        final long part = NativeViewport.sceneActiveBodyId();
        // Ctrl+Z takes the conversion back: the box returns by its own id.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(activity.onKeyDown(KeyEvent.KEYCODE_Z, ctrl(KeyEvent.KEYCODE_Z, false)));
            return null;
        });
        settleLayout();
        final long box = NativeViewport.sceneActiveBodyId();
        assertNotEquals("Ctrl+Z undid the conversion", part, box);
        assertEquals(NativeViewport.REPRESENTATION_CONSTRUCTION,
                NativeViewport.sceneBodyRepresentation(box));
        // Ctrl+Y redoes it, the same part by the same id.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(activity.onKeyDown(KeyEvent.KEYCODE_Y, ctrl(KeyEvent.KEYCODE_Y, false)));
            return null;
        });
        settleLayout();
        assertEquals("Ctrl+Y redid it", part, NativeViewport.sceneActiveBodyId());

        // The wheel zooms in about the pointer.
        final float[] pose = new float[3];
        NativeViewport.debugCameraPose(pose);
        final float distance = pose[NativeViewport.CAMERA_POSE_DISTANCE];
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final ForgeShapeSurfaceView viewport = workspace.findViewById(R.id.viewport_surface);
            final MotionEvent wheel = mouseEvent(MotionEvent.ACTION_SCROLL, viewport.getWidth() * 0.5f,
                    viewport.getHeight() * 0.5f, 0, 1.0f);
            assertTrue("the wheel is the viewport's", viewport.dispatchGenericMotionEvent(wheel));
            wheel.recycle();
            return null;
        });
        NativeViewport.debugCameraPose(pose);
        assertTrue("the wheel zoomed in: " + distance + " -> " + pose[NativeViewport.CAMERA_POSE_DISTANCE],
                pose[NativeViewport.CAMERA_POSE_DISTANCE] < distance);

        // A right-button drag orbits, and selects nothing.
        final float yaw = pose[NativeViewport.CAMERA_POSE_YAW];
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final ForgeShapeSurfaceView viewport = workspace.findViewById(R.id.viewport_surface);
            final float x = viewport.getWidth() * 0.5f;
            final float y = viewport.getHeight() * 0.5f;
            final int right = MotionEvent.BUTTON_SECONDARY;
            for (MotionEvent event : new MotionEvent[]{
                    mouseEvent(MotionEvent.ACTION_DOWN, x, y, right, 0.0f),
                    mouseEvent(MotionEvent.ACTION_MOVE, x + 120.0f, y, right, 0.0f),
                    mouseEvent(MotionEvent.ACTION_UP, x + 120.0f, y, 0, 0.0f)}) {
                viewport.dispatchTouchEvent(event);
                event.recycle();
            }
            return null;
        });
        NativeViewport.debugCameraPose(pose);
        assertNotEquals("the right-button drag orbited", yaw, pose[NativeViewport.CAMERA_POSE_YAW]);
        assertEquals("and changed no selection", part, NativeViewport.sceneActiveBodyId());
    }

    /**
     * WORKBENCH-RIBBON-R1: the view cube, the view bar and the History strip
     * stand on a workbench; each view act turns native's camera; a step in
     * the strip is its History row's own act; all three step aside while a
     * sketch owns the viewport.
     */
    /**
     * `WORKBENCH-UX-R1`: nothing stands on the docked Properties panel. The
     * sketch's orientation navigator and its actions column used to sit over
     * the panel's Height field and its Apply; and opening Display used to hide
     * the trailing host, so the panel slid into its place.
     */
    @Test
    public void nothing_stands_on_the_docked_properties_and_display_moves_nothing() {
        final int began = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int s = NativeViewport.sketchBegin(NativeViewport.WORKPLANE_XY);
            workspace.onNativeStateChanged();
            return s;
        });
        assertEquals(NativeViewport.CAD_OK, began);
        settleLayout();
        SystemClock.sleep(300);
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final android.graphics.Rect panel = windowRect(workspace.propertyInspector());
            assertTrue("the Properties panel stands in a sketch",
                    workspace.propertyInspector().isShown());
            final View navigator = workspace.sketchNavigator();
            assertTrue("the orientation navigator stands", navigator.isShown());
            assertFalse("and not on the docked Properties panel: " + windowRect(navigator)
                            + " vs " + panel,
                    android.graphics.Rect.intersects(windowRect(navigator), panel));
            final View modify = workspace.sketchModify();
            if (modify.isShown()) {
                assertFalse("nor does the sketch actions column: " + windowRect(modify),
                        android.graphics.Rect.intersects(windowRect(modify), panel));
            }
            NativeViewport.sketchCancel();
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        final int[] before = new int[2];
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.propertyInspector().getLocationInWindow(before);
            return null;
        });
        click(R.id.display_settings_button);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("Display opened", workspace.displayPopover().isOpen());
            final int[] after = new int[2];
            workspace.propertyInspector().getLocationInWindow(after);
            assertEquals("opening Display moved the docked panel nowhere", before[0], after[0]);
            assertTrue("the trailing tools stay", activity.findViewById(R.id.tool_rail_shape) == null
                    || activity.findViewById(R.id.tool_rail_shape).isShown());
            return null;
        });
        click(R.id.display_settings_button);
    }

    private static android.graphics.Rect windowRect(View view) {
        final int[] at = new int[2];
        view.getLocationInWindow(at);
        return new android.graphics.Rect(at[0], at[1], at[0] + view.getWidth(),
                at[1] + view.getHeight());
    }

    @Test
    public void the_view_cube_the_view_bar_and_the_history_strip() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("the view cube stands on the model",
                    activity.findViewById(R.id.view_cube).isShown());
            assertTrue("the view bar stands on the model",
                    activity.findViewById(R.id.view_bar).isShown());
            assertTrue("the History strip stands on the bottom edge",
                    workspace.historyStrip().isShown());
            assertTrue("it lists the part's steps",
                    workspace.historyStrip().stepsContainer().getChildCount() >= 2);
            return null;
        });
        final float[] axes = new float[9];
        // Top: the eye straight above, exactly.
        click(R.id.view_top);
        NativeViewport.cameraViewAxes(axes);
        assertTrue("Top looks straight down: " + axes[7], axes[7] > 0.9999f);
        // The cube's +Z end is the Front view.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final WorkbenchViewCubeView cube = workspace.viewCube();
            final float[] end = cube.endPoint(2, true);
            final long now = SystemClock.uptimeMillis();
            final MotionEvent down = MotionEvent.obtain(now, now, MotionEvent.ACTION_DOWN,
                    end[0], end[1], 0);
            final MotionEvent up = MotionEvent.obtain(now, now + 40, MotionEvent.ACTION_UP,
                    end[0], end[1], 0);
            cube.dispatchTouchEvent(down);
            cube.dispatchTouchEvent(up);
            down.recycle();
            up.recycle();
            return null;
        });
        settleLayout();
        NativeViewport.cameraViewAxes(axes);
        assertTrue("the cube's +Z end is Front: " + axes[8], axes[8] > 0.9999f);
        // Zoom far in, then Fit brings the whole part back.
        final float[] pose = new float[NativeViewport.CAMERA_POSE_SIZE];
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            NativeViewport.cameraPointerZoom(viewport.getWidth() * 0.5f,
                    viewport.getHeight() * 0.5f, 25.0f, viewport.getWidth(), viewport.getHeight());
            return null;
        });
        NativeViewport.debugCameraPose(pose);
        final float zoomed = pose[NativeViewport.CAMERA_POSE_DISTANCE];
        click(R.id.view_fit);
        NativeViewport.debugCameraPose(pose);
        assertTrue("Fit framed the part again: " + zoomed + " -> "
                        + pose[NativeViewport.CAMERA_POSE_DISTANCE],
                pose[NativeViewport.CAMERA_POSE_DISTANCE] > zoomed * 1.5f);
        click(R.id.view_home);
        // A step in the strip is its row's act: the sketch step opens Edit Sketch.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final android.widget.LinearLayout steps = workspace.historyStrip().stepsContainer();
            View sketchStep = null;
            for (int i = 0; i < steps.getChildCount(); i++) {
                final Object tag = steps.getChildAt(i).getTag();
                if (tag instanceof FeatureHistoryPresentation.Row
                        && ((FeatureHistoryPresentation.Row) tag).kind
                                == NativeViewport.TIMELINE_KIND_SKETCH
                        && steps.getChildAt(i).isClickable()) {
                    sketchStep = steps.getChildAt(i);
                    break;
                }
            }
            assertTrue("the strip carries an editable sketch step", sketchStep != null);
            sketchStep.performClick();
            return null;
        });
        settleLayout();
        final double[] sketch = new double[NativeViewport.SKETCH_STATE_SIZE];
        NativeViewport.sketchState(sketch);
        assertNotEquals("the step opened the sketch", 0.0, sketch[NativeViewport.SKETCH_STATE]);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertFalse("the cube steps aside for a sketch",
                    activity.findViewById(R.id.view_cube).isShown());
            assertFalse("and the bar", activity.findViewById(R.id.view_bar).isShown());
            assertFalse("and the strip", workspace.historyStrip().isShown());
            NativeViewport.sketchCancel();
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("they come back with the sketch cancelled",
                    activity.findViewById(R.id.view_cube).isShown()
                            && workspace.historyStrip().isShown());
            return null;
        });
    }

    // -----------------------------------------------------------------------

    private void click(int id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View view = activity.findViewById(id);
            assertTrue("control " + activity.getResources().getResourceEntryName(id)
                    + " is drawn", view != null && view.isShown());
            view.performClick();
            return null;
        });
        settleLayout();
    }

    private static KeyEvent ctrl(int keyCode, boolean shift) {
        final int meta = KeyEvent.META_CTRL_ON | KeyEvent.META_CTRL_LEFT_ON
                | (shift ? KeyEvent.META_SHIFT_ON : 0);
        final long now = SystemClock.uptimeMillis();
        return new KeyEvent(now, now, KeyEvent.ACTION_DOWN, keyCode, 0, meta);
    }

    private static MotionEvent mouseEvent(int action, float x, float y, int buttons, float vscroll) {
        final MotionEvent.PointerProperties[] properties = {new MotionEvent.PointerProperties()};
        properties[0].id = 0;
        properties[0].toolType = MotionEvent.TOOL_TYPE_MOUSE;
        final MotionEvent.PointerCoords[] coords = {new MotionEvent.PointerCoords()};
        coords[0].x = x;
        coords[0].y = y;
        coords[0].pressure = buttons != 0 ? 1.0f : 0.0f;
        coords[0].size = 1.0f;
        if (vscroll != 0.0f) {
            coords[0].setAxisValue(MotionEvent.AXIS_VSCROLL, vscroll);
        }
        final long now = SystemClock.uptimeMillis();
        return MotionEvent.obtain(now, now, action, 1, properties, coords, 0, buttons,
                1.0f, 1.0f, 0, 0, InputDevice.SOURCE_MOUSE, 0);
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
    }
}
