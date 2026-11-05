package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.graphics.Bitmap;
import android.os.ParcelFileDescriptor;
import android.os.SystemClock;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

/**
 * `WORKBENCH-RIBBON-R2`, OWNER report 2026-11-05: "after changing the option
 * the desktop view does not start on Android". The Layout preference is
 * switched through the workspace's own handler in three REAL windows — the
 * emulator's phone, an 800 dp medium window and a 1706 dp tablet in
 * landscape — and what each choice draws is asserted from the view tree and
 * captured (`files/evidence/layout-switch/`).
 *
 * <p>Workbench must give the ribbon, the view cube and the view bar on every
 * window; the docked panels stay window arithmetic (Properties pinned where
 * the inspector docks at the side, the Browser only where its column fits).
 * Phone must take all of it away again, on a tablet too.
 */
@RunWith(AndroidJUnit4.class)
public final class WorkbenchLayoutSwitchTest {

    private static final File OUT = new File(InstrumentationRegistry.getInstrumentation()
            .getTargetContext().getExternalFilesDir(null), "evidence/layout-switch");

    @BeforeClass
    public static void anEmptyEvidenceDirectory() {
        //noinspection ResultOfMethodCallIgnored
        OUT.mkdirs();
        final File[] stale = OUT.listFiles();
        if (stale != null) {
            for (File file : stale) {
                //noinspection ResultOfMethodCallIgnored
                file.delete();
            }
        }
    }

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    @After
    public void automaticAndThePhoneWindowAgain() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onLayoutChosen(WorkspaceLayoutPreference.AUTOMATIC);
            return null;
        });
        shell("wm size reset");
        shell("wm density reset");
        SystemClock.sleep(1500);
    }

    @Test
    public void on_a_phone_workbench_brings_the_ribbon_and_phone_takes_it_away() {
        openProject();
        choose(WorkspaceLayoutPreference.AUTOMATIC);
        expect("phone, automatic", false, false, false);
        capture("phone-1-automatic");

        choose(WorkspaceLayoutPreference.WORKBENCH);
        expect("phone, workbench", true, false, false);
        capture("phone-2-workbench");

        choose(WorkspaceLayoutPreference.PHONE);
        expect("phone, phone", false, false, false);
        capture("phone-3-phone");
    }

    @Test
    public void in_a_medium_window_workbench_docks_the_properties() {
        window("1600x2560", 320);   // 800 x 1280 dp: MEDIUM by the arithmetic
        openProject();
        choose(WorkspaceLayoutPreference.AUTOMATIC);
        expect("medium, automatic", false, false, false);
        capture("medium-1-automatic");

        choose(WorkspaceLayoutPreference.WORKBENCH);
        expect("medium, workbench", true, true, false);
        capture("medium-2-workbench");
    }

    @Test
    public void on_a_tablet_phone_gives_the_small_screen_composition_and_back() {
        window("2560x1600", 240);   // 1706 x 1066 dp: a large tablet in landscape
        openProject();
        choose(WorkspaceLayoutPreference.AUTOMATIC);
        expect("tablet, automatic", true, true, true);
        capture("tablet-1-automatic");

        choose(WorkspaceLayoutPreference.PHONE);
        expect("tablet, phone", false, false, false);
        assertEquals("the phone composition", WorkspaceLayoutMode.COMPACT,
                onWorkspace(rule.getScenario(), (a, w) -> w.layoutMode()));
        capture("tablet-2-phone");

        choose(WorkspaceLayoutPreference.WORKBENCH);
        expect("tablet, workbench", true, true, true);
        capture("tablet-3-workbench");
    }

    // -----------------------------------------------------------------------

    private void openProject() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.sketchCancel();
            NativeViewport.supportChooserCancel();
            NativeViewport.enterConstructionMode();
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
        settleLayout();
    }

    private void choose(WorkspaceLayoutPreference layout) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onLayoutChosen(layout);
            return null;
        });
        settleLayout();
        SystemClock.sleep(400);
        settleLayout();
    }

    private void expect(String where, boolean chrome, boolean pinned, boolean browser) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(where + ": the ribbon", chrome ? View.VISIBLE : View.GONE,
                    workspace.ribbon().getVisibility());
            assertEquals(where + ": the view cube", chrome ? View.VISIBLE : View.GONE,
                    workspace.viewCube().getVisibility());
            assertEquals(where + ": the view bar", chrome ? View.VISIBLE : View.GONE,
                    workspace.viewBar().getVisibility());
            assertEquals(where + ": the pinned Properties", pinned, workspace.propertiesPinned());
            if (pinned) {
                assertTrue(where + ": and open", workspace.propertyInspector().isOpen());
            }
            assertEquals(where + ": the docked Browser", browser, workspace.objectsDocked());
            if (chrome) {
                assertTrue(where + ": a ribbon command is on screen",
                        activity.findViewById(R.id.ribbon_new_sketch).isShown());
            } else {
                assertFalse(where + ": no ribbon command is on screen",
                        activity.findViewById(R.id.ribbon_new_sketch).isShown());
            }
            return null;
        });
    }

    private static void window(String size, int density) {
        shell("wm size " + size);
        shell("wm density " + density);
        SystemClock.sleep(2000);
        settleLayout();
    }

    private void capture(String name) {
        settleLayout();
        final long start = NativeViewport.debugRendererFramesPresented();
        final long began = SystemClock.uptimeMillis();
        while (SystemClock.uptimeMillis() - began < 4000L
                && NativeViewport.debugRendererFramesPresented() - start < 3) {
            SystemClock.sleep(30);
        }
        settleLayout();
        final Bitmap frame =
                InstrumentationRegistry.getInstrumentation().getUiAutomation().takeScreenshot();
        assertNotNull("the display could be captured", frame);
        final File png = new File(OUT, name + ".png");
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
