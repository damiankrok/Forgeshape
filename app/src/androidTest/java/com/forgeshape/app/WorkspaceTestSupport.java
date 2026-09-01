package com.forgeshape.app;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.graphics.Rect;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.test.core.app.ActivityScenario;
import androidx.test.platform.app.InstrumentationRegistry;

import java.util.concurrent.atomic.AtomicReference;

/**
 * Shared machinery for the Editor Workspace instrumentation.
 *
 * <p>Two rules shape everything here. Nothing is located by screen coordinate —
 * every control is named by its stable semantic id, which is the whole reason
 * those ids exist. And nothing asserts a rendered pixel: the Vulkan viewport's
 * appearance is the renderer's business and is verified by the native suites
 * and by runtime evidence, not from Java.
 */
final class WorkspaceTestSupport {

    private WorkspaceTestSupport() {
    }

    /** The Construction state every test starts from, so one test's Apply
     *  cannot decide the next test's expectations. */
    static final double BASELINE_WIDTH_METERS = 2.0;
    static final double BASELINE_HEIGHT_METERS = 1.0;
    static final double BASELINE_DEPTH_METERS = 0.5;

    /** The viewpoint every test starts from — the same one the product opens at
     *  (kInitialYaw / kInitialPitch / kInitialDistance in forgeshape_camera.h),
     *  so what a case can see and grab is the same on every run and in every
     *  order. */
    static final float BASELINE_CAMERA_YAW = 0.7f;
    static final float BASELINE_CAMERA_PITCH = 0.5f;
    static final float BASELINE_CAMERA_DISTANCE = 8.2f;

    interface WorkspaceAction<T> {
        T run(ForgeShapeActivity activity, EditorWorkspaceView workspace);
    }

    /** Runs a block on the UI thread and hands back what it returned. */
    static <T> T onWorkspace(ActivityScenario<ForgeShapeActivity> scenario,
                             final WorkspaceAction<T> action) {
        final AtomicReference<T> result = new AtomicReference<>();
        scenario.onActivity(new ActivityScenario.ActivityAction<ForgeShapeActivity>() {
            @Override
            public void perform(ForgeShapeActivity activity) {
                final EditorWorkspaceView workspace = activity.editorWorkspace();
                assertNotNull("the workspace must exist before any assertion", workspace);
                result.set(action.run(activity, workspace));
            }
        });
        settle();
        return result.get();
    }

    /** Runs a block on the UI thread for its effect. */
    static void doOnWorkspace(ActivityScenario<ForgeShapeActivity> scenario,
                              final WorkspaceAction<Void> action) {
        onWorkspace(scenario, action);
    }

