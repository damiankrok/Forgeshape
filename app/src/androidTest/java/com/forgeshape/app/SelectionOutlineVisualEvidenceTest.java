package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static com.forgeshape.app.WorkspaceTestSupport.waitForLayout;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.pm.ActivityInfo;
import android.graphics.Bitmap;
import android.net.Uri;
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

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

/**
 * `E2E-SELOUTR1-VIS`: deterministic screenshot evidence for `SEL-OUT-R1`,
 * captured through the real chrome on the authoritative emulator.
 *
 * <p>One journey, twelve captures, in the order §20 names them: a selected
 * Construction Body on a dark ground, the same scene with the outline off, a
 * selected Imported Mesh, a selected body in Sculpt, a selected CAD Body, a
 * partly occluded selected body, two bodies with A selected and then B, Warm
 * Light, Cool Light, the left-handed View/Overlay surface with the Selection
 * Outline row in it, and the fallback selection after a Delete.
 *
 * <p><b>The frames are measured, not only looked at.</b> Every capture is
 * written beside facts the suite took from the device at that instant — the
 * selected {@code ObjectId} and its representation, the toggle state, the
 * palette, the renderer's own mask extent and band width — and, for the frames
 * where a band should be present, the band's <em>measured</em> width in
 * screen pixels and its measured colour, scanned out of the captured bitmap
 * itself. That is what turns "the outline is about three pixels wide" from a
 * claim into a number.
 *
 * <p>Captured with {@code UiAutomation.takeScreenshot()}, which captures the
 * composed display, so the Vulkan viewport is in every frame. <b>No owner
 * aesthetic approval is claimed by any of it.</b>
 */
