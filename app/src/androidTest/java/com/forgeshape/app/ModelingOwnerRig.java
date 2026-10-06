package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.graphics.Bitmap;
import android.os.SystemClock;
import android.util.Log;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.test.core.app.ActivityScenario;
import androidx.test.platform.app.InstrumentationRegistry;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;
import java.util.function.BiFunction;
import java.util.function.IntSupplier;

/**
 * The device plumbing the three {@code MODELING-FOUNDATIONS-R1} owner classes
 * share: start at Home, press chrome, touch through the WINDOW (decor view,
 * never a direct call into the viewport's listener), read the status line, and
 * keep evidence — facts and screenshots — under {@code files/evidence/<slug>}.
 *
 * <p>Test infrastructure only; it decides nothing about the product. Every
 * assertion a class makes is about native truth or about what is on screen.
 */
final class ModelingOwnerRig {

    private static final String TAG = "ForgeShape";

    private final ActivityScenario<ForgeShapeActivity> scenario;
    private final String token;
    private final File outDir;
    private final List<String> facts = new ArrayList<>();

    ModelingOwnerRig(ActivityScenario<ForgeShapeActivity> scenario, String slug, String token) {
        this.scenario = scenario;
        this.token = token;
        outDir = new File(InstrumentationRegistry.getInstrumentation().getTargetContext()
                .getExternalFilesDir(null), "evidence/" + slug);
        //noinspection ResultOfMethodCallIgnored
        outDir.mkdirs();
    }

    ActivityScenario<ForgeShapeActivity> scenario() {
        return scenario;
    }

    /** Home, as a first launch shows it: no project, no session, no chooser. */
    void startAtHome() {
        doOnWorkspace(scenario, (activity, workspace) -> {
            workspace.dismissRecoveryPromptForTest();
            NativeViewport.supportChooserCancel();
            NativeViewport.sketchCancel();
            NativeViewport.enterConstructionMode();
            workspace.showHomeAsFirstLaunchForTest();
            return null;
        });
        settleLayout();
    }

    /** Leaves a project behind for the next class, whatever this one did. */
    void restore() {
        writeFacts();
        doOnWorkspace(scenario, (activity, workspace) -> {
            NativeViewport.supportChooserCancel();
            NativeViewport.sketchCancel();
            NativeViewport.enterConstructionMode();
            return null;
        });
        resetToBaselineConstruction(scenario);
    }

    // -----------------------------------------------------------------------
    // Chrome
    // -----------------------------------------------------------------------

    <T> T on(BiFunction<ForgeShapeActivity, EditorWorkspaceView, T> action) {
        return onWorkspace(scenario, action::apply);
    }

    /** A control by id, asserted on screen, clicked. */
    void press(final int id) {
        doOnWorkspace(scenario, (activity, workspace) -> {
            final View control = workspace.findViewById(id);
            assertNotNull("control " + id + " must exist", control);
            assertTrue("control " + id + " must be on screen", control.isShown());
            control.performClick();
            return null;
        });
        settleLayout();
    }

    boolean shown(final int id) {
        return onWorkspace(scenario, (activity, workspace) -> {
            final View control = workspace.findViewById(id);
            return control != null && control.isShown();
        });
    }

    /**
     * A REAL tap on a view: Down and Up at its centre, dispatched to the window
     * root so the event takes the path a finger's does — whatever stands over
     * the point receives it.
     */
    void touchView(final BiFunction<ForgeShapeActivity, EditorWorkspaceView, View> finder) {
        // A control inside a scrolling or growing surface is brought on screen
        // first, the way a user scrolls to it and waits for a surface to finish
        // opening, so the touch lands on the control and not on whatever is
        // under the place it will be. Bounded: a control still not under its
        // own centre after that is occluded, and the failure says by what.
        String why = "";
        for (int attempt = 0; attempt < 6; attempt++) {
            doOnWorkspace(scenario, (activity, workspace) -> {
                final View target = finder.apply(activity, workspace);
                if (target != null) {
                    // The middle band first, so a control taller than a
                    // squeezed scroll window still shows its middle.
                    final int mid = target.getHeight() / 2;
                    target.requestRectangleOnScreen(new android.graphics.Rect(0, Math.max(0, mid - 8),
                            target.getWidth(), Math.min(target.getHeight(), mid + 8)), true);
                }
                return null;
            });
            settleLayout();
            why = onWorkspace(scenario, (activity, workspace) -> {
                final View target = finder.apply(activity, workspace);
                if (target == null || !target.isShown()) {
                    return "absent";
                }
                final float[] point = visibleCentre(activity, target);
                if (point == null) {
                    return "clipped out of view " + describeChain(target, target);
                }
                final View hit = clickableAt(activity.getWindow().getDecorView(), point[0], point[1]);
                return hit == target || isDescendant(hit, target) ? "" : describeChain(target, hit);
            });
            if (why.isEmpty()) {
                break;
            }
            SystemClock.sleep(200);
        }
        assertTrue("the touch lands on the control: " + why, why.isEmpty());
        doOnWorkspace(scenario, (activity, workspace) -> {
            final View target = finder.apply(activity, workspace);
            assertNotNull("the view to touch exists", target);
            assertTrue("the view to touch is on screen", target.isShown());
            final View root = activity.getWindow().getDecorView();
            final int[] rp = new int[2];
            root.getLocationInWindow(rp);
            final float[] point = visibleCentre(activity, target);
            assertNotNull("the control shows some of itself", point);
            final float x = point[0] - rp[0];
            final float y = point[1] - rp[1];
            final long down = SystemClock.uptimeMillis();
            dispatch(root, down, down, MotionEvent.ACTION_DOWN, x, y);
            dispatch(root, down, down + 60L, MotionEvent.ACTION_UP, x, y);
            return null;
        });
        settleLayout();
    }