    /**
     * Returns the workspace to a known Construction state.
     *
     * <p>It cannot un-freeze: native code has no such operation and inventing
     * one purely for a test would put a destructive path into the product.
     * Tests that care about whether a Frozen Sculpt Mesh exists read that state
     * rather than assuming it.
     */
    static void resetToBaselineConstruction(ActivityScenario<ForgeShapeActivity> scenario) {
        doOnWorkspace(scenario, new WorkspaceAction<Void>() {
            @Override
            public Void run(ForgeShapeActivity activity, EditorWorkspaceView workspace) {
                // The start question is asked once per process and stands over
                // everything else, so every case that is not ABOUT it answers
                // it first and then asserts against the ordinary workspace.
                // Cases that are about it call showStartChooserAsFirstLaunch().
                workspace.dismissStartChooserForConstruction();
                // The recovery question stands over everything else in exactly
                // the same way, and for a sharper reason: leaving the foreground
                // checkpoints the project, so almost every case leaves a
                // candidate behind, and the first Activity of the next process
                // would put that question over the chrome this case is trying to
                // measure. Cases that are ABOUT recovery ask for it deliberately.
                workspace.dismissRecoveryPromptForTest();
                NativeViewport.enterConstructionMode();
                // The baseline is a CONSTRUCTION shape, so the body it is
                // applied to must have a Construction Source. Since
                // `IMPORT-01A` a scene can hold Imported Meshes, which refuse a
                // shape Apply by design — and a reset that silently did nothing
                // would hand the next case a baseline it never established.
                // Selecting the first body that has one costs a scan of a
                // handful of ids and makes the reset mean what it says.
                selectFirstConstructionBody();
                NativeViewport.applyConstructionBox(BASELINE_WIDTH_METERS,
                        BASELINE_HEIGHT_METERS, BASELINE_DEPTH_METERS);
                NativeViewport.applyBoxTransform(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
                // The camera is process-scoped too, and where it stands decides
                // which handles are reachable and which are edge-on. Without
                // this, a case that orbited or dollied would silently change
                // what the NEXT case can grab — an order dependency that reads
                // as a flaky solver rather than as inherited state.
                NativeViewport.debugSetCameraPose(BASELINE_CAMERA_YAW, BASELINE_CAMERA_PITCH,
                        BASELINE_CAMERA_DISTANCE);
                NativeViewport.setSculptTool(NativeViewport.TOOL_GRAB);
                workspace.setChromeHidden(false);
                workspace.uiState().setDisplayUnit(LengthUnit.METERS);
                workspace.uiState().setConstructionTool(
                        EditorUiState.CONSTRUCTION_TOOL_SHAPE);
                // The resting workspace has no precision surface in it, in
                // EITHER mode — set for both, not only the one this reset
                // happens to leave the product in, so a Sculpt case cannot
                // inherit a panel a Construction case opened. A case that needs
                // the exact values opens them the way a user does; see
                // openPrecision.
                workspace.uiState().setPrecisionOpen(false, false);
                workspace.uiState().setPrecisionOpen(true, false);
                // The history is process-scoped, exactly like the scene, so
                // without this a case would inherit whatever steps the previous
                // one recorded and "exactly one step" would mean nothing. This
                // is the debug seam, not a product act: there is no New Project
                // in the shipped UI to clear it with.
                NativeViewport.debugResetConstructionHistory();
                closeObjectsPanel(workspace);
                closeAddPrimitive(workspace);
                workspace.syncFromNative();
                return null;
            }
        });
    }

    /**
     * Makes the first body that has a Construction Source the active one.
     *
     * <p>Does nothing when there is none, which leaves the caller's Apply to
     * fail visibly rather than being silently skipped here.
     */
    private static void selectFirstConstructionBody() {
        final long[] ids = new long[NativeViewport.sceneBodyCount()];
        final int written = NativeViewport.sceneBodyIds(ids);
        for (int i = 0; i < written; i++) {
            if (!NativeViewport.sceneBodyIsImported(ids[i])) {
                NativeViewport.sceneSelectBody(ids[i]);
                return;
            }
        }
    }

    // -----------------------------------------------------------------------
    // Contextual surfaces
    //
    // Every one of these drives the control a user would press rather than
    // calling into the workspace, so a case that opens the exact values is
    // exercising the same path the product ships. None of them locates a
    // control by coordinate.
    // -----------------------------------------------------------------------

    /**
     * Holds the Construction context named by a Tool Rail entry id.
     *
     * <p>Drives the entry a user would press rather than writing the UI state,
     * so a case that needs Transform is exercising the same path the product
     * ships — including everything the workspace does as a consequence, which
     * for Transform is the gizmo appearing.
     */
    static void selectConstructionTool(EditorWorkspaceView workspace, int entryId) {
        workspace.findViewById(entryId).performClick();
    }

    /** Opens the precision surface from the Tool Rail's own toggle. */
    static void openPrecision(EditorWorkspaceView workspace) {
        if (!workspace.propertyInspector().isOpen()) {
            precisionToggle(workspace).performClick();
        }
    }

    /** Closes it from the same control. */
    static void closePrecision(EditorWorkspaceView workspace) {
        if (workspace.propertyInspector().isOpen()) {
            precisionToggle(workspace).performClick();
        }
    }

    // Right-cluster verification resolves semantic ids, never root-only fields
    // or child positions. The host may rearrange these controls without
    // changing how a test names their product meaning.

    static WorkspaceTrailingHostView trailingHost(EditorWorkspaceView workspace) {
        return requireView(workspace, R.id.workspace_trailing_host);
    }

    static ToolRailView toolRail(EditorWorkspaceView workspace) {
        return requireView(workspace, R.id.tool_rail);
    }

    static BoundedScrollView toolRailScroll(EditorWorkspaceView workspace) {
        return requireView(workspace, R.id.tool_rail_scroll);
    }

    static ImageView precisionToggle(EditorWorkspaceView workspace) {
        return requireView(workspace, R.id.precision_toggle);
    }

    static View precisionGroup(EditorWorkspaceView workspace) {
        return requireView(workspace, R.id.precision_group);
    }

    static LinearLayout transformModeGroup(EditorWorkspaceView workspace) {
        return requireView(workspace, R.id.transform_mode_group);
    }

    static LinearLayout transformSpaceGroup(EditorWorkspaceView workspace) {
        return requireView(workspace, R.id.transform_space_group);
    }

    static View transformSelectorRow(EditorWorkspaceView workspace) {
        return requireView(workspace, R.id.transform_selector_row);
    }

    static ImageView transformMoveAction(EditorWorkspaceView workspace) {
        return requireView(workspace, R.id.transform_mode_move);
    }

    static ImageView transformRotateAction(EditorWorkspaceView workspace) {
        return requireView(workspace, R.id.transform_mode_rotate);
    }

    static ImageView transformScaleAction(EditorWorkspaceView workspace) {
        return requireView(workspace, R.id.transform_mode_scale);
    }

    static ImageView transformSpaceWorldAction(EditorWorkspaceView workspace) {
        return requireView(workspace, R.id.transform_space_world);
    }

    static ImageView transformSpaceLocalAction(EditorWorkspaceView workspace) {
        return requireView(workspace, R.id.transform_space_local);
    }

    @SuppressWarnings("unchecked")
    private static <T extends View> T requireView(EditorWorkspaceView workspace, int id) {
        final View view = workspace.findViewById(id);
        assertNotNull("semantic workspace view " + id + " must exist", view);
        return (T) view;
    }

    /**
     * Opens the scene list from the Objects capsule.
     *
     * <p>Does nothing in a window whose Objects section already has a column:
     * there the list is permanently on screen and there is no panel to open.
     */
    static void openObjectsPanel(EditorWorkspaceView workspace) {
        if (!workspace.objectsDocked() && !workspace.objectsPopover().isOpen()) {
            workspace.objectsCapsule().findViewById(R.id.objects_capsule_active)
                    .performClick();
        }
    }

    static void closeObjectsPanel(EditorWorkspaceView workspace) {
        if (workspace.objectsPopover().isOpen()) {
            workspace.objectsCapsule().findViewById(R.id.objects_capsule_active)
                    .performClick();
        }
    }

    /** Opens Add Primitive from whichever host currently carries the plus. */
    static void openAddPrimitive(EditorWorkspaceView workspace) {
        if (workspace.addPrimitivePalette().isOpen()) {
            return;
        }
        if (workspace.objectsDocked()) {
            workspace.objectsSection().findViewById(R.id.add_body).performClick();
            return;
        }
        workspace.objectsCapsule().findViewById(R.id.objects_capsule_add).performClick();
    }

    static void closeAddPrimitive(EditorWorkspaceView workspace) {
        if (!workspace.addPrimitivePalette().isOpen()) {
            return;
        }
        if (workspace.objectsDocked()) {
            workspace.objectsSection().findViewById(R.id.add_body).performClick();
            return;
        }
        workspace.objectsCapsule().findViewById(R.id.objects_capsule_add).performClick();
    }

    /**
     * Drives a real Grab stroke across the viewport through the native touch
     * path.
     *
     * <p>Shared because more than one suite now needs a sculpt mesh that has
     * genuinely been edited, and a second copy of the gesture would be a second
     * definition of what "a stroke" means. The caller is responsible for being
     * in Sculpt with a frozen mesh and a brush large enough to capture vertices.
     *
     * <p>The coordinates are the one place the rules allow one: this is the
     * repository's existing touch harness injecting viewport input, and nothing
     * is inferred FROM the coordinate — what is asserted afterwards is the
     * revision and the vertex data native code reports.
     */
    static void sculptTheViewport(ActivityScenario<ForgeShapeActivity> scenario) {
        doOnWorkspace(scenario, new WorkspaceAction<Void>() {
            @Override
            public Void run(ForgeShapeActivity activity, EditorWorkspaceView workspace) {
                final View viewport = workspace.findViewById(R.id.viewport_surface);
                final int w = viewport.getWidth();
                final int h = viewport.getHeight();
                assertTrue("precondition: the viewport must be laid out", w > 0 && h > 0);
                final float cx = w / 2f;
                final float cy = h / 2f;
                sendViewportTouch(MotionEvent.ACTION_DOWN, cx, cy, w, h);
                for (int step = 1; step <= 8; ++step) {
                    sendViewportTouch(MotionEvent.ACTION_MOVE, cx + step * 6f, cy + step * 4f,
                            w, h);
                }
                sendViewportTouch(MotionEvent.ACTION_UP, cx + 48f, cy + 32f, w, h);
                return null;
            }
        });
        settleLayout();
    }

    private static void sendViewportTouch(int action, float x, float y, int width, int height) {
        // Null stylus arrays on purpose: this is a plain finger, and native code
        // fills in the documented defaults exactly as it does for hardware that
        // reports nothing.
        NativeViewport.touchEvent(action, -1, 1, new int[]{0}, new float[]{x},
                new float[]{y}, null, null, null, null, width, height);
    }

    static void settle() {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    /**
     * Waits for a change that alters layout, not just state.
     *
     * <p>Idle-sync alone returns as soon as the main looper drains, which can
     * be before the traversal that re-measures a view whose visibility just
     * changed. Anything that reads a measured bound needs the frame as well.
     */
    static void settleLayout() {
        settle();
        SystemClock.sleep(250);
        settle();
    }

    /** Waits until the workspace has laid out at a size the caller accepts. */
    static void waitForLayout(ActivityScenario<ForgeShapeActivity> scenario,
                              final boolean expectLandscape) {
        for (int attempt = 0; attempt < 60; attempt++) {
            final Boolean ready = onWorkspace(scenario, new WorkspaceAction<Boolean>() {
                @Override
                public Boolean run(ForgeShapeActivity activity, EditorWorkspaceView workspace) {
                    final boolean landscape = workspace.getWidth() > workspace.getHeight();
                    return workspace.getWidth() > 0 && landscape == expectLandscape;
                }
            });
            if (Boolean.TRUE.equals(ready)) {
                settle();
                return;
            }
            SystemClock.sleep(100);
        }
    }

    // -----------------------------------------------------------------------
    // Native state snapshots
    // -----------------------------------------------------------------------

    /**
     * Everything below JNI that a UI action must not disturb, in one array.
     *
     * <p>Compared bit for bit rather than approximately: "the display unit
     * changed nothing" is either exactly true or it is a defect.
     */
    static double[] nativeSnapshot() {
        final double[] primitive = new double[NativeViewport.PRIMITIVE_STATE_SIZE];
        final double[] transform = new double[NativeViewport.TRANSFORM_SIZE];
        final double[] sculpt = new double[NativeViewport.SCULPT_STATE_SIZE];
        NativeViewport.constructionPrimitive(primitive);
        NativeViewport.boxTransform(transform);
        NativeViewport.sculptState(sculpt);

        final double[] all = new double[primitive.length + transform.length + sculpt.length];
        System.arraycopy(primitive, 0, all, 0, primitive.length);
        System.arraycopy(transform, 0, all, primitive.length, transform.length);
        System.arraycopy(sculpt, 0, all, primitive.length + transform.length, sculpt.length);
        return all;
    }

    static String describeSnapshotDifference(double[] before, double[] after) {
        final StringBuilder message = new StringBuilder();
        for (int i = 0; i < before.length; i++) {
            if (Double.compare(before[i], after[i]) != 0) {
                message.append(" slot ").append(i).append(": ").append(before[i])
                        .append(" -> ").append(after[i]);
            }
        }
        return message.toString();
    }

    // -----------------------------------------------------------------------
    // Touch
    // -----------------------------------------------------------------------

    /**
     * Drags across a chrome surface and reports whether it consumed the whole
     * gesture.
     *
     * <p>Consumption is the guarantee that matters: the viewport is a sibling
     * <i>below</i> the chrome in the workspace's stack, and Android never
     * offers a consumed event to a sibling underneath. A surface that consumes
     * its own drag therefore cannot orbit the camera or deform the model.
     */
    static boolean dragConsumed(View target) {
        final float x = target.getWidth() * 0.5f;
        final float y = target.getHeight() * 0.5f;
        final long start = SystemClock.uptimeMillis();
        boolean consumed = send(target, start, start, MotionEvent.ACTION_DOWN, x, y);
        for (int step = 1; step <= 6; step++) {
            consumed &= send(target, start, start + step * 16L, MotionEvent.ACTION_MOVE,
                    x, y - step * 12.0f);
        }
        consumed &= send(target, start, start + 128L, MotionEvent.ACTION_UP, x, y - 72.0f);
        return consumed;
    }

    private static boolean send(View target, long downTime, long eventTime, int action,
                                float x, float y) {
        final MotionEvent event = MotionEvent.obtain(downTime, eventTime, action, x, y, 0);
        try {
            return target.dispatchTouchEvent(event);
        } finally {
            event.recycle();
        }
    }

    // -----------------------------------------------------------------------
    // Geometry
    // -----------------------------------------------------------------------

    /**
     * The fraction of the window through which the model can actually be seen.
     *
     * <p>The Vulkan surface is always the whole window, so what is left of the
     * viewport is the window minus the union of the chrome rectangles. The
     * union is computed exactly, by splitting the window on every chrome edge
     * and testing each resulting cell, rather than by summing areas — which
     * would double-count wherever two surfaces meet.
     */
    static double unoccludedViewportFraction(int windowWidth, int windowHeight, Rect[] chrome) {
        if (windowWidth <= 0 || windowHeight <= 0) {
            return 0.0;
        }
        final int[] xs = edges(windowWidth, chrome, true);
        final int[] ys = edges(windowHeight, chrome, false);
        long covered = 0L;
        for (int i = 0; i + 1 < xs.length; i++) {
            for (int j = 0; j + 1 < ys.length; j++) {
                final int cellWidth = xs[i + 1] - xs[i];
                final int cellHeight = ys[j + 1] - ys[j];
                if (cellWidth <= 0 || cellHeight <= 0) {
                    continue;
                }
                final int centerX = xs[i] + cellWidth / 2;
                final int centerY = ys[j] + cellHeight / 2;
                for (Rect rect : chrome) {
                    if (rect.contains(centerX, centerY)) {
                        covered += (long) cellWidth * cellHeight;
                        break;
                    }
                }
            }
        }
        final double windowArea = (double) windowWidth * windowHeight;
        return (windowArea - covered) / windowArea;
    }

    private static int[] edges(int limit, Rect[] chrome, boolean horizontal) {
        final java.util.TreeSet<Integer> values = new java.util.TreeSet<>();
        values.add(0);
        values.add(limit);
        for (Rect rect : chrome) {
            values.add(clamp(horizontal ? rect.left : rect.top, limit));
            values.add(clamp(horizontal ? rect.right : rect.bottom, limit));
        }
        final int[] result = new int[values.size()];
        int index = 0;
        for (Integer value : values) {
            result[index++] = value;
        }
        return result;
    }

    private static int clamp(int value, int limit) {
        return value < 0 ? 0 : Math.min(value, limit);
    }

    /** Whether a view is laid out entirely inside its window. */
    static boolean isFullyOnScreen(View view, View window) {
        if (view.getVisibility() != View.VISIBLE || view.getWidth() <= 0) {
            return false;
        }
        final int[] windowLocation = new int[2];
        final int[] viewLocation = new int[2];
        window.getLocationInWindow(windowLocation);
        view.getLocationInWindow(viewLocation);
        final int top = viewLocation[1] - windowLocation[1];
        final int left = viewLocation[0] - windowLocation[0];
        return left >= 0 && top >= 0
                && left + view.getWidth() <= window.getWidth()
                && top + view.getHeight() <= window.getHeight();
    }

    static void setOrientation(ActivityScenario<ForgeShapeActivity> scenario,
                               final int orientation) {
        scenario.onActivity(new ActivityScenario.ActivityAction<ForgeShapeActivity>() {
            @Override
            public void perform(ForgeShapeActivity activity) {
                activity.setRequestedOrientation(orientation);
            }
        });
    }

    static void releaseOrientation(ActivityScenario<ForgeShapeActivity> scenario) {
        setOrientation(scenario, android.content.pm.ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
    }
}