@RunWith(AndroidJUnit4.class)
public final class SelectionOutlineVisualEvidenceTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    /**
     * The authored outline colours, mirrored from
     * `forgeshape_selection_outline.cpp` so the scan below knows what it is
     * looking for.
     *
     * <p>A mirror rather than a JNI read on purpose: the values are pinned at
     * source by the render-shading self-test, which measures them against every
     * ground, and adding a JNI method so a screenshot could ask for a colour
     * would put an evidence concern on the product's boundary. If they ever
     * drift apart the scan finds no band and this suite fails loudly.
     */
    private static final int[] OUTLINE_ON_DARK = {255, 184, 66};    // 1.00, 0.72, 0.26
    private static final int[] OUTLINE_ON_LIGHT = {148, 51, 0};     // 0.58, 0.20, 0.00

    /** How far a pixel may sit from the authored colour and still count. */
    private static final int MATCH_TOLERANCE = 26;

    private File outDir;
    private File scratch;
    private final List<String> facts = new ArrayList<>();
    private int captureIndex;

    @Before
    public void startFromTheDefaults() {
        outDir = new File(InstrumentationRegistry.getInstrumentation().getTargetContext()
                .getExternalFilesDir(null), "evidence/sel-out-r1");
        //noinspection ResultOfMethodCallIgnored
        outDir.mkdirs();
        for (File stale : orEmpty(outDir.listFiles())) {
            //noinspection ResultOfMethodCallIgnored
            stale.delete();
        }
        scratch = new File(InstrumentationRegistry.getInstrumentation().getTargetContext()
                .getCacheDir(), "sel-out-r1-vis");
        assertTrue("scratch directory", scratch.isDirectory() || scratch.mkdirs());
        restoreDefaults();
        WorkspaceTestSupport.setOrientation(rule.getScenario(),
                ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        waitForLayout(rule.getScenario(), false);
        resetToBaselineConstruction(rule.getScenario());
    }

    @After
    public void leaveTheDefaultsBehind() {
        writeFacts();
        restoreDefaults();
        resetToBaselineConstruction(rule.getScenario());
    }

    @Test
    public void e2eSelOutR1Vis_theWholeJourneyInTwelveCaptures() {
        // --- 01 / 02: a Construction Body, outline on and then off ---------
        run((activity, workspace) -> {
            NativeViewport.applyConstructionSphere(1.0);
            workspace.syncFromNative();
        });
        settleLayout();
        sceneFacts("construction_selected", "dark ground, outline on");
        capture("01_construction_selected_dark");

        run((activity, workspace) -> NativeViewport.setSelectionOutlineVisible(false));
        settleLayout();
        sceneFacts("construction_outline_off", "the same scene with the outline off");
        capture("02_construction_outline_off", false);

        run((activity, workspace) -> NativeViewport.setSelectionOutlineVisible(true));
        settleLayout();

        // --- 03: an Imported Mesh -------------------------------------------
        importSentinelAndSelectIt();
        sceneFacts("imported_selected", "a selected Imported Mesh");
        capture("03_imported_selected");

        // Back to a clean single-body Construction project for the rest.
        freshProject();

        // --- 04: a body in Sculpt -------------------------------------------
        run((activity, workspace) -> {
            NativeViewport.applyConstructionSphere(1.0);
            NativeViewport.setSculptBrush(300.0, 1.0);
            workspace.findViewById(R.id.freeze_to_sculpt).performClick();
        });
        settleLayout();
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        sceneFacts("sculpt_selected", "a selected body in Sculpt, after a real stroke");
        capture("04_sculpt_selected");
        run((activity, workspace) -> {
            NativeViewport.enterConstructionMode();
            workspace.onNativeStateChanged();
        });
        settleLayout();
        freshProject();

        // --- 05: a CAD Body --------------------------------------------------
        buildACadBodyAndSelectIt();
        sceneFacts("cad_selected", "a selected CAD Body");
        capture("05_cad_selected");
        freshProject();

        // --- 06: a partly occluded selected body ----------------------------
        //
        // The selected sphere sits at the origin and a tall, narrow slab stands
        // BETWEEN it and the camera, so the sphere is visible either side of
        // the slab and hidden behind it. The band must trace the two visible
        // parts and stop dead at the slab; anything running across the slab
        // would be an x-ray outline. That is the whole depth claim in one
        // frame.
        //
        // The slab is placed along the camera's own orbit direction rather than
        // along a world axis, because the baseline camera looks at the origin
        // from an oblique yaw/pitch and a world-axis offset would simply put
        // the two bodies side by side. `debugCameraPose` reports the orbit pose
        // and the direction is derived from it with the ONE convention
        // CameraController::orbitDirection states, so this stays correct if the
        // baseline pose is ever retuned.
        final long behind = NativeViewport.sceneActiveBodyId();
        run((activity, workspace) -> {
            NativeViewport.applyConstructionSphere(1.0);
            NativeViewport.applyBoxTransform(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
            workspace.syncFromNative();
        });
        final float[] pose = new float[NativeViewport.CAMERA_POSE_SIZE];
        NativeViewport.debugCameraPose(pose);
        final double yaw = pose[NativeViewport.CAMERA_POSE_YAW];
        final double pitch = pose[NativeViewport.CAMERA_POSE_PITCH];
        // target -> eye, unit length. Mirrored from
        // CameraController::orbitDirection.
        final double cp = Math.cos(pitch);
        final double toEyeX = cp * Math.sin(yaw);
        final double toEyeY = Math.sin(pitch);
        final double toEyeZ = cp * Math.cos(yaw);
        // The camera's screen RIGHT, from the same convention
        // CameraController::cameraBasis uses: cross(eye -> target, world up),
        // normalised. It has no Y component, because the world up is Y.
        final double rightLength = Math.sqrt(toEyeZ * toEyeZ + toEyeX * toEyeX);
        final double rightX = rightLength > 0.001 ? toEyeZ / rightLength : 1.0;
        final double rightZ = rightLength > 0.001 ? -toEyeX / rightLength : 0.0;
        // Toward the camera by a SHORT step and sideways by a third of the
        // sphere's radius. The step is short deliberately: a slab close to the
        // eye is magnified hard by the perspective projection and swallows the
        // sphere whole, which is a fully occluded body and a different frame
        // from the one this capture is for. The lateral shift is what makes the
        // partial overlap robust rather than a matter of getting the
        // magnification exactly right.
        final double step = 0.8;
        final double sideways = 0.35;
        final long inFront = addABoxAt(
                toEyeX * step + rightX * sideways,
                toEyeY * step,
                toEyeZ * step + rightZ * sideways,
                0.40, 2.6, 0.40);
        run((activity, workspace) -> {
            NativeViewport.sceneSelectBody(behind);
            workspace.syncFromNative();
        });
        settleLayout();
        fact("camera_orbit_pose_yaw_pitch_distance",
                pose[0] + "," + pose[1] + "," + pose[2]);
        fact("occluder_placement", String.format(java.util.Locale.US,
                "toward_eye %.2f, sideways %.2f, world (%.3f, %.3f, %.3f)", step, sideways,
                toEyeX * step + rightX * sideways, toEyeY * step,
                toEyeZ * step + rightZ * sideways));
        fact("occluder_object_id", inFront);
        fact("occluded_selected_object_id", behind);
        sceneFacts("occluded_selected",
                "a slab stands between the camera and the SELECTED sphere");
        capture("06_occluded_selected");
        freshProject();

        // --- 07 / 08: two bodies, selection A then B -------------------------
        final long bodyA = NativeViewport.sceneActiveBodyId();
        final long bodyB = addABodyAt(1.7, 0.0, 0.0);
        run((activity, workspace) -> {
            NativeViewport.sceneSelectBody(bodyA);
            workspace.syncFromNative();
        });
        settleLayout();
        fact("body_a", bodyA);
        fact("body_b", bodyB);
        sceneFacts("two_bodies_a_selected", "two bodies, A selected");
        capture("07_two_bodies_selection_a");

        run((activity, workspace) -> {
            NativeViewport.sceneSelectBody(bodyB);
            workspace.syncFromNative();
        });
        settleLayout();
        sceneFacts("two_bodies_b_selected", "the same two bodies, B selected");
        capture("08_two_bodies_selection_b");

        // --- 09 / 10: the two LIGHT grounds ----------------------------------
        run((activity, workspace) ->
                NativeViewport.setViewportBackground(NativeViewport.VIEWPORT_BACKGROUND_WARM_LIGHT));
        settleLayout();
        sceneFacts("warm_light", "the outline over the Warm Light ground");
        capture("09_warm_light");

        run((activity, workspace) ->
                NativeViewport.setViewportBackground(NativeViewport.VIEWPORT_BACKGROUND_COOL_LIGHT));
        settleLayout();
        sceneFacts("cool_light", "the outline over the Cool Light ground");
        capture("10_cool_light");

        run((activity, workspace) ->
                NativeViewport.setViewportBackground(
                        NativeViewport.VIEWPORT_BACKGROUND_WARM_GRAPHITE));
        settleLayout();

        // --- 11: the left-handed View/Overlay surface ------------------------
        setHandedness(R.id.handedness_left);
        // Hold Transform so the rail zone is populated, then measure it BEFORE
        // the popover opens. That is the only moment both exist: the workspace
        // WITHDRAWS the rail zone while the Display popover is open (the
        // `displayPopover.isOpen()` flag reaches the host's own visibility
        // decision), which is the product honouring "a surface may never
        // partially cover another live control". Measuring only after the
        // popover opened would report the host as absent and make the overlap
        // fact vacuously true — evidence that proves nothing.
        run((activity, workspace) ->
                WorkspaceTestSupport.selectConstructionTool(workspace, R.id.tool_rail_place));
        settleLayout();
        run((activity, workspace) -> {
            // READ, never asserted: a hardcoded "left" here would be a caption,
            // not evidence, and would still read "left" if the preference had
            // not applied.
            fact("handedness", AppPreferencesStore.current(activity).handedness().name());
            fact("workspace_reports_left_handed", workspace.leftHanded());
            bounds(workspace, "rail_zone_host_before_popover",
                    R.id.workspace_trailing_host);
            bounds(workspace, "tool_rail_before_popover", R.id.tool_rail);
        });
        press(R.id.display_settings_button);
        run((activity, workspace) -> {
            bounds(workspace, "display_popover", R.id.display_settings_popover);
            bounds(workspace, "selection_outline_on", R.id.view_selection_outline_on);
            bounds(workspace, "selection_outline_off", R.id.view_selection_outline_off);
            // Expected to be absent: withdrawn, not overlapped.
            bounds(workspace, "rail_zone_host_while_popover_open",
                    R.id.workspace_trailing_host);
            fact("rail_zone_withdrawn_while_popover_open",
                    !isShown(workspace, R.id.workspace_trailing_host));
            fact("popover_overlaps_the_rail_zone",
                    overlaps(workspace.findViewById(R.id.display_settings_popover),
                            workspace.findViewById(R.id.workspace_trailing_host)));
        });
        sceneFacts("left_handed_overlay", "the View/Overlay surface, left-handed workspace");
        capture("11_left_handed_view_overlay");
        press(R.id.display_settings_button);
        setHandedness(R.id.handedness_right);

        // --- 12: the fallback selection after a Delete -----------------------
        final long doomed = NativeViewport.sceneActiveBodyId();
        run((activity, workspace) -> {
            NativeViewport.sceneDeleteBody(doomed);
            workspace.syncFromNative();
        });
        settleLayout();
        fact("deleted_object_id", doomed);
        fact("bodies_after_delete", NativeViewport.sceneBodyCount());
        sceneFacts("after_delete_fallback", "the fallback selection after a Delete");
        capture("12_after_delete_fallback");
    }

    // =======================================================================
    // Facts
    // =======================================================================

    /**
     * The facts every capture carries: what is selected, what it is, what the
     * toggle and the palette are, and what the renderer reports about the
     * outline resources it holds.
     */
    private void sceneFacts(String phase, String what) {
        final double[] stats = new double[NativeViewport.OUTLINE_STATS_SIZE];
        run((activity, workspace) -> {
            NativeViewport.selectionOutlineStats(stats);
            final long selected = NativeViewport.sceneActiveBodyId();
            fact("phase", phase);
            fact("what", what);
            fact("selected_object_id", selected);
            fact("selected_representation", representationName(
                    NativeViewport.sceneBodyRepresentation(selected)));
            fact("body_count", NativeViewport.sceneBodyCount());
            fact("outline_enabled", NativeViewport.selectionOutlineVisible());
            fact("grid_visible", NativeViewport.gridVisible());
            fact("viewport_background_index", NativeViewport.viewportBackground());
            fact("product_mode", NativeViewport.productMode() == NativeViewport.MODE_SCULPT
                    ? "sculpt" : "construction");
            fact("renderer_mask_extent_px",
                    (int) stats[NativeViewport.OUTLINE_STAT_MASK_WIDTH] + "x"
                            + (int) stats[NativeViewport.OUTLINE_STAT_MASK_HEIGHT]);
            fact("renderer_mask_allocations",
                    (long) stats[NativeViewport.OUTLINE_STAT_MASK_ALLOCATIONS]);
            fact("renderer_band_half_width_px",
                    stats[NativeViewport.OUTLINE_STAT_WIDTH_PIXELS]);
            fact("renderer_composite_draws",
                    (long) stats[NativeViewport.OUTLINE_STAT_COMPOSITE_DRAWS]);
        });
    }

    private static String representationName(int representation) {
        if (representation == NativeViewport.REPRESENTATION_IMPORTED) return "imported_mesh";
        if (representation == NativeViewport.REPRESENTATION_CAD) return "cad_body";
        if (representation == NativeViewport.REPRESENTATION_CONSTRUCTION) return "construction";
        return "none";
    }

    /**
     * Scans the captured frame for the outline band and records what it found.
     *
     * <p>What it reports as the band's THICKNESS is the MEDIAN run of
     * outline-coloured pixels along a horizontal cut, not the widest. A
     * horizontal line crosses a rounded silhouette's band perpendicularly
     * almost everywhere and tangentially at the top and bottom, where it runs
     * along it for tens of pixels; the widest run measures that tangent and the
     * median measures the thickness. Both are recorded, so the reader can see
     * the difference rather than take the choice on trust.
     *
     * <p>It also scans for the colour of the OTHER ground family. On a light
     * ground that count must be zero, which is what proves the palette swap
     * actually reached the renderer rather than the band simply still being
     * there in the old colour.
     *
     * <p>A frame with the outline off must find nothing at all, and that
     * absence is the evidence for capture 02.
     */
    private void measureBand(Bitmap frame, boolean expectBand, String name) {
        final boolean light = NativeViewport.viewportBackground()
                        == NativeViewport.VIEWPORT_BACKGROUND_WARM_LIGHT
                || NativeViewport.viewportBackground()
                        == NativeViewport.VIEWPORT_BACKGROUND_COOL_LIGHT;
        final int[] target = light ? OUTLINE_ON_LIGHT : OUTLINE_ON_DARK;
        final int[] otherFamily = light ? OUTLINE_ON_DARK : OUTLINE_ON_LIGHT;

        final List<Integer> runs = new ArrayList<>();
        int widestRun = 0;
        int matchedPixels = 0;
        int otherFamilyPixels = 0;
        int minX = Integer.MAX_VALUE;
        int maxX = -1;
        int minY = Integer.MAX_VALUE;
        int maxY = -1;
        for (int y = 0; y < frame.getHeight(); y += 2) {
            int run = 0;
            for (int x = 0; x < frame.getWidth(); x++) {
                final int pixel = frame.getPixel(x, y);
                if (isNear(pixel, otherFamily)) {
                    otherFamilyPixels++;
                }
                if (isNear(pixel, target)) {
                    run++;
                    matchedPixels++;
                    if (x < minX) minX = x;
                    if (x > maxX) maxX = x;
                    if (y < minY) minY = y;
                    if (y > maxY) maxY = y;
                } else {
                    if (run > 0) {
                        runs.add(run);
                        if (run > widestRun) widestRun = run;
                    }
                    run = 0;
                }
            }
            if (run > 0) {
                runs.add(run);
                if (run > widestRun) widestRun = run;
            }
        }
        java.util.Collections.sort(runs);
        final int median = runs.isEmpty() ? 0 : runs.get(runs.size() / 2);

        fact("measured_band_target_rgb", target[0] + "," + target[1] + "," + target[2]);
        fact("measured_band_thickness_px_median_run", median);
        fact("measured_band_widest_run_px_tangent", widestRun);
        fact("measured_band_run_count", runs.size());
        fact("measured_band_matched_pixels", matchedPixels);
        fact("measured_band_bounding_box", maxX < 0 ? "none"
                : "x[" + minX + ".." + maxX + "] y[" + minY + ".." + maxY + "]");
        fact("measured_other_family_colour_pixels", otherFamilyPixels);
        // The ground, sampled where no chrome and no body can be: one pixel in
        // from the leading edge, a third of the way down.
        final int ground = frame.getPixel(2, (int) (frame.getHeight() * 0.33));
        fact("sampled_ground_rgb", ((ground >> 16) & 0xFF) + "," + ((ground >> 8) & 0xFF)
                + "," + (ground & 0xFF));

        // The evidence ASSERTS, so a journey that quietly stopped drawing the
        // outline fails here rather than producing twelve plausible frames.
        if (expectBand) {
            assertTrue(name + ": a band must be present in this frame, and none was found",
                    matchedPixels > 0);
            assertTrue(name + ": the band thickness must be a few screen pixels, measured " + median,
                    median >= 1 && median <= 8);
        } else {
            assertTrue(name + ": no band may be present in this frame, and " + matchedPixels
                    + " outline-coloured pixels were found", matchedPixels == 0);
        }
        // A light ground must carry NO dark-family outline pixels, and the
        // other way round. This is what says the palette reached the renderer.
        assertTrue(name + ": the outline must be drawn in exactly one ground family colour, and "
                        + otherFamilyPixels + " pixels of the other family were found",
                otherFamilyPixels == 0);
    }

    private static boolean isNear(int pixel, int[] target) {
        final int r = (pixel >> 16) & 0xFF;
        final int g = (pixel >> 8) & 0xFF;
        final int b = pixel & 0xFF;
        return Math.abs(r - target[0]) <= MATCH_TOLERANCE
                && Math.abs(g - target[1]) <= MATCH_TOLERANCE
                && Math.abs(b - target[2]) <= MATCH_TOLERANCE;
    }

    // =======================================================================
    // Journey helpers
    // =======================================================================

    private void importSentinelAndSelectIt() {
        final byte[] bytes = readAsset("glb/construction_sentinel.glb");
        final File file = new File(scratch, "outline-evidence.glb");
        writeFile(file, bytes);
        run((activity, workspace) -> workspace.onOpenGlbDocumentChosen(Uri.fromFile(file)));
        settleLayout();
        final long[] ids = new long[NativeViewport.sceneBodyCount()];
        final int written = NativeViewport.sceneBodyIds(ids);
        for (int i = 0; i < written; i++) {
            if (NativeViewport.sceneBodyRepresentation(ids[i])
                    == NativeViewport.REPRESENTATION_IMPORTED) {
                final long imported = ids[i];
                run((activity, workspace) -> {
                    NativeViewport.sceneSelectBody(imported);
                    workspace.syncFromNative();
                });
                settleLayout();
                return;
            }
        }
        throw new AssertionError("the import produced no Imported Mesh to outline");
    }

    private void buildACadBodyAndSelectIt() {
        run((activity, workspace) -> {
            WorkspaceTestSupport.openAddPrimitive(workspace);
            workspace.addPrimitivePalette().findViewById(R.id.sketch_plane_by_name).performClick();
            workspace.addPrimitivePalette().findViewById(R.id.sketch_plane_xy).performClick();
        });
        settleLayout();
        final long body = SketchTestSupport.drawRectangleAndExtrude(rule.getScenario(),
                1.2, 0.8, "0.5");
        run((activity, workspace) -> {
            NativeViewport.sceneSelectBody(body);
            workspace.syncFromNative();
        });
        settleLayout();
    }

    /**
     * Closes the project and opens a fresh one-body Construction project.
     *
     * <p>`resetToBaselineConstruction` restores the ACTIVE body to a known
     * shape and placement; it does not remove the bodies earlier phases added,
     * because deleting a user's bodies is not what a reset means. An evidence
     * frame captioned "two bodies" must actually show two, so the phases that
     * make a claim about the scene's population start from an empty one.
     * Closing writes nothing and is exactly what Back to Home does.
     */
    private void freshProject() {
        run((activity, workspace) -> {
            NativeViewport.sketchCancel();
            NativeViewport.supportChooserCancel();
            NativeViewport.enterConstructionMode();
            NativeViewport.closeProject();
            workspace.ensureConstructionProjectForTest();
            NativeViewport.debugResetConstructionHistory();
            workspace.syncFromNative();
        });
        settleLayout();
        resetToBaselineConstruction(rule.getScenario());
    }

    /** A new Construction box of the given size at the given placement. */
    private long addABoxAt(final double x, final double y, final double z,
                           final double width, final double height, final double depth) {
        final Long added = WorkspaceTestSupport.onWorkspace(rule.getScenario(),
                (activity, workspace) -> {
                    final long id = NativeViewport.sceneAddBody();
                    NativeViewport.applyConstructionBox(width, height, depth);
                    NativeViewport.applyBoxTransform(x, y, z, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
                    workspace.syncFromNative();
                    return id;
                });
        settleLayout();
        assertNotNull(added);
        return added;
    }

    private long addABodyAt(final double x, final double y, final double z) {
        final Long added = WorkspaceTestSupport.onWorkspace(rule.getScenario(),
                (activity, workspace) -> {
                    final long id = NativeViewport.sceneAddBody();
                    NativeViewport.applyConstructionSphere(0.9);
                    NativeViewport.applyBoxTransform(x, y, z, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
                    workspace.syncFromNative();
                    return id;
                });
        settleLayout();
        assertNotNull(added);
        return added;
    }

    private void restoreDefaults() {
        run((activity, workspace) -> {
            NativeViewport.setSelectionOutlineVisible(true);
            NativeViewport.setGridVisible(true);
            NativeViewport.setViewportBackground(NativeViewport.VIEWPORT_BACKGROUND_WARM_GRAPHITE);
        });
    }

    /**
     * Switches handedness through the Settings page a user would use, rather
     * than by writing the preference: the frame is evidence of the product's
     * own path, and the mirrored layout is a consequence the page applies.
     */
    private void setHandedness(int chipId) {
        run((activity, workspace) -> {
            if (workspace.settingsVisible()) {
                return;
            }
            workspace.findViewById(R.id.project_actions_button).performClick();
            workspace.findViewById(R.id.project_settings).performClick();
        });
        settleLayout();
        press(chipId);
        run((activity, workspace) -> {
            if (workspace.settingsVisible()) {
                workspace.findViewById(R.id.settings_back).performClick();
            }
        });
        waitForLayout(rule.getScenario(), false);
    }

    // =======================================================================
    // Capture plumbing
    // =======================================================================

    private interface Effect {
        void run(ForgeShapeActivity activity, EditorWorkspaceView workspace);
    }

    private void run(final Effect effect) {
        WorkspaceTestSupport.doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            effect.run(activity, workspace);
            return null;
        });
    }

    private void press(final int id) {
        run((activity, workspace) -> {
            final View control = workspace.findViewById(id);
            assertNotNull(workspace.getResources().getResourceEntryName(id), control);
            control.performClick();
        });
        settleLayout();
    }

    private void capture(String name) {
        capture(name, true);
    }

    /**
     * Captures one frame and measures the band in it.
     *
     * `expectBand` says whether this frame is one the outline should be
     * visible in, so the absence in capture 02 is asserted rather than merely
     * recorded.
     */
    private void capture(String name, boolean expectBand) {
        settleLayout();
        awaitPresentedFrames(name);
        final Bitmap frame =
                InstrumentationRegistry.getInstrumentation().getUiAutomation().takeScreenshot();
        assertNotNull("the display could be captured", frame);
        // Written BEFORE it is measured, deliberately: when the measurement
        // fails, the frame that failed it is the first thing anyone will want
        // to look at, and a capture discarded on the way to an assertion is
        // evidence thrown away at exactly the moment it was needed.
        captureIndex++;
        final File png = new File(outDir, name + ".png");
        try (FileOutputStream out = new FileOutputStream(png)) {
            assertTrue(frame.compress(Bitmap.CompressFormat.PNG, 100, out));
        } catch (IOException error) {
            throw new AssertionError("could not write " + png, error);
        }
        // Then measure, and only then close the block. The collector reads
        // facts.txt as "every fact up to a capture line belongs to it", so the
        // measurements must be appended BEFORE that line or they would be
        // reported against the next frame.
        measureBand(frame, expectBand, name);
        facts.add("capture=" + name + ".png width=" + frame.getWidth() + " height="
                + frame.getHeight());
        facts.add("");
    }

    /**
     * Frames that must be presented after a state change before the display
     * can be trusted to show it: the frame that may have been recorded just
     * before the change, the first one recorded after it, and the four-image
     * FIFO swapchain's depth behind that. The number `CadVerticalSliceTest`
     * already waits for, for the same reason.
     */
    private static final int CAPTURE_PRESENTED_FRAMES = 6;
    private static final long CAPTURE_FRAME_TIMEOUT_MS = 15000;
    private static final long CAPTURE_FRAME_POLL_MS = 25;

    /**
     * Waits until the renderer has presented {@link #CAPTURE_PRESENTED_FRAMES}
     * more frames, and FAILS the test when it does not within
     * {@link #CAPTURE_FRAME_TIMEOUT_MS}.
     *
     * <p>A fixed delay is not a synchronisation: SwiftShader on the CI emulator
     * presents about three frames a second, so a screenshot taken 500 ms after
     * the Warm Light ground was set could show a frame recorded before it, and
     * the scan then found no light-family band in a frame that was simply
     * stale. The presented-frame count is process-monotonic, so the delta read
     * here is frames the renderer really presented after the state change.
     *
     * <p>Unlike `CadVerticalSliceTest`, whose captures are illustration and
     * which records a timeout, this class ASSERTS pixels, so a frame the wait
     * cannot vouch for is never taken: the case fails before the screenshot,
     * with the counts that say why.
     */
    private void awaitPresentedFrames(String name) {
        final long start = NativeViewport.debugRendererFramesPresented();
        final long began = SystemClock.uptimeMillis();
        long latest = start;
        long elapsed = 0;
        while (elapsed < CAPTURE_FRAME_TIMEOUT_MS) {
            latest = NativeViewport.debugRendererFramesPresented();
            elapsed = SystemClock.uptimeMillis() - began;
            if (latest < start) {
                break;  // the count is monotonic; a smaller one is not a delta
            }
            if (latest - start >= CAPTURE_PRESENTED_FRAMES) {
                fact("presented_frames_start", start);
                fact("presented_frames_final", latest);
                fact("presented_frames_delta", latest - start);
                fact("presented_frames_wait_ms", elapsed);
                return;
            }
            SystemClock.sleep(CAPTURE_FRAME_POLL_MS);
        }
        final String diagnosis = name + ": the renderer did not present "
                + CAPTURE_PRESENTED_FRAMES + " frames after the state change, so no frame"
                + " can be trusted to show it; start=" + start + " latest=" + latest
                + " delta=" + (latest - start) + " required=" + CAPTURE_PRESENTED_FRAMES
                + " elapsed_ms=" + elapsed + " timeout_ms=" + CAPTURE_FRAME_TIMEOUT_MS
                + " renderer_lifecycle=" + NativeViewport.rendererLifecycle()
                + " viewport_background_index=" + NativeViewport.viewportBackground()
                + " selected_object_id=" + NativeViewport.sceneActiveBodyId();
        fact("presented_frames_wait", "FAILED " + diagnosis);
        throw new AssertionError(diagnosis);
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
        facts.add("  " + label + "=[" + at[0] + "," + at[1] + "][" + (at[0] + view.getWidth())
                + "," + (at[1] + view.getHeight()) + "] width_px=" + view.getWidth()
                + " height_px=" + view.getHeight());
    }

    private static boolean isShown(EditorWorkspaceView workspace, int id) {
        final View view = workspace.findViewById(id);
        return view != null && view.isShown();
    }

    private static boolean overlaps(View a, View b) {
        if (a == null || b == null || !a.isShown() || !b.isShown()) {
            return false;
        }
        final int[] pa = new int[2];
        final int[] pb = new int[2];
        a.getLocationOnScreen(pa);
        b.getLocationOnScreen(pb);
        return pa[0] < pb[0] + b.getWidth() && pb[0] < pa[0] + a.getWidth()
                && pa[1] < pb[1] + b.getHeight() && pb[1] < pa[1] + a.getHeight();
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

    private static byte[] readAsset(String name) {
        try (InputStream in = InstrumentationRegistry.getInstrumentation().getContext()
                .getAssets().open(name)) {
            final ByteArrayOutputStream out = new ByteArrayOutputStream();
            final byte[] buffer = new byte[8192];
            int read;
            while ((read = in.read(buffer)) > 0) {
                out.write(buffer, 0, read);
            }
            return out.toByteArray();
        } catch (IOException e) {
            throw new AssertionError("could not read asset " + name, e);
        }
    }

    private static void writeFile(File file, byte[] bytes) {
        try (FileOutputStream out = new FileOutputStream(file)) {
            out.write(bytes);
        } catch (IOException e) {
            throw new AssertionError("could not write " + file, e);
        }
    }

    private static File[] orEmpty(File[] files) {
        return files == null ? new File[0] : files;
    }
}