    /**
     * The centre of the part of a control that is actually visible, in window
     * coordinates -- where a finger lands on a control a scroll window shows
     * only some of. Null when none of it is visible.
     */
    private static float[] visibleCentre(ForgeShapeActivity activity, View target) {
        final android.graphics.Rect visible = new android.graphics.Rect();
        if (!target.getGlobalVisibleRect(visible) || visible.isEmpty()) {
            return null;
        }
        final int[] screen = new int[2];
        activity.getWindow().getDecorView().getLocationInWindow(screen);
        // getGlobalVisibleRect is in the root view's coordinates, which are the
        // window's for an activity's decor view.
        return new float[]{visible.exactCenterX() + screen[0], visible.exactCenterY() + screen[1]};
    }

    /** Where the target and each ancestor stand, for an occlusion failure. */
    private static String describeChain(View target, View hit) {
        final StringBuilder out = new StringBuilder("hit=").append(hit).append(" chain:");
        View at = target;
        while (at != null) {
            final int[] p = new int[2];
            at.getLocationInWindow(p);
            out.append(" [").append(at.getClass().getSimpleName()).append(' ')
                    .append(p[0]).append(',').append(p[1]).append(' ')
                    .append(at.getWidth()).append('x').append(at.getHeight())
                    .append(" sy=").append(at.getScrollY())
                    .append(" a=").append(at.getAlpha()).append(" s=").append(at.getScaleY()).append(']');
            at = at.getParent() instanceof View ? (View) at.getParent() : null;
        }
        return out.toString();
    }

    void touchId(final int id) {
        touchView((activity, workspace) -> workspace.findViewById(id));
    }

    /** One finger through the window over the viewport: Down, Moves, Up. */
    void realGesture(final float[][] path) {
        doOnWorkspace(scenario, (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final View root = activity.getWindow().getDecorView();
            final int[] vp = new int[2];
            final int[] rp = new int[2];
            viewport.getLocationInWindow(vp);
            root.getLocationInWindow(rp);
            final float ox = vp[0] - rp[0];
            final float oy = vp[1] - rp[1];
            final long down = SystemClock.uptimeMillis();
            dispatch(root, down, down, MotionEvent.ACTION_DOWN, path[0][0] + ox, path[0][1] + oy);
            for (int i = 1; i < path.length; i++) {
                dispatch(root, down, down + 16L * i, MotionEvent.ACTION_MOVE, path[i][0] + ox,
                        path[i][1] + oy);
            }
            final float[] last = path[path.length - 1];
            dispatch(root, down, down + 16L * path.length + 24L, MotionEvent.ACTION_UP,
                    last[0] + ox, last[1] + oy);
            return null;
        });
        settleLayout();
    }

    /** A straight drag in viewport pixels, through the window, in {@code steps} moves. */
    void realDrag(float x0, float y0, float x1, float y1, int steps) {
        final float[][] path = new float[steps + 1][];
        for (int i = 0; i <= steps; i++) {
            final float t = i / (float) steps;
            path[i] = new float[]{x0 + (x1 - x0) * t, y0 + (y1 - y0) * t};
        }
        realGesture(path);
    }

