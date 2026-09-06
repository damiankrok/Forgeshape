package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.selectConstructionTool;
import static com.forgeshape.app.WorkspaceTestSupport.setOrientation;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static com.forgeshape.app.WorkspaceTestSupport.waitForLayout;
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
import org.junit.runner.RunWith;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

/**
 * `E2E-UIPREFR1-VIS`: deterministic screenshot evidence for `UI-PREF-R1`,
 * captured through the real chrome on the authoritative emulator.
 *
 * <p>One journey, twenty captures, in the order Part N names them: the two
 * Settings doors and the page, the same Home and the same editor scene in all
 * five palettes, the right- and left-handed editor with a left-handed Exact,
 * and the gizmo at its default, smallest and largest size and its thinnest and
 * boldest stroke.
 *
 * <p>Every capture is written beside a line of <b>measurable facts</b> — the
 * preference in force, the rail's bounds, the palette's viewport index, the
 * gizmo's native values — so the frames are evidence of a state and not of a
 * look. Captured with {@code UiAutomation.takeScreenshot()}, which captures
 * the composed display, so the Vulkan viewport is in every frame. No owner
 * aesthetic approval is claimed by any of it.
 */
@RunWith(AndroidJUnit4.class)
public final class UiPrefVisualEvidenceTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private File outDir;
    private final List<String> facts = new ArrayList<>();
    private int captureIndex;

    @Before
    public void startFromTheDefaults() {
        outDir = new File(InstrumentationRegistry.getInstrumentation().getTargetContext()
                .getExternalFilesDir(null), "evidence/ui-pref-r1");
        //noinspection ResultOfMethodCallIgnored
        outDir.mkdirs();
        for (File stale : orEmpty(outDir.listFiles())) {
            //noinspection ResultOfMethodCallIgnored
            stale.delete();
        }
        restoreDefaults();
        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        waitForLayout(rule.getScenario(), false);
        resetToBaselineConstruction(rule.getScenario());
    }

    @After
    public void leaveTheDefaultsBehind() {
        writeFacts();
        closeSettings();
        restoreDefaults();
        resetToBaselineConstruction(rule.getScenario());
    }

    @Test
    public void e2eUiPrefR1Vis_theWholeJourneyInTwentyCaptures() {
        // --- Settings: the two doors and the page --------------------------
        run((activity, workspace) -> {
            workspace.dismissRecoveryPromptForTest();
            workspace.showHomeAsFirstLaunchForTest();
        });
        settleLayout();
        run((activity, workspace) -> {
            assertTrue(workspace.homeVisible());
            fact("phase", "home");
            preferenceFacts(activity, workspace);
            bounds(workspace, "home_settings", R.id.home_settings);
        });
        capture("01_home_settings_entry");

        press(R.id.home_settings);
        run((activity, workspace) -> {
            assertTrue(workspace.settingsVisible());
            fact("phase", "settings_from_home");
            preferenceFacts(activity, workspace);
            bounds(workspace, "settings_page", R.id.settings_page);
            bounds(workspace, "appearance_warm_graphite", R.id.appearance_warm_graphite);
            bounds(workspace, "handedness_left", R.id.handedness_left);
            bounds(workspace, "gizmo_size_largest", R.id.gizmo_size_largest);
            bounds(workspace, "gizmo_weight_bold", R.id.gizmo_weight_bold);
            bounds(workspace, "settings_back", R.id.settings_back);
        });
        capture("02_settings_page");
        closeSettings();
        resetToBaselineConstruction(rule.getScenario());
        run((activity, workspace) -> {
            workspace.findViewById(R.id.project_actions_button).performClick();
        });
        settleLayout();
        run((activity, workspace) -> {
            assertTrue(workspace.projectPopover().isOpen());
            fact("phase", "project_surface_settings_entry");
            bounds(workspace, "project_settings", R.id.project_settings);
        });
        capture("03_editor_settings_entry");
        run((activity, workspace) -> workspace.findViewById(R.id.project_settings).performClick());
        settleLayout();

        // --- Five palettes: the same Home and the same editor scene --------
        final AppTheme[] palettes = AppTheme.values();
        final String[] names = {"warm_graphite", "neutral_charcoal", "light_charcoal",
                "warm_light", "cool_light"};
        final int[] rows = {R.id.appearance_warm_graphite, R.id.appearance_neutral_charcoal,
                R.id.appearance_light_charcoal, R.id.appearance_warm_light,
                R.id.appearance_cool_light};
        for (int i = 0; i < palettes.length; i++) {
            final AppTheme palette = palettes[i];
            openSettings();
            // The first palette is the one already worn: a press on it repaints
            // the row and recreates nothing, so there is no new Activity to wait for.
            final boolean[] alreadyWorn = new boolean[1];
            run((activity, workspace) -> alreadyWorn[0] = workspace.appTheme() == palette);
            activityBeforePalette = identity();
            press(rows[i]);
            if (!alreadyWorn[0]) {
                waitForPalette(palette);
            }
            closeSettings();
            run((activity, workspace) -> {
                selectConstructionTool(workspace, R.id.tool_rail_place);
                fact("phase", "editor_" + palette.name());
                preferenceFacts(activity, workspace);
                fact("system_bars_light", activity.systemBarsLight());
                fact("window_luminance", String.format(java.util.Locale.US, "%.3f",
                        luminance(EditorControlStyles.themeColor(activity,
                                android.R.attr.windowBackground))));
            });
            capture(String.format(java.util.Locale.US, "%02d_editor_%s", 4 + 2 * i, names[i]));
            run((activity, workspace) -> {
                workspace.showHomeAsFirstLaunchForTest();
            });
            settleLayout();
            run((activity, workspace) -> {
                assertTrue(workspace.homeVisible());
                fact("phase", "home_" + palette.name());
                preferenceFacts(activity, workspace);
            });
            capture(String.format(java.util.Locale.US, "%02d_home_%s", 5 + 2 * i, names[i]));
            resetToBaselineConstruction(rule.getScenario());
        }
        openSettings();
        activityBeforePalette = identity();
        press(R.id.appearance_warm_graphite);
        waitForPalette(AppTheme.WARM_GRAPHITE);
        closeSettings();

        // --- Handedness -----------------------------------------------------
        run((activity, workspace) -> {
            selectConstructionTool(workspace, R.id.tool_rail_place);
            fact("phase", "right_handed_editor");
            preferenceFacts(activity, workspace);
            railFacts(workspace);
        });
        capture("14_right_handed_editor");
        openSettings();
        press(R.id.handedness_left);
        closeSettings();
        run((activity, workspace) -> {
            fact("phase", "left_handed_editor");
            preferenceFacts(activity, workspace);
            railFacts(workspace);
        });
        capture("15_left_handed_editor");
        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        waitForLayout(rule.getScenario(), true);
        run((activity, workspace) -> {
            WorkspaceTestSupport.openPrecision(workspace);
        });
        settleLayout();
        run((activity, workspace) -> {
            fact("phase", "left_handed_exact_open");
            preferenceFacts(activity, workspace);
            railFacts(workspace);
            bounds(workspace, "property_inspector", R.id.property_inspector);
            fact("exact_overlaps_rail", overlaps(workspace.findViewById(R.id.property_inspector),
                    workspace.findViewById(R.id.workspace_trailing_host)));
        });
        capture("16_left_handed_exact_open");
        run((activity, workspace) -> WorkspaceTestSupport.closePrecision(workspace));
        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        waitForLayout(rule.getScenario(), false);
        openSettings();
        press(R.id.handedness_right);
        closeSettings();

        // --- Gizmo ----------------------------------------------------------
        run((activity, workspace) -> {
            selectConstructionTool(workspace, R.id.tool_rail_place);
            fact("phase", "gizmo_default");
            preferenceFacts(activity, workspace);
            gizmoFacts();
        });
        capture("17_gizmo_default");
        openSettings();
        press(R.id.gizmo_size_small);
        press(R.id.gizmo_weight_thin);
        closeSettings();
        run((activity, workspace) -> {
            fact("phase", "gizmo_smallest_thin");
            preferenceFacts(activity, workspace);
            gizmoFacts();
        });
        capture("18_gizmo_smallest_thin");
        openSettings();
        press(R.id.gizmo_size_largest);
        press(R.id.gizmo_weight_bold);
        closeSettings();
        run((activity, workspace) -> {
            fact("phase", "gizmo_largest_bold");
            preferenceFacts(activity, workspace);
            gizmoFacts();
        });
        capture("19_gizmo_largest_bold");
        run((activity, workspace) -> {
            workspace.onTransformModeRequested(NativeViewport.GIZMO_MODE_ROTATE);
            fact("phase", "gizmo_largest_bold_rotate");
            preferenceFacts(activity, workspace);
            gizmoFacts();
        });
        capture("20_gizmo_largest_bold_rotate");
        run((activity, workspace) -> {
            workspace.onTransformModeRequested(NativeViewport.GIZMO_MODE_MOVE);
        });
    }

    // -----------------------------------------------------------------------
    // Facts
    // -----------------------------------------------------------------------

    private void preferenceFacts(ForgeShapeActivity activity, EditorWorkspaceView workspace) {
        final AppPreferences p = AppPreferencesStore.current(activity);
        fact("palette", p.palette().name());
        fact("handedness", p.handedness().name());
        fact("gizmo_visual_scale", p.gizmoVisualScale());
        fact("gizmo_stroke_weight", p.gizmoStrokeWeight().name());
        fact("viewport_background_index", NativeViewport.viewportBackground());
        fact("native_gizmo_visual_scale", NativeViewport.gizmoVisualScale());
        fact("native_gizmo_stroke_weight", NativeViewport.gizmoStrokeWeight());
        fact("project_open", NativeViewport.projectOpen());
        fact("body_count", NativeViewport.sceneBodyCount());
        fact("construction_undo_depth", NativeViewport.constructionUndoDepth());
        fact("left_handed", workspace.leftHanded());
    }

    private void railFacts(EditorWorkspaceView workspace) {
        bounds(workspace, "workspace_trailing_host", R.id.workspace_trailing_host);
        bounds(workspace, "brush_edge_controls", R.id.brush_edge_controls);
        bounds(workspace, "objects_capsule", R.id.objects_capsule);
        bounds(workspace, "history_group", R.id.history_group);
        final View host = workspace.findViewById(R.id.workspace_trailing_host);
        final View overlay = (View) workspace.findViewById(R.id.restore_ui_chip).getParent();
        final Rect frame = new Rect(0, 0, host.getWidth(), host.getHeight());
        workspace.offsetDescendantRectToMyCoords(host, frame);
        final float density = workspace.getResources().getDisplayMetrics().density;
        fact("rail_left_inset_dp", String.format(java.util.Locale.US, "%.1f",
                (frame.left - overlay.getPaddingLeft()) / density));
        fact("rail_right_inset_dp", String.format(java.util.Locale.US, "%.1f",
                (workspace.getWidth() - overlay.getPaddingRight() - frame.right) / density));
        fact("rail_width_dp", String.format(java.util.Locale.US, "%.1f", frame.width() / density));
        fact("rail_top_dp", String.format(java.util.Locale.US, "%.1f",
                (frame.top - overlay.getPaddingTop()) / density));
    }

    private void gizmoFacts() {
        final double[] gizmo = new double[NativeViewport.GIZMO_STATE_SIZE];
        NativeViewport.gizmoState(gizmo);
        fact("gizmo_visible", gizmo[NativeViewport.GIZMO_VISIBLE]);
        fact("gizmo_mode", (int) gizmo[NativeViewport.GIZMO_MODE]);
        final int[] handles = {NativeViewport.GIZMO_HANDLE_AXIS_X,
                NativeViewport.GIZMO_HANDLE_AXIS_Y, NativeViewport.GIZMO_HANDLE_AXIS_Z};
        final float[] pivot = new float[2];
        NativeViewport.gizmoHandlePoint(NativeViewport.GIZMO_HANDLE_NONE, pivot);
        fact("gizmo_pivot_px", pivot[0] + "," + pivot[1]);
        for (int handle : handles) {
            final float[] pixel = new float[2];
            if (NativeViewport.gizmoHandlePoint(handle, pixel)) {
                fact("handle_" + handle + "_px", pixel[0] + "," + pixel[1] + " hit="
                        + NativeViewport.gizmoHitTest(pixel[0], pixel[1]));
            }
        }
    }

    // -----------------------------------------------------------------------
    // Support
    // -----------------------------------------------------------------------

    private interface Effect {
        void run(ForgeShapeActivity activity, EditorWorkspaceView workspace);
    }

    private void run(final Effect effect) {
        WorkspaceTestSupport.doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            effect.run(activity, workspace);
            return null;
        });
    }

    private int identity() {
        final Integer id = WorkspaceTestSupport.onWorkspace(rule.getScenario(),
                (activity, workspace) -> System.identityHashCode(activity));
        return id == null ? 0 : id;
    }

    private void press(final int id) {
        run((activity, workspace) -> {
            final View control = workspace.findViewById(id);
            assertNotNull(workspace.getResources().getResourceEntryName(id), control);
            control.performClick();
        });
        settleLayout();
    }

    private void openSettings() {
        run((activity, workspace) -> {
            if (workspace.settingsVisible()) {
                return;
            }
            if (workspace.homeVisible()) {
                workspace.findViewById(R.id.home_settings).performClick();
                return;
            }
            workspace.findViewById(R.id.project_actions_button).performClick();
            workspace.findViewById(R.id.project_settings).performClick();
        });
        settleLayout();
    }

    private void closeSettings() {
        run((activity, workspace) -> {
            if (workspace.settingsVisible()) {
                workspace.findViewById(R.id.settings_back).performClick();
            }
        });
        settleLayout();
    }

    private void restoreDefaults() {
        final boolean[] wearingDefault = new boolean[1];
        run((activity, workspace) -> wearingDefault[0] =
                workspace.appTheme() == AppTheme.defaultTheme());
        if (!wearingDefault[0]) {
            activityBeforePalette = identity();
            run((activity, workspace) -> activity.requestTheme(AppTheme.defaultTheme()));
            waitForPalette(AppTheme.defaultTheme());
        }
        run((activity, workspace) -> {
            AppPreferencesStore.resetForVerification(activity);
            workspace.reapplyPreferencesForTest();
        });
        settleLayout();
    }

    private int activityBeforePalette;

    /** A new Activity instance, laid out, wearing the palette -- the store
     *  answers the new palette before the recreation has begun. */
    private void waitForPalette(final AppTheme theme) {
        for (int attempt = 0; attempt < 100; attempt++) {
            settleLayout();
            final Boolean ready = WorkspaceTestSupport.onWorkspace(rule.getScenario(),
                    (activity, workspace) -> workspace.appTheme() == theme
                            && workspace.getWidth() > 0
                            && System.identityHashCode(activity) != activityBeforePalette);
            if (Boolean.TRUE.equals(ready)) {
                return;
            }
            SystemClock.sleep(100);
        }
        throw new AssertionError("the workspace never came back wearing " + theme);
    }

    /** Takes one composed-display frame after a short settle. */
    private void capture(String name) {
        settleLayout();
        SystemClock.sleep(400);  // a few more frames, so the viewport has drawn the state
        final Bitmap frame =
                InstrumentationRegistry.getInstrumentation().getUiAutomation().takeScreenshot();
        assertNotNull("the display could be captured", frame);
        captureIndex++;
        final File png = new File(outDir, name + ".png");
        try (FileOutputStream out = new FileOutputStream(png)) {
            assertTrue(frame.compress(Bitmap.CompressFormat.PNG, 100, out));
        } catch (IOException error) {
            throw new AssertionError("could not write " + png, error);
        }
        facts.add("capture=" + name + ".png width=" + frame.getWidth() + " height="
                + frame.getHeight());
        facts.add("");
    }

    private void fact(String key, Object value) {
        facts.add("  " + key + "=" + value);
    }

    private void bounds(EditorWorkspaceView workspace, String label, int id) {
        final View view = workspace.findViewById(id);
        if (view == null || !view.isShown()) {
            facts.add("  " + label + "=absent");
            return;
        }
        final int[] at = new int[2];
        view.getLocationOnScreen(at);
        final Rect rect = new Rect(at[0], at[1], at[0] + view.getWidth(), at[1] + view.getHeight());
        facts.add("  " + label + "=" + rect.toShortString() + " width_px=" + view.getWidth()
                + " height_px=" + view.getHeight());
    }

    private static boolean overlaps(View a, View b) {
        if (a == null || b == null || !a.isShown() || !b.isShown()) {
            return false;
        }
        final int[] pa = new int[2];
        final int[] pb = new int[2];
        a.getLocationInWindow(pa);
        b.getLocationInWindow(pb);
        return pa[0] < pb[0] + b.getWidth() && pb[0] < pa[0] + a.getWidth()
                && pa[1] < pb[1] + b.getHeight() && pb[1] < pa[1] + a.getHeight();
    }

    private static double luminance(int color) {
        return 0.2126 * channel(android.graphics.Color.red(color))
                + 0.7152 * channel(android.graphics.Color.green(color))
                + 0.0722 * channel(android.graphics.Color.blue(color));
    }

    private static double channel(int value) {
        final double c = value / 255.0;
        return c <= 0.03928 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4);
    }

    private void writeFacts() {
        final File file = new File(outDir, "facts.txt");
        try (PrintWriter out = new PrintWriter(file, "UTF-8")) {
            out.println("captures=" + captureIndex);
            for (String line : facts) {
                out.println(line);
            }
        } catch (IOException error) {
            throw new AssertionError("could not write " + file, error);
        }
    }

    private static File[] orEmpty(File[] files) {
        return files == null ? new File[0] : files;
    }
}
