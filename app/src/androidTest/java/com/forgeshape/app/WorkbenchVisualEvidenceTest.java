package com.forgeshape.app;

import static com.forgeshape.app.SketchTestSupport.dragSketch;
import static com.forgeshape.app.SketchTestSupport.selectTool;
import static com.forgeshape.app.SketchTestSupport.sketchEntityCount;
import static com.forgeshape.app.SketchTestSupport.sketchState;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.graphics.Bitmap;
import android.os.ParcelFileDescriptor;
import android.os.SystemClock;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.AfterClass;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

/**
 * The workbench as it LOOKS (`WORKBENCH-UX-R1`): one walk through a CAD part's
 * life in a real tablet window (2560 x 1600 at 240 dpi, the 1706 x 1066 dp
 * landscape of a large tablet), a capture at each state, written to
 * {@code files/evidence/workbench-ux/} for `CI DEVICE` to collect. It is the
 * before/after record the look is iterated against; each capture is preceded by
 * presented frames so the model is in it, and the few assertions only make sure
 * the walk reached the state the capture is named for.
 *
 * <p>Emulator captures close no physical-device gate: the owner's Galaxy Tab S9
 * Ultra, its colour and its density stay UNVERIFIED.
 */
@RunWith(AndroidJUnit4.class)
public final class WorkbenchVisualEvidenceTest {

    private static final int CAPTURE_PRESENTED_FRAMES = 3;
    private static final long CAPTURE_FRAME_TIMEOUT_MS = 4000L;

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

    @Test
    public void the_workbench_through_a_part() {
        final File out = new File(InstrumentationRegistry.getInstrumentation().getTargetContext()
                .getExternalFilesDir(null), "evidence/workbench-ux");
        //noinspection ResultOfMethodCallIgnored
        out.mkdirs();
        final File[] stale = out.listFiles();
        if (stale != null) {
            for (File file : stale) {
                //noinspection ResultOfMethodCallIgnored
                file.delete();
            }
        }
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.sketchCancel();
            NativeViewport.supportChooserCancel();
            NativeViewport.enterConstructionMode();
            NativeViewport.closeProject();
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        capture(out, "01-home");

        resetToBaselineConstruction(rule.getScenario());
        final int converted = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int s = NativeViewport.sceneConvertToCadPart(NativeViewport.sceneActiveBodyId());
            workspace.onNativeStateChanged();
            return s;
        });
        assertEquals(NativeViewport.CAD_OK, converted);
        capture(out, "02-workbench-part");

        // A sketch on XY: the sketch's own chrome over the workbench.
        final int began = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int s = NativeViewport.sketchBegin(NativeViewport.WORKPLANE_XY);
            workspace.onNativeStateChanged();
            return s;
        });
        assertEquals(NativeViewport.CAD_OK, began);
        capture(out, "03-sketch-empty");
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), 1.4, 0.2, 2.6, 1.2);
        assertEquals(1, sketchEntityCount());
        capture(out, "04-sketch-rectangle");
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.finish_sketch).performClick();
            return null;
        });
        settleLayout();
        assertEquals(NativeViewport.SKETCH_READY, sketchState());
        capture(out, "05-sketch-ready");
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.sketchCancel();
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();

        // The Display popover open over the workbench.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.display_settings_button).performClick();
            return null;
        });
        capture(out, "06-display-popover");
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.display_settings_button).performClick();
            return null;
        });
        settleLayout();
        // The Top view from the view bar.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.view_top).performClick();
            return null;
        });
        capture(out, "07-top-view");
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.view_home).performClick();
            workspace.findViewById(R.id.browser_panel_toggle).performClick();
            workspace.findViewById(R.id.precision_toggle).performClick();
            return null;
        });
        capture(out, "08-panels-collapsed");
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.browser_panel_toggle).performClick();
            workspace.findViewById(R.id.precision_toggle).performClick();
            return null;
        });
        settleLayout();
    }

    private void capture(File out, String name) {
        settleLayout();
        final long start = NativeViewport.debugRendererFramesPresented();
        final long began = SystemClock.uptimeMillis();
        while (SystemClock.uptimeMillis() - began < CAPTURE_FRAME_TIMEOUT_MS
                && NativeViewport.debugRendererFramesPresented() - start < CAPTURE_PRESENTED_FRAMES) {
            SystemClock.sleep(30);
        }
        settleLayout();
        final Bitmap frame =
                InstrumentationRegistry.getInstrumentation().getUiAutomation().takeScreenshot();
        assertNotNull("the display could be captured", frame);
        final File png = new File(out, name + ".png");
        try (FileOutputStream stream = new FileOutputStream(png)) {
            assertTrue(frame.compress(Bitmap.CompressFormat.PNG, 100, stream));
        } catch (IOException error) {
            throw new AssertionError("could not write " + png, error);
        }
    }

    private static void shell(String command) {
        final ParcelFileDescriptor fd = InstrumentationRegistry.getInstrumentation()
                .getUiAutomation().executeShellCommand(command);
        try (InputStream in = new ParcelFileDescriptor.AutoCloseInputStream(fd)) {
            final byte[] buffer = new byte[1024];
            //noinspection StatementWithEmptyBody
            while (in.read(buffer) >= 0) {
            }
        } catch (IOException ignored) {
            // A shell that printed nothing is still a shell that ran.
        }
    }
}