    private static void dispatch(View root, long downTime, long eventTime, int action, float x,
                                 float y) {
        final MotionEvent event = MotionEvent.obtain(downTime, eventTime, action, x, y, 0);
        event.setSource(InputDevice.SOURCE_TOUCHSCREEN);
        root.dispatchTouchEvent(event);
        event.recycle();
    }

    /** Whether a viewport pixel is on the viewport and under no clickable chrome. */
    boolean viewportPointFree(final float x, final float y) {
        return onWorkspace(scenario, (activity, workspace) -> {
            final float density = activity.getResources().getDisplayMetrics().density;
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            if (x < 8 * density || y < 8 * density || x > viewport.getWidth() - 8 * density
                    || y > viewport.getHeight() - 8 * density) {
                return false;
            }
            final int[] vp = new int[2];
            viewport.getLocationInWindow(vp);
            final View owner = clickableAt(activity.getWindow().getDecorView(), x + vp[0], y + vp[1]);
            return owner == null || owner == viewport;
        });
    }

    private static View clickableAt(View view, float wx, float wy) {
        if (view.getVisibility() != View.VISIBLE) {
            return null;
        }
        final int[] at = new int[2];
        view.getLocationInWindow(at);
        if (wx < at[0] || wy < at[1] || wx >= at[0] + view.getWidth()
                || wy >= at[1] + view.getHeight()) {
            return null;
        }
        if (view instanceof ViewGroup) {
            final ViewGroup group = (ViewGroup) view;
            for (int i = group.getChildCount() - 1; i >= 0; i--) {
                final View hit = clickableAt(group.getChildAt(i), wx, wy);
                if (hit != null) {
                    return hit;
                }
            }
        }
        return view.isClickable() || view.getId() == R.id.viewport_surface ? view : null;
    }

    private static boolean isDescendant(View view, View ancestor) {
        View at = view;
        while (at != null) {
            if (at == ancestor) {
                return true;
            }
            at = at.getParent() instanceof View ? (View) at.getParent() : null;
        }
        return false;
    }

    static void collectById(View view, int id, List<View> out) {
        if (view.getId() == id) {
            out.add(view);
        }
        if (view instanceof ViewGroup) {
            final ViewGroup group = (ViewGroup) view;
            for (int i = 0; i < group.getChildCount(); i++) {
                collectById(group.getChildAt(i), id, out);
            }
        }
    }

    /** A native act on the UI thread followed by the workspace's ordinary re-read. */
    int applyOnUi(IntSupplier apply) {
        final int status = onWorkspace(scenario, (activity, workspace) -> {
            final int why = apply.getAsInt();
            workspace.onNativeStateChanged();
            return why;
        });
        settleLayout();
        return status;
    }

    void refresh() {
        doOnWorkspace(scenario, (activity, workspace) -> {
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    String statusLine() {
        return onWorkspace(scenario, (activity, workspace) -> {
            final View message = workspace.findViewById(R.id.status_message);
            return message instanceof TextView && message.isShown()
                    ? ((TextView) message).getText().toString() : "";
        });
    }

    // -----------------------------------------------------------------------
    // Evidence
    // -----------------------------------------------------------------------

    /** Saves the screen after the renderer presented a few new frames; evidence only. */
    void capture(String name) {
        final long start = NativeViewport.debugRendererFramesPresented();
        final long began = SystemClock.uptimeMillis();
        long seen = 0;
        while (SystemClock.uptimeMillis() - began < 15000L && seen < 6) {
            final long now = NativeViewport.debugRendererFramesPresented();
            seen = now >= start ? now - start : now;
            SystemClock.sleep(50);
        }
        final Bitmap frame = InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .takeScreenshot();
        if (frame == null) {
            fact("capture." + name, "unavailable");
            return;
        }
        final File png = new File(outDir, name + ".png");
        try (FileOutputStream out = new FileOutputStream(png)) {
            frame.compress(Bitmap.CompressFormat.PNG, 100, out);
            fact("capture." + name, png.getName() + " frames=" + seen);
        } catch (IOException error) {
            fact("capture." + name, "write_failed");
        }
    }

    void fact(String key, Object value) {
        final String line = key + "=" + value;
        facts.add(line);
        Log.i(TAG, token + " " + line);
    }

    private void writeFacts() {
        final File file = new File(outDir, "facts-" + System.nanoTime() + ".txt");
        try (PrintWriter out = new PrintWriter(file, "UTF-8")) {
            for (String line : facts) {
                out.println(line);
            }
        } catch (IOException error) {
            Log.w(TAG, token + " facts not written: " + error);
        }
        facts.clear();
    }
}
